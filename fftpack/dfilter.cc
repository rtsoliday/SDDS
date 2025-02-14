/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file    : dfilter.cc
 * contents: digitalFilter()
 *           copy_dp_array()
 *           dp_pad_with_zeroes()
 *           power_of_2()
 *           fft_derivative()
 *
 * Michael Borland, 1990-5
 $Log: not supported by cvs2svn $
 Revision 1.7  2009/12/02 22:22:34  soliday
 Added complex number support for non C99 compilers.

 Revision 1.6  2009/11/09 18:01:51  borland
 Changed to use C99 complex routines.

 Revision 1.5  2002/08/14 16:41:19  soliday
 Added Open License

 Revision 1.4  1999/09/14 18:19:39  soliday
 dp_pad_with_zeroes and power_of_2 are no longer defined locally

 Revision 1.3  1999/05/28 14:54:37  soliday
 Removed compiler warnings under Linux.

 Revision 1.2  1995/09/05 21:13:03  saunders
 First test release of the SDDS1.5 package.

 */
#include <complex>
#include "mdb.h"
#include "fftpackC.h"

void copy_dp_array(double **c, double *o, long n);


void digitalFilter(
    double *T, double *Y, long n_pts0,
    double **T_out, double **Y_out, long *n_out,     /* output data */
    long do_window,
    double high_pass_freq1, double high_pass_freq2,
    double low_pass_freq1, double low_pass_freq2,
    long do_pad, long do_truncate, long block_dc, long zp_spectrum,
    double threshold, long filter_points, double **filter
    )
{
    long i, n_freq, n_pts, code;
    double r1, r2, length, factor, amp, max_amp, freq;
    double *t, *y, *real_imag, dt;
 
    n_pts = n_pts0;
    if (n_pts<=1)
      bomb((char*)"Too few data points.", NULL);
    if (largest_prime_factor(n_pts)>100)
        puts("Warning: number of points has large prime factors.\nThis could take a very long time.\nConsider padding or truncating to 2^n points.");

    copy_dp_array(&t, T, n_pts);
    copy_dp_array(&y, Y, n_pts);
    if (!power_of_2(n_pts)) {
        if (do_pad)
            n_pts = dp_pad_with_zeroes(&t, &y, n_pts);
        else if (do_truncate)
            printf("data truncated at %ld points\n",
                n_pts = ipow(2., (long)(log((double)n_pts)/log(2.0F)) ));
        }
    n_freq = n_pts/2+1;

    /* do windowing, if requested, using Parzen window */
    if (do_window) {
        r1 = (n_pts-1)/2.0;
        r2 = sqr((n_pts+1)/2.0);
        for (i=0; i<n_pts; i++) 
            y[i] *= 1 - sqr(i-r1)/r2;
        }

    real_imag = (double*)tmalloc(sizeof(double)*(n_pts+2));
    realFFT2(real_imag, y, n_pts, 0);

    /* calculate factor for converting k to f or omega*/
    length = (t[n_pts-1]-t[0])+(dt=t[1]-t[0]);
    factor = 1.0/length;

    /* apply filters, if requested */
    if (block_dc)
        real_imag[0] = real_imag[1] = 0;
    if (high_pass_freq1>=0) {
        long i1, i2;

        if (high_pass_freq2<0)
            high_pass_freq2 = high_pass_freq1;
        if ((i1 = high_pass_freq1/factor+.5)>=n_freq)
            i1 = n_freq-1;
        if (i1<0)
            i1 = 0;
        if ((i2 = high_pass_freq2/factor+.5)>=n_freq)
            i2 = n_freq-1;
#if defined(DEBUG)
        printf("high pass: i1=%ld, i2=%ld\n", i1, i2);
#endif
        if (i2<0)
            i2 = 0;
        if (i1!=i2) {
            for (i=i1; i<i2; i++) {
                real_imag[2*i]   *= ((double)(i-i1))/(i2-i1);
                real_imag[2*i+1] *= ((double)(i-i1))/(i2-i1);
                }
            }
        for (i=0; i<=i1; i++)
            real_imag[2*i] = real_imag[2*i+1] = 0;
        }
    if (low_pass_freq1>=0) {
        long i1, i2;

        if (low_pass_freq2<0)
            low_pass_freq2 = low_pass_freq1;
        if ((i1 = low_pass_freq1/factor+.5)>=n_freq)
            i1 = n_freq-1;
        if (i1<0)
            i1 = 0;
        if ((i2 = low_pass_freq2/factor+.5)>=n_freq)
            i2 = n_freq-1;
        if (i2<0)
            i2 = 0;
#if defined(DEBUG)
        printf("low pass: i1=%ld, i2=%ld\n", i1, i2);
#endif
        if (i1!=i2) {
            for (i=i1+1; i<=i2; i++) {
                real_imag[2*i]   *= ((double)(i2-i))/(i2-i1);
                real_imag[2*i+1] *= ((double)(i2-i))/(i2-i1);
                }
            }
        for (i=i2; i<n_freq; i++)
            real_imag[2*i] = real_imag[2*i+1] = 0;
        }
    /* apply threshold */
    if (threshold!=0) {
        if (threshold<0) {
            /* fractional threshold */
            max_amp = 0;
            for (i=0; i<n_freq; i++)
                if ((amp=sqr(real_imag[2*i])+sqr(real_imag[2*i+1]))>max_amp)
                    max_amp = amp;
            threshold = sqr(threshold)*max_amp;
            }
        else
            /* absolute threshold */
            threshold = sqr(threshold*n_pts/2);
        for (i=0; i<n_freq; i++)
            if (sqr(real_imag[2*i])+sqr(real_imag[2*i+1])<threshold)
                real_imag[2*i] = real_imag[2*i+1] = 0;
        }
    /* apply filter function */
    if (filter && filter_points>=2) {
      std::complex <double> filter_value(0,0), product;
      for (i=0; i<n_freq; i++) {
        freq = i*factor;
        if (freq<=filter[0][filter_points-1] && freq>=filter[0][0]) {
          filter_value = interp(filter[1], filter[0], filter_points, freq, 0, 1, &code);
          if (filter[2])
	    filter_value += std::complex<double>(0,1)*interp(filter[2], filter[0], filter_points, freq, 0, 1, &code);
	  product = filter_value*(real_imag[2*i] + std::complex<double>(0,1)*real_imag[2*i+1]);
	  real_imag[2*i  ] = product.real();
	  real_imag[2*i+1] = product.imag();
	}
      }
    }
    
    factor = 1;
    if (zp_spectrum>1) {
      /* pad the spectrum */
      real_imag = (double*)trealloc(real_imag, sizeof(double)*2*(zp_spectrum*n_pts));
      for (i=2*n_pts; i<2*zp_spectrum*n_pts; i++)
        real_imag[i] = 0;
      n_freq *= zp_spectrum;
      n_pts  *= zp_spectrum;
      dt /= zp_spectrum;
      factor = zp_spectrum;
    }
    
    /* compute IFFT */
    realFFT2(real_imag, real_imag, n_pts, INVERSE_FFT);

    /* copy result into output data arrays */
    if (!*T_out || !*Y_out) {
        /* at least on array is not allocated */
        if (!(!*T_out && !*Y_out))
            bomb((char*)"calling routine supplied only one of two output arrays (dp_digital_filter)", NULL);
        *n_out = n_pts;
        *T_out = (double*)tmalloc(sizeof(**T_out)*n_pts);
        *Y_out = (double*)tmalloc(sizeof(**Y_out)*n_pts);
        }
    else {
        /* both arrays are allocated--may need to lengthen, however */
        if (n_pts>n_pts0) {
            *T_out = (double*)trealloc(*T_out, sizeof(**T_out)*n_pts);
            *Y_out = (double*)trealloc(*Y_out, sizeof(**Y_out)*n_pts);
            }
        }

    for (i=0; i<n_pts; i++) {
        (*Y_out)[i] = factor*real_imag[i];
        (*T_out)[i] = t[0]+i*dt;
        }

    free(real_imag);
    free(t);
    free(y);
    }

long power_of_2(long n)
{
    while (n%2==0) {
        n /= 2;
        if (n==1)
            return(1);
        }
    return(0);
    }

long dp_pad_with_zeroes(double **t, double **f, long n)
{
    long np2, i;
    double dt;

    np2 = ipow(2., (long)(log((double)n)/log(2.0F)+1.) );
    *t  = (double*)trealloc(*t, sizeof(**t)*np2);
    *f  = (double*)trealloc(*f, sizeof(**f)*np2);
    dt  = ((*t)[n-1] - (*t)[0])/(n-1);
    for (i=n; i<np2; i++) {
        (*t)[i] = (*t)[n-1] + (i-n+1)*dt;
        (*f)[i] = 0;
        }
    return(np2);        
    }

void copy_dp_array(double **c, double *o, long n)
{
    *c = (double*)tmalloc(sizeof(**c)*n);
    while (n--)
        (*c)[n] = o[n];
    }

void FFTderivative(
    double *T, double *Y, long n_pts0,
    double **T_out, double **Y_out, long *n_out,     /* output data */
    long do_pad, long do_truncate, long zp_spectrum
    )
{
    long i, n_freq, n_pts;
    double length, factor;
    double *t, *y, *real_imag, dt;
    double real, imag;

    n_pts = n_pts0;
    if (n_pts<=1)
      bomb((char*)"Too few data points.", NULL);
    copy_dp_array(&t, T, n_pts);
    copy_dp_array(&y, Y, n_pts);
    if (!power_of_2(n_pts)) {
        if (do_pad)
            n_pts = dp_pad_with_zeroes(&t, &y, n_pts);
        else if (do_truncate)
            printf("data truncated at %ld points\n",
                n_pts = ipow(2., (long)(log((double)n_pts)/log(2.0F)) ));
        }
    n_freq = n_pts/2+1;

    real_imag = (double*)tmalloc(sizeof(double)*(n_pts+2));
    realFFT2(real_imag, y, n_pts, 0);

    /* calculate factor for converting k to f or omega*/
    length = (t[n_pts-1]-t[0])+(dt=t[1]-t[0]);
    factor = PIx2/length;

    for (i=0; i<n_freq; i++) {
        real = real_imag[2*i];
        imag = real_imag[2*i+1];
        real_imag[2*i]   = -i*factor*imag;
        real_imag[2*i+1] =  i*factor*real;
        }

    factor = 1;
    if (zp_spectrum>1) {
        /* pad the spectrum */
        real_imag = (double*)trealloc(real_imag, sizeof(double)*2*(zp_spectrum*n_pts));
        for (i=2*n_pts; i<2*zp_spectrum*n_pts; i++)
            real_imag[i] = 0;
        n_freq *= zp_spectrum;
        n_pts  *= zp_spectrum;
        dt /= zp_spectrum;
        factor = zp_spectrum;
        }

    /* compute IFFT */
    realFFT2(real_imag, real_imag, n_pts, INVERSE_FFT);

    /* copy result into output data arrays */
    if (!*T_out || !*Y_out) {
        /* at least on array is not allocated */
        if (!(!*T_out && !*Y_out))
          bomb((char*)"calling routine supplied only one of two output arrays (dp_digital_filter)", NULL);
        *n_out = n_pts;
        *T_out = (double*)tmalloc(sizeof(**T_out)*n_pts);
        *Y_out = (double*)tmalloc(sizeof(**Y_out)*n_pts);
        }
    else {
        /* both arrays are allocated--may need to lengthen, however */
        if (n_pts>n_pts0) {
            *T_out = (double*)trealloc(*T_out, sizeof(**T_out)*n_pts);
            *Y_out = (double*)trealloc(*Y_out, sizeof(**Y_out)*n_pts);
            }
        }
    for (i=0; i<n_pts; i++) {
        (*Y_out)[i] = factor*real_imag[i];
        (*T_out)[i] = t[0]+i*dt;
        }

    free(real_imag);
    free(t);
    free(y);
    }

