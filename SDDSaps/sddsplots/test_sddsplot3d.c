/**
 * @file test_sddsplot3d.c
 * @brief Exercise the production 3D array reader without launching a viewer.
 * @copyright Copyright (c) 2026 The University of Chicago, as Operator of Argonne National Laboratory.
 * @license See LICENSE in the distribution.
 */
#include "sddsplot3d.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  SDDS_DATASET dataset;
  double *data[4];
  int32_t elements, i, j, arrays = argc - 2;

  if (argc != 5 && argc != 6)
    return 2;
  SDDS_RegisterProgramName(argv[0]);
  if (!SDDS_InitializeInput(&dataset, argv[1]) || SDDS_ReadPage(&dataset) <= 0 ||
      !read3DPlotArrays(&dataset, argv + 2, arrays, data, &elements)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  printf("%" PRId32 "\n", elements);
  for (i = 0; i < elements; i++) {
    for (j = 0; j < arrays; j++)
      printf("%s%.17g", j ? " " : "", data[j][i]);
    putchar('\n');
  }
  for (j = 0; j < arrays; j++)
    free(data[j]);
  return SDDS_Terminate(&dataset) ? 0 : 1;
}
