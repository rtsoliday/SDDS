/**
 * @file substituteTagValue.c
 * @brief Handles macro substitution within input strings.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author H. Shang, M. Borland, R. Soliday
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include <ctype.h>

/**
 * @brief Replaces macro tags in the input string with their corresponding values.
 *
 * This function iterates through each macro tag and replaces occurrences of the tag in the input string
 * with its corresponding value. It manages memory allocation for temporary buffers and ensures that
 * substitutions are correctly applied for different tag formats.
 *
 * @param input      The input string where macro substitution is to be performed.
 * @param buflen     The length of the input buffer.
 * @param macroTag   An array of macro tag strings to be replaced.
 * @param macroValue An array of values corresponding to each macro tag.
 * @param macros     The number of macros to process.
 */
void substituteTagValue(char *input, long buflen,
                        char **macroTag, char **macroValue, long macros) {
  char *buffer;
  long i;
  char *version1 = NULL, *version2 = NULL;
  if (!(buffer = malloc(sizeof(*buffer) * buflen)))
    bomb("memory allocation failure doing macro substitution", NULL);
  for (i = 0; i < macros; i++) {
    if (i == 0) {
      if (!(version1 = malloc(sizeof(*version1) * (strlen(macroTag[i]) + 10))) ||
          !(version2 = malloc(sizeof(*version2) * (strlen(macroTag[i]) + 10))))
        bomb("memory allocation failure doing macro substitution", NULL);
    } else {
      if (!(version1 = realloc(version1, sizeof(*version1) * (strlen(macroTag[i]) + 10))) ||
          !(version2 = realloc(version2, sizeof(*version2) * (strlen(macroTag[i]) + 10))))
        bomb("memory allocation failure doing macro substitution", NULL);
    }
    sprintf(version1, "<%s>", macroTag[i]);
    sprintf(version2, "$%s", macroTag[i]);
    if (replace_string(buffer, input, version1, macroValue[i]))
      strcpy_ss(input, buffer);
    if (replace_string(buffer, input, version2, macroValue[i]))
      strcpy_ss(input, buffer);
  }
  free(buffer);
  if (version1)
    free(version1);
  if (version2)
    free(version2);
}
