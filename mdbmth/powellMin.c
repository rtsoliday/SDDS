/**
 * @file powellMin.c
 * @brief Implementation of Powell's optimization algorithm for multidimensional minimization.
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

#define DEFAULT_MAXEVALS 100
#define DEFAULT_MAXPASSES 5

#define DEBUG 0

long powellMoveToMin(double *yReturn, double *x, double *xWork, double *dx,
                     double *xLower, double *xUpper, long dims, long linMinIterations, long maxGoodSteps,
                     double (*func)(double *x, long *invalid)) {
  double y0, y1 = 0;
  long i, j, invalid, goodSteps, goodSteps0;
  long divisor, division, divisions, success, evals, limitHit;

  y0 = *yReturn;

  /* first, try to find a direction and step-size that leads to a decrease in the function value */
  divisor = 10;
  success = evals = 0;
  divisions = 20;
  division = 0;
  memcpy(xWork, x, sizeof(*x) * dims);
  while (division++ < divisions) {
    /* step in direction dx, looking for reduction */
    for (i = 0; i < dims; i++) {
      x[i] += dx[i] / divisor;
      if (xLower && xLower[i] > x[i])
        x[i] = xLower[i];
      if (xUpper && xUpper[i] < x[i])
        x[i] = xUpper[i];
    }
    y1 = (*func)(x, &invalid);
#if DEBUG
    fprintf(stderr, ">> values are: ");
    for (i = 0; i < dims; i++)
      fprintf(stderr, "%g, ", x[i]);
    fprintf(stderr, "\n");
    fprintf(stderr, "result is %g, invalid=%ld\n",
            y1, invalid);
#endif
    evals++;
    if (invalid)
      y1 = 1e9 * fabs(y0) + 1;
    if (y1 >= y0) {
#if DEBUG
      fprintf(stderr, ">> not improved\n");
#endif
      memcpy(x, xWork, sizeof(*x) * dims);
      if (division % 2)
        divisor *= -1;
      else
        divisor *= -10;
    } else {
#if DEBUG
      fprintf(stderr, ">> improved\n");
#endif
      success = 1;
      break;
    }
  }
  if (!success) {
#if DEBUG
    fprintf(stderr, ">> optimization failed\n");
#endif
    return evals;
  }
#if DEBUG
  fprintf(stderr, ">> optimization succeeded\n");
#endif

  /* continue in this direction, if possible */
  limitHit = 0;
  for (j = 0; !limitHit && j < linMinIterations; j++) {
#if DEBUG
    fprintf(stderr, ">> trying to step again\n");
#endif
    success = 0;
    memcpy(xWork, x, sizeof(*x) * dims);
    goodSteps = goodSteps0 = 0;
    while (1) {
      y0 = y1;
      for (i = 0; i < dims; i++) {
        x[i] += dx[i] / divisor;
        if (xLower && xLower[i] > x[i]) {
          limitHit = 1;
          x[i] = xLower[i];
        }
        if (xUpper && xUpper[i] < x[i]) {
          limitHit = 1;
          x[i] = xUpper[i];
        }
      }
      y1 = (*func)(x, &invalid);
      evals++;
      if (invalid)
        y1 = 1e9 * fabs(y0) + 1;
      if (y1 >= y0) {
        memcpy(x, xWork, sizeof(*x) * dims);
        y1 = y0;
        break;
      } else {
        memcpy(xWork, x, sizeof(*x) * dims);
        success = 1;
        goodSteps++;
        if (maxGoodSteps > 0 && goodSteps > maxGoodSteps) {
          *yReturn = y0;
          return evals;
        }
        if ((goodSteps - goodSteps0) > 5) {
          divisor /= 2;
          goodSteps0 = goodSteps;
        }
      }
    }
    if (success)
      /* turn around and repeat */
      divisor *= -PI;
    else
      break;
  }

  *yReturn = y0;
  return evals;
}

long powellMinStep(double *yReturn, double *xReturn, double **dirVector, double **P,
                   double *xLower, double *xUpper, long dims, double target,
                   long linMinIterations, long limitGoodSteps,
                   double (*func)(double *x, long *invalid)) {
  long i, invalid, evals;
  double *yValue, yExtrap, *PExtrap = NULL, dyMax, dy;

  if (!(yValue = malloc(sizeof(*yValue) * (dims + 1))) ||
      !(PExtrap = malloc(sizeof(*PExtrap) * dims)))
    bomb("memory allocation failure (powellMinStep)", NULL);

  memcpy(P[0], xReturn, sizeof(*xReturn) * dims);
  yValue[0] = *yReturn;
  dyMax = -DBL_MAX;
  evals = 0;
  for (i = 1; i <= dims; i++) {
#if DEBUG
    fprintf(stderr, ">> Scanning %ld\n", i - 1);
#endif
    memcpy(P[i], P[i - 1], sizeof(**P) * dims);
    yValue[i] = yValue[i - 1];
    evals += powellMoveToMin(yValue + i, P[i], PExtrap, dirVector[i - 1], xLower, xUpper, dims,
                             linMinIterations, limitGoodSteps ? 3 : 0, func);
    if ((dy = yValue[i] - yValue[i - 1]) > dyMax)
      dyMax = dy;
  }
  if (dyMax == -DBL_MAX)
    return evals;

  for (i = 0; i < dims; i++)
    PExtrap[i] = 2 * P[dims][i] - P[0][i];
  yExtrap = (*func)(PExtrap, &invalid);
  if (invalid)
    yExtrap = 1e9 * yValue[0];
  if (!(yExtrap >= yValue[0] ||
        2 * (yValue[0] - 2 * yValue[dims] + yExtrap) *
            sqr(yValue[0] - yValue[dims] - dyMax) >=
          sqr(yValue[0] - yExtrap) * dyMax)) {
    /* exchange dir vectors */
    for (i = 0; i < dims - 1; i++)
      memcpy(dirVector[i], dirVector[i + 1], sizeof(**dirVector) * dims);
    for (i = 0; i < dims; i++)
      dirVector[dims - 1][i] = P[dims][i] - P[0][i];
  }
  memcpy(xReturn, P[dims], sizeof(*xReturn) * dims);
  *yReturn = yValue[dims];
  free(yValue);
  free(PExtrap);
  return evals;
}

/**
 * @brief Minimizes an objective function using Powell's method.
 *
 * This is the main function implementing Powell's optimization algorithm. It iteratively
 * searches for the minimum of the provided objective function within the specified bounds
 * and tolerance levels.
 *
 * @param yReturn Pointer to store the final function value at the minimum.
 * @param xGuess Initial guess for the position in the parameter space.
 * @param dxGuess Initial guess for the step sizes in each dimension.
 * @param xLowerLimit Array of lower bounds for each dimension.
 * @param xUpperLimit Array of upper bounds for each dimension.
 * @param dims Number of dimensions in the parameter space.
 * @param target Target function value; the algorithm will terminate if any value is
 *               less than or equal to this target.
 * @param tolerance Tolerance for convergence. If negative, interpreted as a fractional
 *                  tolerance; if positive, as an absolute tolerance.
 * @param func Pointer to the objective function to be minimized. It should accept
 *             the current position and a pointer to a long indicating if the
 *             evaluation is invalid.
 * @param report Pointer to a reporting function that is called after each pass. It
 *               receives the current minimum value, position, pass number, total
 *               evaluations, and number of dimensions.
 * @param maxPasses Maximum number of passes (iterations) to perform.
 * @param maxEvaluations Maximum number of function evaluations allowed.
 * @param linMinIterations Number of linear minimization iterations to perform.
 * @return The total number of function evaluations performed during the minimization.
 */
long powellMin(
  double *yReturn,
  double *xGuess,
  double *dxGuess,
  double *xLowerLimit,
  double *xUpperLimit,
  long dims,
  double target,    /* will return if any value is <= this */
  double tolerance, /* <0 means fractional, >0 means absolute */
  double (*func)(double *x, long *invalid),
  void (*report)(double ymin, double *xmin, long pass, long evals, long dims),
  long maxPasses,
  long maxEvaluations,
  long linMinIterations) {
  double *xTrial, *dxLocal = NULL, **dirVector = NULL, **P = NULL;
  double y0, dVector, denominator, merit;
  long dir, totalEvaluations = 0, isInvalid, pass = 0;

  if (!yReturn || !xGuess || !dxGuess)
    return -1;
  if (dims <= 0)
    return (-3);

  if (!(xTrial = malloc(sizeof(*xTrial) * dims)) ||
      !(dxLocal = malloc(sizeof(*dxLocal) * dims)) ||
      !(P = (double **)zarray_2d(sizeof(**P), dims + 1, dims)) ||
      !(dirVector = (double **)zarray_2d(sizeof(**dirVector), dims, dims)))
    bomb("memory allocation failure (powellMin)", NULL);
  memcpy(dxLocal, dxGuess, sizeof(*dxLocal) * dims);

  for (dir = 0; dir < dims; dir++) {
    if (dxLocal[dir] == 0) {
      if (xLowerLimit && xUpperLimit)
        dxLocal[dir] = (xUpperLimit[dir] - xLowerLimit[dir]) / 4;
      else if ((dxLocal[dir] = xGuess[dir] / 4) == 0)
        dxLocal[dir] = 1;
    }
    if ((xLowerLimit && xUpperLimit) &&
        (dVector = fabs(xUpperLimit[dir] - xLowerLimit[dir]) / 4) < fabs(dxLocal[dir]))
      dxLocal[dir] = dVector;
  }

  if (xLowerLimit || xUpperLimit) {
    /* if start is at lower/upper limit, make sure initial step is positive/negative */
    for (dir = 0; dir < dims; dir++) {
      if (xLowerLimit && xUpperLimit && xLowerLimit[dir] >= xUpperLimit[dir])
        continue;
      if (xLowerLimit && xLowerLimit[dir] >= xGuess[dir]) {
        dxLocal[dir] = fabs(dxLocal[dir]);
        xGuess[dir] = xLowerLimit[dir];
      }
      if (xUpperLimit && xUpperLimit[dir] <= xGuess[dir]) {
        dxLocal[dir] = -fabs(dxLocal[dir]);
        xGuess[dir] = xUpperLimit[dir];
      }
    }
  }

  for (dir = 0; dir < dims; dir++) {
    P[0][dir] = xGuess[dir];
    dirVector[dir][dir] = dxLocal[dir];
  }

  if (maxPasses <= 0)
    maxPasses = DEFAULT_MAXPASSES;

  y0 = (*func)(xGuess, &isInvalid);
  totalEvaluations++;
  if (isInvalid) {
    fprintf(stderr, "Fatal error (powellMin): initial guess is invalid\n");
    exit(1);
  }
  if (y0 <= target) {
    if (report)
      (*report)(y0, xGuess, pass, totalEvaluations, dims);
    return (totalEvaluations);
  }

  while (pass++ < maxPasses) {
    *yReturn = y0;
#if DEBUG
    fprintf(stderr, ">> Performing powell step to min %ld\n", pass);
#endif
    totalEvaluations += powellMinStep(yReturn, xGuess, dirVector, P,
                                      xLowerLimit, xUpperLimit, dims, target, linMinIterations,
                                      !pass, func);
#if DEBUG
    fprintf(stderr, ">> %ld evaluations total, value is %le\n",
            totalEvaluations, *yReturn);
#endif
    if (tolerance <= 0) {
      denominator = (y0 + (*yReturn)) / 2;
      if (denominator)
        merit = fabs(y0 - (*yReturn)) / denominator;
      else {
        fputs("error: divide-by-zero in fractional tolerance evaluation (powellMin)\n", stderr);
        return -1;
      }
    } else
      merit = fabs(y0 - (*yReturn));
    if (merit <= fabs(tolerance) || (*yReturn) <= target || totalEvaluations > maxEvaluations) {
      if (report)
        (*report)(*yReturn, xGuess, pass, totalEvaluations, dims);
      return (totalEvaluations);
    }
    if (report)
      (*report)(*yReturn, xGuess, pass, totalEvaluations, dims);
    y0 = *yReturn;
  }
#if DEBUG
  fprintf(stderr, ">> returning from powell optimization\n");
#endif
  return totalEvaluations;
}

