#ifndef SDDS_TEST_WINDOWS_PTHREAD_H
#define SDDS_TEST_WINDOWS_PTHREAD_H

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pthread_thread_state {
  HANDLE handle;
  void *(*start)(void *);
  void *argument;
  void *result;
} *pthread_t;

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT

static DWORD WINAPI pthread_test_start(LPVOID context) {
  struct pthread_thread_state *state = (struct pthread_thread_state *)context;
  state->result = state->start(state->argument);
  return 0;
}

static int pthread_create(pthread_t *thread, const void *attributes,
                          void *(*start)(void *), void *argument) {
  struct pthread_thread_state *state;
  (void)attributes;
  state = (struct pthread_thread_state *)calloc(1, sizeof(*state));
  if (!state)
    return ERROR_NOT_ENOUGH_MEMORY;
  state->start = start;
  state->argument = argument;
  state->handle = CreateThread(NULL, 0, pthread_test_start, state, 0, NULL);
  if (!state->handle) {
    int error = (int)GetLastError();
    free(state);
    return error ? error : 1;
  }
  *thread = state;
  return 0;
}

static int pthread_join(pthread_t thread, void **result) {
  DWORD wait_result;
  if (!thread)
    return ERROR_INVALID_HANDLE;
  wait_result = WaitForSingleObject(thread->handle, INFINITE);
  if (wait_result != WAIT_OBJECT_0)
    return (int)GetLastError();
  if (result)
    *result = thread->result;
  CloseHandle(thread->handle);
  free(thread);
  return 0;
}

static int pthread_mutex_init(pthread_mutex_t *mutex, const void *attributes) {
  (void)attributes;
  InitializeSRWLock(mutex);
  return 0;
}

static int pthread_mutex_destroy(pthread_mutex_t *mutex) {
  (void)mutex;
  return 0;
}

static int pthread_mutex_lock(pthread_mutex_t *mutex) {
  AcquireSRWLockExclusive(mutex);
  return 0;
}

static int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  ReleaseSRWLockExclusive(mutex);
  return 0;
}

static int pthread_cond_init(pthread_cond_t *condition, const void *attributes) {
  (void)attributes;
  InitializeConditionVariable(condition);
  return 0;
}

static int pthread_cond_destroy(pthread_cond_t *condition) {
  (void)condition;
  return 0;
}

static int pthread_cond_wait(pthread_cond_t *condition, pthread_mutex_t *mutex) {
  if (SleepConditionVariableSRW(condition, mutex, INFINITE, 0))
    return 0;
  return (int)GetLastError();
}

static int pthread_cond_broadcast(pthread_cond_t *condition) {
  WakeAllConditionVariable(condition);
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif
