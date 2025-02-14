/* redefine Matrix so that meschach lib is no longer needed */
#include "SDDS.h"

#ifndef MATRIX_H
#define MATRIX_H

/* if nothing is defined, choose double precision */
#ifndef REAL_DBL
#ifndef REAL_FLT
#define REAL_DBL 1
#endif
#endif

/* single precision */
#ifdef REAL_FLT
#define  Real float
#define  LongReal float
#define REAL FLOAT
#define LONGREAL FLOAT
#endif

/* double precision */
#ifdef REAL_DBL
#define Real double
#define LongReal double
#define REAL DOUBLE
#define LONGREAL DOUBLE
#endif

#include "matrix.h"


/* allocate one bject of given type */
#define	NEW(type)	((type *)calloc((size_t)1,(size_t)sizeof(type)))

/* allocate num objects of given type */
#define	NEW_A(num,type)	((type *)calloc((size_t)(num),(size_t)sizeof(type)))

/* get  vector/matrix to given dimension */
extern	VEC *vec_get(int);
extern	MAT *matrix_get(int,int); 
extern	int matrix_free(MAT *),vec_free(VEC *);
extern  MAT *matrix_copy(MAT *);
/* A is the input*/
extern  MAT *matrix_transpose(MAT *A); 

/*in following routines, mat1 and mat2 are input, no change */
extern  MAT *matrix_add(MAT *mat1, MAT *mat2);
extern  MAT *matrix_sub(MAT *mat1, MAT *mat2);
extern  MAT *matrix_h_mult(MAT *mat1, MAT *mat2);
extern  MAT *matrix_h_divide(MAT *mat1, MAT *mat2);
extern  MAT *matrix_mult(MAT *mat1, MAT *mat2);
extern void matrix_output(FILE *fp, MAT *m);

/*in following routines, mat1 is input and output, changed; mat2 is input, no change*/
extern  int32_t matrix_add_sm(MAT *mat1, MAT *mat2); 
extern  int32_t matrix_sub_sm(MAT *mat1, MAT *mat2);
extern  int32_t matrix_h_mult_sm(MAT *mat1, MAT *mat2);
extern  int32_t matrix_h_divide_sm(MAT *mat1, MAT *mat2);

/*invert routine*/
extern  MAT *matrix_invert(MAT *A, int32_t largestSValue, int32_t smallestSValue, double minRatio,
                   int32_t deleteVectors, int32_t *deleteVector, char **deletedVector,
                   VEC **S_Vec, int32_t *sValues,
                   VEC **S_Vec_used, int32_t *usedSValues,
                   MAT **U_matrix, MAT **Vt_matrix,
                   double *conditionNum);
extern  MAT *matrix_invert_weight(MAT *A, double *weight, int32_t largestSValue, int32_t smallestSValue, double minRatio,
                   int32_t deleteVectors, int32_t *deleteVector, char **deletedVector,
                   VEC **S_Vec, int32_t *sValues,
                   VEC **S_Vec_used, int32_t *usedSValues,
                   MAT **U_matrix, MAT **Vt_matrix,
                   double *conditionNum);

/*generate identity matrix*/
MAT *matrix_identity(int32_t m, int32_t n);

/*compute matrix determinant */
double matrix_det(MAT *D);

/*read column major ordered matrix array*/
void *SDDS_GetCastMatrixOfRowsByColumn(SDDS_DATASET *SDDS_dataset, int32_t *n_rows, long sddsType);

#define Mij(A, i, j) A->me[j][i]

#endif
