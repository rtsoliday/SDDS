/**
 * @file minc.c
 * @brief Finds the minimum of a multi-parameter function with parameter constraints.
 *
 * This file contains the implementation of the minc() function, which searches for the
 * minimum of a multi-parameter function while respecting constraints on the parameter ranges.
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
 * @brief Finds the minimum of a multi-parameter function with constraints.
 *
 * The minc() function searches for the minimum value of a user-defined multi-parameter
 * function, allowing constraints on the range of each parameter. It iteratively adjusts
 * the parameters within specified limits to find the function's minimum value.
 *
 * @param fn       Pointer to the function to be minimized.
 * @param x        Array of starting values for the parameters.
 * @param dx       Array of step sizes for each parameter.
 * @param dx_lim   Array of step size limits for each parameter.
 * @param xlo      Array of lower bounds for the parameters.
 * @param xhi      Array of upper bounds for the parameters.
 * @param np       Number of parameters.
 * @param ns_max   Maximum number of steps to take before increasing the step size.
 * @param p_flag   If >= 0, prints information during the minimization process.
 *
 * @return The minimum value of the function.
 */
double minc(fn, x, dx, dx_lim, xlo, xhi, np, ns_max, p_flag) double (*fn)(); /* pointer to fn to be minimize */
double *x;                                                                   /* array of starting values of parameters */
double *dx;                                                                  /* array of step sizes */
double *dx_lim;                                                              /* array of step size limits */
double *xlo;                                                                 /* array of lower limits */
double *xhi;                                                                 /* array of upper limits */
long np;                                                                     /* number of parameters */
long ns_max;                                                                 /* number of steps to take before increasing dx */
long p_flag;                                                                 /* if >= 0, information is printed during minimization */
{
  register long i, n_steps, pc;
  register double f0, f1, _dx;
  long flag, *constraint, at_upper, at_lower;

  pc = 0;
  constraint = tmalloc(sizeof(long) * np);
  for (i = 0; i < np; i++)
    constraint[i] = xlo[i] != xhi[i];

  f0 = (*fn)(x);

  do {
    for (i = flag = 0; i < np; i++) {
      if (fabs(_dx = dx[i]) < dx_lim[i]) {
        flag++;
        continue;
      }
      x[i] += _dx;
      if (constraint[i]) {
        if (x[i] < xlo[i]) {
          x[i] = xlo[i] + 2 * (dx[i] = fabs(_dx) / 2);
          continue;
        }
        if (x[i] > xhi[i]) {
          x[i] = xhi[i] + 2 * (dx[i] = -fabs(_dx) / 2);
          continue;
        }
      }
      f1 = (*fn)(x);
      n_steps = 0;
      if (f1 > f0) {
        dx[i] = _dx = -_dx;
        x[i] += 2 * _dx;
        if (constraint[i]) {
          if (x[i] < xlo[i]) {
            x[i] = xlo[i] + 2 * (dx[i] = fabs(_dx) / 2);
            continue;
          }
          if (x[i] > xhi[i]) {
            x[i] = xhi[i] + 2 * (dx[i] = -fabs(_dx) / 2);
            continue;
          }
        }
        f1 = (*fn)(x);
      }
      while (f1 < f0) {
        if (n_steps++ == ns_max) {
          n_steps = 0;
          dx[i] = _dx = 2 * _dx;
        }
        f0 = f1;
        x[i] += _dx;
        if (constraint[i]) {
          if (x[i] < xlo[i]) {
            x[i] = xlo[i] + 2 * (dx[i] = fabs(_dx));
            break;
          }
          if (x[i] > xhi[i]) {
            x[i] = xhi[i] + 2 * (dx[i] = -fabs(_dx));
            break;
          }
        }
        f1 = (*fn)(x);
      }
      dx[i] /= 2;
      x[i] -= _dx;
    }
    if (pc++ == p_flag) {
      printf("%.16le\n", f0);
      for (i = 0; i < np; i++)
        printf("%.16le\t%.16le\n", x[i], dx[i]);
      pc = 0;
    }
  } while (flag != np);
  return (f0);
}
