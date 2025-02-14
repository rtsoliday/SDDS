/**
 * @file mmid.c
 * @brief Modified midpoint method for integrating ordinary differential equations (ODEs).
 *
 * This file implements the modified midpoint method for integrating ODEs,
 * based on the algorithms presented in "Numerical Recipes in C" by
 * Michael Borland (1995).
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

#define MAX_EXIT_ITERATIONS 400
#define ITER_FACTOR 0.995
#define TINY 1.0e-30

 /**
  * @brief Integrates a system of ODEs using the modified midpoint method.
  *
  * This function performs numerical integration of a system of ordinary differential
  * equations using the modified midpoint method. It computes the final values of
  * the dependent variables after a specified number of steps over a given interval.
  *
  * @param yInitial        Starting values of dependent variables.
  * @param dydxInitial     Derivatives of the dependent variables at the initial point.
  * @param equations       Number of equations in the system.
  * @param xInitial        Starting value of the independent variable.
  * @param interval        Size of the integration interval in the independent variable.
  * @param steps           Number of steps to divide the interval into.
  * @param yFinal          Array to store the final values of the dependent variables.
  * @param derivs          Function pointer to compute derivatives.
  */
void mmid(
  double *yInitial,    /* starting values of dependent variables */
  double *dydxInitial, /* and their derivatives */
  long equations,      /* number of equations */
  double xInitial,     /* starting value of independent variable */
  double interval,     /* size of interval in x */
  long steps,          /* number of steps to divide interval into */
  double *yFinal,      /* final values of dependent variables */
  /* function return derivatives */
  void (*derivs)(double *_dydx, double *_y, double _x)) {
  long i, j;
  double x = 0, ynSave, h, hTimes2;
  static double *ym, *yn;
  static long last_equations = 0;
  double *dydxTemp;

  if (equations > last_equations) {
    if (last_equations) {
      free(ym);
      free(yn);
    }
    /* allocate arrays for solutions at two adjacent points in x */
    ym = tmalloc(sizeof(*ym) * equations);
    yn = tmalloc(sizeof(*yn) * equations);
    last_equations = equations;
  }

  hTimes2 = (h = interval / steps) * 2;

  /* Copy starting values and compute first set of estimated values */
  for (i = 0; i < equations; i++) {
    ym[i] = yInitial[i];
    yn[i] = yInitial[i] + h * dydxInitial[i];
  }

  dydxTemp = yFinal; /* use yFinal for temporary storage */
  for (j = 1; j < steps; j++) {
    x = xInitial + h * j;
    (*derivs)(dydxTemp, yn, x);
    for (i = 0; i < equations; i++) {
      ynSave = yn[i];
      yn[i] = ym[i] + hTimes2 * dydxTemp[i];
      ym[i] = ynSave;
    }
  }

  /* Compute final values */
  (*derivs)(dydxTemp, yn, x + interval);
  for (i = 0; i < equations; i++)
    yFinal[i] = (ym[i] + yn[i] + h * dydxTemp[i]) / 2;
}

 /**
  * @brief Enhances the modified midpoint method with error correction.
  *
  * This function applies the modified midpoint method with an additional correction step
  * to improve the accuracy of the integration. It performs integration with a specified
  * number of steps and then refines the result by integrating with half the number of steps,
  * combining both results to achieve higher precision.
  *
  * @param y               Starting values of dependent variables.
  * @param dydx            Derivatives of the dependent variables at the initial point.
  * @param equations       Number of variables in the system.
  * @param x0              Starting value of the independent variable.
  * @param interval        Size of the integration interval in the independent variable.
  * @param steps           Number of steps to divide the interval into.
  * @param yFinal          Array to store the final values of the dependent variables.
  * @param derivs          Function pointer to compute derivatives.
  */
void mmid2(
  double *y,       /* starting values of dependent variables */
  double *dydx,    /* and their derivatives */
  long equations,  /* number of variables */
  double x0,       /* starting value of independent variable */
  double interval, /* size of interval in x */
  long steps,      /* number of steps to divide interval into */
  double *yFinal,  /* final values of dependent variables */
  /* function return derivatives */
  void (*derivs)(double *_dydx, double *_y, double _x)) {
  static double *yFinal2;
  static long i, last_equations = 0;

  if (steps % 2)
    steps += 1;
  if (steps < 8)
    steps = 8;

  if (equations > last_equations) {
    if (last_equations) {
      free(yFinal2);
    }
    /* allocate arrays for second solution */
    yFinal2 = tmalloc(sizeof(*yFinal2) * equations);
    last_equations = equations;
  }

  mmid(y, dydx, equations, x0, interval, steps, yFinal, derivs);
  mmid(y, dydx, equations, x0, interval, steps / 2, yFinal2, derivs);
  for (i = 0; i < equations; i++)
    yFinal[i] = (4 * yFinal[i] - yFinal2[i]) / 3;
}

 /**
  * @brief Integrates ODEs until a condition is met or the interval is reached.
  *
  * This function integrates a set of ordinary differential equations using the modified
  * midpoint method until either the upper limit of the independent variable is reached
  * or a user-supplied exit condition is satisfied (i.e., a specified function becomes zero).
  *
  * @param yif              Initial and final values of dependent variables.
  * @param derivs           Function pointer to compute derivatives.
  * @param n_eq             Number of equations in the system.
  * @param accuracy         Desired accuracy for each dependent variable.
  * @param accmode          Desired accuracy-control mode.
  * @param tiny             Ignored parameter.
  * @param misses           Ignored parameter.
  * @param x0               Initial value of the independent variable (updated to final value).
  * @param xf               Upper limit of integration for the independent variable.
  * @param x_accuracy       Desired accuracy for the final value of the independent variable.
  * @param h_step           Initial step size for integration.
  * @param h_max            Ignored parameter.
  * @param h_rec            Ignored parameter.
  * @param exit_func        Function to determine when to stop integration.
  * @param exit_accuracy    Desired accuracy for the exit condition function.
  *
  * @return
  *   - Returns a positive value (>=1) on successful integration.
  *   - Returns 0 or a negative value on failure.
  *   - Specific return values indicate different outcomes, such as zero found or
  *     stepping outside the interval.
  */
long mmid_odeint3_na(
  double *yif,                                       /* initial/final values of dependent variables */
  void (*derivs)(double *dydx, double *y, double x), /* (*derivs)(dydx, y, x) */
  long n_eq,                                         /* number of equations */
  /* for each dependent variable: */
  double *accuracy, /* desired accuracy--see below for meaning */
  long *accmode,    /* desired accuracy-control mode */
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
  double exit_accuracy /* how close to zero to get */
) {
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
    mmid2(y0, dydx0, n_eq, x1, h_step, 8, y1, derivs);
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
    printf("failure in mmid_odeint3_na():  solution stepped outside interval\n");
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
    h_step = -ex0 * (x1 - *x0) / (ex1 - ex0) * ITER_FACTOR;
    x2 = *x0;
    mmid2(y0, dydx0, n_eq, x2, h_step, 8, y2, derivs);
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
