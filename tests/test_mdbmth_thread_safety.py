import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libmdbmth.a",
  LIB_DIR / "libmdblib.a",
]

HARNESS_SOURCE = r"""
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdb.h"

void rk4_step(double *yf, double x, double *yi, double *dydx, double h,
              long n_eq, void (*derivs)(double *dydx, double *y, double x));

#define THREADS 8
#define ITERATIONS 500

typedef struct {
  long id;
  char error[256];
} Worker;

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int owner_ready;
  int runner_done;
  char owner_error[256];
  char runner_error[256];
} AbortIsolationState;

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int writer_ready;
  int reader_done;
  short expected;
  char writer_error[256];
  char reader_error[256];
} ProcessGlobalFlagState;

static int nearly_equal(double actual, double expected)
{
  return fabs(actual - expected) < 1e-12;
}

static void set_error(Worker *worker, const char *message)
{
  snprintf(worker->error, sizeof(worker->error), "%s", message);
}

static void set_abort_error(char *error, const char *message)
{
  if (!error[0])
    snprintf(error, 256, "%s", message);
}

static double square(double x)
{
  return x * x;
}

static double quadratic(double *x, long *invalid)
{
  *invalid = 0;
  return (x[0] - 1.0) * (x[0] - 1.0);
}

static void exp_deriv(double *dydx, double *y, double x)
{
  (void)x;
  dydx[0] = y[0];
}

static int check_random_state(Worker *worker, long iteration)
{
  long seed = 100000 + worker->id * 1000 + iteration;
  double a, b, c, d;
  double oa, ob, oc, od;

  random_1(-seed);
  a = random_1(0);
  b = random_1(0);
  random_1(-seed);
  c = random_1(0);
  d = random_1(0);
  if (!nearly_equal(a, c) || !nearly_equal(b, d)) {
    set_error(worker, "random_1 seed state interleaved");
    return 1;
  }

  random_oag(-seed, 3);
  oa = random_oag(0, 3);
  ob = random_oag(0, 3);
  random_oag(-seed, 3);
  oc = random_oag(0, 3);
  od = random_oag(0, 3);
  if (!nearly_equal(oa, oc) || !nearly_equal(ob, od)) {
    set_error(worker, "random_oag seed state interleaved");
    return 1;
  }

  return 0;
}

static int check_statistics(Worker *worker, long iteration)
{
  double offset = worker->id * 1000.0 + iteration;
  double data[5] = {offset + 5, offset + 1, offset + 3, offset + 2, offset + 4};
  double value;
  long index;

  if (!compute_median(&value, data, 5) || !nearly_equal(value, offset + 3)) {
    set_error(worker, "compute_median scratch state interleaved");
    return 1;
  }

  index = find_median(&value, data, 5);
  if (index != 2 || !nearly_equal(value, offset + 3)) {
    set_error(worker, "find_median scratch state interleaved");
    return 1;
  }

  return 0;
}

static int check_histogram(Worker *worker, long iteration)
{
  double lo = worker->id * 1000.0 + iteration;
  double hist[4];
  double first[4] = {lo + 0.1, lo + 1.1, lo + 2.1, lo + 3.1};
  double second[1] = {lo + 3.5};

  if (make_histogram(hist, 4, lo, lo + 4, first, 4, 1) != 4 ||
      !nearly_equal(hist[0], 1) || !nearly_equal(hist[1], 1) ||
      !nearly_equal(hist[2], 1) || !nearly_equal(hist[3], 1)) {
    set_error(worker, "make_histogram first pass changed");
    return 1;
  }

  if (make_histogram(hist, 4, lo, lo + 4, second, 1, 0) != 5 ||
      !nearly_equal(hist[0], 1) || !nearly_equal(hist[1], 1) ||
      !nearly_equal(hist[2], 1) || !nearly_equal(hist[3], 2)) {
    set_error(worker, "make_histogram accumulator state interleaved");
    return 1;
  }

  return 0;
}

static int check_halton(Worker *worker)
{
  int32_t radix = 2;
  long id = startHaltonSequence(&radix, 0.0);
  double h1 = nextHaltonSequencePoint(id);
  double h2 = nextHaltonSequencePoint(id);
  double h3 = nextHaltonSequencePoint(id);

  if (id <= 0 || radix != 2 ||
      !nearly_equal(h1, 0.5) || !nearly_equal(h2, 0.25) ||
      !nearly_equal(h3, 0.75)) {
    set_error(worker, "Halton sequence state interleaved");
    return 1;
  }

  return 0;
}

static int check_process_rand_compatibility(void)
{
  int expected_int, actual_int;
  double expected_fraction, actual_fraction;

  srand(13579);
  expected_int = rand();
  srand(13579);
  actual_int = mdbmth_locked_rand();
  if (actual_int != expected_int) {
    fprintf(stderr, "mdbmth_locked_rand ignored process srand\n");
    return 1;
  }

  srand(24680);
  expected_fraction = (double)rand() / ((double)RAND_MAX + 1.0);
  srand(24680);
  actual_fraction = mdbmth_locked_rand_fraction();
  if (!nearly_equal(actual_fraction, expected_fraction)) {
    fprintf(stderr, "mdbmth_locked_rand_fraction ignored process srand\n");
    return 1;
  }

  mdbmth_lock_rand();
  mdbmth_srand_unlocked(97531);
  actual_int = mdbmth_rand_unlocked();
  actual_fraction = mdbmth_rand_fraction_unlocked();
  mdbmth_unlock_rand();

  srand(97531);
  expected_int = rand();
  expected_fraction = (double)rand() / ((double)RAND_MAX + 1.0);
  if (actual_int != expected_int || !nearly_equal(actual_fraction, expected_fraction)) {
    fprintf(stderr, "locked rand sequence changed\n");
    return 1;
  }

  return 0;
}

static int check_numeric_workspaces(Worker *worker)
{
  double smooth[5] = {1, 2, 10, 2, 1};
  double y0[1] = {1};
  double dydx[1] = {1};
  double yf[1];
  double best;
  double xret[1];
  double lower[1] = {0};
  double upper[1] = {2};
  double step[1] = {1};
  double mean, rms, stand_dev;
  double data[5] = {5, 1, 3, 2, 4};
  double weights[5] = {1, 1, 1, 1, 1};
  double reset_data[2] = {9, 11};
  double reset_weights[2] = {1, 1};

  smoothData(smooth, 5, 3, 1);
  if (!nearly_equal(smooth[0], 1.5) ||
      !nearly_equal(smooth[1], 4.333333333333333) ||
      !nearly_equal(smooth[2], 4.666666666666667) ||
      !nearly_equal(smooth[3], 4.333333333333333) ||
      !nearly_equal(smooth[4], 1.5)) {
    set_error(worker, "smoothData workspace interleaved");
    return 1;
  }

  if (!nearly_equal(qromb(square, 8, 0, 1, 1e-12), 0.33333333333333331)) {
    set_error(worker, "qromb workspace interleaved");
    return 1;
  }

  rk4_step(yf, 0, y0, dydx, 0.1, 1, exp_deriv);
  if (!nearly_equal(yf[0], 1.1051708333333334)) {
    set_error(worker, "rk4_step workspace interleaved");
    return 1;
  }

  mmid2(y0, dydx, 1, 0, 0.1, 8, yf, exp_deriv);
  if (!nearly_equal(yf[0], 1.1051709160299692)) {
    set_error(worker, "mmid workspace interleaved");
    return 1;
  }

  if (!grid_search_min(&best, xret, lower, upper, step, 1, -1, quadratic) ||
      !nearly_equal(best, 0) || !nearly_equal(xret[0], 1)) {
    set_error(worker, "grid_search_min workspace interleaved");
    return 1;
  }

  if (!accumulateMoments(&mean, &rms, &stand_dev, data, 5, 1) ||
      !nearly_equal(mean, 3) ||
      !nearly_equal(rms, 3.3166247903553998) ||
      !nearly_equal(stand_dev, 1.5811388300841898)) {
    set_error(worker, "accumulateMoments state interleaved");
    return 1;
  }
  if (!accumulateWeightedMoments(&mean, &rms, &stand_dev, data, weights, 5, 1) ||
      !nearly_equal(mean, 3) ||
      !nearly_equal(rms, 3.3166247903553998) ||
      !nearly_equal(stand_dev, 1.5811388300841898)) {
    set_error(worker, "accumulateWeightedMoments reset state interleaved");
    return 1;
  }
  if (accumulateMoments(&mean, &rms, &stand_dev, data, 0, 1) ||
      !accumulateMoments(&mean, &rms, &stand_dev, reset_data, 2, 0) ||
      !nearly_equal(mean, 10) ||
      !nearly_equal(rms, 10.04987562112089) ||
      !nearly_equal(stand_dev, 1.4142135623730951)) {
    set_error(worker, "accumulateMoments zero-length reset did not clear state");
    return 1;
  }
  if (accumulateWeightedMoments(&mean, &rms, &stand_dev, data, weights, 0, 1) ||
      !accumulateWeightedMoments(&mean, &rms, &stand_dev, reset_data, reset_weights, 2, 0) ||
      !nearly_equal(mean, 10) ||
      !nearly_equal(rms, 10.04987562112089) ||
      !nearly_equal(stand_dev, 1.4142135623730951)) {
    set_error(worker, "accumulateWeightedMoments zero-length reset did not clear state");
    return 1;
  }

  return 0;
}

static void *abort_owner_worker(void *arg)
{
  AbortIsolationState *state = (AbortIsolationState *)arg;

  optimAbort(1);
  simplexMinAbort(1);

  pthread_mutex_lock(&state->mutex);
  state->owner_ready = 1;
  pthread_cond_broadcast(&state->cond);
  while (!state->runner_done)
    pthread_cond_wait(&state->cond, &state->mutex);
  pthread_mutex_unlock(&state->mutex);

  if (!optimAbort(0))
    set_abort_error(state->owner_error, "optimAbort state was unexpectedly cleared");
  if (!simplexMinAbort(0))
    set_abort_error(state->owner_error, "simplexMinAbort state was unexpectedly cleared");

  return state->owner_error[0] ? state : NULL;
}

static void *abort_runner_worker(void *arg)
{
  AbortIsolationState *state = (AbortIsolationState *)arg;

  pthread_mutex_lock(&state->mutex);
  while (!state->owner_ready)
    pthread_cond_wait(&state->cond, &state->mutex);
  pthread_mutex_unlock(&state->mutex);

  if (!optimAbort(0))
    set_abort_error(state->runner_error, "optimAbort state was not visible across threads");
  if (!simplexMinAbort(0))
    set_abort_error(state->runner_error, "simplexMinAbort state was not visible across threads");

  pthread_mutex_lock(&state->mutex);
  state->runner_done = 1;
  pthread_cond_broadcast(&state->cond);
  pthread_mutex_unlock(&state->mutex);

  return state->runner_error[0] ? state : NULL;
}

static int check_abort_state_is_process_global(void)
{
  AbortIsolationState state;
  pthread_t owner_thread, runner_thread;
  void *owner_status = NULL;
  void *runner_status = NULL;

  memset(&state, 0, sizeof(state));
  pthread_mutex_init(&state.mutex, NULL);
  pthread_cond_init(&state.cond, NULL);

  if (pthread_create(&owner_thread, NULL, abort_owner_worker, &state)) {
    fprintf(stderr, "pthread_create failed for abort owner\n");
    pthread_cond_destroy(&state.cond);
    pthread_mutex_destroy(&state.mutex);
    return 1;
  }
  if (pthread_create(&runner_thread, NULL, abort_runner_worker, &state)) {
    fprintf(stderr, "pthread_create failed for abort runner\n");
    pthread_mutex_lock(&state.mutex);
    state.runner_done = 1;
    pthread_cond_broadcast(&state.cond);
    pthread_mutex_unlock(&state.mutex);
    pthread_join(owner_thread, NULL);
    pthread_cond_destroy(&state.cond);
    pthread_mutex_destroy(&state.mutex);
    return 1;
  }

  pthread_join(owner_thread, &owner_status);
  pthread_join(runner_thread, &runner_status);
  pthread_cond_destroy(&state.cond);
  pthread_mutex_destroy(&state.mutex);

  if (owner_status || runner_status || state.owner_error[0] || state.runner_error[0]) {
    if (state.owner_error[0])
      fprintf(stderr, "%s\n", state.owner_error);
    if (state.runner_error[0])
      fprintf(stderr, "%s\n", state.runner_error);
    return 1;
  }

  return 0;
}

static void *inhibit_writer_worker(void *arg)
{
  ProcessGlobalFlagState *state = (ProcessGlobalFlagState *)arg;

  inhibitRandomSeedPermutation(state->expected);

  pthread_mutex_lock(&state->mutex);
  state->writer_ready = 1;
  pthread_cond_broadcast(&state->cond);
  while (!state->reader_done)
    pthread_cond_wait(&state->cond, &state->mutex);
  pthread_mutex_unlock(&state->mutex);

  if (inhibitRandomSeedPermutation(-1) != state->expected)
    set_abort_error(state->writer_error, "seed permutation flag was unexpectedly changed");

  return state->writer_error[0] ? state : NULL;
}

static void *inhibit_reader_worker(void *arg)
{
  ProcessGlobalFlagState *state = (ProcessGlobalFlagState *)arg;

  pthread_mutex_lock(&state->mutex);
  while (!state->writer_ready)
    pthread_cond_wait(&state->cond, &state->mutex);
  pthread_mutex_unlock(&state->mutex);

  if (inhibitRandomSeedPermutation(-1) != state->expected)
    set_abort_error(state->reader_error, "seed permutation flag was not visible across threads");

  pthread_mutex_lock(&state->mutex);
  state->reader_done = 1;
  pthread_cond_broadcast(&state->cond);
  pthread_mutex_unlock(&state->mutex);

  return state->reader_error[0] ? state : NULL;
}

static int check_seed_permutation_state_is_process_global(void)
{
  ProcessGlobalFlagState state;
  pthread_t writer_thread, reader_thread;
  void *writer_status = NULL;
  void *reader_status = NULL;

  memset(&state, 0, sizeof(state));
  state.expected = 1;
  pthread_mutex_init(&state.mutex, NULL);
  pthread_cond_init(&state.cond, NULL);

  if (pthread_create(&writer_thread, NULL, inhibit_writer_worker, &state)) {
    fprintf(stderr, "pthread_create failed for seed permutation writer\n");
    pthread_cond_destroy(&state.cond);
    pthread_mutex_destroy(&state.mutex);
    return 1;
  }
  if (pthread_create(&reader_thread, NULL, inhibit_reader_worker, &state)) {
    fprintf(stderr, "pthread_create failed for seed permutation reader\n");
    pthread_mutex_lock(&state.mutex);
    state.reader_done = 1;
    pthread_cond_broadcast(&state.cond);
    pthread_mutex_unlock(&state.mutex);
    pthread_join(writer_thread, NULL);
    pthread_cond_destroy(&state.cond);
    pthread_mutex_destroy(&state.mutex);
    inhibitRandomSeedPermutation(0);
    return 1;
  }

  pthread_join(writer_thread, &writer_status);
  pthread_join(reader_thread, &reader_status);
  pthread_cond_destroy(&state.cond);
  pthread_mutex_destroy(&state.mutex);

  inhibitRandomSeedPermutation(0);

  if (writer_status || reader_status || state.writer_error[0] || state.reader_error[0]) {
    if (state.writer_error[0])
      fprintf(stderr, "%s\n", state.writer_error);
    if (state.reader_error[0])
      fprintf(stderr, "%s\n", state.reader_error);
    return 1;
  }

  return 0;
}

static int run_regression(void)
{
  double value;
  double data[5] = {5, 1, 3, 2, 4};
  double percent[3] = {0, 50, 100};
  double positions[3];
  int32_t keep[5] = {1, 0, 1, 0, 1};
  double hist[4];
  double hdata[5] = {0.1, 0.2, 1.2, 2.2, 3.2};
  double smooth[5] = {1, 2, 10, 2, 1};
  int32_t radix = 2;
  long halton;
  double r1, r2, r3, o1, o2, h1, h2, h3;
  double y0[1] = {1};
  double dydx[1] = {1};
  double yf[1];
  double best;
  double xret[1];
  double lower[1] = {0};
  double upper[1] = {2};
  double step[1] = {1};
  double mean, rms, stand_dev;
  double weights[5] = {1, 1, 1, 1, 1};

  random_1(-12345);
  r1 = random_1(0);
  r2 = random_1(0);
  r3 = random_1(0);
  random_oag(-2468, 3);
  o1 = random_oag(0, 3);
  o2 = random_oag(0, 3);

  printf("random_1=%.17g,%.17g,%.17g\n", r1, r2, r3);
  printf("random_oag=%.17g,%.17g\n", o1, o2);

  compute_median(&value, data, 5);
  printf("median=%.17g\n", value);
  compute_percentiles(positions, percent, 3, data, 5);
  printf("percentiles=%.17g,%.17g,%.17g\n", positions[0], positions[1], positions[2]);
  compute_percentiles_flagged(positions, percent, 3, data, keep, 5);
  printf("flagged=%.17g,%.17g,%.17g\n", positions[0], positions[1], positions[2]);
  printf("find_median=%ld", find_median(&value, data, 5));
  printf(":%.17g\n", value);

  printf("hist_count=%ld\n", make_histogram(hist, 4, 0, 4, hdata, 5, 1));
  printf("hist=%.17g,%.17g,%.17g,%.17g\n", hist[0], hist[1], hist[2], hist[3]);
  smoothData(smooth, 5, 3, 1);
  printf("smooth=%.17g,%.17g,%.17g,%.17g,%.17g\n",
         smooth[0], smooth[1], smooth[2], smooth[3], smooth[4]);

  halton = startHaltonSequence(&radix, 0.0);
  h1 = nextHaltonSequencePoint(halton);
  h2 = nextHaltonSequencePoint(halton);
  h3 = nextHaltonSequencePoint(halton);
  printf("halton=%ld:%d:%.17g,%.17g,%.17g\n", halton, radix, h1, h2, h3);
  printf("qromb=%.17g\n", qromb(square, 8, 0, 1, 1e-12));
  rk4_step(yf, 0, y0, dydx, 0.1, 1, exp_deriv);
  printf("rk4=%.17g\n", yf[0]);
  mmid2(y0, dydx, 1, 0, 0.1, 8, yf, exp_deriv);
  printf("mmid2=%.17g\n", yf[0]);
  grid_search_min(&best, xret, lower, upper, step, 1, -1, quadratic);
  printf("grid=%.17g:%.17g\n", best, xret[0]);
  accumulateMoments(&mean, &rms, &stand_dev, data, 5, 1);
  printf("accum=%.17g,%.17g,%.17g\n", mean, rms, stand_dev);
  accumulateWeightedMoments(&mean, &rms, &stand_dev, data, weights, 5, 1);
  printf("waccum=%.17g,%.17g,%.17g\n", mean, rms, stand_dev);
  set_argument_offset(1.0);
  set_argument_scale(2.0);
  printf("basis=%.17g,%.17g,%.17g,%.17g\n",
         tcheby(2.0, 3), dtcheby(2.0, 3), ipower(3.0, 3), dipower(3.0, 3));
  printf("cei=%.17g,%.17g\n", K_cei(0.5), E_cei(0.5));
  printf("diffeq=%s\n", diffeq_result_description(2));
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_random_state(worker, i) ||
        check_statistics(worker, i) ||
        check_histogram(worker, i) ||
        check_halton(worker) ||
        check_numeric_workspaces(worker)) {
      return worker;
    }
  }

  return NULL;
}

static int run_thread_test(void)
{
  pthread_t threads[THREADS];
  Worker workers[THREADS];
  long i;

  inhibitRandomSeedPermutation(0);

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

  if (check_abort_state_is_process_global())
    return 1;
  if (check_seed_permutation_state_is_process_global())
    return 1;

  puts("thread-safe");
  return 0;
}

static int run_rng_test(void)
{
  if (check_process_rand_compatibility())
    return 1;
  puts("rng-compatible");
  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "threads"))
    return run_thread_test();
  if (argc > 1 && !strcmp(argv[1], "rng"))
    return run_rng_test();
  return run_regression();
}
"""


def compiler_command():
  command = shlex.split(os.environ.get("CC", "gcc"))
  if not command or not shutil.which(command[0]):
    pytest.skip("C compiler not available")
  return command


@pytest.fixture(scope="module")
def mdbmth_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("mdbmth libraries not built")

  tmp_path = tmp_path_factory.mktemp("mdbmth_thread_safety")
  source = tmp_path / "mdbmth_thread_safety.c"
  executable = tmp_path / "mdbmth_thread_safety"
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
    "-lmdbmth",
    "-lmdblib",
    "-fopenmp",
    "-lpthread",
    "-lm",
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


def test_mdbmth_api_regression_outputs(mdbmth_harness):
  result = run_harness(mdbmth_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "random_1=0.088151410123405327,0.79804063838956907,0.44767473882507502\n"
    "random_oag=0.72514344520407192,0.90156378704913109\n"
    "median=3\n"
    "percentiles=1,3,5\n"
    "flagged=3,4,5\n"
    "find_median=2:3\n"
    "hist_count=5\n"
    "hist=2,1,1,1\n"
    "smooth=1.5,4.333333333333333,4.666666666666667,4.333333333333333,1.5\n"
    "halton=1:2:0.5,0.25,0.75\n"
    "qromb=0.33333333333333331\n"
    "rk4=1.1051708333333334\n"
    "mmid2=1.1051709160299692\n"
    "grid=0:1\n"
    "accum=3,3.3166247903553998,1.5811388300841898\n"
    "waccum=3,3.3166247903553998,1.5811388300841898\n"
    "basis=-1,-1.1141400536168886e-15,1,3\n"
    "cei=1.6857503548125961,1.4674622093394272\n"
    "diffeq=zero of exit-function found\n"
  )


def test_mdbmth_api_is_thread_safe_without_caller_locks(mdbmth_harness):
  result = run_harness(mdbmth_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"


def test_mdbmth_locked_rand_preserves_process_srand(mdbmth_harness):
  result = run_harness(mdbmth_harness, "rng")
  assert result.stderr == ""
  assert result.stdout == "rng-compatible\n"
