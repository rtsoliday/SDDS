/* memory.c, rewrote from meschach (H SHANG)*/
#if defined(__APPLE__)
#  define ACCELERATE_NEW_LAPACK
#  include <Accelerate/Accelerate.h>
#endif
#include "SDDS.h"
#include "mdb.h"
#include "matrixop.h"

#if !defined(HUGE)
#  define HUGE DBL_MAX
#endif

#ifdef MKL
#  include "mkl.h"
int dgemm_(char *transa, char *transb, const MKL_INT *m, const MKL_INT *n, const MKL_INT *k, double *alpha, double *a, const MKL_INT *lda, double *b, const MKL_INT *ldb, double *beta, double *c, const MKL_INT *ldc);
#endif

#ifdef CLAPACK
#  if !defined(_WIN32) && !defined(__APPLE__)
#    include "cblas.h"
#  endif
#  ifdef F2C
#    include "f2c.h"
#  endif
#  if !defined(__APPLE__)
#    include "clapack.h"
#  endif
#  if defined(_WIN32)
int f2c_dgemm(char *transA, char *transB, integer *M, integer *N, integer *K,
              doublereal *alpha,
              doublereal *A, integer *lda,
              doublereal *B, integer *ldb,
              doublereal *beta,
              doublereal *C, integer *ldc);
#  endif
#endif

#ifdef LAPACK
#  include "f2c.h"
#  include <lapacke.h>
int dgemm_(char *transa, char *transb, lapack_int *m, lapack_int *n, lapack_int *k, double *alpha, double *a, lapack_int *lda, double *b, lapack_int *ldb, double *beta, double *c, lapack_int *ldc);
#endif

/* m_get -- gets an mxn matrix (in MAT form) by dynamic memory allocation in column major order*/
MAT *matrix_get(int m, int n) {
  MAT *matrix;
  int i;

  if (m < 0 || n < 0)
    SDDS_Bomb("Error for matrix_get, invalid row and/or column value provided.");
  if ((matrix = NEW(MAT)) == (MAT *)NULL)
    SDDS_Bomb("Unable to allocate memory for matrix (matrix_get (1))");

  matrix->m = m;
  matrix->n = matrix->max_n = n;
  matrix->max_m = m;
  matrix->max_size = m * n;

  if ((matrix->base = NEW_A(m * n, Real)) == (Real *)NULL) {
    free(matrix);
    SDDS_Bomb("Unable to allocate memory for matrix->base (matrix_get (2))");
  }
  if ((matrix->me = (Real **)calloc(n, sizeof(Real *))) == (Real **)NULL) {
    free(matrix->base);
    free(matrix);
    SDDS_Bomb("Unable to allocate memory for matrix->me (matrix_get (3))");
  }
  /*the 0 column data is from base[0] to base[m-1]; and 1 column data is
    from base[m] to base[2m-1]; etc...*/
  for (i = 0; i < n; i++)
    matrix->me[i] = &(matrix->base[i * matrix->m]);

  return matrix;
}

VEC *vec_get(int size) {
  VEC *vector;

  if (size < 0)
    SDDS_Bomb("negative size provided for v_get");

  if ((vector = NEW(VEC)) == (VEC *)NULL)
    SDDS_Bomb("Unable to allocate memory for vector(v_get)");

  vector->dim = vector->max_dim = size;
  if ((vector->ve = NEW_A(size, Real)) == (Real *)NULL) {
    free(vector);
    SDDS_Bomb("Unable to allocate memory for vector(v_get)");
  }
  return (vector);
}

int matrix_free(MAT *mat) {
  if (mat == (MAT *)NULL || (int)(mat->m) < 0 ||
      (int)(mat->n) < 0)
    /* don't trust it */
    return (-1);
  if (mat->me)
    free(mat->me);
  if (mat->base)
    free(mat->base);
  if (mat)
    free(mat);
  mat = (MAT *)NULL;
  return 0;
}

int vec_free(VEC *vec) {
  if (vec == (VEC *)NULL || (int)(vec->dim) < 0)
    /* don't trust it */
    return (-1);
  if (vec->ve)
    free((char *)vec->ve);
  free((char *)vec);
  vec = (VEC *)NULL;
  return (0);
}

MAT *matrix_copy(MAT *mat) {
  MAT *matrix = NULL;
  if (!mat)
    return NULL;
  matrix = matrix_get(mat->m, mat->n);
  memcpy((char *)matrix->base, (char *)mat->base, sizeof(*mat->base) * mat->m * mat->n);
  return matrix;
}

MAT *matrix_transpose(MAT *A) {
  register long i, j;
  MAT *matrix;

  if (!A)
    return NULL;
  matrix = matrix_get(A->n, A->m);
  for (i = 0; i < matrix->n; i++)
    for (j = 0; j < matrix->m; j++)
      matrix->me[i][j] = A->me[j][i];
  return matrix;
}

MAT *matrix_add(MAT *mat1, MAT *mat2) {
  int i;
  MAT *matrix = NULL;
  if (!mat1 || !mat2)
    SDDS_Bomb("Memory is not allocated for the addition matrices.\n");
  if (mat1->m != mat2->m || mat1->n != mat2->n)
    SDDS_Bomb("The input matrix rows and columns do not match!");
  matrix = matrix_get(mat1->m, mat1->n);
  for (i = 0; i < matrix->m * matrix->n; i++)
    matrix->base[i] = mat1->base[i] + mat2->base[i];
  return matrix;
}

/* add mat2 to mat1 without allocating new memory (save memory) */
int32_t matrix_add_sm(MAT *mat1, MAT *mat2) {
  int i;
  if (mat1->m != mat2->m || mat1->n != mat2->n)
    return 0;
  for (i = 0; i < mat1->n * mat1->m; i++)
    mat1->base[i] += mat2->base[i];
  return 1;
}

MAT *matrix_sub(MAT *mat1, MAT *mat2) {
  int i;
  MAT *matrix = NULL;

  if (!mat1 || !mat2)
    SDDS_Bomb("Memory is not allocated for the substraction matrices!");
  if (mat1->m != mat2->m || mat1->n != mat2->n)
    SDDS_Bomb("The rows and columns of input matrices do not match(matrix_suB)!");
  matrix = matrix_get(mat1->m, mat1->n);

  for (i = 0; i < mat1->n * mat1->m; i++)
    matrix->base[i] = mat1->base[i] - mat2->base[i];
  return matrix;
}

/* add mat2 to mat1 without allocating new memory (save memory) */
int32_t matrix_sub_sm(MAT *mat1, MAT *mat2) {
  int i;
  if (mat1->m != mat2->m || mat1->n != mat2->n)
    return 0;
  for (i = 0; i < mat1->n * mat1->m; i++)
    mat1->base[i] -= mat2->base[i];
  return 1;
}

MAT *matrix_h_mult(MAT *mat1, MAT *mat2) {
  int i;
  MAT *new_mat = NULL;

  if (!mat1 || !mat2)
    SDDS_Bomb("Memory is not allocated for the hadamard multiplication matrices(matrix_h_mult)!");
  if (mat1->m != mat2->m || mat1->n != mat2->n)
    SDDS_Bomb("The rows and columns of input matrices do not match(matrix_h_mult)!");
  new_mat = matrix_get(mat1->m, mat1->n);
  for (i = 0; i < mat1->n * mat1->m; i++)
    new_mat->base[i] = mat1->base[i] * mat2->base[i];
  return new_mat;
}

/* self multiply (to save memory), multiply the mat2 to mat1 without allocating new matrix */
int32_t matrix_h_mult_sm(MAT *mat1, MAT *mat2) {
  int i;
  if (!mat1 || !mat2)
    return 0;
  if (mat1->m != mat2->m || mat1->n != mat2->n)
    return 0;
  for (i = 0; i < mat1->n * mat1->m; i++)
    mat1->base[i] *= mat2->base[i];
  return 1;
}

MAT *matrix_h_divide(MAT *mat1, MAT *mat2) {
  int i;
  MAT *new_mat = NULL;

  if (!mat1 || !mat2)
    SDDS_Bomb("Memory is not allocated for the hadamard division matrices(matrix_h_divide)!");
  if (mat1->m != mat2->m || mat1->n != mat2->n)
    SDDS_Bomb("The rows and columns of input matrices do not match(matrix_h_divide)!");
  new_mat = matrix_get(mat1->m, mat1->n);

  for (i = 0; i < mat1->n * mat1->m; i++)
    new_mat->base[i] = mat1->base[i] / mat2->base[i];
  return new_mat;
}

/* self multiply (to save memory), multiply the mat2 to mat1 without allocating new matrix */
int32_t matrix_h_divide_sm(MAT *mat1, MAT *mat2) {
  int i;
  if (!mat1 || !mat2)
    return 0;
  if (mat1->m != mat2->m || mat1->n != mat2->n)
    return 0;
  for (i = 0; i < mat1->n * mat1->m; i++)
    mat1->base[i] /= mat2->base[i];
  return 1;
}

/*m is the row number, n is the column number */
/*base is allocated by column order */
MAT *op_matrix_mult(MAT *A, MAT *B) {
  register long i, j, k, m, n, p;
  Real *a_j;
  MAT *matrix = NULL;

  if ((p = A->n) != B->m)
    SDDS_Bomb("The columns of A and rows of B do not match, can not do multiplication of A X B (op_matrix_mult)!");
  m = A->m;
  n = B->n;

  matrix = matrix_get(m, n);
  /*A->base is in column order, change it to row order */
  p = A->n;
  a_j = (Real *)malloc(sizeof(Real) * m * p);
  for (i = 0; i < m; i++)
    for (j = 0; j < p; j++)
      a_j[i * p + j] = A->base[j * m + i];

  for (j = 0; j < n; j++)
    for (i = 0; i < m; i++) {
      matrix->me[j][i] = 0;
      for (k = 0; k < p; k++)
        matrix->me[j][i] += a_j[i * p + k] * B->base[k + j * p];
    }
  free(a_j);
  return matrix;
}

MAT *matrix_mult(MAT *mat1, MAT *mat2) {

  MAT *new_mat = NULL;
#if defined(CLAPACK) || defined(MKL)
  long lda, kk, ldb;
  double alpha = 1.0, beta = 0.0;
#endif
#if defined(LAPACK)
  integer lda, kk, ldb;
  doublereal alpha = 1.0, beta = 0.0;
#endif
  if (!mat1 || !mat2)
    SDDS_Bomb("Memory is not allocated (matrix_mult)!");
  if (mat1->n != mat2->m)
    SDDS_Bomb("The columns of A and rows of B do not match, can not do A X B operation(matrix_mult)!");

#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
  kk = MAX(1, mat1->n);
  lda = MAX(1, mat1->m);
  ldb = MAX(1, mat2->m);
  new_mat = matrix_get(mat1->m, mat2->n);
#    if defined(MKL)
  dgemm_("N", "N",
         (MKL_INT *)&new_mat->m, (MKL_INT *)&new_mat->n, (MKL_INT *)&kk, &alpha, mat1->base,
         (MKL_INT *)&lda, mat2->base, (MKL_INT *)&ldb, &beta, new_mat->base, (MKL_INT *)&new_mat->m);
#    else
#      if defined(__APPLE__)
  dgemm_("N", "N",
         (__LAPACK_int *)&new_mat->m, (__LAPACK_int *)&new_mat->n, (int *)&kk, &alpha, mat1->base,
         (int *)&lda, mat2->base, (int *)&ldb, &beta, new_mat->base, (__LAPACK_int *)&new_mat->m);
#      else
  dgemm_("N", "N",
    (lapack_int *)&new_mat->m, (lapack_int *)&new_mat->n, (lapack_int *)&kk, &alpha, mat1->base,
         (lapack_int *)&lda, mat2->base, (lapack_int *)&ldb, &beta, new_mat->base, (lapack_int *)&new_mat->m);
#      endif
#  endif
#else
  new_mat = op_matrix_mult(mat1, mat2);
#endif
  return new_mat;
}

MAT *matrix_invert(MAT *A, int32_t largestSValue, int32_t smallestSValue, double minRatio,
                   int32_t deleteVectors, int32_t *deleteVector, char **deletedVector,
                   VEC **S_Vec, int32_t *sValues,
                   VEC **S_Vec_used, int32_t *usedSValues,
                   MAT **U_matrix, MAT **Vt_matrix,
                   double *conditionNum) {
  MAT *Inv = NULL;
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
  MAT *U = NULL, *V = NULL, *Vt = NULL, *Invt = NULL;
  int32_t i, NSVUsed = 0, m, n, firstdelete = 1;
  VEC *SValue = NULL, *SValueUsed = NULL, *InvSValue = NULL;
  double max, min;
  char deletedVectors[1024];
  int info;
  /*use economy svd method i.e. U matrix is rectangular not square matrix */
  char calcMode = 'S';
#endif
#if defined(CLAPACK) || defined(MKL)
  double *work;
  long lwork;
  long lda;
  double alpha = 1.0, beta = 0.0;
  int kk, ldb;
#endif
#if defined(LAPACK)
  doublereal *work;
  integer lwork;
  integer lda;
  double alpha = 1.0, beta = 0.0;
  int kk, ldb;
#endif

#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
  if (!A || A->m <= 0 || A->n <= 0)
    SDDS_Bomb("Invalid matrix provided for invert (matrix_invert)!");
  n = A->n;
  m = A->m;
  deletedVectors[0] = '\0';
  if (S_Vec)
    SValue = *S_Vec;
  if (S_Vec_used)
    SValueUsed = *S_Vec_used;
  if (U_matrix)
    U = *U_matrix;
  if (Vt_matrix)
    V = *Vt_matrix;
  if (!SValue)
    SValue = vec_get(A->n);
  if (!SValueUsed)
    SValueUsed = vec_get(A->n);
  if (!InvSValue)
    InvSValue = vec_get(A->n);
  if (!Vt)
    Vt = matrix_get(A->n, A->n);
  if (!U)
    U = matrix_get(A->m, MIN(A->m, A->n));

#  if defined(MKL)
  work = (double *)malloc(sizeof(double) * 1);
  lwork = -1;
  lda = MAX(1, A->m);
  dgesvd_((char *)&calcMode, (char *)&calcMode, (MKL_INT *)&A->m, (MKL_INT *)&A->n,
          (double *)A->base, (MKL_INT *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (MKL_INT *)&A->m,
          (double *)Vt->base, (MKL_INT *)&A->n,
          (double *)work, (MKL_INT *)&lwork,
          (MKL_INT *)&info);

  lwork = work[0];
  work = (double *)realloc(work, sizeof(double) * lwork);

  dgesvd_((char *)&calcMode, (char *)&calcMode, (MKL_INT *)&A->m, (MKL_INT *)&A->n,
          (double *)A->base, (MKL_INT *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (MKL_INT *)&A->m,
          (double *)Vt->base, (MKL_INT *)&A->n,
          (double *)work, (MKL_INT *)&lwork,
          (MKL_INT *)&info);
  free(work);
#  endif
#  if defined(CLAPACK)
 #if defined(ACCELERATE_NEW_LAPACK)
  work = (double *)malloc(sizeof(double) * 1);
  lwork = -1;
  lda = MAX(1, A->m);
  dgesvd_((char *)&calcMode, (char *)&calcMode, (__LAPACK_int *)&A->m, (__LAPACK_int *)&A->n,
          (double *)A->base, (__LAPACK_int *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (__LAPACK_int *)&A->m,
          (double *)Vt->base, (__LAPACK_int *)&A->n,
          (double *)work, (__LAPACK_int *)&lwork,
          (__LAPACK_int *)&info);

  lwork = work[0];
  work = (double *)realloc(work, sizeof(double) * lwork);

  dgesvd_((char *)&calcMode, (char *)&calcMode, (__LAPACK_int *)&A->m, (__LAPACK_int *)&A->n,
          (double *)A->base, (__LAPACK_int *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (__LAPACK_int *)&A->m,
          (double *)Vt->base, (__LAPACK_int *)&A->n,
          (double *)work, (__LAPACK_int *)&lwork,
          (__LAPACK_int *)&info);
  free(work);
 #else
  work = (double *)malloc(sizeof(double) * 1);
  lwork = -1;
  lda = MAX(1, A->m);
  dgesvd_((char *)&calcMode, (char *)&calcMode, (long *)&A->m, (long *)&A->n,
          (double *)A->base, (long *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (long *)&A->m,
          (double *)Vt->base, (long *)&A->n,
          (double *)work, (long *)&lwork,
          (long *)&info);

  lwork = work[0];
  work = (double *)realloc(work, sizeof(double) * lwork);

  dgesvd_((char *)&calcMode, (char *)&calcMode, (long *)&A->m, (long *)&A->n,
          (double *)A->base, (long *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (long *)&A->m,
          (double *)Vt->base, (long *)&A->n,
          (double *)work, (long *)&lwork,
          (long *)&info);
  free(work);
 #endif
#  endif
#  if defined(LAPACK)
  work = (doublereal *)malloc(sizeof(doublereal) * 1);
  lwork = -1;
  lda = MAX(1, A->m);

  LAPACK_dgesvd((char *)&calcMode, (char *)&calcMode, (lapack_int *)&A->m, (lapack_int *)&A->n,
                (doublereal *)A->base, (lapack_int *)&lda,
                (doublereal *)SValue->ve,
                (doublereal *)U->base, (lapack_int *)&A->m,
                (doublereal *)Vt->base, (lapack_int *)&A->n,
                (doublereal *)work, (lapack_int *)&lwork,
                (lapack_int *)&info);

  lwork = work[0];
  work = (doublereal *)realloc(work, sizeof(doublereal) * lwork);

  LAPACK_dgesvd((char *)&calcMode, (char *)&calcMode, (lapack_int *)&A->m, (lapack_int *)&A->n,
                (doublereal *)A->base, (lapack_int *)&lda,
                (doublereal *)SValue->ve,
                (doublereal *)U->base, (lapack_int *)&A->m,
                (doublereal *)Vt->base, (lapack_int *)&A->n,
                (doublereal *)work, (lapack_int *)&lwork,
                (lapack_int *)&info);
  free(work);
#  endif
  max = 0;
  min = HUGE;
  InvSValue->ve[0] = 1 / SValue->ve[0];
  SValueUsed->ve[0] = SValue->ve[0];
  max = MAX(SValueUsed->ve[0], max);
  min = MIN(SValueUsed->ve[0], min);
  NSVUsed = 1;
  /*
    1) first remove SVs that are exactly zero
    2) remove SV according to ratio option
    3) remove SV according to largests option
    4) remove SV of user-selected vectors
  */

  for (i = 1; i < n; i++) {
    if (!SValue->ve[i]) {
      InvSValue->ve[i] = 0;
    } else if ((SValue->ve[i] / SValue->ve[0]) < minRatio) {
      InvSValue->ve[i] = 0;
      SValueUsed->ve[i] = 0;
    } else if (largestSValue && i >= largestSValue) {
      InvSValue->ve[i] = 0;
      SValueUsed->ve[i] = 0;
    } else if (smallestSValue && i >= (n - smallestSValue)) {
      InvSValue->ve[i] = 0;
      SValueUsed->ve[i] = 0;
    } else {
      InvSValue->ve[i] = 1 / SValue->ve[i];
      SValueUsed->ve[i] = SValue->ve[i];
      max = MAX(SValueUsed->ve[i], max);
      min = MIN(SValueUsed->ve[i], min);
      NSVUsed++;
    }
  }
  /*4) remove SV of user-selected vectors -
    delete vector given in the -deleteVectors option
    by setting the inverse singular values to 0*/
  for (i = 0; i < deleteVectors; i++) {
    if (0 <= deleteVector[i] && deleteVector[i] < n) {
      if (firstdelete)
        sprintf(deletedVectors, "%d", deleteVector[i]);
      else
        sprintf(deletedVectors + strlen(deletedVectors), " %d", deleteVector[i]);
      firstdelete = 0;
      InvSValue->ve[deleteVector[i]] = 0;
      SValueUsed->ve[deleteVector[i]] = 0;
      if (largestSValue && deleteVector[i] >= largestSValue)
        break;
      NSVUsed--;
    }
  }
  if (deletedVector)
    SDDS_CopyString(deletedVector, deletedVectors);
  if (conditionNum)
    *conditionNum = max / min;
  /* R = U S Vt and Inv = V SInv Ut, Inv is nxm matrix*/
  /* then Invt = U Sinv Vt , Invt is mxn matrix*/
  Invt = matrix_get(m, n);
  V = matrix_get(Vt->m, Vt->n);
  /* U ant Vt matrix (in column order) are available */
  for (i = 0; i < Vt->n; i++) {
    for (kk = 0; kk < n; kk++)
      V->base[i * V->m + kk] = Vt->base[i * V->m + kk] * InvSValue->ve[kk];
  }
  vec_free(InvSValue);
  /* Rinvt = U Sinv Vt = U V */
  /* note that, the arguments of dgemm should be
     U->m -- should be the row number of product matrix, which is U x V
     V->n -- the column number of the product matrix
     kk   -- should be the column number of U and row number of V, whichever is smaller (otherwise, memory access error)
     lda  -- should be the row number of U matrix
     ldb  -- should be the row number of V matrix */
  kk = MIN(U->n, V->m);
  lda = MAX(1, U->m);
  ldb = MAX(1, V->m);
#    if defined(MKL)
  dgemm_("N", "N",
         (MKL_INT *)&U->m, (MKL_INT *)&V->n, (MKL_INT *)&kk, &alpha, U->base,
         (MKL_INT *)&lda, V->base, (MKL_INT *)&ldb, &beta, Invt->base, (MKL_INT *)&U->m);
#    else
#      if defined(__APPLE__)
  dgemm_("N", "N",
         (__LAPACK_int *)&U->m, (__LAPACK_int *)&V->n, &kk, &alpha, U->base,
         (int *)&lda, V->base, &ldb, &beta, Invt->base, (__LAPACK_int *)&U->m);
#      else
  dgemm_("N", "N",
    (lapack_int *)&U->m, (lapack_int *)&V->n, &kk, &alpha, U->base,
         (lapack_int *)&lda, V->base, (lapack_int *)&ldb, &beta, Invt->base, (lapack_int *)&U->m);
#      endif
#    endif
  matrix_free(V);
  if (Vt_matrix) {
    if (*Vt_matrix)
      matrix_free(*Vt_matrix);
    *Vt_matrix = Vt;
  } else
    matrix_free(Vt);
  if (U_matrix) {
    if (*U_matrix)
      matrix_free(*U_matrix);
    *U_matrix = U;
  } else
    matrix_free(U);
  if (S_Vec) {
    if (*S_Vec)
      vec_free(*S_Vec);
    *S_Vec = SValue;
  } else
    vec_free(SValue);
  if (sValues)
    *sValues = SValue->dim;
  if (S_Vec_used) {
    if (*S_Vec_used)
      vec_free(*S_Vec_used);
    *S_Vec_used = SValueUsed;
    *usedSValues = NSVUsed;
  } else
    vec_free(SValueUsed);
  /*transpose Invt to Inv */
  Inv = matrix_transpose(Invt);
  matrix_free(Invt);
#else
  SDDS_Bomb("Matrix inversion is unable for non LAPACK/CLAPACK/MKL library.");
#endif
  return Inv;
}

MAT *matrix_identity(int32_t m, int32_t n) {
  register long i, j;
  register double *a_j;
  MAT *matrix = NULL;
  matrix = matrix_get(m, n);
  for (j = 0; j < n; j++) {
    a_j = matrix->me[j];
    for (i = 0; i < m; i++)
      a_j[i] = (i == j ? 1 : 0);
  }
  return matrix;
}

/* the data is one dimension array, ordered by column */
void *SDDS_GetCastMatrixOfRowsByColumn(SDDS_DATASET *SDDS_dataset, int32_t *n_rows, long sddsType) {
  void *data;
  long i, j, k, size;
  /*  long type;*/
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetCastMatrixOfRowsByColumn"))
    return (NULL);
  if (!SDDS_NUMERIC_TYPE(sddsType)) {
    SDDS_SetError("Unable to get matrix of rows--no columns selected (SDDS_GetCastMatrixOfRowsByColumn) (1)");
    return NULL;
  }
  if (SDDS_dataset->n_of_interest <= 0) {
    SDDS_SetError("Unable to get matrix of rows--no columns selected (SDDS_GetCastMatrixOfRowsByColumn) (2)");
    return (NULL);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_GetCastMatrixOfRowsByColumn"))
    return (NULL);
  size = SDDS_type_size[sddsType - 1];
  if ((*n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get matrix of rows--no rows of interest (SDDS_GetCastMatrixOfRowsByColumn) (3)");
    return (NULL);
  }
  for (i = 0; i < SDDS_dataset->n_of_interest; i++) {
    if (!SDDS_NUMERIC_TYPE(SDDS_dataset->layout.column_definition[SDDS_dataset->column_order[i]].type)) {
      SDDS_SetError("Unable to get matrix of rows--not all columns are numeric (SDDS_GetCastMatrixOfRowsByColumn) (4)");
      return NULL;
    }
  }
  if (!(data = (void *)SDDS_Malloc(size * (*n_rows) * SDDS_dataset->n_of_interest))) {
    SDDS_SetError("Unable to get matrix of rows--memory allocation failure (SDDS_GetCastMatrixOfRowsByColumn) (5)");
    return (NULL);
  }
  for (j = k = 0; j < SDDS_dataset->n_rows; j++) {
    if (SDDS_dataset->row_flag[j]) {
      for (i = 0; i < SDDS_dataset->n_of_interest; i++)
        SDDS_CastValue(SDDS_dataset->data[SDDS_dataset->column_order[i]], j,
                       SDDS_dataset->layout.column_definition[SDDS_dataset->column_order[i]].type,
                       sddsType, (char *)data + (k + i * SDDS_dataset->n_rows) * sizeof(Real));
      k++;
    }
  }
  return (data);
}

double matrix_det(MAT *A) {
  double det = 1.0;
  MAT *B;
#if defined(CLAPACK) || defined(MKL)
  long i, lda, n, m, *ipvt, info;
#endif
#if defined(LAPACK)
  lapack_int i, lda, n, m, *ipvt, info;
#endif
#if !defined(LAPACK) && !defined(CLAPACK) && !defined(MKL)
  MATRIX *DET;
  long i, j;
#endif

  if (A->m != A->n)
    return 0;
#if defined(LAPACK) || defined(CLAPACK) || defined(MKL)
  lda = A->m;
  n = A->n;
  m = A->m;
  ipvt = calloc(n, sizeof(*ipvt));
  B = matrix_copy(A);
  /*LU decomposition*/
#  if defined(MKL)
  dgetrf_((MKL_INT *)&m, (MKL_INT *)&n, B->base, (MKL_INT *)&lda, (MKL_INT *)ipvt, (MKL_INT *)&info);
#  else
#    if defined(LAPACK)
  LAPACK_dgetrf((lapack_int *)&m, (lapack_int *)&n, B->base, (lapack_int *)&lda, ipvt, (lapack_int *)&info);
#    else
#if defined(ACCELERATE_NEW_LAPACK)
  dgetrf_((__LAPACK_int *)&m, (__LAPACK_int *)&n, B->base, (__LAPACK_int *)&lda, (__LAPACK_int *)ipvt, (__LAPACK_int *)&info);
#else
  dgetrf_(&m, &n, B->base, &lda, ipvt, &info);
#endif
#    endif
#  endif
  if (info < 0) {
    fprintf(stderr, "Error in LU decomposition, the %d-th argument had an illegal value.\n", -info);
    return 0;
  } else if (info > 0) {
    fprintf(stderr, "Error in LU decomposition, U(%d) is exactly zero. The factorization has been completed, but the factor U is exactly singular, and division by zero will occur if it is used to solve a system of equations.\n", info);
    return 0;
  }
  for (i = 0; i < n; i++)
    det *= B->me[i][i];
  matrix_free(B);
#else
  /* m is A's rows, n is A's columns A is in column major order; MATRIX B should be in row major order */
  m_alloc(&DET, A->m, A->n);
  for (i = 0; i < A->m; i++)
    for (j = 0; j < A->n; j++) {
      DET->a[i][j] = A->me[j][i];
    }
  det = m_det(DET);
  m_free(&DET);
#endif
  return det;
}

MAT *matrix_invert_weight(MAT *A, double *weight, int32_t largestSValue, int32_t smallestSValue, double minRatio,
                          int32_t deleteVectors, int32_t *deleteVector, char **deletedVector,
                          VEC **S_Vec, int32_t *sValues,
                          VEC **S_Vec_used, int32_t *usedSValues,
                          MAT **U_matrix, MAT **Vt_matrix,
                          double *conditionNum) {
  MAT *Inv = NULL;
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
  MAT *U = NULL, *V = NULL, *Vt = NULL, *Invt = NULL;
  int32_t i, j, NSVUsed = 0, m, n, firstdelete = 1;
  VEC *SValue = NULL, *SValueUsed = NULL, *InvSValue = NULL;
  double max, min;
  char deletedVectors[1024];
  int info;
  /*use economy svd method i.e. U matrix is rectangular not square matrix */
  char calcMode = 'S';
#endif
#if defined(CLAPACK) || defined(MKL)
  double *work;
  long lwork;
  long lda;
  double alpha = 1.0, beta = 0.0;
  int kk, ldb;
#endif
#if defined(LAPACK)
  doublereal *work;
  integer lwork;
  integer lda;
  double alpha = 1.0, beta = 0.0;
  int kk, ldb;
#endif

#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
  if (!A || A->m <= 0 || A->n <= 0)
    SDDS_Bomb("Invalid matrix provided for invert (matrix_invert)!");
  n = A->n;
  m = A->m;
  deletedVectors[0] = '\0';
  if (S_Vec)
    SValue = *S_Vec;
  if (S_Vec_used)
    SValueUsed = *S_Vec_used;
  if (U_matrix)
    U = *U_matrix;
  if (Vt_matrix)
    V = *Vt_matrix;
  if (!SValue)
    SValue = vec_get(A->n);
  if (!SValueUsed)
    SValueUsed = vec_get(A->n);
  if (!InvSValue)
    InvSValue = vec_get(A->n);
  if (!Vt)
    Vt = matrix_get(A->n, A->n);
  if (!U)
    U = matrix_get(A->m, MIN(A->m, A->n));
  if (weight) {
    for (i = 0; i < A->m; i++)
      for (j = 0; j < A->n; j++)
        Mij(A, i, j) *= weight[i];
  }
#  if defined(MKL)
  work = (double *)malloc(sizeof(double) * 1);
  lwork = -1;
  lda = MAX(1, A->m);
  dgesvd_((char *)&calcMode, (char *)&calcMode, (MKL_INT *)&A->m, (MKL_INT *)&A->n,
          (double *)A->base, (MKL_INT *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (MKL_INT *)&A->m,
          (double *)Vt->base, (MKL_INT *)&A->n,
          (double *)work, (MKL_INT *)&lwork,
          (MKL_INT *)&info);

  lwork = work[0];
  work = (double *)realloc(work, sizeof(double) * lwork);

  dgesvd_((char *)&calcMode, (char *)&calcMode, (MKL_INT *)&A->m, (MKL_INT *)&A->n,
          (double *)A->base, (MKL_INT *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (MKL_INT *)&A->m,
          (double *)Vt->base, (MKL_INT *)&A->n,
          (double *)work, (MKL_INT *)&lwork,
          (MKL_INT *)&info);
  free(work);
#  endif
#  if defined(CLAPACK)
#if defined(ACCELERATE_NEW_LAPACK)
  work = (double *)malloc(sizeof(double) * 1);
  lwork = -1;
  lda = MAX(1, A->m);
  dgesvd_((char *)&calcMode, (char *)&calcMode, (__LAPACK_int *)&A->m, (__LAPACK_int *)&A->n,
          (double *)A->base, (__LAPACK_int *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (__LAPACK_int *)&A->m,
          (double *)Vt->base, (__LAPACK_int *)&A->n,
          (double *)work, (__LAPACK_int *)&lwork,
          (__LAPACK_int *)&info);

  lwork = work[0];
  work = (double *)realloc(work, sizeof(double) * lwork);

  dgesvd_((char *)&calcMode, (char *)&calcMode, (__LAPACK_int *)&A->m, (__LAPACK_int *)&A->n,
          (double *)A->base, (__LAPACK_int *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (__LAPACK_int *)&A->m,
          (double *)Vt->base, (__LAPACK_int *)&A->n,
          (double *)work, (__LAPACK_int *)&lwork,
          (__LAPACK_int *)&info);
  free(work);
#else
  work = (double *)malloc(sizeof(double) * 1);
  lwork = -1;
  lda = MAX(1, A->m);
  dgesvd_((char *)&calcMode, (char *)&calcMode, (long *)&A->m, (long *)&A->n,
          (double *)A->base, (long *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (long *)&A->m,
          (double *)Vt->base, (long *)&A->n,
          (double *)work, (long *)&lwork,
          (long *)&info);

  lwork = work[0];
  work = (double *)realloc(work, sizeof(double) * lwork);

  dgesvd_((char *)&calcMode, (char *)&calcMode, (long *)&A->m, (long *)&A->n,
          (double *)A->base, (long *)&lda,
          (double *)SValue->ve,
          (double *)U->base, (long *)&A->m,
          (double *)Vt->base, (long *)&A->n,
          (double *)work, (long *)&lwork,
          (long *)&info);
  free(work);
#endif
#  endif
#  if defined(LAPACK)
  work = (doublereal *)malloc(sizeof(doublereal) * 1);
  lwork = -1;
  lda = MAX(1, A->m);

  LAPACK_dgesvd((char *)&calcMode, (char *)&calcMode, (lapack_int *)&A->m, (lapack_int *)&A->n,
                (doublereal *)A->base, (lapack_int *)&lda,
                (doublereal *)SValue->ve,
                (doublereal *)U->base, (lapack_int *)&A->m,
                (doublereal *)Vt->base, (lapack_int *)&A->n,
                (doublereal *)work, (lapack_int *)&lwork,
                (lapack_int *)&info);

  lwork = work[0];
  work = (doublereal *)realloc(work, sizeof(doublereal) * lwork);

  LAPACK_dgesvd((char *)&calcMode, (char *)&calcMode, (lapack_int *)&A->m, (lapack_int *)&A->n,
                (doublereal *)A->base, (lapack_int *)&lda,
                (doublereal *)SValue->ve,
                (doublereal *)U->base, (lapack_int *)&A->m,
                (doublereal *)Vt->base, (lapack_int *)&A->n,
                (doublereal *)work, (lapack_int *)&lwork,
                (lapack_int *)&info);
  free(work);
#  endif
  max = 0;
  min = HUGE;
  InvSValue->ve[0] = 1 / SValue->ve[0];
  SValueUsed->ve[0] = SValue->ve[0];
  max = MAX(SValueUsed->ve[0], max);
  min = MIN(SValueUsed->ve[0], min);
  NSVUsed = 1;
  /*
    1) first remove SVs that are exactly zero
    2) remove SV according to ratio option
    3) remove SV according to largests option
    4) remove SV of user-selected vectors
  */

  for (i = 1; i < n; i++) {
    if (!SValue->ve[i]) {
      InvSValue->ve[i] = 0;
    } else if ((SValue->ve[i] / SValue->ve[0]) < minRatio) {
      InvSValue->ve[i] = 0;
      SValueUsed->ve[i] = 0;
    } else if (largestSValue && i >= largestSValue) {
      InvSValue->ve[i] = 0;
      SValueUsed->ve[i] = 0;
    } else if (smallestSValue && i >= (n - smallestSValue)) {
      InvSValue->ve[i] = 0;
      SValueUsed->ve[i] = 0;
    } else {
      InvSValue->ve[i] = 1 / SValue->ve[i];
      SValueUsed->ve[i] = SValue->ve[i];
      max = MAX(SValueUsed->ve[i], max);
      min = MIN(SValueUsed->ve[i], min);
      NSVUsed++;
    }
  }
  /*4) remove SV of user-selected vectors -
    delete vector given in the -deleteVectors option
    by setting the inverse singular values to 0*/
  for (i = 0; i < deleteVectors; i++) {
    if (0 <= deleteVector[i] && deleteVector[i] < n) {
      if (firstdelete)
        sprintf(deletedVectors, "%d", deleteVector[i]);
      else
        sprintf(deletedVectors + strlen(deletedVectors), " %d", deleteVector[i]);
      firstdelete = 0;
      InvSValue->ve[deleteVector[i]] = 0;
      SValueUsed->ve[deleteVector[i]] = 0;
      if (largestSValue && deleteVector[i] >= largestSValue)
        break;
      NSVUsed--;
    }
  }
  if (deletedVector)
    SDDS_CopyString(deletedVector, deletedVectors);
  if (conditionNum)
    *conditionNum = max / min;
  /* R = U S Vt and Inv = V SInv Ut, Inv is nxm matrix*/
  /* then Invt = U Sinv Vt , Invt is mxn matrix*/
  Invt = matrix_get(m, n);
  V = matrix_get(Vt->m, Vt->n);
  /* U ant Vt matrix (in column order) are available */
  for (i = 0; i < Vt->n; i++) {
    for (kk = 0; kk < n; kk++)
      V->base[i * V->m + kk] = Vt->base[i * V->m + kk] * InvSValue->ve[kk];
  }
  vec_free(InvSValue);
  /* Rinvt = U Sinv Vt = U V */
  /* note that, the arguments of dgemm should be
     U->m -- should be the row number of product matrix, which is U x V
     V->n -- the column number of the product matrix
     kk   -- should be the column number of U and row number of V, whichever is smaller (otherwise, memory access error)
     lda  -- should be the row number of U matrix
     ldb  -- should be the row number of V matrix */
  kk = MIN(U->n, V->m);
  lda = MAX(1, U->m);
  ldb = MAX(1, V->m);
#    if defined(MKL)
  dgemm_("N", "N",
         (MKL_INT *)&U->m, (MKL_INT *)&V->n, (MKL_INT *)&kk, &alpha, U->base,
         (MKL_INT *)&lda, V->base, (MKL_INT *)&ldb, &beta, Invt->base, (MKL_INT *)&U->m);
#    else
#      if defined(__APPLE__)
  dgemm_("N", "N",
         (__LAPACK_int *)&U->m, (__LAPACK_int *)&V->n, &kk, &alpha, U->base,
         (int *)&lda, V->base, &ldb, &beta, Invt->base, (__LAPACK_int *)&U->m);
#      else
  dgemm_("N", "N",
    (lapack_int *)&U->m, (lapack_int *)&V->n, &kk, &alpha, U->base,
         (lapack_int *)&lda, V->base, &ldb, &beta, Invt->base, (lapack_int *)&U->m);
#      endif
#    endif
  matrix_free(V);
  if (Vt_matrix) {
    if (*Vt_matrix)
      matrix_free(*Vt_matrix);
    *Vt_matrix = Vt;
  } else
    matrix_free(Vt);
  if (U_matrix) {
    if (*U_matrix)
      matrix_free(*U_matrix);
    *U_matrix = U;
  } else
    matrix_free(U);
  if (S_Vec) {
    if (*S_Vec)
      vec_free(*S_Vec);
    *S_Vec = SValue;
  } else
    vec_free(SValue);
  if (sValues)
    *sValues = SValue->dim;
  if (S_Vec_used) {
    if (*S_Vec_used)
      vec_free(*S_Vec_used);
    *S_Vec_used = SValueUsed;
    *usedSValues = NSVUsed;
  } else
    vec_free(SValueUsed);
  /*transpose Invt to Inv */
  Inv = matrix_transpose(Invt);
  matrix_free(Invt);
  if (weight) {
    /* multiply ith column of inverse by weight[i] */
    for (i = 0; i < Inv->n; i++)
      for (j = 0; j < Inv->m; j++)
        Mij(Inv, j, i) *= weight[i];
  }
#else
  SDDS_Bomb("Matrix inversion is unable for non LAPACK/CLAPACK/MKL library.");
#endif
  return Inv;
}

/*print column major ordered matrix */
void matrix_output(FILE *fp, MAT *mat) {
  unsigned int i, j;

  fprintf(fp, "Matrix: %d by %d\n", mat->m, mat->n);
  for (i = 0; i < mat->n; i++) {
    fprintf(fp, "column %u: ", i);
    for (j = 0; j < mat->m; j++)
      fprintf(fp, "%14.9g ", mat->me[i][j]);
    fprintf(fp, "\n");
  }
}
