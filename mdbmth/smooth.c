/**
 * @file smooth.c
 * @brief Functions for smoothing data and removing spikes from data arrays.
 *
 * This file provides two main functions:
 * - smoothData(): Smooths a data set using a simple moving average over a defined number of points and passes.
 * - despikeData(): Attempts to remove spike values from a data set by comparing each point to its neighbors and replacing it if it exceeds a defined threshold.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, R. Soliday, H. Shang
 */

#include "mdb.h"

/**
 * @brief Smooth a data array using a moving average.
 *
 * This function applies a moving average smoothing operation to an array of data 
 * points a specified number of times. It uses a window defined by smoothPoints, 
 * averaging over this number of points and performing the operation for smoothPasses 
 * iterations.
 *
 * @param data Pointer to the array of data to be smoothed.
 * @param rows The number of data points.
 * @param smoothPoints The number of points to include in the smoothing window.
 * @param smoothPasses The number of smoothing passes to perform.
 */
void smoothData(double *data, long rows, long smoothPoints, long smoothPasses) {
  long lower, upper, row, pass, smoothPoints2, terms;
  double sum;
  static double *smoothedData = NULL;

  smoothedData = trealloc(smoothedData, rows * sizeof(*smoothedData));

  smoothPoints2 = smoothPoints / 2;

  for (pass = 0; pass < smoothPasses; pass++) {
    for (row = sum = 0; row < smoothPoints2; row++)
      sum += data[row];

    terms = row;
    lower = -smoothPoints2;
    upper = smoothPoints2;
    for (row = 0; row < rows; row++, lower++, upper++) {
      if (upper < rows) {
        sum += data[upper];
        terms += 1;
      }
      smoothedData[row] = sum / terms;
      if (lower >= 0) {
        sum -= data[lower];
        terms -= 1;
      }
    }

    for (row = 0; row < rows; row++)
      data[row] = smoothedData[row];
  }
}

/**
 * @brief Remove spikes from a data array by comparing each point to its neighbors.
 *
 * This function identifies and replaces "spikes" in a data array. A spike is defined 
 * as a value that differs significantly from its neighboring values. The function 
 * compares each point to its neighbors over multiple passes and replaces values 
 * exceeding a given threshold with an average of their neighbors. The process can 
 * be halted if too many spikes are found (based on countLimit).
 *
 * @param data Pointer to the data array.
 * @param rows The number of data points.
 * @param neighbors The number of neighboring points to consider around each point.
 * @param passes The maximum number of passes to attempt when removing spikes.
 * @param averageOf The number of points to average when replacing a spiked value.
 * @param threshold The threshold for determining if a value is a spike.
 * @param countLimit The maximum number of spikes allowed before the process stops.
 * @return The number of spikes removed on the final pass.
 */
long despikeData(double *data, long rows, long neighbors, long passes, long averageOf,
                 double threshold, long countLimit) {
  long i0, i1, i2, i, j, i1a, i2a;
  int64_t imin, imax;
  double *deltaSum, sum, *tempdata;
  long despikeCount;

  neighbors = 2 * ((neighbors + 1) / 2);
  if (!(tempdata = (double *)malloc(sizeof(*tempdata) * rows)) ||
      !(deltaSum = (double *)malloc(sizeof(*deltaSum) * (neighbors + 1))))
    bomb("despikeData: memory allocation failure", NULL);
  memcpy(tempdata, data, sizeof(*data) * rows);
  despikeCount = 0;
  while (passes-- > 0) {
    despikeCount = 0;
    for (i0 = 0; i0 < rows; i0 += neighbors / 2) {
      i1 = i0 - neighbors / 2;
      i2 = i0 + neighbors / 2;
      if (i1 < 0)
        i1 = 0;
      if (i2 >= rows)
        i2 = rows - 1;
      if (i2 - i1 == 0)
        continue;
      for (i = i1; i <= i2; i++) {
        deltaSum[i - i1] = 0;
        for (j = i1; j <= i2; j++)
          deltaSum[i - i1] += fabs(tempdata[i] - tempdata[j]);
      }
      if (index_min_max(&imin, &imax, deltaSum, i2 - i1 + 1)) {
        if ((imax += i1) < 0 || imax > rows) {
          fprintf(stderr, "Error: index out of range in despikeData (sddssmooth)\n");
          fprintf(stderr, "  imax = %" PRId64 ", rows=%ld, i1=%ld, i2=%ld, neighbors=%ld\n",
                  imax - 1, rows, i1, i2, neighbors);
          exit(1);
        }
        if (threshold == 0 || threshold * neighbors < deltaSum[imax - i1]) {
          if ((i1a = imax - averageOf / 2) < 0)
            i1a = 0;
          if ((i2a = imax + averageOf / 2) >= rows)
            i2a = rows - 1;
          for (sum = 0, i = i1a; i <= i2a; i++) {
            if (i != imax)
              sum += tempdata[i];
          }
          despikeCount++;
          tempdata[imax] = sum / (i2a - i1a);
        }
      }
    }
    if (!despikeCount || (countLimit > 0 && despikeCount > countLimit))
      break;
  }
  if (countLimit <= 0 || despikeCount < countLimit)
    for (i = 0; i < rows; i++)
      data[i] = tempdata[i];
  else
    despikeCount = 0;
  free(tempdata);
  free(deltaSum);
  return despikeCount;
}
