
/**
 * @file lsfp.c
 * @brief Polynomial least squares fit using specified terms.
 *
 * Implements the `lsfp` routine that computes an nth order polynomial
 * least squares fit using only the terms requested by the caller.
 *
 * Michael Borland, 1986.
 */
#include "matlib.h"
#include "mdb.h"
int p_merror(char *message);

long lsfp(double *xd, double *yd, double *sy, /* data */
          long n_pts,                         /* number of data points */
          long n_terms,                       /* number of terms of the form An.x^n */
          long *power,                        /* power for each term */
          double *coef,                       /* place to put co-efficients */
          double *s_coef,                     /* and their sigmas */
          double *chi,                        /* place to put reduced chi-squared */
          double *diff                        /* place to put difference table    */
) {
  long i, j, unweighted;
  double xp, *x_i, x0;
  MATRIX *X = NULL, *Y = NULL, *Yp = NULL, *C = NULL, *C_1 = NULL, *Xt = NULL, *A = NULL, *Ca = NULL, *XtC = NULL, *XtCX = NULL, *T = NULL, *Tt = NULL, *TC = NULL;
  long status = 0;

  if (n_pts < n_terms) {
    printf("error: insufficient data for requested order of fit\n");
    printf("(%ld data points, %ld terms in fit)\n", n_pts, n_terms);
    exit(1);
  }

  unweighted = 1;
  if (sy)
    for (i = 1; i < n_pts; i++)
      if (sy[i] != sy[0]) {
        unweighted = 0;
        break;
      }

  /* allocate matrices */
  m_alloc(&X, n_pts, n_terms);
  m_alloc(&Y, n_pts, 1);
  m_alloc(&Yp, n_pts, 1);
  m_alloc(&Xt, n_terms, n_pts);
  if (!unweighted) {
    m_alloc(&C, n_pts, n_pts);
    m_alloc(&C_1, n_pts, n_pts);
    m_zero(C);
    m_zero(C_1);
  }
  m_alloc(&A, n_terms, 1);
  m_alloc(&Ca, n_terms, n_terms);
  m_alloc(&XtC, n_terms, n_pts);
  m_alloc(&XtCX, n_terms, n_terms);
  m_alloc(&T, n_terms, n_pts);
  m_alloc(&Tt, n_pts, n_terms);
  m_alloc(&TC, n_terms, n_pts);

  /* Compute X, Y, C, C_1.  X[i][j] = (xd[i])^power[j]. Y[i][0] = yd[i].  
   * C   = delta(i,j)*sy[i]^2  (covariance matrix of yd)            
   * C_1 = INV(C)                                                        
   */
  for (i = 0; i < n_pts; i++) {
    x_i = X->a[i];
    Y->a[i][0] = yd[i];
    if (!unweighted) {
      C->a[i][i] = sqr(sy[i]);
      C_1->a[i][i] = 1 / C->a[i][i];
    }
    x0 = xd[i];
    for (j = 0; j < n_terms; j++)
      x_i[j] = ipow(x0, power[j]);
  }

  /* Compute A, the matrix of coefficients.
   * Weighted least-squares solution is A = INV(Xt.INV(C).X).Xt.INV(C).y 
   * Unweighted solution is A = INV(Xt.X).Xt.y 
   */
  if (unweighted) {
    /* eliminating 2 matrix operations makes this much faster than a weighted fit
       * if there are many data points.
       */
    if (!m_trans(Xt, X))
      { status = p_merror("transposing X"); goto cleanup; }
    if (!m_mult(XtCX, Xt, X))
      { status = p_merror("multiplying Xt.X"); goto cleanup; }
    if (!m_invert(XtCX, XtCX))
      { status = p_merror("inverting XtCX"); goto cleanup; }
    if (!m_mult(T, XtCX, Xt))
      { status = p_merror("multiplying XtX.Xt"); goto cleanup; }
    if (!m_mult(A, T, Y))
      { status = p_merror("multiplying T.Y"); goto cleanup; }

    /* Compute covariance matrix of A, Ca = (T.Tt)*C[0][0] */
    if (!m_trans(Tt, T))
      { status = p_merror("computing transpose of T"); goto cleanup; }
    if (!m_mult(Ca, T, Tt))
      { status = p_merror("multiplying T.Tt"); goto cleanup; }
    if (!m_scmul(Ca, Ca, sy ? sqr(sy[0]) : 1))
      { status = p_merror("multiplying T.Tt by scalar"); goto cleanup; }
  } else {
    if (!m_trans(Xt, X))
      { status = p_merror("transposing X"); goto cleanup; }
    if (!m_mult(XtC, Xt, C_1))
      { status = p_merror("multiplying Xt.C_1"); goto cleanup; }
    if (!m_mult(XtCX, XtC, X))
      { status = p_merror("multiplying XtC.X"); goto cleanup; }
    if (!m_invert(XtCX, XtCX))
      { status = p_merror("inverting XtCX"); goto cleanup; }
    if (!m_mult(T, XtCX, XtC))
      { status = p_merror("multiplying XtCX.XtC"); goto cleanup; }
    if (!m_mult(A, T, Y))
      { status = p_merror("multiplying T.Y"); goto cleanup; }

    /* Compute covariance matrix of A, Ca = T.C.Tt */
    if (!m_mult(TC, T, C))
      { status = p_merror("multiplying T.C"); goto cleanup; }
    if (!m_trans(Tt, T))
      { status = p_merror("computing transpose of T"); goto cleanup; }
    if (!m_mult(Ca, TC, Tt))
      { status = p_merror("multiplying TC.Tt"); goto cleanup; }
  }

  for (i = 0; i < n_terms; i++) {
    coef[i] = A->a[i][0];
    s_coef[i] = sqrt(Ca->a[i][i]);
  }

  /* Compute Yp = X.A, use to compute chi-squared */
  if (!m_mult(Yp, X, A))
    { status = p_merror("multiplying X.A"); goto cleanup; }
  *chi = 0;
  for (i = 0; i < n_pts; i++) {
    xp = (Yp->a[i][0] - yd[i]);
    if (diff != NULL)
      diff[i] = xp;
    xp /= sy ? sy[i] : 1;
    *chi += xp * xp;
  }
  if (n_terms != n_pts)
    *chi /= (n_pts - n_terms);

  status = 1;

cleanup:
  m_free(&X);
  m_free(&Y);
  m_free(&Yp);
  m_free(&Xt);
  if (!unweighted) {
    m_free(&C);
    m_free(&C_1);
  }
  m_free(&A);
  m_free(&Ca);
  m_free(&XtC);
  m_free(&XtCX);
  m_free(&T);
  m_free(&Tt);
  m_free(&TC);
  return (status);
}
