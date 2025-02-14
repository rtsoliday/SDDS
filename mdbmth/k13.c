/**
 * @file k13.c
 * @brief Computes the Modified Bessel Function of the Second Kind K_{1/3}(z).
 *
 * This file provides the implementation of k13(), which computes the
 * Modified Bessel Function of the Second Kind K_{1/3}(z) for a given input.
 * It uses a series expansion for small arguments and an asymptotic expansion for
 * larger arguments, referencing Abramowitz & Stegun and derived from Roger Dejus's k13.f code.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author R. Dejus, H. Shang, R. Soliday
 */

#include "mdb.h"
#include "constants.h"
#include "math.h"

#define A_LIM 10.1
#define EPS1 1.0e-12
#define EPS2 1.0e-8
#define GAMMA_OF_NY 2.678938534707747898
#define NY 1.0 / 3.0

/**
 * @brief Compute the Modified Bessel Function of the Second Kind K_{1/3}(z).
 *
 * This function calculates K_{1/3}(z) for inputs in the approximate range 0.0 < z < 60.0.
 * For smaller z (z < A_LIM), it uses a series expansion, while for larger z, 
 * it employs an asymptotic expansion. The method and constants are adapted from
 * the original Fortran implementation by Roger Dejus and mathematical formulas 
 * from Abramowitz & Stegun.
 *
 * @param z The input value for which K_{1/3}(z) is evaluated.
 * @return The computed value of the Modified Bessel Function K_{1/3}(z).
 */
double k13(double z) {
  double c1, c2, zs, ny_fac, neg_ny_fac, zm, zp, pm, pp, term, sum, ze, za, mu, pa;
  static double e2;
  long k;

  if (z < A_LIM) {
    /*series expansion */
    c1 = PI / 2.0 / sin(PI * NY);
    c2 = 2.0 * c1;
    zs = z * z / 4.0;
    ny_fac = NY * GAMMA_OF_NY;     /*Factorial of (+ny)*/
    neg_ny_fac = c2 / GAMMA_OF_NY; /* Factorial of (-ny) */
    e2 = pow(z / 2.0, NY);
    zm = 1 / e2 / neg_ny_fac;
    zp = e2 / ny_fac;
    /*first term */
    pm = pp = 1.0;
    sum = term = c1 * (pm * zm - pp * zp);
    /* additional term */
    k = 0;
    while (fabs(term) > EPS1 * sum) {
      k = k + 1;
      pm = pm * zs / (k * (k - NY));
      pp = pp * zs / (k * (k + NY));
      term = c1 * (pm * zm - pp * zp);
      sum += term;
    }
  } else {
    /* Use asymptotic expansion for large arguments */
    ze = sqrt(PI / 2.0 / z) * exp(-z);
    za = 1.0 / (8.0 * z);
    mu = 4.0 * NY * NY;
    /* first term */
    pa = 1.0;
    sum = term = pa * ze;
    /*Additional terms */
    k = 0;
    while (fabs(term) > EPS2 * sum) {
      k = k + 1;
      pa = pa * za * (mu - (2 * k - 1) * (2 * k - 1)) / k;
      term = pa * ze;
      sum += term;
    }
  }
  return sum;
}
