import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libxls.a",
]

HARNESS_SOURCE = r"""
#include <atomic>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "common/systype.h"

#define THREADS 8
#define ITERATIONS 10000
#define REPORTERS 4

typedef struct {
  long id;
  char error[256];
} Worker;

static std::atomic<long> reporter_counts[REPORTERS];
static std::atomic<long> invalid_reports;

static void note_invalid_report(const char *expr, const char *fname, int lineno, const char *funcname)
{
  if (!expr || strcmp(expr, "thread_expr") ||
      !fname || strcmp(fname, "thread_file.cpp") ||
      lineno < 0 || lineno >= THREADS ||
      !funcname || strcmp(funcname, "thread_func"))
    invalid_reports.fetch_add(1, std::memory_order_relaxed);
}

static void reporter0(const char *expr, const char *fname, int lineno, const char *funcname)
{
  note_invalid_report(expr, fname, lineno, funcname);
  reporter_counts[0].fetch_add(1, std::memory_order_relaxed);
}

static void reporter1(const char *expr, const char *fname, int lineno, const char *funcname)
{
  note_invalid_report(expr, fname, lineno, funcname);
  reporter_counts[1].fetch_add(1, std::memory_order_relaxed);
}

static void reporter2(const char *expr, const char *fname, int lineno, const char *funcname)
{
  note_invalid_report(expr, fname, lineno, funcname);
  reporter_counts[2].fetch_add(1, std::memory_order_relaxed);
}

static void reporter3(const char *expr, const char *fname, int lineno, const char *funcname)
{
  note_invalid_report(expr, fname, lineno, funcname);
  reporter_counts[3].fetch_add(1, std::memory_order_relaxed);
}

static xlslib_userdef_assertion_reporter *reporters[REPORTERS] = {
  reporter0,
  reporter1,
  reporter2,
  reporter3,
};

static long custom_count;
static char custom_expr[64];
static char custom_file[64];
static char custom_func[64];
static int custom_line;

static void custom_reporter(const char *expr, const char *fname, int lineno, const char *funcname)
{
  custom_count++;
  snprintf(custom_expr, sizeof(custom_expr), "%s", expr ? expr : "");
  snprintf(custom_file, sizeof(custom_file), "%s", fname ? fname : "");
  snprintf(custom_func, sizeof(custom_func), "%s", funcname ? funcname : "");
  custom_line = lineno;
}

static int run_regression(void)
{
  try {
    xlslib_report_failed_assertion("expr", "file.cpp", 77, "func");
    fprintf(stderr, "default reporter did not throw\n");
    return 1;
  } catch (const std::string& message) {
    printf("default=threw:%d\n", message.empty() ? 0 : 1);
  } catch (...) {
    fprintf(stderr, "default reporter threw unexpected type\n");
    return 1;
  }

  xlslib_register_assert_reporter(custom_reporter);
  xlslib_report_failed_assertion("custom_expr", "custom_file.cpp", 12, "custom_func");
  printf("custom=%ld:%s:%s:%d:%s\n",
         custom_count, custom_expr, custom_file, custom_line, custom_func);

  xlslib_register_assert_reporter(NULL);
  xlslib_report_failed_assertion("ignored", "ignored.cpp", 13, "ignored_func");
  printf("null=%ld\n", custom_count);
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  for (i = 0; i < ITERATIONS; i++) {
    xlslib_register_assert_reporter(reporters[(worker->id + i) % REPORTERS]);
    xlslib_report_failed_assertion("thread_expr", "thread_file.cpp",
                                   (int)worker->id, "thread_func");
  }

  return NULL;
}

static int run_thread_test(void)
{
  pthread_t threads[THREADS];
  Worker workers[THREADS];
  long i;
  long total = 0;

  for (i = 0; i < REPORTERS; i++)
    reporter_counts[i].store(0, std::memory_order_relaxed);
  invalid_reports.store(0, std::memory_order_relaxed);

  xlslib_register_assert_reporter(reporters[0]);

  for (i = 0; i < THREADS; i++) {
    workers[i].id = i;
    workers[i].error[0] = 0;
    if (pthread_create(&threads[i], NULL, thread_worker, &workers[i])) {
      fprintf(stderr, "pthread_create failed\n");
      return 1;
    }
  }

  for (i = 0; i < THREADS; i++) {
    if (pthread_join(threads[i], NULL)) {
      fprintf(stderr, "pthread_join failed\n");
      return 1;
    }
  }

  for (i = 0; i < REPORTERS; i++)
    total += reporter_counts[i].load(std::memory_order_relaxed);

  if (invalid_reports.load(std::memory_order_relaxed) != 0) {
    fprintf(stderr, "invalid report observed\n");
    return 1;
  }

  if (total != THREADS * ITERATIONS) {
    fprintf(stderr, "lost report: actual=%ld expected=%ld\n", total, (long)(THREADS * ITERATIONS));
    return 1;
  }

  puts("thread-safe");
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
  command = shlex.split(os.environ.get("CXX", "g++"))
  if not command or not shutil.which(command[0]):
    pytest.skip("C++ compiler not available")
  return command


@pytest.fixture(scope="module")
def xlslib_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("xlslib library not built")

  tmp_path = tmp_path_factory.mktemp("xlslib_thread_safety")
  source = tmp_path / "xlslib_thread_safety.cpp"
  executable = tmp_path / "xlslib_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *compiler_command(),
    "-std=c++11",
    "-Wall",
    "-Wextra",
    "-I",
    str(ROOT_DIR / "xlslib"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lxls",
    "-lpthread",
    "-o",
    str(executable),
  ]
  if platform.system() == "Linux":
    command[-2:-2] = ["-lrt", "-ldl", "-lgcc"]

  subprocess.run(command, check=True)
  return executable


def run_harness(executable, *args):
  return subprocess.run(
    [str(executable), *args],
    check=True,
    capture_output=True,
    text=True,
  )


def test_xlslib_assertion_reporter_regression_outputs(xlslib_harness):
  result = run_harness(xlslib_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "default=threw:1\n"
    "custom=1:custom_expr:custom_file.cpp:12:custom_func\n"
    "null=1\n"
  )


def test_xlslib_assertion_reporter_is_thread_safe(xlslib_harness):
  result = run_harness(xlslib_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
