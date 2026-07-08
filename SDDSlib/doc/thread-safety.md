# SDDS Library Thread Safety

The SDDS libraries are intended to support concurrent use by multiple threads
when each thread operates on its own library objects and caller-owned buffers.
This applies to the core SDDS library and the supporting utility libraries used
by the toolkit, including RPN, mdb, mdbmth, mdblib, namelist, matlib, fftpack,
meschach, CSA, and natural-neighbor interpolation.

## General Contract

- Independent `SDDS_DATASET` objects may be read, written, queried, and
  terminated concurrently from different threads.
- A single `SDDS_DATASET` object is not internally locked as a unit. If multiple
  threads access or mutate the same dataset handle, the caller must serialize
  those operations.
- Returned pointers and caller-owned buffers keep their normal lifetime rules.
  Thread safety does not make borrowed pointers safe to retain while another
  thread mutates the object that owns them.
- Process-global settings are protected against data races, but they are still
  process-global settings. Set them before creating worker threads when possible,
  or serialize changes if deterministic per-thread behavior is required.
- The libraries are not async-signal-safe.

## Thread-Safety Support Layer

`include/mdb_thread.h` provides the internal portability layer used by the
libraries:

- `MDB_THREAD_LOCAL` maps to compiler-supported thread-local storage.
- `MDB_THREAD_LOCK`, `MDB_THREAD_LOCK_INITIALIZER`, `mdb_thread_lock()`, and
  `mdb_thread_unlock()` provide small internal locks for process-global state.
- Defining `SDDS_UNSAFE_NO_THREADS` disables these protections and is intended
  only for explicit single-thread builds. Normal builds require compiler
  thread-local storage and an atomic or platform lock primitive.

## Core SDDS Library

The core SDDS library now keeps error state per thread. Calls such as
`SDDS_SetError()`, `SDDS_SetError0()`, `SDDS_NumberOfErrors()`,
`SDDS_GetErrorMessages()`, `SDDS_PrintErrors()`, and `SDDS_ClearErrors()`
operate on the calling thread's error stack.

The following SDDS process-global settings are guarded by internal locks:

- `SDDS_SetDefaultIOBufferSize()`
- `SDDS_SetRowLimit()` and `SDDS_GetRowLimit()`
- `SDDS_SetTerminateMode()`
- `SDDS_SetNameValidityFlags()`
- `SDDS_SetAutoCheckMode()`
- `SDDS_RegisterProgramName()`

These functions can be called without corrupting shared library state, but their
settings affect the process, not just the calling thread.

Several former static scratch buffers and match-state arrays are now
thread-local, including the internal state used by recursive pointer-array
construction and by `SDDS_MatchColumns()`, `SDDS_MatchParameters()`, and
`SDDS_MatchArrays()`.

## RPN Library

The RPN execution stacks are per-thread. Concurrent calls to `rpn()` and normal
stack operations do not share numeric, logical, string, or integer stack
contents between threads.

RPN definitions, memories, and arrays remain process-global. The RPN library
serializes access to that shared state with a recursive lock:

- `rpn_lock()`
- `rpn_unlock()`

Use this lock when a caller needs a multi-call operation on RPN global state to
be atomic. This is especially important when retaining a pointer returned by
`rpn_getarraypointer()`: the pointer refers to process-global RPN array storage,
so hold `rpn_lock()` while using it if other threads may mutate RPN arrays.

Additional stack helpers are available for callers that need explicit stack
management:

- `rpn_clear_stacks()`
- `rpn_numeric_stack_size()`
- `rpn_long_stack_size()`
- `rpn_string_stack_size()`
- `rpn_logical_stack_size()`

For source compatibility, `dstackptr` and `sstackptr` normally expand to
per-thread accessors. Define `RPN_NO_STACKPTR_COMPAT_MACROS` before including
`rpn.h` only if legacy code requires the historical exported data symbols.

## Math And Utility Libraries

The supporting libraries have been updated to reduce shared static state:

- mdblib, mdbcommon, namelist, matlib, fftpack, mdbmth, CSA, natural-neighbor
  interpolation, meschach, and xlslib use thread-local storage or internal locks
  for formerly shared scratch buffers, flags, error state, and work arrays.
- `mdbmth` random-number helpers that use the C library `rand()`/`srand()` are
  guarded by `mdbmth_lock_rand()` and `mdbmth_unlock_rand()`. The public
  `drand()` and `rdrand()` wrappers use the locked path. The unlocked helpers
  are available for code that already holds the lock or intentionally supplies
  its own synchronization.
- Per-thread random sequences, interpolation settings, meschach error state,
  FFT work buffers, and optimization scratch arrays no longer share mutable
  storage between threads.

These changes make independent calls suitable for worker-thread use. They do
not make concurrent mutation of the same caller-owned matrix, interpolation
object, file stream, or output buffer safe without caller synchronization.

## MPI Note

The SDDS MPI support guards its process-global defaults in the same style as the
serial SDDS library. MPI calls are still subject to the thread support level
provided by the MPI implementation used by the application.
