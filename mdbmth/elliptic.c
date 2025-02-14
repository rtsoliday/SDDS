/** 
 * @file elliptic.c
 * @brief Functions for evaluating complete elliptic integrals of the first and second kind (K and E),
 *         as well as their total derivatives with respect to the modulus k.
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

static double ceiAccuracy = 1e-14;

void setCeiAccuracy(double newAccuracy) {
  ceiAccuracy = newAccuracy;
}

/**
 * @brief Compute the complete elliptic integral of the first kind, K(k).
 *
 * K(k) = ∫_0^(π/2) dθ / √(1 - k² sin² θ)
 *
 * @param[in] k The modulus of the elliptic integral.
 * @return The value of K(k).
 */
double K_cei(double k) {
  double a0, b0, c0, a1, b1;

  a0 = 1;
  b0 = sqrt(1 - sqr(k));
  c0 = k;

  do {
    /* do two steps of recurrence per pass in the loop */
    a1 = (a0 + b0) / 2;
    b1 = sqrt(a0 * b0);
    a0 = (a1 + b1) / 2;
    b0 = sqrt(a1 * b1);
    c0 = (a1 - b1) / 2;
  } while (fabs(c0) > ceiAccuracy);
  return PI / (2 * a0);
}

/**
 * @brief Compute the complete elliptic integral of the second kind, E(k).
 *
 * E(k) = ∫_0^(π/2) √(1 - k² sin² θ) dθ
 *
 * @param[in] k The modulus of the elliptic integral.
 * @return The value of E(k).
 */
double E_cei(double k) {
  double a0, b0, c0, a1, b1, c1, K, sum, powerOf2;

  a0 = 1;
  b0 = sqrt(1 - sqr(k));
  c0 = k;
  sum = sqr(c0);
  powerOf2 = 1;

  do {
    /* do two steps of recurrence per pass in the loop */
    a1 = (a0 + b0) / 2;
    b1 = sqrt(a0 * b0);
    c1 = (a0 - b0) / 2;
    sum += sqr(c1) * (powerOf2 *= 2);
    ;

    a0 = (a1 + b1) / 2;
    b0 = sqrt(a1 * b1);
    c0 = (a1 - b1) / 2;
    sum += sqr(c0) * (powerOf2 *= 2);
  } while (fabs(c0) > ceiAccuracy);

  K = PI / (2 * a0);
  return K * (1 - sum / 2);
}

double *KE_cei(double k, double *buffer) {
  double a0, b0, c0, a1, b1, c1, K, sum, powerOf2;

  if (!buffer)
    buffer = tmalloc(sizeof(*buffer) * 2);

  a0 = 1;
  b0 = sqrt(1 - sqr(k));
  c0 = k;
  sum = sqr(c0);
  powerOf2 = 1;

  do {
    /* do two steps of recurrence per pass in the loop */
    a1 = (a0 + b0) / 2;
    b1 = sqrt(a0 * b0);
    c1 = (a0 - b0) / 2;
    sum += sqr(c1) * (powerOf2 *= 2);
    ;

    a0 = (a1 + b1) / 2;
    b0 = sqrt(a1 * b1);
    c0 = (a1 - b1) / 2;
    sum += sqr(c0) * (powerOf2 *= 2);
  } while (fabs(c0) > ceiAccuracy);

  buffer[0] = K = PI / (2 * a0);
  buffer[1] = K * (1 - sum / 2);
  return buffer;
}

/* These two functions rely on formulae quoted in Landau and Lifshitz,
   ELECTRODYNAMICS OF CONTINUOUS MEDIA, pg 112.
 */

/**
 * @brief Compute the total derivative dK/dk of the complete elliptic integral of the first kind.
 *
 * Uses K(k) and E(k) to determine the derivative with respect to k.
 *
 * @param[in] k The modulus of the elliptic integral.
 * @return The value of dK/dk.
 */
double dK_cei(double k) {
  double buffer[2];
  KE_cei(k, buffer);
  return (buffer[1] / (1 - k * k) - buffer[0]) / k;
}

/**
 * @brief Compute the total derivative dE/dk of the complete elliptic integral of the second kind.
 *
 * Uses K(k) and E(k) to determine the derivative with respect to k.
 *
 * @param[in] k The modulus of the elliptic integral.
 * @return The value of dE/dk.
 */
double dE_cei(k) double k;
{
  double buffer[2];
  KE_cei(k, buffer);
  return (buffer[1] - buffer[0]) / k;
}
