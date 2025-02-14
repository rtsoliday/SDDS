/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* The following must be 1024 or less for WIN32 */
#define FIXED_MSG_SIZE 1024

int MIF_init(void);
int CMIF_init(void);
int MIF_graphics(void);
int MIF_text(void);
int MIF_reset(void);
int MIF_linetype(int linetype);
int MIF_line_thickness(int line_thickness);
int MIF_move(int x, int y);
int MIF_vector(int x, int y);
int MIF_dot(int x, int y, int number);
int MIF_fill_box(int shade, int xl, int xh, int yl, int yh);
int PS1_init(void);
int CPS1_init(void);
int PS1_init_rot(void);
int CPS1_init_rot(void);
int EPSF1_init(void);
int PS_move(int x, int y);
int PS_vector(int x, int y);
int PS_linetype(int linetype);
int PS_fill_box(int shade, int xl, int xh, int yl, int yh);
int PS_graphics(void);
int CPS_graphics(void);
int PS_dot(int x, int y, int number);
int EPSF_graphics(void);
int CEPSF_graphics(void);
int EPSF_text(void);
int EPSF_reset(void);
int PS_text(void);
int PS_reset(void);
int CPS_linetype(int linetype);
int CPS_fill_box(int shade, int xl, int xh, int yl, int yh);
int PS_line_thickness(int line_thickness);
int CPS_color(unsigned short red, unsigned short green, unsigned short blue);
int CPS_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);

int PNG_init(void);
int LPNG_init(void);
int PNG_graphics(void);
int PNG_text(void);
int PNG_reset(void);
int PNG_linetype(int linetype);
int PNG_line_thickness(int linetype);
int PNG_move(int x, int y);
int PNG_vector(int x, int y);
int PNG_dot(int x, int y, int number);
int PNG_fill_box(int shade, int xl, int xh, int yl, int yh);
int PNG_color(unsigned short red, unsigned short green, unsigned short blue);
int PNG_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);
void PNG_passCommandLineToPNGFromMotif(char *command);
void PS_passCommandLineToPSFromMotif(char *command);

void xerrmsg(int x, const char *fmt, ...);

