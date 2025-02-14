/**
 * @file makeHistogram.c
 * @brief Compiles histograms from data points.
 *
 * This file contains functions to compile histograms from data points. It provides both standard and weighted histogram functions,
 * as well as a function to compute the mode of a dataset.
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
 * @brief Compiles a histogram from data points.
 *
 * @param hist Pointer to the histogram array to be filled.
 * @param n_bins Number of bins in the histogram.
 * @param lo Lower bound of the histogram range.
 * @param hi Upper bound of the histogram range.
 * @param data Pointer to the data array.
 * @param n_pts Number of data points.
 * @param new_start Flag indicating whether to initialize the histogram (1 to initialize, 0 to accumulate).
 * @return Returns the total number of points binned.
 */
long make_histogram(
  double *hist, long n_bins, double lo, double hi, double *data,
  int64_t n_pts, long new_start) {
  static long bin;
  static int64_t i;
  static double bin_size, dbin;

  if (new_start) {
    bin_size = (hi - lo) / n_bins;
    for (i = 0; i < n_bins; i++)
      hist[i] = 0;
  }

  for (i = 0; i < n_pts; i++) {
    bin = (dbin = (data[i] - lo) / bin_size);
    if (dbin < 0)
      continue;
    if (bin < 0 || bin >= n_bins)
      continue;
    hist[bin] += 1;
  }

  for (i = bin = 0; i < n_bins; i++)
    bin += hist[i];

  return (bin);
}

/**
 * @brief Compiles a weighted histogram from data points.
 *
 * @param hist Pointer to the histogram array to be filled.
 * @param n_bins Number of bins in the histogram.
 * @param lo Lower bound of the histogram range.
 * @param hi Upper bound of the histogram range.
 * @param data Pointer to the data array.
 * @param n_pts Number of data points.
 * @param new_start Flag indicating whether to initialize the histogram (1 to initialize, 0 to accumulate).
 * @param weight Pointer to the weights array corresponding to each data point.
 * @return Returns the total number of points binned.
 */
long make_histogram_weighted(
  double *hist, long n_bins, double lo, double hi, double *data,
  long n_pts, long new_start, double *weight) {
  static long bin, i, count;
  static double bin_size, dbin;

  if (new_start) {
    count = 0;
    bin_size = (hi - lo) / n_bins;
    for (i = 0; i < n_bins; i++)
      hist[i] = 0;
  }

  for (i = 0; i < n_pts; i++) {
    bin = (dbin = (data[i] - lo) / bin_size);
    if (dbin < 0)
      continue;
    if (bin < 0 || bin >= n_bins)
      continue;
    hist[bin] += weight[i];
    count++;
  }

  return (count);
}

/**
 * @brief Computes the mode of a dataset using histogram binning.
 *
 * @param result Pointer to store the computed mode value.
 * @param data Pointer to the data array.
 * @param pts Number of data points.
 * @param binSize Size of each histogram bin. If greater than 0, determines the bin size; otherwise, the number of bins is used.
 * @param bins Number of bins in the histogram.
 * @return 
 *         @return 1 on success,
 *         @return -1 if invalid binSize and bins parameters,
 *         @return -2 if pts <= 0,
 *         @return -3 if data is NULL,
 *         @return -4 if result is NULL.
 */
long computeMode(double *result, double *data, long pts, double binSize, long bins) {
  double min, max;
  int64_t imin, imax;
  double *histogram;

  if ((binSize <= 0 && bins <= 2) || (binSize > 0 && bins > 2))
    return -1;
  if (pts <= 0)
    return -2;
  if (!data)
    return -3;
  if (!result)
    return -4;
  if (pts == 1) {
    *result = data[0];
    return 1;
  }
  find_min_max(&min, &max, data, pts);
  /* add buffer bins and compute bin size or number of bins */
  if (binSize > 0) {
    max += binSize;
    min -= binSize;
    bins = (max - min) / binSize + 0.5;
  } else {
    binSize = (max - min) / bins;
    max += binSize;
    min -= binSize;
    bins += 2;
    binSize = (max - min) / bins;
  }
  if (!(histogram = malloc(sizeof(*histogram) * bins)))
    bomb("memory allocation failure (computeMode)", NULL);
  make_histogram(histogram, bins, min, max, data, pts, 1);
  index_min_max(&imin, &imax, histogram, bins);
  free(histogram);
  *result = (imax + 0.5) * binSize + min;
  return 1;
}
