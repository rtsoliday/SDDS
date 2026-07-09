import ctypes
import os
import platform
import shlex
import shutil
import subprocess

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libmeschach.a",
]

HARNESS_SOURCE = r"""
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"
#include "matrix2.h"
#include "sparse.h"
#include "iter.h"

#ifdef mem_connect
#undef mem_connect
#endif
extern MEM_CONNECT mem_connect[MEM_CONNECT_MAX_LISTS];
#ifdef restart
#undef restart
#endif
extern jmp_buf restart;
static void *legacy_restart_symbol(void)
{
  return (void *)restart;
}

static int check_legacy_restart_recovery(void)
{
  if (setjmp(restart)) {
    set_err_flag(EF_EXIT);
    return 1;
  }
  set_err_flag(EF_SILENT);
  error(E_NULL, "legacy_restart");
  set_err_flag(EF_EXIT);
  return 0;
}
#define restart (*meschach_restart_ptr())

#define THREADS 8
#define ITERATIONS 500

typedef struct {
  long list[56];
  int started;
  int inext;
  int inextp;
} RefRand;

typedef struct {
  long id;
  int seed;
  RefRand reference_random;
  char error[256];
} Worker;

#ifdef LONG_MAX
#define REF_MODULUS LONG_MAX
#else
#define REF_MODULUS 1000000000L
#endif

static double ref_mrand(RefRand *state);

static long ref_seed_step(long value)
{
  unsigned long product = 123413UL * (unsigned long)value;
  unsigned long magnitude;

  if (product <= (unsigned long)LONG_MAX)
    return (long)(product % (unsigned long)REF_MODULUS);
  magnitude = ULONG_MAX - product + 1UL;
  return -(long)(magnitude % (unsigned long)REF_MODULUS);
}

static long ref_subtract(long left, long right)
{
  unsigned long difference = (unsigned long)left - (unsigned long)right;
  unsigned long magnitude;

  if (difference <= (unsigned long)LONG_MAX)
    return (long)difference;
  magnitude = ULONG_MAX - difference + 1UL;
  if (magnitude == (unsigned long)LONG_MAX + 1UL)
    return LONG_MIN;
  return -(long)magnitude;
}

static void ref_smrand(RefRand *state, int seed)
{
  int i;

  state->list[0] = ref_seed_step((long)seed);
  for (i = 1; i < 55; i++)
    state->list[i] = ref_seed_step(state->list[i - 1]);

  state->started = 1;
  state->inext = 0;
  state->inextp = 31;
  for (i = 0; i < 55 * 55; i++)
    ref_mrand(state);
}

static double ref_mrand(RefRand *state)
{
  long lval;
  double factor = 1.0 / ((double)REF_MODULUS);

  if (!state->started)
    ref_smrand(state, 3127);

  state->inext = (state->inext >= 54) ? 0 : state->inext + 1;
  state->inextp = (state->inextp >= 54) ? 0 : state->inextp + 1;

  lval = ref_subtract(state->list[state->inext], state->list[state->inextp]);
  if (lval < 0L)
    lval += REF_MODULUS;
  state->list[state->inext] = lval;

  return (double)lval * factor;
}

static int nearly_equal(double actual, double expected)
{
  return fabs(actual - expected) < 1e-10;
}

static void set_error(Worker *worker, const char *message, double actual, double expected)
{
  snprintf(worker->error, sizeof(worker->error), "%s: actual=%.17g expected=%.17g",
           message, actual, expected);
}

static void fill_matrix(MAT *A, double id)
{
  A->me[0][0] = 4.0 + id;
  A->me[0][1] = 7.0;
  A->me[1][0] = 2.0;
  A->me[1][1] = 6.0 + id;
}

static int check_dense_math(Worker *worker)
{
  double id = (double)worker->id;
  double a = 4.0 + id;
  double d = 6.0 + id;
  double det = a * d - 14.0;
  MAT *A = m_get(2, 2);
  MAT *inverse = MNULL;
  MAT *power = MNULL;
  VEC *rhs = v_get(2);
  VEC *solution = VNULL;
  PERM *pivot = px_get(2);

  fill_matrix(A, id);
  inverse = m_inverse(A, inverse);
  if (!nearly_equal(inverse->me[0][0], d / det)) {
    set_error(worker, "m_inverse[0][0]", inverse->me[0][0], d / det);
    return 1;
  }
  if (!nearly_equal(inverse->me[1][0], -2.0 / det)) {
    set_error(worker, "m_inverse[1][0]", inverse->me[1][0], -2.0 / det);
    return 1;
  }

  power = m_pow(A, 2, power);
  if (!nearly_equal(power->me[0][0], a * a + 14.0)) {
    set_error(worker, "m_pow[0][0]", power->me[0][0], a * a + 14.0);
    return 1;
  }
  if (!nearly_equal(power->me[0][1], 7.0 * (a + d))) {
    set_error(worker, "m_pow[0][1]", power->me[0][1], 7.0 * (a + d));
    return 1;
  }

  rhs->ve[0] = 1.0;
  rhs->ve[1] = 0.0;
  LUfactor(A, pivot);
  solution = LUsolve(A, pivot, rhs, solution);
  if (!nearly_equal(solution->ve[0], d / det)) {
    set_error(worker, "LUsolve[0]", solution->ve[0], d / det);
    return 1;
  }
  if (!nearly_equal(solution->ve[1], -2.0 / det)) {
    set_error(worker, "LUsolve[1]", solution->ve[1], -2.0 / det);
    return 1;
  }

  m_free(A);
  m_free(inverse);
  m_free(power);
  v_free(rhs);
  v_free(solution);
  px_free(pivot);
  return 0;
}

static int check_sparse_math(Worker *worker)
{
  double id = (double)worker->id;
  SPMAT *A = sp_get(2, 2, 1);
  SPMAT *B = sp_get(2, 2, 1);
  double d0, d1;

  sp_set_val(A, 0, 0, id);
  sp_set_val(A, 1, 1, id + 1.0);
  sp_set_val(B, 0, 0, 2.0);
  sp_set_val(B, 1, 1, 3.0);
  sp_add(A, B, A);

  d0 = sp_get_val(A, 0, 0);
  d1 = sp_get_val(A, 1, 1);
  if (!nearly_equal(d0, id + 2.0)) {
    set_error(worker, "sp_add[0][0]", d0, id + 2.0);
    return 1;
  }
  if (!nearly_equal(d1, id + 4.0)) {
    set_error(worker, "sp_add[1][1]", d1, id + 4.0);
    return 1;
  }

  sp_free(A);
  sp_free(B);
  return 0;
}

static int ref_mrand_index(RefRand *state, int limit)
{
  int value;

  if (limit <= 1)
    return 0;
  value = (int)((double)limit * ref_mrand(state));
  return value >= limit ? limit - 1 : value;
}

static void ref_iter_gen_sym(RefRand *state, int n, int nrow)
{
  int i, k, k_max;

  if (nrow <= 1)
    nrow = 2;
  if (nrow & 1)
    nrow -= 1;
  for (i = 0; i < n; i++) {
    k_max = ref_mrand_index(state, nrow / 2);
    for (k = 0; k <= k_max; k++) {
      ref_mrand_index(state, n);
      ref_mrand(state);
    }
  }
}

static void ref_iter_gen_nonsym(RefRand *state, int m, int n, int nrow)
{
  int i, k, k_max;

  if (nrow <= 1)
    nrow = 2;
  for (i = 0; i < m; i++) {
    k_max = ref_mrand_index(state, nrow - 1);
    for (k = 0; k <= k_max; k++) {
      ref_mrand_index(state, n);
      ref_mrand(state);
    }
  }
  for (i = 0; i < 2 * n; i++) {
    ref_mrand_index(state, n);
    ref_mrand_index(state, n);
  }
}

static void ref_iter_gen_nonsym_posdef(RefRand *state, int n, int nrow)
{
  int i, k, k_max;

  if (nrow <= 1)
    nrow = 2;
  for (i = 0; i < n; i++) {
    k_max = ref_mrand_index(state, nrow - 1);
    for (k = 0; k <= k_max; k++) {
      ref_mrand_index(state, n);
      ref_mrand(state);
    }
  }
}

static int check_iter_generators(Worker *worker)
{
  SPMAT *sym;
  SPMAT *nonsym;
  SPMAT *posdef;
  double actual_next;
  double expected_next;
  int seed = worker->seed + 20000;

  smrand(seed);
  ref_smrand(&worker->reference_random, seed);

  sym = iter_gen_sym(8, 4);
  nonsym = iter_gen_nonsym(9, 7, 4, 1.0);
  posdef = iter_gen_nonsym_posdef(8, 4);
  ref_iter_gen_sym(&worker->reference_random, 8, 4);
  ref_iter_gen_nonsym(&worker->reference_random, 9, 7, 4);
  ref_iter_gen_nonsym_posdef(&worker->reference_random, 8, 4);

  sp_free(sym);
  sp_free(nonsym);
  sp_free(posdef);

  actual_next = mrand();
  expected_next = ref_mrand(&worker->reference_random);
  if (!nearly_equal(actual_next, expected_next)) {
    set_error(worker, "iter generator random state interleaved", actual_next, expected_next);
    return 1;
  }
  return 0;
}

static int check_random_state(Worker *worker)
{
  double values[3];
  double expected[3];
  int i;

  ref_smrand(&worker->reference_random, worker->seed);
  expected[0] = ref_mrand(&worker->reference_random);
  expected[1] = ref_mrand(&worker->reference_random);
  expected[2] = ref_mrand(&worker->reference_random);

  smrand(worker->seed);
  values[0] = mrand();
  values[1] = mrand();
  values[2] = mrand();

  for (i = 0; i < 3; i++) {
    if (!nearly_equal(values[i], expected[i])) {
      set_error(worker, "mrand sequence interleaved", values[i], expected[i]);
      return 1;
    }
  }
  return 0;
}

static int check_memory_accounting(Worker *worker)
{
  VEC *probe;
  int before;
  int after;

  mem_info_on(1);
  before = mem_info_numvar(TYPE_VEC, 0);
  probe = v_get(5);
  after = mem_info_numvar(TYPE_VEC, 0);
  if (after != before + 1) {
    snprintf(worker->error, sizeof(worker->error),
             "mem_info_numvar after allocation: actual=%d expected=%d",
             after, before + 1);
    return 1;
  }
  v_free(probe);
  after = mem_info_numvar(TYPE_VEC, 0);
  if (after != before) {
    snprintf(worker->error, sizeof(worker->error),
             "mem_info_numvar after free: actual=%d expected=%d",
             after, before);
    return 1;
  }
  mem_info_on(0);
  return 0;
}

static int check_error_recovery(Worker *worker)
{
  int caught = 0;

  catch(E_NULL, v_zero(VNULL), caught = 1);
  if (!caught) {
    snprintf(worker->error, sizeof(worker->error), "catch did not recover E_NULL");
    return 1;
  }
  return 0;
}

static int run_regression(void)
{
  MAT *A = m_get(2, 2);
  MAT *inverse = MNULL;
  MAT *power = MNULL;
  VEC *rhs = v_get(2);
  VEC *solution = VNULL;
  PERM *pivot = px_get(2);
  VEC *probe;
  int caught = 0;
  double r0, r1, r2;

  void *legacy_restart_addr = legacy_restart_symbol();

  if (!mem_connect[0].type_names || !mem_connect[0].free_funcs ||
      !mem_connect[0].info_sum || mem_connect[0].ntypes == 0)
    return 1;
  if (!legacy_restart_addr)
    return 1;
  if (!check_legacy_restart_recovery())
    return 1;

  A->me[0][0] = 4.0;
  A->me[0][1] = 7.0;
  A->me[1][0] = 2.0;
  A->me[1][1] = 6.0;
  rhs->ve[0] = 1.0;
  rhs->ve[1] = 0.0;

  inverse = m_inverse(A, inverse);
  power = m_pow(A, 2, power);
  LUfactor(A, pivot);
  solution = LUsolve(A, pivot, rhs, solution);

  smrand(7);
  r0 = mrand();
  r1 = mrand();
  r2 = mrand();

  catch(E_NULL, v_zero(VNULL), caught = 1);

  printf("inverse=%.17g,%.17g,%.17g,%.17g\n",
         inverse->me[0][0], inverse->me[0][1], inverse->me[1][0], inverse->me[1][1]);
  printf("power=%.17g,%.17g,%.17g,%.17g\n",
         power->me[0][0], power->me[0][1], power->me[1][0], power->me[1][1]);
  printf("solve=%.17g,%.17g\n", solution->ve[0], solution->ve[1]);
  printf("rand=%.17g,%.17g,%.17g\n", r0, r1, r2);
  printf("catch=%d\n", caught);
  printf("legacy_mem_connect=1\n");
  printf("legacy_restart=1\n");

  mem_info_on(1);
  probe = v_get(5);
  printf("mem_after=%ld,%d\n", mem_info_bytes(TYPE_VEC, 0), mem_info_numvar(TYPE_VEC, 0));
  v_free(probe);
  printf("mem_final=%ld,%d\n", mem_info_bytes(TYPE_VEC, 0), mem_info_numvar(TYPE_VEC, 0));
  mem_info_on(0);
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  if (check_error_recovery(worker))
    return worker;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_random_state(worker) ||
        check_iter_generators(worker) ||
        check_memory_accounting(worker) ||
        check_dense_math(worker) ||
        check_sparse_math(worker))
      return worker;
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
    workers[i].seed = 1000 + (int)i;
    memset(&workers[i].reference_random, 0, sizeof(workers[i].reference_random));
    workers[i].reference_random.inextp = 31;
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
def meschach_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("meschach library not built")

  tmp_path = tmp_path_factory.mktemp("meschach_thread_safety")
  source = tmp_path / "meschach_thread_safety.c"
  executable = tmp_path / "meschach_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I",
    str(ROOT_DIR / "meschach"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lmeschach",
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


def test_meschach_api_regression_outputs(meschach_harness):
  result = run_harness(meschach_harness)
  assert result.stderr == ""
  if ctypes.sizeof(ctypes.c_long) == 4:
    expected_random = "0.27627434128721912,0.04329907197658861,0.67216925633706581"
  else:
    expected_random = "0.80915062618171318,0.74767531892110062,0.51433257064167481"
  assert result.stdout == (
    "inverse=0.60000000000000009,-0.70000000000000007,-0.20000000000000001,0.40000000000000002\n"
    "power=30,70,20,50\n"
    "solve=0.60000000000000009,-0.20000000000000001\n"
    f"rand={expected_random}\n"
    "catch=1\n"
    "legacy_mem_connect=1\n"
    "legacy_restart=1\n"
    "mem_after=56,1\n"
    "mem_final=0,0\n"
  )


def test_meschach_api_is_thread_safe_without_caller_locks(meschach_harness):
  result = run_harness(meschach_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
