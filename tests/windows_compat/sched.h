#ifndef SDDS_TEST_WINDOWS_SCHED_H
#define SDDS_TEST_WINDOWS_SCHED_H

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static int sched_yield(void) {
  SwitchToThread();
  return 0;
}

#endif
