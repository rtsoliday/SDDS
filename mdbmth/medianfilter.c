/**
 * @file medianfilter.c
 * @brief Core MEX routine implementing a fast version of the classical 1D running median filter of size W, where W is odd.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M.A. Little, N.S. Jones, H. Shang, R. Soliday
 */
/*this was modified from fastmedfilt1d_core.c wrote by M.A. Little, N.S. Jones 
  following is their documention */
/*
    Core MEX routine implementing a fast version of the classical 1D running
    median filter of size W, where W is odd.

    Usage:
    m = fastmedfilt1d_core(x, xic, xfc, W2)

    Input arguments:
    - x          Input signal
    - xic        Initial boundary condition
    - xfc        End boundary condition
    - W2         Half window size (so that window size is W=2*W2+1)

    Output arguments:
    - m          Median filtered signal

    (c) Max Little, 2010. If you use this code, please cite:
    Little, M.A. and Jones, N.S. (2010),
    "Sparse Bayesian Step-Filtering for High-Throughput Analysis of Molecular
    Machine Dynamics"
    in Proceedings of ICASSP 2010, IEEE Publishers: Dallas, USA.

    This ZIP file contains a fast Matlab implementation of 1D median filtering.
    With the MEX core routine compiled using a decent compiler, compared against
    Matlab's own proprietary toolbox implementation, this algorithm easily
    achieves 10:1 performance gains for large window sizes. Note that, although
    there are more efficient algorithms used in 2D image processing, these are
    restricted to integer-valued data.

    If you use this code for your research, please cite [1].

    References:

    [1] M.A. Little, N.S. Jones (2010), Sparse Bayesian Step-Filtering for High-
    Throughput Analysis of Molecular Machine Dynamics in 2010 IEEE International
    Conference on Acoustics, Speech and Signal Processing, 2010. ICASSP 2010
    Proceedings.: Dallas, TX, USA (in press)

    ZIP file contents:

    fastmedfilt1d.m - The main routine. This calls the MEX core routine described
    below. Ensure this file is placed in the same directory as the MEX files
    below. Typing 'help fastmedfilt1d' gives usage instructions.

    fastmedfilt1d_core.c - Core routine for performing running median filtering,
    written in C with Matlab MEX integration.

    fastmedfilt1d_core.mexw32 - Above code compiled as a Matlab version 7 or
    greater library for direct use with Matlab under Windows 32. Place the
    library in a directory accessible to Matlab and invoke as with any other
    function.
*/

/* H. Shang modified fastmedfilt1d_core.c to be able to call by C routines outside matlab
   environment. April, 2015 
   modifications: depending on the window size, appending repeating values of the first and last value to 
                  make full window size for the boundaries (first and last point).
*/

#include "mdb.h"

#define SWAP(a, b)           \
  {                          \
    register double t = (a); \
    (a) = (b);               \
    (b) = t;                 \
  }

double quickSelect(double *arr, int n) {
  long low, high;
  long median;
  long middle, ll, hh;

  low = 0;
  high = n - 1;
  median = (low + high) / 2;
  while (1) {
    /* One element only */
    if (high <= low)
      return arr[median];

    /* Two elements only */
    if (high == low + 1) {
      if (arr[low] > arr[high])
        SWAP(arr[low], arr[high]);
      return arr[median];
    }

    /* Find median of low, middle and high items; swap to low position */
    middle = (low + high) / 2;
    if (arr[middle] > arr[high])
      SWAP(arr[middle], arr[high]);
    if (arr[low] > arr[high])
      SWAP(arr[low], arr[high]);
    if (arr[middle] > arr[low])
      SWAP(arr[middle], arr[low]);

    /* Swap low item (now in position middle) into position (low+1) */
    SWAP(arr[middle], arr[low + 1]);

    /* Work from each end towards middle, swapping items when stuck */
    ll = low + 1;
    hh = high;
    while (1) {
      do {
        ll++;
      } while (arr[low] > arr[ll]);

      do {
        hh--;
      } while (arr[hh] > arr[low]);

      if (hh < ll)
        break;

      SWAP(arr[ll], arr[hh]);
    }

    /* Swap middle item (in position low) back into correct position */
    SWAP(arr[low], arr[hh]);

    /* Reset active partition */
    if (hh <= median)
      low = ll;
    if (hh >= median)
      high = hh - 1;
  }
}

/**
 * @brief Applies a median filter to an input signal.
 *
 * This function processes the input signal using a sliding window approach to compute the median value for each position.
 * It handles boundary conditions by replicating the edge values to maintain the window size.
 *
 * @param x Input signal array.
 * @param m Output signal buffer array where the median filtered signal will be stored.
 * @param n Size of the input signal.
 * @param W Size of the sliding window (must be an odd number, W = 2*W2 + 1).
 */
void median_filter(double *x, double *m, long n, long W) {
  /* x  -- input signal
     m  -- output signal buffer 
     n  -- size of input signal
     W  -- size of slding window (must be odd number) W = 2*W2 + 1) */
  long i, k, idx, W2;
  double *w;
  if (W % 2 == 0)
    W = W + 1;
  W2 = (W - 1) / 2;

  w = calloc(sizeof(*w), W);
  for (i = 0; i < n; i++) {
    for (k = 0; k < W; k++) {
      idx = i - W2 + k;
      if (idx < 0) {
        w[k] = x[0];
      } else if (idx >= n) {
        w[k] = x[n - 1];
      } else {
        w[k] = x[idx];
      }
    }
    m[i] = quickSelect(w, W);
  }
  free(w);
}
