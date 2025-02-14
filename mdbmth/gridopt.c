/**
 * @file gridopt.c
 * @brief Functions for performing grid search and random search minimization on an N-dimensional function.
 *
 * This file provides several methods for locating the minimum of a function that may be 
 * expensive or complicated to evaluate. The methods include:
 * - grid_search_min(): Systematically samples the parameter space at fixed intervals.
 * - grid_sample_min(): Randomly samples a grid of points in the parameter space.
 * - randomSampleMin(): Selects random points in the parameter space to find a suitable starting point.
 * - randomWalkMin(): Performs a random walk starting from a given point, potentially refining a solution.
 * Additionally, optimAbort() can signal an external abort condition to halt the search processes.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, R. Soliday, Y. Wang
 */

#include "mdb.h"

#define OPTIM_ABORT 0x0001UL
static unsigned long optimFlags = 0;

/**
 * @brief Set or query the abort condition for optimization routines.
 *
 * When called with a non-zero parameter, this function sets an internal flag that 
 * signals the optimization routines to abort their search. When called with zero, 
 * it returns the current state of the abort flag.
 *
 * @param abort If non-zero, sets the abort condition. If zero, simply queries it.
 * @return Returns 1 if the abort flag is set, otherwise 0.
 */
long optimAbort(long abort) {
  if (abort) {
    /* if zero, then operation is a query */
    optimFlags |= OPTIM_ABORT;
  }
  return optimFlags & OPTIM_ABORT ? 1 : 0;
}

/**
 * @brief Perform a grid search to find the minimum of a given function.
 *
 * Given ranges and steps for each dimension, this function systematically evaluates 
 * the target function over a grid of points. It returns the best found minimum and 
 * updates xReturn with the coordinates of that minimum.
 *
 * @param best_result Pointer to a double that will store the best function value found.
 * @param xReturn Pointer to an array that will be updated with the coordinates of the minimum.
 * @param lower Array specifying the lower bounds of each dimension.
 * @param upper Array specifying the upper bounds of each dimension.
 * @param step Array specifying the step sizes for each dimension.
 * @param n_dimen The number of dimensions in the parameter space.
 * @param target The target function value to achieve or surpass.
 * @param func A pointer to the function to minimize, taking coordinates and returning a value and validity flag.
 * @return Returns 1 if a minimum was found, otherwise 0.
 */
long grid_search_min(
  double *best_result,
  double *xReturn,
  double *lower,
  double *upper,
  double *step,
  long n_dimen,
  double target,
  double (*func)(double *x, long *invalid)) {
  static double *x = NULL, *best_x = NULL;
  static long last_n_dimen = 0;
  static long *index, *counter, *maxcount;
  double result;
  long flag, i, best_found;

  optimFlags = 0;

  if (last_n_dimen < n_dimen) {
    if (x)
      tfree(x);
    if (index)
      tfree(index);
    if (counter)
      tfree(counter);
    if (maxcount)
      tfree(maxcount);
    x = tmalloc(sizeof(*x) * n_dimen);
    best_x = tmalloc(sizeof(*best_x) * n_dimen);
    index = tmalloc(sizeof(*index) * n_dimen);
    counter = tmalloc(sizeof(*counter) * n_dimen);
    maxcount = tmalloc(sizeof(*maxcount) * n_dimen);
    last_n_dimen = n_dimen;
  }

  *best_result = DBL_MAX;
  for (i = 0; i < n_dimen; i++) {
    index[i] = i;
    counter[i] = 0;
    x[i] = lower[i];
    if (lower[i] >= upper[i]) {
      step[i] = 0;
      maxcount[i] = 0;
    } else {
      maxcount[i] = (upper[i] - lower[i]) / step[i] + 1.5;
      if (maxcount[i] <= 1)
        maxcount[i] = 2;
      step[i] = (upper[i] - lower[i]) / (maxcount[i] - 1);
    }
  }

  best_found = 0;
  do {
    if ((result = (*func)(x, &flag)) < *best_result && flag == 0) {
      *best_result = result;
      for (i = 0; i < n_dimen; i++)
        best_x[i] = x[i];
      best_found = 1;
      if (result < target)
        break;
    }
    if (optimFlags & OPTIM_ABORT)
      break;
  } while (advance_values(x, index, lower, step, n_dimen, counter, maxcount, n_dimen) >= 0);

  if (best_found)
    for (i = 0; i < n_dimen; i++)
      xReturn[i] = best_x[i];

  return (best_found);
}

/**
 * @brief Perform a partial (sampled) grid search to find the minimum of a function.
 *
 * This routine performs a grid-based search similar to grid_search_min(), but only evaluates 
 * a fraction of the points, chosen randomly. It returns the best found minimum and updates 
 * xReturn with the coordinates of that minimum.
 *
 * @param best_result Pointer to a double that will store the best function value found.
 * @param xReturn Pointer to an array that will be updated with the coordinates of the minimum.
 * @param lower Array specifying the lower bounds of each dimension.
 * @param upper Array specifying the upper bounds of each dimension.
 * @param step Array specifying the step sizes for each dimension.
 * @param n_dimen The number of dimensions.
 * @param target The target function value to achieve or surpass.
 * @param func The function to minimize.
 * @param sample_fraction The fraction or number of points to sample from the grid.
 * @param random_f Optional random function for generating samples (default random_1).
 * @return Returns 1 if a minimum was found, otherwise 0.
 */
long grid_sample_min(
  double *best_result,
  double *xReturn,
  double *lower,
  double *upper,
  double *step,
  long n_dimen,
  double target,
  double (*func)(double *x, long *invalid),
  double sample_fraction,
  double (*random_f)(long iseed)) {
  static double *x = NULL, *best_x = NULL;
  static long last_n_dimen = 0;
  static long *index, *counter, *maxcount;
  double result;
  long flag, i, best_found;

  optimFlags = 0;

  if (random_f == NULL)
    random_f = random_1;

  if (last_n_dimen < n_dimen) {
    if (x)
      tfree(x);
    if (index)
      tfree(index);
    if (counter)
      tfree(counter);
    if (maxcount)
      tfree(maxcount);
    x = tmalloc(sizeof(*x) * n_dimen);
    best_x = tmalloc(sizeof(*best_x) * n_dimen);
    index = tmalloc(sizeof(*index) * n_dimen);
    counter = tmalloc(sizeof(*counter) * n_dimen);
    maxcount = tmalloc(sizeof(*maxcount) * n_dimen);
    last_n_dimen = n_dimen;
  }

  *best_result = DBL_MAX;
  for (i = 0; i < n_dimen; i++) {
    index[i] = i;
    counter[i] = 0;
    x[i] = lower[i];
    if (lower[i] >= upper[i]) {
      step[i] = 0;
      maxcount[i] = 0;
    } else {
      maxcount[i] = (upper[i] - lower[i]) / step[i] + 1.5;
      if (maxcount[i] <= 1)
        maxcount[i] = 2;
      step[i] = (upper[i] - lower[i]) / (maxcount[i] - 1);
    }
  }

  if (sample_fraction >= 1) {
    double npoints = 1;
    for (i = 0; i < n_dimen; i++)
      npoints *= maxcount[i];
    sample_fraction /= npoints;
  }

  best_found = 0;
  do {
    if (sample_fraction < (*random_f)(1))
      continue;
    if ((result = (*func)(x, &flag)) < *best_result && flag == 0) {
      *best_result = result;
      for (i = 0; i < n_dimen; i++)
        best_x[i] = x[i];
      best_found = 1;
      if (result < target)
        break;
    }
    if (optimFlags & OPTIM_ABORT)
      break;
  } while (advance_values(x, index, lower, step, n_dimen, counter, maxcount, n_dimen) >= 0);

  if (best_found)
    for (i = 0; i < n_dimen; i++)
      xReturn[i] = best_x[i];

  return (best_found);
}

/**
 * @brief Randomly sample the parameter space to find a minimum.
 *
 * This routine randomly samples points in the given parameter space (defined by lower and upper bounds) 
 * for a specified number of samples. It returns the best found minimum and updates xReturn with its coordinates.
 *
 * @param best_result Pointer to a double that will store the best function value found.
 * @param xReturn Pointer to an array that will be updated with the coordinates of the minimum.
 * @param lower Array specifying the lower bounds of each dimension.
 * @param upper Array specifying the upper bounds of each dimension.
 * @param n_dimen The number of dimensions.
 * @param target The target function value.
 * @param func The function to minimize.
 * @param nSamples The number of random samples to try.
 * @param random_f Optional random function for sampling (default random_1).
 * @return Returns 1 if a minimum was found, otherwise 0.
 */
long randomSampleMin(
  double *best_result,
  double *xReturn,
  double *lower,
  double *upper,
  long n_dimen,
  double target,
  double (*func)(double *x, long *invalid),
  long nSamples,
  double (*random_f)(long iseed)) {
  double *x, *xBest;
  double result;
  long flag, i, best_found = 0;

  optimFlags = 0;
  if (random_f == NULL)
    random_f = random_1;

  x = tmalloc(sizeof(*x) * n_dimen);
  xBest = tmalloc(sizeof(*xBest) * n_dimen);
  for (i = 0; i < n_dimen; i++)
    xBest[i] = xReturn[i];
  *best_result = DBL_MAX;
  while (nSamples--) {
    for (i = 0; i < n_dimen; i++)
      x[i] = lower[i] + (upper[i] - lower[i]) * (*random_f)(0);
    if ((result = (*func)(x, &flag)) < *best_result && flag == 0) {
      *best_result = result;
      for (i = 0; i < n_dimen; i++)
        xBest[i] = x[i];
      best_found = 1;
      if (result < target)
        break;
    }
    if (optimFlags & OPTIM_ABORT)
      break;
  }
  if (best_found) {
    for (i = 0; i < n_dimen; i++)
      xReturn[i] = xBest[i];
  }
  free(x);
  free(xBest);
  return (best_found);
}

/**
 * @brief Perform a random walk starting from a given point to find a function minimum.
 *
 * This function starts at a user-supplied point and randomly perturbs it within given bounds 
 * and step sizes. It evaluates the function at each new point, seeking improvements. 
 * If a better minimum is found, xReturn is updated.
 *
 * @param best_result Pointer to a double that will store the best found function value.
 * @param xReturn Pointer to an array with the starting coordinates, updated on success.
 * @param lower Array specifying the lower bounds for each dimension.
 * @param upper Array specifying the upper bounds for each dimension.
 * @param stepSize Array specifying the maximum step size for random perturbations in each dimension.
 * @param n_dimen The number of dimensions.
 * @param target The target function value.
 * @param func The function to minimize.
 * @param nSamples The number of random steps to take.
 * @param random_f Optional random function (default random_1).
 * @return Returns 1 if a minimum was found, otherwise 0.
 */
long randomWalkMin(
  double *best_result,
  double *xReturn,
  double *lower,
  double *upper,
  double *stepSize,
  long n_dimen,
  double target,
  double (*func)(double *x, long *invalid),
  long nSamples,
  double (*random_f)(long iseed)) {
  double *x, *xBest;
  double result;
  long flag, i, best_found = 0;

  optimFlags = 0;

  if (random_f == NULL)
    random_f = random_1;

  x = tmalloc(sizeof(*x) * n_dimen);
  xBest = tmalloc(sizeof(*xBest) * n_dimen);
  for (i = 0; i < n_dimen; i++)
    xBest[i] = xReturn[i];
  *best_result = DBL_MAX;
  while (nSamples--) {
    for (i = 0; i < n_dimen; i++) {
      x[i] = xBest[i] + 2 * stepSize[i] * (0.5 - random_f(0));
      if (lower && x[i] < lower[i])
        x[i] = lower[i];
      if (upper && x[i] > upper[i])
        x[i] = upper[i];
    }
    result = (*func)(x, &flag);
    if (flag == 0 && result < *best_result) {
      *best_result = result;
      for (i = 0; i < n_dimen; i++)
        xBest[i] = x[i];
      best_found = 1;
      if (result < target)
        break;
    }
    if (optimFlags & OPTIM_ABORT)
      break;
  }
  if (best_found) {
    for (i = 0; i < n_dimen; i++)
      xReturn[i] = xBest[i];
  }
  free(x);
  free(xBest);
  return (best_found);
}
