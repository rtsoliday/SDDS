/**
 * @file mkdir.c
 * @brief Provides functionality to create directories recursively.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author R. Soliday
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#if defined(_WIN32)
#  include <windows.h>
#  include <direct.h>
#  include <io.h>
#  define mkdir(dirname, mode) _mkdir(dirname)
#  ifdef _MSC_VER
#    define strdup(str) _strdup(str)
#  endif
#endif
#if defined(vxWorks)
#  define mkdir(dirname, mode) mkdir(dirname)
#endif

/**
 * @brief Creates a directory and all necessary parent directories.
 *
 * This function attempts to create the directory specified by `newdir`.
 * It handles the creation of parent directories recursively. It aborts if an ENOENT
 * error is encountered, but ignores other errors such as when the directory
 * already exists.
 *
 * @param newdir The path of the directory to create.
 * @return int Returns 1 if the directory was created successfully, 0 on error.
 */
int makedir(char *newdir) {
  char *buffer = strdup(newdir);
  char *p;
  int len = strlen(buffer);

  if (len <= 0) {
    free(buffer);
    return 0;
  }
  if (buffer[len - 1] == '/') {
    buffer[len - 1] = '\0';
  }
  if (mkdir(buffer, 0755) == 0) {
    free(buffer);
    return 1;
  }

  p = buffer + 1;
  while (1) {
    char hold;

    while (*p && *p != '\\' && *p != '/')
      p++;
    hold = *p;
    *p = 0;
    if ((mkdir(buffer, 0755) == -1) && (errno == ENOENT)) {
      fprintf(stderr, "Couldn't create directory %s\n", buffer);
      free(buffer);
      return 0;
    }
    if (hold == 0)
      break;
    *p++ = hold;
  }
  free(buffer);
  return 1;
}
