import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR, openmp_link_args, built_library


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  built_library("fftpack"),
  built_library("mdbmth"),
  built_library("mdblib"),
]

HARNESS_SOURCE = r"""
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdb.h"
#include "fftpackC.h"

#define THREADS 8
#define ITERATIONS 200
#define POINTS 96

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

static void reference_magnitude(double *magnitude2, double *data, long points)
{
  long i, k;

  for (k = 0; k <= points / 2; k++) {
    double real = 0;
    double imag = 0;
    for (i = 0; i < points; i++) {
      double angle = PIx2 * k * i / points;
      real += data[i] * cos(angle);
      imag += data[i] * sin(angle);
    }
    real /= points;
    imag /= points;
    if (k != 0 && !(k == points / 2 && points % 2 == 0)) {
      real *= 2;
      imag *= 2;
    }
    magnitude2[k] = real * real + imag * imag;
  }
}

static void make_signal(double *data, long points, long worker_id, long iteration)
{
  long i;
  long harmonic1 = 2 + (worker_id % 7);
  long harmonic2 = 5 + (iteration % 11);
  double phase = 0.013 * worker_id + 0.001 * iteration;

  for (i = 0; i < points; i++) {
    data[i] = 0.25 * worker_id +
              cos(PIx2 * harmonic1 * i / points + phase) +
              0.35 * sin(PIx2 * harmonic2 * i / points - 0.5 * phase);
  }
}

static int check_simple_fft(Worker *worker, long iteration)
{
  double data[POINTS];
  double magnitude[POINTS / 2 + 1];
  double expected[POINTS / 2 + 1];
  long i, result;

  make_signal(data, POINTS, worker->id, iteration);
  reference_magnitude(expected, data, POINTS);
  result = simpleFFT(magnitude, data, POINTS);
  if (result != POINTS / 2 + 1) {
    set_error(worker, "simpleFFT returned unexpected count");
    return 1;
  }
  for (i = 0; i <= POINTS / 2; i++) {
    if (!nearly_equal(magnitude[i], expected[i], 1e-10)) {
      set_error(worker, "simpleFFT scratch state interleaved");
      return 1;
    }
  }
  return 0;
}

static int check_real_roundtrip(Worker *worker, long iteration)
{
  double data[POINTS];
  double spectrum[POINTS + 2];
  double recovered[POINTS + 2];
  long i;

  make_signal(data, POINTS, worker->id + 3, iteration);
  if (!realFFT2(spectrum, data, POINTS, 0) ||
      !realFFT2(recovered, spectrum, POINTS, INVERSE_FFT)) {
    set_error(worker, "realFFT2 failed");
    return 1;
  }
  for (i = 0; i < POINTS; i++) {
    if (!nearly_equal(recovered[i], data[i], 1e-10)) {
      set_error(worker, "realFFT2 workspace state interleaved");
      return 1;
    }
  }
  return 0;
}

static int check_complex_roundtrip(Worker *worker, long iteration)
{
  double data[2 * POINTS];
  double original[2 * POINTS];
  long i;

  for (i = 0; i < POINTS; i++) {
    data[2 * i] = cos(PIx2 * (worker->id + 1) * i / POINTS) + iteration * 0.0001;
    data[2 * i + 1] = sin(PIx2 * (worker->id + 2) * i / POINTS) - iteration * 0.0002;
  }
  memcpy(original, data, sizeof(data));
  if (!complexFFT(data, POINTS, 0) ||
      !complexFFT(data, POINTS, INVERSE_FFT)) {
    set_error(worker, "complexFFT failed");
    return 1;
  }
  for (i = 0; i < 2 * POINTS; i++) {
    if (!nearly_equal(data[i], original[i], 1e-10)) {
      set_error(worker, "complexFFT workspace state interleaved");
      return 1;
    }
  }
  return 0;
}

static int check_naff(Worker *worker, long iteration)
{
  double data[POINTS];
  double frequency[1];
  double amplitude[1];
  double phase[1];
  double significance[1];
  long harmonic = 3 + (worker->id % 5);
  long i, found;

  for (i = 0; i < POINTS; i++)
    data[i] = cos(PIx2 * harmonic * i / POINTS + 0.01 * worker->id + 0.001 * iteration);
  frequency[0] = 0;
  found = PerformNAFF(frequency, amplitude, phase, significance,
                      0.0, 1.0, data, POINTS, 0, 0.0, 1, 100.0,
                      1e-12, 0.0, 0.0);
  if (found != 1 ||
      !nearly_equal(frequency[0], (double)harmonic / POINTS, 1e-4) ||
      amplitude[0] < 0.9 || amplitude[0] > 1.1 ||
      significance[0] > 1e-3) {
    snprintf(worker->error, sizeof(worker->error),
             "PerformNAFF thread-local state interleaved: found=%ld frequency=%.17g expected=%.17g amplitude=%.17g significance=%.17g",
             found, frequency[0], (double)harmonic / POINTS, amplitude[0], significance[0]);
    return 1;
  }
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_simple_fft(worker, i) ||
        check_real_roundtrip(worker, i) ||
        check_complex_roundtrip(worker, i) ||
        check_naff(worker, i))
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
  double data[8];
  double magnitude[5];
  double spectrum[10];
  double recovered[10];
  double complexData[8];
  double frequency[1];
  double amplitude[1];
  double phase[1];
  double significance[1];
  long i, result;

  for (i = 0; i < 8; i++)
    data[i] = sin(PIx2 * 2 * i / 8);
  result = simpleFFT(magnitude, data, 8);
  printf("simple=%ld:%.6f:%.6f:%.6f\n",
         result, magnitude[0], magnitude[2], magnitude[4]);

  if (!realFFT2(spectrum, data, 8, 0) ||
      !realFFT2(recovered, spectrum, 8, INVERSE_FFT))
    return 1;
  printf("real_roundtrip=%.6f:%.6f:%.6f\n",
         recovered[0], recovered[2], recovered[6]);

  for (i = 0; i < 4; i++) {
    complexData[2 * i] = (double)(i + 1);
    complexData[2 * i + 1] = (double)(4 - i);
  }
  if (!complexFFT(complexData, 4, 0))
    return 1;
  printf("complex_forward=%.6f:%.6f:%.6f:%.6f\n",
         complexData[0], complexData[1], complexData[2], complexData[3]);
  if (!complexFFT(complexData, 4, INVERSE_FFT))
    return 1;
  printf("complex_roundtrip=%.6f:%.6f:%.6f:%.6f\n",
         complexData[0], complexData[1], complexData[2], complexData[3]);

  {
    double naffData[64];
    for (i = 0; i < 64; i++)
      naffData[i] = cos(PIx2 * 8 * i / 64 + 0.25);
    frequency[0] = 8.0 / 64.0;
    result = PerformNAFF(frequency, amplitude, phase, significance,
                         0.0, 1.0, naffData, 64, NAFF_FREQ_FOUND,
                         0.0, 1, 100.0, 1e-12, 0.0, 0.0);
  }
  printf("naff=%ld:%.6f:%.6f:%.6f:%.6f\n",
         result, frequency[0], amplitude[0], phase[0], significance[0]);

  atexitFFTpack();
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
def fftpack_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("fftpack libraries not built")

  tmp_path = tmp_path_factory.mktemp("fftpack_thread_safety")
  source = tmp_path / "fftpack_thread_safety.c"
  executable = tmp_path / "fftpack_thread_safety"
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
    str(source),
    "-L",
    str(LIB_DIR),
    "-lfftpack",
    "-lmdbmth",
    "-lmdblib",
    "-lpthread",
    "-lm",
  ])
  if platform.system() == "Linux":
    command.extend(["-lrt", "-ldl", "-lgcc"])
  command.extend(openmp_link_args())
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


def test_fftpack_regression_outputs(fftpack_harness):
  result = run_harness(fftpack_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "simple=5:0.000000:1.000000:0.000000\n"
    "real_roundtrip=0.000000:0.000000:0.000000\n"
    "complex_forward=2.500000:2.500000:0.000000:1.000000\n"
    "complex_roundtrip=1.000000:4.000000:2.000000:3.000000\n"
    "naff=1:0.125000:1.000019:0.250035:0.000000\n"
  )


def test_fftpack_is_thread_safe_without_caller_locks(fftpack_harness):
  result = run_harness(fftpack_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
