/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


#ifdef __cplusplus
extern "C" {
#endif

#if !defined(FFTPACKC_INCLUDE)
#define FFTPACKC_INCLUDE 1

#undef epicsShareFuncFFTPACK
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_FFTPACK)
#define epicsShareFuncFFTPACK  __declspec(dllexport)
#else
#define epicsShareFuncFFTPACK
#endif
#else
#define epicsShareFuncFFTPACK
#endif

#define INVERSE_FFT 0x0001UL
#define MINUS_I_THETA 0x0002UL
/* If MINUS_I_THETA bit is set, then fftpack routines
   operate with a different convention than the normal
   one.  The Fourier sum is
   Sum[j=0,n-1]{ A(k)*exp[-i*j*k*2*pi/n] }
   This corresponds to a exp[+i ...] in the Fourier
   decomposition.
   However, by default, the signs of i in these expressions
   are reversed.  This is the more usual convention.
 */

epicsShareFuncFFTPACK void atexitFFTpack();
epicsShareFuncFFTPACK long realFFT(double *data, long n, unsigned long flags);
epicsShareFuncFFTPACK long complexFFT(double *data, long n, unsigned long flags);
epicsShareFuncFFTPACK long realFFT2(double *output, double *input, long n, unsigned long flags);
epicsShareFuncFFTPACK void FFTderivative(double *T, double *Y, long n_pts0,
    double **T_out, double **Y_out, long *n_out, long do_pad,
    long do_truncate, long zp_spectrum);
epicsShareFuncFFTPACK extern long power_of_2(long n);
epicsShareFuncFFTPACK long dp_pad_with_zeroes(double **t, double **f, long n);

#define NAFF_RMS_CHANGE_LIMIT    0x0001U
#define NAFF_FREQS_DESIRED       0x0002U
#define NAFF_MAX_FREQUENCIES     0x0004U
#define NAFF_FREQ_CYCLE_LIMIT    0x0008U
#define NAFF_FREQ_ACCURACY_LIMIT 0x0010U
#define NAFF_FREQ_FOUND          0x0100U
epicsShareFuncFFTPACK long CalculatePhaseAndAmplitudeFromFreq
  (double *hanning, long points, double NAFFdt, double frequency, double t0,
   double *phase, double *amplitude, double *significance, double *cosine, double *sine,
   unsigned long flags);

epicsShareFuncFFTPACK long PerformNAFF(double *frequency, double *amplitude, double *phase, 
				       double *significance, double t0, double dt,
				       double *data, long points, unsigned long flags,
				       double fracRMSChangeLimit, long maxFrequencies,
				       double freqCycleLimit, double fracFreqAccuracyLimit,
				       double lowerFreqLimit, double upperFreqLimit);

epicsShareFuncFFTPACK long simpleFFT(double *magnitude2, double *data, long points);
epicsShareFuncFFTPACK double adjustFrequencyHalfPlane(double frequency, 
                                                double phase0, double phase1, double dt);

#endif

#ifdef __cplusplus
}
#endif

