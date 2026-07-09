import os
import platform
import shlex
import shutil
import subprocess
from pathlib import Path

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR, external_library_args


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "librpnlib.a",
  LIB_DIR / "libmdbmth.a",
  LIB_DIR / "libmdblib.a",
]

HARNESS_SOURCE = r"""
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <pthread.h>
#if !defined(_WIN32)
#include <signal.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpn.h"

#define THREADS 8
#define ITERATIONS 1000
#define CSH_ITERATIONS 20
#define SHARED_MEMORY_VALUE 42.5
#define SHARED_UDF_VALUE 85.0

double rpn_internal(char *expression);

typedef struct {
  long id;
  char error[256];
} Worker;

static long shared_memory = -1;
static long shared_array_memory = -1;

static int nearly_equal(double actual, double expected)
{
  return fabs(actual - expected) < 1e-12;
}

#if !defined(_WIN32)
static void test_sigusr1_handler(int signum)
{
  (void)signum;
}
#endif

static void set_error(Worker *worker, const char *message, double actual, double expected)
{
  snprintf(worker->error, sizeof(worker->error), "%s: actual=%.17g expected=%.17g",
           message, actual, expected);
}

static int setup_shared_state(void)
{
  long array_index;
  int32_t array_length = 0;
  double *array_data;

  rpn(NULL);
  shared_memory = rpn_create_mem("shared_answer", 0);
  if (shared_memory < 0)
    return 1;
  if (!rpn_store(SHARED_MEMORY_VALUE, NULL, shared_memory))
    return 2;
  create_udf("shared_twice", "shared_answer 2 *");

  shared_array_memory = rpn_create_mem("shared_array", 0);
  if (shared_array_memory < 0)
    return 3;
  array_index = rpn_createarray(3);
  if (array_index < 0)
    return 4;
  if (!rpn_store((double)array_index, NULL, shared_array_memory))
    return 5;
  array_data = rpn_getarraypointer(shared_array_memory, &array_length);
  if (!array_data || array_length != 3)
    return 6;
  array_data[0] = 10.0;
  array_data[1] = 20.0;
  array_data[2] = 30.0;
  return 0;
}

static int run_regression(void)
{
  char ifix[64] = "3 + 4 * 2";
  char pfix[128];
  long memory;
  double expr_add;
  double expr_pow;
  double internal_first;
  double internal_second;
  double after_internal;
  double memory_value;
  double stack_subtract;
  long array_memory;
  long array_index;
  int32_t array_length;
  double *array_data;
  int if2pf_status;

  rpn(NULL);

  expr_add = rpn("cle 1 2 +");
  expr_pow = rpn("cle 9 sqrt 2 pow");
  internal_first = rpn_internal("1 2 +");
  internal_second = rpn_internal("3 4 +");
  after_internal = rpn("cle 4 5 +");

  memory = rpn_create_mem("answer", 0);
  if (memory < 0)
    return 1;
  if (!rpn_store(42.5, NULL, memory))
    return 2;
  memory_value = rpn_recall(memory);

  rpn_clear();
  push_num(10.0);
  push_num(4.0);
  rpn_subtract();
  stack_subtract = pop_num();

  rpn_clear_stacks();
  push_num(1.0);
  push_long(2);
  push_log(1);
  push_string("api");
  if (rpn_numeric_stack_size() != 1 ||
      rpn_long_stack_size() != 1 ||
      rpn_logical_stack_size() != 1 ||
      rpn_string_stack_size() != 1)
    return 3;
  rpn_clear_stacks();
  if (rpn_numeric_stack_size() != 0 ||
      rpn_long_stack_size() != 0 ||
      rpn_logical_stack_size() != 0 ||
      rpn_string_stack_size() != 0)
    return 4;
  dstackptr = 0;
  sstackptr = 0;
  push_long(3);
  push_string("api");
  if (dstackptr != 1 || sstackptr != 1)
    return 5;
  rpn_clear_stacks();

  array_memory = rpn_create_mem("regression_array", 0);
  if (array_memory < 0)
    return 6;
  array_index = rpn_createarray(2);
  if (array_index < 0)
    return 7;
  if (!rpn_resizearray(array_index, 4))
    return 8;
  if (rpn_resizearray(-1, 4) || rpn_resizearray(array_index + 1, 4))
    return 9;
  if (!rpn_store((double)array_index, NULL, array_memory))
    return 10;
  array_length = -1;
  array_data = rpn_getarraypointer(array_memory, &array_length);
  if (!array_data || array_length != 4)
    return 11;
  if (!rpn_getarraypointer(array_memory, NULL))
    return 12;
  if (!rpn_store(-1.0, NULL, array_memory))
    return 13;
  array_length = -1;
  if (rpn_getarraypointer(array_memory, &array_length) || array_length != 0)
    return 14;
  if (!rpn_store((double)(array_index + 1), NULL, array_memory))
    return 15;
  array_length = -1;
  if (rpn_getarraypointer(array_memory, &array_length) || array_length != 0)
    return 16;
  if (!rpn_store((double)array_index, NULL, array_memory))
    return 17;
  rpn_clear_stacks();
  push_num(123.0);
  push_num(2.0);
  push_num((double)array_index);
  sref();
  push_num(2.0);
  push_num((double)array_index);
  rref();
  if (!nearly_equal(pop_num(), 123.0))
    return 18;

  if2pf_status = if2pf(pfix, ifix, sizeof(pfix));

  printf("rpn_add=%.17g\n", expr_add);
  printf("rpn_pow=%.17g\n", expr_pow);
  printf("rpn_internal=%.17g:%.17g\n", internal_first, internal_second);
  printf("rpn_after_internal=%.17g\n", after_internal);
  printf("rpn_memory=%.17g\n", memory_value);
  printf("stack_subtract=%.17g\n", stack_subtract);
  printf("direct_stackptrs=%ld:%ld\n", dstackptr, sstackptr);
  printf("array_bounds=ok\n");
  printf("if2pf=%d:%s\n", if2pf_status, pfix);
  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  char memory_name[64];
  char udf_name[64];
  char mudf_name[64];
  char udf_expression[128];
  char mudf_expression[128];
  char rpn_expression[128];
  char mudf_rpn_expression[128];
  long memory;
  long udf_number;
  long udf_index;
  long i;
  int32_t array_length = 0;
  double *array_data;
  double shared_value;
  double shared_udf_value;

  if (rpn_long_stack_size() != 0 || rpn_string_stack_size() != 0) {
    snprintf(worker->error, sizeof(worker->error), "fresh thread inherited legacy stack pointer mirrors");
    return worker;
  }

  rpn(NULL);
  shared_value = rpn_recall(shared_memory);
  if (!nearly_equal(shared_value, SHARED_MEMORY_VALUE)) {
    set_error(worker, "shared memory was not visible", shared_value, SHARED_MEMORY_VALUE);
    return worker;
  }
  shared_udf_value = rpn("cle shared_twice");
  if (!nearly_equal(shared_udf_value, SHARED_UDF_VALUE)) {
    set_error(worker, "shared UDF was not visible", shared_udf_value, SHARED_UDF_VALUE);
    return worker;
  }
  udf_number = find_udf("shared_twice");
  if (udf_number < 0) {
    snprintf(worker->error, sizeof(worker->error), "find_udf failed for shared UDF");
    return worker;
  }
  udf_index = find_udf_mod("shared_twice");
  if (udf_index < 0) {
    snprintf(worker->error, sizeof(worker->error), "find_udf_mod failed for shared UDF");
    return worker;
  }
  array_data = rpn_getarraypointer(shared_array_memory, &array_length);
  if (!array_data || array_length != 3 ||
      !nearly_equal(array_data[0], 10.0) ||
      !nearly_equal(array_data[1], 20.0) ||
      !nearly_equal(array_data[2], 30.0)) {
    snprintf(worker->error, sizeof(worker->error), "shared array was not visible");
    return worker;
  }

  snprintf(memory_name, sizeof(memory_name), "thread_value_%ld", worker->id);
  memory = rpn_create_mem(memory_name, 0);
  if (memory < 0) {
    snprintf(worker->error, sizeof(worker->error), "rpn_create_mem failed");
    return worker;
  }

  snprintf(udf_name, sizeof(udf_name), "thread_udf_%ld", worker->id);
  snprintf(udf_expression, sizeof(udf_expression), "%s 2 *", memory_name);
  create_udf(udf_name, udf_expression);
  link_udfs();
  snprintf(rpn_expression, sizeof(rpn_expression), "cle %s", udf_name);

  snprintf(mudf_name, sizeof(mudf_name), "thread_mudf_%ld", worker->id);
  snprintf(mudf_expression, sizeof(mudf_expression), "%s 3 +", memory_name);
  rpn_clear_stacks();
  push_string(mudf_name);
  push_string(mudf_expression);
  rpn_mudf();
  snprintf(mudf_rpn_expression, sizeof(mudf_rpn_expression), "cle %s", mudf_name);

  for (i = 0; i < ITERATIONS; i++) {
    double expected_stack = (double)(worker->id + i);
    double expected_memory = (double)(worker->id * 1000000 + i);
    double stack_value;
    double memory_value;
    double expression_value;
    double udf_value;
    double mudf_value;
    char ifix[64] = "3 + 4 * 2";
    char pfix[128];

    if (is_func("+") < 0) {
      snprintf(worker->error, sizeof(worker->error), "is_func failed during concurrent use");
      return worker;
    }

    rpn_clear();
    push_num((double)worker->id);
    push_num((double)i);
    rpn_add();
    stack_value = pop_num();
    if (!nearly_equal(stack_value, expected_stack)) {
      set_error(worker, "stack API interleaved", stack_value, expected_stack);
      return worker;
    }

    if (!rpn_store(expected_memory, NULL, memory)) {
      snprintf(worker->error, sizeof(worker->error), "rpn_store failed");
      return worker;
    }
    memory_value = rpn_recall(memory);
    if (!nearly_equal(memory_value, expected_memory)) {
      set_error(worker, "worker memory changed", memory_value, expected_memory);
      return worker;
    }

    expression_value = rpn("cle 1 2 +");
    if (!nearly_equal(expression_value, 3.0)) {
      set_error(worker, "rpn expression changed", expression_value, 3.0);
      return worker;
    }
    udf_value = rpn(rpn_expression);
    if (!nearly_equal(udf_value, expected_memory * 2.0)) {
      set_error(worker, "thread UDF changed", udf_value, expected_memory * 2.0);
      return worker;
    }
    mudf_value = rpn(mudf_rpn_expression);
    if (!nearly_equal(mudf_value, expected_memory + 3.0)) {
      set_error(worker, "thread mudf changed", mudf_value, expected_memory + 3.0);
      return worker;
    }
    udf_number = find_udf(udf_name);
    if (udf_number < 0) {
      snprintf(worker->error, sizeof(worker->error), "find_udf failed during concurrent use");
      return worker;
    }

    if (if2pf(pfix, ifix, sizeof(pfix)) || strcmp(pfix, "3 4 2 * + ")) {
      snprintf(worker->error, sizeof(worker->error), "if2pf changed: %s", pfix);
      return worker;
    }
  }

  udf_number = find_udf("shared_twice");
  if (udf_number < 0 || !get_udf(udf_number)) {
    snprintf(worker->error, sizeof(worker->error), "get_udf failed for shared UDF");
    return worker;
  }
  get_udf_indexes(udf_number);
  udf_number = find_udf(udf_name);
  if (udf_number < 0 || !get_udf(udf_number)) {
    snprintf(worker->error, sizeof(worker->error), "get_udf failed for thread UDF");
    return worker;
  }
  get_udf_indexes(udf_number);

  return NULL;
}

static void *defns_thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  double value;

  rpn(NULL);
  value = rpn("cle env_twice");
  if (!nearly_equal(value, 42.0)) {
    set_error(worker, "RPN_DEFNS UDF was not visible", value, 42.0);
    return worker;
  }
  return NULL;
}

static void *csh_thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  rpn(NULL);
  for (i = 0; i < CSH_ITERATIONS; i++) {
    rpn_clear_error();
    push_string("true");
    rpn_csh_str();
    if (rpn_check_error()) {
      snprintf(worker->error, sizeof(worker->error), "rpn_csh_str failed");
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
  int setup_status;

  setup_status = setup_shared_state();
  if (setup_status) {
    fprintf(stderr, "setup_shared_state failed: %d\n", setup_status);
    return 1;
  }

  rpn_clear_stacks();
  push_long(99);
  push_string("main-thread-only");
  if (rpn_long_stack_size() != 1 || rpn_string_stack_size() != 1) {
    fputs("main stack setup failed\n", stderr);
    rpn_clear_stacks();
    return 1;
  }

  for (i = 0; i < THREADS; i++) {
    workers[i].id = i + 1;
    workers[i].error[0] = 0;
    if (pthread_create(&threads[i], NULL, thread_worker, &workers[i])) {
      fprintf(stderr, "pthread_create failed\n");
      rpn_clear_stacks();
      return 1;
    }
  }

  for (i = 0; i < THREADS; i++) {
    void *status = NULL;
    if (pthread_join(threads[i], &status)) {
      fprintf(stderr, "pthread_join failed\n");
      rpn_clear_stacks();
      return 1;
    }
    if (status) {
      fprintf(stderr, "thread %ld failed: %s\n", i + 1, ((Worker *)status)->error);
      rpn_clear_stacks();
      return 1;
    }
  }

  rpn_clear_stacks();
  puts("thread-safe");
  return 0;
}

static int run_defns_thread_test(void)
{
  pthread_t threads[THREADS];
  Worker workers[THREADS];
  double value;
  long i;

  rpn(NULL);
  value = rpn("cle env_twice");
  if (!nearly_equal(value, 42.0)) {
    fprintf(stderr, "main RPN_DEFNS UDF failed: %.17g\n", value);
    return 1;
  }

  for (i = 0; i < THREADS; i++) {
    workers[i].id = i + 1;
    workers[i].error[0] = 0;
    if (pthread_create(&threads[i], NULL, defns_thread_worker, &workers[i])) {
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

  puts("defns-thread-safe");
  return 0;
}

static int run_csh_thread_test(void)
{
#if defined(_WIN32)
  fputs("csh signal regression is not available on Windows\n", stderr);
  return 1;
#else
  pthread_t threads[THREADS];
  Worker workers[THREADS];
  struct sigaction sigusr1_action;
  struct sigaction old_sigusr1_action;
  long i;
  long created = 0;
  int status = 0;

  memset(&sigusr1_action, 0, sizeof(sigusr1_action));
  sigusr1_action.sa_handler = test_sigusr1_handler;
  sigemptyset(&sigusr1_action.sa_mask);
  if (sigaction(SIGUSR1, &sigusr1_action, &old_sigusr1_action)) {
    perror("sigaction install failed");
    return 1;
  }

  for (i = 0; i < THREADS; i++) {
    workers[i].id = i + 1;
    workers[i].error[0] = 0;
    if (pthread_create(&threads[i], NULL, csh_thread_worker, &workers[i])) {
      fputs("pthread_create failed\n", stderr);
      status = 1;
      break;
    }
    created++;
  }

  for (i = 0; i < created; i++) {
    void *thread_status = NULL;
    if (pthread_join(threads[i], &thread_status)) {
      fputs("pthread_join failed\n", stderr);
      status = 1;
      continue;
    }
    if (thread_status) {
      fprintf(stderr, "thread %ld failed: %s\n", i + 1, ((Worker *)thread_status)->error);
      status = 1;
    }
  }

  if (sigaction(SIGUSR1, NULL, &sigusr1_action)) {
    perror("sigaction readback failed");
    status = 1;
  } else if (sigusr1_action.sa_handler != test_sigusr1_handler) {
    fputs("SIGUSR1 handler changed during cshs execution\n", stderr);
    status = 1;
  }

  if (sigaction(SIGUSR1, &old_sigusr1_action, NULL)) {
    perror("sigaction restore failed");
    return 1;
  }

  if (!status)
    puts("csh-thread-safe");
  return status;
#endif
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "threads"))
    return run_thread_test();
  if (argc > 1 && !strcmp(argv[1], "defns"))
    return run_defns_thread_test();
  if (argc > 1 && !strcmp(argv[1], "csh"))
    return run_csh_thread_test();
  return run_regression();
}
"""


def compiler_command():
  command = shlex.split(os.environ.get("CC", "gcc"))
  if not command or not shutil.which(command[0]):
    pytest.skip("C compiler not available")
  return command


@pytest.fixture(scope="module")
def rpn_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("RPN libraries not built")

  tmp_path = tmp_path_factory.mktemp("rpn_thread_safety")
  source = tmp_path / "rpn_thread_safety.c"
  executable = tmp_path / "rpn_thread_safety"
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
    "-lrpnlib",
    "-lmdbmth",
    "-lmdblib",
    *external_library_args("gsl", "gslcblas", "z"),
    "-lpthread",
    "-lm",
    "-o",
    str(executable),
  ]
  if platform.system() == "Linux":
    command[-2:-2] = ["-lrt", "-ldl", "-lgcc"]

  subprocess.run(command, check=True)
  return executable


def run_harness(executable, *args, extra_env=None):
  env = os.environ.copy()
  env["RPN_DEFNS"] = ""
  if extra_env:
    env.update(extra_env)
  return subprocess.run(
    [str(executable), *args],
    check=True,
    capture_output=True,
    text=True,
    env=env,
    timeout=30,
  )


def test_rpn_api_regression_outputs(rpn_harness):
  result = run_harness(rpn_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "rpn_add=3\n"
    "rpn_pow=9\n"
    "rpn_internal=3:7\n"
    "rpn_after_internal=9\n"
    "rpn_memory=42.5\n"
    "stack_subtract=6\n"
    "direct_stackptrs=0:0\n"
    "array_bounds=ok\n"
    "if2pf=0:3 4 2 * + \n"
  )


def test_rpn_legacy_stackptr_data_symbols_still_link(tmp_path):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("RPN libraries not built")

  source = tmp_path / "rpn_legacy_stackptr_symbols.c"
  executable = tmp_path / "rpn_legacy_stackptr_symbols"
  source.write_text(
    '#define RPN_NO_STACKPTR_COMPAT_MACROS\n'
    '#include <stdio.h>\n'
    '#include "rpn.h"\n'
    'int main(void) {\n'
    '  dstackptr = 12;\n'
    '  sstackptr = 34;\n'
    '  printf("%ld:%ld\\n", dstackptr, sstackptr);\n'
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
    str(source),
    "-L",
    str(LIB_DIR),
    "-lrpnlib",
    "-lmdbmth",
    "-lmdblib",
    *external_library_args("gsl", "gslcblas", "z"),
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


def test_rpn_api_is_thread_safe_without_caller_locks(rpn_harness):
  result = run_harness(rpn_harness, "threads")
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"


def test_rpn_defns_are_shared_with_worker_threads(rpn_harness, tmp_path):
  defns = tmp_path / "rpn.defns"
  defns.write_text(
    "21 sto env_answer\n"
    "udf\n"
    "env_twice\n"
    "env_answer 2 *\n"
    "\n",
    encoding="ascii",
  )

  result = run_harness(rpn_harness, "defns", extra_env={"RPN_DEFNS": str(defns)})
  assert result.stderr == ""
  assert result.stdout == "defns-thread-safe\n"


def test_rpn_cshs_is_thread_safe_without_signal_handlers(rpn_harness):
  if platform.system() == "Windows" or not shutil.which("csh"):
    pytest.skip("csh not available")

  result = run_harness(rpn_harness, "csh")
  assert result.stderr == ""
  assert result.stdout == "csh-thread-safe\n"
