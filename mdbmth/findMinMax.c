/**
 * @file findMinMax.c
 * @brief Provides functions to find minimum and maximum values in arrays.
 *
 * This file contains functions to find the minimum and maximum values in one-dimensional and two-dimensional arrays,
 * as well as functions to find the indices of these values and perform assignments based on comparisons.
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
 * @brief Finds the minimum and maximum values in a list of doubles.
 *
 * This function iterates through the provided list of doubles to determine the minimum and maximum values.
 * The results are stored in the provided pointers if they are not NULL.
 *
 * @param min Pointer to store the minimum value found. Can be NULL if not needed.
 * @param max Pointer to store the maximum value found. Can be NULL if not needed.
 * @param list Pointer to the array of doubles to search.
 * @param n Number of elements in the list.
 * @return Returns 1 on success, 0 on failure (e.g., if n <= 0 or list is NULL).
 */
int find_min_max(
  double *min, double *max, double *list,
  int64_t n) {
  register int64_t i;
  register double lo, hi, val;

  if (!n || !list)
    return (0);
  if (!min && !max)
    return 0;

  lo = DBL_MAX;
  hi = -DBL_MAX;
  for (i = 0; i < n; i++) {
    if ((val = list[i]) < lo)
      lo = val;
    if (val > hi)
      hi = val;
  }
  if (min)
    *min = lo;
  if (max)
    *max = hi;
  return (1);
}

/**
 * @brief Updates the minimum and maximum values based on a list of doubles.
 *
 * This function iterates through the provided list of doubles to update the minimum and maximum values.
 * If the 'reset' flag is non-zero, the function resets the current min and max before processing the list.
 *
 * @param min Pointer to the current minimum value. If 'reset' is non-zero, this will be set to the new minimum.
 * @param max Pointer to the current maximum value. If 'reset' is non-zero, this will be set to the new maximum.
 * @param list Pointer to the array of doubles to process.
 * @param n Number of elements in the list.
 * @param reset Flag indicating whether to reset the current min and max before updating. Non-zero to reset.
 * @return Returns 1 on success, 0 on failure (e.g., if n <= 0 or list is NULL).
 */
int update_min_max(
  double *min, double *max, double *list,
  int64_t n, int32_t reset) {
  register int64_t i;
  register double lo, hi, val;

  if (!n || !list)
    return (0);
  if (!min && !max)
    return 0;

  if (reset) {
    lo = DBL_MAX;
    hi = -DBL_MAX;
  } else {
    lo = *min;
    hi = *max;
  }

  for (i = 0; i < n; i++) {
    if ((val = list[i]) < lo)
      lo = val;
    if (val > hi)
      hi = val;
  }
  if (min)
    *min = lo;
  if (max)
    *max = hi;
  return (1);
}

/**
 * @brief Finds the indices of the minimum and maximum values in a list of doubles.
 *
 * This function iterates through the provided list of doubles to determine the indices of the minimum and maximum values.
 * The indices are stored in the provided pointers if they are not NULL.
 *
 * @param imin Pointer to store the index of the minimum value. Can be NULL if not needed.
 * @param imax Pointer to store the index of the maximum value. Can be NULL if not needed.
 * @param list Pointer to the array of doubles to search.
 * @param n Number of elements in the list.
 * @return Returns 1 on success, 0 on failure (e.g., if n <= 0 or list is NULL).
 */
int index_min_max(
  int64_t *imin, int64_t *imax, double *list, int64_t n) {
  register int64_t i;
  register double lo, hi, val;
  int64_t iMin, iMax;

  if (!n || !list)
    return (0);
  if (!imin && !imax)
    return 0;

  lo = DBL_MAX;
  hi = -DBL_MAX;
  iMin = iMax = 0;
  for (i = 0; i < n; i++) {
    if ((val = list[i]) < lo) {
      iMin = i;
      lo = val;
    }
    if (val > hi) {
      hi = val;
      iMax = i;
    }
  }
  if (imin)
    *imin = iMin;
  if (imax)
    *imax = iMax;

  return (1);
}

/**
 * @brief Finds the indices of the minimum and maximum values in a list of longs.
 *
 * This function iterates through the provided list of longs to determine the indices of the minimum and maximum values.
 * The indices are stored in the provided pointers if they are not NULL.
 *
 * @param imin Pointer to store the index of the minimum value. Can be NULL if not needed.
 * @param imax Pointer to store the index of the maximum value. Can be NULL if not needed.
 * @param list Pointer to the array of longs to search.
 * @param n Number of elements in the list.
 * @return Returns 1 on success, 0 on failure (e.g., if n <= 0 or list is NULL).
 */
int index_min_max_long(
  int64_t *imin, int64_t *imax, long *list, int64_t n) {
  register int64_t i;
  register long lo, hi, val;
  int64_t iMin, iMax;

  if (!n || !list)
    return (0);
  if (!imin && !imax)
    return 0;

  lo = LONG_MAX;
  hi = -LONG_MAX;
  iMin = iMax = 0;
  for (i = 0; i < n; i++) {
    if ((val = list[i]) < lo) {
      iMin = i;
      lo = val;
    }
    if (val > hi) {
      hi = val;
      iMax = i;
    }
  }
  if (imin)
    *imin = iMin;
  if (imax)
    *imax = iMax;

  return (1);
}

/* routine: assign_min_max()
 * purpose: compare a value to running minimum and maximum values,
 *          and assign these values accordingly.
 */

int assign_min_max(double *min, double *max, double val) {
  int flag = 0;

  if (!min || !max)
    return (0);
  flag |= 1;
  if (*min > val) {
    *min = val;
    flag |= 2;
  }
  if (*max < val) {
    *max = val;
    flag |= 4;
  }
  return (flag);
}

/* routine: find_min_max_2d()
 * purpose: find minimum and maximum values in a 2-d array (array
 *          of pointers).
 */

int find_min_max_2d(double *min, double *max, double **value,
                    long n1, long n2) {
  double data, rmin, rmax, *value_i1;
  long i1, i2;

  if (!n1 || !n2 || !min || !max || !value)
    return (0);

  rmin = DBL_MAX;
  rmax = -DBL_MAX;
  for (i1 = 0; i1 < n1; i1++) {
    if (!(value_i1 = value[i1]))
      return (0);
    for (i2 = 0; i2 < n2; i2++) {
      if ((data = value_i1[i2]) > rmax)
        rmax = data;
      if (data < rmin)
        rmin = data;
    }
  }
  return (1);
}

/* routine: find_min_max_2d_float()
 * purpose: find minimum and maximum values in a 2-d array (array
 *          of pointers) of floats.
 */

int find_min_max_2d_float(float *min, float *max, float **value,
                          long n1, long n2) {
  float data, rmin, rmax, *value_i1;
  long i1, i2;

  if (!n1 || !n2 || !min || !max || !value)
    return (0);

  rmin = FLT_MAX;
  rmax = -FLT_MAX;
  for (i1 = 0; i1 < n1; i1++) {
    if (!(value_i1 = value[i1]))
      return (0);
    for (i2 = 0; i2 < n2; i2++) {
      if ((data = value_i1[i2]) > rmax)
        rmax = data;
      if (data < rmin)
        rmin = data;
    }
  }
  *min = rmin;
  *max = rmax;
  return (1);
}

int find_min(
  double *min, double *loc, double *c1, double *c2,
  long n) {
  long i;
  double val;

  if (!n || !loc || !c1 || !c2)
    return (0);

  *min = DBL_MAX;
  for (i = 0; i < n; i++) {
    if ((val = c2[i]) < *min) {
      *min = val;
      *loc = c1[i];
    }
  }
  return (1);
}

int find_max(
  double *max, double *loc, double *c1, double *c2,
  long n) {
  long i;
  double val;

  if (!n || !c1 || !c2 || !loc || !max)
    return (0);
  *max = -DBL_MAX;
  for (i = 0; i < n; i++) {
    if ((val = c2[i]) > *max) {
      *max = val;
      *loc = c1[i];
    }
  }
  return (1);
}

/**
 * @brief Finds the maximum value in an array of doubles.
 *
 * This function iterates through the provided array of doubles to determine the maximum value.
 *
 * @param array Pointer to the array of doubles to search.
 * @param n Number of elements in the array.
 * @return Returns the maximum value found in the array. If the array is empty, returns -DBL_MAX.
 */
double max_in_array(double *array, long n) {
  double max = -DBL_MAX;

  while (n--)
    if (array[n] > max)
      max = array[n];
  return (max);
}

/**
 * @brief Finds the minimum value in an array of doubles.
 *
 * This function iterates through the provided array of doubles to determine the minimum value.
 *
 * @param array Pointer to the array of doubles to search.
 * @param n Number of elements in the array.
 * @return Returns the minimum value found in the array. If the array is empty, returns DBL_MAX.
 */
double min_in_array(double *array, long n) {
  double min = DBL_MAX;

  while (n--)
    if (array[n] < min)
      min = array[n];
  return (min);
}
