/**
 * @file rkODE.c
 * @brief Fourth-order Runge-Kutta ODE integration routines (double-precision version).
 *
 * This file provides functions for integrating ordinary differential equations using
 * fourth-order Runge-Kutta methods. It includes adaptive step-size control and
 * supports various integration scenarios.
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
#include <float.h>

#define MAX_EXIT_ITERATIONS 400
#define ITER_FACTOR 0.995
#define TINY 1.0e-30
#define DEBUG 0

void new_scale_factors_dp(
  double *yscale,
  double *y0,
  double *dydx0,
  double h_start,
  double *tiny,
  long *accmode,
  double *accuracy,
  long n_eq) {
  int i;

  for (i = 0; i < n_eq; i++) {
    switch (accmode[i]) {
    case -1: /* no accuracy control */
      yscale[i] = DBL_MAX;
      break;
    case 0: /* fractional local accuracy specified */
      yscale[i] =
        (y0[i] + dydx0[i] * h_start + tiny[i]) * accuracy[i];
      break;
    case 1: /* fractional global accuracy specified */
      yscale[i] =
        (dydx0[i] * h_start + tiny[i]) * accuracy[i];
      break;
    case 2: /* absolute local accuracy specified */
      yscale[i] = accuracy[i];
      break;
    case 3: /* absolute global accuracy specified */
      yscale[i] = accuracy[i] * h_start;
      break;
    default:
      printf("error: accmode[%d] = %ld (new_scale_factors_dp)\n", i, accmode[i]);
      abort();
      break;
    }
    if (yscale[i] <= 0) {
      printf("error: yscale[%d] = %e\n", i, yscale[i]);
      abort();
    }
  }
}

void initial_scale_factors_dp(
  double *yscale,
  double *y0,
  double *dydx0,
  double h_start,
  double *tiny,
  long *accmode,
  double *accuracy,
  double *accur,
  double x0, double xf,
  long n_eq) {
  int i;

  for (i = 0; i < n_eq; i++) {
    if ((accur[i] = accuracy[i]) <= 0) {
      printf("error: accuracy[%d] = %e (initial_scale_factors_dp)\n", i, accuracy[i]);
      abort();
    }
    switch (accmode[i]) {
    case -1: /* no accuracy control */
      yscale[i] = DBL_MAX;
      break;
    case 0: /* fractional local accuracy specified */
      yscale[i] = (y0[i] + dydx0[i] * h_start + tiny[i]) * accur[i];
      break;
    case 1: /* fractional global accuracy specified */
      yscale[i] = (dydx0[i] * h_start + tiny[i]) * accur[i];
      break;
    case 2: /* absolute local accuracy specified */
      yscale[i] = accur[i];
      break;
    case 3: /* absolute global accuracy specified */
      yscale[i] = (accur[i] /= (xf - x0)) * h_start;
      break;
    default:
      printf("error: accmode[%d] = %ld (initial_scale_factors_dp)\n", i, accmode[i]);
      abort();
      break;
    }
    if (yscale[i] <= 0) {
      printf("error: yscale[%d] = %e (initial_scale_factors_dp)\n", i, yscale[i]);
      abort();
    }
  }
}

void report_state_dp(FILE *fp, double *y, double *dydx, double *yscale, long *misses,
                     double x, double h, long n_eq) {
  int i;
  fputs("integration state:\n", fp);
  fprintf(fp, "%ld equations, indep.var.=%e, step size=%e",
          n_eq, x, h);
  fprintf(fp, "\ny        : ");
  for (i = 0; i < n_eq; i++)
    fprintf(fp, "%e ", y[i]);
  fprintf(fp, "\ndydx     : ");
  for (i = 0; i < n_eq; i++)
    fprintf(fp, "%e ", dydx[i]);
  fprintf(fp, "\ntol.scale: ");
  for (i = 0; i < n_eq; i++)
    fprintf(fp, "%e ", yscale[i]);
  fprintf(fp, "\nmisses   : ");
  for (i = 0; i < n_eq; i++)
    fprintf(fp, "%ld ", misses[i]);
}

/* routine: rk4_step()
 * purpose: advance a system of differential equations by one step using
 *          fourth-order Runge-Kutta.
 */

void rk4_step(
  double *yf,   /* return: final values of the dependent variables */
  double x,     /* initial value of independent the variable */
  double *yi,   /* initial values of the dependent variables */
  double *dydx, /* derivatives at x */
  double h,     /* step size in x */
  long n_eq,    /* number of equations */
  void (*derivs)(double *dydx, double *y, double x)
  /* function to return dy/dx at x for given y */
) {
  static long last_n_eq = 0;
  static double *k1, *k2, *k3, *yTemp, *dydxTemp;
  double x1;
  long i;

  /* for speed, I avoid reallocating the arrays unless it is necessary */
  if (last_n_eq < n_eq) {
    if (last_n_eq != 0) {
      free(k1);
      free(k2);
      free(k3);
      free(yTemp);
      free(dydx);
    }
    last_n_eq = n_eq;
    k1 = tmalloc(sizeof(*k1) * n_eq);
    k2 = tmalloc(sizeof(*k2) * n_eq);
    k3 = tmalloc(sizeof(*k3) * n_eq);
    yTemp = tmalloc(sizeof(*yTemp) * n_eq);
    dydxTemp = tmalloc(sizeof(*dydxTemp) * n_eq);
  }

  /* yf = yi + k1/6 + k2/3 + k3/3 + k4/6 where
       k1 = h*dydt(x    , y)
       k2 = h*dydt(x+h/2, y+k1/2)
       k3 = h*dydt(x+h/2, y+k2/2)
       k4 = h*dydt(x+h  , y+k3)
       */

  /* compute k1 and yTemp for k2 computation */
  for (i = 0; i < n_eq; i++) {
    k1[i] = h * dydx[i];
    yTemp[i] = yi[i] + k1[i] / 2;
  }

  /* compute k2 and yTemp for k3 computation */
  x1 = x + h / 2;
  (*derivs)(dydxTemp, yTemp, x1);
  for (i = 0; i < n_eq; i++) {
    k2[i] = h * dydxTemp[i];
    yTemp[i] = yi[i] + k2[i] / 2;
  }

  /* compute k3 and yTemp for k4 computation */
  (*derivs)(dydxTemp, yTemp, x1);
  for (i = 0; i < n_eq; i++) {
    k3[i] = h * dydxTemp[i];
    yTemp[i] = yi[i] + k3[i];
  }

  /* compute k4 and put results into yf array */
  x1 = x + h;
  (*derivs)(dydxTemp, yTemp, x1);
  for (i = 0; i < n_eq; i++)
    yf[i] = yi[i] + (k1[i] / 2 + k2[i] + k3[i] + h * dydxTemp[i] / 2) / 3;
}

/* routine: rk_qcstep(), rk_qctune()
 * purpose: take/tune a quality controlled Runge-Kutta step 
 * returns: 1 is success, 0 is failure.  
 * based on Numerical Recipes in C
 */

/* default values for quality control as recommended in NRC */
static double safetyMargin = 0.9;
static double increasePower = 0.2;
static double decreasePower = 0.25;
static double maxIncreaseFactor = 4.0;

void rk4_qctune(double newSafetyMargin, double newIncreasePower,
                double newDecreasePower, double newMaxIncreaseFactor) {
  if (newSafetyMargin > 0 && newSafetyMargin < 1)
    safetyMargin = newSafetyMargin;
  if (newIncreasePower > 0)
    increasePower = newIncreasePower;
  if (newDecreasePower > 0)
    decreasePower = newDecreasePower;
  if (newMaxIncreaseFactor > 1)
    maxIncreaseFactor = newMaxIncreaseFactor;
}

long rk_qcstep(
  double *yFinal,       /* final values of the dependent variables */
  double *x,            /* initial value of independent the variable */
  double *yInitial,     /* initial values of the dependent variables */
  double *dydxInitial,  /* derivatives at x */
  double hInput,        /* desired step size in x */
  double *hUsed,        /* step size used */
  double *hRecommended, /* step size recommended for next step */
  double *yScale,       /* allowable absolute error */
  long equations,       /* number of equations */
  void (*derivs)(double *dydx, double *y, double x),
  /* function to return dy/dx at x for given y */
  long *misses /* number of times each variable forces decreasing of
                             step size.  Accumulates between calls if not zeroed 
                             externally */
) {
  static long last_equations = 0;
  static double *dydxTemp, *yTemp;
  double hOver2, h, xTemp;
  double error, maxError, hFactor;
  long i, iWorst = 0, minStepped = 0, noAdaptation = 0;

  /* for speed, I avoid reallocating the arrays unless it is necessary */
  if (last_equations < equations) {
    if (last_equations != 0) {
      tfree(dydxTemp);
      tfree(yTemp);
    }
    last_equations = equations;
    dydxTemp = tmalloc(sizeof(*dydxTemp) * equations);
    yTemp = tmalloc(sizeof(*yTemp) * equations);
  }

  for (i = 0; i < equations; i++)
    if (yScale[i] != DBL_MAX)
      break;
  if (i == equations)
    noAdaptation = 1;

  h = hInput;
  do {
#if DEBUG
    printf("x = %e, h = %e\n", *x, h);
#endif
    hOver2 = h / 2;
    xTemp = *x + hOver2;
    if (xTemp == *x) {
      if (!minStepped) {
        puts("warning: step-size underflow in rk_qcstep()");
        report_state_dp(stdout, yInitial, dydxInitial, yScale, misses, *x, h, equations);
        if ((h = 2 * fabs(*x) * DBL_EPSILON) == 0)
          h = 2 * DBL_EPSILON;
        hOver2 = h / 2;
        xTemp = *x + hOver2;
        minStepped = 1;
      } else
        return 0;
    } else
      minStepped = 1;

    /* get solution via two half-steps */
    /* -- first get solution at x+h/2 into yTemp */
    rk4_step(yTemp, xTemp, yInitial, dydxInitial, hOver2, equations, derivs);

    /* -- get solution at x+h into yFinal, starting from x+h/2 */
    (*derivs)(dydxTemp, yTemp, xTemp);
    xTemp = *x + h;
    rk4_step(yFinal, xTemp, yTemp, dydxTemp, hOver2, equations, derivs);

    /* get solution at x+h into yTemp by taking one step */
    rk4_step(yTemp, xTemp, yInitial, dydxInitial, h, equations, derivs);

    maxError = 0;
    for (i = 0; i < equations; i++)
      yTemp[i] = yFinal[i] - yTemp[i];
    if (!noAdaptation) {
      /* Evaluate accuracy */
      iWorst = -1;
      for (i = 0; i < equations; i++) {
#if DEBUG
        printf("%ld: yTemp=%e  yFinal=%e  yScale=%e, ", i, yTemp[i], yFinal[i], yScale[i]);
#endif
        if (maxError < (error = fabs(yTemp[i]) / yScale[i])) {
          maxError = error;
          iWorst = i;
        }
#if DEBUG
        printf(" error = %e\n", error);
#endif
      }
    }

    if (maxError <= 1) {
      /* step was successful at the following stepsize: */
      *hUsed = h;

      if (!noAdaptation) {
        /* Compute recommended stepsize for next step, but don't increase 
                   by more than maxIncreaseFactor-fold.  Also, use only safetyMargin times
                   the theoretical optimum factor.
                   */
        if (maxError)
          hFactor = safetyMargin * pow(maxError, -increasePower);
        else
          /* error is zero, so go for broke */
          hFactor = maxIncreaseFactor;
#if DEBUG
        printf("maxError = %e, hFactor = %e\n", maxError, hFactor);
#endif
        if (hFactor > maxIncreaseFactor)
          hFactor = maxIncreaseFactor;
        else if (hFactor < 1)
          hFactor = 1;
      } else
        hFactor = 1;

      *hRecommended = hFactor * h;
      /* yTemp stores the two-step solution minus the one-step solution.
               An improved value is obtained by adding 1/15 this difference to
               the one-step solution .
               */
      for (i = 0; i < equations; i++)
        yFinal[i] += yTemp[i] / 15;

      *x = xTemp; /* update value of independent variable */
      return (1);
    }

    if (iWorst >= 0)
      /* record that equation iWorst caused a step-size reduction */
      misses[iWorst] += 1;

    /* compute a new, smaller step-size.  It will be tested for underflow at the
           top of the loop.
           */
    hOver2 = (h = safetyMargin * h * pow(maxError, -decreasePower)) / 2;
  } while (1);
}

/**
 * @brief Integrate a set of ODEs until the upper limit or an exit condition is met.
 *
 * Integrates a system of ordinary differential equations using adaptive step-size
 * control until the independent variable reaches the upper limit or a user-defined
 * exit condition is satisfied.
 *
 * @param y0 Initial and final values of dependent variables.
 * @param derivs Function to compute derivatives.
 * @param n_eq Number of equations.
 * @param accuracy Desired accuracies for each variable.
 * @param accmode Accuracy control modes for each variable.
 * @param tiny Small values relative to what's important for each variable.
 * @param misses Array tracking the number of step size reductions per variable.
 * @param x0 Pointer to the initial value of the independent variable. Updated to final value.
 * @param xf Upper limit of the independent variable.
 * @param x_accuracy Desired accuracy for the final value of the independent variable.
 * @param h_start Suggested starting step size.
 * @param h_max Maximum step size allowed.
 * @param h_rec Pointer to store the recommended step size for continuation.
 * @param exit_func Function to determine when to stop integration based on a condition.
 * @param exit_accuracy Accuracy required for the exit condition.
 * @param n_to_skip Number of zeros of the exit function to skip before returning.
 * @param store_data Function to store intermediate data points.
 * @return DIFFEQ_ZERO_FOUND (>=1) on success, or error code (<=0) on failure.
 */
long rk_odeint(
  double *y0, /* initial/final values of dependent variables */
  /* (*derivs)(dydx, y, x): */
  void (*derivs)(double *dydx, double *y, double x),
  long n_eq, /* number of equations */
  /* for each dependent variable: */
  double *accuracy, /* desired accuracy--see below for meaning */
  long *accmode,    /* desired accuracy-control mode */
  double *tiny,     /* small value relative to what's important */
  long *misses,     /* number of times each variable caused reset
                            of step size */
  /* for the dependent variable: */
  double *x0,        /* initial/final value */
  double xf,         /* upper limit of integration */
  double x_accuracy, /* accuracy of final value */
  double h_start,    /* suggested starting step size */
  double h_max,      /* maximum step size allowed */
  double *h_rec,     /* recommended step size for continuation */
  /* function for determining when to stop integration: */
  double (*exit_func)(double *dydx, double *y, double x),
  double exit_accuracy, /* how close to zero to get */
  long n_to_skip,       /* number of zeros of exit function to skip before
                             returning */
  /* function to store points: */
  void (*store_data)(double *dydx, double *y, double x, double exval)) {
  double *y_return;
  double *dydx0, *y1, *dydx1, *dydx2, *y2;
  double ex0, ex1, ex2, *yscale, *accur;
  double h_used, h_next, x1, x2, xdiff;
  long i, n_exit_iterations, n_step_ups = 0, is_zero;
#define MAX_N_STEP_UPS 10

  if (*x0 > xf)
    return (DIFFEQ_XI_GT_XF);
  if (FABS(*x0 - xf) < x_accuracy)
    return (DIFFEQ_SOLVED_ALREADY);

  /* Meaning of accmode:
     * accmode = -1 -> no accuracy control
     * accmode = 0 -> accuracy[i] is desired fractional accuracy at
     *                each step for ith variable.  tiny[i] is lower limit 
     *                of significance for the ith variable.
     * accmode = 1 -> same as accmode=0, except that the accuracy is to be
     *                satisfied globally, not locally.
     * accmode = 2 -> accuracy[i] is the desired absolute accuracy per
     *                step for the ith variable.  tiny[i] is ignored.
     * accmode = 3 -> samed as accmode=2, except that the accuracy is to 
     *                be satisfied globally, not locally.
     */
  for (i = 0; i < n_eq; i++) {
    if (accmode[i] < -1 || accmode[i] > 3)
      bomb("accmode must be on [-1, 3] (rk_odeint)", NULL);
    if (accmode[i] < 2 && tiny[i] < TINY)
      tiny[i] = TINY;
    misses[i] = 0;
  }

  y_return = y0;
  dydx0 = tmalloc(sizeof(double) * n_eq);
  y1 = tmalloc(sizeof(double) * n_eq);
  dydx1 = tmalloc(sizeof(double) * n_eq);
  y2 = tmalloc(sizeof(double) * n_eq);
  dydx2 = tmalloc(sizeof(double) * n_eq);
  yscale = tmalloc(sizeof(double) * n_eq);

  /* calculate derivatives and exit function at the initial point */
  (*derivs)(dydx0, y0, *x0);

  /* set the scales for evaluating accuracy.  yscale[i] is the
     * absolute level of accuracy required of the next integration step.
     */
  accur = tmalloc(sizeof(double) * n_eq);
  initial_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                           accuracy, accur, *x0, xf, n_eq);

  ex0 = exit_func ? (*exit_func)(dydx0, y0, *x0) : 0;
  if (store_data)
    (*store_data)(dydx0, y0, *x0, ex0);
  is_zero = 0;

  do {
    /* check for zero of exit function */
    if (exit_func && FABS(ex0) < exit_accuracy) {
      if (!is_zero) {
        if (n_to_skip == 0) {
          if (store_data)
            (*store_data)(dydx0, y0, *x0, ex0);
          for (i = 0; i < n_eq; i++)
            y_return[i] = y0[i];
          *h_rec = h_start;
          tfree(dydx0);
          tfree(dydx1);
          tfree(dydx2);
          tfree(yscale);
          tfree(accur);
          if (y0 != y_return)
            tfree(y0);
          if (y1 != y_return)
            tfree(y1);
          if (y2 != y_return)
            tfree(y2);
          return (DIFFEQ_ZERO_FOUND);
        } else {
          is_zero = 1;
          --n_to_skip;
        }
      }
    } else
      is_zero = 0;
    /* adjust step size to stay within interval */
    if ((xdiff = xf - *x0) < h_start)
      h_start = xdiff;
    /* take a step */
    x1 = *x0;
    if (!rk_qcstep(y1, &x1, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses)) {
      if (n_step_ups++ > MAX_N_STEP_UPS)
        bomb("cannot take initial step (rk_odeint--1)", NULL);
      h_start = (n_step_ups - 1 ? h_start * 10 : h_used * 10);
      continue;
    }
    /* calculate derivatives and exit function at new point */
    (*derivs)(dydx1, y1, x1);
    ex1 = exit_func ? (*exit_func)(dydx1, y1, x1) : 0;
    if (store_data)
      (*store_data)(dydx1, y1, x1, ex1);
    /* check for change in sign of exit function */
    if (exit_func && SIGN(ex0) != SIGN(ex1) && !is_zero) {
      if (n_to_skip == 0)
        break;
      else {
        --n_to_skip;
        is_zero = 1;
      }
    }

    /* check for end of interval */
    if (FABS(xdiff = xf - x1) < x_accuracy) {
      /* end of the interval */
      if (store_data) {
        (*derivs)(dydx1, y1, x1);
        ex1 = exit_func ? (*exit_func)(dydx1, y1, x1) : 0;
        (*store_data)(dydx1, y1, x1, ex1);
      }
      for (i = 0; i < n_eq; i++)
        y_return[i] = y1[i];
      *x0 = x1;
      *h_rec = h_start;
      tfree(dydx0);
      tfree(dydx1);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
      if (y0 != y_return)
        tfree(y0);
      if (y1 != y_return)
        tfree(y1);
      if (y2 != y_return)
        tfree(y2);
      return (DIFFEQ_END_OF_INTERVAL);
    }

    /* copy the new solution into the old variables */
    SWAP_PTR(dydx0, dydx1);
    SWAP_PTR(y0, y1);
    ex0 = ex1;
    *x0 = x1;
    /* adjust the step size as recommended by rk_qcstep() */
    h_start = (h_next > h_max ? (h_max ? h_max : h_next) : h_next);
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);

  } while (1);
  *h_rec = h_start;

  if (!exit_func) {
    printf("failure in rk_odeint():  solution stepped outside interval\n");
    tfree(dydx0);
    tfree(dydx1);
    tfree(dydx2);
    tfree(yscale);
    tfree(accur);
    if (y0 != y_return)
      tfree(y0);
    if (y1 != y_return)
      tfree(y1);
    if (y2 != y_return)
      tfree(y2);
    return (DIFFEQ_OUTSIDE_INTERVAL);
  }

  /* The root has been bracketed. */
  n_exit_iterations = MAX_EXIT_ITERATIONS;
  do {
    /* try to take a step to the position where the zero is expected */
    h_start = -ex0 * (x1 - *x0) / (ex1 - ex0) * ITER_FACTOR;
    x2 = *x0;
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);
    if (!rk_qcstep(y2, &x2, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses))
      bomb("step size too small (rk_odeint--2)", NULL);
    /* check the exit function at the new position */
    (*derivs)(dydx2, y2, x2);
    ex2 = (*exit_func)(dydx2, y2, x2);
    if (FABS(ex2) < exit_accuracy) {
      for (i = 0; i < n_eq; i++)
        y_return[i] = y2[i];
      *x0 = x2;
      tfree(dydx0);
      tfree(dydx1);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
      if (y0 != y_return)
        tfree(y0);
      if (y1 != y_return)
        tfree(y1);
      if (y2 != y_return)
        tfree(y2);
      return (DIFFEQ_ZERO_FOUND);
    }
    /* rebracket the root */
    if (SIGN(ex1) == SIGN(ex2)) {
      SWAP_PTR(y1, y2);
      SWAP_PTR(dydx1, dydx2);
      x1 = x2;
      ex1 = ex2;
    } else {
      SWAP_PTR(y0, y2);
      SWAP_PTR(dydx0, dydx2);
      *x0 = x2;
      ex0 = ex2;
    }
  } while (n_exit_iterations--);
  return (DIFFEQ_EXIT_COND_FAILED);
}

/**
 * @brief Integrate ODEs without exit conditions or intermediate output.
 *
 * Integrates a system of ordinary differential equations until the upper limit
 * of the independent variable is reached. This function does not monitor exit
 * conditions or store intermediate data, making it faster for simple integrations.
 *
 * @param y0 Initial and final values of dependent variables.
 * @param derivs Function to compute derivatives.
 * @param n_eq Number of equations.
 * @param accuracy Ignored in this function.
 * @param accmode Ignored in this function.
 * @param tiny Ignored in this function.
 * @param misses Ignored in this function.
 * @param x0 Pointer to the initial value of the independent variable. Updated to final value.
 * @param xf Upper limit of the independent variable.
 * @param x_accuracy Ignored in this function.
 * @param h_start Step size.
 * @param h_max Maximum step size allowed.
 * @param h_rec Pointer to store the recommended step size for continuation.
 * @return DIFFEQ_END_OF_INTERVAL (1) on success, or an error code (<=0) on failure.
 */
long rk_odeint1(
  double *y0, /* initial/final values of dependent variables */
  /* (*derivs)(dydx, y, x): */
  void (*derivs)(double *dydx, double *y, double x),
  long n_eq, /* number of equations */
  /* for each dependent variable: */
  double *accuracy, /* desired accuracy--see below for meaning */
  long *accmode,    /* desired accuracy-control mode */
  double *tiny,     /* small value relative to what's important */
  long *misses,     /* number of times each variable caused reset
                            of step size */
  /* for the dependent variable: */
  double *x0,        /* initial/final value */
  double xf,         /* upper limit of integration */
  double x_accuracy, /* accuracy of final value */
  double h_start,    /* suggested starting step size */
  double h_max,      /* maximum step size allowed */
  double *h_rec      /* recommended step size for continuation */
) {
  double *y_return;
  double *dydx0, *y1, *dydx1, *dydx2, *y2;
  double *yscale, *accur;
  double x1;
  double h_used, h_next, xdiff;
  long i, n_step_ups = 0;
#define MAX_N_STEP_UPS 10

  if (*x0 > xf)
    return (DIFFEQ_XI_GT_XF);
  if (FABS(*x0 - xf) < x_accuracy)
    return (DIFFEQ_SOLVED_ALREADY);

  /* Meaning of accmode:
     * accmode = -1 -> no accuracy control
     * accmode = 0 -> accuracy[i] is desired fractional accuracy at
     *                each step for ith variable.  tiny[i] is lower limit 
     *                of significance for the ith variable.
     * accmode = 1 -> same as accmode=0, except that the accuracy is to be
     *                satisfied globally, not locally.
     * accmode = 2 -> accuracy[i] is the desired absolute accuracy per
     *                step for the ith variable.  tiny[i] is ignored.
     * accmode = 3 -> samed as accmode=2, except that the accuracy is to 
     *                be satisfied globally, not locally.
     */
  for (i = 0; i < n_eq; i++) {
    if (accmode[i] < -1 || accmode[i] > 3)
      bomb("accmode must be on [-1, 3] (rk_odeint)", NULL);
    if (accmode[i] < 2 && tiny[i] < TINY)
      tiny[i] = TINY;
    misses[i] = 0;
  }

  y_return = y0;
  dydx0 = tmalloc(sizeof(double) * n_eq);
  y1 = tmalloc(sizeof(double) * n_eq);
  dydx1 = tmalloc(sizeof(double) * n_eq);
  y2 = tmalloc(sizeof(double) * n_eq);
  dydx2 = tmalloc(sizeof(double) * n_eq);
  yscale = tmalloc(sizeof(double) * n_eq);

  /* calculate derivatives at the initial point */
  (*derivs)(dydx0, y0, *x0);

  /* set the scales for evaluating accuracy.  yscale[i] is the
     * absolute level of accuracy required of the next integration step
     */
  accur = tmalloc(sizeof(double) * n_eq);
  initial_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                           accuracy, accur, *x0, xf, n_eq);

  do {
    /* adjust step size to stay within interval */
    if ((xdiff = xf - *x0) < h_start)
      h_start = xdiff;
    /* take a step */
    x1 = *x0;
    if (!rk_qcstep(y1, &x1, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses)) {
      if (n_step_ups++ > MAX_N_STEP_UPS) {
        puts("error: cannot take step (rk_odeint1--1)");
        printf("xf = %.16e  x0 = %.16e\n", xf, *x0);
        printf("h_start = %.16e  h_used = %.16e\n", h_start, h_used);
        puts("dump of integration state:");
        puts(" variable      value       derivative        scale      misses");
        puts("---------------------------------------------------------------");
        for (i = 0; i < n_eq; i++)
          printf("  %5ld    %13.6e  %13.6e  %13.6e  %5ld  \n",
                 i, y0[i], dydx0[i], yscale[i], misses[i]);
        exit(1);
      }
      h_start = (n_step_ups - 1 ? h_start * 10 : h_used * 10);
      continue;
    }
    /* check for end of interval */
    if (FABS(xdiff = xf - x1) < x_accuracy) {
      /* end of the interval */
      for (i = 0; i < n_eq; i++)
        y_return[i] = y1[i];
      *x0 = x1;
      *h_rec = h_start;
      tfree(dydx0);
      tfree(dydx1);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
      if (y0 != y_return)
        tfree(y0);
      if (y1 != y_return)
        tfree(y1);
      if (y2 != y_return)
        tfree(y2);
      return (DIFFEQ_END_OF_INTERVAL);
    }
    /* calculate derivatives at new point */
    (*derivs)(dydx1, y1, x1);
    /* copy the new solution into the old variables */
    SWAP_PTR(dydx0, dydx1);
    SWAP_PTR(y0, y1);
    *x0 = x1;
    /* adjust the step size as recommended by rk_qcstep() */
    h_start = (h_next > h_max ? (h_max ? h_max : h_next) : h_next);
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);
  } while (1);
}

/**
 * @brief Integrate ODEs until a specific component reaches a target value.
 *
 * Integrates a system of ordinary differential equations until a specified
 * component of the solution reaches a target value within a given accuracy.
 * This function does not monitor general exit conditions or store intermediate data.
 *
 * @param y0 Initial and final values of dependent variables.
 * @param derivs Function to compute derivatives.
 * @param n_eq Number of equations.
 * @param accuracy Desired accuracies for each variable.
 * @param accmode Accuracy control modes for each variable.
 * @param tiny Small values relative to what's important for each variable.
 * @param misses Array tracking the number of step size reductions per variable.
 * @param x0 Pointer to the initial value of the independent variable. Updated to final value.
 * @param xf Upper limit of the independent variable.
 * @param x_accuracy Desired accuracy for the final value of the independent variable.
 * @param h_start Suggested starting step size.
 * @param h_max Maximum step size allowed.
 * @param h_rec Pointer to store the recommended step size for continuation.
 * @param exit_value Target value for the specified component.
 * @param i_exit_value Index of the component to monitor.
 * @param exit_accuracy Accuracy required for the target value.
 * @param n_to_skip Number of target value crossings to skip before stopping.
 * @return DIFFEQ_ZERO_FOUND (1) on success, or an error code (<=0) on failure.
 */
long rk_odeint2(
  double *y0, /* initial/final values of dependent variables */
  /* (*derivs)(dydx, y, x): */
  void (*derivs)(double *dydx, double *y, double x),
  long n_eq, /* number of equations */
  /* for each dependent variable: */
  double *accuracy, /* desired accuracy--see below for meaning */
  long *accmode,    /* desired accuracy-control mode */
  double *tiny,     /* small value relative to what's important */
  long *misses,     /* number of times each variable caused reset
                            of step size */
  /* for the dependent variable: */
  double *x0,        /* initial/final value */
  double xf,         /* upper limit of integration */
  double x_accuracy, /* accuracy of final value */
  double h_start,    /* suggested starting step size */
  double h_max,      /* maximum step size allowed */
  double *h_rec,     /* recommended step size for continuation */
  /* for determining when to stop integration: */
  double exit_value,    /* value to be obtained */
  long i_exit_value,    /* index of independent variable this pertains to */
  double exit_accuracy, /* how close to get */
  long n_to_skip        /* number of zeros to skip before returning */
) {
  double *y_return, *accur;
  double *dydx0, *y1, *dydx1, *dydx2, *y2;
  double ex0, ex1, ex2, x1, x2, *yscale;
  double h_used, h_next, xdiff;
  long i, n_exit_iterations, n_step_ups = 0, is_zero;
#define MAX_N_STEP_UPS 10

  if (*x0 > xf)
    return (DIFFEQ_XI_GT_XF);
  if (FABS(*x0 - xf) < x_accuracy)
    return (DIFFEQ_SOLVED_ALREADY);
  if (i_exit_value < 0 || i_exit_value >= n_eq)
    bomb("index of variable for exit testing is out of range (rk_odeint2)", NULL);

  /* Meaning of accmode:
     * accmode = -1 -> no accuracy control
     * accmode = 0 -> accuracy[i] is desired fractional accuracy at
     *                each step for ith variable.  tiny[i] is lower limit 
     *                of significance for the ith variable.
     * accmode = 1 -> same as accmode=0, except that the accuracy is to be
     *                satisfied globally, not locally.
     * accmode = 2 -> accuracy[i] is the desired absolute accuracy per
     *                step for the ith variable.  tiny[i] is ignored.
     * accmode = 3 -> samed as accmode=2, except that the accuracy is to 
     *                be satisfied globally, not locally.
     */
  for (i = 0; i < n_eq; i++) {
    if (accmode[i] < -1 || accmode[i] > 3)
      bomb("accmode must be on [-1, 3] (rk_odeint2)", NULL);
    if (accmode[i] < 2 && tiny[i] < TINY)
      tiny[i] = TINY;
    misses[i] = 0;
  }

  y_return = y0;
  dydx0 = tmalloc(sizeof(double) * n_eq);
  y1 = tmalloc(sizeof(double) * n_eq);
  dydx1 = tmalloc(sizeof(double) * n_eq);
  y2 = tmalloc(sizeof(double) * n_eq);
  dydx2 = tmalloc(sizeof(double) * n_eq);
  yscale = tmalloc(sizeof(double) * n_eq);

  /* calculate derivatives and exit function at the initial point */
  (*derivs)(dydx0, y0, *x0);

  /* set the scales for evaluating accuracy.  yscale[i] is the
     * absolute level of accuracy required of the next integration step
     */
  accur = tmalloc(sizeof(double) * n_eq);
  initial_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                           accuracy, accur, *x0, xf, n_eq);

  ex0 = exit_value - y0[i_exit_value];
  is_zero = 0;
  do {
    /* check for zero of exit function */
    if (FABS(ex0) < exit_accuracy) {
      if (!is_zero) {
        if (n_to_skip == 0) {
          for (i = 0; i < n_eq; i++)
            y_return[i] = y0[i];
          *h_rec = h_start;
          tfree(dydx0);
          tfree(dydx1);
          tfree(dydx2);
          tfree(yscale);
          tfree(accur);
          if (y0 != y_return)
            tfree(y0);
          if (y1 != y_return)
            tfree(y1);
          if (y2 != y_return)
            tfree(y2);
          return (DIFFEQ_ZERO_FOUND);
        } else {
          is_zero = 1;
          --n_to_skip;
        }
      }
    } else
      is_zero = 0;
    /* adjust step size to stay within interval */
    if ((xdiff = xf - *x0) < h_start)
      h_start = xdiff;
    /* take a step */
    x1 = *x0;
    if (!rk_qcstep(y1, &x1, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses)) {
      if (n_step_ups++ > MAX_N_STEP_UPS) {
        bomb("error: cannot take initial step (rk_odeint2--1)", NULL);
      }
      h_start = (n_step_ups - 1 ? h_start * 10 : h_used * 10);
      continue;
    }
    /* calculate derivatives and exit function at new point */
    (*derivs)(dydx1, y1, x1);
    ex1 = exit_value - y1[i_exit_value];
    /* check for change in sign of exit function */
    if (SIGN(ex0) != SIGN(ex1) && !is_zero) {
      if (n_to_skip == 0)
        break;
      else {
        --n_to_skip;
        is_zero = 1;
      }
    }
    /* check for end of interval */
    if (FABS(xdiff = xf - x1) < x_accuracy) {
      /* end of the interval */
      for (i = 0; i < n_eq; i++)
        y_return[i] = y1[i];
      *x0 = x1;
      *h_rec = h_start;
      tfree(dydx0);
      tfree(dydx1);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
      if (y0 != y_return)
        tfree(y0);
      if (y1 != y_return)
        tfree(y1);
      if (y2 != y_return)
        tfree(y2);
      return (DIFFEQ_END_OF_INTERVAL);
    }
    /* copy the new solution into the old variables */
    SWAP_PTR(dydx0, dydx1);
    SWAP_PTR(y0, y1);
    ex0 = ex1;
    *x0 = x1;
    /* adjust the step size as recommended by rk_qcstep() */
    h_start = (h_next > h_max ? (h_max ? h_max : h_next) : h_next);
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);

  } while (1);
  *h_rec = h_start;

  /* The root has been bracketed. */
  n_exit_iterations = MAX_EXIT_ITERATIONS;
  do {
    /* try to take a step to the position where the zero is expected */
    h_start = -ex0 * (x1 - *x0) / (ex1 - ex0) * ITER_FACTOR;
    x2 = *x0;
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);
    if (!rk_qcstep(y2, &x2, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses))
      bomb("step size too small (rk_odeint2--2)", NULL);
    /* check the exit function at the new position */
    (*derivs)(dydx2, y2, x2);
    ex2 = exit_value - y2[i_exit_value];
    if (FABS(ex2) < exit_accuracy) {
      for (i = 0; i < n_eq; i++)
        y_return[i] = y2[i];
      *x0 = x2;
      tfree(dydx0);
      tfree(dydx1);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
      if (y0 != y_return)
        tfree(y0);
      if (y1 != y_return)
        tfree(y1);
      if (y2 != y_return)
        tfree(y2);
      return (DIFFEQ_ZERO_FOUND);
    }
    /* rebracket the root */
    if (SIGN(ex1) == SIGN(ex2)) {
      SWAP_PTR(y1, y2);
      SWAP_PTR(dydx1, dydx2);
      x1 = x2;
      ex1 = ex2;
    } else {
      SWAP_PTR(y0, y2);
      SWAP_PTR(dydx0, dydx2);
      *x0 = x2;
      ex0 = ex2;
    }
  } while (n_exit_iterations--);
  return (DIFFEQ_EXIT_COND_FAILED);
}

/**
 * @brief Integrate ODEs without adaptive step-size control.
 *
 * Integrates a system of ordinary differential equations without using
 * adaptive step-size control. This function is intended for scenarios where
 * adaptive control is not required and allows for faster computations.
 *
 * @param y0 Initial and final values of dependent variables.
 * @param derivs Function to compute derivatives.
 * @param n_eq Number of equations.
 * @param accuracy Ignored in this function.
 * @param accmode Ignored in this function.
 * @param tiny Ignored in this function.
 * @param misses Ignored in this function.
 * @param x0 Pointer to the initial value of the independent variable. Updated to final value.
 * @param xf Upper limit of the independent variable.
 * @param x_accuracy Ignored in this function.
 * @param h Step size.
 * @param h_max Ignored in this function.
 * @param h_rec Ignored in this function.
 * @return DIFFEQ_END_OF_INTERVAL (1) on success, or an error code (<=0) on failure.
 */
long rk_odeint_na(
  double *y0, /* initial/final values of dependent variables */
  /* (*derivs)(dydx, y, x): */
  void (*derivs)(double *dydx, double *y, double x),
  long n_eq, /* number of equations */
  /* for each dependent variable: */
  double *accuracy, /* ignored */
  long *accmode,    /* ignored */
  double *tiny,     /* ignored */
  long *misses,     /* ignored */
  /* for the dependent variable: */
  double *x0,        /* initial/final value */
  double xf,         /* upper limit of integration */
  double x_accuracy, /* ignored */
  double h,          /* step size */
  double h_max,      /* ignored */
  double *h_rec      /* ignored */
) {
  double *dydx2, *dydx1, *ytemp, *dydx0;
  double xh, hh, h6, x;
  long i, j, n;

  if ((x = *x0) > xf)
    return (DIFFEQ_XI_GT_XF);
  if (h == 0)
    return (DIFFEQ_ZERO_STEPSIZE);
  if (!(n = (xf - x) / h + 0.5))
    n = 1;
  h = (xf - x) / n;

  dydx0 = tmalloc(sizeof(double) * n_eq);
  dydx2 = tmalloc(sizeof(double) * n_eq);
  dydx1 = tmalloc(sizeof(double) * n_eq);
  ytemp = tmalloc(sizeof(double) * n_eq);

  n_eq--; /* for convenience in loops */

  h6 = h / 6;
  hh = h / 2;
  for (j = 0; j < n; j++) {
    if (j == n - 1) {
      h = xf - x;
      h6 = h / 6;
      hh = h / 2;
    }
    xh = x + hh;

    /* first step */
    (*derivs)(dydx0, y0, x);
    for (i = n_eq; i >= 0; i--)
      ytemp[i] = y0[i] + hh * dydx0[i];

    /* second step */
    (*derivs)(dydx1, ytemp, xh);
    for (i = n_eq; i >= 0; i--)
      ytemp[i] = y0[i] + hh * dydx1[i];

    /* third step */
    (*derivs)(dydx2, ytemp, xh);
    for (i = n_eq; i >= 0; i--) {
      ytemp[i] = y0[i] + h * dydx2[i];
      dydx2[i] += dydx1[i];
    }

    /* fourth step */
    (*derivs)(dydx1, ytemp, x = xh + hh);
    for (i = n_eq; i >= 0; i--)
      y0[i] += h6 * (dydx0[i] + dydx1[i] + 2 * dydx2[i]);
  }

  tfree(dydx0);
  tfree(dydx2);
  tfree(dydx1);
  tfree(ytemp);
  *x0 = x;
  return (DIFFEQ_END_OF_INTERVAL);
}

/**
 * @brief Integrate ODEs with an exit condition.
 *
 * Integrates a system of ordinary differential equations until the upper limit
 * of the independent variable is reached or a user-defined exit condition is satisfied.
 *
 * @param yif Initial and final values of dependent variables.
 * @param derivs Function to compute derivatives.
 * @param n_eq Number of equations.
 * @param accuracy Desired accuracies for each variable.
 * @param accmode Accuracy control modes for each variable.
 * @param tiny Small values relative to what's important for each variable.
 * @param misses Array tracking the number of step size reductions per variable.
 * @param x0 Pointer to the initial value of the independent variable. Updated to final value.
 * @param xf Upper limit of the independent variable.
 * @param x_accuracy Desired accuracy for the final value of the independent variable.
 * @param h_start Suggested starting step size.
 * @param h_max Maximum step size allowed.
 * @param h_rec Pointer to store the recommended step size for continuation.
 * @param exit_func Function to determine when to stop integration based on a condition.
 * @param exit_accuracy Accuracy required for the exit condition.
 * @return DIFFEQ_ZERO_FOUND (>=1) on success, or an error code (<=0) on failure.
 */
long rk_odeint3(
  double *yif,                                       /* initial/final values of dependent variables */
  void (*derivs)(double *dydx, double *y, double x), /* (*derivs)(dydx, y, x) */
  long n_eq,                                         /* number of equations */
  /* for each dependent variable: */
  double *accuracy, /* desired accuracy--see below for meaning */
  long *accmode,    /* desired accuracy-control mode */
  double *tiny,     /* small value relative to what's important */
  long *misses,     /* number of times each variable caused reset
                            of step size */
  /* for the dependent variable: */
  double *x0,        /* initial/final value */
  double xf,         /* upper limit of integration */
  double x_accuracy, /* accuracy of final value */
  double h_start,    /* suggested starting step size */
  double h_max,      /* maximum step size allowed */
  double *h_rec,     /* recommended step size for continuation */
  /* function for determining when to stop integration: */
  double (*exit_func)(double *dydx, double *y, double x),
  /* function that is to be zeroed */
  double exit_accuracy /* how close to zero to get */
) {
  static double *y0, *yscale;
  static double *dydx0, *y1, *dydx1, *dydx2, *y2, *accur;
  static long last_neq = 0;
  double ex0, ex1, ex2, x1, x2;
  double h_used, h_next, xdiff;
  long i, n_exit_iterations, n_step_ups = 0;
#define MAX_N_STEP_UPS 10

  if (*x0 > xf)
    return (DIFFEQ_XI_GT_XF);
  if (FABS(*x0 - xf) < x_accuracy)
    return (DIFFEQ_SOLVED_ALREADY);

  /* Meaning of accmode:
     * accmode = -1 -> no accuracy control
     * accmode = 0 -> accuracy[i] is desired fractional accuracy at
     *                each step for ith variable.  tiny[i] is lower limit 
     *                of significance for the ith variable.
     * accmode = 1 -> same as accmode=0, except that the accuracy is to be
     *                satisfied globally, not locally.
     * accmode = 2 -> accuracy[i] is the desired absolute accuracy per
     *                step for the ith variable.  tiny[i] is ignored.
     * accmode = 3 -> samed as accmode=2, except that the accuracy is to 
     *                be satisfied globally, not locally.
     */
  for (i = 0; i < n_eq; i++) {
    if (accmode[i] < -1 || accmode[i] > 3)
      bomb("accmode must be on [-1, 3] (rk_odeint)", NULL);
    if (accmode[i] < 2 && tiny[i] < TINY)
      tiny[i] = TINY;
    misses[i] = 0;
  }

  if (last_neq < n_eq) {
    if (last_neq != 0) {
      tfree(y0);
      tfree(dydx0);
      tfree(y1);
      tfree(dydx1);
      tfree(y2);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
    }
    y0 = tmalloc(sizeof(double) * n_eq);
    dydx0 = tmalloc(sizeof(double) * n_eq);
    y1 = tmalloc(sizeof(double) * n_eq);
    dydx1 = tmalloc(sizeof(double) * n_eq);
    y2 = tmalloc(sizeof(double) * n_eq);
    dydx2 = tmalloc(sizeof(double) * n_eq);
    yscale = tmalloc(sizeof(double) * n_eq);
    accur = tmalloc(sizeof(double) * n_eq);
    last_neq = n_eq;
  }

  for (i = 0; i < n_eq; i++)
    y0[i] = yif[i];

  /* calculate derivatives and exit function at the initial point */
  (*derivs)(dydx0, y0, *x0);

  /* set the scales for evaluating accuracy.  yscale[i] is the
     * absolute level of accuracy required of the next integration step
     */
  initial_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                           accuracy, accur, *x0, xf, n_eq);

  ex0 = (*exit_func)(dydx0, y0, *x0);

  do {
    /* check for zero of exit function */
    if (FABS(ex0) < exit_accuracy) {
      for (i = 0; i < n_eq; i++)
        yif[i] = y0[i];
      *h_rec = h_start;
      return (DIFFEQ_ZERO_FOUND);
    }

    /* adjust step size to stay within interval */
    if ((xdiff = xf - *x0) < h_start)
      h_start = xdiff;
    /* take a step */
    x1 = *x0;
    if (!rk_qcstep(y1, &x1, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses)) {
      if (n_step_ups++ > MAX_N_STEP_UPS)
        bomb("error: cannot take initial step (rk_odeint3--1)", NULL);
      h_start = (n_step_ups - 1 ? h_start * 10 : h_used * 10);
      continue;
    }
    /* calculate derivatives and exit function at new point */
    (*derivs)(dydx1, y1, x1);
    ex1 = (*exit_func)(dydx1, y1, x1);
    if (SIGN(ex0) != SIGN(ex1))
      break;
    /* check for end of interval */
    if (FABS(xdiff = xf - x1) < x_accuracy) {
      /* end of the interval */
      for (i = 0; i < n_eq; i++)
        yif[i] = y1[i];
      *x0 = x1;
      *h_rec = h_start;
      return (DIFFEQ_END_OF_INTERVAL);
    }
    /* copy the new solution into the old variables */
    SWAP_PTR(dydx0, dydx1);
    SWAP_PTR(y0, y1);
    ex0 = ex1;
    *x0 = x1;
    /* adjust the step size as recommended by rk_qcstep() */
    h_start = (h_next > h_max ? (h_max ? h_max : h_next) : h_next);
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);
  } while (1);
  *h_rec = h_start;

  if (!exit_func) {
    printf("failure in rk_odeint3():  solution stepped outside interval\n");
    return (DIFFEQ_OUTSIDE_INTERVAL);
  }

  if (FABS(ex1) < exit_accuracy) {
    for (i = 0; i < n_eq; i++)
      yif[i] = y1[i];
    *x0 = x1;
    return (DIFFEQ_ZERO_FOUND);
  }

  /* The root has been bracketed. */
  n_exit_iterations = MAX_EXIT_ITERATIONS;
  do {
    /* try to take a step to the position where the zero is expected */
    h_start = -ex0 * (x1 - *x0) / (ex1 - ex0) * ITER_FACTOR;
    x2 = *x0;
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);
    if (!rk_qcstep(y2, &x2, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses))
      bomb("step size too small (rk_odeint3--2)", NULL);
    /* check the exit function at the new position */
    (*derivs)(dydx2, y2, x2);
    ex2 = (*exit_func)(dydx2, y2, x2);
    if (FABS(ex2) < exit_accuracy) {
      for (i = 0; i < n_eq; i++)
        yif[i] = y2[i];
      *x0 = x2;
      return (DIFFEQ_ZERO_FOUND);
    }
    /* rebracket the root */
    if (SIGN(ex1) == SIGN(ex2)) {
      SWAP_PTR(y1, y2);
      SWAP_PTR(dydx1, dydx2);
      x1 = x2;
      ex1 = ex2;
    } else {
      SWAP_PTR(y0, y2);
      SWAP_PTR(dydx0, dydx2);
      *x0 = x2;
      ex0 = ex2;
    }
  } while (n_exit_iterations--);
  return (DIFFEQ_EXIT_COND_FAILED);
}

/**
 * @brief Integrate ODEs until a specific component reaches a target value with intermediate storage.
 *
 * Integrates a system of ordinary differential equations until a specified
 * component of the solution reaches a target value within a given accuracy.
 * Allows for storing intermediate data points during the integration process.
 *
 * @param y0 Initial and final values of dependent variables.
 * @param derivs Function to compute derivatives.
 * @param n_eq Number of equations.
 * @param accuracy Desired accuracies for each variable.
 * @param accmode Accuracy control modes for each variable.
 * @param tiny Small values relative to what's important for each variable.
 * @param misses Array tracking the number of step size reductions per variable.
 * @param x0 Pointer to the initial value of the independent variable. Updated to final value.
 * @param xf Upper limit of the independent variable.
 * @param x_accuracy Desired accuracy for the final value of the independent variable.
 * @param h_start Suggested starting step size.
 * @param h_max Maximum step size allowed.
 * @param h_rec Pointer to store the recommended step size for continuation.
 * @param exit_value Target value for the specified component.
 * @param i_exit_value Index of the component to monitor.
 * @param exit_accuracy Accuracy required for the target value.
 * @param n_to_skip Number of target value crossings to skip before stopping.
 * @param store_data Function to store intermediate data points.
 * @return DIFFEQ_ZERO_FOUND (>=1) on success, or an error code (<=0) on failure.
 */
long rk_odeint4(
  double *y0, /* initial/final values of dependent variables */
  /* (*derivs)(dydx, y, x): */
  void (*derivs)(double *dydx, double *y, double x),
  long n_eq, /* number of equations */
  /* for each dependent variable: */
  double *accuracy, /* desired accuracy--see below for meaning */
  long *accmode,    /* desired accuracy-control mode */
  double *tiny,     /* small value relative to what's important */
  long *misses,     /* number of times each variable caused reset
                            of step size */
  /* for the dependent variable: */
  double *x0,        /* initial/final value */
  double xf,         /* upper limit of integration */
  double x_accuracy, /* accuracy of final value */
  double h_start,    /* suggested starting step size */
  double h_max,      /* maximum step size allowed */
  double *h_rec,     /* recommended step size for continuation */
  /* for determining when to stop integration: */
  double exit_value,                                                /* value to be obtained */
  long i_exit_value,                                                /* index of independent variable this pertains to */
  double exit_accuracy,                                             /* how close to get */
  long n_to_skip,                                                   /* number of zeros to skip before returning */
  void (*store_data)(double *dydx, double *y, double x, double exf) /* function to store points */
) {
  double *y_return, *accur;
  double *dydx0, *y1, *dydx1, *dydx2, *y2;
  double ex0, ex1, ex2, x1, x2, *yscale;
  double h_used, h_next, xdiff;
  long i, n_exit_iterations, n_step_ups = 0, is_zero;
#define MAX_N_STEP_UPS 10

  if (*x0 > xf)
    return (DIFFEQ_XI_GT_XF);
  if (fabs(*x0 - xf) < x_accuracy)
    return (DIFFEQ_SOLVED_ALREADY);
  if (i_exit_value < 0 || i_exit_value >= n_eq)
    bomb("index of variable for exit testing is out of range (rk_odeint4)", NULL);

  /* Meaning of accmode:
     * accmode = -1 -> no accuracy control
     * accmode = 0 -> accuracy[i] is desired fractional accuracy at
     *                each step for ith variable.  tiny[i] is lower limit 
     *                of significance for the ith variable.
     * accmode = 1 -> same as accmode=0, except that the accuracy is to be
     *                satisfied globally, not locally.
     * accmode = 2 -> accuracy[i] is the desired absolute accuracy per
     *                step for the ith variable.  tiny[i] is ignored.
     * accmode = 3 -> samed as accmode=2, except that the accuracy is to 
     *                be satisfied globally, not locally.
     */
  for (i = 0; i < n_eq; i++) {
    if (accmode[i] < -1 || accmode[i] > 3)
      bomb("accmode must be on [-1, 3] (rk_odeint4)", NULL);
    if (accmode[i] < 2 && tiny[i] < TINY)
      tiny[i] = TINY;
    misses[i] = 0;
  }

  y_return = y0;
  dydx0 = tmalloc(sizeof(double) * n_eq);
  y1 = tmalloc(sizeof(double) * n_eq);
  dydx1 = tmalloc(sizeof(double) * n_eq);
  y2 = tmalloc(sizeof(double) * n_eq);
  dydx2 = tmalloc(sizeof(double) * n_eq);
  yscale = tmalloc(sizeof(double) * n_eq);

  /* calculate derivatives and exit function at the initial point */
  (*derivs)(dydx0, y0, *x0);

  /* set the scales for evaluating accuracy.  yscale[i] is the
     * absolute level of accuracy required of the next integration step
     */
  accur = tmalloc(sizeof(double) * n_eq);
  initial_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                           accuracy, accur, *x0, xf, n_eq);

  ex0 = exit_value - y0[i_exit_value];
  if (store_data)
    (*store_data)(dydx0, y0, *x0, ex0);
  is_zero = 0;
  do {
    /* check for zero of exit function */
    if (fabs(ex0) < exit_accuracy) {
      if (!is_zero) {
        if (n_to_skip == 0) {
          if (store_data)
            (*store_data)(dydx0, y0, *x0, ex0);
          for (i = 0; i < n_eq; i++)
            y_return[i] = y0[i];
          *h_rec = h_start;
          tfree(dydx0);
          tfree(dydx1);
          tfree(dydx2);
          tfree(yscale);
          tfree(accur);
          if (y0 != y_return)
            tfree(y0);
          if (y1 != y_return)
            tfree(y1);
          if (y2 != y_return)
            tfree(y2);
          return (DIFFEQ_ZERO_FOUND);
        } else {
          is_zero = 1;
          --n_to_skip;
        }
      }
    } else
      is_zero = 0;
    /* adjust step size to stay within interval */
    if ((xdiff = xf - *x0) < h_start)
      h_start = xdiff;
    /* take a step */
    x1 = *x0;
    if (!rk_qcstep(y1, &x1, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses)) {
      if (n_step_ups++ > MAX_N_STEP_UPS) {
        bomb("error: cannot take initial step (rk_odeint4--1)", NULL);
      }
      h_start = (n_step_ups - 1 ? h_start * 10 : h_used * 10);
      continue;
    }
    /* calculate derivatives and exit function at new point */
    (*derivs)(dydx1, y1, x1);
    ex1 = exit_value - y1[i_exit_value];
    if (store_data)
      (*store_data)(dydx1, y1, x1, ex1);
    /* check for change in sign of exit function */
    if (SIGN(ex0) != SIGN(ex1) && !is_zero) {
      if (n_to_skip == 0)
        break;
      else {
        --n_to_skip;
        is_zero = 1;
      }
    }
    /* check for end of interval */
    if (fabs(xdiff = xf - x1) < x_accuracy) {
      /* end of the interval */
      if (store_data) {
        (*derivs)(dydx1, y1, x1);
        ex1 = exit_value - y0[i_exit_value];
        (*store_data)(dydx1, y1, x1, ex1);
      }
      for (i = 0; i < n_eq; i++)
        y_return[i] = y1[i];
      *x0 = x1;
      *h_rec = h_start;
      tfree(dydx0);
      tfree(dydx1);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
      if (y0 != y_return)
        tfree(y0);
      if (y1 != y_return)
        tfree(y1);
      if (y2 != y_return)
        tfree(y2);
      return (DIFFEQ_END_OF_INTERVAL);
    }
    /* copy the new solution into the old variables */
    SWAP_PTR(dydx0, dydx1);
    SWAP_PTR(y0, y1);
    ex0 = ex1;
    *x0 = x1;
    /* adjust the step size as recommended by rk_qcstep() */
    h_start = (h_next > h_max ? (h_max ? h_max : h_next) : h_next);
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);
  } while (1);
  *h_rec = h_start;

  /* The root has been bracketed. */
  n_exit_iterations = MAX_EXIT_ITERATIONS;
  do {
    /* try to take a step to the position where the zero is expected */
    h_start = -ex0 * (x1 - *x0) / (ex1 - ex0) * ITER_FACTOR;
    x2 = *x0;
    /* calculate new scale factors */
    new_scale_factors_dp(yscale, y0, dydx0, h_start, tiny, accmode,
                         accur, n_eq);
    if (!rk_qcstep(y2, &x2, y0, dydx0, h_start, &h_used, &h_next,
                   yscale, n_eq, derivs, misses))
      bomb("step size too small (rk_odeint4--2)", NULL);
    /* check the exit function at the new position */
    (*derivs)(dydx2, y2, x2);
    ex2 = exit_value - y2[i_exit_value];
    if (fabs(ex2) < exit_accuracy) {
      for (i = 0; i < n_eq; i++)
        y_return[i] = y2[i];
      *x0 = x2;
      tfree(dydx0);
      tfree(dydx1);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
      if (y0 != y_return)
        tfree(y0);
      if (y1 != y_return)
        tfree(y1);
      if (y2 != y_return)
        tfree(y2);
      return (DIFFEQ_ZERO_FOUND);
    }
    /* rebracket the root */
    if (SIGN(ex1) == SIGN(ex2)) {
      SWAP_PTR(y1, y2);
      SWAP_PTR(dydx1, dydx2);
      x1 = x2;
      ex1 = ex2;
    } else {
      SWAP_PTR(y0, y2);
      SWAP_PTR(dydx0, dydx2);
      *x0 = x2;
      ex0 = ex2;
    }
  } while (n_exit_iterations--);
  return (DIFFEQ_EXIT_COND_FAILED);
}

/**
 * @brief Integrate ODEs without adaptive step-size and with stochastic processes.
 *
 * Integrates a system of ordinary differential equations without using
 * adaptive step-size control and allows for the inclusion of stochastic processes.
 *
 * @param yif Initial and final values of dependent variables.
 * @param derivs Function to compute derivatives.
 * @param n_eq Number of equations.
 * @param accuracy Ignored in this function.
 * @param accmode Ignored in this function.
 * @param tiny Ignored in this function.
 * @param misses Ignored in this function.
 * @param x0 Pointer to the initial value of the independent variable. Updated to final value.
 * @param xf Upper limit of the independent variable.
 * @param x_accuracy Desired accuracy for the final value of the independent variable.
 * @param h_step Step size.
 * @param h_max Ignored in this function.
 * @param h_rec Ignored in this function.
 * @param exit_func Function to determine when to stop integration based on a condition.
 * @param exit_accuracy Accuracy required for the exit condition.
 * @param stochastic Function to add stochastic processes to the solution.
 * @return DIFFEQ_ZERO_FOUND (>=1) on success, or an error code (<=0) on failure.
 */
long rk_odeint3_na(
  double *yif,                                       /* initial/final values of dependent variables */
  void (*derivs)(double *dydx, double *y, double x), /* (*derivs)(dydx, y, x) */
  long n_eq,                                         /* number of equations */
  /* for each dependent variable: */
  double *accuracy, /* ignored */
  long *accmode,    /* ignored */
  double *tiny,     /* ignored */
  long *misses,     /* ignored */
  /* for the dependent variable: */
  double *x0,        /* initial/final value */
  double xf,         /* upper limit of integration */
  double x_accuracy, /* accuracy of final value */
  double h_step,     /* step size */
  double h_max,      /* ignored */
  double *h_rec,     /* ignored */
  /* function for determining when to stop integration: */
  double (*exit_func)(double *dydx, double *y, double x),
  /* function that is to be zeroed */
  double exit_accuracy, /* how close to zero to get */
  /* function for adding stochastic processes */
  void (*stochastic)(double *y, double x, double h)) {
  static double *y0, *yscale;
  static double *dydx0, *y1, *dydx1, *dydx2, *y2, *accur;
  static long last_neq = 0;
  double ex0, ex1, ex2, x1, x2;
  double xdiff;
  long i, n_exit_iterations;
#define MAX_N_STEP_UPS 10

  if (*x0 > xf)
    return (DIFFEQ_XI_GT_XF);
  if (FABS(*x0 - xf) < x_accuracy)
    return (DIFFEQ_SOLVED_ALREADY);

  if (last_neq < n_eq) {
    if (last_neq != 0) {
      tfree(y0);
      tfree(dydx0);
      tfree(y1);
      tfree(dydx1);
      tfree(y2);
      tfree(dydx2);
      tfree(yscale);
      tfree(accur);
    }
    y0 = tmalloc(sizeof(double) * n_eq);
    dydx0 = tmalloc(sizeof(double) * n_eq);
    y1 = tmalloc(sizeof(double) * n_eq);
    dydx1 = tmalloc(sizeof(double) * n_eq);
    y2 = tmalloc(sizeof(double) * n_eq);
    dydx2 = tmalloc(sizeof(double) * n_eq);
    last_neq = n_eq;
  }

  for (i = 0; i < n_eq; i++)
    y0[i] = yif[i];

  /* calculate derivatives and exit function at the initial point */
  (*derivs)(dydx0, y0, *x0);

  ex0 = (*exit_func)(dydx0, y0, *x0);

  do {
    /* check for zero of exit function */
    if (FABS(ex0) < exit_accuracy) {
      for (i = 0; i < n_eq; i++)
        yif[i] = y0[i];
      return (DIFFEQ_ZERO_FOUND);
    }

    /* adjust step size to stay within interval */
    if ((xdiff = xf - *x0) < h_step)
      h_step = xdiff;
    /* take a step */
    x1 = *x0;
    rk4_step(y1, x1, y0, dydx0, h_step, n_eq, derivs);
    if (stochastic)
      /* add stochastic processes */
      (*stochastic)(y1, x1, h_step);
    x1 += h_step;
    /* calculate derivatives and exit function at new point */
    (*derivs)(dydx1, y1, x1);
    ex1 = (*exit_func)(dydx1, y1, x1);
    if (SIGN(ex0) != SIGN(ex1))
      break;
    /* check for end of interval */
    if (FABS(xdiff = xf - x1) < x_accuracy) {
      /* end of the interval */
      for (i = 0; i < n_eq; i++)
        yif[i] = y1[i];
      *x0 = x1;
      return (DIFFEQ_END_OF_INTERVAL);
    }
    /* copy the new solution into the old variables */
    SWAP_PTR(dydx0, dydx1);
    SWAP_PTR(y0, y1);
    ex0 = ex1;
    *x0 = x1;
  } while (1);

  if (!exit_func) {
    printf("failure in rk_odeint3_na():  solution stepped outside interval\n");
    return (DIFFEQ_OUTSIDE_INTERVAL);
  }

  if (FABS(ex1) < exit_accuracy) {
    for (i = 0; i < n_eq; i++)
      yif[i] = y1[i];
    *x0 = x1;
    return (DIFFEQ_ZERO_FOUND);
  }

  /* The root has been bracketed. */
  n_exit_iterations = MAX_EXIT_ITERATIONS;
  do {
    /* try to take a step to the position where the zero is expected */
    /* no stochastic effects are included here! */
    h_step = -ex0 * (x1 - *x0) / (ex1 - ex0) * ITER_FACTOR;
    x2 = *x0;
    rk4_step(y2, x2, y0, dydx0, h_step, n_eq, derivs);
    x2 += h_step;
    /* check the exit function at the new position */
    (*derivs)(dydx2, y2, x2);
    ex2 = (*exit_func)(dydx2, y2, x2);
    if (FABS(ex2) < exit_accuracy) {
      for (i = 0; i < n_eq; i++)
        yif[i] = y2[i];
      *x0 = x2;
      return (DIFFEQ_ZERO_FOUND);
    }
    /* rebracket the root */
    if (SIGN(ex1) == SIGN(ex2)) {
      SWAP_PTR(y1, y2);
      SWAP_PTR(dydx1, dydx2);
      x1 = x2;
      ex1 = ex2;
    } else {
      SWAP_PTR(y0, y2);
      SWAP_PTR(dydx0, dydx2);
      *x0 = x2;
      ex0 = ex2;
    }
  } while (n_exit_iterations--);
  return (DIFFEQ_EXIT_COND_FAILED);
}



