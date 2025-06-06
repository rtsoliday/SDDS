/*
/////////////////////////////////////////////////////////////////////////////////
// 
//  Prototypes and definitions for the Levenberg - Marquardt minimization algorithm
//  Copyright (C) 2004  Manolis Lourakis (lourakis@ics.forth.gr)
//  Institute of Computer Science, Foundation for Research & Technology - Hellas
//  Heraklion, Crete, Greece.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
/////////////////////////////////////////////////////////////////////////////////
*/
#ifndef _LM_H_
#define _LM_H_

#ifdef SUNPERF
#define HAVE_LAPACK /* // comment this if you do not have LAPACK */
#endif

#define LINSOLVERS_RETAIN_MEMORY /* // comment this is if you don't want routines in Axb.c retain working memory between calls */

/* no changes necessary beyond this point */

#ifdef __cplusplus
extern "C" {
#endif


#define FABS(x) (((x)>=0.0)? (x) : -(x))

  /* work arrays size for LM with & without jacobian, should be multiplied by sizeof(double)
   * or sizeof(float) to be converted to bytes
   */
#define LM_DER_WORKSZ(npar, nmeas) (2*(nmeas) + 4*(npar) + (nmeas)*(npar) + (npar)*(npar))
#define LM_DIF_WORKSZ(npar, nmeas) (3*(nmeas) + 4*(npar) + (nmeas)*(npar) + (npar)*(npar))

#define LM_OPTS_SZ    	 5 /* max(4, 5) */
#define LM_INFO_SZ    	 9
#define LM_INIT_MU    	 1E-03
#define LM_STOP_THRESH	 1E-17
#define LM_DIFF_DELTA    1E-06
#define LM_VERSION       "2.0 (Feb. 2005)"

  /* double precision LM, with & without jacobian */
  /* unconstrained minimization */
  extern int dlevmar_der(
                         void (*func)(double *p, double *hx, int m, int n, void *adata),
                         void (*jacf)(double *p, double *j, int m, int n, void *adata),
                         double *p, double *x, int m, int n, int itmax, double opts[4],
                         double info[LM_INFO_SZ], double *work, double *covar, void *adata);

  extern int dlevmar_dif(
                         void (*func)(double *p, double *hx, int m, int n, void *adata),
                         double *p, double *x, int m, int n, int itmax, double opts[5],
                         double info[LM_INFO_SZ], double *work, double *covar, void *adata);

  /* box-constrained minimization */
  extern int dlevmar_bc_der(
                            void (*func)(double *p, double *hx, int m, int n, void *adata),
                            void (*jacf)(double *p, double *j, int m, int n, void *adata),  
                            double *p, double *x, int m, int n, double *lb, double *ub,
                            int itmax, double opts[4], double info[LM_INFO_SZ], double *work, double *covar, void *adata);

  extern int dlevmar_bc_dif(
                            void (*func)(double *p, double *hx, int m, int n, void *adata),
                            double *p, double *x, int m, int n, double *lb, double *ub,
                            int itmax, double opts[5], double info[LM_INFO_SZ], double *work, double *covar, void *adata);

#ifdef HAVE_LAPACK
  /* linear equation constrained minimization */
  extern int dlevmar_lec_der(
                             void (*func)(double *p, double *hx, int m, int n, void *adata),
                             void (*jacf)(double *p, double *j, int m, int n, void *adata),
                             double *p, double *x, int m, int n, double *A, double *b, int k,
                             int itmax, double opts[4], double info[LM_INFO_SZ], double *work, double *covar, void *adata);

  extern int dlevmar_lec_dif(
                             void (*func)(double *p, double *hx, int m, int n, void *adata),
                             double *p, double *x, int m, int n, double *A, double *b, int k,
                             int itmax, double opts[5], double info[LM_INFO_SZ], double *work, double *covar, void *adata);
#endif /* HAVE_LAPACK */


  /* single precision LM, with & without jacobian */
  /* unconstrained minimization */
  extern int slevmar_der(
                         void (*func)(float *p, float *hx, int m, int n, void *adata),
                         void (*jacf)(float *p, float *j, int m, int n, void *adata),
                         float *p, float *x, int m, int n, int itmax, float opts[4],
                         float info[LM_INFO_SZ], float *work, float *covar, void *adata);

  extern int slevmar_dif(
                         void (*func)(float *p, float *hx, int m, int n, void *adata),
                         float *p, float *x, int m, int n, int itmax, float opts[5],
                         float info[LM_INFO_SZ], float *work, float *covar, void *adata);

  /* box-constrained minimization */
  extern int slevmar_bc_der(
                            void (*func)(float *p, float *hx, int m, int n, void *adata),
                            void (*jacf)(float *p, float *j, int m, int n, void *adata),  
                            float *p, float *x, int m, int n, float *lb, float *ub,
                            int itmax, float opts[4], float info[LM_INFO_SZ], float *work, float *covar, void *adata);

  extern int slevmar_bc_dif(
                            void (*func)(float *p, float *hx, int m, int n, void *adata),
                            float *p, float *x, int m, int n, float *lb, float *ub,
                            int itmax, float opts[5], float info[LM_INFO_SZ], float *work, float *covar, void *adata);

#ifdef HAVE_LAPACK
  /* linear equation constrained minimization */
  extern int slevmar_lec_der(
                             void (*func)(float *p, float *hx, int m, int n, void *adata),
                             void (*jacf)(float *p, float *j, int m, int n, void *adata),
                             float *p, float *x, int m, int n, float *A, float *b, int k,
                             int itmax, float opts[4], float info[LM_INFO_SZ], float *work, float *covar, void *adata);

  extern int slevmar_lec_dif(
                             void (*func)(float *p, float *hx, int m, int n, void *adata),
                             float *p, float *x, int m, int n, float *A, float *b, int k,
                             int itmax, float opts[5], float info[LM_INFO_SZ], float *work, float *covar, void *adata);
#endif /* HAVE LAPACK */

  /* linear system solvers */
#ifdef HAVE_LAPACK
  extern int dAx_eq_b_QR(double *A, double *B, double *x, int m);
  extern int dAx_eq_b_QRLS(double *A, double *B, double *x, int m, int n);
  extern int dAx_eq_b_Chol(double *A, double *B, double *x, int m);
  extern int dAx_eq_b_LU(double *A, double *B, double *x, int m);
  extern int dAx_eq_b_SVD(double *A, double *B, double *x, int m);

  extern int sAx_eq_b_QR(float *A, float *B, float *x, int m);
  extern int sAx_eq_b_QRLS(float *A, float *B, float *x, int m, int n);
  extern int sAx_eq_b_Chol(float *A, float *B, float *x, int m);
  extern int sAx_eq_b_LU(float *A, float *B, float *x, int m);
  extern int sAx_eq_b_SVD(float *A, float *B, float *x, int m);
#else /* // no LAPACK */
  extern int dAx_eq_b_LU_noLapack(double *A, double *B, double *x, int n);

  extern int sAx_eq_b_LU_noLapack(float *A, float *B, float *x, int n);
#endif /* HAVE_LAPACK */

  /* jacobian verification, double & single precision */
  extern void dlevmar_chkjac(
                             void (*func)(double *p, double *hx, int m, int n, void *adata),
                             void (*jacf)(double *p, double *j, int m, int n, void *adata),
                             double *p, int m, int n, void *adata, double *err);

  extern void slevmar_chkjac(
                             void (*func)(float *p, float *hx, int m, int n, void *adata),
                             void (*jacf)(float *p, float *j, int m, int n, void *adata),
                             float *p, int m, int n, void *adata, float *err);

#ifdef __cplusplus
}
#endif

#endif /* _LM_H_ */
