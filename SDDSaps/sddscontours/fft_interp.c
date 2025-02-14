/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: contour_interp.c
 * contents: fft_interpolation_index1(), fft_interpolation_index2()
 * purpose: interpolation, low-pass filtering, and other operations on
 *          each dimension of a 2D array.
 * Michael Borland, 1993.
 $Log: not supported by cvs2svn $
 Revision 1.3  1999/06/03 20:42:29  soliday
 Removed compiler warnings under Linux.

 Revision 1.2  1995/09/05 21:08:48  saunders
 First test release of the SDDS1.5 package.

 */

#include "mdb.h"
#include "contour.h"
#include "fftpackC.h"

double **fft_interpolation_index1(double **data, long nx, long ny, long nx_mult, long lowpass, long flags)
{
    long ix, iy, nx1, ix1, nfreq;
    double **newdata;
    static double *buffer;
    static long bufsize = 0;

/*
    if (!power_of_2(nx))
        bomb("old number of grid points must be 2^n", NULL); 
 */
    nx1 = nx*nx_mult;
/*
    if (!power_of_2(nx1))
        bomb("new number of grid points must be 2^n", NULL);
 */

    nfreq = nx/2+1;
    if (lowpass>nfreq)
        bomb("lowpass cutoff frequency is invalid", NULL);

    if (bufsize<(nx1+2)) {
        bufsize = nx1+2;
        buffer = trealloc(buffer, sizeof(*buffer)*bufsize);
        }
    newdata = (double **) zarray_2d(sizeof(**newdata), nx1, ny);
    

    for (iy=0; iy<ny; iy++) {
        for (ix=0; ix<nx; ix++)
            buffer[ix] = data[ix][iy];
        for (ix=nx; ix<nx1+2; ix++)
            buffer[ix] = 0;
        realFFT2(buffer, buffer, nx, 0);
        if (lowpass>0) {
            for (ix=nfreq-lowpass; ix<nfreq; ix++)
                buffer[2*ix] = buffer[2*ix+1] = 0;
            }
        realFFT2(buffer, buffer, nx1, INVERSE_FFT);
        for (ix=0; ix<nx1; ix++)
            newdata[ix][iy] = buffer[ix];
        }

    if (flags&CONTOUR_FLOOR) {
        double min;
        min = DBL_MAX;
        for (ix=0; ix<nx; ix++)
            for (iy=0; iy<ny; iy++)
                if (data[ix][iy]<min)
                    min = data[ix][iy];
        for (ix=0; ix<nx1; ix++)
            for (iy=0; iy<ny; iy++)
                if (newdata[ix][iy]<min)
                    newdata[ix][iy] = min;
        }
    if (flags&CONTOUR_CEILING) {
        double max;
        max = -DBL_MAX;
        for (ix=0; ix<nx; ix++)
            for (iy=0; iy<ny; iy++)
                if (data[ix][iy]>max)
                    max = data[ix][iy];
        for (ix=0; ix<nx1; ix++)
            for (iy=0; iy<ny; iy++)
                if (newdata[ix][iy]>max)
                    newdata[ix][iy] = max;
        }
    if (flags&CONTOUR_ANTIRIPPLE) {
        for (iy=0; iy<ny; iy++)
            for (ix=1; ix<nx; ix++) 
                if (data[ix-1][iy]==0 && data[ix][iy]==0) 
                    for (ix1=0; ix1<nx_mult; ix1++) 
                        newdata[nx_mult*(ix-1)+ix1][iy] = 0;
        }
    
    return(newdata);
    }

double **fft_interpolation_index2(double **data, long nx, long ny, long ny_mult, long lowpass, long flags)
{
    long ix, iy, ny1, iy1, nfreq;
    double **newdata;
    static double *buffer;
    static long bufsize = 0;

/*
    if (!power_of_2(ny))
        bomb("old number of grid points must be 2^n", NULL);
 */
    ny1 = ny*ny_mult;
/*
    if (!power_of_2(ny1))
        bomb("new number of grid points must be 2^n", NULL);
*/

    nfreq = ny/2+1;
    if (lowpass>nfreq)
        bomb("lowpass cutoff frequency is invalid", NULL);

    if (bufsize<(ny1+2)) {
        bufsize = ny1+2;
        buffer = trealloc(buffer, sizeof(*buffer)*bufsize);
        }
    newdata = (double **) zarray_2d(sizeof(**newdata), nx, ny1);

    for (ix=0; ix<nx; ix++) {
        for (iy=0; iy<ny; iy++)
            buffer[iy] = data[ix][iy];
        for (iy=ny; iy<ny1+2; iy++)
            buffer[iy] = 0;
        realFFT2(buffer, buffer, ny, 0);
        if (lowpass>0) {
            for (iy=nfreq-lowpass; iy<nfreq; iy++)
                buffer[2*iy] = buffer[2*iy+1] = 0;
            }
        realFFT2(buffer, buffer, ny1, INVERSE_FFT);
        for (iy=0; iy<ny1; iy++)
            newdata[ix][iy] = buffer[iy];
        }

    if (flags&CONTOUR_FLOOR) {
        double min;
        min = DBL_MAX;
        for (ix=0; ix<nx; ix++)
            for (iy=0; iy<ny; iy++)
                if (data[ix][iy]<min)
                    min = data[ix][iy];
        for (ix=0; ix<nx; ix++)
            for (iy=0; iy<ny1; iy++)
                if (newdata[ix][iy]<min)
                    newdata[ix][iy] = min;
        }
    if (flags&CONTOUR_CEILING) {
        double max;
        max = -DBL_MAX;
        for (ix=0; ix<nx; ix++)
            for (iy=0; iy<ny; iy++)
                if (data[ix][iy]>max)
                    max = data[ix][iy];
        for (ix=0; ix<nx; ix++)
            for (iy=0; iy<ny1; iy++)
                if (newdata[ix][iy]>max)
                    newdata[ix][iy] = max;
        }
    if (flags&CONTOUR_ANTIRIPPLE) {
        for (ix=0; ix<nx; ix++)
            for (iy=1; iy<ny; iy++) 
                if (data[ix][iy-1]==0 && data[ix][iy]==0) 
                    for (iy1=0; iy1<ny_mult; iy1++) 
                        newdata[ix][ny_mult*(iy-1)+iy1] = 0;
        }

    return(newdata);
    }

