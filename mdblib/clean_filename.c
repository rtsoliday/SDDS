/**
 * @file clean_filename.c
 * @brief Provides functions to manipulate and clean filename strings.
 *
 * This file contains functions to create, manage, and manipulate buffers that store
 * lines of text strings. Buffers can be dynamically created, added to, cleared, and
 * printed to files.
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
#include <ctype.h>

/**
 * @brief Removes path and version specifications from a filename string.
 *
 * This function eliminates any path components enclosed in ']' and 
 * truncates the filename at the first occurrence of ';' to remove version information.
 *
 * @param filename The filename string to be cleaned.
 * @return A pointer to the cleaned filename.
 */
char *clean_filename(char *filename) {
  register char *ptr;

  if ((ptr = strchr(filename, ']')))
    strcpy_ss(filename, ptr + 1);
  if ((ptr = strchr(filename, ';')))
    *ptr = 0;
  return (filename);
}
