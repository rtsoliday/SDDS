/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: fftpackC.c
 * contents: C languange interface to fftpack routines
 * author: M. Borland, 1995.
 $Log: not supported by cvs2svn $
 Revision 1.6  2002/08/14 16:41:20  soliday
 Added Open License

 Revision 1.5  1999/05/28 14:55:01  soliday
 Removed compiler warnings under Linux.

 Revision 1.4  1999/04/22 03:34:10  borland
 Change to avoid WIN32 compiler warning.

 Revision 1.3  1999/04/22 03:32:43  borland
 Eliminated references to mdb.h to solve WIN32 build problem.

 * Revision 1.2  1995/09/05  21:13:16  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "stdio.h"
#include "stdlib.h"
#include "fftpack.h"
#include "fftpackC.h"
#include "string.h"

#if defined(linux) || (defined(_WIN32) && !defined(_MINGW))
#  include <omp.h>
#else
#  define NOTHREADS 1
#endif

#define SWAP_DOUBLE(x, y)   \
  {                         \
    double tmp_swap_double; \
    tmp_swap_double = (x);  \
    (x) = (y);              \
    (y) = tmp_swap_double;  \
  }

typedef struct {
  double *array;
  long length;
  short inUse;
} FFTPACK_WORKSPACE;

FFTPACK_WORKSPACE *realWorkspace = NULL;
long realWorkspaces = 0;

FFTPACK_WORKSPACE *complexWorkspace = NULL;
long complexWorkspaces = 0;

short atexitFFTpackRegistered = 0;

void atexitFFTpack(void) {
  long i;
  if (realWorkspace) {
    for (i = 0; i < realWorkspaces; i++)
      if (realWorkspace[i].array)
        free(realWorkspace[i].array);
    free(realWorkspace);
    realWorkspace = NULL;
    realWorkspaces = 0;
  }
  if (complexWorkspace) {
    for (i = 0; i < complexWorkspaces; i++)
      if (complexWorkspace[i].array)
        free(complexWorkspace[i].array);
    free(complexWorkspace);
    complexWorkspace = NULL;
    complexWorkspaces = 0;
  }
  atexitFFTpackRegistered = 0;
}

long realFFT(double *data, long n, unsigned long flags) {
  long i, iWorkspace, memerror=0;

  if (flags & (~(INVERSE_FFT | MINUS_I_THETA))) {
    fputs("invalid flag bits set for realFFT()\n", stderr);
    exit(1);
  }

  if (n < 1)
    return 0;
  if (n == 1)
    return 1;

#pragma omp critical
  {
    if (!atexitFFTpackRegistered) {
      atexit(atexitFFTpack);
      atexitFFTpackRegistered = 1;
    }
    
    for (iWorkspace = 0; iWorkspace < realWorkspaces; iWorkspace++)
      if (n == realWorkspace[iWorkspace].length && !realWorkspace[iWorkspace].inUse)
        break;
    if (iWorkspace == realWorkspaces) {
      if (!realWorkspace)
        realWorkspace = (FFTPACK_WORKSPACE *)
          malloc(sizeof(*realWorkspace) * (realWorkspaces + 1));
      else
        realWorkspace = (FFTPACK_WORKSPACE *)
          realloc(realWorkspace, sizeof(*realWorkspace) * (realWorkspaces + 1));
      if (!realWorkspace ||
          !(realWorkspace[realWorkspaces].array = (double *)malloc(sizeof(double) * (2 * n + 15)))) {
        memerror=1;
      } else {
        realWorkspace[realWorkspaces].length = n;
        realWorkspaces++;
        rffti_(&n, realWorkspace[iWorkspace].array);
      }
    }
    realWorkspace[iWorkspace].inUse = 1;
  }
  if (memerror) {
    fputs("allocation error in realFFT()\n", stderr);
    return 0;
  }
  if (!(flags & INVERSE_FFT)) {
    /* forward FFT:
           data[0] = DC term
           (data[1], data[2]) = (real, imag) first harmonic
           ...
           if (n is even) data[n-1] = Nyquist frequency term
           else data ends with (real, imag) for frequency (n-1)/2.
         */
    rfftf_(&n, data, realWorkspace[iWorkspace].array);

    /* normalize the transform */
    for (i = 0; i < n; i++)
      data[i] /= n;
    if (flags & MINUS_I_THETA)
      /* convert to exp(-i*theta) convention for DFT commposition */
      for (i = 2; i < n; i += 2)
        data[i] *= -1;
  } else {
    if (flags & MINUS_I_THETA)
      /* convert from exp(-i*theta) convention for DFT commposition */
      for (i = 2; i < n; i += 2)
        data[i] *= -1;
    rfftb_(&n, data, realWorkspace[iWorkspace].array);
  }
#pragma omp critical
  {
    realWorkspace[iWorkspace].inUse = 0;
  }
  return 1;
}

long realFFT2(double *output, double *input, long n, unsigned long flags)
/* for this routine, I use two arrays and more convenient storage of the IFFT:
   fft[0], fft[1] = (real, imag) of DC (imag is of course==0)
   fft[2], fft[3] = (real, imag) of harmonic 1
   ...
   if (n is even)
      fft[n], fft[n+1] = (real, imag) of Nyquist (imag is == 0)
   else
      fft ends with (fft[n-1], fft[n])
   Note that the fft input or output array is required to be 2*n+2 in length,
   where n is the number of points in the real sequence.
 */
{
  long i;

  if (!input)
    input = output;

  if (!(flags & INVERSE_FFT)) {
    if (input != output)
      memcpy((char *)(output + 1), (char *)input, sizeof(*output) * n);
    else
      for (i = n - 1; i >= 0; i--)
        output[i + 1] = output[i];
    if (!realFFT(output + 1, n, flags))
      return 0;
    output[0] = output[1]; /* DC real term */
    output[1] = 0;         /* DC imag term */
    if (n % 2 == 0)
      output[n + 1] = 0; /* Nyquist real term */
    return 1;
  }

  /* inverse FFT: input is floor[n/2] complex points */
  output[0] = input[0];
  for (i = 1; i <= n; i++)
    output[i] = input[i + 1];
  if (!realFFT(output, n, flags))
    return 0;
  return 1;
}

long complexFFT(double *data, long n, unsigned long flags) {
  long i, iWorkspace, memerror=0;

  if (flags & (~(INVERSE_FFT | MINUS_I_THETA))) {
    fputs("invalid flag bits set for realFFT()\n", stderr);
    exit(1);
  }

  if (n < 1)
    return 0;
  if (n == 1)
    return 1;

#pragma omp critical
  {
    if (!atexitFFTpackRegistered) {
#if defined(ATEXIT_AVAIL)
      atexit(atexitFFTpack);
#endif
      atexitFFTpackRegistered = 1;
    }

    for (iWorkspace = 0; iWorkspace < complexWorkspaces; iWorkspace++)
      if (n == complexWorkspace[iWorkspace].length && !complexWorkspace[iWorkspace].inUse)
        break;
    if (iWorkspace == complexWorkspaces) {
      if (!complexWorkspace)
        complexWorkspace = (FFTPACK_WORKSPACE *)malloc(sizeof(*complexWorkspace) * (complexWorkspaces + 1));
      else
        complexWorkspace = (FFTPACK_WORKSPACE *)realloc(complexWorkspace, sizeof(*complexWorkspace) * (complexWorkspaces + 1));
      if (!complexWorkspace ||
          !(complexWorkspace[complexWorkspaces].array = (double *)malloc(sizeof(double) * (4 * n + 15)))) {
        memerror = 1;
      } else {
        complexWorkspace[complexWorkspaces].length = n;
        complexWorkspaces++;
        cffti_(&n, complexWorkspace[iWorkspace].array);
      }
    }
    complexWorkspace[iWorkspace].inUse = 1;
  }
  if (memerror) {
    fputs("allocation error in complexFFT()\n", stderr);
    return 0;
  }
  if (!(flags & INVERSE_FFT)) {
    /* forward FFT */
    cfftf_(&n, data, complexWorkspace[iWorkspace].array);

    /* normalize the transform */
    for (i = 0; i < 2 * n; i++)
      data[i] /= n;

    if (flags & MINUS_I_THETA) {
      /* convert to exp(-i*theta) convention for DFT commposition */
      for (i = 2; i < n; i += 2) {
        SWAP_DOUBLE(data[i], data[2 * n - i]);
        SWAP_DOUBLE(data[i + 1], data[2 * n - i + 1]);
      }
    }
  } else {
    if (flags & MINUS_I_THETA) {
      /* convert to exp(-i*theta) convention for DFT commposition */
      for (i = 2; i < n; i += 2) {
        SWAP_DOUBLE(data[i], data[2 * n - i]);
        SWAP_DOUBLE(data[i + 1], data[2 * n - i + 1]);
      }
    }

    /* inverse FFT */
    cfftb_(&n, data, complexWorkspace[iWorkspace].array);
  }
#pragma omp critical
  {
    complexWorkspace[iWorkspace].inUse = 0;
  }
  return 1;
}
