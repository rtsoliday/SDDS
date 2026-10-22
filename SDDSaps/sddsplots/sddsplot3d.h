/**
 * @file sddsplot3d.h
 * @brief Checked array extraction for three-dimensional SDDS plots.
 * @copyright Copyright (c) 2026 The University of Chicago, as Operator of Argonne National Laboratory.
 * @license See LICENSE in the distribution.
 */
#ifndef SDDSPLOT3D_H
#define SDDSPLOT3D_H

#include "SDDS.h"

int32_t read3DPlotArrays(SDDS_DATASET *dataset, char **names, int32_t arrays,
                         double **data, int32_t *elements);

#endif
