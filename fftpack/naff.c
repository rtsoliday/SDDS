/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "mdb.h"
/*#include "SDDS.h"*/
#include "fftpackC.h"
#include <ctype.h>

/* functions to perform Numerical Analysis of Fundamental Frequencies
 * (Laskar's method).
 * M. Borland, ANL/APS, 2001
 */

/* basic FFT of real data: return the magnitude */

long simpleFFT(double *magnitude2, double *data, long points)
{
  static double *real_imag = NULL;
  static long sizeLimit = 0;
  long FFTFreqs, i;

  if (sizeLimit<(points+2) &&
      !(real_imag = malloc(sizeof(*real_imag)*(sizeLimit=points+2)))) 
      return 0;

  realFFT2(real_imag, data, points, 0);
  FFTFreqs = points/2+1;
  for (i=0; i<FFTFreqs; i++) {
    if (i!=0 && !(i==(FFTFreqs-1) && points%2==0)) {
      real_imag[2*i  ] *= 2;
      real_imag[2*i+1] *= 2;
    }
    magnitude2[i] = sqr(real_imag[2*i]) + sqr(real_imag[2*i+1]);
  }
  return FFTFreqs;
}

double *NAFFData, NAFFdt;
long NAFFPoints;

/* used to find optimum frequency */
double NAFFFunc(double omega, long *invalid)
{
  long i;
  double sum1, sum2, cosine, sine;
  
  *invalid = 0;
  for (i=sum1=sum2=0; i<NAFFPoints; i++) {
    cosine = cos(omega*i*NAFFdt);
    sine   = sin(omega*i*NAFFdt);
    sum1  += cosine*NAFFData[i];
    sum2  += sine*NAFFData[i];
  }
  return sqr(sum1)+sqr(sum2);
}


long PerformNAFF(double *frequency,   /* return or input frequencies */
		 double *amplitude,   /* return amplitudes */
		 double *phase,       /* return phases */
                 double *significance, /* return "significance" (0=very, large=not) */
		 double t0,           /* initial "time" or independent value */
                 double dt,           /* spacing of time points */
		 double *data,        /* data to be analyzed */
		 long points,         /* number of data points */
		 unsigned long flags, /* control flags (NAFF_* in mdb.h) */
		 /* these control termination of the iteration for frequencies: */
		 /* min acceptable contribution of frequency */
		 double fracRMSChangeLimit, 
		 /* maximum number of frequencies */
		 long maxFrequencies,
		 /* these control the accuracy of each frequency: */
		 /* maximum iteractions of parabolic optimizer */
		 double freqCycleLimit, 
		 /* acceptable fractional accuracy of frequency */
		 double fracFreqAccuracyLimit,
		 /* search only for frequencies between these limits */
		 double lowerFreqLimit, 
		 double upperFreqLimit)
{
  double rmsOrig, rmsLast, rmsNow, mean, *magnitude2, *hanning;
  long i, freqsFound = 0, FFTFreqs;
  double wStart, freqSpacing;
  long invalid, iBest, code, try;
  double maxMag2, *sine, *cosine;

  if (points<2) {
    return -1;
  }
  sine=cosine=NAFFData=NULL;
  if (!(magnitude2 = malloc(sizeof(*magnitude2)*points)) ||
      !(hanning = malloc(sizeof(*hanning)*points)) ||
      !(NAFFData = malloc(sizeof(*NAFFData)*points)) ||
      !(cosine = malloc(sizeof(*cosine)*points)) ||
      !(sine = malloc(sizeof(*sine)*points)))
    return -1;
  freqSpacing = 1./(points*dt);
  NAFFdt = dt;
  
  /* subtract off mean and apply the Hanning window */
  mean = arithmeticAverage(data, points);
  for (i=0; i<points; i++) {
    hanning[i]  = (1 - cos(PIx2*i/(points-1.0)))/2;
    NAFFData[i] = (data[i]-mean)*hanning[i];
  }
  rmsOrig = 0;
  if (fracRMSChangeLimit) {
    for (i=rmsOrig=0; i<points; i++)
      rmsOrig += sqr(NAFFData[i]);
    rmsOrig = sqrt(rmsOrig/points);
  }
  rmsLast = rmsOrig;
  
  FFTFreqs = points/2-1;
  NAFFPoints = points;
  for (i=0; i<maxFrequencies; i++) 
    amplitude[i] = phase[i] = significance[i] = -1;
  if (!(flags&NAFF_FREQ_FOUND)) {
    for (i=0; i<maxFrequencies; i++) 
      frequency[i] = -1;
    while (freqsFound<maxFrequencies) {
      simpleFFT(magnitude2, NAFFData, points);
      maxMag2 = 0;
      iBest = 0;
      for (i=0; i<FFTFreqs; i++) {
        if (magnitude2[i]>maxMag2) {
          if (lowerFreqLimit<upperFreqLimit && 
              (i*freqSpacing<lowerFreqLimit ||
               i*freqSpacing>upperFreqLimit))
            continue;
          iBest = i;
          maxMag2 = magnitude2[i];
        }
      }
      if (iBest==0)
        break;
      wStart = frequency[freqsFound] = iBest*freqSpacing*PIx2;
      NAFFFunc(wStart, &invalid);
      for (try=0; try<2; try++) {
        code = OneDParabolicOptimization(amplitude+freqsFound, frequency+freqsFound,
                                         PIx2*freqSpacing, 
                                         0.0, PI/dt, NAFFFunc, 
                                         freqCycleLimit, 
                                         fracFreqAccuracyLimit*PI/dt, 
                                         0.0, 1);
        if (code<0) {
          /* amplitude[freqsFound] = frequency[freqsFound] = -1; */
          frequency[freqsFound] = wStart;
          amplitude[freqsFound] = maxMag2;
          break;
        }
      }
      /*if (code<0)
        break; */
      
      /* remove this component by computing the overlap with cosine and sine functions
       * then subtracting it 
       */
      /*
        fprintf(stderr, "f=%10.3e  a=%10.3e  p=%10.3e  s=%10.3e\n",
        frequency[freqsFound], amplitude[freqsFound], phase[freqsFound], significance[freqsFound]);
        */
      CalculatePhaseAndAmplitudeFromFreq(hanning, points, NAFFdt, frequency[freqsFound], t0,
                                         &phase[freqsFound], &amplitude[freqsFound],
                                         &significance[freqsFound], cosine, sine, flags);
      frequency[freqsFound] /=PIx2;
      freqsFound ++;
      
      rmsNow = 0;
      if (fracRMSChangeLimit) {
        /* determine if residual is too small to bother with */
        for (i=rmsNow=0; i<points; i++) 
          rmsNow += sqr(NAFFData[i]);
        rmsNow = sqrt(rmsNow/points);
        if ((rmsLast-rmsNow)/rmsOrig < fracRMSChangeLimit)
          break;
      }
      rmsLast = rmsNow;
      if (code<0)
        break;
    }
  } else {
    freqsFound = 0;
    for (i=0;i<maxFrequencies;i++) {
      if (frequency[i]==-1)
        break;
      CalculatePhaseAndAmplitudeFromFreq(hanning, points, NAFFdt, frequency[i], t0,
                                         &phase[i], &amplitude[i],
                                         &significance[i], cosine, sine, flags);
      freqsFound ++;
    }
  }
  free(NAFFData);
  free(magnitude2);
  free(hanning);
  free(cosine);
  free(sine);
  return freqsFound;
}

long CalculatePhaseAndAmplitudeFromFreq
  (double *hanning, long points, double NAFFdt, double frequency, double t0,
   double *phase, double *amplitude, double *significance, double *cosine, double  *sine,
        unsigned long flags)
{
  long i;
  double sum_ee1, sum_ee2, sum_ef1, sum_ef2;
  double sum1, sum2, freq0;
  
  if (flags&NAFF_FREQ_FOUND)
    freq0 = frequency * PIx2;
  else 
    freq0 = frequency;
  sum_ee1 = sum_ee2 = sum_ef1 = sum_ef2 = 0;
  for (i=0; i<points; i++) {
    cosine[i] =  cos(freq0*i*NAFFdt);
    sine[i]   =  sin(freq0*i*NAFFdt);
    /* this gives normalization of the overlap sums */
    sum_ee1 += sqr(cosine[i])*hanning[i];
    sum_ee2 += sqr(sine[i])*hanning[i];
    /* these are the overlap sums */
    sum_ef1 += cosine[i]*NAFFData[i];
    sum_ef2 += sine[i]*NAFFData[i];
  }
  for (i=sum1=0; i<points; i++) 
    sum1 += sqr(NAFFData[i]);
  for (i=0; i<points; i++)
    NAFFData[i] -= (sum_ef1/sum_ee1*cosine[i] + sum_ef2/sum_ee2*sine[i])*hanning[i];
  for (i=sum2=0; i<points; i++) 
    sum2 += sqr(NAFFData[i]);
  
  if (sum1>0)
    *significance = sum2/sum1;
  else
    *significance = -1;
  
  if (flags&NAFF_FREQ_FOUND)
     freq0 = frequency;
  else 
     freq0 = frequency / PIx2;
  *amplitude = sqrt(sqr(sum_ef1/sum_ee1)+sqr(sum_ef2/sum_ee2));
  
  /* compute the phase and ensure that it is in the range [-PI, PI] */
  *phase = fmod(atan2(-sum_ef2/sum_ee2, sum_ef1/sum_ee1) + 
                freq0*t0*PIx2, PIx2);
  if (*phase<-PI)
    *phase += PIx2;
  if (*phase>PI)
    *phase -= PIx2;
  return 0;
}

double adjustFrequencyHalfPlane(double frequency, double phase0, double phase1, double dt)
{
  if (fabs(phase0-phase1)>PI) {
    if (phase0<phase1)
      phase0 += PIx2;
    else 
      phase1 += PIx2;
  }
  if (phase0<phase1)
    return frequency;
  return 1/dt-frequency;
}
