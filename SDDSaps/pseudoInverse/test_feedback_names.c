/**
 * @file test_feedback_names.c
 * @brief Exercise the feedback simulator's dictionary-aware name reader.
 *
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license Distributed under the Software License Agreement in LICENSE.
 */
#include "SDDS.h"
#include <stdio.h>
#include <stdlib.h>

char **get_names(char *filename, char *columnname, int32_t *names);

int main(int argc, char **argv) {
  char **names;
  int32_t count, i;

  if (argc != 3)
    return EXIT_FAILURE;
  SDDS_RegisterProgramName(argv[0]);
  names = get_names(argv[1], argv[2], &count);
  for (i = 0; i < count; i++)
    puts(names[i]);
  SDDS_FreeStringArray(names, count);
  free(names);
  return EXIT_SUCCESS;
}
