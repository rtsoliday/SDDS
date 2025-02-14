/**
 * @file linfit.c
 * @brief Provides routines for performing simple linear fits.
 *
 * This file contains functions to perform unweighted linear regression on datasets.
 * It includes functions to compute the slope, intercept, and variance of the best-fit line.
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

/**
 * @brief Performs an unweighted linear fit on the provided data.
 *
 * This function computes the slope and intercept of the best-fit line for the given x and y data
 * by invoking the select-based linear fit function without selecting specific data points.
 *
 * @param xData Pointer to the array of x-values.
 * @param yData Pointer to the array of y-values.
 * @param nData Number of data points in the arrays.
 * @param slope Pointer to store the computed slope of the best-fit line.
 * @param intercept Pointer to store the computed intercept of the best-fit line.
 * @param variance Pointer to store the computed variance of the fit.
 * @return 
 *         @return 1 on successful fit,
 *         @return 0 if the fit could not be performed (e.g., insufficient data).
 */
long unweightedLinearFit(double *xData, double *yData, long nData,
                         double *slope, double *intercept, double *variance) {
  return unweightedLinearFitSelect(xData, yData, NULL, nData, slope, intercept, variance);
}

/**
 * @brief Performs an unweighted linear fit on the provided data with optional data point selection.
 *
 * This function computes the slope and intercept of the best-fit line for the given x and y data,
 * optionally considering only selected data points as indicated by the select array.
 * It also calculates the variance of the residuals from the fit.
 *
 * @param xData Pointer to the array of x-values.
 * @param yData Pointer to the array of y-values.
 * @param select Pointer to an array of selection flags (non-zero to include the data point). Can be NULL to include all points.
 * @param nData Number of data points in the arrays.
 * @param slope Pointer to store the computed slope of the best-fit line.
 * @param intercept Pointer to store the computed intercept of the best-fit line.
 * @param variance Pointer to store the computed variance of the fit.
 * @return 
 *         @return 1 on successful fit,
 *         @return 0 if the fit could not be performed (e.g., insufficient data or no variation in x-data).
 */
long unweightedLinearFitSelect(double *xData, double *yData, short *select, long nData,
                               double *slope, double *intercept, double *variance) {
  long i;
  double x, y, sum_x2, sum_x, sum_xy, sum_y, D, residual1, nUsed;

  if (nData < 2)
    return 0;

  /* compute linear fit and associated parameters */
  /* linear fit to y = a + bx:
     a = (S x^2 Sy - S x S xy)/D
     b = (N S xy  - Sx Sy)/D
     D = N S x^2 - (S x)^2
     */
  sum_x = sum_x2 = sum_y = sum_xy = 0;
  nUsed = 0;
  for (i = 0; i < nData; i++) {
    if (!select || select[i]) {
      nUsed++;
      sum_x += (x = xData[i]);
      sum_x2 += x * x;
      sum_y += (y = yData[i]);
      sum_xy += x * y;
    }
  }
  if (nUsed < 2)
    return 0;

  if ((D = nUsed * sum_x2 - sum_x * sum_x)) {
    *slope = (nUsed * sum_xy - sum_x * sum_y) / D;
    *intercept = (sum_x2 * sum_y - sum_x * sum_xy) / D;
    *variance = 0;
    for (i = 0; i < nData; i++) {
      if (!select || select[i]) {
        residual1 = (yData[i] - (xData[i] * (*slope) + (*intercept)));
        *variance += residual1 * residual1;
      }
    }
    if (nUsed > 2)
      *variance /= (nUsed - 2);
    else
      *variance = DBL_MAX;
    return 1;
  } else
    return 0;
}
