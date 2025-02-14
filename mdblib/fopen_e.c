/**
 * @file fopen_e.c
 * @brief Provides the fopen_e function for opening files with error checking and handling.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author C. Saunders, R. Soliday
 */

#include "mdb.h"

/**
 * @brief Opens a file with error checking, messages, and aborts.
 *
 * This function attempts to open a file with the specified mode. If the file exists and the mode includes
 * `FOPEN_SAVE_IF_EXISTS`, it renames the existing file by appending a tilde (`~`). If opening the file fails,
 * it either returns `NULL` or exits the program based on the mode flags.
 *
 * @param file The path to the file to open.
 * @param open_mode The mode string for `fopen` (e.g., "r", "w").
 * @param mode Flags controlling behavior (e.g., `FOPEN_SAVE_IF_EXISTS`, `FOPEN_RETURN_ON_ERROR`).
 * @return FILE* Pointer to the opened file, or `NULL` if `FOPEN_RETURN_ON_ERROR` is set and the file could not be opened.
 */
FILE *fopen_e(char *file, char *open_mode, long mode) {
  FILE *fp;
  static char buffer[1024];

  if ((mode & FOPEN_SAVE_IF_EXISTS) && fexists(file)) {
    sprintf(buffer, "%s~", file);
    if (rename(file, buffer) != 0) {
      fprintf(stderr, "error: cannot save previous version of %s--new file not opened.\n", file);
      if (mode & FOPEN_RETURN_ON_ERROR)
        return (NULL);
      exit(1);
    }
  }

  if ((fp = fopen(file, open_mode))) {
    if (mode & FOPEN_INFORM_OF_OPEN)
      printf("%s opened in mode %s\n", file, open_mode);
    return (fp);
  }

  sprintf(buffer, "unable to open %s in mode %s", file, open_mode);
  perror(buffer);

  if (!(mode & FOPEN_RETURN_ON_ERROR))
    exit(1);

  return (NULL);
}
