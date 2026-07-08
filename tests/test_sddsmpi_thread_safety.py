import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libSDDS1mpi.a",
  LIB_DIR / "libnamelist.a",
  LIB_DIR / "librpnlib.a",
  LIB_DIR / "libmdbmth.a",
  LIB_DIR / "libmdblib.a",
]

HARNESS_SOURCE = r"""
#define _GNU_SOURCE

#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDDS.h"

extern int32_t SDDS_GetTerminateMode(void);

#define THREADS 8
#define ITERATIONS 500
#define INITIAL_READ_BUFFER 111111
#define INITIAL_WRITE_BUFFER 222222
#define INITIAL_TITLE_BUFFER 333333
#define INITIAL_STRING_LENGTH 44
#define INITIAL_TERMINATE_MODE (TERMINATE_DONT_FREE_TABLE_STRINGS | TERMINATE_DONT_FREE_ARRAY_STRINGS)

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

static int check_initial_global_settings(Worker *worker)
{
  if (SDDS_SetDefaultReadBufferSize(-1) != INITIAL_READ_BUFFER) {
    set_error(worker, "read buffer size did not match initial global value");
    return 1;
  }
  if (SDDS_SetDefaultWriteBufferSize(-1) != INITIAL_WRITE_BUFFER) {
    set_error(worker, "write buffer size did not match initial global value");
    return 1;
  }
  if (SDDS_SetDefaultTitleBufferSize(-1) != INITIAL_TITLE_BUFFER) {
    set_error(worker, "title buffer size did not match initial global value");
    return 1;
  }
  if (SDDS_SetDefaultStringLength(-1) != INITIAL_STRING_LENGTH) {
    set_error(worker, "string length did not match initial global value");
    return 1;
  }
  if (SDDS_GetTerminateMode() != INITIAL_TERMINATE_MODE) {
    set_error(worker, "terminate mode did not match initial global value");
    return 1;
  }
  if (SDDS_CheckStringTruncated() != 1) {
    set_error(worker, "string truncation count did not match initial global value");
    return 1;
  }
  return 0;
}

static int check_default_read_buffer_size(Worker *worker, long iteration)
{
  int32_t expected = 10000 + (int32_t)worker->id * 1000 + (int32_t)(iteration % 97);

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetDefaultReadBufferSize(expected);
  sched_yield();
  if (SDDS_SetDefaultReadBufferSize(-1) != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "read buffer size state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_default_write_buffer_size(Worker *worker, long iteration)
{
  int32_t expected = 20000 + (int32_t)worker->id * 1000 + (int32_t)(iteration % 89);

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetDefaultWriteBufferSize(expected);
  sched_yield();
  if (SDDS_SetDefaultWriteBufferSize(-1) != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "write buffer size state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_default_title_buffer_size(Worker *worker, long iteration)
{
  int32_t expected = 30000 + (int32_t)worker->id * 1000 + (int32_t)(iteration % 83);

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetDefaultTitleBufferSize(expected);
  sched_yield();
  if (SDDS_SetDefaultTitleBufferSize(-1) != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "title buffer size state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_default_string_length(Worker *worker, long iteration)
{
  int32_t expected = 10 + (int32_t)worker->id * 100 + (int32_t)(iteration % 79);

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetDefaultStringLength(expected);
  sched_yield();
  if (SDDS_SetDefaultStringLength(-1) != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "string length state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_terminate_mode(Worker *worker, long iteration)
{
  uint32_t expected = (uint32_t)(worker->id * 16 + (iteration & 7));

  pthread_mutex_lock(&process_global_settings_mutex);
  SDDS_SetTerminateMode(expected);
  sched_yield();
  if ((uint32_t)SDDS_GetTerminateMode() != expected) {
    pthread_mutex_unlock(&process_global_settings_mutex);
    set_error(worker, "terminate mode state interleaved");
    return 1;
  }
  pthread_mutex_unlock(&process_global_settings_mutex);
  return 0;
}

static int check_string_truncation_count(Worker *worker)
{
  (void)worker;
  SDDS_StringTuncated();
  sched_yield();
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;
  int initial_global_failed;

  initial_global_failed = check_initial_global_settings(worker);
  if (finish_initial_global_checks(worker, initial_global_failed))
    return worker;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_default_read_buffer_size(worker, i) ||
        check_default_write_buffer_size(worker, i) ||
        check_default_title_buffer_size(worker, i) ||
        check_default_string_length(worker, i) ||
        check_terminate_mode(worker, i) ||
        check_string_truncation_count(worker))
      return worker;
  }
  return NULL;
}

static int run_thread_test(void)
{
  pthread_t threads[THREADS];
  Worker workers[THREADS];
  long i;

  SDDS_SetDefaultReadBufferSize(INITIAL_READ_BUFFER);
  SDDS_SetDefaultWriteBufferSize(INITIAL_WRITE_BUFFER);
  SDDS_SetDefaultTitleBufferSize(INITIAL_TITLE_BUFFER);
  SDDS_SetDefaultStringLength(INITIAL_STRING_LENGTH);
  SDDS_SetTerminateMode(INITIAL_TERMINATE_MODE);
  SDDS_StringTuncated();

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

  if (SDDS_CheckStringTruncated() != 1 + THREADS * ITERATIONS) {
    fprintf(stderr, "string truncation count lost increments\n");
    return 1;
  }

  puts("thread-safe");
  return 0;
}

static int run_regression(void)
{
  int32_t previous_read;
  int32_t previous_write;
  int32_t previous_title;
  int32_t previous_string;

  previous_read = SDDS_SetDefaultReadBufferSize(123456);
  previous_write = SDDS_SetDefaultWriteBufferSize(234567);
  previous_title = SDDS_SetDefaultTitleBufferSize(345678);
  previous_string = SDDS_SetDefaultStringLength(56);
  SDDS_SetTerminateMode(TERMINATE_DONT_FREE_TABLE_STRINGS);
  SDDS_StringTuncated();

  printf("read_buffer=%d:%d\n", previous_read, SDDS_SetDefaultReadBufferSize(-1));
  printf("write_buffer=%d:%d\n", previous_write, SDDS_SetDefaultWriteBufferSize(-1));
  printf("title_buffer=%d:%d\n", previous_title, SDDS_SetDefaultTitleBufferSize(-1));
  printf("string_length=%d:%d\n", previous_string, SDDS_SetDefaultStringLength(-1));
  printf("terminate_mode=%d\n", SDDS_GetTerminateMode());
  printf("truncated=%d\n", SDDS_CheckStringTruncated());

  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "threads"))
    return run_thread_test();
  return run_regression();
}
"""


def mpi_compiler_command():
  for name in ("MPI_CC", "MPICC"):
    value = os.environ.get(name)
    if value:
      command = shlex.split(value)
      if command and shutil.which(command[0]):
        return command
  for name in ("mpicc", "/usr/lib64/mpich/bin/mpicc"):
    command = shlex.split(name)
    if shutil.which(command[0]):
      return command
  pytest.skip("MPI C compiler not available")


@pytest.fixture(scope="module")
def sddsmpi_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("SDDS MPI libraries not built")

  tmp_path = tmp_path_factory.mktemp("sddsmpi_thread_safety")
  source = tmp_path / "sddsmpi_thread_safety.c"
  executable = tmp_path / "sddsmpi_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *mpi_compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-DSDDS_MPI_IO=1",
    "-I",
    str(ROOT_DIR / "include"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lSDDS1mpi",
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


def test_sddsmpi_regression_outputs(sddsmpi_harness):
  result = run_harness(sddsmpi_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "read_buffer=4000000:123456\n"
    "write_buffer=0:234567\n"
    "title_buffer=2400000:345678\n"
    "string_length=16:56\n"
    "terminate_mode=1\n"
    "truncated=1\n"
  )


def test_sddsmpi_process_global_defaults_are_thread_safe(sddsmpi_harness):
  result = run_harness(sddsmpi_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
