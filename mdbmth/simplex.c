/** @file simplex.c
 *  @brief Provides routines for performing multivariate function optimization using the simplex method.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday, H. Shang, J. Calvey
 */

#include "mdb.h"
#include <time.h>

#define DEFAULT_MAXEVALS 100
#define DEFAULT_MAXPASSES 5

#define SIMPLEX_ABORT 0x0001UL
static unsigned long simplexFlags = 0;

/**
 * @brief Abort or query the status of the simplex optimization.
 *
 * If a nonzero value is passed, an abort is requested. If zero, this function queries whether
 * an abort was previously requested.
 *
 * @param abort Nonzero to request abort, zero to query.
 *
 * @return 1 if abort was requested, 0 otherwise.
 */
long simplexMinAbort(unsigned long abort) {
  if (abort) {
    /* if zero, then operation is a query */
    simplexFlags |= SIMPLEX_ABORT;
    if (abort & SIMPLEX_ABORT_ANNOUNCE_STDOUT) {
      printf("simplexMin abort requested\n");
      fflush(stdout);
    }
    if (abort & SIMPLEX_ABORT_ANNOUNCE_STDERR)
      fprintf(stderr, "simplexMin abort requested\n");
  }
  return simplexFlags & SIMPLEX_ABORT ? 1 : 0;
}

long checkVariableLimits(double *x, double *xlo, double *xhi, long n) {
  long i;

  if (xlo)
    for (i = 0; i < n; i++)
      if (xlo[i] != xhi[i] && x[i] < xlo[i])
        return 0;

  if (xhi)
    for (i = 0; i < n; i++)
      if (xlo[i] != xhi[i] && x[i] > xhi[i])
        return 0;
  return 1;
}

void computeSimplexCenter(double *center, double **vector, long dimensions, long activeDimensions) {
  long point, direction;
  for (direction = 0; direction < dimensions; direction++) {
    /* outer loop over dimension */
    for (point = center[direction] = 0; point <= activeDimensions; point++)
      /* inner loop over vectors */
      center[direction] += vector[point][direction];
    center[direction] /= activeDimensions; /* sic--not activeDimensions+1, as one term will get
                                             * subtracted out later 
                                             */
  }
}

double trialSimplex(
  double **simplexVector,
  double *funcValue,
  double *simplexCenter,
  double *coordLowerLimit,
  double *coordUpperLimit,
  short *disable,
  long dimensions,
  long activeDimensions,
  double (*func)(double *x, long *inval),
  long worstPoint,
  long *evaluations,
  double factor,
  short *usedLast, short *newPoint) {
  double *trialVector;
  long direction, isInvalid;
  double trialValue, center;

  *newPoint = *usedLast = 0;
  trialVector = tmalloc(sizeof(*trialVector) * dimensions);

#if DEBUG
  fprintf(stdout, "Creating new trial simplex\n");
  fflush(stdout);
#endif

  for (direction = 0; direction < dimensions; direction++) {
    /* compute the center of the simplex excluding the worst point */
    center = simplexCenter[direction] - simplexVector[worstPoint][direction] / activeDimensions;
    /* Move relative to that center by factor times the distance from it to the worst point. 
     * (In some cases, the "worst point" is actually just the new (improved) point put in the
     * slot of the previous worst point.)
     */
    if (!disable || !disable[direction])
      trialVector[direction] =
        center + factor * (simplexVector[worstPoint][direction] - center);
    else
      trialVector[direction] = simplexVector[worstPoint][direction];
  }

  /* check limits on the values of each coordinate of the trial vector */
  if (!checkVariableLimits(trialVector, coordLowerLimit, coordUpperLimit, dimensions)) {
    /* return 1e9*funcValue[worstPoint]; this is wrong 
      in the casue of funcValue[worstPoint]<0 */
#if DEBUG
    fprintf(stdout, "Variables out of limits\n");
    fflush(stdout);
#endif
    free(trialVector);
    return DBL_MAX;
    /*return a positive value so that simplex will do contraction in case of exceeding limits*/
  } else {
    /* check to see if this is the same as the last point evaluated here */
    *usedLast = 0;

#if DEBUG
    fprintf(stdout, "Evaluating point\n");
    fflush(stdout);
#endif
    trialValue = (*func)(trialVector, &isInvalid);
    ++(*evaluations);
    if (isInvalid) {
#if DEBUG
      fprintf(stdout, "Invalid point\n");
      fflush(stdout);
#endif
      free(trialVector);
      return DBL_MAX;
    }
  }

  if (trialValue < funcValue[worstPoint]) {
    /* this is better than the previous worst value, so replace the worst value */
    *newPoint = 1;
    funcValue[worstPoint] = trialValue;
    for (direction = 0; direction < dimensions; direction++) {
      /* adjust the "center" values for the simplex and copy the new vector coordinates */
      simplexCenter[direction] += (trialVector[direction] - simplexVector[worstPoint][direction]) / activeDimensions;
      simplexVector[worstPoint][direction] = trialVector[direction];
    }
  }
#if DEBUG
  fprintf(stdout, "Returning improved trial point\n");
  fflush(stdout);
#endif

  free(trialVector);
  return (trialValue);
}

void simplexFindBestWorst(double *fValue, long points,
                          long *bestPointPtr, long *worstPointPtr,
                          long *nextWorstPointPtr) {
  long bestPoint, worstPoint, nextWorstPoint, point;
  double fBest, fNextWorst, fWorst;

  if (fValue[0] > fValue[1]) {
    bestPoint = nextWorstPoint = 1;
    worstPoint = 0;
  } else {
    bestPoint = nextWorstPoint = 0;
    worstPoint = 1;
  }
  fBest = fNextWorst = fValue[bestPoint];
  fWorst = fValue[worstPoint];
  for (point = 1; point < points; point++) {
    if (fBest > fValue[point]) {
      bestPoint = point;
      fBest = fValue[point];
    }
    if (fWorst < fValue[point]) {
      worstPoint = point;
      fWorst = fValue[point];
    }
  }
  for (point = 0; point < points; point++)
    if (fNextWorst < fValue[point] && point != worstPoint) {
      fNextWorst = fValue[point];
      nextWorstPoint = point;
    }
  *bestPointPtr = bestPoint;
  *worstPointPtr = worstPoint;
  *nextWorstPointPtr = nextWorstPoint;
}

/**
 * @brief Perform a simplex-based minimization of a given function.
 *
 * This function uses the simplex method to minimize the given function, updating the
 * simplex until the desired tolerance is reached or maximum evaluations are met.
 *
 * @param simplexVector 2D array defining the current simplex vertices.
 * @param fValue Array of function values at each simplex vertex.
 * @param coordLowerLimit Array of lower limits for each variable.
 * @param coordUpperLimit Array of upper limits for each variable.
 * @param disable Array indicating which variables are fixed (not optimized).
 * @param dimensions Total number of variables.
 * @param activeDimensions Number of variables currently being optimized.
 * @param target Target function value; stop if reached.
 * @param tolerance Tolerance for stopping criteria.
 * @param tolerance_mode Defines whether tolerance is absolute (1) or fractional (0).
 * @param function Pointer to the function to be minimized.
 * @param maxEvaluations Maximum number of function evaluations allowed.
 * @param evaluations Pointer to store the number of evaluations performed.
 * @param flags Bitwise flags modifying the behavior of the minimization.
 *
 * @return Nonzero if a solution is found or zero if the iteration limit is reached.
 */
long simplexMinimization(
  double **simplexVector,  /* vectors defining the simplex */
  double *fValue,          /* values of the function at the vertices of the simplex */
  double *coordLowerLimit, /* lower limits allowed for independent variables */
  double *coordUpperLimit, /* upper limits allowed for independent variables */
  short *disable,          /* indicates coordinate not involved in optimization */
  long dimensions,         /* number of variables in function */
  long activeDimensions,   /* number of variables changed in optimization */
  double target,           /* will return with any value <= this */
  double tolerance,        /* desired tolerance of minimum value */
  long tolerance_mode,     /* 0==fractional, 1==absolute */
  double (*function)(double *x, long *invalid),
  long maxEvaluations,
  long *evaluations, /* number of function evaluations done during minimization */
  unsigned long flags) {
  long point, points, invalids, degenerates, isDegenerate, isInvalid;
  long direction, bestPoint, worstPoint, nextWorstPoint;
  double fTrial, fProblem, fWorst, fBest, merit, denominator;
  double *simplexCenter = NULL, *tmpVector;
  short usedLast, usedLastCount = 0, newPoint;
  long reflectionWorked = 0, extensionWorked = 0, contractionWorked = 0, shrinkingDone = 0;
  long progressMade;

  simplexCenter = tmalloc(sizeof(*simplexCenter) * (dimensions));
  tmpVector = tmalloc(sizeof(*tmpVector) * (dimensions));

  *evaluations = 0;
  if (maxEvaluations <= 0)
    maxEvaluations = DEFAULT_MAXEVALS;

  computeSimplexCenter(simplexCenter, simplexVector, dimensions, activeDimensions);

  points = activeDimensions + 1;
  while (*evaluations < maxEvaluations && !(simplexFlags & SIMPLEX_ABORT)) {
    /* find indices of lowest, highest, and next-to-highest y values .
       These starting values are to guarantee that worstPoint!=bestPoint even if
       all function values are the same.
       */
    if (flags & SIMPLEX_VERBOSE_LEVEL2) {
      fprintf(stdout, "simplexMinimization: finding best and worst points\n");
      fflush(stdout);
    }
    simplexFindBestWorst(fValue, points, &bestPoint, &worstPoint, &nextWorstPoint);
    fBest = fValue[bestPoint];
    fWorst = fValue[worstPoint];

    if (flags & SIMPLEX_VERBOSE_LEVEL2) {
      fprintf(stdout, "simplexMinimization: evaluating present results\n");
      fflush(stdout);
    }
    /* evaluate the merit of the present vectors */
    if (tolerance_mode == 0) {
      /* fractional tolerance */
      if ((denominator = (fabs(fWorst) + fabs(fBest)) / 2))
        merit = fabs(fWorst - fBest) / denominator;
      else {
        fputs("error: divide-by-zero in fractional tolerance evaluation (simplexMinimization)\n", stderr);
        free(simplexCenter);
        free(tmpVector);
        return 0;
      }
    } else
      /* absolute tolerance */
      merit = fabs(fWorst - fBest);
    if (merit < tolerance || fBest <= target) {
      /* tolerance exceeded, or value small enough */
      if (flags & SIMPLEX_VERBOSE_LEVEL2) {
        fprintf(stdout, "simplexMinimization: tolerance exceed or value small enough\n");
        fflush(stdout);
      }
      break;
    }

    progressMade = 0;
    if (flags & SIMPLEX_VERBOSE_LEVEL2) {
      fprintf(stdout, "simplexMinimization: Reflecting simplex\n");
      fflush(stdout);
    }
    /* Reflect the simplex through the high point */
    fTrial = trialSimplex(simplexVector, fValue, simplexCenter, coordLowerLimit,
                          coordUpperLimit, disable, dimensions, activeDimensions, function,
                          worstPoint, evaluations, -1.0, &usedLast, &newPoint);
    if (flags & SIMPLEX_VERBOSE_LEVEL2) {
      fprintf(stdout, "simplexMinization: reflection returns (newPoint=%d)\n", newPoint);
      fflush(stdout);
    }
    reflectionWorked += newPoint ? 1 : 0;
    progressMade += newPoint;
    if (usedLast)
      usedLastCount++;
    else
      usedLastCount = 0;
    if (usedLastCount > 2) {
      if (flags & SIMPLEX_VERBOSE_LEVEL2) {
        fprintf(stdout, "simplexMinization: simplex is looping--ending iterations\n");
        fflush(stdout);
      }
      /* stuck in some kind of loop */
      break;
    }
    if (fTrial < fValue[bestPoint]) {
      /* since this worked, extend the simplex by the same amount in that direction.
       * relies on the fact that the new point of the simplex is in the old "worstPoint"
       * slot
       */
      if (flags & SIMPLEX_VERBOSE_LEVEL2) {
        fprintf(stdout, "simplexMinization: extending simplex\n");
        fflush(stdout);
      }
      fTrial = trialSimplex(simplexVector, fValue, simplexCenter, coordLowerLimit,
                            coordUpperLimit, disable, dimensions, activeDimensions, function,
                            worstPoint, evaluations, 2.0, &usedLast, &newPoint);
      if (flags & SIMPLEX_VERBOSE_LEVEL2) {
        fprintf(stdout, "simplexMinization: extension returns (newPoint=%d)\n", newPoint);
        fflush(stdout);
      }
      extensionWorked += newPoint ? 1 : 0;
      progressMade += newPoint;
    } else if (fTrial > fValue[nextWorstPoint]) {
      /* reflection through the simplex didn't help, so try contracting away from worst point without
       * going through the face opposite the worst point */
      if (flags & SIMPLEX_VERBOSE_LEVEL2) {
        fprintf(stdout, "simplexMinization: contracting simplex\n");
        fflush(stdout);
      }
      fProblem = fTrial;
      fTrial = trialSimplex(simplexVector, fValue, simplexCenter, coordLowerLimit,
                            coordUpperLimit, disable, dimensions, activeDimensions, function,
                            worstPoint, evaluations, 0.5, &usedLast, &newPoint);
      if (flags & SIMPLEX_VERBOSE_LEVEL2) {
        fprintf(stdout, "simplexMinization: contraction returns (newPoint=%d)\n", newPoint);
        fflush(stdout);
      }
      contractionWorked += newPoint ? 1 : 0;
      progressMade += newPoint;
      if (fTrial > fProblem) {
        /* the new point is worse than the old trial point, so try moving the entire simplex in on the
           best point by averaging each vector with the vector to the best point.  Don't allow invalid points,
           however, and keep track of the number of degenerate points (those with the same vector or the
           same function value).
           */
        if (flags & SIMPLEX_VERBOSE_LEVEL2) {
          fprintf(stdout, "simplexMinimization: contracting on best point\n");
          fflush(stdout);
        }
        invalids = degenerates = 0;
        for (point = 0; point < points; point++) {
          if (point == bestPoint)
            continue;
          for (direction = 0; direction < dimensions; direction++)
            tmpVector[direction] = 0.5 * (simplexVector[point][direction] + simplexVector[bestPoint][direction]);
          for (direction = 0; direction < dimensions; direction++)
            if (tmpVector[direction] != simplexVector[point][direction])
              break;
          isInvalid = 0;
          if (!(isDegenerate = direction != dimensions)) {
            fTrial = (*function)(tmpVector, &isInvalid);
            if (!isInvalid) {
              if (fTrial == fValue[point])
                isDegenerate = 1;
              for (direction = 0; direction < dimensions; direction++)
                simplexVector[point][direction] = tmpVector[direction];
              fValue[point] = fTrial;
            }
          }
          if (isInvalid)
            invalids++;
          if (isDegenerate)
            degenerates++;
        }
        shrinkingDone++;
        if (invalids + degenerates >= points - 1) {
          SWAP_PTR(simplexVector[0], simplexVector[bestPoint]);
          SWAP_DOUBLE(fValue[0], fValue[bestPoint]);
          if (flags & SIMPLEX_VERBOSE_LEVEL2) {
            fprintf(stdout, "simplexMinimization exiting: reflection: %ld   extension: %ld  contraction: %ld  shrinking: %ld\n",
                    reflectionWorked, extensionWorked, contractionWorked, shrinkingDone);
            fflush(stdout);
          }
          free(simplexCenter);
          free(tmpVector);
          return 0;
        }
        *evaluations += points;
        /* since the simplex was changed without using trialSimplex, the "center" must be recomputed
         */
        progressMade += 1;
        computeSimplexCenter(simplexCenter, simplexVector, dimensions, activeDimensions);
      }
    }
    if (!progressMade) {
      if (flags & SIMPLEX_VERBOSE_LEVEL2) {
        fprintf(stdout, "simplexMinimization: Breaking out of loop--no progress.\n");
        fflush(stdout);
      }
      break;
    }
  }
  simplexFindBestWorst(fValue, points, &bestPoint, &worstPoint, &nextWorstPoint);
  if (*evaluations >= maxEvaluations) {
    SWAP_PTR(simplexVector[0], simplexVector[bestPoint]);
    SWAP_DOUBLE(fValue[0], fValue[bestPoint]);
    if (flags & SIMPLEX_VERBOSE_LEVEL2) {
      fprintf(stdout, "simplexMinimization: too many iterations\n");
      fflush(stdout);
    }
    free(simplexCenter);
    free(tmpVector);
    return 0;
  }
  SWAP_PTR(simplexVector[0], simplexVector[bestPoint]);
  SWAP_DOUBLE(fValue[0], fValue[bestPoint]);
  if (flags & SIMPLEX_VERBOSE_LEVEL2) {
    fprintf(stdout, "simplexMinimization exit report: reflection: %ld   extension: %ld  contraction: %ld  shrinking: %ld\n",
            reflectionWorked, extensionWorked, contractionWorked, shrinkingDone);
    fflush(stdout);
  }
  free(simplexCenter);
  free(tmpVector);
  return (1);
}

/**
 * @brief Top-level convenience function for simplex-based minimization.
 *
 * This function sets up and runs a simplex optimization on the provided function, 
 * attempting to find a minimum within given constraints and stopping criteria.
 *
 * @param yReturn Pointer to store the best found function value.
 * @param xGuess Initial guess for the variables.
 * @param dxGuess Initial step sizes for each variable (may be adjusted automatically).
 * @param xLowerLimit Lower variable limits.
 * @param xUpperLimit Upper variable limits.
 * @param disable Array indicating which variables are fixed.
 * @param dimensions Total number of variables.
 * @param target Target function value to stop if reached.
 * @param tolerance Tolerance for stopping criteria. Negative means fractional.
 * @param func Pointer to the objective function.
 * @param report Optional reporting function called at each pass.
 * @param maxEvaluations Maximum function evaluations.
 * @param maxPasses Maximum passes over the simplex.
 * @param maxDivisions Maximum allowed divisions in initial simplex setup.
 * @param divisorFactor Factor to adjust step size.
 * @param passRangeFactor Factor to adjust range after each pass.
 * @param flags Bitwise flags for controlling verbosity and behavior.
 *
 * @return Number of evaluations if successful, negative on error, or if aborted.
 */
long simplexMin(
  double *yReturn,
  double *xGuess,
  double *dxGuess,
  double *xLowerLimit,
  double *xUpperLimit,
  short *disable,
  long dimensions,
  double target,    /* will return if any value is <= this */
  double tolerance, /* <0 means fractional, >0 means absolute */
  double (*func)(double *x, long *invalid),
  void (*report)(double ymin, double *xmin, long pass, long evals, long dims),
  long maxEvaluations,
  long maxPasses,
  long maxDivisions,
  double divisorFactor,   /* for old default behavior, set to 3 */
  double passRangeFactor, /* for old default behavior, set to 1 */
  unsigned long flags) {
  double **simplexVector = NULL, *y = NULL, *trialVector = NULL, *dxLocal = NULL;
  long *dimIndex = NULL;
  double yLast, dVector = 1, divisor, denominator, merit;
  long direction, point, evaluations, totalEvaluations = 0, isInvalid, pass = 0, step, divisions;
  long activeDimensions, dimension, i;

  if (divisorFactor <= 1.0)
    divisorFactor = 3; /* old default value */
  simplexFlags = 0;
  if (dimensions <= 0)
    return (-3);
  if (disable) {
    activeDimensions = 0;
    for (direction = 0; direction < dimensions; direction++)
      if (!disable[direction])
        activeDimensions++;
  } else
    activeDimensions = dimensions;
  if (activeDimensions <= 0)
    return -3;

  if (flags & SIMPLEX_VERBOSE_LEVEL1) {
    fprintf(stdout, "simplexMin: Active dimensions: %ld\n", activeDimensions);
    fflush(stdout);
  }
  simplexVector = (double **)zarray_2d(sizeof(**simplexVector), activeDimensions + 1, dimensions);
  y = tmalloc(sizeof(*y) * (activeDimensions + 1));
  trialVector = tmalloc(sizeof(*trialVector) * activeDimensions);
  dxLocal = tmalloc(sizeof(*dxLocal) * activeDimensions);
  dimIndex = tmalloc(sizeof(*dimIndex) * activeDimensions);

  for (direction = i = 0; direction < dimensions; direction++) {
    if (!disable || !disable[direction])
      dimIndex[i++] = direction;
  }
  if (i != activeDimensions) {
    fprintf(stderr, "Fatal error (simplexMin): active dimensions not properly counted\n");
    exit(1);
  }

  if (!dxGuess) {
    dxGuess = dxLocal;
    for (direction = 0; direction < dimensions; direction++)
      dxGuess[direction] = 0;
  }
  if (flags & SIMPLEX_RANDOM_SIGNS) {
    time_t intTime;
    time(&intTime);
    srand(intTime);
  }
  for (direction = 0; direction < dimensions; direction++) {
    if (dxGuess[direction] == 0) {
      if (xLowerLimit && xUpperLimit)
        dxGuess[direction] = (xUpperLimit[direction] - xLowerLimit[direction]) / 4;
      else if ((dxGuess[direction] = xGuess[direction] / 4) == 0)
        dxGuess[direction] = 1;
    }
    if (flags & SIMPLEX_RANDOM_SIGNS) {
      if (rand() > RAND_MAX / 2.0)
        dxGuess[direction] *= -1;
    }
    if (xLowerLimit && xUpperLimit) {
      if ((dVector = fabs(xUpperLimit[direction] - xLowerLimit[direction]) / 4) < fabs(dxGuess[direction]))
        dxGuess[direction] = dVector;
    }
    if (disable && disable[direction])
      dxGuess[direction] = 0;
  }

  if (xLowerLimit) {
    /* if start is at lower limit, make sure initial step is positive */
    for (direction = 0; direction < dimensions; direction++)
      if (xLowerLimit[direction] >= xGuess[direction])
        dxGuess[direction] = fabs(dxGuess[direction]);
  }
  if (xUpperLimit) {
    /* if start is at upper limit, make sure initial step is negative */
    for (direction = 0; direction < dimensions; direction++)
      if (xUpperLimit[direction] <= xGuess[direction])
        dxGuess[direction] = -fabs(dxGuess[direction]);
  }
  if (flags & SIMPLEX_VERBOSE_LEVEL1) {
    fprintf(stdout, "simplexMin: starting conditions:\n");
    for (direction = 0; direction < dimensions; direction++)
      fprintf(stdout, "direction %ld: guess=%le delta=%le disable=%hd\n",
              direction, xGuess[direction], dxGuess[direction],
              disable ? disable[direction] : (short)0);
    fflush(stdout);
  }

  if (maxPasses <= 0)
    maxPasses = DEFAULT_MAXPASSES;

  /* need this to prevent problems when abort occurs before the
   * initial simplex is formed
   */
  for (point = 0; point < activeDimensions + 1; point++)
    y[point] = DBL_MAX;

  while (pass < maxPasses && !(simplexFlags & SIMPLEX_ABORT)) {
    /* Set up the initial simplex */
    /* The first vertex is just the starting point */
    for (direction = 0; direction < dimensions; direction++)
      simplexVector[0][direction] = xGuess[direction];
    *yReturn = y[0] = (*func)(simplexVector[0], &isInvalid);
    totalEvaluations++;
    pass++;
    if (isInvalid) {
      fprintf(stderr, "error: initial guess is invalid in simplexMin()\n");
      free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
      free(y);
      free(trialVector);
      free(dxLocal);
      free(dimIndex);
      return (-3);
    }
    if (y[0] <= target) {
      if (flags & SIMPLEX_VERBOSE_LEVEL1) {
        fprintf(stdout, "simplexMin: target value achieved in initial simplex setup.\n");
        fflush(stdout);
      }
      if (report)
        (*report)(y[0], simplexVector[0], pass, totalEvaluations, dimensions);
      free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
      free(y);
      free(trialVector);
      free(dxLocal);
      free(dimIndex);
      return (totalEvaluations);
    }

    divisor = 1;
    divisions = 0;
    for (point = 1; !(simplexFlags & SIMPLEX_ABORT) && point < activeDimensions + 1; point++) {
      if (flags & SIMPLEX_VERBOSE_LEVEL1) {
        fprintf(stdout, "simplexMin: Setting initial simplex for direction %ld\n", point - 1);
        fflush(stdout);
      }
      dimension = dimIndex[point - 1];
      if (!(flags & SIMPLEX_NO_1D_SCANS)) {
        /* Set up the rest of the simplex.  Each vertex is found by doing a 1-D scan
         * starting with the first or the last vertex. 
         */
        for (direction = 0; direction < dimensions; direction++)
          simplexVector[point][direction] = simplexVector[(flags & SIMPLEX_START_FROM_VERTEX1) ? 0 : point - 1][direction];

        /* Scan direction point-1 until a direction of improvement is found. */
        divisions = 0;
        divisor = 1;
        yLast = y[point - 1];
        while (divisions < maxDivisions && !(simplexFlags & SIMPLEX_ABORT)) {
          if (flags & SIMPLEX_VERBOSE_LEVEL1) {
            fprintf(stdout, "simplexMin: working on division %ld (divisor=%e) for direction %ld\n",
                    divisions, divisor, point - 1);
            fflush(stdout);
          }
          simplexVector[point][dimension] = simplexVector[point - 1][dimension] + dxGuess[dimension] / divisor;
          if ((xLowerLimit || xUpperLimit) &&
              !checkVariableLimits(simplexVector[point], xLowerLimit, xUpperLimit, dimensions)) {
#if DEBUG
            long idum;
            fprintf(stdout, " Point outside of bounds:\n");
            fflush(stdout);
            for (idum = 0; idum < dimensions; idum++)
              fprintf(stdout, "    %le  %le, %le\n", simplexVector[point][idum],
                      xLowerLimit[idum], xUpperLimit[idum]);
            fflush(stdout);
#endif
            /* y[point] = fabs(y[0])*1e9;*/
            y[point] = DBL_MAX;
          } else {
            y[point] = (*func)(simplexVector[point], &isInvalid);
            totalEvaluations++;
            if (isInvalid) {
#if DEBUG
              fprintf(stdout, " Point is invalid\n");
              fflush(stdout);
#endif
              /*  y[point] = fabs(y[0])*1e9; */
              y[point] = DBL_MAX;
            }
            if (y[point] <= target) {
              for (direction = 0; direction < dimensions; direction++)
                xGuess[direction] = simplexVector[point][direction];
              *yReturn = y[point];
              if (report)
                (*report)(*yReturn, xGuess, pass, totalEvaluations, dimensions);
              if (flags & SIMPLEX_VERBOSE_LEVEL1) {
                fprintf(stdout, "simplexMin: invalid function status.  Returning.\n");
                fflush(stdout);
              }
              free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
              free(y);
              free(trialVector);
              free(dxLocal);
              free(dimIndex);
              return (totalEvaluations);
            }
          }
          if (flags & SIMPLEX_VERBOSE_LEVEL1) {
            fprintf(stdout, "simplexMin: New value: %le   Last value: %le\n", y[point], yLast);
            fflush(stdout);
          }
          if (y[point] < yLast)
            /* decrease found */
            break;
          divisions++;
          if (divisions % 2)
            /* reverse directions */
            divisor *= -1;
          else
            /* decrease the step size */
            divisor *= divisorFactor;
        }
      }
      if ((flags & SIMPLEX_NO_1D_SCANS) || divisions == maxDivisions) {
        for (direction = 0; direction < dimensions; direction++)
          simplexVector[point][direction] = simplexVector[0][direction];

        /* Try +/-step-size/divisor until a valid point is found */
        divisions = 0;
        divisor = 1;
        yLast = y[point - 1];
        while (divisions < maxDivisions && !(simplexFlags & SIMPLEX_ABORT)) {
#if DEBUG
          fprintf(stdout, "Trying divisor %ld\n", divisions);
          fflush(stdout);
#endif
          simplexVector[point][dimension] = simplexVector[0][dimension] +
                                            dxGuess[dimension] / divisor;
          if ((xLowerLimit || xUpperLimit) &&
              !checkVariableLimits(simplexVector[point], xLowerLimit, xUpperLimit, dimensions)) {
            divisions++;
          } else {
            y[point] = (*func)(simplexVector[point], &isInvalid);
            totalEvaluations++;
            if (isInvalid) {
#if DEBUG
              fprintf(stdout, " Point is invalid\n");
              fflush(stdout);
#endif
              /* y[point] = fabs(y[0])*1e9; */
              y[point] = DBL_MAX;
              divisions++;
            } else
              break;
          }
          if (divisions % 2)
            /* reverse directions */
            divisor *= -1;
          else
            /* decrease the step size */
            divisor *= 10;
        }
        if (divisions == maxDivisions) {
          fprintf(stderr, "error: can't find valid initial simplex in simplexMin()\n");
          free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
          free(y);
          free(trialVector);
          free(dxLocal);
          free(dimIndex);
          return (-4);
        }

      } else {
        if (flags & SIMPLEX_VERBOSE_LEVEL1) {
          fprintf(stdout, "simplexMin: decrease found---trying more steps\n");
          fflush(stdout);
        }
        /* decrease found---try a few more steps in this direction */
        for (step = 0; !(simplexFlags & SIMPLEX_ABORT) && step < 3; step++) {
          divisor /= divisorFactor; /* increase step size */
          simplexVector[point][dimension] += dxGuess[dimension] / divisor;
          if ((xLowerLimit || xUpperLimit) &&
              !checkVariableLimits(simplexVector[point], xLowerLimit, xUpperLimit, dimensions)) {
            simplexVector[point][dimension] -= dxGuess[dimension] / divisor;
            break;
          }
          yLast = y[point];
          y[point] = (*func)(simplexVector[point], &isInvalid);
          totalEvaluations++;
          if (isInvalid || y[point] > yLast) {
            simplexVector[point][dimension] -= dxGuess[dimension] / divisor;
            y[point] = yLast;
            break;
          }
          if (y[point] <= target) {
            for (direction = 0; direction < dimensions; direction++)
              xGuess[direction] = simplexVector[point][direction];
            *yReturn = y[point];
            if (report)
              (*report)(*yReturn, xGuess, pass, totalEvaluations, dimensions);
            if (flags & SIMPLEX_VERBOSE_LEVEL1) {
              fprintf(stdout, "simplexMin: value below target during 1D scan---returning\n");
              fflush(stdout);
            }
            free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
            free(y);
            free(trialVector);
            free(dxLocal);
            free(dimIndex);
            return totalEvaluations;
          }
        }
      }
    }

    if (flags & SIMPLEX_VERBOSE_LEVEL1) {
      fprintf(stdout, "simplexMin: Starting simplex: \n");
      for (point = 0; point < activeDimensions + 1; point++) {
        fprintf(stdout, "V%2ld  %.5g: ", point, y[point]);
        for (direction = 0; direction < dimensions; direction++)
          fprintf(stdout, "%.5g ", simplexVector[point][direction]);
        fprintf(stdout, "\n");
      }
      fflush(stdout);
    }

    if (simplexFlags & SIMPLEX_ABORT) {
      long best = 0;
      for (point = 1; point < activeDimensions + 1; point++)
        if (y[point] < y[best])
          best = point;
      for (direction = 0; direction < dimensions; direction++)
        xGuess[direction] = simplexVector[best][direction];
      if (flags & SIMPLEX_VERBOSE_LEVEL1) {
        fprintf(stdout, "simplexMin: abort received before simplex began---returning\n");
        fflush(stdout);
      }
      free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
      free(y);
      free(trialVector);
      free(dxLocal);
      free(dimIndex);
      return totalEvaluations;
    }

    evaluations = 0;
    simplexMinimization(simplexVector, y, xLowerLimit, xUpperLimit, disable,
                        dimensions, activeDimensions, target,
                        fabs(tolerance), (tolerance < 0 ? 0 : 1), func, maxEvaluations, &evaluations,
                        flags);
    if (flags & SIMPLEX_VERBOSE_LEVEL1) {
      fprintf(stdout, "simplexMin: returned from simplexMinimization after %ld evaluations\n",
              evaluations);
      fflush(stdout);
    }
    totalEvaluations += evaluations;
    for (point = 1; point < activeDimensions + 1; point++) {
      if (y[0] > y[point]) {
        free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
        free(y);
        free(trialVector);
        free(dxLocal);
        free(dimIndex);
        bomb("problem with ordering of data from simplexMinimization", NULL);
      }
    }

    /* Copy the new best result into the guess vector (for return or re-use) */
    for (direction = 0; direction < dimensions; direction++)
      xGuess[direction] = simplexVector[0][direction];

    if (report)
      (*report)(y[0], simplexVector[0], pass, totalEvaluations, dimensions);

    if (y[0] <= target || (simplexFlags & SIMPLEX_ABORT)) {
      *yReturn = y[0];
      if (flags & SIMPLEX_VERBOSE_LEVEL1) {
        fprintf(stdout, "simplexMin: target value achieved---returning\n");
        fflush(stdout);
      }
      free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
      free(y);
      free(trialVector);
      free(dxLocal);
      free(dimIndex);
      return (totalEvaluations);
    }

    if (tolerance <= 0) {
      denominator = (y[0] + (*yReturn)) / 2;
      if (denominator)
        merit = fabs(y[0] - (*yReturn)) / denominator;
      else {
        fputs("error: divide-by-zero in fractional tolerance evaluation (simplexMin)\n", stderr);
        free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
        free(y);
        free(trialVector);
        free(dxLocal);
        free(dimIndex);
        return -1;
      }
    } else
      merit = fabs(y[0] - (*yReturn));
    if (merit <= fabs(tolerance) || y[0] <= target)
      break;

    /* Set up step sizes for finding the new simplex */
    for (direction = 0; direction < dimensions; direction++) {
      double min, max;
      min = max = simplexVector[0][direction];
      for (point = 1; point < activeDimensions + 1; point++) {
        if (simplexVector[point][direction] > max)
          max = simplexVector[point][direction];
        if (simplexVector[point][direction] < min)
          min = simplexVector[point][direction];
      }
      if (max > min)
        dxGuess[direction] = passRangeFactor * (max - min);
    }
  }

  if (flags & SIMPLEX_VERBOSE_LEVEL1) {
    fprintf(stdout, "simplexMin: iterations exhausted---returning\n");
    fflush(stdout);
  }
  *yReturn = y[0];

  free_zarray_2d((void **)simplexVector, activeDimensions + 1, dimensions);
  free(y);
  free(trialVector);
  free(dxLocal);
  free(dimIndex);

  if (pass > maxPasses)
    return (-2);
  return (totalEvaluations);
}

/**
 * @brief Enforce variable limits on a given vector of variables.
 *
 * @note
 * - No longer used---checkVariableLimits is used instead.
 * @param x Array of variable values to be checked and corrected.
 * @param xlo Array of lower limits for each variable.
 * @param xhi Array of upper limits for each variable.
 * @param n Number of variables.
 */
void enforceVariableLimits(double *x, double *xlo, double *xhi, long n) {
  long i;

  if (xlo)
    for (i = 0; i < n; i++)
      if ((!xhi || xlo[i] != xhi[i]) && x[i] < xlo[i])
        x[i] = xlo[i];

  if (xhi)
    for (i = 0; i < n; i++)
      if ((!xlo || xlo[i] != xhi[i]) && x[i] > xhi[i])
        x[i] = xhi[i];
}

