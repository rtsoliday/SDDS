/**
 * @file topbase.c
 * @brief Provides routines to determine the top-level and base-level of data.
 *
 * This file contains functions to find the top-level and base-level of a dataset, as well as to identify
 * crossing points within the data. The routines utilize histogram binning and statistical analysis
 * to compute these levels.
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

#define DEFAULT_BINFACTOR 0.05
#define DEFAULT_SIGMAS 2

/**
 * @brief Finds the top-level and base-level of a dataset.
 *
 * @param top Pointer to store the computed top-level value.
 * @param base Pointer to store the computed base-level value.
 * @param data Pointer to the array of data points.
 * @param points Number of data points in the array.
 * @param bins Number of bins to use for histogram computation. If <= 0, a default bin factor is applied.
 * @param sigmasRequired Number of standard deviations required for determining significant histogram peaks.
 * @return Returns 1 on success, 0 on failure.
 */
long findTopBaseLevels(double *top, double *base, double *data, int64_t points,
                       long bins, double sigmasRequired) {
  long binned, iMaximum, i;
  static long maxBins = 0;
  static double *histogram = NULL;
  double min, max, midpoint, maxHistogram, meanBinned;
  double lower, upper, delta;

  if (bins <= 0)
    bins = DEFAULT_BINFACTOR * points;
  if (sigmasRequired <= 0)
    sigmasRequired = DEFAULT_SIGMAS;
  if (points < 2)
    return 0;

  if (bins > maxBins)
    histogram = tmalloc(sizeof(*histogram) * (maxBins = bins));

  if (!find_min_max(&min, &max, data, points))
    return 0;

  *base = min;
  *top = max;
  midpoint = (min + max) / 2;
  if (points < 10)
    return 1;

  delta = (midpoint - min) / (bins - 1);
  lower = min - delta / 2;
  upper = midpoint + delta / 2;
  if ((binned = make_histogram(histogram, bins, lower, upper, data, points, 1))) {
    maxHistogram = 0;
    iMaximum = -1;
    for (i = 0; i < bins; i++) {
      if (maxHistogram < histogram[i]) {
        iMaximum = i;
        maxHistogram = histogram[i];
      }
    }
    meanBinned = ((double)binned) / bins;
    if ((maxHistogram > 1) && (maxHistogram > (meanBinned + sigmasRequired * sqrt(meanBinned))))
      *base = lower + (iMaximum + 0.5) * ((upper - lower) / bins);
  }

  delta = (max - midpoint) / (bins - 1);
  lower = midpoint - delta / 2;
  upper = max + delta / 2;
  if ((binned = make_histogram(histogram, bins, lower, upper, data, points, 1))) {
    maxHistogram = 0;
    iMaximum = -1;
    for (i = 0; i < bins; i++) {
      if (maxHistogram < histogram[i]) {
        iMaximum = i;
        maxHistogram = histogram[i];
      }
    }
    meanBinned = ((double)binned) / bins;
    if ((maxHistogram > 1) && (maxHistogram > (meanBinned + sigmasRequired * sqrt(meanBinned))))
      *top = lower + (iMaximum + 0.5) * ((upper - lower) / bins);
  }

  if (*top == *base) {
    *base = min;
    *top = max;
  }
  return 1;
}

/**
 * @brief Finds the crossing point in the data where the data crosses a specified level.
 *
 * @param start The starting index to search for the crossing point.
 * @param data Pointer to the array of data points.
 * @param points Number of data points in the array.
 * @param level The level at which to find the crossing point.
 * @param direction The direction of crossing (positive for upward, negative for downward).
 * @param indepData Pointer to the independent data array corresponding to the data points. Can be NULL.
 * @param location Pointer to store the interpolated location of the crossing point. Can be NULL.
 * @return 
 *         @return The index of the crossing point on success,
 *         @return -1 if no crossing is found or if input parameters are invalid.
 */
int64_t findCrossingPoint(int64_t start, double *data, int64_t points, double level, long direction,
                          double *indepData, double *location) {
  double diff;
  int64_t i;
  long transitionPossible;

  if (start < 0 || start > (points - 1))
    return -1;
  transitionPossible = 0;
  for (i = start; i < points; i++) {
    diff = direction * (data[i] - level);
    if (diff <= 0)
      transitionPossible = 1;
    if (diff > 0 && transitionPossible)
      break;
  }
  if (i == points)
    return -1;
  if (!indepData || !location)
    return i;
  if (i == 0 || data[i] == data[i - 1]) {
    *location = indepData[i];
    return i;
  }
  *location = indepData[i - 1] + (indepData[i] - indepData[i - 1]) / (data[i] - data[i - 1]) * (level - data[i - 1]);
  return i;
}
