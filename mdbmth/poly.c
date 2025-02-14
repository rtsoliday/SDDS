/** 
 * @file poly.c
 * @brief Functions for evaluating polynomials and their derivatives, as well as solving quadratic equations.
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
 * @brief Evaluate a polynomial at a given point.
 *
 * Given an array of coefficients a and the number of terms n, this function returns the value of 
 * the polynomial at x. The polynomial is assumed to be of the form:
 * a[0] + a[1]*x + a[2]*x^2 + ... + a[n-1]*x^(n-1).
 *
 * @param[in] a Pointer to the array of polynomial coefficients.
 * @param[in] n Number of coefficients (degree+1 of the polynomial).
 * @param[in] x The point at which to evaluate the polynomial.
 * @return The value of the polynomial at x.
 */
double poly(double *a, long n, double x)
{
  register long i;
  register double sum, xp;

  sum = 0;
  xp = 1;
  for (i = 0; i < n; i++) {
    sum += xp * a[i];
    xp *= x;
  }
  return (sum);
}

/**
 * @brief Evaluate the derivative of a polynomial at a given point.
 *
 * Given an array of coefficients a and the number of terms n, this function returns the value 
 * of the derivative of the polynomial at x. The polynomial is assumed to be:
 * a[0] + a[1]*x + a[2]*x^2 + ... 
 * Its derivative is:
 * a[1] + 2*a[2]*x + 3*a[3]*x^2 + ...
 *
 * @param[in] a Pointer to the array of polynomial coefficients.
 * @param[in] n Number of coefficients (degree+1 of the polynomial).
 * @param[in] x The point at which to evaluate the derivative.
 * @return The value of the derivative at x.
 */
double dpoly(double *a, long n, double x) {
  register long i;
  register double sum, xp;

  sum = 0;
  xp = 1;
  for (i = 1; i < n; i++) {
    sum += i * xp * a[i];
    xp *= x;
  }
  return (sum);
}

/**
 * @brief Evaluate a polynomial with arbitrary powers at a given point.
 *
 * This function computes the value of a polynomial at x, where each term may have an arbitrary power.
 * Given arrays a and power, the polynomial is:
 * a[0]*x^(power[0]) + a[1]*x^(power[1]) + ... + a[n-1]*x^(power[n-1]).
 *
 * @param[in] a     Pointer to the array of polynomial coefficients.
 * @param[in] power Pointer to the array of powers for each term.
 * @param[in] n     Number of terms.
 * @param[in] x     The point at which to evaluate the polynomial.
 * @return The value of the polynomial at x.
 */
double polyp(double *a, long *power, long n, double x) {
  register long i;
  register double sum, xp;

  xp = ipow(x, power[0]);
  sum = xp * a[0];
  for (i = 1; i < n; i++) {
    xp *= ipow(x, power[i] - power[i - 1]);
    sum += xp * a[i];
  }
  return (sum);
}

/**
 * @brief Evaluate the derivative of a polynomial with arbitrary powers at a given point.
 *
 * Given arrays a and power that define a polynomial 
 * a[0]*x^(power[0]) + a[1]*x^(power[1]) + ... + a[n-1]*x^(power[n-1]),
 * this function returns the derivative of that polynomial at x:
 * power[0]*a[0]*x^(power[0]-1) + power[1]*a[1]*x^(power[1]-1) + ...
 *
 * @param[in] a     Pointer to the array of polynomial coefficients.
 * @param[in] power Pointer to the array of powers for each term.
 * @param[in] n     Number of terms.
 * @param[in] x     The point at which to evaluate the derivative.
 * @return The value of the derivative at x.
 */
double dpolyp(double *a, long *power, long n, double x) {
  register long i;
  register double sum, xp;

  xp = ipow(x, power[0] - 1);
  sum = power[0] * xp * a[0];
  for (i = 1; i < n; i++) {
    xp *= ipow(x, power[i] - power[i - 1]);
    sum += power[i] * xp * a[i];
  }
  return (sum);
}

/**
 * @brief Solve a quadratic equation for real solutions.
 *
 * Given a quadratic equation of the form a*x^2 + b*x + c = 0, this function finds its real solutions, if any.
 * The solutions are returned in the array solution. The function returns the number of real solutions found:
 * - 0 if no real solutions,
 * - 1 if one real solution (repeated root),
 * - 2 if two real solutions.
 *
 * @param[in]  a        Quadratic coefficient (for x^2).
 * @param[in]  b        Linear coefficient (for x).
 * @param[in]  c        Constant term.
 * @param[out] solution Array of size 2 for storing the found solutions.
 * @return The number of real solutions (0, 1, or 2).
 */
int solveQuadratic(double a, double b, double c, double *solution) {
  double det;
  if (a == 0) {
    if (b == 0)
      return 0;
    solution[0] = -c / b;
    return 1;
  }
  if ((det = b * b - 4 * a * c) < 0)
    return 0;
  if (det == 0) {
    solution[0] = -b / a;
    return 1;
  }
  solution[0] = (-b - sqrt(det)) / (2 * a);
  solution[1] = (-b + sqrt(det)) / (2 * a);
  return 2;
}
