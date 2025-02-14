/**
 * @file str_copy.c
 * @brief Implementation of the strcpy_ss function for safe string copying.
 * 
 * This file contains the implementation of the strcpy_ss function, which safely
 * copies a string from the source to the destination buffer, ensuring that overlapping
 * memory regions are handled correctly.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author Y. Wang, M. Borland, R. Soliday
 */

#include <string.h>
#include "mdb.h"

/**
 * @brief Safely copies a string, handling memory overlap.
 *
 * Copies the string pointed to by `src`, including the terminating null byte,
 * to the buffer pointed to by `dest`. This function ensures that the copy is
 * performed safely even if the source and destination buffers overlap.
 *
 * @param dest Pointer to the destination buffer where the string will be copied.
 * @param src Pointer to the null-terminated source string to copy.
 * @return Pointer to the destination string `dest`.
 */
char *strcpy_ss(char *dest, const char *src) {
  return (char *)memmove(dest, src, strlen(src) + 1);
}
