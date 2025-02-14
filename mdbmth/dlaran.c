/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* dlaran.f -- translated by f2c (version of 30 January 1990  16:02:04).
   You must link the resulting object file with the libraries:
	-lF77 -lI77 -lm -lc   (in that order)
 $Log: not supported by cvs2svn $
 Revision 1.7  2005/11/04 22:47:14  soliday
 Updated code to be compiled by a 64 bit processor.

 Revision 1.6  2002/08/14 16:18:54  soliday
 Added Open License

 Revision 1.5  2000/03/27 20:30:11  borland
 Removed confusing (unnecessary) static designation for some variables.

 Revision 1.4  1999/07/29 21:54:00  borland
 Added back a file I actually need.

 Revision 1.2  1995/09/05 21:19:33  saunders
 First test release of the SDDS1.5 package.

*/

#include "f2c.h"

doublereal dlaran_(integer *iseed) {
  /* System generated locals */
  doublereal ret_val;

  /* Local variables */
  integer it1, it2, it3, it4;

  /* Parameter adjustments */
  /*--iseed;*/

  /* Function Body */

  /*  -- LAPACK auxiliary routine (version 2.0) -- */
  /*     Univ. of Tennessee, Univ. of California Berkeley, NAG Ltd., */
  /*     Courant Institute, Argonne National Lab, and Rice University */
  /*     February 29, 1992 */

  /*     .. Array Arguments .. */
  /*     .. */

  /*  Purpose */
  /*  ======= */

  /*  DLARAN returns a random real number from a uniform (0,1) */
  /*  distribution. */

  /*  Arguments */
  /*  ========= */

  /*  ISEED   (input/output) INTEGER array, dimension (4) */
  /*          On entry, the seed of the random number generator; the array 
*/
  /*          elements must be between 0 and 4095, and ISEED(4) must be */
  /*          odd. */
  /*          On exit, the seed is updated. */

  /*  Further Details */
  /*  =============== */

  /*  This routine uses a multiplicative congruential method with modulus */

  /*  2**48 and multiplier 33952834046453 (see G.S.Fishman, */
  /*  'Multiplicative congruential random number generators with modulus */
  /*  2**b: an exhaustive analysis for b = 32 and a partial analysis for */
  /*  b = 48', Math. Comp. 189, pp 331-344, 1990). */

  /*  48-bit integers are stored in 4 integer array elements with 12 bits */

  /*  per element. Hence the routine is portable across machines with */
  /*  integers of 32 bits or more. */

  /*  ===================================================================== 
*/

  /*     .. Parameters .. */
  /*     .. */
  /*     .. Local Scalars .. */
  /*     .. */
  /*     .. Intrinsic Functions .. */
  /*     .. */
  /*     .. Executable Statements .. */

  /*     multiply the seed by the multiplier modulo 2**48 */

  it4 = iseed[3] * 2549;
  it3 = it4 / 4096;
  it4 -= it3 << 12;
  it3 = it3 + iseed[2] * 2549 + iseed[3] * 2508;
  it2 = it3 / 4096;
  it3 -= it2 << 12;
  it2 = it2 + iseed[1] * 2549 + iseed[2] * 2508 + iseed[3] * 322;
  it1 = it2 / 4096;
  it2 -= it1 << 12;
  it1 = it1 + iseed[0] * 2549 + iseed[1] * 2508 + iseed[2] * 322 + iseed[3] * 494;
  it1 %= 4096;

  /*     return updated seed */

  iseed[0] = it1;
  iseed[1] = it2;
  iseed[2] = it3;
  iseed[3] = it4;

  /*     convert 48-bit integer to a real number in the interval (0,1) */

  ret_val = ((doublereal)it1 + ((doublereal)it2 + ((doublereal)it3 + (doublereal)it4 * 2.44140625e-4) * 2.44140625e-4) * 2.44140625e-4) * 2.44140625e-4;
  return ret_val;

  /*     End of DLARAN */

} /* dlaran_ */

doublereal dlaran_oag(integer *iseed, long increment) {
  doublereal ret_val;

  integer it1, it2, it3, it4, i;

  if (increment < 1)
    increment = 1;
  for (i = 0; i < increment; i++) {
    it4 = iseed[3] * 2549;
    it3 = it4 / 4096;
    it4 -= it3 << 12;
    it3 = it3 + iseed[2] * 2549 + iseed[3] * 2508;
    it2 = it3 / 4096;
    it3 -= it2 << 12;
    it2 = it2 + iseed[1] * 2549 + iseed[2] * 2508 + iseed[3] * 322;
    it1 = it2 / 4096;
    it2 -= it1 << 12;
    it1 = it1 + iseed[0] * 2549 + iseed[1] * 2508 + iseed[2] * 322 + iseed[3] * 494;
    it1 %= 4096;

    iseed[0] = it1;
    iseed[1] = it2;
    iseed[2] = it3;
    iseed[3] = it4;
  }

  ret_val = ((doublereal)it1 + ((doublereal)it2 + ((doublereal)it3 + (doublereal)it4 * 2.44140625e-4) * 2.44140625e-4) * 2.44140625e-4) * 2.44140625e-4;
  return ret_val;
}
