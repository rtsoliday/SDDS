/**
 * @file kstests.c
 * @brief Perform the Kolmogorov-Smirnov test for two sets of samples.
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
#include "sort.h"

double twoVariableKStest(double *d1, long n1, double *d2, long n2, double *maxCDFerror) {
  long i1, i2;
  double CDFerror, xDifference, CDF1, CDF2, sqrtNe;

  qsort((void *)d1, n1, sizeof(*d1), double_cmpasc);
  qsort((void *)d2, n2, sizeof(*d2), double_cmpasc);
  i1 = i2 = 0;
  CDF1 = CDF2 = 0;
  *maxCDFerror = 0;
  while (i1 < n1 && i2 < n2) {
    /* move through the arrays together, keeping value from both arrays as close as possible.
           xDifference is the distance between the points at which the CDFs are evaluated.
         */
    if ((xDifference = d1[i1] - d2[i2]) <= 0)
      /* d2 is evaluated ahead of d1, so advance d1 */
      CDF1 = ++i1 / ((double)n1);
    if (xDifference >= 0)
      /* d1 is evaluated ahead of d2 */
      CDF2 = ++i2 / ((double)n2);
    if ((CDFerror = fabs(CDF1 - CDF2)) > *maxCDFerror)
      *maxCDFerror = CDFerror;
  }
  sqrtNe = sqrt(((double)n1 * n2) / ((double)n1 + n2));
  return KS_Qfunction((sqrtNe + 0.12 + 0.11 / sqrtNe) * (*maxCDFerror));
}

#define KS_Q_ACCURACY 1e-8
#define KS_Q_MAXTERMS 1000

/**
 * @brief Compute the Q-function for the Kolmogorov-Smirnov test.
 *
 * Given a lambda value, this function approximates the complementary 
 * cumulative distribution function needed to determine the KS probability (Q-value).
 *
 * @param lambda The computed KS statistic scaled by sqrt(N).
 * @return The KS Q-value as a probability measure.
 */
double KS_Qfunction(double lambda) {
  long j;
  double expFactor, factor;
  double sum, term;

  expFactor = -2 * lambda * lambda;
  factor = 1;
  j = 1;
  sum = 0;
  do {
    term = exp(expFactor * j * j);
    sum += factor * term;
    factor *= -1;
  } while (term > KS_Q_ACCURACY && j++ < KS_Q_MAXTERMS);
  if (j > KS_Q_MAXTERMS)
    fputs("warning: KS test did not converge\n", stderr);
  return 2 * sum;
}
