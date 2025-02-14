/**
 * @file replacefile.c
 * @brief Provides functions to replace files with options for backup and robust renaming.
 *
 * This file contains the implementation of the replaceFile(), replaceFileAndBackUp(),
 * and renameRobust() functions, which handle file replacement with error checking and
 * backup capabilities.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"

long replaceFile(char *file, char *replacement) {
  if (renameRobust(file, replacement, RENAME_OVERWRITE)) {
    fprintf(stderr, "unable to rename file %s to %s\n",
            replacement, file);
    perror(NULL);
    return 0;
  }
  return 1;
}

long renameRobust(char *oldName, char *newName, unsigned long flags)
/* returns 0 on success, 1 on failure, just like ANSI rename() */
{
  char buffer[1024];
  /*
  if (fexists(newName) && flags&RENAME_OVERWRITE) {
    remove(newName);
  }
  if (fexists(newName))
    return 1; 
*/
  if (fexists(newName) && !(flags & RENAME_OVERWRITE))
    return 1;

  /* try using the system-provided version first */
  if (rename(oldName, newName) == 0)
    return 0;
    /* do a copy-and-delete operation */
#if defined(_WIN32)
  sprintf(buffer, "copy %s %s", oldName, newName);
#else
  sprintf(buffer, "cp %s %s", oldName, newName);
#endif
  system(buffer);
  if (!fexists(newName)) {
    fprintf(stderr, "unable to copy %s to %s\n", oldName, newName);
    return 1;
  }
  remove(oldName); /* ignore return value */
  return 0;
}

/**
 * @brief Replaces a file with a replacement file and creates a backup of the original.
 *
 * Creates a backup of the original file by renaming it with a "~" suffix, then replaces it
 * with the replacement file. If the replacement fails, the function attempts to restore the
 * original file from the backup. Error messages are printed to stderr in case of failures.
 *
 * @param file The name of the file to be replaced.
 * @param replacement The name of the replacement file.
 * @return Returns 1 on success, 0 on failure.
 */
long replaceFileAndBackUp(char *file, char *replacement) {
  char *backup;
  backup = tmalloc(sizeof(*backup) * (strlen(file) + 2));
  sprintf(backup, "%s~", file);
  if (renameRobust(file, backup, RENAME_OVERWRITE) == 0) {
    if (renameRobust(replacement, file, RENAME_OVERWRITE)) {
      fprintf(stderr, "unable to rename temporary file %s to %s\n",
              replacement, file);
      perror(NULL);
      if (renameRobust(backup, file, 0)) {
        fprintf(stderr, "unable to rename %s back to %s !\n", backup, file);
        perror(NULL);
      } else
        fprintf(stderr, "original version of %s restored\n", file);
      free(backup);
      return 0;
    }
  } else {
    fprintf(stderr, "unable to replace %s--result stored in %s\n",
            file, replacement);
    perror(NULL);
    free(backup);
    return 0;
  }
  free(backup);
  return 1;
}
