import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libnamelist.a",
  LIB_DIR / "librpnlib.a",
  LIB_DIR / "libmdbmth.a",
  LIB_DIR / "libmdblib.a",
]

HARNESS_SOURCE = r"""
#define _GNU_SOURCE

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdb.h"
#include "scan.h"
#include "namelist.h"

#define THREADS 8
#define ITERATIONS 500

typedef struct {
  int value;
  int value_default;
  double gain;
  double gain_default;
  char *label;
  char *label_default;
} Config;

typedef struct {
  long id;
  char error[256];
} Worker;

static int nearly_equal(double actual, double expected)
{
  return fabs(actual - expected) < 1e-12;
}

static void set_error(Worker *worker, const char *message)
{
  snprintf(worker->error, sizeof(worker->error), "%s", message);
}

static void init_config(Config *config)
{
  config->value = -1;
  config->value_default = 7;
  config->gain = -1;
  config->gain_default = 1.5;
  config->label_default = (char *)"default label";
  config->label = config->label_default;
}

static void init_namelist(NAMELIST *nl, ITEM *items, Config *config)
{
  items[0].name = (char *)"value";
  items[0].type = TYPE_INT;
  items[0].n_subscripts = 0;
  items[0].dimensions = NULL;
  items[0].root = (char *)&config->value;
  items[0].def_root = (char *)&config->value_default;
  items[0].data_size = sizeof(config->value);

  items[1].name = (char *)"gain";
  items[1].type = TYPE_DOUBLE;
  items[1].n_subscripts = 0;
  items[1].dimensions = NULL;
  items[1].root = (char *)&config->gain;
  items[1].def_root = (char *)&config->gain_default;
  items[1].data_size = sizeof(config->gain);

  items[2].name = (char *)"label";
  items[2].type = TYPE_STRING;
  items[2].n_subscripts = 0;
  items[2].dimensions = NULL;
  items[2].root = (char *)&config->label;
  items[2].def_root = (char *)&config->label_default;
  items[2].data_size = sizeof(config->label);

  nl->item_list = items;
  nl->n_items = 3;
  nl->name = (char *)"thread";
}

static void init_value_namelist(NAMELIST *nl, ITEM *item, Config *config)
{
  item->name = (char *)"value";
  item->type = TYPE_INT;
  item->n_subscripts = 0;
  item->dimensions = NULL;
  item->root = (char *)&config->value;
  item->def_root = (char *)&config->value_default;
  item->data_size = sizeof(config->value);

  nl->item_list = item;
  nl->n_items = 1;
  nl->name = (char *)"thread";
}

static void cleanup_config(Config *config)
{
  if (config->label && config->label != config->label_default)
    free(config->label);
  config->label = config->label_default;
}

static char *print_to_string(NAMELIST *nl, long flags)
{
  char *output = NULL;
  size_t output_size = 0;
  FILE *fp = open_memstream(&output, &output_size);

  if (!fp)
    return NULL;
  set_print_namelist_flags(flags);
  print_namelist(fp, nl);
  if (fclose(fp)) {
    free(output);
    return NULL;
  }
  return output;
}

static char *print_current_to_string(NAMELIST *nl)
{
  char *output = NULL;
  size_t output_size = 0;
  FILE *fp = open_memstream(&output, &output_size);

  if (!fp)
    return NULL;
  print_namelist(fp, nl);
  if (fclose(fp)) {
    free(output);
    return NULL;
  }
  return output;
}

static int check_escape_quotes(Worker *worker, long iteration)
{
  char text[128];
  char expected[128];

  snprintf(text, sizeof(text), "worker-%ld \"quoted\" value-%ld",
           worker->id, iteration);
  snprintf(expected, sizeof(expected), "worker-%ld \\\"quoted\\\" value-%ld",
           worker->id, iteration);
  escape_quotes(text);
  if (strcmp(text, expected)) {
    set_error(worker, "escape_quotes scratch buffer interleaved");
    return 1;
  }
  return 0;
}

static int check_scan_namelist(Worker *worker, long iteration)
{
  NAMELIST_TEXT text;
  long repeat = 2 + (worker->id % 4);
  long value = worker->id * 100000 + iteration;
  char line[256];
  char expected_label[64];

  snprintf(expected_label, sizeof(expected_label), "worker-%ld", worker->id);
  snprintf(line, sizeof(line),
           "&thread value = %ld*%ld, gain = %.6f, label = \"worker-%ld\" &end",
           repeat, value, (double)value / 1000.0, worker->id);
  if (scan_namelist(&text, line) != 3) {
    set_error(worker, "scan_namelist returned unexpected entity count");
    return 1;
  }
  if (strcmp(text.group_name, "thread") ||
      strcmp(text.entity[0], "value") ||
      text.n_values[0] != 1 ||
      text.repeat[0][0] != repeat ||
      atol(text.value[0][0]) != value ||
      strcmp(text.entity[2], "label") ||
      strcmp(text.value[2][0], expected_label)) {
    free_namelist_text(&text);
    set_error(worker, "scan_namelist parsed values changed");
    return 1;
  }
  free_namelist_text(&text);
  return 0;
}

static int check_scanargs(Worker *worker, long iteration)
{
  SCANNED_ARG *scanned = NULL;
  char option[256];
  char filename[64];
  char expected_first[64];
  char expected_second[64];
  char *argv[3];
  int argc;

  snprintf(option, sizeof(option),
           "-items=alpha\\,worker%ld,(beta,%ld),gamma",
           worker->id, iteration);
  snprintf(filename, sizeof(filename), "file-%ld-%ld.sdds",
           worker->id, iteration);
  snprintf(expected_first, sizeof(expected_first), "alpha,worker%ld",
           worker->id);
  snprintf(expected_second, sizeof(expected_second), "beta,%ld",
           iteration);

  argv[0] = (char *)"program";
  argv[1] = option;
  argv[2] = filename;
  argc = scanargs(&scanned, 3, argv);
  if (argc != 3 ||
      scanned[1].arg_type != OPTION ||
      scanned[1].n_items != 4 ||
      strcmp(scanned[1].list[0], "items") ||
      strcmp(scanned[1].list[1], expected_first) ||
      strcmp(scanned[1].list[2], expected_second) ||
      strcmp(scanned[1].list[3], "gamma") ||
      scanned[2].arg_type != A_LIST ||
      strcmp(scanned[2].list[0], filename)) {
    if (scanned)
      free_scanargs(&scanned, argc > 0 ? argc : 0);
    set_error(worker, "scanargs parseList scratch state interleaved");
    return 1;
  }
  free_scanargs(&scanned, argc);
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_escape_quotes(worker, i) ||
        check_scan_namelist(worker, i) ||
        check_scanargs(worker, i)) {
      return worker;
    }
  }
  return NULL;
}

static void *process_global_settings_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  Config config;
  NAMELIST nl;
  ITEM items[3];
  ITEM item;
  NAMELIST_TEXT text;
  char *printed;

  if (get_namelist_buffer_size() != 30000) {
    set_error(worker, "namelist buffer size was not visible across threads");
    return worker;
  }

  init_config(&config);
  config.value = 11;
  config.value_default = -11;
  config.gain = 0;
  config.gain_default = -1;
  init_namelist(&nl, items, &config);
  if (scan_namelist(&text, "&thread gain = 2.5 &end") != 1) {
    set_error(worker, "scan_namelist failed for process-global settings test");
    cleanup_config(&config);
    return worker;
  }
  if (process_namelist(&nl, &text) != 1) {
    free_namelist_text(&text);
    cleanup_config(&config);
    set_error(worker, "process_namelist failed for process-global settings test");
    return worker;
  }
  if (config.value != config.value_default || !nearly_equal(config.gain, 2.5)) {
    free_namelist_text(&text);
    cleanup_config(&config);
    set_error(worker, "namelist processing flags were not visible across threads");
    return worker;
  }
  free_namelist_text(&text);
  cleanup_config(&config);

  init_config(&config);
  config.value = 42;
  config.value_default = 42;
  init_value_namelist(&nl, &item, &config);
  printed = print_current_to_string(&nl);
  if (!printed || strcmp(printed, "&thread &end\n")) {
    free(printed);
    cleanup_config(&config);
    set_error(worker, "print_namelist flags were not visible across threads");
    return worker;
  }
  free(printed);
  cleanup_config(&config);
  return NULL;
}

static int check_process_global_settings_visible_across_threads(void)
{
  pthread_t thread;
  Worker worker;
  void *result = NULL;

  worker.id = 1;
  worker.error[0] = 0;
  set_namelist_buffer_size(30000);
  set_namelist_processing_flags(STICKY_NAMELIST_DEFAULTS);
  set_print_namelist_flags(PRINT_NAMELIST_NODEFAULTS);
  if (pthread_create(&thread, NULL, process_global_settings_worker, &worker)) {
    fprintf(stderr, "pthread_create failed for process-global namelist settings test\n");
    set_namelist_processing_flags(0);
    set_print_namelist_flags(0);
    return 1;
  }
  if (pthread_join(thread, &result)) {
    fprintf(stderr, "pthread_join failed for process-global namelist settings test\n");
    set_namelist_processing_flags(0);
    set_print_namelist_flags(0);
    return 1;
  }
  set_namelist_processing_flags(0);
  set_print_namelist_flags(0);
  if (result || worker.error[0]) {
    fprintf(stderr, "process-global namelist settings failed: %s\n", worker.error);
    return 1;
  }
  return 0;
}

static int run_thread_test(void)
{
  pthread_t threads[THREADS];
  Worker workers[THREADS];
  long i;

  for (i = 0; i < THREADS; i++) {
    workers[i].id = i + 1;
    workers[i].error[0] = 0;
    if (pthread_create(&threads[i], NULL, thread_worker, &workers[i])) {
      fprintf(stderr, "pthread_create failed\n");
      return 1;
    }
  }

  for (i = 0; i < THREADS; i++) {
    void *result = NULL;
    if (pthread_join(threads[i], &result)) {
      fprintf(stderr, "pthread_join failed\n");
      return 1;
    }
    if (result || workers[i].error[0]) {
      fprintf(stderr, "thread %ld failed: %s\n", i + 1, workers[i].error);
      return 1;
    }
  }

  if (check_process_global_settings_visible_across_threads())
    return 1;

  puts("thread-safe");
  return 0;
}

static int run_regression(void)
{
  Config config;
  NAMELIST nl;
  ITEM items[3];
  NAMELIST_TEXT text;
  char quote[128] = "alpha \"beta\" gamma";
  char line[256] = "&thread value = 42, gain = 3.25, label = \"hello world\" &end";
  char *printed;

  set_namelist_buffer_size(20000);
  printf("buffer_size=%ld\n", get_namelist_buffer_size());
  printf("escape=%s\n", escape_quotes(quote));

  if (scan_namelist(&text, line) != 3)
    return 1;
  printf("scan=%s:%ld:%s:%ld*%s:%s:%ld*%s:%s:%ld*%s\n",
         text.group_name,
         text.n_entities,
         text.entity[0],
         text.repeat[0][0],
         text.value[0][0],
         text.entity[1],
         text.repeat[1][0],
         text.value[1][0],
         text.entity[2],
         text.repeat[2][0],
         text.value[2][0]);

  init_config(&config);
  init_namelist(&nl, items, &config);
  set_namelist_processing_flags(STICKY_NAMELIST_DEFAULTS);
  if (process_namelist(&nl, &text) != 3) {
    free_namelist_text(&text);
    return 1;
  }
  printf("processed=%d:%.2f:%s\n", config.value, config.gain, config.label);

  printed = print_to_string(&nl, 0);
  if (!printed) {
    free_namelist_text(&text);
    cleanup_config(&config);
    return 1;
  }
  printf("print_full=%s", printed);
  free(printed);

  printed = print_to_string(&nl, PRINT_NAMELIST_COMPACT);
  if (!printed) {
    free_namelist_text(&text);
    cleanup_config(&config);
    return 1;
  }
  printf("print_compact=%s", printed);
  free(printed);

  config.value = config.value_default;
  config.gain = config.gain_default;
  cleanup_config(&config);
  printed = print_to_string(&nl, PRINT_NAMELIST_NODEFAULTS);
  if (!printed) {
    free_namelist_text(&text);
    return 1;
  }
  printf("print_nodefaults=%s", printed);
  free(printed);

  free_namelist_text(&text);
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
def namelist_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("namelist libraries not built")

  tmp_path = tmp_path_factory.mktemp("namelist_thread_safety")
  source = tmp_path / "namelist_thread_safety.c"
  executable = tmp_path / "namelist_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I",
    str(ROOT_DIR / "include"),
    str(source),
    str(ROOT_DIR / "namelist" / "scanargs.c"),
    "-L",
    str(LIB_DIR),
    "-lnamelist",
    "-lrpnlib",
    "-lmdbmth",
    "-lmdblib",
    "-lgsl",
    "-lgslcblas",
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


def test_namelist_api_regression_outputs(namelist_harness):
  result = run_harness(namelist_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "buffer_size=20000\n"
    "escape=alpha \\\"beta\\\" gamma\n"
    "scan=thread:3:value:1*42:gain:1*3.25:label:1*hello world\n"
    "processed=42:3.25:hello world\n"
    "print_full=&thread\n"
    "    value = 42,\n"
    "    gain = 3.250000000000000e+00,\n"
    "    label = \"hello world\",\n"
    "&end\n"
    "print_compact=&thread\n"
    " value = 42,  gain = 3.250000000000000e+00,  label = \"hello world\", &end\n"
    "print_nodefaults=&thread &end\n"
  )


def test_namelist_api_is_thread_safe_without_caller_locks(namelist_harness):
  result = run_harness(namelist_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
