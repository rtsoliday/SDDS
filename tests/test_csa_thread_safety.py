import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libcsa.a",
  LIB_DIR / "libmdbmth.a",
  LIB_DIR / "libmdblib.a",
]

HARNESS_SOURCE = r"""
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csa.h"
#include "svd.h"

#define THREADS 8
#define ITERATIONS 120
#define CSA_SUM_OFFSET 9.744009279558
#define CSA_CENTER_OFFSET 2.621679401620
#define CSA_TOLERANCE 1e-6

typedef struct {
  long id;
  char error[256];
} Worker;

static int nearly_equal(double actual, double expected, double tolerance)
{
  return fabs(actual - expected) <= tolerance;
}

static void set_error(Worker *worker, const char *message)
{
  snprintf(worker->error, sizeof(worker->error), "%s", message);
}

static double surface(double base, double x, double y)
{
  return base + 2.0 * x + 3.0 * y + 0.5 * x * y;
}

static int expected_summary(double base, double sum, double center)
{
  return nearly_equal(sum, base * 4.0 + CSA_SUM_OFFSET, CSA_TOLERANCE) &&
         nearly_equal(center, base + CSA_CENTER_OFFSET, CSA_TOLERANCE);
}

static void fill_input(point *points, double base)
{
  int index = 0;
  int ix, iy;

  for (iy = 0; iy < 3; iy++) {
    for (ix = 0; ix < 3; ix++) {
      double x = ix * 0.5;
      double y = iy * 0.5;
      points[index].x = x;
      points[index].y = y;
      points[index].z = surface(base, x, y);
      index++;
    }
  }
}

static int run_csa_points(double base, double *sum, double *center)
{
  double xin[9], yin[9], zin[9];
  double xout[4] = {0.25, 0.50, 0.75, 0.50};
  double yout[4] = {0.25, 0.50, 0.75, 0.25};
  double *zout;
  point input[9];
  int i;

  fill_input(input, base);
  for (i = 0; i < 9; i++) {
    xin[i] = input[i].x;
    yin[i] = input[i].y;
    zin[i] = input[i].z;
  }

  zout = csa_approximatepoints2(9, xin, yin, zin, NULL, 4, xout, yout, 0, 0, 0, 0);
  if (!zout)
    return 0;

  *sum = 0.0;
  for (i = 0; i < 4; i++) {
    if (isnan(zout[i])) {
      free(zout);
      return 0;
    }
    *sum += zout[i];
  }
  *center = zout[1];
  if (!expected_summary(base, *sum, *center)) {
    free(zout);
    return 0;
  }
  free(zout);
  return 1;
}

static int run_csa_invariant(double base, double *sum, double *center)
{
  specs spec;
  int nin = 9;
  int nout = 4;
  point *pin = malloc(sizeof(*pin) * nin);
  point *pout = malloc(sizeof(*pout) * nout);
  int i;

  if (!pin || !pout) {
    free(pin);
    free(pout);
    return 0;
  }

  memset(&spec, 0, sizeof(spec));
  fill_input(pin, base);
  spec.invariant = 1;
  spec.nppc = 0;
  spec.k = 0.0;
  pout[0].x = 0.25;
  pout[0].y = 0.25;
  pout[1].x = 0.50;
  pout[1].y = 0.50;
  pout[2].x = 0.75;
  pout[2].y = 0.75;
  pout[3].x = 0.50;
  pout[3].y = 0.25;
  for (i = 0; i < nout; i++)
    pout[i].z = 0.0;

  if (!do_csa_2d_interpolate(&spec, nin, pin, &nout, &pout, NULL) || nout != 4) {
    free(pin);
    free(pout);
    return 0;
  }

  *sum = 0.0;
  for (i = 0; i < nout; i++) {
    if (isnan(pout[i].z)) {
      free(pin);
      free(pout);
      return 0;
    }
    *sum += pout[i].z;
  }
  *center = pout[1].z;
  if (!expected_summary(base, *sum, *center)) {
    free(pin);
    free(pout);
    return 0;
  }
  free(pin);
  free(pout);
  return 1;
}

static int check_config_tls(Worker *worker, long iteration)
{
  int expected_csa = (int)(worker->id * 1000 + iteration);
  int expected_svd = (int)(worker->id * 100 + iteration);

  csa_verbose = expected_csa;
  svd_verbose = expected_svd;
  sched_yield();
  if (csa_verbose != expected_csa || svd_verbose != expected_svd) {
    set_error(worker, "csa configuration state interleaved");
    return 1;
  }
  return 0;
}

static int check_interpolation(Worker *worker, long iteration)
{
  double base = worker->id * 100.0 + iteration * 0.01;
  double sum, center;

  csa_verbose = 0;
  svd_verbose = 0;
  if (!run_csa_points(base, &sum, &center)) {
    set_error(worker, "csa approximation state interleaved");
    return 1;
  }

  if ((iteration % 5) == 0 &&
      !run_csa_invariant(base, &sum, &center)) {
    set_error(worker, "csa invariant/minell state interleaved");
    return 1;
  }
  return 0;
}

static int check_invariant_uses_process_rand(void)
{
  double sum, center;
  int expected, actual;
  int i;

  srand(98765);
  if (!run_csa_invariant(10.0, &sum, &center)) {
    fputs("invariant interpolation failed\n", stderr);
    return 1;
  }
  actual = rand();

  srand(1);
  for (i = 0; i < 9; i++)
    (void)rand();
  expected = rand();
  if (actual != expected) {
    fprintf(stderr, "minell did not use the process rand sequence: actual=%d expected=%d\n", actual, expected);
    return 1;
  }
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_config_tls(worker, i) ||
        check_interpolation(worker, i))
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
  double sum, center;

  csa_verbose = 7;
  svd_verbose = 11;
  printf("config=%d:%d\n", csa_verbose, svd_verbose);
  csa_verbose = 0;
  svd_verbose = 0;

  if (!run_csa_points(10.0, &sum, &center))
    return 1;
  printf("points=%.6f:%.6f\n", sum, center);

  if (!run_csa_invariant(10.0, &sum, &center))
    return 1;
  printf("invariant=%.6f:%.6f\n", sum, center);

  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "threads"))
    return run_thread_test();
  if (argc > 1 && !strcmp(argv[1], "rng")) {
    if (check_invariant_uses_process_rand())
      return 1;
    puts("rng-compatible");
    return 0;
  }
  return run_regression();
}
"""


def compiler_command():
  command = shlex.split(os.environ.get("CC", "gcc"))
  if not command or not shutil.which(command[0]):
    pytest.skip("C compiler not available")
  return command


@pytest.fixture(scope="module")
def csa_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("csa libraries not built")

  tmp_path = tmp_path_factory.mktemp("csa_thread_safety")
  source = tmp_path / "csa_thread_safety.c"
  executable = tmp_path / "csa_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I",
    str(ROOT_DIR / "include"),
    "-I",
    str(ROOT_DIR / "2d_interpolate" / "csa"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lcsa",
    "-lmdbmth",
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


def test_csa_regression_outputs(csa_harness):
  result = run_harness(csa_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "config=7:11\n"
    "points=49.744009:12.621679\n"
    "invariant=49.744009:12.621679\n"
  )


def test_csa_is_thread_safe_without_caller_locks(csa_harness):
  result = run_harness(csa_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"


def test_csa_invariant_scaling_uses_process_rand(csa_harness):
  result = run_harness(csa_harness, "rng")
  assert result.stderr == ""
  assert result.stdout == "rng-compatible\n"


def test_csa_legacy_verbose_data_symbols_still_link(tmp_path):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("csa libraries not built")

  source = tmp_path / "csa_legacy_verbose_symbols.c"
  executable = tmp_path / "csa_legacy_verbose_symbols"
  source.write_text(
    '#define CSA_NO_GLOBAL_COMPAT_MACROS\n'
    '#include <stdio.h>\n'
    '#include "csa.h"\n'
    '#include "svd.h"\n'
    'int main(void) {\n'
    '  csa_verbose = 12;\n'
    '  svd_verbose = 34;\n'
    '  printf("%d:%d\\n", csa_verbose, svd_verbose);\n'
    '  return 0;\n'
    '}\n',
    encoding="ascii",
  )
  command = [
    *compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I",
    str(ROOT_DIR / "include"),
    "-I",
    str(ROOT_DIR / "2d_interpolate" / "csa"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lcsa",
    "-lmdbmth",
    "-lmdblib",
    "-lpthread",
    "-lm",
    "-o",
    str(executable),
  ]
  if platform.system() == "Linux":
    command[-2:-2] = ["-lrt", "-ldl", "-lgcc"]
  subprocess.run(command, check=True)

  result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
  assert result.stdout == "12:34\n"
  assert result.stderr == ""
