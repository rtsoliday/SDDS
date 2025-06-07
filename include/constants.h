/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file   : constants.h
 * purpose: mathematical and physical constants
 *
 * Michael Borland, 1991
 *
 */

#ifndef MDB_CONSTANTS_INCLUDED
/* mathematical constants from Abramowitz and Stegun */
#undef PI
#define PI   3.141592653589793238462643
#define PIx2 6.283185307179586476925287
#define PIo2 1.570796326794896619231322
#define SQRT2 1.4142135623730950488

/* physical constants from 1988 Particle Properties Data Booklet */
    /* speed of light */
#define c_cgs	(2.99792458e10)
#define c_mks   (2.99792458e8)
    /* unit charge */
#define e_cgs   (4.8032068e-10)
#define e_mks   (1.60217733e-19)
    /* electron mass */
#define me_cgs (9.1093897e-28)
#define me_mks (9.1093897e-31)
#define me_mev (0.51099906)
    /* classical electron radius */
#define re_cgs (2.81794092E-13)
#define re_mks (2.81794092E-15)

    /* Planck's constant */
#define h_cgs (6.6260755e-27)
#define hbar_cgs (h_cgs/PIx2)
#define h_mks (6.6260755e-34)
#define hbar_mks (h_mks/PIx2)
    /* Boltzmann's constant */
#define k_boltzmann_cgs (1.380658e-16)
#define k_boltzmann_mks (1.380658e-23)
    /* mu and epsilon */
#define mu_o (4*PI*1e-7)
#define epsilon_o (1.e7/(4*PI*sqr(c_mks)))

#define NAvogadro (6.02214129e23)

#define MDB_CONSTANTS_INCLUDED 1
#endif
