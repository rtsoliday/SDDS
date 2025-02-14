/**
 * @file rcds_powell.c
 * @brief Implementation of the RCDS (Robust Conjugate Direction Search) algorithm.
 *
 * This code is translated from XiaoBiao Huang's MATLAB code for the RCDS algorithm.
 * The RCDS algorithm is used for automated tuning via minimization.
 *
 * Reference: X. Huang, et al. Nucl. Instr. Methods, A, 726 (2013) 77-83.
 */

#include "mdb.h"
#include <time.h>

#define DEFAULT_MAXEVALS 100
#define DEFAULT_MAXPASSES 5

#define DEBUG 0

#define RCDS_ABORT 0x0001UL
static unsigned long rcdsFlags = 0;

/**
 * @brief Sets or queries the abort flag for the RCDS minimization.
 *
 * @param abort If non-zero, sets the abort flag. If zero, queries the abort flag status.
 * @return Returns 1 if the abort flag is set, 0 otherwise.
 */
long rcdsMinAbort(long abort) {
  if (abort) {
    /* if zero, then operation is a query */
    rcdsFlags |= RCDS_ABORT;
#ifdef DEBUG
    fprintf(stderr, "rcdsMin abort requested\n");
#endif
  }
  return rcdsFlags & RCDS_ABORT ? 1 : 0;
}

/* translated from powellmain.m by X. Huang
   %Powell's method for minimization
   %use line scan
   %Input:
   %   func, function handle
   %   xGuess, initial solution
   %   dxGuess, step size
   %   dmat0, initial direction set, default to unit vectors
   %   tol, a small number to define the termination condition, set to 0 to
   %        disable the feature. 
   %   target, the target value
   %   maxPasses, maximum number of iteration, default to 100
   %   maxEvaluations, maximum number of function evaluation, default to 1500
   %   dimensions -- number of variables
   %   noise  -- function value noise
   %Output:
   %   xBset, best solution
   %   yReturn, best func(x1)
   %   nevals, number of evaluations
   %
   %Created by X. Huang, 2/22/2013
   %[x1,f1,nf]=powellmain(@func_obj,x0,step,dmat)
   %[x1,f1,nf]=powellmain(@func_obj,x0,step,dmat,[],100,'noplot')
   %[x1,f1,nf]=powellmain(@func_obj,x0,step,dmat,0,100,'noplot',2000)
   %
   %Reference: X. Huang, et al. Nucl. Instr. Methods, A, 726 (2013) 77-83.
   %
   %Disclaimer: The RCDS algorithm or the Matlab RCDS code come with absolutely 
   %NO warranty. The author of the RCDS method and the Matlab RCDS code does not 
   %take any responsibility for any damage to equipments or personnel injury 
   %that may result from the use of the algorithm or the code.
   %
*/

void sort_two_arrays(double *x, double *y, long n);
/*normalize variable values to [0,1] */
void normalize_variables(double *x0, double *relative_x, double *lowerLimit, double *upperLimit, long dimensions);
/*compute the variable values from its normalized values */
void scale_variables(double *x0, double *relative_x, double *lowerLimit, double *upperLimit, long dimensions);

/*static double (*ipower)(double x, long n); */
static long DIMENSIONS;

long bracketmin(double (*func)(double *x, long *invalid),
                double *x0, double f0, double *dv, double *lowerLimit, double *upperLimit, long dimensions, double noise, double step, double *a10, double *a20, double **stepList, double **flist, long *nflist, double *xm, double *fm, double *xmin, double *fmin);

long linescan(double (*func)(double *x, long *invalid), double *x0, double f0, double *dv, double *lowerLimit, double *upperLimit, long dimensions, double alo, double ahi, long Np, double *step_list, double *f_list, long n_list, double *xm, double *fm, double *xmin, double *fmin);

long outlier_1d(double *x, long n, double mul_tol, double perlim, long *removed_index);

/**
 * @brief Performs minimization using the RCDS (Robust Conjugate Direction Search) algorithm.
 *
 * This function minimizes the given objective function using the RCDS algorithm, which is based on Powell's method with line scans.
 *
 * @param yReturn Pointer to store the best function value found.
 * @param xBest Pointer to an array to store the best solution found.
 * @param xGuess Initial guess for the solution (array of size 'dimensions').
 * @param dxGuess Initial step sizes for each variable (array of size 'dimensions').
 * @param xLowerLimit Lower bounds for the variables (array of size 'dimensions'). Can be NULL.
 * @param xUpperLimit Upper bounds for the variables (array of size 'dimensions'). Can be NULL.
 * @param dmat0 Initial direction set (array of pointers to arrays of size 'dimensions x dimensions'). If NULL, unit vectors are used.
 * @param dimensions Number of variables (dimensions of the problem).
 * @param target Target function value to reach. The minimization will stop if this value is reached.
 * @param tolerance Tolerance for termination condition. If negative, interpreted as fractional tolerance; if positive, interpreted as absolute tolerance.
 * @param func Objective function to minimize. Should take an array of variables and a pointer to an invalid flag, and return the function value.
 * @param report Optional reporting function to call after each iteration. Can be NULL.
 * @param maxEvaluations Maximum number of function evaluations allowed.
 * @param maxPasses Maximum number of iterations (passes) allowed.
 * @param noise Estimated noise level in the function value.
 * @param rcdsStep Initial step size for the line searches.
 * @param flags Control flags for the algorithm behavior.
 * @return Number of function evaluations performed, or negative value on error.
 */
long rcdsMin(double *yReturn, double *xBest, double *xGuess, double *dxGuess, double *xLowerLimit, double *xUpperLimit, double **dmat0, long dimensions, double target, /* will return if any value is <= this */
             double tolerance,                                                                                                                                          /* <0 means fractional, >0 means absolute */
             double (*func)(double *x, long *invalid), void (*report)(double ymin, double *xmin, long pass, long evals, long dims), long maxEvaluations,                /*maimum number of funcation evaluation */
             long maxPasses,                                                                                                                                            /*maximum number of iterations */
             double noise, double rcdsStep, unsigned long flags) {
  long i, j, totalEvaluations = 0, inValid = 0, k, pass, Npmin = 6, direction;
  double *x0 = NULL; /*normalized xGuess */
  double *dv = NULL, del = 0, f0, step = 0.01, f1, fm, ft, a1, a2, tmp, norm, maxp = 0, tmpf, *tmpx = NULL;
  double *xm = NULL, *x1 = NULL, *xt = NULL, *ndv = NULL, *dotp = NULL, *x_value = NULL, *xmin = NULL, fmin;
  double *step_list = NULL, *f_list = NULL, step_init;
  long n_list;

  rcdsFlags = 0;
  if (rcdsStep > 0 && rcdsStep < 1)
    step = rcdsStep;

  if (dimensions <= 0)
    return (-3);
  DIMENSIONS = dimensions;

  if (flags & SIMPLEX_VERBOSE_LEVEL1)
    fprintf(stdout, "rcdsMin dimensions: %ld\n", dimensions);

  x0 = malloc(sizeof(*x0) * dimensions);
  tmpx = malloc(sizeof(*tmpx) * dimensions);
  /*normalize xGuess  to between 0 and 1; for step unification purpose */
  normalize_variables(xGuess, x0, xLowerLimit, xUpperLimit, dimensions);

  f0 = (*func)(xGuess, &inValid); /*note that the function evaluation still uses non-normalized */
  if (inValid) {
    f0 = DBL_MAX;
    fprintf(stderr, "error: initial guess is invalid in rcdsMin()\n");
    free(x0);
    free(tmpx);
    return (-3);
  }
  totalEvaluations++;
  if (!dmat0) {
    dmat0 = malloc(sizeof(*dmat0) * dimensions);
    for (i = 0; i < dimensions; i++) {
      dmat0[i] = calloc(dimensions, sizeof(**dmat0));
      for (j = 0; j < dimensions; j++)
        if (j == i)
          dmat0[i][j] = 1;
    }
  }
  if (dxGuess) {
    step = 0;
    for (i = 0; i < dimensions; i++) {
      if (xLowerLimit && xUpperLimit) {
        step += dxGuess[i] / (xUpperLimit[i] - xLowerLimit[i]);
      } else
        step += dxGuess[i];
    }
    step /= dimensions;
  }
  /* step = 0.01; */
  if (rcdsStep > 0 && rcdsStep < 1)
    step = rcdsStep;
  /*best solution so far */
  xm = malloc(sizeof(*xm) * dimensions);
  xmin = malloc(sizeof(*xmin) * dimensions);
  memcpy(xm, x0, sizeof(*xm) * dimensions);
  memcpy(xmin, x0, sizeof(*xm) * dimensions);
  fmin = fm = f0;
  memcpy(xBest, xGuess, sizeof(*xBest) * dimensions);
  *yReturn = f0;
  if (f0 <= target) {
    if (flags & SIMPLEX_VERBOSE_LEVEL1) {
      fprintf(stdout, "rcdsMin: target value achieved in initial setup.\n");
    }
    if (report)
      (*report)(f0, xGuess, 0, 1, dimensions);
    free(tmpx);
    free(x0);
    free(xm);
    free(xmin);
    free_zarray_2d((void **)dmat0, dimensions, dimensions);
    return (totalEvaluations);
  }

  if (maxPasses <= 0)
    maxPasses = DEFAULT_MAXPASSES;

  x1 = tmalloc(sizeof(*x1) * dimensions);
  xt = tmalloc(sizeof(*xt) * dimensions);
  ndv = tmalloc(sizeof(*ndv) * dimensions);
  dotp = tmalloc(sizeof(*dotp) * dimensions);
  for (i = 0; i < dimensions; i++)
    dotp[i] = 0;

  if (!x_value)
    x_value = tmalloc(sizeof(*x_value) * dimensions);

  if (flags & SIMPLEX_VERBOSE_LEVEL1) {
    fprintf(stdout, "rcdsMin: starting conditions:\n");
    for (direction = 0; direction < dimensions; direction++)
      fprintf(stdout, "direction %ld: guess=%le \n", direction, xGuess[direction]);
    fprintf(stdout, "starting funcation value %le \n", f0);
  }
  pass = 0;
  while (pass < maxPasses && !(rcdsFlags & RCDS_ABORT)) {
    step = step / 1.2;
    step_init = step;
    k = 0;
    del = 0;
    for (i = 0; !(rcdsFlags & RCDS_ABORT) && i < dimensions; i++) {
      dv = dmat0[i];
      if (flags & SIMPLEX_VERBOSE_LEVEL1)
        fprintf(stdout, "begin iteration %ld, var %ld, nf=%ld\n", pass + 1, i + 1, totalEvaluations);
      if (step_list) {
        free(step_list);
        step_list = NULL;
      }
      if (f_list) {
        free(f_list);
        f_list = NULL;
      }
      totalEvaluations += bracketmin(func, xm, fm, dv, xLowerLimit, xUpperLimit, dimensions, noise, step_init, &a1, &a2, &step_list, &f_list, &n_list, x1, &f1, xmin, &fmin);
      memcpy(tmpx, x1, sizeof(*tmpx) * dimensions);
      tmpf = f1;
      if (flags & SIMPLEX_VERBOSE_LEVEL1)
        fprintf(stdout, "\niter %ld, dir (var) %ld: begin linescan %ld\n", pass + 1, i + 1, totalEvaluations);
      if (rcdsFlags & RCDS_ABORT)
        break;
      totalEvaluations += linescan(func, tmpx, tmpf, dv, xLowerLimit, xUpperLimit, dimensions, a1, a2, Npmin, step_list, f_list, n_list, x1, &f1, xmin, &fmin);
      /*direction with largest decrease */
      if ((fm - f1) > del) {
        del = fm - f1;
        k = i;
        if (flags & SIMPLEX_VERBOSE_LEVEL1)
          fprintf(stdout, "iteration %ld, var %ld: del= %f updated", pass + 1, i + 1, del);
      }
      if (flags & SIMPLEX_VERBOSE_LEVEL1)
        fprintf(stdout, "iteration %ld, director %ld done, fm=%f, f1=%f\n", pass + 1, i + 1, fm, f1);

      if (flags & RCDS_USE_MIN_FOR_BRACKET) {
        fm = fmin;
        memcpy(xm, xmin, sizeof(*xm) * dimensions);
      } else {
        fm = f1;
        memcpy(xm, x1, sizeof(*xm) * dimensions);
      }
    }
    if (flags & SIMPLEX_VERBOSE_LEVEL1)
      fprintf(stderr, "\niteration %ld, fm=%f fmin=%f\n", pass + 1, fm, fmin);
    if (rcdsFlags & RCDS_ABORT)
      break;
    inValid = 0;
    for (i = 0; i < dimensions; i++) {
      xt[i] = 2 * xm[i] - x0[i];
      if (fabs(xt[i]) > 1) {
        inValid = 1;
        break;
      }
    }
    if (!inValid) {
      scale_variables(x_value, xt, xLowerLimit, xUpperLimit, dimensions);
      ft = (*func)(x_value, &inValid);
      totalEvaluations++;
    }
    if (inValid)
      ft = DBL_MAX;
    tmp = 2 * (f0 - 2 * fm + ft) * pow((f0 - fm - del) / (ft - f0), 2);
    if ((f0 <= ft) || tmp >= del) {
      if (flags & SIMPLEX_VERBOSE_LEVEL1)
        fprintf(stdout, "dir %ld not replaced, %d, %d\n", k, f0 <= ft, tmp >= del);
    } else {
      /*compute norm of xm - x0 */
      if (flags & SIMPLEX_VERBOSE_LEVEL1)
        fprintf(stdout, "compute dotp\n");
      norm = 0;
      for (i = 0; i < dimensions; i++) {
        norm += (xm[i] - x0[i]) * (xm[i] - x0[i]);
      }
      norm = pow(norm, 0.5);
      for (i = 0; i < dimensions; i++)
        ndv[i] = (xm[i] - x0[i]) / norm;
      maxp = 0;
      for (i = 0; i < dimensions; i++) {
        dv = dmat0[i];
        dotp[i] = 0;
        for (j = 0; j < dimensions; j++)
          dotp[i] += ndv[j] * dv[j];
        dotp[i] = fabs(dotp[i]);
        if (dotp[i] > maxp)
          maxp = dotp[i];
      }
      if (maxp < 0.9) {
        if (flags & SIMPLEX_VERBOSE_LEVEL1)
          fprintf(stdout, "max dot product <0.9, do bracketmin and linescan...\n");
        if (k < dimensions - 1) {
          for (i = k; i < dimensions - 1; i++) {
            for (j = 0; j < dimensions; j++)
              dmat0[i][j] = dmat0[i + 1][j];
          }
        }
        for (j = 0; j < dimensions; j++)
          dmat0[dimensions - 1][j] = ndv[j];
        dv = dmat0[dimensions - 1];
        totalEvaluations += bracketmin(func, xm, fm, dv, xLowerLimit, xUpperLimit, dimensions, noise, step, &a1, &a2, &step_list, &f_list, &n_list, x1, &f1, xmin, &fmin);

        memcpy(tmpx, x1, sizeof(*tmpx) * dimensions);
        tmpf = f1;
        totalEvaluations += linescan(func, tmpx, tmpf, dv, xLowerLimit, xUpperLimit, dimensions, a1, a2, Npmin, step_list, f_list, n_list, x1, &f1, xmin, &fmin);
        memcpy(xm, x1, sizeof(*xm) * dimensions);
        fm = f1;
        if (flags & SIMPLEX_VERBOSE_LEVEL1)
          fprintf(stderr, "fm=%le \n", fm);
      } else {
        if (flags & SIMPLEX_VERBOSE_LEVEL1)
          fprintf(stdout, "   , skipped new direction %ld, max dot product %f\n", k, maxp);
      }
    }
    /*termination */
    if (totalEvaluations > maxEvaluations) {
      fprintf(stderr, "Terminated, reaching function evaluation limit %ld > %ld\n", totalEvaluations, maxEvaluations);
      break;
    }
    if (2.0 * fabs(f0 - fmin) < tolerance * (fabs(f0) + fabs(fmin)) && tolerance > 0) {
      if (flags & SIMPLEX_VERBOSE_LEVEL1)
        fprintf(stdout, "Reach tolerance, terminated, f0=%le, fmin=%le, f0-fmin=%le\n", f0, fmin, f0 - fmin);
      break;
    }
    if (fmin <= target) {
      if (flags & SIMPLEX_VERBOSE_LEVEL1)
        fprintf(stdout, "Reach target, terminated, fm=%le, target=%le\n", fm, target);
      break;
    }
    f0 = fm;
    memcpy(x0, xm, sizeof(*x0) * dimensions);
    pass++;
  }

  /*x1, f1 best solution */
  scale_variables(xBest, xmin, xLowerLimit, xUpperLimit, dimensions);
  *yReturn = fmin;

  free(x0);
  free(xm);
  free_zarray_2d((void **)dmat0, dimensions, dimensions);
  free(x1);
  free(xt);
  free(ndv);
  free(dotp);
  free(f_list);
  f_list = NULL;
  free(step_list);
  step_list = NULL;
  free(tmpx);
  free(x_value);
  return (totalEvaluations);
}

/* translated from bracket.m by X. Huang
   %bracket the minimum along the line with unit direction dv
   %Input:
   %   func, function handle, f=func(x)
   %   x0,  Npx1 vec, initial point, func(x0+alpha*dv)
   %   f0,  f0=func(x0), provided so that no need to re-evaluate, can be NaN
   %        or [], to be evaluated. 
   %   dv, Npx1  vec, the unit vector for a direction in the parameter space
   %   step, initial stepsize of alpha,
   %Output:
   %   xm, fm, the best solution and its value
   %   a1,a2, values of alpha that satisfy f(xm+a1*dv)>fmin and
   %   f(xm+a2*dv)>fmin
   %   xflist, Nx2, all tried solutions
   %   nf, number of function evluations
   %created by X. Huang, 1/25/2013
   %
   
*/

long bracketmin(double (*func)(double *x, long *invalid),
                double *x0, double f0, double *dv, double *lowerLimit, double *upperLimit, long dimensions, 
                double noise, double step, double *a10, double *a20, double **stepList, double **fList, 
                long *nflist, double *xm, double *fm, double *xmin, double *fmin) {
  long nf = 0, inValid, i, n_list;
  //long count;
  static double *x1 = NULL, *x2 = NULL, gold_r = 1.618034;
  double *step_list = NULL, *f_list = NULL;
  double f1, step_init, am, a1, a2, f2, tmp, step0;
  static double *x_value = NULL;

  *fm = f0;
  memcpy(xm, x0, sizeof(*xm) * dimensions);
  am = 0;

  if (!x1)
    x1 = malloc(sizeof(*x1) * dimensions);
  if (!f_list)
    f_list = malloc(sizeof(*f_list) * 100);
  if (!step_list)
    step_list = malloc(sizeof(*step_list) * 100);
  n_list = 0;
  step_list[0] = 0;
  f_list[0] = f0;
  n_list++;
  step_init = step;
  inValid = 0;
  for (i = 0; i < dimensions; i++) {
    x1[i] = x0[i] + dv[i] * step;
    if (fabs(x1[i]) > 1) {
      inValid = 1;
      break;
    }
  }
  if (!x_value)
    x_value = malloc(sizeof(*x_value) * dimensions);

  if (inValid)
    f1 = DBL_MAX;
  else {
    scale_variables(x_value, x1, lowerLimit, upperLimit, dimensions);
    /*need scale variable values before calling the function */
    f1 = (*func)(x_value, &inValid);
    nf++;
    if (inValid)
      f1 = DBL_MAX;
  }

  f_list[n_list] = f1;
  step_list[n_list] = step;
  n_list++;

  if (f1 < *fm) {
    *fm = f1;
    memcpy(xm, x1, sizeof(*xm) * dimensions);
    am = step;
  }
  if (f1 < *fmin) {
    *fmin = f1;
    memcpy(xmin, x1, sizeof(*xmin) * dimensions);
  }
  //count = 0;
  while (f1 < *fm + noise * 3 && !(rcdsFlags & RCDS_ABORT)) {
    step0 = step;
    /*maximum step 0.1 */
    if (fabs(step) < 0.1)
      step = step * (1.0 + gold_r);
    else
      step = step + 0.01;

    inValid = 0;
    for (i = 0; i < dimensions; i++) {
      x1[i] = x0[i] + dv[i] * step;
      if (fabs(x1[i]) > 1) {
        inValid = 1;
        break;
      }
    }
    if (inValid) {
      f1 = DBL_MAX;
    } else {
      scale_variables(x_value, x1, lowerLimit, upperLimit, dimensions);
      f1 = (*func)(x_value, &inValid);
      if (inValid)
        f1 = DBL_MAX;
      nf++;
    }
    if (n_list > 100) {
      f_list = trealloc(f_list, sizeof(*f_list) * (n_list + 1));
      step_list = trealloc(step_list, sizeof(*step_list) * (n_list + 1));
    }
    if (inValid) {
      step = step0; /*get the last vaild solution */
      break;
    }
    f_list[n_list] = f1;
    step_list[n_list] = step;
    n_list++;
    if (f1 < *fm) {
      *fm = f1;
      am = step;
      memcpy(xm, x1, sizeof(*xm) * dimensions);
    }
    if (f1 < *fmin) {
      *fmin = f1;
      memcpy(xmin, x1, sizeof(*xmin) * dimensions);
    }
    //count++;
  }
  a2 = step;
  if (f0 > *fm + noise * 3) {
    a1 = 0;
    a1 = a1 - am;
    a2 = a2 - am;
    for (i = 0; i < n_list; i++)
      step_list[i] -= am;
    free(x1);
    x1 = NULL;
    free(x2);
    x2 = NULL;
    *a10 = a1;
    *a20 = a2;

    *fList = f_list;
    *stepList = step_list;
    *nflist = n_list;
    *a10 = a1;
    *a20 = a2;
    free(x1);
    x1 = NULL;
    free(x2);
    x2 = NULL;
    return nf;
  }

  if (!x2)
    x2 = malloc(sizeof(*x2) * dimensions);
  /*go to negative direction */
  step = -1 * step_init;
  inValid = 0;
  for (i = 0; i < dimensions; i++) {
    x2[i] = x0[i] + dv[i] * step;
    if (fabs(x2[i]) > 1) {
      inValid = 1;
      break;
    }
  }
  if (inValid)
    f2 = DBL_MAX;
  else {
    scale_variables(x_value, x2, lowerLimit, upperLimit, dimensions);
    f2 = (*func)(x_value, &inValid);
    if (inValid)
      f2 = DBL_MAX;
    nf++;
  }
  if (n_list > 100) {
    f_list = trealloc(f_list, sizeof(*f_list) * (n_list + 1));
    step_list = trealloc(step_list, sizeof(*step_list) * (n_list + 1));
  }
  f_list[n_list] = f2;
  step_list[n_list] = step;
  n_list++;

  if (f2 < *fm) {
    *fm = f2;
    am = step;
    memcpy(xm, x2, sizeof(*xm) * dimensions);
  }
  if (f2 < *fmin) {
    *fmin = f2;
    memcpy(xmin, x2, sizeof(*xmin) * dimensions);
  }
  //count = 0;
  while (f2 < *fm + noise * 3 && !(rcdsFlags & RCDS_ABORT)) {
    step0 = step;
    if (fabs(step) < 0.1)
      step = step * (1.0 + gold_r);
    else
      step = step - 0.01;
    inValid = 0;
    for (i = 0; i < dimensions; i++) {
      x2[i] = x0[i] + dv[i] * step;
      if (fabs(x2[i]) > 1) {
        inValid = 1;
        break;
      }
    }
    if (inValid) {
      f2 = DBL_MAX;
    } else {
      scale_variables(x_value, x2, lowerLimit, upperLimit, dimensions);
      f2 = (*func)(x_value, &inValid);
      if (inValid)
        f2 = DBL_MAX;
      nf++;
    }
    if (inValid) {
      /* get the last valid solution */
      step = step0;
      break;
    }

    if (n_list > 100) {
      f_list = trealloc(f_list, sizeof(*f_list) * (n_list + 1));
      step_list = trealloc(step_list, sizeof(*step_list) * (n_list + 1));
    }
    f_list[n_list] = f2;
    step_list[n_list] = step;
    n_list++;
    //count++;
    if (f2 < *fm) {
      *fm = f2;
      am = step;
      memcpy(xm, x2, sizeof(*xm) * dimensions);
    }
    if (f2 < *fmin) {
      *fmin = f2;
      memcpy(xmin, x2, sizeof(*xmin) * dimensions);
    }
  }

  a1 = step;
  if (a1 > a2) {
    tmp = a1;
    a1 = a2;
    a2 = tmp;
  }
  a1 = a1 - am;
  a2 = a2 - am;
  for (i = 0; i < n_list; i++)
    step_list[i] -= am;

  sort_two_arrays(step_list, f_list, n_list);
  /******/
  /*move linescan here instead of powellMain so that no need to return step_list and f_list */

  *stepList = step_list;
  *fList = f_list;
  *nflist = n_list;

  free(x1);
  x1 = NULL;
  free(x2);
  x2 = NULL;
  *a10 = a1;
  *a20 = a2;
  return nf;
}

void scale_variables(double *x0, double *relative_x, double *lowerLimit, double *upperLimit, long dimensions) {
  long i;
  if (lowerLimit && upperLimit) {
    for (i = 0; i < dimensions; i++) {
      x0[i] = relative_x[i] * (upperLimit[i] - lowerLimit[i]) + lowerLimit[i];
    }
  } else {
    memcpy(x0, relative_x, sizeof(*x0) * dimensions);
  }
}

void normalize_variables(double *x0, double *relative_x, double *lowerLimit, double *upperLimit, long dimensions) {
  long i;
  if (lowerLimit && upperLimit) {
    for (i = 0; i < dimensions; i++) {
      relative_x[i] = (x0[i] - lowerLimit[i]) / (upperLimit[i] - lowerLimit[i]);
    }
  } else
    memcpy(relative_x, x0, sizeof(*relative_x) * dimensions);
}

/* transfered from X. Huang's matlab code linescan.m
   %line scan in the parameter space along a direction dv
   %Input:
   %   func, function handle, f=func(x)
   %   x0,  Npx1 vec, initial point
   %   f0,  f0=func(x0), provided so that no need to re-evaluate, can be NaN
   %        or [], to be evaluated. 
   %   dv, Npx1  vec, the unit vector for a direction in the parameter space
   %   alo, ahi, scalar, the low and high bound of alpha, for f=func(x0+a dv)
   %   Np, minimum number of points for fitting. 
   %   xflist, Nx2, known solutions
   %Output:
   %   xm, the new solution
   %   fm, the value at the new solution, f1=func(x1)
   %   nf, the number of function evaluations
   %Created by X. Huang, 1/25/2013
   %

*/

long linescan(double (*func)(double *x, long *invalid), double *x0, double f0, double *dv, double *lowerLimit, double *upperLimit, long dimensions, double alo, double ahi, long Np, double *step_list, double *f_list, long n_list, double *xm, double *fm, double *xmin, double *fmin) {
  long nf = 0, i, j, k, MP, n_new, terms, *order;
  long inValid;
  int64_t imin, imax;
  long *is_outlier, outliers;
  double a1, f1, tmp_min, tmp_max, tmp, delta, delta2, mina;
  static double *x1 = NULL, *aNew = NULL, *fNew = NULL, *av = NULL, *x_value = NULL;
  double *coef, *coefSigma, chi, *diff, *tmpa = NULL, *tmpf = NULL, *fv = NULL;

  if (alo >= ahi) {
    fprintf(stderr, "high bound should be larger than the low bound\n");
    return 0;
  }
  if (Np < 6)
    Np = 6;
  delta = (ahi - alo) / (Np - 1);
  delta2 = delta / 2.0;
  if (!x1)
    x1 = malloc(sizeof(*x1) * dimensions);
  /*add interpolation points to eveanly space */
  n_new = 0;
  if (!aNew)
    aNew = malloc(sizeof(*aNew) * Np);
  if (!fNew)
    fNew = malloc(sizeof(*fNew) * Np);
  if (!x_value)
    x_value = malloc(sizeof(*x_value) * dimensions);

  for (i = 0; i < Np && !(rcdsFlags & RCDS_ABORT); i++) {
    a1 = alo + delta * i;
    mina = fabs(a1 - step_list[0]);
    for (j = 1; j < n_list; j++) {
      tmp = fabs(a1 - step_list[j]);
      if (tmp < mina) {
        mina = fabs(a1 - step_list[j]);
      }
    }
    /*remove points which mian<=delta/2.0, keep the insert points if mina>delta/2.0 */
    /*added a small value 1.0e-16, to keep the point that close to delta/2.0 */
    if (mina + 1.0e-16 > delta2) {
      for (k = 0; k < dimensions; k++) {
        x1[k] = x0[k] + dv[k] * a1;
      }
      scale_variables(x_value, x1, lowerLimit, upperLimit, dimensions);
      f1 = (*func)(x_value, &inValid);
      nf++;
      if (f1 < *fmin) {
        *fmin = f1;
        memcpy(xmin, x1, sizeof(*xmin) * dimensions);
      }
      if (inValid) {
        f1 = DBL_MAX;
      } else {
        aNew[n_new] = a1;
        fNew[n_new] = f1;
        n_new++;
      }
    }
  }
  if (rcdsFlags & RCDS_ABORT) {
    if (x1)
      free(x1);
    x1 = NULL;
    return nf;
  }

  if (n_new) {
    /*merge insertion points */
    if (n_new + n_list > 100) {
      f_list = trealloc(f_list, sizeof(*f_list) * (n_list + n_new + 1));
      step_list = trealloc(step_list, sizeof(*step_list) * (n_list + n_new + 1));
    }
    for (i = 0; i < n_new; i++) {
      step_list[n_list + i] = aNew[i];
      f_list[n_list + i] = fNew[i];
    }
    n_list += n_new;
  }

  if (aNew)
    free(aNew);
  aNew = NULL;
  if (fNew)
    free(fNew);
  fNew = NULL;

  /*sort new list after adding inserting points */
  sort_two_arrays(step_list, f_list, n_list);

  index_min_max(&imin, &imax, f_list, n_list);
  for (i = 0; i < dimensions; i++)
    xm[i] = x0[i] + step_list[imin] * dv[i];
  *fm = f_list[imin];
  if (*fm < *fmin) {
    *fmin = *fm;
    memcpy(xmin, xm, sizeof(*xmin) * dimensions);
  }
  if (n_list <= 5)
    return nf;
  /*else, do outlier */
  tmp_min = MAX(step_list[0], step_list[imin] - 6 * delta);
  tmp_max = MIN(step_list[n_list - 1], step_list[imin] + 6 * delta);

  MP = 101;
  if (!av)
    av = tmalloc(sizeof(*av) * MP);
  for (i = 0; i < MP; i++)
    av[i] = tmp_min + (tmp_max - tmp_min) * i * 1.0 / MP;
  fv = tmalloc(sizeof(*fv) * MP);

  /* polynormial fit */
  terms = 3;
  coef = tmalloc(sizeof(*coef) * terms);
  coefSigma = tmalloc(sizeof(*coefSigma) * terms);
  order = tmalloc(sizeof(*order) * terms);
  for (i = 0; i < terms; i++)
    order[i] = i;
  diff = tmalloc(sizeof(*diff) * n_list);

  lsfp(step_list, f_list, NULL, n_list, terms, order, coef, coefSigma, &chi, diff);

  /*do outlier based on diff */
  is_outlier = tmalloc(sizeof(*is_outlier) * n_list);
  for (i = 0; i < n_list; i++)
    diff[i] = -1.0 * diff[i];

  outliers = outlier_1d(diff, n_list, 3.0, 0.25, is_outlier);

  for (i = 0; i < n_list; i++) {
    diff[i] = -1 * diff[i];
  }
  if (outliers <= 1) {
    if (outliers == 1) {
      tmpa = tmalloc(sizeof(*tmpa) * n_list);
      tmpf = tmalloc(sizeof(*tmpf) * n_list);
      n_new = 0;
      for (j = 0; j < n_list; j++)
        if (!is_outlier[j]) {
          tmpa[n_new] = step_list[j];
          tmpf[n_new] = f_list[j];
          n_new++;
        }

      lsfp(tmpa, tmpf, NULL, n_new, terms, order, coef, coefSigma, &chi, diff);

      free(tmpf);
      free(tmpa);
    }
    for (i = 0; i < MP; i++) {
      fv[i] = coef[0] + av[i] * coef[1] + coef[2] * av[i] * av[i];
    }
    index_min_max(&imin, &imax, fv, MP);
    for (i = 0; i < dimensions; i++) {
      x1[i] = xm[i] = x0[i] + av[imin] * dv[i];
    }
    scale_variables(x_value, x1, lowerLimit, upperLimit, dimensions);
    memcpy(xm, x1, sizeof(*xm) * dimensions);

    f1 = (*func)(x_value, &inValid);
    nf++;
    if (inValid)
      f1 = DBL_MAX;
    *fm = f1;
    if (f1 < *fmin) {
      *fmin = f1;
      memcpy(xmin, x1, sizeof(*xmin) * dimensions);
    }
  } else {
    /* do nothing, use the minimum result */
  }
  if (x1)
    free(x1);
  x1 = NULL;
  free(av);
  av = NULL;
  free(coef);
  coef = NULL;
  free(coefSigma);
  coefSigma = NULL;
  free(order);
  order = NULL;
  free(diff);
  diff = NULL;
  free(is_outlier);
  is_outlier = NULL;
  free(fv);
  fv = NULL;
  return nf;
}

void sort_two_arrays(double *x, double *y, long n) {
  double *tmpx, *tmpy;
  long i, j;

  tmpx = malloc(sizeof(*tmpx) * n);
  memcpy(tmpx, x, sizeof(*tmpx) * n);
  tmpy = malloc(sizeof(*tmpy) * n);

  qsort(tmpx, n, sizeof(*tmpx), double_cmpasc);
  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      if (tmpx[i] == x[j])
        break;
    }
    tmpy[i] = y[j];
  }
  for (i = 0; i < n; i++) {
    y[i] = tmpy[i];
    x[i] = tmpx[i];
  }
  free(tmpx);
  free(tmpy);
  return;
}

/* function [x,indxin,indxout]=outlier1d(x,varargin)
   %[x,indxin]=outlier1d(x,varargin)
   %remove the outliers of x from it
   %x is 1d vector with dim>=3
   %mul_tol = varargin{1}, default 3.0, 
   %perlim  = varargin{2}, default 0.25
   %The algorithm is: 1. sort x to ascending order. 2. calculate the difference series of the 
   %sorted x. 3. calculate the average difference of the central (1- 2*perlim) part.
   %4. examine the difference series of the upper and lower perlim (25% by default) part, if 
   %there is a jump that is larger than mul_tol (3 by default) times of the std, remove 
   %the upper or lower part from that point on
   %
   %Created by X. Huang in matlab, Nov. 2004
   %
*/

long outlier_1d(double *x, long n, double mul_tol, double perlim, long *is_outlier) {
  long i, j, outlier = 0, *index = NULL, upl, dnl, upcut, dncut;
  double *tmpx = NULL, *diff = NULL, ave1 = 0, ave2 = 0;

  if (n < 3)
    return 0;
  index = tmalloc(sizeof(*index) * n);
  tmpx = tmalloc(sizeof(*tmpx) * n);
  diff = tmalloc(sizeof(*diff) * n);

  /*sort data x */
  memcpy(tmpx, x, sizeof(*tmpx) * n);
  qsort((void *)tmpx, n, sizeof(*tmpx), double_cmpasc);

  for (i = 0; i < n; i++) {
    is_outlier[i] = 0;
    for (j = 0; j < n; j++) {
      if (tmpx[i] == x[j])
        break;
    }
    index[i] = j;
  }

  /*length of diff is n-1 */
  for (i = 1; i < n; i++) {
    diff[i - 1] = tmpx[i] - tmpx[i - 1];
  }
  if (n <= 4) {
    /*n=3 or n=4; remove lower or upper diff; diff dimension is n-1  */
    /*average from 0 to n-3 */
    for (i = 0; i < n - 2; i++)
      ave1 += diff[i];
    for (i = 1; i < n - 1; i++)
      ave2 += diff[i];
    ave1 /= n - 2;
    ave2 /= n - 2;
    if (diff[n - 2] > mul_tol * ave1) {
      is_outlier[index[n - 2]] = 1;
      outlier++;
    }
    if (diff[0] > mul_tol * ave2) {
      is_outlier[index[0]] = 1;
      outlier++;
    }
    return outlier;
  }
  upl = MAX((int)(n * (1 - perlim)), 3) - 1;
  dnl = MAX((int)(n * perlim), 2) - 1;
  ave1 = 0;
  for (i = dnl; i <= upl; i++)
    ave1 += diff[i];
  ave1 /= upl - dnl + 1;
  upcut = n;
  dncut = -1;

  outlier = 0;
  for (i = upl; i < n - 1; i++) {
    if (diff[i] > mul_tol * ave1) {
      upcut = i + 1;
    }
  }

  for (i = dnl; i >= 0; i--) {
    if (diff[i] > mul_tol * ave1) {
      dncut = i;
    }
  }
  for (i = upcut; i < n; i++) {
    is_outlier[index[i]] = 1;
    outlier++;
  }
  for (i = dncut; i >= 0; i--) {
    is_outlier[index[i]] = 1;
    outlier++;
  }
  free(tmpx);
  free(diff);
  free(index);
  return outlier;
}
