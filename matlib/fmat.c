/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_add()
 * purpose: add two matrices
 * usage  : m_add(C, A, B) ==> C = A+B
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:37  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:03  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

int fmat_add(FMATRIX *C, FMATRIX *A, FMATRIX *B)
{
    register int i, j;
    int a_n, a_m;
    register float *a_i, *b_i, *c_i;

    if ((a_n=A->n)!=B->n || B->n!=C->n || (a_m=A->m)!=B->m || B->m!=C->m)
        return(0);
    for (i=0; i<a_n; i++) {
        a_i = A->a[i];
        b_i = B->a[i];
        c_i = C->a[i];
        for (j=0; j<a_m; j++) 
            c_i[j] = a_i[j] + b_i[j];
        }
     return(1);
     }

/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: m_alloc()
 * purpose: allocate storage for a matrix 
 * usage  : 
 *   MATRIX *A;
 *   int n, m;
 *   m_alloc(&A, n, m)
 * 
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.4  1999/05/28 14:55:29  soliday
 Removed compiler warnings under Linux.

 Revision 1.3  1998/04/21 21:26:37  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:05  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "matlib.h"

void fmat_alloc(FMATRIX **A, int n, int m)
{
    register long i;
  
    if (n<=0)
        bomb("error in m_alloc: 0xn array requested", NULL);
    if ((*A = (FMATRIX*)tmalloc(sizeof(**A)))) {
        if (((*A)->a = (float**)tmalloc(sizeof(float*)*n))) {
            (*A)->n = n;
            (*A)->m = m;
            if (m!=0) {
                /* m==0 means only row pointers were wanted */
                for (i=0; i<n; i++) {
                    if (!((*A)->a[i] = (float*)tmalloc(m*sizeof(float))))
	                break;
                    }
                if (i==n) {
                    fmat_zero(*A);
                    return;
                    }
                }
            }
        }
    puts("Allocation failure in fmat_alloc().");
    abort();
    }

void fmat_alloc1(FMATRIX **A, int n, int m)
{
    if (n<=0)
        bomb("error in fmat_alloc1: 0xn array requested", NULL);
    if (!*A || !(*A)->a)
        fmat_alloc(A, n, m);
    else if ((*A)->n!=n || (*A)->m!=m) {
        fmat_free(A);
        fmat_alloc(A, n, m);
        }
    }

#include "matlib.h"

int fmat_check(FMATRIX *A)
{
    register int i, a_n;
  
    a_n = A->n;
    if (!(A->a))
        return(0);
    for (i=0; i<a_n; i++) {
        if (!(A->a)[i])
            return(0);
        }
    return(1);
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_copy()
 * purpose: copy a matrix
 * usage:
 *   MATRIX *A, *B;
 *   ...
 *   fmat_copy(A, B);	! copy B to A 
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:38  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:10  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

int fmat_copy(FMATRIX *A, FMATRIX *B)
{
    register int i, j, a_m, a_n;
    register float *a_i, *b_i;
  
    if ((a_n=A->n)!=B->n || (a_m=A->m)!=B->m) 
        return(0);
    for (i=0; i<a_n; i++) {
        a_i = (A->a)[i];
        b_i = (B->a)[i];
        for (j=0; j<a_m; j++)
            a_i[j] = b_i[j];
        }
    return(1);
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_det()
 * purpose: compute determinant of a matrix 
 * usage:
 *   MATRIX *A;
 *   double det;
 *   ...
 *   det = fmat_det(A);
 *
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.5  2002/03/21 16:50:32  shang
 replaced A->a[j][j]!=0 (which is a bug) by A->a[j][i]!=0

 Revision 1.4  2000/01/19 14:43:05  borland
 Fixed memory leak.

 Revision 1.3  1998/04/21 21:26:38  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:12  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

#define TMP a_j

float fmat_det(FMATRIX *D)
{
    register long i, j, k, n;
    register float det, *a_i, *a_j, a_i_i;
    FMATRIX *A;

    det = 1.;
    if ((n=D->n)!=D->m)
        return(0.);

    fmat_alloc(&A, n, n);
    if (!fmat_copy(A, D)) {
      fmat_free(&A);
      return(0.);
    }
    
    /* first diagonalize the matrix */
    for (i=0; i<(n-1); i++) {
        if (A->a[i][i]==0.) {
            for (j=0; j<n; j++) 
                if (A->a[j][i]!=0)  
                    break;
            if (j==n)
                return(0.);
            TMP     = A->a[i];
            A->a[i] = A->a[j];
            A->a[j] = TMP;
            det *= -1.0;
            }
        det *= A->a[i][i];
        a_i = A->a[i];
        a_i_i = a_i[i];
        for (j=i+1; j<n; j++) {
            a_j = A->a[j];
            for (k=i+1; k<n; k++)
                a_j[k] = a_j[k] - a_i[k]*a_j[i]/a_i_i;
            }
        }
    det = det*A->a[i][i];
    fmat_free(&A);
    return(det);
    }
 

/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:39  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:14  saunders
 * First test release of the SDDS1.5 package.
 *
*/

#include "mdb.h"
#include "matlib.h"
void fmat_diag(FMATRIX *A, float *diagonal_elements) {
    long i;
    
    if(!A||!A->a)
        bomb("Problem with allocation of matrix.\n",NULL);
    if(!diagonal_elements)
        bomb("Problem with allocation of vector of diagonal elements.\n",NULL);
    fmat_zero(A);
    for(i=0;i<MIN(A->n,A->m);i++)
        A->a[i][i]=diagonal_elements[i];
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_free()
 * purpose: free storage allocated to a matrix 
 * usage  :
 *   MATRIX *A;
 *   long n, m;
 *   fmat_alloc(&A, n, m);
 *     ...
 *   fmat_free(&A);
 *
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.4  1999/12/15 16:56:23  soliday
 Added support for vxWorks.

 Revision 1.3  1998/04/21 21:26:39  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:17  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"
#if defined(vxWorks)
#include <stdlib.h>
#else
#if !defined(__APPLE__)
#include <malloc.h>
#endif
#endif

void fmat_free(FMATRIX **A)
{
    register long i, n;

    if (!A || !*A || !(*A)->a)
        return;
    n = (*A)->n;
    for (i=0; i<n; i++) {
        if ((*A)->a[i])
            free((*A)->a[i]);
        (*A)->a[i] = NULL;
        }
    free((*A)->a);
    (*A)->a = NULL;
    free(*A);
    *A = NULL;
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this fmaterial resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_identity()
 * purpose: set elements of a matrix to zero.
 * usage:
 *  MATRIX *A;
 *  ...
 *  fmat_identity(A);
 *
 * Michael Borland, 1989.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:40  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:19  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

void fmat_identity(FMATRIX *A)
{
    register long i, j, n, m;
    register float *a_i;

    n = A->n;
    m = A->m;
    for (i=0; i<n; i++) {
        a_i = A->a[i];
        for (j=0; j<m; j++) 
            a_i[j] = (i==j?1:0);
        }
    }

/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_invert()
 * purpose: invert a matrix
 * usage: fmat_invert(A, B) ==>  A=INV(B); A and B must point to matrix
 * 	  structures of the same size.
 * Michael Borland, 1986 (after CITLIB routine MATINV)
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:40  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:21  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"
#include "mdb.h"
#include "float.h"

#define NDECADES (DBL_MAX_EXP-1)


int fmat_invert(FMATRIX *A, FMATRIX *B)         /* A=inv(B) */
{
    register long i, j, k, l, m, n;
    static long *ipivot=NULL, **index=NULL, *ind_l;
    static float *pivot=NULL, piv;
    static float t, swap, amax, *tmp, abs_amax; 
    static long row, col, max_n=0;
    static float *a_j, *a_col, *a_m;
    long log10_t, log10_piv;

    if (!A)
        bomb("NULL matrix (A) passed to fmat_invert", NULL);
    if (!B)
        bomb("NULL matrix (B) passed to fmat_invert", NULL);
    if (!A->a)
        bomb("NULL matrix data (A) in fmat_invert", NULL);
    if (!B->a)
        bomb("NULL matrix data (A) in fmat_invert", NULL);

    if ((n=A->n)!=A->m || A->m!=B->n || B->n!=B->m) {
        fprintf(stderr, "matrix size mismatch (fmat_invert)\n");
	return(0);
        }

    if (max_n<n) {
        ipivot = trealloc(ipivot, sizeof(*ipivot)*n);
        pivot  = trealloc(pivot, sizeof(*pivot)*n);
        if (!index || !max_n)
            index = (long**)zarray_2d(sizeof(**index), n, 2);
        else
            index = (long**)resize_zarray_2d(sizeof(**index), max_n, 2, (void**)index, n, 2);
        max_n = n;
        }
    if (!ipivot)
        bomb("pivot index array is NULL (fmat_invert)", NULL);
    if (!index)
        bomb("index array is NULL (fmat_invert)", NULL);
    if (!pivot)
        bomb("pivot array is NULL (fmat_invert)", NULL);

    fmat_copy(A, B);
    for (j=0; j<n; j++) 
	ipivot[j] = 0;

    for (i=0; i<n; i++) {
	/* search for pivot element */
	amax = abs_amax = 0;
	for (j=0; j<n; j++) {    
    	    if (!(a_j = A->a[j])) {
                fprintf(stderr, "row %ld of matrix A is NULL (fmat_invert):\n", j);
                fmat_show(A, "%14.6e ", NULL, stderr);
                abort();
                }
	    if (ipivot[j]!=1) {
		for (k=0; k<n; k++) {
		    if (ipivot[k]>1)
			return(0);
		    if (ipivot[k]!=1 && abs_amax<fabs(a_j[k])) {
			col = k;
			row = j;
			abs_amax = fabs(amax = a_j[k]);
			}
		    }
		}
	    }
        if (amax == 0.)
    	    return(0);
        ipivot[col] = ipivot[col] + 1;
        /* interchange rows to put pivot element on diagonal */
        if (row!=col) {
	    tmp       = A->a[row];
	    A->a[row] = A->a[col];
	    A->a[col] = tmp;
	    }
        if (!index[i])
            bomb("pivot index array has NULL element (fmat_invert)", NULL);
        index[i][0] = row;
        index[i][1] = col;
        pivot[i] = A->a[col][col];
        /* divide pivot row by pivot element */
        A->a[col][col] = 1.;
        a_col = A->a[col];
        piv = pivot[i];
        log10_piv = log10(FABS(piv))+0.5;
        for (l=0; l<n; l++) {
            if (a_col[l]!=0 && ((long)log10(FABS(a_col[l]))-log10_piv > NDECADES)) {
                fprintf(stderr, "error: floating overflow in fmat_invert (pivot too small):\n");
                fmat_show(A, "%14.6e ", NULL, stderr);
                return(0);
                }
	    a_col[l] /= piv;
            }
        /* reduce non-pivot rows */
        for (m=0; m<n; m++) {
    	    if (m!=col) {
	    	a_m = A->a[m];
	        t = a_m[col];
	        a_m[col] = 0.;
                if (t!=0)
                    log10_t = log10(FABS(t))+0.5;
                else
                    log10_t = 0;
	        for (l=0; l<n; l++) {
                    if ((a_col[l]!=0?(long)log10(FABS(a_col[l])+0.5):0)+log10_t > NDECADES) {
                        fprintf(stderr, "error: floating-point overflow in fmat_invert():\n");
                        fmat_show(A, "%14.6e ", NULL, stderr);
                        return(0);
                        }
		    a_m[l] -= a_col[l]*t;
                    }
		}
	    }
	}

    /* interchange columns */
    for (i=0; i<n; i++) {
	l = n - i - 1;
	ind_l = index[l];
	if (ind_l[0] == ind_l[1])
	    continue;
	if ((row = ind_l[0])<0 || row>n) 
            bomb("row out of range in column interchange (fmat_invert)", NULL);
	if ((col = ind_l[1])<0 || col>n)
            bomb("column out of range in column interchange (fmat_invert)", NULL);
	for (j=0; j<n; j++) {
	    a_j = A->a[j];
	    swap = a_j[row];
	    a_j[row] = a_j[col];
	    a_j[col] = swap;
	    }
	}
    return(1);
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_mult()
 * purpose: multiply two matrices
 * usage  : fmat_mult(C, A, B)  ==>  C= AB
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:41  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:23  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

int fmat_mult(
    FMATRIX *C, FMATRIX *A, FMATRIX *B
    )
{
    register long i, j, k;
    long n, m, p;
    register float *a_i, *c_i, c_i_j;

    if ((m=A->m)!=B->n || (n=A->n)!=C->n || (p=B->m)!=C->m) 
        return(0);
    for (i=0; i<n; i++) {
        a_i = A->a[i];
        c_i = C->a[i];
        for (j=0; j<p; j++) {
            c_i_j = 0;
            for (k=0; k<m; k++) 
	        c_i_j += a_i[k]*B->a[k][j];
            c_i[j] = c_i_j;
            }
        }
    return(1);
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:41  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:24  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* routine: fmat_scmul()
 * purpose: multiply a matrix by a scalar
 * usage  : scmul(B, A, a)  ==>  B=aA
 */
#include "matlib.h"

int fmat_scmul(
    FMATRIX *B, FMATRIX *A,
    float a
    )
{
    register long i, j;
    long a_n, a_m;
    register float afast, *a_i, *b_i;

    afast = a;
    if ((a_n=A->n)!=B->n || (a_m=A->m)!=B->m)
        return(0);
    for (i=0; i<a_n; i++) {
        a_i = A->a[i];
        b_i = B->a[i];
        for (j=0; j<a_m; j++) 
            b_i[j] = afast*a_i[j];
        }
    return(1);
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_show()
 * purpose: to display a matrix on the screen 
 * usage:
 *   MATRIX *A;
 *   ...
 *   fmat_show(A, "%.4f  ", "label", stdout);   ! print w/a certain format 
 *   
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:41  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:26  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "matlib.h"

void fmat_show(
    FMATRIX *A,
    char *format,
    char *label,
    FILE *fp
    )
{
    register long i, j;

    if (label)
        fputs(label, fp);
    for (i=0; i<A->n; i++) {
        for (j=0; j<A->m; j++)
            fprintf(fp, format, A->a[i][j]);
        fputc('\n', fp);
        }
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_trans()
 * purpose: transpose a matrix 
 * usage  : fmat_trans(B, A) ==> B=TRANS(A)
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:42  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:28  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

int fmat_trans(FMATRIX *B, FMATRIX *A)
{
    register long i, j;
    register float *a_i, a_m, a_n;
    
    if (A->m!=B->n || A->n!=B->m)
        return(0);
    a_m = A->m;
    a_n = A->n;
    for (i=0; i<a_n; i++) {
        a_i = A->a[i];
        for (j=0; j<a_m; j++)
            B->a[j][i] = a_i[j];
        }
    return(1);
    }


/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001*/

/* routine: fmat_zero()
 * purpose: set elements of a matrix to zero.
 * usage:
 *  MATRIX *A;
 *  ...
 *  fmat_zero(A);
 *
 * Michael Borland, 1986.
 $Log: not supported by cvs2svn $
 Revision 1.2  2002/08/08 15:17:32  soliday
 Added support for OSX.

 Revision 1.1  2002/08/07 18:52:55  borland
 First version.  Single-precision routines.

 Revision 1.3  1998/04/21 21:26:42  borland
 New names to allow concurrent use with the Meschach library.

 * Revision 1.2  1995/09/05  21:16:30  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "matlib.h"

void fmat_zero(FMATRIX *A)
{
    register long i, j, n, m;
    register float *a_i;

    if (!A || !A->a)
        mat_error("null matrix passed to fmat_zero()");
    n = A->n;
    m = A->m;
    for (i=0; i<n; i++) {
        if (!(a_i = A->a[i]))
            mat_error("null pointer in matrix passed to fmat_zero()");
        for (j=0; j<m; j++) 
            a_i[j] = 0;
        }
    }

