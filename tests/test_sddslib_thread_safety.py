import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libSDDS1.a",
  LIB_DIR / "libnamelist.a",
  LIB_DIR / "librpnlib.a",
  LIB_DIR / "libmdbmth.a",
  LIB_DIR / "libmdblib.a",
]

HARNESS_SOURCE = r"""
#define _GNU_SOURCE

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDDS.h"
#include "rpn.h"

extern void SDDS_FreePointerArray(void **data, int32_t dimensions, int32_t *dimension);

#define THREADS 8
#define ITERATIONS 500
#define INITIAL_ROW_LIMIT 424242
#define INITIAL_IO_BUFFER 16384
#define INITIAL_NAME_FLAGS SDDS_ALLOW_ANY_NAME
#define INITIAL_LZMA_LEVEL 6
#define INITIAL_AUTO_CHECK TABULAR_DATA_CHECKS
#define INITIAL_PROGRAM_NAME "sddslib-main"

typedef struct {
  long id;
  char error[256];
} Worker;

static pthread_mutex_t initial_global_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initial_global_cond = PTHREAD_COND_INITIALIZER;
static int initial_global_checks_complete = 0;
static int initial_global_check_failed = 0;
static pthread_mutex_t process_global_settings_mutex = PTHREAD_MUTEX_INITIALIZER;

static void set_error(Worker *worker, const char *message)
{
  snprintf(worker->error, sizeof(worker->error), "%s", message);
}

static void free_error_messages(char **messages, int32_t count)
{
  int32_t i;

  if (!messages)
    return;
  for (i = 0; i < count; i++)
    free(messages[i]);
  free(messages);
}

static int check_error_stack(Worker *worker, long iteration)
{
  char expected[64];
  char expected_after_print[80];
  char **messages;
  int32_t count = 0;
  FILE *fp;

  snprintf(expected, sizeof(expected), "worker-%ld-error-%ld",
           worker->id, iteration);
  snprintf(expected_after_print, sizeof(expected_after_print),
           "worker-%ld-error-after-print-%ld", worker->id, iteration);
  SDDS_ClearErrors();
  SDDS_SetError0(expected);
  sched_yield();
  if (SDDS_NumberOfErrors() != 1) {
    set_error(worker, "error stack count interleaved");
    return 1;
  }
  messages = SDDS_GetErrorMessages(&count, SDDS_ALL_GetErrorMessages);
  if (count != 1 || !messages || strcmp(messages[0], expected)) {
    free_error_messages(messages, count);
    set_error(worker, "error stack message interleaved");
    return 1;
  }
  free_error_messages(messages, count);

  fp = tmpfile();
  if (!fp) {
    SDDS_ClearErrors();
    set_error(worker, "tmpfile failed for error stack print check");
    return 1;
  }
  SDDS_PrintErrors(fp, 0);
  fclose(fp);
  if (SDDS_NumberOfErrors() != 0) {
    SDDS_ClearErrors();
    set_error(worker, "printed error stack was not cleared");
    return 1;
  }
  SDDS_SetError0(expected_after_print);
  messages = SDDS_GetErrorMessages(&count, SDDS_ALL_GetErrorMessages);
  if (count != 1 || !messages || strcmp(messages[0], expected_after_print)) {
    free_error_messages(messages, count);
    SDDS_ClearErrors();
    set_error(worker, "error stack reuse after print changed");
    return 1;
  }
  free_error_messages(messages, count);
  SDDS_ClearErrors();
  return 0;
}

static int check_initial_global_settings(Worker *worker)
{
  SDDS_DATASET dataset;
  FILE *fp;
  char buffer[256];

  if (SDDS_GetRowLimit() != INITIAL_ROW_LIMIT) {
    set_error(worker, "row limit did not match initial global value");
    return 1;
  }
  if (SDDS_SetDefaultIOBufferSize(-1) != INITIAL_IO_BUFFER) {
    set_error(worker, "default I/O buffer size did not match initial global value");
    return 1;
  }
  if (!SDDS_IsValidName("bad name with spaces", "column")) {
    SDDS_ClearErrors();
    set_error(worker, "name validity flags did not match initial global value");
    return 1;
  }
  if (SDDS_GetLZMACompressionLevel() != INITIAL_LZMA_LEVEL) {
    set_error(worker, "LZMA compression level did not match initial global value");
    return 1;
  }
  memset(&dataset, 0, sizeof(dataset));
  dataset.layout.n_columns = 1;
  if (SDDS_CheckTabularData(&dataset, "initial-global-auto-check")) {
    set_error(worker, "auto-check mode did not match initial global value");
    return 1;
  }
  SDDS_ClearErrors();

  fp = tmpfile();
  if (!fp) {
    set_error(worker, "tmpfile failed");
    return 1;
  }
  SDDS_PrintCheckText(fp, "missing", NULL, 0, "column", SDDS_CHECK_NONEXISTENT);
  fflush(fp);
  rewind(fp);
  if (!fgets(buffer, sizeof(buffer), fp) ||
      !strstr(buffer, "(" INITIAL_PROGRAM_NAME ")")) {
    fclose(fp);
    set_error(worker, "program name did not match initial global value");
    return 1;
  }
  fclose(fp);
  return 0;
}

static int finish_initial_global_checks(Worker *worker, int failed)
{
  pthread_mutex_lock(&initial_global_mutex);
  if (failed)
    initial_global_check_failed = 1;
  initial_global_checks_complete++;
  if (initial_global_checks_complete == THREADS)
    pthread_cond_broadcast(&initial_global_cond);
  else
    while (initial_global_checks_complete < THREADS)
      pthread_cond_wait(&initial_global_cond, &initial_global_mutex);
  failed = initial_global_check_failed;
  pthread_mutex_unlock(&initial_global_mutex);

  if (failed) {
    if (!worker->error[0])
      set_error(worker, "initial global setting check failed");
    return 1;
  }
  return 0;
}

static int check_row_limit(Worker *worker, long iteration)
{
  int64_t expected = worker->id * 1000000 + iteration + 1;

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetRowLimit(expected);
  sched_yield();
  if (SDDS_GetRowLimit() != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "row limit state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_default_io_buffer_size(Worker *worker, long iteration)
{
  int32_t expected = 4096 + (int32_t)worker->id * 512 + (int32_t)(iteration % 97);

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetDefaultIOBufferSize(expected);
  sched_yield();
  if (SDDS_SetDefaultIOBufferSize(-1) != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "default I/O buffer size state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_name_validity_flags(Worker *worker, long iteration)
{
  uint32_t expected = 0x100u + (uint32_t)worker->id * 16u + (uint32_t)(iteration & 0xf);

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetNameValidityFlags(expected);
  sched_yield();
  if ((uint32_t)SDDS_SetNameValidityFlags(0) != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "name validity flags state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_lzma_compression_level(Worker *worker, long iteration)
{
  int32_t expected = (int32_t)((worker->id + iteration) % 10);

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetLZMACompressionLevel(expected);
  sched_yield();
  if (SDDS_GetLZMACompressionLevel() != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "LZMA compression level state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_auto_check_mode(Worker *worker, long iteration)
{
  uint32_t expected = 0x1000u + (uint32_t)worker->id * 1024u + (uint32_t)iteration;

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetAutoCheckMode(expected);
  sched_yield();
  if (SDDS_SetAutoCheckMode(0) != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "auto-check mode state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_pointer_array(Worker *worker, long iteration)
{
  int32_t dimensions[2] = {2, 3};
  double data[6];
  double **pointer;
  long i;

  for (i = 0; i < 6; i++)
    data[i] = worker->id * 1000.0 + iteration * 10.0 + (double)i;
  pointer = (double **)SDDS_MakePointerArray(data, SDDS_DOUBLE, 2, dimensions);
  sched_yield();
  if (!pointer || pointer[0][2] != data[2] || pointer[1][2] != data[5]) {
    if (pointer)
      SDDS_FreePointerArray((void **)pointer, 2, dimensions);
    set_error(worker, "pointer-array mapping changed");
    return 1;
  }
  SDDS_FreePointerArray((void **)pointer, 2, dimensions);
  return 0;
}

static int nearly_equal(double actual, double expected)
{
  return fabs(actual - expected) < 1e-12;
}

static void free_test_layout(SDDS_DATASET *dataset)
{
  int32_t i;

  for (i = 0; i < dataset->layout.n_columns; i++) {
    free(dataset->layout.column_definition[i].name);
    free(dataset->layout.column_definition[i].symbol);
    free(dataset->layout.column_definition[i].units);
    free(dataset->layout.column_definition[i].description);
    free(dataset->layout.column_definition[i].format_string);
  }
  for (i = 0; i < dataset->layout.n_columns; i++)
    free(dataset->layout.column_index[i]);
  free(dataset->layout.column_definition);
  free(dataset->layout.column_index);

  for (i = 0; i < dataset->layout.n_parameters; i++) {
    free(dataset->layout.parameter_definition[i].name);
    free(dataset->layout.parameter_definition[i].symbol);
    free(dataset->layout.parameter_definition[i].units);
    free(dataset->layout.parameter_definition[i].description);
    free(dataset->layout.parameter_definition[i].format_string);
    free(dataset->layout.parameter_definition[i].fixed_value);
  }
  for (i = 0; i < dataset->layout.n_parameters; i++)
    free(dataset->layout.parameter_index[i]);
  free(dataset->layout.parameter_definition);
  free(dataset->layout.parameter_index);
}

static int check_rpn_compute_column(Worker *worker)
{
  SDDS_DATASET dataset;
  char source_name[64];
  char result_name[64];
  char parameter_name[64];
  char udf_name[64];
  char equation[256];
  double source[3];
  double result[3];
  double parameter_value;
  void *data[2];
  void *parameters[1];
  int32_t row_flag[3] = {1, 1, 1};
  int32_t source_index;
  int32_t result_index;
  int32_t parameter_index;
  long iteration;
  int64_t row;

  memset(&dataset, 0, sizeof(dataset));
  snprintf(source_name, sizeof(source_name), "sddslib_rpn_source_%ld", worker->id);
  snprintf(result_name, sizeof(result_name), "sddslib_rpn_result_%ld", worker->id);
  snprintf(parameter_name, sizeof(parameter_name), "sddslib_rpn_param_%ld", worker->id);
  snprintf(udf_name, sizeof(udf_name), "sddslib_rpn_udf_%ld", worker->id);

  rpn(NULL);
  source_index = SDDS_DefineColumn(&dataset, source_name, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0);
  result_index = SDDS_DefineColumn(&dataset, result_name, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0);
  parameter_index = SDDS_DefineParameter(&dataset, parameter_name, NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL);
  if (source_index < 0 || result_index < 0 || parameter_index < 0) {
    free_test_layout(&dataset);
    SDDS_ClearErrors();
    set_error(worker, "RPN compute dataset definition failed");
    return 1;
  }

  data[source_index] = source;
  data[result_index] = result;
  parameters[parameter_index] = &parameter_value;
  dataset.data = data;
  dataset.parameter = parameters;
  dataset.row_flag = row_flag;
  dataset.n_rows = 3;
  dataset.n_rows_allocated = 3;
  snprintf(equation, sizeof(equation), "%s %s + i_row +", source_name, parameter_name);
  create_udf(udf_name, equation);

  for (iteration = 0; iteration < 100; iteration++) {
    parameter_value = worker->id * 1000.0 + iteration;
    for (row = 0; row < 3; row++) {
      source[row] = worker->id * 100.0 + iteration * 10.0 + row;
      result[row] = -1.0;
    }
    dataset.page_number = iteration + 1;
    sched_yield();
    if (!SDDS_ComputeColumn(&dataset, result_index, udf_name)) {
      free_test_layout(&dataset);
      SDDS_ClearErrors();
      set_error(worker, "SDDS_ComputeColumn failed under concurrency");
      return 1;
    }
    for (row = 0; row < 3; row++) {
      double expected = source[row] + parameter_value + row;
      if (!nearly_equal(result[row], expected)) {
        free_test_layout(&dataset);
        set_error(worker, "SDDS_ComputeColumn result interleaved");
        return 1;
      }
    }
  }

  free_test_layout(&dataset);
  return 0;
}

static void free_name_list(char **names, int32_t count)
{
  int32_t i;

  if (!names)
    return;
  for (i = 0; i < count; i++)
    free(names[i]);
  free(names);
}

static int check_match_helpers_clear_reused_state(void)
{
  SDDS_DATASET dataset;
  COLUMN_DEFINITION columns[2];
  PARAMETER_DEFINITION parameters[2];
  ARRAY_DEFINITION arrays[2];
  SORTED_INDEX column_index_data[2];
  SORTED_INDEX parameter_index_data[2];
  SORTED_INDEX array_index_data[2];
  SORTED_INDEX *column_index[2];
  SORTED_INDEX *parameter_index[2];
  SORTED_INDEX *array_index[2];
  char **names = NULL;
  int32_t count;

  memset(&dataset, 0, sizeof(dataset));
  memset(columns, 0, sizeof(columns));
  memset(parameters, 0, sizeof(parameters));
  memset(arrays, 0, sizeof(arrays));

  columns[0].name = "c0";
  columns[0].type = SDDS_DOUBLE;
  columns[1].name = "c1";
  columns[1].type = SDDS_DOUBLE;
  parameters[0].name = "p0";
  parameters[0].type = SDDS_LONG;
  parameters[1].name = "p1";
  parameters[1].type = SDDS_LONG;
  arrays[0].name = "a0";
  arrays[0].type = SDDS_SHORT;
  arrays[1].name = "a1";
  arrays[1].type = SDDS_SHORT;

  column_index_data[0].name = columns[0].name;
  column_index_data[0].index = 0;
  column_index_data[1].name = columns[1].name;
  column_index_data[1].index = 1;
  column_index[0] = &column_index_data[0];
  column_index[1] = &column_index_data[1];
  parameter_index_data[0].name = parameters[0].name;
  parameter_index_data[0].index = 0;
  parameter_index_data[1].name = parameters[1].name;
  parameter_index_data[1].index = 1;
  parameter_index[0] = &parameter_index_data[0];
  parameter_index[1] = &parameter_index_data[1];
  array_index_data[0].name = arrays[0].name;
  array_index_data[0].index = 0;
  array_index_data[1].name = arrays[1].name;
  array_index_data[1].index = 1;
  array_index[0] = &array_index_data[0];
  array_index[1] = &array_index_data[1];

  dataset.layout.n_columns = 2;
  dataset.layout.column_definition = columns;
  dataset.layout.column_index = column_index;
  dataset.layout.n_parameters = 2;
  dataset.layout.parameter_definition = parameters;
  dataset.layout.parameter_index = parameter_index;
  dataset.layout.n_arrays = 2;
  dataset.layout.array_definition = arrays;
  dataset.layout.array_index = array_index;

  count = SDDS_MatchColumns(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "c0", NULL);
  if (count != 1 || !names || strcmp(names[0], "c0")) {
    free_name_list(names, count);
    return 1;
  }
  free_name_list(names, count);
  names = NULL;
  count = SDDS_MatchColumns(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "c1", NULL);
  if (count != 1 || !names || strcmp(names[0], "c1")) {
    free_name_list(names, count);
    return 2;
  }
  free_name_list(names, count);
  names = NULL;
  count = SDDS_MatchColumns(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "missing-column", NULL);
  if (count != 0 || names) {
    free_name_list(names, count);
    return 3;
  }
  count = SDDS_MatchColumns(&dataset, NULL, SDDS_MATCH_STRING, FIND_ANY_TYPE, "c*", SDDS_0_PREVIOUS | SDDS_OR);
  if (count != 2)
    return 4;
  count = SDDS_MatchColumns(&dataset, &names, SDDS_MATCH_STRING, FIND_ANY_TYPE, "c0", SDDS_NEGATE_MATCH | SDDS_AND);
  if (count != 1 || !names || strcmp(names[0], "c1")) {
    free_name_list(names, count);
    return 5;
  }
  free_name_list(names, count);
  names = NULL;

  count = SDDS_MatchParameters(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "p0", NULL);
  if (count != 1 || !names || strcmp(names[0], "p0")) {
    free_name_list(names, count);
    return 6;
  }
  free_name_list(names, count);
  names = NULL;
  count = SDDS_MatchParameters(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "p1", NULL);
  if (count != 1 || !names || strcmp(names[0], "p1")) {
    free_name_list(names, count);
    return 7;
  }
  free_name_list(names, count);
  names = NULL;
  count = SDDS_MatchParameters(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "missing-parameter", NULL);
  if (count != 0 || names) {
    free_name_list(names, count);
    return 8;
  }
  count = SDDS_MatchParameters(&dataset, NULL, SDDS_MATCH_STRING, FIND_ANY_TYPE, "p*", SDDS_0_PREVIOUS | SDDS_OR);
  if (count != 2)
    return 9;
  count = SDDS_MatchParameters(&dataset, &names, SDDS_MATCH_STRING, FIND_ANY_TYPE, "p0", SDDS_NEGATE_MATCH | SDDS_AND);
  if (count != 1 || !names || strcmp(names[0], "p1")) {
    free_name_list(names, count);
    return 10;
  }
  free_name_list(names, count);
  names = NULL;

  count = SDDS_MatchArrays(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "a0", NULL);
  if (count != 1 || !names || strcmp(names[0], "a0")) {
    free_name_list(names, count);
    return 11;
  }
  free_name_list(names, count);
  names = NULL;
  count = SDDS_MatchArrays(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "a1", NULL);
  if (count != 1 || !names || strcmp(names[0], "a1")) {
    free_name_list(names, count);
    return 12;
  }
  free_name_list(names, count);
  names = NULL;
  count = SDDS_MatchArrays(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "missing-array", NULL);
  if (count != 0 || names) {
    free_name_list(names, count);
    return 13;
  }
  count = SDDS_MatchArrays(&dataset, NULL, SDDS_MATCH_STRING, FIND_ANY_TYPE, "a*", SDDS_0_PREVIOUS | SDDS_OR);
  if (count != 2)
    return 14;
  count = SDDS_MatchArrays(&dataset, &names, SDDS_MATCH_STRING, FIND_ANY_TYPE, "a0", SDDS_NEGATE_MATCH | SDDS_AND);
  if (count != 1 || !names || strcmp(names[0], "a1")) {
    free_name_list(names, count);
    return 15;
  }
  free_name_list(names, count);

  dataset.layout.n_columns = 0;
  dataset.layout.n_parameters = 0;
  dataset.layout.n_arrays = 0;
  names = NULL;
  count = SDDS_MatchColumns(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "c0", NULL);
  if (count != 0 || names) {
    free_name_list(names, count);
    return 16;
  }
  count = SDDS_MatchParameters(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "p0", NULL);
  if (count != 0 || names) {
    free_name_list(names, count);
    return 17;
  }
  count = SDDS_MatchArrays(&dataset, &names, SDDS_NAME_STRINGS, FIND_ANY_TYPE, "a0", NULL);
  if (count != 0 || names) {
    free_name_list(names, count);
    return 18;
  }
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  char program_name[64];
  long i;
  int initial_global_failed;

  initial_global_failed = check_initial_global_settings(worker);
  if (finish_initial_global_checks(worker, initial_global_failed))
    return worker;
  snprintf(program_name, sizeof(program_name), "sddslib-worker-%ld", worker->id);
  SDDS_RegisterProgramName(program_name);
  if (check_rpn_compute_column(worker))
    return worker;
  for (i = 0; i < ITERATIONS; i++) {
    if (check_error_stack(worker, i) ||
        check_row_limit(worker, i) ||
        check_default_io_buffer_size(worker, i) ||
        check_name_validity_flags(worker, i) ||
        check_lzma_compression_level(worker, i) ||
        check_auto_check_mode(worker, i) ||
        check_pointer_array(worker, i))
      return worker;
  }
  return NULL;
}

static int run_thread_test(void)
{
  pthread_t threads[THREADS];
  Worker workers[THREADS];
  long i;

  SDDS_RegisterProgramName(INITIAL_PROGRAM_NAME);
  SDDS_SetRowLimit(INITIAL_ROW_LIMIT);
  SDDS_SetDefaultIOBufferSize(INITIAL_IO_BUFFER);
  SDDS_SetNameValidityFlags(INITIAL_NAME_FLAGS);
  SDDS_SetLZMACompressionLevel(INITIAL_LZMA_LEVEL);
  SDDS_SetAutoCheckMode(INITIAL_AUTO_CHECK);

  for (i = 0; i < THREADS; i++) {
    workers[i].id = i + 1;
    workers[i].error[0] = 0;
    if (pthread_create(&threads[i], NULL, thread_worker, &workers[i])) {
      fprintf(stderr, "pthread_create failed\n");
      return 1;
    }
  }

  for (i = 0; i < THREADS; i++) {
    void *status = NULL;
    if (pthread_join(threads[i], &status)) {
      fprintf(stderr, "pthread_join failed\n");
      return 1;
    }
    if (status) {
      fprintf(stderr, "thread %ld failed: %s\n", i + 1, ((Worker *)status)->error);
      return 1;
    }
  }

  puts("thread-safe");
  return 0;
}

static int run_regression(void)
{
  char **messages;
  int32_t count = 0;
  int32_t previous_buffer;
  int64_t previous_limit;
  double data[6] = {0, 1, 2, 3, 4, 5};
  double **pointer;
  int32_t dimensions[2] = {2, 3};
  int32_t previous_lzma;
  int32_t previous_name_flags;
  int32_t valid_name;
  uint32_t previous_auto;
  uint32_t observed_auto;

  SDDS_RegisterProgramName("sddslib-regression");
  SDDS_ClearErrors();
  SDDS_SetError0("alpha");
  messages = SDDS_GetErrorMessages(&count, SDDS_ALL_GetErrorMessages);
  if (count != 1 || !messages || strcmp(messages[0], "alpha"))
    return 1;
  printf("errors=%" PRId32 ":%" PRId32 ":%s\n",
         SDDS_NumberOfErrors(), count, messages[0]);
  free_error_messages(messages, count);
  SDDS_ClearErrors();

  previous_limit = SDDS_SetRowLimit(12345);
  printf("row_limit=%lld:%lld\n",
         (long long)previous_limit, (long long)SDDS_GetRowLimit());
  SDDS_SetRowLimit(previous_limit);

  previous_buffer = SDDS_SetDefaultIOBufferSize(4096);
  printf("io_buffer=%" PRId32 ":%" PRId32 "\n",
         previous_buffer, SDDS_SetDefaultIOBufferSize(-1));
  SDDS_SetDefaultIOBufferSize(previous_buffer);

  previous_name_flags = SDDS_SetNameValidityFlags(SDDS_ALLOW_ANY_NAME);
  valid_name = SDDS_IsValidName("bad name with spaces", "column");
  printf("name_valid=%" PRId32 ":%" PRId32 "\n",
         previous_name_flags, valid_name);
  SDDS_SetNameValidityFlags((uint32_t)previous_name_flags);

  previous_lzma = SDDS_GetLZMACompressionLevel();
  SDDS_SetLZMACompressionLevel(7);
  printf("lzma=%" PRId32 ":%" PRId32 "\n",
         previous_lzma, SDDS_GetLZMACompressionLevel());
  SDDS_SetLZMACompressionLevel(previous_lzma);

  previous_auto = SDDS_SetAutoCheckMode(0x1234u);
  observed_auto = SDDS_SetAutoCheckMode(0x55u);
  printf("autocheck=%u:%u\n", previous_auto, observed_auto);
  SDDS_SetAutoCheckMode(previous_auto);

  pointer = (double **)SDDS_MakePointerArray(data, SDDS_DOUBLE, 2, dimensions);
  if (!pointer)
    return 1;
  printf("pointer=%.1f:%.1f\n", pointer[0][2], pointer[1][2]);
  SDDS_FreePointerArray((void **)pointer, 2, dimensions);

  printf("match_helpers=%d\n", check_match_helpers_clear_reused_state());

  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "threads"))
    return run_thread_test();
  return run_regression();
}
"""


def compiler_command():
  command = shlex.split(os.environ.get("CC", "gcc"))
  if not command or not shutil.which(command[0]):
    pytest.skip("C compiler not available")
  return command


@pytest.fixture(scope="module")
def sddslib_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("SDDSlib libraries not built")

  tmp_path = tmp_path_factory.mktemp("sddslib_thread_safety")
  source = tmp_path / "sddslib_thread_safety.c"
  executable = tmp_path / "sddslib_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I",
    str(ROOT_DIR / "include"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lSDDS1",
    "-lnamelist",
    "-lrpnlib",
    "-lmdbmth",
    "-lmdblib",
    "-lgsl",
    "-lgslcblas",
    "-llzma",
    "-lz",
    "-lpthread",
    "-lm",
  ]
  if platform.system() == "Linux":
    command.extend(["-lrt", "-ldl", "-lgcc"])
  command.extend(["-o", str(executable)])

  subprocess.run(command, check=True)
  return executable


def run_harness(executable, *args):
  env = os.environ.copy()
  env["RPN_DEFNS"] = ""
  return subprocess.run(
    [str(executable), *args],
    check=True,
    capture_output=True,
    text=True,
    env=env,
  )


def test_sddslib_regression_outputs(sddslib_harness):
  result = run_harness(sddslib_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "errors=1:1:alpha\n"
    "row_limit=9223372036854775807:12345\n"
    "io_buffer=262144:4096\n"
    "name_valid=0:1\n"
    "lzma=2:7\n"
    "autocheck=0:4660\n"
    "pointer=2.0:5.0\n"
    "match_helpers=0\n"
  )


def test_sddslib_process_global_defaults_are_thread_safe(sddslib_harness):
  result = run_harness(sddslib_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
