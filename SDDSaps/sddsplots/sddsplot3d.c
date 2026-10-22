/**
 * @file sddsplot3d.c
 * @brief Extract numeric plot arrays without reinterpreting primitive storage.
 * @copyright Copyright (c) 2026 The University of Chicago, as Operator of Argonne National Laboratory.
 * @license See LICENSE in the distribution.
 */
#include "sddsplot3d.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Read three coordinates and an optional intensity array from the current page.
 * @param data Receives owned double buffers; all entries are NULL on failure.
 * @param elements Receives the common element count, including zero for empty arrays.
 * @return One on success, zero with an SDDS error on invalid types or unequal lengths.
 */
int32_t read3DPlotArrays(SDDS_DATASET *dataset, char **names, int32_t arrays,
                         double **data, int32_t *elements) {
  int32_t i, index, count;
  char message[1024];

  *elements = 0;
  for (i = 0; i < arrays; i++)
    data[i] = NULL;
  if (arrays != 3 && arrays != 4) {
    SDDS_SetError("A 3D plot requires three or four arrays");
    return 0;
  }
  /* Validate every selected field before allocating any buffers. */
  for (i = 0; i < arrays; i++) {
    index = SDDS_GetArrayIndex(dataset, names[i]);
    if (index < 0 || !SDDS_NUMERIC_TYPE(dataset->layout.array_definition[index].type)) {
      snprintf(message, sizeof(message), "array %.800s must have numeric type for -3d (use sddscast for explicit conversion)", names[i]);
      SDDS_SetError(message);
      return 0;
    }
    count = dataset->array[index].elements;
    if (i && count != *elements) {
      snprintf(message, sizeof(message), "array %.800s has a different element count for -3d", names[i]);
      SDDS_SetError(message);
      return 0;
    }
    *elements = count;
  }
  for (i = 0; i < arrays && *elements; i++) {
    data[i] = SDDS_GetArrayInDoubles(dataset, names[i], &count);
    if (!data[i]) {
      int32_t j;
      for (j = 0; j < i; j++) {
        free(data[j]);
        data[j] = NULL;
      }
      return 0;
    }
  }
  return 1;
}
