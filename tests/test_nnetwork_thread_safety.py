import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libnnetwork.a",
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

#include "nn_2d_interpolate.h"
#include "distribute.h"

#define THREADS 8
#define ITERATIONS 150

int nprocesses = THREADS;
int rank = 0;

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

static double plane(double base, double x, double y)
{
  return base + 2.0 * x + 3.0 * y;
}

static void fill_input(point *points, double base)
{
  points[0].x = 0.0;
  points[0].y = 0.0;
  points[0].z = plane(base, points[0].x, points[0].y);
  points[1].x = 1.0;
  points[1].y = 0.0;
  points[1].z = plane(base, points[1].x, points[1].y);
  points[2].x = 0.0;
  points[2].y = 1.0;
  points[2].z = plane(base, points[2].x, points[2].y);
  points[3].x = 1.0;
  points[3].y = 1.0;
  points[3].z = plane(base, points[3].x, points[3].y);
  points[4].x = 0.5;
  points[4].y = 0.5;
  points[4].z = plane(base, points[4].x, points[4].y);
}

static int run_linear_grid(double base, int invariant, int square, double *sum, double *center)
{
  specs *spec = specs_create();
  int nin = 5;
  point *pin = malloc(sizeof(*pin) * nin);
  point *pout = NULL;
  int nout = 0;
  int i;

  if (!spec || !pin)
    return 0;
  fill_input(pin, base);
  spec->linear = 1;
  spec->invariant = invariant;
  spec->square = square;
  spec->nx = 3;
  spec->ny = 3;
  spec->xmin = 0.0;
  spec->xmax = 1.0;
  spec->ymin = 0.0;
  spec->ymax = 1.0;
  do_nn_2d_interpolate(spec, &nin, &pin, &nout, &pout);
  if (nout != 9 || !pout) {
    free(pin);
    free(pout);
    free(spec);
    return 0;
  }

  *sum = 0.0;
  for (i = 0; i < nout; i++) {
    double expected = plane(base, pout[i].x, pout[i].y);
    if (isnan(pout[i].z) || !nearly_equal(pout[i].z, expected, 1e-10)) {
      free(pin);
      free(pout);
      free(spec);
      return 0;
    }
    *sum += pout[i].z;
  }
  *center = pout[4].z;
  free(pin);
  free(pout);
  free(spec);
  return 1;
}

static int run_natural_point(double base, NN_RULE rule, double *value)
{
  point input[5];
  point output[1];
  delaunay *d;

  fill_input(input, base);
  output[0].x = 0.25;
  output[0].y = 0.75;
  output[0].z = 0.0;
  nn_rule = rule;
  d = delaunay_build(5, input, 0, NULL, 0, NULL);
  if (!d)
    return 0;
  nnpi_interpolate_points(d, 0.0, 1, output);
  delaunay_destroy(d);
  *value = output[0].z;
  return !isnan(*value) && nearly_equal(*value, plane(base, output[0].x, output[0].y), 1e-10);
}

static int check_config_tls(Worker *worker, long iteration)
{
  int expected_verbose = (int)(worker->id * 1000 + iteration);
  int expected_vertex = (int)(worker->id * 100 + iteration);
  NN_RULE expected_rule = (worker->id & 1) ? SIBSON : NON_SIBSONIAN;

  nn_verbose = expected_verbose;
  nn_test_vertice = expected_vertex;
  nn_rule = expected_rule;
  sched_yield();
  if (nn_verbose != expected_verbose ||
      nn_test_vertice != expected_vertex ||
      nn_rule != expected_rule) {
    set_error(worker, "nn configuration state interleaved");
    return 1;
  }
  return 0;
}

static int check_distribute_tls(Worker *worker, long iteration)
{
  int start = 1000 + (int)iteration * 100;
  int myrank = (int)worker->id - 1;
  int expected_first = start + myrank * 3;
  int expected_last = expected_first + 2;

  distribute_iterations(start, start + THREADS * 3 - 1, THREADS, myrank);
  sched_yield();
  if (my_number_of_iterations != 3 ||
      my_first_iteration != expected_first ||
      my_last_iteration != expected_last ||
      !number_of_iterations ||
      !first_iteration ||
      !last_iteration ||
      number_of_iterations[myrank] != 3 ||
      first_iteration[myrank] != expected_first ||
      last_iteration[myrank] != expected_last) {
    distribute_free();
    set_error(worker, "distribute_iterations state interleaved");
    return 1;
  }
  distribute_free();
  return 0;
}

static int check_interpolation(Worker *worker, long iteration)
{
  double base = worker->id * 100.0 + iteration * 0.01;
  double sum, center, natural;

  nn_verbose = 0;
  nn_test_vertice = -1;
  if (!run_linear_grid(base, 0, 0, &sum, &center) ||
      !nearly_equal(center, plane(base, 0.5, 0.5), 1e-10)) {
    set_error(worker, "linear interpolation state interleaved");
    return 1;
  }
  if ((iteration % 5) == 0 &&
      (!run_linear_grid(base, 1, 0, &sum, &center) ||
       !nearly_equal(center, plane(base, 0.5, 0.5), 1e-10))) {
    set_error(worker, "minell invariant scaling state interleaved");
    return 1;
  }
  if ((iteration % 10) == 0 &&
      !run_natural_point(base, (worker->id & 1) ? SIBSON : NON_SIBSONIAN, &natural)) {
    set_error(worker, "natural-neighbour interpolation state interleaved");
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
  if (!run_linear_grid(10.0, 1, 0, &sum, &center)) {
    fputs("invariant interpolation failed\n", stderr);
    return 1;
  }
  actual = rand();

  srand(1);
  for (i = 0; i < 5; i++)
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

  if (my_number_of_iterations != -1 ||
      my_first_iteration != -1 ||
      my_last_iteration != -1 ||
      number_of_iterations ||
      first_iteration ||
      last_iteration) {
    set_error(worker, "fresh thread inherited distribute compatibility mirrors");
    return worker;
  }

  for (i = 0; i < ITERATIONS; i++) {
    if (check_config_tls(worker, i) ||
        check_distribute_tls(worker, i) ||
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

  distribute_iterations(0, THREADS - 1, THREADS, 0);
  if (my_number_of_iterations != 1 ||
      my_first_iteration != 0 ||
      my_last_iteration != 0) {
    fputs("main distribute setup failed\n", stderr);
    distribute_free();
    return 1;
  }

  for (i = 0; i < THREADS; i++) {
    workers[i].id = i + 1;
    workers[i].error[0] = 0;
    if (pthread_create(&threads[i], NULL, thread_worker, &workers[i])) {
      fprintf(stderr, "pthread_create failed\n");
      distribute_free();
      return 1;
    }
  }

  for (i = 0; i < THREADS; i++) {
    void *status = NULL;
    if (pthread_join(threads[i], &status)) {
      fprintf(stderr, "pthread_join failed\n");
      distribute_free();
      return 1;
    }
    if (status) {
      fprintf(stderr, "thread %ld failed: %s\n", i + 1, ((Worker *)status)->error);
      distribute_free();
      return 1;
    }
  }

  distribute_free();
  puts("thread-safe");
  return 0;
}

static int run_regression(void)
{
  double sum, center, natural_sibson, natural_non_sibson;

  nn_verbose = 7;
  nn_test_vertice = 11;
  nn_rule = NON_SIBSONIAN;
  printf("config=%d:%d:%d\n", nn_verbose, nn_test_vertice, (int)nn_rule);
  nn_verbose = 0;
  nn_test_vertice = -1;

  distribute_iterations(10, 21, 4, 2);
  printf("distribute=%d:%d:%d:%d:%d:%d\n",
         my_number_of_iterations,
         my_first_iteration,
         my_last_iteration,
         number_of_iterations[2],
         first_iteration[2],
         last_iteration[2]);
  distribute_free();

  if (!run_linear_grid(10.0, 0, 0, &sum, &center))
    return 1;
  printf("linear=%.6f:%.6f\n", sum, center);

  if (!run_linear_grid(10.0, 1, 0, &sum, &center))
    return 1;
  printf("invariant=%.6f:%.6f\n", sum, center);

  if (!run_natural_point(10.0, SIBSON, &natural_sibson) ||
      !run_natural_point(10.0, NON_SIBSONIAN, &natural_non_sibson))
    return 1;
  printf("natural=%.6f:%.6f\n", natural_sibson, natural_non_sibson);

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
def nnetwork_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("nnetwork libraries not built")

  tmp_path = tmp_path_factory.mktemp("nnetwork_thread_safety")
  source = tmp_path / "nnetwork_thread_safety.c"
  executable = tmp_path / "nnetwork_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I",
    str(ROOT_DIR / "include"),
    "-I",
    str(ROOT_DIR / "2d_interpolate" / "nn"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lnnetwork",
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


def test_nnetwork_regression_outputs(nnetwork_harness):
  result = run_harness(nnetwork_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "config=7:11:1\n"
    "distribute=3:16:18:3:16:18\n"
    "linear=112.500000:12.500000\n"
    "invariant=112.500000:12.500000\n"
    "natural=12.750000:12.750000\n"
  )


def test_nnetwork_is_thread_safe_without_caller_locks(nnetwork_harness):
  result = run_harness(nnetwork_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"


def test_nnetwork_invariant_scaling_uses_process_rand(nnetwork_harness):
  result = run_harness(nnetwork_harness, "rng")
  assert result.stderr == ""
  assert result.stdout == "rng-compatible\n"


def test_nnetwork_legacy_global_data_symbols_still_link(tmp_path):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("nnetwork libraries not built")

  source = tmp_path / "nnetwork_legacy_global_symbols.c"
  executable = tmp_path / "nnetwork_legacy_global_symbols"
  source.write_text(
    '#define NN_NO_GLOBAL_COMPAT_MACROS\n'
    '#include <stdio.h>\n'
    '#include "nn.h"\n'
    '#include "distribute.h"\n'
    'int nprocesses = 1;\n'
    'int rank = 0;\n'
    'int main(void) {\n'
    '  nn_verbose = 12;\n'
    '  nn_test_vertice = 34;\n'
    '  nn_rule = NON_SIBSONIAN;\n'
    '  my_number_of_iterations = 56;\n'
    '  my_first_iteration = 78;\n'
    '  my_last_iteration = 90;\n'
    '  printf("%d:%d:%d:%d:%d:%d\\n", nn_verbose, nn_test_vertice, (int)nn_rule,\n'
    '         my_number_of_iterations, my_first_iteration, my_last_iteration);\n'
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
    str(ROOT_DIR / "2d_interpolate" / "nn"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lnnetwork",
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
  assert result.stdout == "12:34:1:56:78:90\n"
  assert result.stderr == ""
