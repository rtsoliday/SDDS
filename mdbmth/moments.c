/**
 * @file moments.c
 * @brief Computes statistical moments and related measures.
 *
 * This file contains functions to compute various statistical measures such as standard deviation, moments,
 * weighted moments, correlations, averages, RMS values, and more. It also includes threaded versions of these
 * functions for parallel computation using OpenMP.
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
#if defined(linux) || (defined(_WIN32) && !defined(_MINGW))
#  include <omp.h>
#else
void omp_set_num_threads(int a) {}
#endif

/**
 * @brief Calculates the standard deviation of an array of doubles.
 *
 * This function computes the standard deviation of the given array by invoking the threaded version with
 * a single thread.
 *
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns the standard deviation as a double.
 */
double standardDeviation(double *x, long n) {
  return standardDeviationThreaded(x, n, 1);
}

/**
 * @brief Calculates the standard deviation of an array of doubles using multiple threads.
 *
 * This function computes the standard deviation of the given array using the specified number of threads.
 *
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the standard deviation as a double.
 */
double standardDeviationThreaded(double *x, long n, long numThreads) {
  long i;
  double sum = 0, sumSqr = 0, value, mean;
  if (n < 1)
    return (0.0);
  omp_set_num_threads(numThreads);
#pragma omp parallel shared(sum)
  {
    double partial_sum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += x[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1)
        sum += partial_sum;
      else
        sum = partial_sum;
    }
  }
  mean = sum / n;
#pragma omp parallel private(value) shared(sumSqr)
  {
    double partial_sumSqr = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      value = x[i] - mean;
      partial_sumSqr += value * value;
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1)
        sumSqr += partial_sumSqr;
      else
        sumSqr = partial_sumSqr;
    }
  }
  return sqrt(sumSqr / (n - 1));
}

/**
 * @brief Computes the mean, RMS, standard deviation, and mean absolute deviation of an array.
 *
 * This function calculates multiple statistical moments of the provided data array by invoking the threaded
 * version with a single thread.
 *
 * @param mean Pointer to store the computed mean value.
 * @param rms Pointer to store the computed RMS value.
 * @param standDev Pointer to store the computed standard deviation.
 * @param meanAbsoluteDev Pointer to store the computed mean absolute deviation.
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns the number of degrees of freedom (n - 1) on success, 0 on failure.
 */
long computeMoments(double *mean, double *rms, double *standDev,
                    double *meanAbsoluteDev, double *x, long n) {
  return computeMomentsThreaded(mean, rms, standDev, meanAbsoluteDev, x, n, 1);
}

/**
 * @brief Computes the mean, RMS, standard deviation, and mean absolute deviation of an array using multiple threads.
 *
 * This function calculates multiple statistical moments of the provided data array using the specified number of threads.
 *
 * @param mean Pointer to store the computed mean value.
 * @param rms Pointer to store the computed RMS value.
 * @param standDev Pointer to store the computed standard deviation.
 * @param meanAbsoluteDev Pointer to store the computed mean absolute deviation.
 * @param x Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the number of degrees of freedom (n - 1) on success, 0 on failure.
 */
long computeMomentsThreaded(double *mean, double *rms, double *standDev,
                            double *meanAbsoluteDev, double *x, long n, long numThreads) {
  long i;
  double sum = 0, sumSqr = 0, value, sum2 = 0;
  double lMean, lRms, lStDev, lMAD;

  if (!mean)
    mean = &lMean;
  if (!rms)
    rms = &lRms;
  if (!standDev)
    standDev = &lStDev;
  if (!meanAbsoluteDev)
    meanAbsoluteDev = &lMAD;

  *mean = *standDev = *meanAbsoluteDev = DBL_MAX;

  if (n < 1)
    return (0);

  omp_set_num_threads(numThreads);

#pragma omp parallel private(value) shared(sumSqr, sum)
  {
    double partial_sumSqr = 0;
    double partial_sum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += (value = x[i]);
      partial_sumSqr += sqr(value);
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        sum += partial_sum;
        sumSqr += partial_sumSqr;
      } else {
        sum = partial_sum;
        sumSqr = partial_sumSqr;
      }
    }
  }
  *mean = sum / n;
  *rms = sqrt(sumSqr / n);

  sum = 0;
#pragma omp parallel private(value) shared(sum, sum2)
  {
    double partial_sum = 0;
    double partial_sum2 = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      value = x[i] - *mean;
      partial_sum2 += value * value;
      partial_sum += fabs(value);
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        sum2 += partial_sum2;
        sum += partial_sum;
      } else {
        sum2 = partial_sum2;
        sum = partial_sum;
      }
    }
  }
  if (n)
    *standDev = sqrt(sum2 / (n - 1));
  *meanAbsoluteDev = sum / n;

  return (n - 1);
}

/**
 * @brief Computes weighted statistical moments of an array.
 *
 * This function calculates the weighted mean, RMS, standard deviation, and mean absolute deviation of the provided
 * data array by invoking the threaded version with a single thread.
 *
 * @param mean Pointer to store the computed weighted mean value.
 * @param rms Pointer to store the computed weighted RMS value.
 * @param standDev Pointer to store the computed weighted standard deviation.
 * @param meanAbsoluteDev Pointer to store the computed weighted mean absolute deviation.
 * @param x Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @return Returns 1 on success, 0 on failure.
 */
long computeWeightedMoments(double *mean, double *rms, double *standDev,
                            double *meanAbsoluteDev, double *x, double *w, long n) {
  return computeWeightedMomentsThreaded(mean, rms, standDev, meanAbsoluteDev, x, w, n, 1);
}

/**
 * @brief Computes weighted statistical moments of an array using multiple threads.
 *
 * This function calculates the weighted mean, RMS, standard deviation, and mean absolute deviation of the provided
 * data array using the specified number of threads.
 *
 * @param mean Pointer to store the computed weighted mean value.
 * @param rms Pointer to store the computed weighted RMS value.
 * @param standDev Pointer to store the computed weighted standard deviation.
 * @param meanAbsoluteDev Pointer to store the computed weighted mean absolute deviation.
 * @param x Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns 1 on success, 0 on failure.
 */
long computeWeightedMomentsThreaded(double *mean, double *rms, double *standDev,
                                    double *meanAbsoluteDev, double *x, double *w, long n, long numThreads) {
  long i;
  double sumW = 0, sum = 0, sumWx = 0, sumSqrWx = 0, sum2 = 0, value;

  double lMean, lRms, lStDev, lMAD;

  if (!mean)
    mean = &lMean;
  if (!rms)
    rms = &lRms;
  if (!standDev)
    standDev = &lStDev;
  if (!meanAbsoluteDev)
    meanAbsoluteDev = &lMAD;

  *mean = *standDev = *meanAbsoluteDev = DBL_MAX;

  if (n < 1)
    return (0);

  omp_set_num_threads(numThreads);

#pragma omp parallel private(value) shared(sumW, sumWx, sumSqrWx)
  {
    double partial_sumW = 0;
    double partial_sumWx = 0;
    double partial_sumSqrWx = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sumW += w[i];
      partial_sumWx += (value = x[i]) * w[i];
      partial_sumSqrWx += value * value * w[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        sumW += partial_sumW;
        sumWx += partial_sumWx;
        sumSqrWx += partial_sumSqrWx;
      } else {
        sumW = partial_sumW;
        sumWx = partial_sumWx;
        sumSqrWx = partial_sumSqrWx;
      }
    }
  }

  if (sumW) {
    *mean = sumWx / sumW;
    *rms = sqrt(sumSqrWx / sumW);
#pragma omp parallel private(value) shared(sum, sum2)
    {
      double partial_sum = 0;
      double partial_sum2 = 0;
#pragma omp for
      for (i = 0; i < n; i++) {
        value = x[i] - *mean;
        partial_sum += value * w[i];
        partial_sum2 += value * value * w[i];
      }
#pragma omp critical
      {
        /* Found this is necessary to avoid variation of results in cases that should be identical */
        if (numThreads > 1) {
          sum += partial_sum;
          sum2 += partial_sum2;
        } else {
          sum = partial_sum;
          sum2 = partial_sum2;
        }
      }
    }
    if (n)
      /* adjust for n-1 weighting */
      *standDev = sqrt((sum2 * n) / (sumW * (n - 1.0)));
    *meanAbsoluteDev = sum / sumW;
    return (1);
  }
  return (0);
}

long accumulateMoments(double *mean, double *rms, double *standDev,
                       double *x, long n, long reset) {
  return accumulateMomentsThreaded(mean, rms, standDev, x, n, reset, 1);
}

long accumulateMomentsThreaded(double *mean, double *rms, double *standDev,
                               double *x, long n, long reset, long numThreads) {
  long i;
  static double sum = 0, sumSqr = 0, value;
  static long nTotal;

  if (reset)
    nTotal = sum = sumSqr = 0;

  nTotal += n;
  if (nTotal < 1)
    return (0);

  omp_set_num_threads(numThreads);
#pragma omp parallel private(value) shared(sum, sumSqr)
  {
    double partial_sum = 0;
    double partial_sumSqr = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += (value = x[i]);
      partial_sumSqr += sqr(value);
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        sum += partial_sum;
        sumSqr += partial_sumSqr;
      } else {
        sum = partial_sum;
        sumSqr = partial_sumSqr;
      }
    }
  }

  *mean = sum / nTotal;
  *rms = sqrt(sumSqr / nTotal);
  *standDev = sqrt((sumSqr / nTotal - sqr(*mean)) * nTotal / (nTotal - 1.0));

  return (1);
}

long accumulateWeightedMoments(double *mean, double *rms, double *standDev,
                               double *x, double *w, long n, long reset) {
  return accumulateWeightedMomentsThreaded(mean, rms, standDev, x, w, n, reset, 1);
}

long accumulateWeightedMomentsThreaded(double *mean, double *rms, double *standDev,
                                       double *x, double *w, long n, long reset, long numThreads) {
  long i;
  static double sumW = 0, sumWx = 0, sumSqrWx = 0;
  static long nTotal;

  nTotal += n;
  if (nTotal < 1)
    return (0);

  if (reset)
    sumW = sumWx = sumSqrWx = nTotal = 0;

  omp_set_num_threads(numThreads);
#pragma omp parallel shared(sumW, sumWx, sumSqrWx)
  {
    double partial_sumW = 0;
    double partial_sumWx = 0;
    double partial_sumSqrWx = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sumW += w[i];
      partial_sumWx += w[i] * x[i];
      partial_sumSqrWx += x[i] * x[i] * w[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        sumW += partial_sumW;
        sumWx += partial_sumWx;
        sumSqrWx += partial_sumSqrWx;
      } else {
        sumW = partial_sumW;
        sumWx = partial_sumWx;
        sumSqrWx = partial_sumSqrWx;
      }
    }
  }
  if (sumW) {
    *mean = sumWx / sumW;
    *rms = sqrt(sumSqrWx / sumW);
    *standDev = sqrt((sumSqrWx / sumW - sqr(*mean)) * (nTotal / (nTotal - 1.0)));
    return (1);
  }
  return (0);
}

/**
 * @brief Computes the correlations between two datasets.
 *
 * This function calculates the correlation coefficients C11, C12, and C22 between two arrays of doubles by invoking
 * the threaded version with a single thread.
 *
 * @param C11 Pointer to store the computed C11 correlation coefficient.
 * @param C12 Pointer to store the computed C12 correlation coefficient.
 * @param C22 Pointer to store the computed C22 correlation coefficient.
 * @param x Pointer to the first array of doubles.
 * @param y Pointer to the second array of doubles.
 * @param n Number of elements in each array.
 * @return Returns 1 on success, 0 on failure.
 */
long computeCorrelations(double *C11, double *C12, double *C22, double *x, double *y, long n) {
  return computeCorrelationsThreaded(C11, C12, C22, x, y, n, 1);
}

/**
 * @brief Computes the correlations between two datasets using multiple threads.
 *
 * This function calculates the correlation coefficients C11, C12, and C22 between two arrays of doubles using the
 * specified number of threads.
 *
 * @param C11 Pointer to store the computed C11 correlation coefficient.
 * @param C12 Pointer to store the computed C12 correlation coefficient.
 * @param C22 Pointer to store the computed C22 correlation coefficient.
 * @param x Pointer to the first array of doubles.
 * @param y Pointer to the second array of doubles.
 * @param n Number of elements in each array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns 1 on success, 0 on failure.
 */
long computeCorrelationsThreaded(double *C11, double *C12, double *C22, double *x, double *y, long n, long numThreads) {
  long i;
  double xAve = 0, yAve = 0, dx, dy;

  *C11 = *C12 = *C22 = 0;
  if (!n)
    return (0);

  omp_set_num_threads(numThreads);
#pragma omp parallel shared(xAve, yAve)
  {
    double partial_xAve = 0;
    double partial_yAve = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_xAve += x[i];
      partial_yAve += y[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        xAve += partial_xAve;
        yAve += partial_yAve;
      } else {
        xAve = partial_xAve;
        yAve = partial_yAve;
      }
    }
  }
  xAve /= n;
  yAve /= n;

#pragma omp parallel private(dx, dy) shared(C11, C12, C22)
  {
    double partial_C11 = 0;
    double partial_C12 = 0;
    double partial_C22 = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      dx = x[i] - xAve;
      dy = y[i] - yAve;
      partial_C11 += dx * dx;
      partial_C12 += dx * dy;
      partial_C22 += dy * dy;
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        *C11 += partial_C11;
        *C12 += partial_C12;
        *C22 += partial_C22;
      } else {
        *C11 = partial_C11;
        *C12 = partial_C12;
        *C22 = partial_C22;
      }
    }
  }
  *C11 /= n;
  *C12 /= n;
  *C22 /= n;

  return (1);
}

/**
 * @brief Calculates the arithmetic average of an array of doubles.
 *
 * This function computes the arithmetic average of the given array by invoking the threaded version with
 * a single thread.
 *
 * @param y Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns the arithmetic average as a double.
 */
double arithmeticAverage(double *y, long n) {
  return arithmeticAverageThreaded(y, n, 1);
}

/**
 * @brief Calculates the arithmetic average of an array of doubles using multiple threads.
 *
 * This function computes the arithmetic average of the given array using the specified number of threads.
 *
 * @param y Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the arithmetic average as a double.
 */
double arithmeticAverageThreaded(double *y, long n, long numThreads) {
  long i;
  double sum = 0;

  if (!n)
    return (0.0);
  omp_set_num_threads(numThreads);
#pragma omp parallel shared(sum)
  {
    double partial_sum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += y[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1)
        sum += partial_sum;
      else
        sum = partial_sum;
    }
  }
  return (sum / n);
}

/**
 * @brief Calculates the RMS (Root Mean Square) value of an array of doubles.
 *
 * This function computes the RMS value of the given array by invoking the threaded version with
 * a single thread.
 *
 * @param y Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns the RMS value as a double.
 */
double rmsValue(double *y, long n) {
  return rmsValueThreaded(y, n, 1);
}

/**
 * @brief Calculates the RMS (Root Mean Square) value of an array of doubles using multiple threads.
 *
 * This function computes the RMS value of the given array using the specified number of threads.
 *
 * @param y Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the RMS value as a double.
 */
double rmsValueThreaded(double *y, long n, long numThreads) {
  long i;
  double sum = 0;

  if (!n)
    return (0.0);
  omp_set_num_threads(numThreads);
#pragma omp parallel shared(sum)
  {
    double partial_sum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += y[i] * y[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1)
        sum += partial_sum;
      else
        sum = partial_sum;
    }
  }
  return (sqrt(sum / n));
}

/**
 * @brief Calculates the mean absolute deviation of an array of doubles.
 *
 * This function computes the mean absolute deviation of the given array by invoking the threaded version with
 * a single thread.
 *
 * @param y Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @return Returns the mean absolute deviation as a double.
 */
double meanAbsoluteDeviation(double *y, long n) {
  return meanAbsoluteDeviationThreaded(y, n, 1);
}

/**
 * @brief Calculates the mean absolute deviation of an array of doubles using multiple threads.
 *
 * This function computes the mean absolute deviation of the given array using the specified number of threads.
 *
 * @param y Pointer to the array of doubles.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the mean absolute deviation as a double.
 */
double meanAbsoluteDeviationThreaded(double *y, long n, long numThreads) {
  long i;
  double ave = 0, sum = 0;

  if (!n)
    return (0.0);
  omp_set_num_threads(numThreads);
#pragma omp parallel shared(ave)
  {
    double partial_ave = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_ave += y[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1)
        ave += partial_ave;
      else
        ave = partial_ave;
    }
  }
  ave /= n;
#pragma omp parallel shared(sum)
  {
    double partial_sum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += fabs(y[i] - ave);
    }
#pragma omp critical
    {
      if (numThreads > 1)
        sum += partial_sum;
      else
        sum = partial_sum;
    }
  }
  return (sum / n);
}

/**
 * @brief Calculates the weighted average of an array of doubles.
 *
 * This function computes the weighted average of the given array by invoking the threaded version with
 * a single thread.
 *
 * @param y Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @return Returns the weighted average as a double.
 */
double weightedAverage(double *y, double *w, long n) {
  return weightedAverageThreaded(y, w, n, 1);
}

/**
 * @brief Calculates the weighted average of an array of doubles using multiple threads.
 *
 * This function computes the weighted average of the given array using the specified number of threads.
 *
 * @param y Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the weighted average as a double.
 */
double weightedAverageThreaded(double *y, double *w, long n, long numThreads) {
  long i;
  double ySum = 0, wSum = 0;

  if (!n)
    return 0.0;
  omp_set_num_threads(numThreads);
#pragma omp parallel shared(wSum, ySum)
  {
    double partial_wSum = 0;
    double partial_ySum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_wSum += w[i];
      partial_ySum += y[i] * w[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        wSum += partial_wSum;
        ySum += partial_ySum;
      } else {
        wSum = partial_wSum;
        ySum = partial_ySum;
      }
    }
  }
  if (wSum)
    return ySum / wSum;
  return 0.0;
}

/**
 * @brief Calculates the weighted RMS (Root Mean Square) value of an array of doubles.
 *
 * This function computes the weighted RMS value of the given array by invoking the threaded version with
 * a single thread.
 *
 * @param y Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @return Returns the weighted RMS value as a double.
 */
double weightedRMS(double *y, double *w, long n) {
  return weightedRMSThreaded(y, w, n, 1);
}

/**
 * @brief Calculates the weighted RMS (Root Mean Square) value of an array of doubles using multiple threads.
 *
 * This function computes the weighted RMS value of the given array using the specified number of threads.
 *
 * @param y Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the weighted RMS value as a double.
 */
double weightedRMSThreaded(double *y, double *w, long n, long numThreads) {
  long i;
  double sum = 0, wSum = 0;

  if (!n)
    return (0.0);
  omp_set_num_threads(numThreads);
#pragma omp parallel shared(sum, wSum)
  {
    double partial_sum = 0;
    double partial_wSum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += y[i] * y[i] * w[i];
      partial_wSum += w[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        sum += partial_sum;
        wSum += partial_wSum;
      } else {
        sum = partial_sum;
        wSum = partial_wSum;
      }
    }
  }
  if (wSum)
    return sqrt(sum / wSum);
  return 0.0;
}

/**
 * @brief Calculates the weighted mean absolute deviation of an array of doubles.
 *
 * This function computes the weighted mean absolute deviation of the given array by invoking the threaded version with
 * a single thread.
 *
 * @param y Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @return Returns the weighted mean absolute deviation as a double.
 */
double weightedMAD(double *y, double *w, long n) {
  return weightedMADThreaded(y, w, n, 1);
}

/**
 * @brief Calculates the weighted mean absolute deviation of an array of doubles using multiple threads.
 *
 * This function computes the weighted mean absolute deviation of the given array using the specified number of threads.
 *
 * @param y Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the weighted mean absolute deviation as a double.
 */
double weightedMADThreaded(double *y, double *w, long n, long numThreads) {
  long i;
  double mean, sum = 0, wSum = 0;

  if (!n)
    return (0.0);
  omp_set_num_threads(numThreads);
#pragma omp parallel shared(sum, wSum)
  {
    double partial_sum = 0;
    double partial_wSum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += y[i] * w[i];
      partial_wSum += w[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        sum += partial_sum;
        wSum += partial_wSum;
      } else {
        sum = partial_sum;
        wSum = partial_wSum;
      }
    }
  }
  if (!wSum)
    return 0.0;
  mean = sum / wSum;
  sum = 0;
#pragma omp parallel shared(sum)
  {
    double partial_sum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += fabs(y[i] - mean) * w[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1)
        sum += partial_sum;
      else
        sum = partial_sum;
    }
  }
  return sum / wSum;
}

/**
 * @brief Calculates the weighted standard deviation of an array of doubles.
 *
 * This function computes the weighted standard deviation of the given array by invoking the threaded version with
 * a single thread.
 *
 * @param y Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @return Returns the weighted standard deviation as a double.
 */
double weightedStDev(double *y, double *w, long n) {
  return weightedStDevThreaded(y, w, n, 1);
}

/**
 * @brief Calculates the weighted standard deviation of an array of doubles using multiple threads.
 *
 * This function computes the weighted standard deviation of the given array using the specified number of threads.
 *
 * @param y Pointer to the array of doubles.
 * @param w Pointer to the array of weights corresponding to each data point.
 * @param n Number of elements in the array.
 * @param numThreads Number of threads to use for computation.
 * @return Returns the weighted standard deviation as a double.
 */
double weightedStDevThreaded(double *y, double *w, long n, long numThreads) {
  long i;
  double mean, sum = 0, wSum = 0, value;

  if (!n)
    return (0.0);
  omp_set_num_threads(numThreads);
#pragma omp parallel shared(sum, wSum)
  {
    double partial_sum = 0;
    double partial_wSum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      partial_sum += y[i] * w[i];
      partial_wSum += w[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1) {
        sum += partial_sum;
        wSum += partial_wSum;
      } else {
        sum = partial_sum;
        wSum = partial_wSum;
      }
    }
  }
  if (!wSum)
    return 0.0;
  mean = sum / wSum;
  sum = 0;
#pragma omp parallel private(value) shared(sum)
  {
    double partial_sum = 0;
#pragma omp for
    for (i = 0; i < n; i++) {
      value = y[i] - mean;
      partial_sum += value * value * w[i];
    }
#pragma omp critical
    {
      /* Found this is necessary to avoid variation of results in cases that should be identical */
      if (numThreads > 1)
        sum += partial_sum;
      else
        sum = partial_sum;
    }
  }
  return sqrt((sum * n) / (wSum * (n - 1.0)));
}
