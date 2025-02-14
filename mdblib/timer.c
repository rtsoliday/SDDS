/**
 * @file timer.c
 * @brief Provides functions for collecting run-time statistics such as CPU time, memory usage, I/O counts, and elapsed time.
 *
 * @details
 * This file contains implementations of functions like `init_stats()`, `cpu_time()`, `memory_count()`, and others,
 * which collect various run-time statistics. The implementations vary depending on the operating system
 * (VAX/VMS, SUN UNIX, Linux, Solaris, etc.). The functions utilize system-specific libraries and system calls
 * to retrieve the necessary information.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C Saunders, R. Soliday, Y. Wang
 */

#include "mdb.h"

double delapsed_time(void);

/* routine elapsed_time() returns the clock time elapsed since init_stats()
 * was last called, as a character string
 */

char *elapsed_time() {
  static char buffer[20];
  static double dtime;
  int h, m;
  float s;

  dtime = delapsed_time();
  h = dtime / 3600;
  dtime -= h * 3600;
  m = dtime / 60;
  dtime -= m * 60;
  s = dtime;

  sprintf(buffer, "%02d:%02d:%02.3f", h, m, s);
  return (buffer);
}

# if defined(_WIN32)
#  include <time.h>
# else
#  if defined(vxWorks)
#   include <sysLib.h>
#   include <time.h>
#  else
#   if defined(linux)
#    include <time.h>
#   endif
#   if defined(__rtems__)
#    include <unistd.h>
#   endif
#   include <sys/time.h>
#  endif
# endif

# if defined(linux)
static struct timespec delapsedStart;
static short delapsedStartInitialized = 0;
/**
 * @brief Initializes the run-time statistics collection.
 *
 * This function initializes the timer by calling system-specific initialization routines.
 * It should be called before collecting any run-time statistics.
 */
void init_stats() {
  clock_gettime(CLOCK_REALTIME, &delapsedStart);
  delapsedStartInitialized = 1;
}
# else
static double delapsed_start = 0;
void init_stats() {
  delapsed_start = delapsed_time();
  clock();
}
# endif

/**
 * @brief Calculates the elapsed clock time since the last initialization as a numerical value.
 *
 * This function calculates the elapsed time in seconds since `init_stats()` was last called.
 *
 * @return The elapsed time in seconds, or 0.0 if not available.
 */
double delapsed_time() {
# if defined(linux)
  struct timespec delapsedNow;
  if (!delapsedStartInitialized)
    init_stats();
  clock_gettime(CLOCK_REALTIME, &delapsedNow);
  return (double)(delapsedNow.tv_sec - delapsedStart.tv_sec) +
    (delapsedNow.tv_nsec - delapsedStart.tv_nsec)/1e9;
# else
  static long delapsed_start = -1;
  if (delapsed_start == -1) {
    delapsed_start = time(0);
    return 0.0;
  } else
    return time(0) - delapsed_start;
# endif
}

/**
 * @brief Retrieves the CPU time used since the last initialization.
 *
 * This function retrieves the CPU time consumed by the process since `init_stats()` was last called.
 *
 * @return The CPU time used in hundredths of seconds, or -1 on error.
 */
long cpu_time() {
  return (long)((clock() * 100.0) / CLOCKS_PER_SEC);
}

/* routine bio_count() returns the buffered io count since init_stats() was
 * last called
 */

long bio_count() {
  return 0;
}

/* routine dio_count() returns the direct io count since init_stats() was
 * last called
 */

long dio_count() {
  return (0);
}

/**
 * @brief Retrieves the number of page faults since the last initialization.
 *
 * This function retrieves the number of page faults that have occurred since `init_stats()` was last called.
 *
 * @return The number of page faults, or -1 on error.
 */
long page_faults() {
  return 0;
}

# if defined(linux) && !defined(__powerpc__)
#  include <stdio.h>
#  include <sys/types.h>
#  include <unistd.h>

/**
 * @brief Retrieves the memory usage count since the last initialization.
 *
 * This function retrieves the memory usage information in kilobytes since `init_stats()` was last called.
 *
 * @return The memory usage in kilobytes, or 0 on error.
 */
long memory_count()
{
  FILE *fp;
  static short first = 1;
  static char proc[100];
  long size1;

  if (first) {
    sprintf(proc, "/proc/%ld/statm", (long)getpid());
    first = 0;
  }
  if (!(fp = fopen(proc, "r"))) {
    perror("fopen failed in memory_count()");
    exit(1);
  }
  if (fscanf(fp, "%ld", &size1) != 1) {
    perror("fscanf failed in memory_count()");
    exit(1);
  }
  fclose(fp);
  /* Assume a page is 1 kB */
  return size1;
}
# else
long memory_count() {
  return 0;
}
# endif

