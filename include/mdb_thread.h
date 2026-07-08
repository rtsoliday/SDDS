#ifndef MDB_THREAD_H
#define MDB_THREAD_H 1

#if defined(SDDS_UNSAFE_NO_THREADS)
#  define MDB_THREAD_LOCAL
#elif defined(_MSC_VER)
#  define MDB_THREAD_LOCAL __declspec(thread)
#elif defined(__cplusplus) && __cplusplus >= 201103L
#  define MDB_THREAD_LOCAL thread_local
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#  define MDB_THREAD_LOCAL _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
#  define MDB_THREAD_LOCAL __thread
#else
#  error "SDDS thread-safety requires compiler TLS support. Define SDDS_UNSAFE_NO_THREADS only for explicit single-thread builds."
#endif

#if defined(SDDS_UNSAFE_NO_THREADS)
typedef int MDB_THREAD_LOCK;
#  define MDB_THREAD_LOCK_INITIALIZER 0
static inline void mdb_thread_lock(MDB_THREAD_LOCK *lock) {
  (void)lock;
}
static inline void mdb_thread_unlock(MDB_THREAD_LOCK *lock) {
  (void)lock;
}
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#  include <stdatomic.h>
typedef atomic_flag MDB_THREAD_LOCK;
#  define MDB_THREAD_LOCK_INITIALIZER ATOMIC_FLAG_INIT
static inline void mdb_thread_lock(MDB_THREAD_LOCK *lock) {
  while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
  }
}
static inline void mdb_thread_unlock(MDB_THREAD_LOCK *lock) {
  atomic_flag_clear_explicit(lock, memory_order_release);
}
#elif defined(_MSC_VER)
#  include <windows.h>
typedef volatile LONG MDB_THREAD_LOCK;
#  define MDB_THREAD_LOCK_INITIALIZER 0
static inline void mdb_thread_lock(MDB_THREAD_LOCK *lock) {
  while (InterlockedCompareExchange(lock, 1, 0) != 0)
    Sleep(0);
}
static inline void mdb_thread_unlock(MDB_THREAD_LOCK *lock) {
  InterlockedExchange(lock, 0);
}
#elif defined(__GNUC__) || defined(__clang__)
typedef volatile int MDB_THREAD_LOCK;
#  define MDB_THREAD_LOCK_INITIALIZER 0
static inline void mdb_thread_lock(MDB_THREAD_LOCK *lock) {
  while (__sync_lock_test_and_set(lock, 1)) {
    while (*lock) {
    }
  }
}
static inline void mdb_thread_unlock(MDB_THREAD_LOCK *lock) {
  __sync_lock_release(lock);
}
#else
#  error "SDDS thread-safety requires atomic lock support. Define SDDS_UNSAFE_NO_THREADS only for explicit single-thread builds."
#endif

#endif
