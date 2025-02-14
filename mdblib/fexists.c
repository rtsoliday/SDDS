/**
 * @file fexists.c
 * @brief Implementation of file existence checking function.
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
 * @brief Checks if a file exists.
 *
 * This function attempts to open the specified file in read mode. If the file is successfully opened,
 * it indicates that the file exists and the function returns 1. Otherwise, it returns 0.
 *
 * @param filename The name of the file to check for existence.
 * @return Returns 1 if the file exists, otherwise returns 0.
 */
long fexists(const char *filename) {
  FILE *fp;

  if ((fp = fopen(filename, FOPEN_READ_MODE))) {
    fclose(fp);
    return (1);
  }
  return (0);
}
