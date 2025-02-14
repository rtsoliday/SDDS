/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.4  2002/08/14 16:41:19  soliday
 * Added Open License
 *
 * Revision 1.3  1999/05/28 14:54:48  soliday
 * Removed compiler warnings under Linux.
 *
   Revision 1.2  1995/09/05 21:13:14  saunders
   First test release of the SDDS1.5 package.
   */
#include "f2c.h"

extern int cfftb_(integer *n, doublereal *c, doublereal *wsave);
/*:ref: cfftb1_ 10 5 3 5 5 5 5 */
extern int cfftb1_(integer *n, doublereal *c, doublereal *ch, doublereal *wa, integer *ifac);
/*:ref: passb4_ 10 7 3 3 5 5 5 5 5 */
/*:ref: passb2_ 10 5 3 3 5 5 5 */
/*:ref: passb3_ 10 6 3 3 5 5 5 5 */
/*:ref: passb5_ 10 8 3 3 5 5 5 5 5 5 */
/*:ref: passb_ 10 11 3 3 3 3 3 5 5 5 5 5 5 */
extern int cfftf_(integer *n, doublereal *c, doublereal *wsave);
/*:ref: cfftf1_ 10 5 3 5 5 5 5 */
extern int cfftf1_(integer *n, doublereal *c, doublereal *ch, doublereal *wa, integer *ifac);
/*:ref: passf4_ 10 7 3 3 5 5 5 5 5 */
/*:ref: passf2_ 10 5 3 3 5 5 5 */
/*:ref: passf3_ 10 6 3 3 5 5 5 5 */
/*:ref: passf5_ 10 8 3 3 5 5 5 5 5 5 */
/*:ref: passf_ 10 11 3 3 3 3 3 5 5 5 5 5 5 */
extern int cffti_(integer *n, doublereal *wsave);
/*:ref: cffti1_ 10 3 3 5 5 */
extern int cffti1_(integer *n, doublereal *wa, integer *ifac);
extern int cosqb_(integer *n, doublereal *x, doublereal *wsave);
/*:ref: cosqb1_ 10 4 3 5 5 5 */
extern int cosqb1_(integer *n, doublereal *x, doublereal *w, doublereal *xh);
/*:ref: rfftb_ 10 3 3 5 5 */
extern int cosqf_(integer *n, doublereal *x, doublereal *wsave);
/*:ref: cosqf1_ 10 4 3 5 5 5 */
extern int cosqf1_(integer *n, doublereal *x, doublereal *w, doublereal *xh);
/*:ref: rfftf_ 10 3 3 5 5 */
extern int cosqi_(integer *n, doublereal *wsave);
/*:ref: rffti_ 10 2 3 5 */
extern int cost_(integer *n, doublereal *x, doublereal *wsave);
/*:ref: rfftf_ 10 3 3 5 5 */
extern int costi_(integer *n, doublereal *wsave);
/*:ref: rffti_ 10 2 3 5 */
extern int ezfft1_(integer *n, doublereal *wa, integer *ifac);
extern int ezfftb_(integer *n, doublereal *r, doublereal *azero, doublereal *a, doublereal *b, doublereal *wsave);
/*:ref: rfftb_ 10 3 3 5 5 */
extern int ezfftf_(integer *n, doublereal *r, doublereal *azero, doublereal *a, doublereal *b, doublereal *wsave);
/*:ref: rfftf_ 10 3 3 5 5 */
extern int ezffti_(integer *n, doublereal *wsave);
/*:ref: ezfft1_ 10 3 3 5 5 */
extern int passb_(integer *nac, integer *ido, integer *ip, integer *l1, integer *idl1, doublereal *cc, doublereal *c1, doublereal *c2, doublereal *ch, doublereal *ch2, doublereal *wa);
extern int passb2_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1);
extern int passb3_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2);
extern int passb4_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3);
extern int passb5_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3, doublereal *wa4);
extern int passf_(integer *nac, integer *ido, integer *ip, integer *l1, integer *idl1, doublereal *cc, doublereal *c1, doublereal *c2, doublereal *ch, doublereal *ch2, doublereal *wa);
extern int passf2_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1);
extern int passf3_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2);
extern int passf4_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3);
extern int passf5_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3, doublereal *wa4);
extern int radb2_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1);
extern int radb3_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2);
extern int radb4_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3);
extern int radb5_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3, doublereal *wa4);
extern int radbg_(integer *ido, integer *ip, integer *l1, integer *idl1, doublereal *cc, doublereal *c1, doublereal *c2, doublereal *ch, doublereal *ch2, doublereal *wa);
extern int radf2_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1);
extern int radf3_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2);
extern int radf4_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3);
extern int radf5_(integer *ido, integer *l1, doublereal *cc, doublereal *ch, doublereal *wa1, doublereal *wa2, doublereal *wa3, doublereal *wa4);
extern int radfg_(integer *ido, integer *ip, integer *l1, integer *idl1, doublereal *cc, doublereal *c1, doublereal *c2, doublereal *ch, doublereal *ch2, doublereal *wa);
extern int rfftb_(integer *n, doublereal *r, doublereal *wsave);
/*:ref: rfftb1_ 10 5 3 5 5 5 5 */
extern int rfftb1_(integer *n, doublereal *c, doublereal *ch, doublereal *wa, integer *ifac);
/*:ref: radb4_ 10 7 3 3 5 5 5 5 5 */
/*:ref: radb2_ 10 5 3 3 5 5 5 */
/*:ref: radb3_ 10 6 3 3 5 5 5 5 */
/*:ref: radb5_ 10 8 3 3 5 5 5 5 5 5 */
/*:ref: radbg_ 10 10 3 3 3 3 5 5 5 5 5 5 */
extern int rfftf_(integer *n, doublereal *r, doublereal *wsave);
/*:ref: rfftf1_ 10 5 3 5 5 5 5 */
extern int rfftf1_(integer *n, doublereal *c, doublereal *ch, doublereal *wa, integer *ifac);
/*:ref: radf4_ 10 7 3 3 5 5 5 5 5 */
/*:ref: radf2_ 10 5 3 3 5 5 5 */
/*:ref: radf3_ 10 6 3 3 5 5 5 5 */
/*:ref: radf5_ 10 8 3 3 5 5 5 5 5 5 */
/*:ref: radfg_ 10 10 3 3 3 3 5 5 5 5 5 5 */
extern int rffti_(integer *n, doublereal *wsave);
/*:ref: rffti1_ 10 3 3 5 5 */
extern int rffti1_(integer *n, doublereal *wa, integer *ifac);
extern int sinqb_(integer *n, doublereal *x, doublereal *wsave);
/*:ref: cosqb_ 10 3 3 5 5 */
extern int sinqf_(integer *n, doublereal *x, doublereal *wsave);
/*:ref: cosqf_ 10 3 3 5 5 */
extern int sinqi_(integer *n, doublereal *wsave);
/*:ref: cosqi_ 10 2 3 5 */
extern int sint_(integer *n, doublereal *x, doublereal *wsave);
/*:ref: sint1_ 10 6 3 5 5 5 5 5 */
extern int sint1_(integer *n, doublereal *war, doublereal *was, doublereal *xh, doublereal *x, integer *ifac);
/*:ref: rfftf1_ 10 5 3 5 5 5 3 */
extern int sinti_(integer *n, doublereal *wsave);
/*:ref: rffti_ 10 2 3 5 */
/*:ref: rffti_ 10 2 3 5 */
/*:ref: rfftf_ 10 3 3 5 5 */
/*:ref: rfftb_ 10 3 3 5 5 */
/*:ref: sinti_ 10 2 3 5 */
/*:ref: sint_ 10 3 3 5 5 */
/*:ref: costi_ 10 2 3 5 */
/*:ref: cost_ 10 3 3 5 5 */
/*:ref: sinqi_ 10 2 3 5 */
/*:ref: sinqb_ 10 3 3 5 5 */
/*:ref: sinqf_ 10 3 3 5 5 */
/*:ref: cosqi_ 10 2 3 5 */
/*:ref: cosqb_ 10 3 3 5 5 */
/*:ref: cosqf_ 10 3 3 5 5 */
/*:ref: ezffti_ 10 2 3 5 */
/*:ref: ezfftf_ 10 6 3 5 5 5 5 5 */
/*:ref: ezfftb_ 10 6 3 5 5 5 5 5 */
/*:ref: cffti_ 10 2 3 5 */
/*:ref: cfftf_ 10 3 3 6 5 */
/*:ref: cfftb_ 10 3 3 6 5 */

