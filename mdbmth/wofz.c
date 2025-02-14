/**
 * @file wofz.c
 * @brief Computes the complex error function \f$ W(z) \f$ for a given complex number \f$ z \f$.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author H. Shang, M. Borland, R. Soliday
 */

#include "mdb.h"
#include "f2c.h"

double pow_di(doublereal *ap, integer *bp) {
  double pow, x;
  integer n;
  unsigned long u;

  pow = 1;
  x = *ap;
  n = *bp;

  if (n != 0) {
    if (n < 0) {
      n = -n;
      x = 1 / x;
    }
    for (u = n;;) {
      if (u & 01)
        pow *= x;
      if (u >>= 1)
        x *= x;
      else
        break;
    }
  }
  return (pow);
}

#undef abs
#define abs(x) ((x) >= 0 ? (x) : -(x))

integer i_dnnt(doublereal *x) {
  return (integer)(*x >= 0. ? floor(*x + .5) : -floor(.5 - *x));
}

/*      ALGORITHM 680, COLLECTED ALGORITHMS FROM ACM. */
/*      THIS WORK PUBLISHED IN TRANSACTIONS ON MATHEMATICAL SOFTWARE, */
/*      VOL. 16, NO. 1, PP. 47. */
/* Subroutine */

/**
 * @brief Computes the complex error function \f$ W(z) \f$ for a given complex number \f$ z \f$.
 *
 * Given a complex number \f$ z = (\text{xi}, \text{yi}) \f$, this function computes the value of the
 * Faddeeva function \f$ W(z) = \exp(-z^2) \cdot \text{ERFC}(-i z) \f$, where \f$ \text{ERFC} \f$ is the
 * complex complementary error function and \f$ i = \sqrt{-1} \f$.
 *
 * The accuracy of the algorithm is:
 * - 14 significant digits for \f$ z \f$ in the 1st and 2nd quadrants.
 * - 13 significant digits for \f$ z \f$ in the 3rd and 4th quadrants, outside a circular region with
 *   a radius of 0.126 around a zero of the function.
 *
 * @param xi Pointer to the real part of the input complex number \f$ z \f$.
 * @param yi Pointer to the imaginary part of the input complex number \f$ z \f$.
 * @param u Pointer to store the real part of the computed \f$ W(z) \f$.
 * @param v Pointer to store the imaginary part of the computed \f$ W(z) \f$.
 * @param flag__ Pointer to a logical flag indicating whether an overflow has occurred.
 *               - \c false: No error condition.
 *               - \c true: Overflow has occurred; the routine becomes inactive.
 *
 * @return Returns \c 0 on successful computation. If an overflow is detected, \p flag__ is set to \c true.
 */
int wofz(xi, yi, u, v, flag__)
  doublereal *xi,
  *yi, *u, *v;
logical *flag__;
{
  /* System generated locals */
  doublereal d__1, d__2;

  /* Builtin functions */
  /*
    double sqrt();
    integer i_dnnt();
    double exp(), cos(), sin(), pow_di();
    */
  /* Local variables */
  static integer kapn;
  static doublereal xabs, yabs, daux, qrho, xaux, xsum, ysum;
  static logical a, b;
  static doublereal c__, h__;
  static integer i__, j, n;
  static doublereal x, y, xabsq, xquad, yquad, h2, u1, v1, u2, v2, w1;
  static integer nu;
  static doublereal rx, ry, sx, sy, tx, ty;
  static integer np1;
  static doublereal qlambda;

  /*  GIVEN A COMPLEX NUMBER Z = (XI,YI), THIS SUBROUTINE COMPUTES */
  /*  THE VALUE OF THE FADDEEVA-FUNCTION W(Z) = EXP(-Z**2)*ERFC(-I*Z), */
  /*  WHERE ERFC IS THE COMPLEX COMPLEMENTARY ERROR-FUNCTION AND I */
  /*  MEANS SQRT(-1). */
  /*  THE ACCURACY OF THE ALGORITHM FOR Z IN THE 1ST AND 2ND QUADRANT */
  /*  IS 14 SIGNIFICANT DIGITS; IN THE 3RD AND 4TH IT IS 13 SIGNIFICANT */
  /*  DIGITS OUTSIDE A CIRCULAR REGION WITH RADIUS 0.126 AROUND A ZERO */
  /*  OF THE FUNCTION. */
  /*  ALL REAL VARIABLES IN THE PROGRAM ARE DOUBLE PRECISION. */

  /*  THE CODE CONTAINS A FEW COMPILER-DEPENDENT PARAMETERS : */
  /*     RMAXREAL = THE MAXIMUM VALUE OF RMAXREAL EQUALS THE ROOT OF */
  /*                RMAX = THE LARGEST NUMBER WHICH CAN STILL BE */
  /*                IMPLEMENTED ON THE COMPUTER IN DOUBLE PRECISION */
  /*                FLOATING-POINT ARITHMETIC */
  /*     RMAXEXP  = LN(RMAX) - LN(2) */
  /*     RMAXGONI = THE LARGEST POSSIBLE ARGUMENT OF A DOUBLE PRECISION */
  /*                GONIOMETRIC FUNCTION (DCOS, DSIN, ...) */
  /*  THE REASON WHY THESE PARAMETERS ARE NEEDED AS THEY ARE DEFINED WILL */
  /*  BE EXPLAINED IN THE CODE BY MEANS OF COMMENTS */

  /*  PARAMETER LIST */
  /*     XI     = REAL      PART OF Z */
  /*     YI     = IMAGINARY PART OF Z */
  /*     U      = REAL      PART OF W(Z) */
  /*     V      = IMAGINARY PART OF W(Z) */
  /*     FLAG   = AN ERROR FLAG INDICATING WHETHER OVERFLOW WILL */
  /*              OCCUR OR NOT; TYPE LOGICAL; */
  /*              THE VALUES OF THIS VARIABLE HAVE THE FOLLOWING */
  /*              MEANING : */
  /*              FLAG=.FALSE. : NO ERROR CONDITION */
  /*              FLAG=.TRUE.  : OVERFLOW WILL OCCUR, THE ROUTINE */
  /*                             BECOMES INACTIVE */
  /*  XI, YI      ARE THE INPUT-PARAMETERS */
  /*  U, V, FLAG  ARE THE OUTPUT-PARAMETERS */

  /*  FURTHERMORE THE PARAMETER FACTOR EQUALS 2/SQRT(PI) */

  /*  THE ROUTINE IS NOT UNDERFLOW-PROTECTED BUT ANY VARIABLE CAN BE */
  /*  PUT TO 0 UPON UNDERFLOW; */

  /*  REFERENCE - GPM POPPE, CMJ WIJERS; MORE EFFICIENT COMPUTATION OF */
  /*  THE COMPLEX ERROR-FUNCTION, ACM TRANS. MATH. SOFTWARE. */

  *flag__ = FALSE_;

  xabs = abs(*xi);
  yabs = abs(*yi);
  x = xabs / (float)6.3;
  y = yabs / (float)4.4;

  /*     THE FOLLOWING IF-STATEMENT PROTECTS */
  /*     QRHO = (X**2 + Y**2) AGAINST OVERFLOW */

  if (xabs > 5e153 || yabs > 5e153) {
    goto L100;
  }

  /* Computing 2nd power */
  d__1 = x;
  /* Computing 2nd power */
  d__2 = y;
  qrho = d__1 * d__1 + d__2 * d__2;

  /* Computing 2nd power */
  d__1 = xabs;
  xabsq = d__1 * d__1;
  /* Computing 2nd power */
  d__1 = yabs;
  xquad = xabsq - d__1 * d__1;
  yquad = xabs * 2 * yabs;

  a = qrho < .085264;

  if (a) {

    /*  IF (QRHO.LT.0.085264D0) THEN THE FADDEEVA-FUNCTION IS EVALUATED */
    /*  USING A POWER-SERIES (ABRAMOWITZ/STEGUN, EQUATION (7.1.5), P.297) 
*/
    /*  N IS THE MINIMUM NUMBER OF TERMS NEEDED TO OBTAIN THE REQUIRED */
    /*  ACCURACY */

    qrho = (1 - y * (float).85) * sqrt(qrho);
    d__1 = qrho * 72 + 6;
    n = i_dnnt(&d__1);
    j = (n << 1) + 1;
    xsum = (float)1. / j;
    ysum = 0.;
    for (i__ = n; i__ >= 1; --i__) {
      j += -2;
      xaux = (xsum * xquad - ysum * yquad) / i__;
      ysum = (xsum * yquad + ysum * xquad) / i__;
      xsum = xaux + (float)1. / j;
      /* L10: */
    }
    u1 = (xsum * yabs + ysum * xabs) * -1.12837916709551257388 + (float)1.;
    v1 = (xsum * xabs - ysum * yabs) * 1.12837916709551257388;
    daux = exp(-xquad);
    u2 = daux * cos(yquad);
    v2 = -daux * sin(yquad);

    *u = u1 * u2 - v1 * v2;
    *v = u1 * v2 + v1 * u2;

  } else {

    /*  IF (QRHO.GT.1.O) THEN W(Z) IS EVALUATED USING THE LAPLACE */
    /*  CONTINUED FRACTION */
    /*  NU IS THE MINIMUM NUMBER OF TERMS NEEDED TO OBTAIN THE REQUIRED */
    /*  ACCURACY */

    /*  IF ((QRHO.GT.0.085264D0).AND.(QRHO.LT.1.0)) THEN W(Z) IS EVALUATED
 */
    /*  BY A TRUNCATED TAYLOR EXPANSION, WHERE THE LAPLACE CONTINUED FRACT
ION */
    /*  IS USED TO CALCULATE THE DERIVATIVES OF W(Z) */
    /*  KAPN IS THE MINIMUM NUMBER OF TERMS IN THE TAYLOR EXPANSION NEEDED
 */
    /*  TO OBTAIN THE REQUIRED ACCURACY */
    /*  NU IS THE MINIMUM NUMBER OF TERMS OF THE CONTINUED FRACTION NEEDED
 */
    /*  TO CALCULATE THE DERIVATIVES WITH THE REQUIRED ACCURACY */

    if (qrho > (float)1.) {
      h__ = 0.;
      kapn = 0;
      qrho = sqrt(qrho);
      nu = (integer)(1442 / (qrho * 26 + 77) + 3);
    } else {
      qrho = (1 - y) * sqrt(1 - qrho);
      h__ = qrho * (float)1.88;
      h2 = h__ * 2;
      d__1 = qrho * 34 + 7;
      kapn = i_dnnt(&d__1);
      d__1 = qrho * 26 + 16;
      nu = i_dnnt(&d__1);
    }

    b = h__ > (float)0.;

    if (b) {
      qlambda = pow_di(&h2, &kapn);
    }

    rx = (float)0.;
    ry = (float)0.;
    sx = (float)0.;
    sy = (float)0.;

    for (n = nu; n >= 0; --n) {
      np1 = n + 1;
      tx = yabs + h__ + np1 * rx;
      ty = xabs - np1 * ry;
      /* Computing 2nd power */
      d__1 = tx;
      /* Computing 2nd power */
      d__2 = ty;
      c__ = (float).5 / (d__1 * d__1 + d__2 * d__2);
      rx = c__ * tx;
      ry = c__ * ty;
      if (b && n <= kapn) {
        tx = qlambda + sx;
        sx = rx * tx - ry * sy;
        sy = ry * tx + rx * sy;
        qlambda /= h2;
      }
      /* L11: */
    }

    if (h__ == (float)0.) {
      *u = rx * 1.12837916709551257388;
      *v = ry * 1.12837916709551257388;
    } else {
      *u = sx * 1.12837916709551257388;
      *v = sy * 1.12837916709551257388;
    }

    if (yabs == (float)0.) {
      /* Computing 2nd power */
      d__1 = xabs;
      *u = exp(-(d__1 * d__1));
    }
  }

  /*  EVALUATION OF W(Z) IN THE OTHER QUADRANTS */

  if (*yi < (float)0.) {

    if (a) {
      u2 *= 2;
      v2 *= 2;
    } else {
      xquad = -xquad;

      /*         THE FOLLOWING IF-STATEMENT PROTECTS 2*EXP(-Z**2) */
      /*         AGAINST OVERFLOW */

      if (yquad > 3537118876014220. || xquad > 708.503061461606) {
        goto L100;
      }

      w1 = exp(xquad) * 2;
      u2 = w1 * cos(yquad);
      v2 = -w1 * sin(yquad);
    }

    *u = u2 - *u;
    *v = v2 - *v;
    if (*xi > (float)0.) {
      *v = -(*v);
    }
  } else {
    if (*xi < (float)0.) {
      *v = -(*v);
    }
  }

  return 0;

L100:
  *flag__ = TRUE_;
  return 0;

} /* wofz */
