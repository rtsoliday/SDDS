import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import (
  built_library,
  PLATFORM_ID,
  ROOT_DIR,
  external_include_args,
  external_library_args,
)


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  built_library("mdbcommon"),
  built_library("SDDS1"),
  built_library("namelist"),
  built_library("rpnlib"),
  built_library("matlib"),
  built_library("fftpack"),
  built_library("mdbmth"),
  built_library("mdblib"),
]

HARNESS_SOURCE = r"""
#define _GNU_SOURCE

#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdb.h"
#include "scan.h"

#define THREADS 8
#define ITERATIONS 200
#define RCDS_INTERVAL 50

long linescan(double (*func)(double *x, long *invalid),
              double *x0, double f0, double *dv,
              double *lowerLimit, double *upperLimit,
              long dimensions, double alo, double ahi, long Np,
              double **stepList, double **fList, long n_list,
              double *xm, double *fm, double *xmin, double *fmin);

typedef struct {
  long id;
  char error[256];
} Worker;

static int nearly_equal(double actual, double expected, double tolerance)
{
  return fabs(actual - expected) <= tolerance;
}

static double display_zero(double value)
{
  return fabs(value) < 0.5e-6 ? 0.0 : value;
}

static void set_error(Worker *worker, const char *message)
{
  snprintf(worker->error, sizeof(worker->error), "%s", message);
}

static void free_list(char **items, int count)
{
  int i;

  if (!items)
    return;
  for (i = 0; i < count; i++)
    free(items[i]);
  free(items);
}

static double polynomial_basis(double x, long ord)
{
  double value = 1.0;

  while (ord-- > 0)
    value *= x;
  return value;
}

static double rcds_quadratic(double *x, long *invalid)
{
  *invalid = 0;
  return (x[0] - 0.25) * (x[0] - 0.25) +
         (x[1] + 0.35) * (x[1] + 0.35);
}

static double rcds_invalid_low_value(double *x, long *invalid)
{
  if (fabs(x[0] - 0.2) < 1e-12) {
    *invalid = 1;
    return -1000.0;
  }
  *invalid = 0;
  return 1.0 + x[0] * x[0];
}

static int check_parse_list(Worker *worker, long iteration)
{
  char text[256];
  char expected0[128];
  char expected2[128];
  char **items = NULL;
  int count;
  int failed = 0;

  snprintf(text, sizeof(text), "alpha=(%ld,%ld),literal\\,comma,(tail%ld)",
           worker->id, iteration, worker->id + iteration);
  snprintf(expected0, sizeof(expected0), "alpha=%ld,%ld", worker->id, iteration);
  snprintf(expected2, sizeof(expected2), "tail%ld", worker->id + iteration);

  count = parseList(&items, text);
  if (count != 3 ||
      strcmp(items[0], expected0) ||
      strcmp(items[1], "literal,comma") ||
      strcmp(items[2], expected2)) {
    set_error(worker, "parseList item buffer interleaved");
    failed = 1;
  }
  free_list(items, count);
  return failed;
}

static int check_fits(Worker *worker, long iteration)
{
  double x[6] = {-2, -1, 0, 1, 2, 3};
  double y[6];
  double coef[3], sigma[3], diff[6], chi;
  long powers[2] = {0, 2};
  int32_t orders[3] = {0, 1, 2};
  double offset = worker->id * 10.0 + iteration * 0.01;
  long i;

  for (i = 0; i < 6; i++)
    y[i] = 1.0 + offset + 2.0 * x[i] + 3.0 * x[i] * x[i];
  if (!lsfn(x, y, NULL, 6, 2, coef, sigma, &chi, diff) ||
      !nearly_equal(coef[0], 1.0 + offset, 1e-9) ||
      !nearly_equal(coef[1], 2.0, 1e-9) ||
      !nearly_equal(coef[2], 3.0, 1e-9)) {
    set_error(worker, "lsfn matrix workspace interleaved");
    return 1;
  }

  for (i = 0; i < 6; i++)
    y[i] = 5.0 + offset + 3.0 * x[i] * x[i];
  if (!lsfp(x, y, NULL, 6, 2, powers, coef, sigma, &chi, diff) ||
      !nearly_equal(coef[0], 5.0 + offset, 1e-9) ||
      !nearly_equal(coef[1], 3.0, 1e-9)) {
    set_error(worker, "lsfp matrix workspace interleaved");
    return 1;
  }

  for (i = 0; i < 6; i++)
    y[i] = 2.0 + offset - x[i] + 0.5 * x[i] * x[i];
  if (!lsfg(x, y, NULL, 6, 3, orders, coef, sigma, &chi, diff, polynomial_basis) ||
      !nearly_equal(coef[0], 2.0 + offset, 1e-9) ||
      !nearly_equal(coef[1], -1.0, 1e-9) ||
      !nearly_equal(coef[2], 0.5, 1e-9)) {
    set_error(worker, "lsfg matrix workspace interleaved");
    return 1;
  }

  return 0;
}

static int check_savitzky_golay(Worker *worker, long iteration)
{
  long n_left = 2 + ((worker->id + iteration) & 1);
  long n_right = 2;
  long np = n_left + n_right + 1;
  double coeff[6];
  double sum, first, second;
  double base = worker->id * 0.01 + iteration * 0.0001;
  double data[5] = {base + 1, base + 2, base + 4, base + 8, base + 16};
  double original[5];
  double expected[5];
  long i;

  SavitzkyGolayCoefficients(coeff, np, 2, n_left, n_right, 0, 1);
  sum = coeff[0];
  first = 0.0;
  second = 0.0;
  for (i = 1; i <= n_left; i++) {
    sum += coeff[i];
    first -= i * coeff[i];
    second += i * i * coeff[i];
  }
  for (i = 1; i <= n_right; i++) {
    sum += coeff[np - i];
    first += i * coeff[np - i];
    second += i * i * coeff[np - i];
  }
  if (!nearly_equal(sum, 1.0, 1e-9) ||
      !nearly_equal(first, 0.0, 1e-9) ||
      !nearly_equal(second, 0.0, 1e-9)) {
    set_error(worker, "SavitzkyGolayCoefficients cache interleaved");
    return 1;
  }

  memcpy(original, data, sizeof(original));
  expected[0] = (original[0] + original[0] + original[1]) / 3.0;
  expected[1] = (original[0] + original[1] + original[2]) / 3.0;
  expected[2] = (original[1] + original[2] + original[3]) / 3.0;
  expected[3] = (original[2] + original[3] + original[4]) / 3.0;
  expected[4] = (original[3] + original[4] + original[4]) / 3.0;
  if (!SavitzkyGolaySmooth(data, 5, 1, 1, 1, 0)) {
    set_error(worker, "SavitzkyGolaySmooth failed");
    return 1;
  }
  for (i = 0; i < 5; i++) {
    if (!nearly_equal(data[i], expected[i], 1e-12)) {
      set_error(worker, "SavitzkyGolaySmooth buffer interleaved");
      return 1;
    }
  }

  return 0;
}

static int check_rcds_min(Worker *worker)
{
  double y;
  double best[2];
  double guess[2] = {0.85, -0.75};
  double step[2] = {0.2, 0.2};
  double lower[2] = {-1.0, -1.0};
  double upper[2] = {1.0, 1.0};
  long evaluations;

  evaluations = rcdsMin(&y, best, guess, step, lower, upper, NULL, 2,
                        1e-10, 1e-12, rcds_quadratic, NULL,
                        5000, 15, 0.0, 0.05, 0);
  if (evaluations <= 0 ||
      y > 5e-3 ||
      fabs(best[0] - 0.25) > 0.05 ||
      fabs(best[1] + 0.35) > 0.05) {
    set_error(worker, "rcdsMin workspace interleaved");
    return 1;
  }
  return 0;
}

static void init_rcds_directions(double d0[2], double d1[2], double *dmat[2])
{
  d0[0] = 1.0;
  d0[1] = 0.0;
  d1[0] = 0.0;
  d1[1] = 1.0;
  dmat[0] = d0;
  dmat[1] = d1;
}

static int check_rcds_caller_owned_directions(void)
{
  double y;
  double best[2];
  double guess[2] = {0.85, -0.75};
  double step[2] = {0.2, 0.2};
  double lower[2] = {-1.0, -1.0};
  double upper[2] = {1.0, 1.0};
  double d0[2], d1[2];
  double *dmat[2];
  long evaluations;

  init_rcds_directions(d0, d1, dmat);
  evaluations = rcdsMin(&y, best, guess, step, lower, upper, dmat, 2,
                        10.0, 1e-12, rcds_quadratic, NULL,
                        5000, 15, 0.0, 0.05, 0);
  if (evaluations <= 0)
    return 1;

  init_rcds_directions(d0, d1, dmat);
  evaluations = rcdsMin(&y, best, guess, step, lower, upper, dmat, 2,
                        1e-10, 1e-12, rcds_quadratic, NULL,
                        5000, 15, 0.0, 0.05, 0);
  return evaluations <= 0;
}

static int check_rcds_invalid_candidate_is_ignored(void)
{
  double x0[1] = {0.0};
  double dv[1] = {1.0};
  double step_storage[100];
  double f_storage[100];
  double *step_list = step_storage;
  double *f_list = f_storage;
  double xm[1] = {0.0};
  double xmin[1] = {0.0};
  double fm = 0.5;
  double fmin = 0.5;
  long evaluations;

  step_storage[0] = 0.0;
  f_storage[0] = 0.5;
  evaluations = linescan(rcds_invalid_low_value, x0, 0.5, dv, NULL, NULL, 1,
                         -1.0, 1.0, 6, &step_list, &f_list, 1,
                         xm, &fm, xmin, &fmin);
  if (evaluations <= 0)
    return 1;
  return fmin < 0.0 || fabs(xmin[0]) > 1e-12;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_parse_list(worker, i) ||
        check_fits(worker, i) ||
        check_savitzky_golay(worker, i))
      return worker;
    if ((worker->id & 1) == 0 && (i % RCDS_INTERVAL) == 0) {
      if (check_rcds_min(worker))
        return worker;
    }
    sched_yield();
  }
  return NULL;
}

static void *rcds_abort_reader_worker(void *arg)
{
  Worker *worker = (Worker *)arg;

  if (!rcdsMinAbort(0)) {
    set_error(worker, "rcdsMinAbort state was not visible across threads");
    return worker;
  }
  return NULL;
}

static int check_rcds_abort_visible_across_threads(void)
{
  pthread_t thread;
  Worker worker;
  void *status = NULL;

  worker.id = 1;
  worker.error[0] = 0;
  rcdsMinAbort(1);
  if (pthread_create(&thread, NULL, rcds_abort_reader_worker, &worker)) {
    fprintf(stderr, "pthread_create failed for rcds abort visibility test\n");
    return 1;
  }
  if (pthread_join(thread, &status)) {
    fprintf(stderr, "pthread_join failed for rcds abort visibility test\n");
    return 1;
  }
  if (status || worker.error[0]) {
    fprintf(stderr, "rcds abort visibility failed: %s\n", worker.error);
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

  if (check_rcds_abort_visible_across_threads())
    return 1;

  puts("thread-safe");
  return 0;
}

static int run_regression(void)
{
  char list_text[] = "alpha=(one,two),literal\\,comma,(tail)";
  char **items = NULL;
  int count;
  double x[6] = {-2, -1, 0, 1, 2, 3};
  double y[6];
  double coef[3], sigma[3], diff[6], chi;
  long powers[2] = {0, 2};
  int32_t orders[3] = {0, 1, 2};
  double sg_coeff[5];
  double smooth[5] = {1, 2, 4, 8, 16};
  double rcds_y;
  double rcds_best[2];
  double guess[2] = {0.85, -0.75};
  double step[2] = {0.2, 0.2};
  double lower[2] = {-1.0, -1.0};
  double upper[2] = {1.0, 1.0};
  long evaluations;
  long i;

  count = parseList(&items, list_text);
  if (count != 3)
    return 1;
  printf("parseList=%d:%s:%s:%s\n", count, items[0], items[1], items[2]);
  free_list(items, count);

  for (i = 0; i < 6; i++)
    y[i] = 1.0 + 2.0 * x[i] + 3.0 * x[i] * x[i];
  if (!lsfn(x, y, NULL, 6, 2, coef, sigma, &chi, diff))
    return 1;
  printf("lsfn=%.6f:%.6f:%.6f:%.6f\n",
         coef[0], coef[1], coef[2], display_zero(chi));

  for (i = 0; i < 6; i++)
    y[i] = 5.0 + 3.0 * x[i] * x[i];
  if (!lsfp(x, y, NULL, 6, 2, powers, coef, sigma, &chi, diff))
    return 1;
  printf("lsfp=%.6f:%.6f:%.6f\n",
         coef[0], coef[1], display_zero(chi));

  for (i = 0; i < 6; i++)
    y[i] = 2.0 - x[i] + 0.5 * x[i] * x[i];
  if (!lsfg(x, y, NULL, 6, 3, orders, coef, sigma, &chi, diff, polynomial_basis))
    return 1;
  printf("lsfg=%.6f:%.6f:%.6f:%.6f\n",
         coef[0], coef[1], coef[2], display_zero(chi));

  SavitzkyGolayCoefficients(sg_coeff, 5, 2, 2, 2, 0, 1);
  printf("sg_coeff=%.6f:%.6f:%.6f:%.6f:%.6f\n",
         sg_coeff[0], sg_coeff[1], sg_coeff[2], sg_coeff[3], sg_coeff[4]);
  if (!SavitzkyGolaySmooth(smooth, 5, 1, 1, 1, 0))
    return 1;
  printf("sg_smooth=%.6f:%.6f:%.6f:%.6f:%.6f\n",
         smooth[0], smooth[1], smooth[2], smooth[3], smooth[4]);

  evaluations = rcdsMin(&rcds_y, rcds_best, guess, step, lower, upper, NULL, 2,
                        1e-10, 1e-12, rcds_quadratic, NULL,
                        5000, 15, 0.0, 0.05, 0);
  if (evaluations <= 0)
    return 1;
  printf("rcds=%ld:%.6f:%.6f:%.6f\n",
         evaluations, rcds_y, rcds_best[0], rcds_best[1]);
  if (check_rcds_caller_owned_directions())
    return 1;
  printf("rcds_caller_directions=preserved\n");
  if (check_rcds_invalid_candidate_is_ignored())
    return 1;
  printf("rcds_invalid_candidate=ignored\n");

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
def mdbcommon_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("mdbcommon libraries not built")

  tmp_path = tmp_path_factory.mktemp("mdbcommon_thread_safety")
  source = tmp_path / "mdbcommon_thread_safety.c"
  executable = tmp_path / "mdbcommon_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
  ]
  if platform.system() == "Linux":
    command.append("-fopenmp")
  command.extend([
    "-I",
    str(ROOT_DIR / "include"),
    *external_include_args("gsl", "lzma"),
    str(source),
    "-L",
    str(LIB_DIR),
  ])
  if platform.system() == "Linux":
    command.extend([
      "-Wl,--start-group",
      "-lmdbcommon",
      "-lSDDS1",
      "-lnamelist",
      "-lrpnlib",
      "-lmatlib",
      "-lfftpack",
      "-lmdbmth",
      "-lmdblib",
      "-Wl,--end-group",
    ])
  else:
    command.extend([
      "-lmdbcommon",
      "-lSDDS1",
      "-lnamelist",
      "-lrpnlib",
      "-lmatlib",
      "-lfftpack",
      "-lmdbmth",
      "-lmdblib",
    ])
  command.extend([
    *external_library_args("gsl", "gslcblas", "lzma", "z"),
    "-lpthread",
    "-lm",
  ])
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


def test_mdbcommon_regression_outputs(mdbcommon_harness):
  result = run_harness(mdbcommon_harness)
  assert result.stderr == ""
  lines = result.stdout.splitlines()
  assert lines[:6] == (
    "parseList=3:alpha=one,two:literal,comma:tail\n"
    "lsfn=1.000000:2.000000:3.000000:0.000000\n"
    "lsfp=5.000000:3.000000:0.000000\n"
    "lsfg=2.000000:-1.000000:0.500000:0.000000\n"
    "sg_coeff=0.485714:0.342857:-0.085714:-0.085714:0.342857\n"
    "sg_smooth=1.333333:2.333333:4.666667:9.333333:13.333333\n"
  ).splitlines()
  rcds = lines[6].replace("=", ":", 1).split(":")
  assert rcds[0] == "rcds"
  assert 200 <= int(rcds[1]) <= 300
  assert abs(float(rcds[2]) - 0.00175) < 0.0001
  assert abs(float(rcds[3]) - 0.292) < 0.001
  assert abs(float(rcds[4]) + 0.3506) < 0.001
  assert lines[7:] == [
    "rcds_caller_directions=preserved",
    "rcds_invalid_candidate=ignored",
  ]


def test_mdbcommon_is_thread_safe_without_caller_locks(mdbcommon_harness):
  result = run_harness(mdbcommon_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
