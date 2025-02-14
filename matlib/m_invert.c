/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routine: m_invert()
 * purpose: invert a matrix
 * usage: m_invert(A, B) ==>  A=INV(B); A and B must point to matrix
 * 	  structures of the same size.
 * Michael Borland, 1986 (after CITLIB routine MATINV)
 $Log: not supported by cvs2svn $
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


int mat_invert(MATRIX *A, MATRIX *B)         /* A=inv(B) */
{
    register long i, j, k, l, m, n;
    static long *ipivot=NULL, **index=NULL, *ind_l;
    static double *pivot=NULL, piv;
    static double t, swap, amax, *tmp, abs_amax; 
    static long row, col, max_n=0;
    static double *a_j, *a_col, *a_m;
    long log10_t, log10_piv;

    if (!A)
        bomb("NULL matrix (A) passed to m_invert", NULL);
    if (!B)
        bomb("NULL matrix (B) passed to m_invert", NULL);
    if (!A->a)
        bomb("NULL matrix data (A) in m_invert", NULL);
    if (!B->a)
        bomb("NULL matrix data (A) in m_invert", NULL);

    if ((n=A->n)!=A->m || A->m!=B->n || B->n!=B->m) {
        fprintf(stderr, "matrix size mismatch (m_invert)\n");
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
        bomb("pivot index array is NULL (m_invert)", NULL);
    if (!index)
        bomb("index array is NULL (m_invert)", NULL);
    if (!pivot)
        bomb("pivot array is NULL (m_invert)", NULL);

    m_copy(A, B);
    for (j=0; j<n; j++) 
	ipivot[j] = 0;

    for (i=0; i<n; i++) {
	/* search for pivot element */
	amax = abs_amax = 0;
	for (j=0; j<n; j++) {    
    	    if (!(a_j = A->a[j])) {
                fprintf(stderr, "row %ld of matrix A is NULL (m_invert):\n", j);
                m_show(A, "%14.6e ", NULL, stderr);
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
            bomb("pivot index array has NULL element (m_invert)", NULL);
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
                fprintf(stderr, "error: floating overflow in m_invert (pivot too small):\n");
                m_show(A, "%14.6e ", NULL, stderr);
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
                        fprintf(stderr, "error: floating-point overflow in m_invert():\n");
                        m_show(A, "%14.6e ", NULL, stderr);
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
            bomb("row out of range in column interchange (m_invert)", NULL);
	if ((col = ind_l[1])<0 || col>n)
            bomb("column out of range in column interchange (m_invert)", NULL);
	for (j=0; j<n; j++) {
	    a_j = A->a[j];
	    swap = a_j[row];
	    a_j[row] = a_j[col];
	    a_j[col] = swap;
	    }
	}
    return(1);
    }


