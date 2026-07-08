#include "mdb.h"
#include "mdb_thread.h"
#include "time_utils.h"

#if !(defined(_MSC_VER) || defined(LINUX) || defined(SOLARIS) || defined(__APPLE__) || defined(__MACH__) || defined(__unix__) || defined(__CYGWIN__))
static MDB_THREAD_LOCK localtime_lock = MDB_THREAD_LOCK_INITIALIZER;
#endif

int mdb_localtime_copy(const time_t *when, struct tm *result) {
  if (!when || !result)
    return 0;

#if defined(_MSC_VER)
  return localtime_s(result, when) == 0;
#elif defined(LINUX) || defined(SOLARIS) || defined(__APPLE__) || defined(__MACH__) || defined(__unix__) || defined(__CYGWIN__)
  return localtime_r(when, result) != NULL;
#else
  {
    struct tm *temporary;
    int status;

    /* Last resort for platforms without reentrant localtime APIs. */
    mdb_thread_lock(&localtime_lock);
    temporary = localtime(when);
    status = temporary != NULL;
    if (status)
      *result = *temporary;
    mdb_thread_unlock(&localtime_lock);
    return status;
  }
#endif
}
