import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libmatlib.a",
  LIB_DIR / "libmdblib.a",
]

HARNESS_SOURCE = r"""
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matlib.h"

#define THREADS 8
#define ITERATIONS 500

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

static void fill_matrix(MATRIX *matrix, long worker_id, long iteration)
{
  long i, j;
  double scale = 0.001 * (worker_id + 1) + 0.00001 * iteration;

  for (i = 0; i < matrix->n; i++) {
    for (j = 0; j < matrix->m; j++) {
      if (i == j)
        matrix->a[i][j] = matrix->n + 2.0 + scale + 0.05 * i;
      else
        matrix->a[i][j] = scale * (i + 1) * (j + 2);
    }
  }
}

static void fill_fmatrix(FMATRIX *matrix, long worker_id, long iteration)
{
  long i, j;
  float scale = (float)(0.001 * (worker_id + 1) + 0.00001 * iteration);

  for (i = 0; i < matrix->n; i++) {
    for (j = 0; j < matrix->m; j++) {
      if (i == j)
        matrix->a[i][j] = (float)(matrix->n + 2.0 + scale + 0.05 * i);
      else
        matrix->a[i][j] = scale * (float)((i + 1) * (j + 2));
    }
  }
}

static int check_identity_product(MATRIX *product, double tolerance)
{
  long i, j;

  for (i = 0; i < product->n; i++) {
    for (j = 0; j < product->m; j++) {
      double expected = i == j ? 1.0 : 0.0;
      if (!nearly_equal(product->a[i][j], expected, tolerance))
        return 0;
    }
  }
  return 1;
}

static int check_fidentity_product(FMATRIX *product, double tolerance)
{
  long i, j;

  for (i = 0; i < product->n; i++) {
    for (j = 0; j < product->m; j++) {
      double expected = i == j ? 1.0 : 0.0;
      if (!nearly_equal(product->a[i][j], expected, tolerance))
        return 0;
    }
  }
  return 1;
}

static int check_double_inversion(Worker *worker, long iteration)
{
  MATRIX *original = NULL;
  MATRIX *inverse = NULL;
  MATRIX *product = NULL;
  int size = 2 + (int)((worker->id + iteration) % 5);
  int failed = 0;

  m_alloc(&original, size, size);
  m_alloc(&inverse, size, size);
  m_alloc(&product, size, size);
  fill_matrix(original, worker->id, iteration);
  if (!m_invert(inverse, original) ||
      !m_mult(product, inverse, original) ||
      !check_identity_product(product, 1e-10)) {
    set_error(worker, "mat_invert scratch state interleaved");
    failed = 1;
  }
  m_free(&original);
  m_free(&inverse);
  m_free(&product);
  return failed;
}

static int check_float_inversion(Worker *worker, long iteration)
{
  FMATRIX *original = NULL;
  FMATRIX *inverse = NULL;
  FMATRIX *product = NULL;
  int size = 2 + (int)((worker->id + iteration) % 5);
  int failed = 0;

  fm_alloc(&original, size, size);
  fm_alloc(&inverse, size, size);
  fm_alloc(&product, size, size);
  fill_fmatrix(original, worker->id, iteration);
  if (!fm_invert(inverse, original) ||
      !fm_mult(product, inverse, original) ||
      !check_fidentity_product(product, 5e-5)) {
    set_error(worker, "fmat_invert scratch state interleaved");
    failed = 1;
  }
  fm_free(&original);
  fm_free(&inverse);
  fm_free(&product);
  return failed;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_double_inversion(worker, i) ||
        check_float_inversion(worker, i))
      return worker;
    sched_yield();
  }
  return NULL;
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

  puts("thread-safe");
  return 0;
}

static int run_regression(void)
{
  MATRIX *matrix = NULL;
  MATRIX *inverse = NULL;
  MATRIX *product = NULL;
  FMATRIX *fmatrix = NULL;
  FMATRIX *finverse = NULL;
  FMATRIX *fproduct = NULL;
  double det;
  int ok = 1;

  m_alloc(&matrix, 2, 2);
  m_alloc(&inverse, 2, 2);
  m_alloc(&product, 2, 2);
  matrix->a[0][0] = 4;
  matrix->a[0][1] = 7;
  matrix->a[1][0] = 2;
  matrix->a[1][1] = 6;
  det = m_det(matrix);
  ok = ok && m_invert(inverse, matrix) && m_mult(product, inverse, matrix);
  if (!ok)
    return 1;
  printf("double_det=%.6f\n", det);
  printf("double_inverse=%.6f:%.6f:%.6f:%.6f\n",
         inverse->a[0][0], inverse->a[0][1],
         inverse->a[1][0], inverse->a[1][1]);
  printf("double_product=%.6f:%.6f:%.6f:%.6f\n",
         display_zero(product->a[0][0]), display_zero(product->a[0][1]),
         display_zero(product->a[1][0]), display_zero(product->a[1][1]));
  m_free(&matrix);
  m_free(&inverse);
  m_free(&product);

  fm_alloc(&fmatrix, 2, 2);
  fm_alloc(&finverse, 2, 2);
  fm_alloc(&fproduct, 2, 2);
  fmatrix->a[0][0] = 4;
  fmatrix->a[0][1] = 7;
  fmatrix->a[1][0] = 2;
  fmatrix->a[1][1] = 6;
  ok = fm_invert(finverse, fmatrix) && fm_mult(fproduct, finverse, fmatrix);
  if (!ok)
    return 1;
  printf("float_inverse=%.6f:%.6f:%.6f:%.6f\n",
         finverse->a[0][0], finverse->a[0][1],
         finverse->a[1][0], finverse->a[1][1]);
  printf("float_product=%.6f:%.6f:%.6f:%.6f\n",
         display_zero(fproduct->a[0][0]), display_zero(fproduct->a[0][1]),
         display_zero(fproduct->a[1][0]), display_zero(fproduct->a[1][1]));
  fm_free(&fmatrix);
  fm_free(&finverse);
  fm_free(&fproduct);

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
def matlib_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("matlib libraries not built")

  tmp_path = tmp_path_factory.mktemp("matlib_thread_safety")
  source = tmp_path / "matlib_thread_safety.c"
  executable = tmp_path / "matlib_thread_safety"
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
    "-lmatlib",
    "-lmdblib",
    "-lpthread",
    "-lm",
  ]
  if platform.system() == "Linux":
    command.extend(["-lrt", "-ldl", "-lgcc"])
  command.extend(["-o", str(executable)])

  subprocess.run(command, check=True)
  return executable


def run_harness(executable, *args):
  return subprocess.run(
    [str(executable), *args],
    check=True,
    capture_output=True,
    text=True,
  )


def test_matlib_regression_outputs(matlib_harness):
  result = run_harness(matlib_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "double_det=10.000000\n"
    "double_inverse=0.600000:-0.700000:-0.200000:0.400000\n"
    "double_product=1.000000:0.000000:0.000000:1.000000\n"
    "float_inverse=0.600000:-0.700000:-0.200000:0.400000\n"
    "float_product=1.000000:0.000000:0.000000:1.000000\n"
  )


def test_matlib_is_thread_safe_without_caller_locks(matlib_harness):
  result = run_harness(matlib_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
