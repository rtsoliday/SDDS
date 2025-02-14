/**
 * @file tmpname.c
 * @brief Provides functions to generate unique temporary filenames.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C Saunders, R. Soliday
 */

#include "mdb.h"
#include <time.h>
#if defined(_WIN32)
#  include <process.h>
#else
#  include <unistd.h>
#endif

/**
 * @brief Supplies a unique temporary filename.
 *
 * Generates a unique temporary filename by appending the process ID and an incrementing
 * counter to a base name. Ensures that the filename does not already exist.
 *
 * @param s Pointer to a buffer where the temporary filename will be stored. If NULL,
 *          the function allocates memory for the filename.
 * @return A pointer to the unique temporary filename string.
 */
char *tmpname(s) char *s;
{
  static long i = 1;
  static long pid = -1;
  if (s == NULL)
    s = tmalloc((unsigned)(40 * sizeof(*s)));
  if (pid < 0)
#if !defined(vxWorks)
    pid = getpid();
#endif
  do {
#if defined(vxWorks)
    sprintf(s, "tmp.%ld", i);
#else
      sprintf(s, "tmp%ld.%ld", pid, i);
#endif
    i += 1;
    if (!fexists(s))
      break;
  } while (1);
  return (s);
}

#include <errno.h>
#if _LIBC
#  define struct_stat64 struct stat64
#else
#  define struct_stat64 struct stat
#  define __getpid getpid
#  ifdef _WIN32
#    define __lxstat64(version, path, buf) stat(path, buf)
#  else
#    define __lxstat64(version, path, buf) lstat(path, buf)
#  endif
#endif
#ifndef __set_errno
#  define __set_errno(Val) errno = (Val)
#endif

/**
 * @brief Generates a unique temporary filename based on a template.
 *
 * Replaces the last six characters ('XXXXXX') of the template with random characters
 * from a predefined set to create a unique temporary filename. It ensures that the
 * generated filename does not already exist by checking the filesystem.
 *
 * @param template A string containing the template for the temporary filename.
 *                 The last six characters should be 'XXXXXX'.
 * @return A pointer to the modified template with the unique temporary filename.
 *         If no unique name could be generated, the first character of the template is set to null.
 */
char *mktempOAG(char *template) {
  int len, pid;
  char *XXXXXX;
  static uint64_t value;
  uint64_t random_time_bits;
  unsigned int count;
  int save_errno = errno;
  struct_stat64 st;
  static const char letters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

#define ATTEMPTS_MIN (62 * 62 * 62)

#if ATTEMPTS_MIN < TMP_MAX
  unsigned int attempts = TMP_MAX;
#else
  unsigned int attempts = ATTEMPTS_MIN;
#endif

  len = strlen(template);
  if (len < 6 || memcmp(&template[len - 6], "XXXXXX", 6)) {
    template[0] = '\0';
    return (template);
  }

  /* This is where the Xs start.  */
  XXXXXX = &template[len - 6];

  /* Get some more or less random data.  */
  random_time_bits = time(NULL);

  pid = __getpid();
  value += random_time_bits ^ (pid * pid);
  for (count = 0; count < attempts; value += 7777, ++count) {
    uint64_t v = value;

    /* Fill in the random bits.  */
    XXXXXX[0] = letters[v % 62];
    v /= 31;
    XXXXXX[1] = letters[v % 62];
    v /= 31;
    XXXXXX[2] = letters[v % 62];
    v /= 31;
    XXXXXX[3] = letters[v % 62];
    v /= 31;
    XXXXXX[4] = letters[v % 62];
    v /= 31;
    XXXXXX[5] = letters[v % 62];

#if defined(vxWorks)
    if (!fexists(template) < 0) {
      return (template);
    }
#else
    if (__lxstat64(_STAT_VER, template, &st) < 0) {
      if (errno == ENOENT) {
        __set_errno(save_errno);
        return (template);
      } else {
        /* Give up now. */
        template[0] = '\0';
        return (template);
      }
    }
#endif
  }

  /* We got out of the loop because we ran out of combinations to try.  */
  template[0] = '\0';
  return (template);
}
