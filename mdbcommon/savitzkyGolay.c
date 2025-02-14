/**
 * @file savitzkyGolay.c
 * @brief Implementation of Savitzky-Golay smoothing functions.
 *
 * This file contains functions for performing Savitzky-Golay smoothing and differentiation on data arrays.
 * It provides tools to smooth noisy data or compute derivatives using polynomial fitting over a moving window.
 */

#include "matlib.h"
#include "mdb.h"
#include "fftpackC.h"

/**
 * @brief Applies Savitzky-Golay smoothing or differentiation to a data array.
 *
 * This function smooths the provided data array using the Savitzky-Golay method.
 * It can also compute derivatives of the data up to a specified order.
 * The smoothing is performed in-place on the input data array.
 *
 * @param data Pointer to the data array to be smoothed or differentiated.
 * @param rows Number of data points in the data array.
 * @param order Polynomial order of the smoothing filter.
 * @param nLeft Number of data points to include to the left of the central point in the smoothing window.
 * @param nRight Number of data points to include to the right of the central point in the smoothing window.
 * @param derivativeOrder Order of the derivative to compute (0 for smoothing only).
 * @return Returns 1 on success, 0 on failure due to invalid parameters or memory allocation error.
 *
 * @note The function modifies the input data array in place.
 * @warning The input data array must have at least (nLeft + nRight + 1) elements.
 * @warning This function is not thread-safe due to the use of static internal buffers.
 */
long SavitzkyGolaySmooth(double *data, long rows, long order, long nLeft, long nRight, long derivativeOrder) {
  static double *FFTdata = NULL, *FFTfilter = NULL, *TMPdata = NULL;
  static long arraySize = 0, TMParraySize = 0;
  long i, nfreq, sizeNeeded;

  if (order < 0) {
    fprintf(stderr, "order<0 (SavitzkyGolaySmooth)\n");
    return (0);
  }
  if (nLeft < 0) {
    fprintf(stderr, "nLeft<0 (SavitzkyGolaySmooth)\n");
    return (0);
  }
  if (nRight < 0) {
    fprintf(stderr, "nRight<0 (SavitzkyGolaySmooth)\n");
    return (0);
  }
  if (derivativeOrder < 0) {
    fprintf(stderr, "derivativeOrder<0 (SavitzkyGolaySmooth)\n");
    return (0);
  }
  if (derivativeOrder > order) {
    fprintf(stderr, "derivativeOrder>order (SavitzkyGolaySmooth)\n");
    return (0);
  }
  if ((nLeft + nRight) < order) {
    fprintf(stderr, "(nLeft+nRight)<order (SavitzkyGolaySmooth)\n");
    return (0);
  }
  if ((nLeft + nRight) == 0) {
    fprintf(stderr, "(nLeft+nRight)==0 (SavitzkyGolaySmooth)\n");
    return (0);
  }
  if (rows < (nLeft + nRight + 1)) {
    fprintf(stderr, "rows<(nLeft+nRight+1) (SavitzkyGolaySmooth)\n");
    return (0);
  }

  if ((order == 1) && (nLeft == nRight) && (derivativeOrder == 0)) {
    /* This is a special case of the filter. Sometimes called moving window averaging. */
    /* It requires that nLeft=nRight and this is the only option for elegant now */
    long np = nLeft + nRight + 1;
    double scale = 1.0 / np;

    sizeNeeded = rows;
    if (TMParraySize < sizeNeeded && (!(TMPdata = realloc(TMPdata, sizeof(*TMPdata) * (TMParraySize = sizeNeeded))))) {
      fprintf(stderr, "Error: memory allocation failure (SavitzkyGolaySmooth)\n");
      exit(1);
    }

    for (i = 0; i < rows; i++) {
      data[i] = scale * data[i];
      TMPdata[i] = data[i];
    }

    /* Smooth the left side data with padding to eliminate end effects */
    for (i = 1; i <= nRight; i++)
      data[0] += data[i];
    data[0] += nLeft * TMPdata[0];

    for (i = 1; i <= nLeft; i++) {
      data[i] = data[i - 1] + data[i + nRight] - TMPdata[0];
    }

    /* Smooth the middle part of the array */
    for (i = nLeft + 1; i < rows - nRight; i++) {
      data[i] = data[i - 1] + data[i + nRight] - TMPdata[i - nLeft - 1];
    }

    /* Smooth the right side data with padding to eliminate end effects */
    for (i = rows - nRight; i < rows; i++) {
      data[i] = data[i - 1] + data[rows - 1] - TMPdata[i - nLeft - 1];
    }

    return (1);
  } else { /* Smooth data in the time domain */
    long np = nLeft + nRight + 1, j;
    static long coeffArraySize = 0;
    static double *filterCoeff = NULL;

    sizeNeeded = rows + nLeft + nRight;
    if (TMParraySize < sizeNeeded && (!(TMPdata = realloc(TMPdata, sizeof(*TMPdata) * (TMParraySize = sizeNeeded))))) {
      fprintf(stderr, "Error: memory allocation failure (SavitzkyGolaySmooth)\n");
      exit(1);
    }
    if (coeffArraySize < np && (!(filterCoeff = realloc(filterCoeff, sizeof(*filterCoeff) * (coeffArraySize = np))))) {
      fprintf(stderr, "Error: memory allocation failure (SavitzkyGolaySmooth)\n");
      exit(1);
    }

    /* copy the data to a temporary array, with padding to eliminate end effects */
    for (i = 0; i < rows; i++)
      TMPdata[i + nLeft] = data[i];
    for (i = 0; i < nLeft; i++)
      TMPdata[i] = data[0];
    for (i = 0; i < nRight; i++)
      TMPdata[rows + nLeft + i] = data[rows - 1];

    /* store SG coefficients in wrap-around order  */
    SavitzkyGolayCoefficients(filterCoeff, np, order, nLeft, nRight, derivativeOrder, 1);

    for (i = 0; i < rows; i++) {
      data[i] = data[i] * filterCoeff[0];
      for (j = 1; j <= nLeft; j++)
        data[i] += TMPdata[i + nLeft - j] * filterCoeff[j];
      for (j = 1; j <= nRight; j++)
        data[i] += TMPdata[i + nLeft + j] * filterCoeff[np - j];
    }
    return (1);
  }

  /* Below are the old implementation in the frequency domain, which could be very time consuming */
  sizeNeeded = 2 * (rows + 1 + nLeft + nRight);
  if (arraySize < sizeNeeded && (!(FFTdata = realloc(FFTdata, sizeof(*FFTdata) * (arraySize = sizeNeeded))) || !(FFTfilter = realloc(FFTfilter, sizeof(*FFTfilter) * (arraySize = sizeNeeded))))) {
    fprintf(stderr, "Error: memory allocation failure (SavitzkyGolaySmooth)\n");
    exit(1);
  }
  for (i = 0; i < arraySize; i++)
    FFTdata[i] = FFTfilter[i] = 0;

  /* store SG coefficients in wrap-around order  */
  SavitzkyGolayCoefficients(FFTfilter, 2 * (rows + nLeft + nRight), order, nLeft, nRight, derivativeOrder, 1);

  /* copy the data to work array, with padding to eliminate end effects */
  for (i = 0; i < rows; i++)
    FFTdata[i + nLeft] = data[i];
  for (i = 0; i < nLeft; i++)
    FFTdata[i] = data[0];
  for (i = 0; i < nRight; i++)
    FFTdata[rows + nLeft + i] = data[rows - 1];

  /* Take FFTs of signal and filter */
  realFFT2(FFTdata, FFTdata, 2 * (rows + nLeft + nRight), 0);
  realFFT2(FFTfilter, FFTfilter, 2 * (rows + nLeft + nRight), 0);

  /* Multiply FFTs */
  nfreq = rows + nLeft + nRight + 1;
  for (i = 0; i < nfreq; i++)
    complex_multiply(FFTdata + 2 * i, FFTdata + 2 * i + 1, FFTdata[2 * i], FFTdata[2 * i + 1], FFTfilter[2 * i], FFTfilter[2 * i + 1]);

  /* Do inverse FFT */
  realFFT2(FFTdata, FFTdata, 2 * (rows + nLeft + nRight), INVERSE_FFT);
  for (i = 0; i < rows; i++)
    data[i] = FFTdata[i + nLeft] * 2 * (rows + nLeft + nRight);

  return (1);
}

typedef struct
{
  double *coef;
  long order, right, left, derivOrder;
} SAVITZKYGOLAY_COEF;

static SAVITZKYGOLAY_COEF *svCoef = NULL;
static long nSVCoef = 0;

void SavitzkyGolayCoefficients(double *coef, long maxCoefs, long order, long nLeft, long nRight, long derivativeOrder, long wrapAround) {
  MATRIX *A, *At, *AtA;
  long i, j, m, iStore;
  double factor;
  long iSave;

  if (!coef || order < 0 || derivativeOrder < 0 || derivativeOrder > order || (nLeft + nRight) < order || nLeft < 0 || nRight < 0 || maxCoefs < (nLeft + nRight + 1)) {
    fprintf(stderr, "Error: invalid arguments (savitzkyGolayCoefficients)\n");
    exit(1);
  }

  for (i = 0; i < maxCoefs; i++)
    coef[i] = 0;

  /* see if these coefs are already stored.  if so, just copy them and return. */
  for (iSave = 0; iSave < nSVCoef; iSave++) {
    if (order == svCoef[iSave].order && nLeft == svCoef[iSave].left && nRight == svCoef[iSave].right && derivativeOrder == svCoef[iSave].derivOrder) {
      for (i = -nLeft; i <= nRight; i++) {
        if (wrapAround) {
          if (i <= 0)
            iStore = -i;
          else
            iStore = maxCoefs - i;
        } else
          iStore = i + nLeft;
        coef[iStore] = svCoef[iSave].coef[i + nLeft];
      }
      return;
    }
  }

  m_alloc(&A, nLeft + nRight + 1, order + 1);
  m_alloc(&At, order + 1, nLeft + nRight + 1);
  m_alloc(&AtA, order + 1, order + 1);

  for (i = -nLeft; i <= nRight; i++) {
    factor = 1;
    for (j = 0; j <= order; j++) {
      A->a[i + nLeft][j] = factor;
      factor *= i;
    }
  }

  if (!m_trans(At, A) || !m_mult(AtA, At, A) || !m_invert(AtA, AtA)) {
    fprintf(stderr, "Error: matrix manipulation problem (savitzkyGolayCoefficients)\n");
    exit(1);
  }

  if (!(svCoef = realloc(svCoef, sizeof(*svCoef) * (nSVCoef + 1))) || !(svCoef[nSVCoef].coef = malloc(sizeof(*svCoef[nSVCoef].coef) * (nRight + nLeft + 1)))) {
    fprintf(stderr, "Error: memory allocation failure (savitzkyGolayCoefficients)\n");
    exit(1);
  }
  svCoef[nSVCoef].left = nLeft;
  svCoef[nSVCoef].right = nRight;
  svCoef[nSVCoef].derivOrder = derivativeOrder;
  svCoef[nSVCoef].order = order;

  for (i = -nLeft; i <= nRight; i++) {
    if (wrapAround) {
      if (i <= 0)
        iStore = -i;
      else
        iStore = maxCoefs - i;
    } else
      iStore = i + nLeft;
    coef[iStore] = 0;
    factor = 1;
    for (m = 0; m <= order; m++) {
      coef[iStore] += AtA->a[derivativeOrder][m] * factor;
      factor *= i;
    }
    svCoef[nSVCoef].coef[i + nLeft] = coef[iStore];
  }
  nSVCoef++;
  m_free(&A);
  m_free(&At);
  m_free(&AtA);
}
