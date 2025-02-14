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
 * Revision 1.6  2001/03/12 21:41:15  soliday
 * Fixed a problem when "I" is the only character. Redrew it so that it has a
 * width.
 *
 * Revision 1.5  2000/05/16 19:42:10  soliday
 * Cleaned up some Solaris compiler warnings.
 *
 * Revision 1.4  1999/06/03 16:08:27  soliday
 * Removed compiler warnings under linux.
 *
 * Revision 1.3  1998/01/27 00:57:26  borland
 * Changed ix and iy arrays from char to short, since using char's doesn't
 * work on some machines.
 *
 * Revision 1.2  1995/09/05  21:09:36  saunders
 * First test release of the SDDS1.5 package.
 * */

#define MAXCHR 223
#define MAXCODE 3938

short ia[256];
double xp[40], yp[40];

extern short ix[MAXCODE];
extern short iy[MAXCODE];
extern short istart[MAXCHR];

