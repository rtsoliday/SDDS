/* prototypes for this file are in term.proto */
/* GNUPLOT - term.c */
/*
 * Copyright (C) 1986,  1987,  1990   Thomas Williams,  Colin Kelley
 *
 * Permission to use,  copy,  and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,  
 * provided that the above copyright notice appear in all copies and 
 * that both that copyright notice and this permission notice appear 
 * in supporting documentation.
 *
 * Permission to modify the software is granted,  but not the right to
 * distribute the modified code.  Modifications are to be distributed 
 * as patches to released version.
 *  
 * This software  is provided "as is" without express or implied warranty.
 * 
 *
 * AUTHORS
 * 
 *   Original Software:
 *     Thomas Williams,   Colin Kelley.
 * 
 *   Gnuplot 2.0 additions:
 *       Russell Lang,  Dave Kotz,  John Campbell.
 * 
 * send your comments or suggestions to (pixar!info-gnuplot@sun.com).
 *
 *
 
 $Log: not supported by cvs2svn $
 Revision 1.28  2011/03/18 18:15:20  borland
 Added gpng (gigantic PNG) type.

 Revision 1.27  2011/01/11 22:51:04  soliday
 Changed all the strcpy commands to strcpy_ss because of problems with
 RedHat Enterprise 6. If a strcpy copies the result to the same memory
 space you will get unexpected results.

 Revision 1.26  2010/07/29 22:09:10  borland
 Added "hpng" device type (huge PNG, twice as large as lpng).

 Revision 1.25  2008/06/11 16:09:50  soliday
 Reindented code.

 Revision 1.24  2008/06/11 16:08:16  soliday
 Added new symbols.

 Revision 1.23  2006/03/21 17:52:00  soliday
 Fixed problems with the intesity bar with ceps and png output. Also modified
 the split by column data option to automatically find the minimum value
 for a plot request and use that as the start value if no start value is given.
 This enables multiple plots to have the same scale even if they are in
 different parts of the range.

 Revision 1.22  2006/01/03 23:22:13  jiaox
 corrected circle drawing in function do_point.

 Revision 1.21  2005/12/07 22:01:11  jiaox
 Made the line types ( thickness color dash style) consistent among motif,
 postscript and png devices. Added device arguments dashes and linetypetable
 for these devices to draw lines with user defined line styles.

 Revision 1.20  2005/08/15 14:07:59  jiaox
 Added keyword "fill" for graphic option. Also added circel type(9) for symbols.

 
*/
#include <stdio.h>
#include <string.h>
#include "graphics.h"
#include "graph.h"
/* #include "setshow.h" */
char *outstr="stdout";
#include "term.h"
#include <math.h>
#include "mdb.h"

#if !defined(__TURBOC__) || defined(__BORLANDC__)
#define REGIS
#define POSTSCRIPT
#define MIF
#define HPGL
#define LATEX
#define TEK
#define EEPIC
#define PNG
#endif
#define X11
#ifdef SUNOS4
#define SUN
#endif /* SUNOS4 */
#if defined(__TURBOC__) && !defined(__BORLANDC__)
#define PC
#define POSTSCRIPT
#define HPGL
#endif

/* for use by all drivers */
#define sign(x) ((x) >= 0 ? 1 : -1)
#define abs(x) ((x) >= 0 ? (x) : -(x))
#ifndef max 
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#if defined(_WIN32)
int term_init;
#define popen(command,type) _popen(command,type)
#else
BOOLEAN term_init;                      /* true if terminal has been initialized */
#endif

extern FILE *outfile;
#if defined(_WIN32)
int term_init;
#else
extern BOOLEAN term_init;
#endif
extern int term;
extern float xsize,  ysize;

char *LineTableFile = NULL ;
LINE_TYPE_TABLE lineTypeTable ;


#if defined(__TURBOC__) && !defined(__BORLANDC__)
#endif
#ifdef PC
#endif
#ifdef VMS
#endif

/* This is needed because the unixplot library only writes to stdout. */
#ifdef UNIXPLOT
FILE save_stdout;
#endif
int unixplot=0;

#define NICE_LINE               0
#define POINT_TYPES             18

int do_dot(int x, int y, int number)
{
  register struct termentry *t = &term_tbl[term];

  if (number < 0) {          /* do dot */
    (*t->move)(x, y);
    (*t->vector)(x, y);
  }
  return 0;
}

int do_point(int x, int y, int number, double size)
{
  register int htic, vtic, size_exp, oldMode;
  register struct termentry *t = &term_tbl[term];
  oldMode = 0;
  if (number < 0) {          /* do dot */
    (*t->move)(x, y);
    (*t->vector)(x, y);
    return 0;
  }

  if (POINT_TYPES>18)
    bomb("programming error--POINT_TYPES>18 (do_point)",  NULL);
  if (size==0) {
    /* this is the old mode, where the point type and size
     * exponent are combined in the number variable
     */
    oldMode = 1;
    size_exp = (number&0x00f0)>>4;
    number &= 0x000f;
    number %= POINT_TYPES;
    if (t->flags&TERM_COLOR && t->flags&TERM_POSTSCRIPT) {
      CPS_symbol_color(size_exp>>2);
      size_exp = 2*(size_exp&3);
    }
    else
      (*term_tbl[term].linetype)(get_linetype());
    size = pow((double)1.1,  (double)(size_exp<0?0:size_exp));
  }
  else {
    number %= POINT_TYPES;
  }
  htic = (size*t->h_tic)/2;
  vtic = (size*t->v_tic)/2;    

  switch(number) {
  case 0: /* do diamond */ 
    (*t->move)(x-htic, y);
    (*t->vector)(x, y-vtic);
    (*t->vector)(x+htic, y);
    (*t->vector)(x, y+vtic);
    (*t->vector)(x-htic, y);
    break;
  case 1: /* do plus */ 
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y-vtic);
    (*t->vector)(x, y-vtic);
    (*t->vector)(x, y+vtic);
    break;
  case 2: /* do box */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x-htic, y-vtic);
    break;
  case 3: /* do X */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->move)(x-htic, y+vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x+htic, y-vtic);
    break;
  case 4: /* do triangle */ 
    (*t->move)(x, y+(4*vtic/3));
    (*t->vector)(x-(4*htic/3), y-(2*vtic/3));
    (*t->vector)(x+(4*htic/3), y-(2*vtic/3));
    (*t->vector)(x, y+(4*vtic/3));
    break;
  case 5: /* do asterisk */ 
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y-vtic);
    (*t->vector)(x, y-vtic);
    (*t->vector)(x, y+vtic);
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x-htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->move)(x-htic, y+vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x+htic, y-vtic);
    break;
  case 6: /* do inverted triangle */ 
    (*t->move)(x, y-(4*vtic/3));
    (*t->vector)(x+(4*htic/3), y+(2*vtic/3));
    (*t->vector)(x-(4*htic/3), y+(2*vtic/3));
    (*t->vector)(x, y-(4*vtic/3));
    break;
  case 7: /* do bow-tie */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->vector)(x+htic, y-vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x-htic, y-vtic);
    break;
  case 8: /* do hour-glass */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y-vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->vector)(x-htic, y-vtic);
    break;
  case 9: /* do circle */
    (*t->move)(x-htic*.414, y-vtic);
    (*t->vector)(x+htic*.414, y-vtic);
    (*t->vector)(x+htic, y-vtic*.414);
    (*t->vector)(x+htic, y+vtic*.414);
    (*t->vector)(x+htic*.414, y+vtic);
    (*t->vector)(x-htic*.414, y+vtic);
    (*t->vector)(x-htic, y+vtic*.414);
    (*t->vector)(x-htic, y-vtic*.414);
    (*t->vector)(x-htic*.414, y-vtic);
    break;
  case 10: /* do left triangle */ 
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->vector)(x-htic, y);
    break;
  case 11: /* do right triangle */ 
    (*t->move)(x+htic, y);
    (*t->vector)(x-htic, y-vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x+htic, y);
    break;
  case 12: /* do x box */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x-htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->move)(x-htic, y+vtic);
    (*t->vector)(x+htic, y-vtic);
    break;
  case 13: /* do x circle */ 
    (*t->move)(x-htic*.414, y-vtic);
    (*t->vector)(x+htic*.414, y-vtic);
    (*t->vector)(x+htic, y-vtic*.414);
    (*t->vector)(x+htic, y+vtic*.414);
    (*t->vector)(x+htic*.414, y+vtic);
    (*t->vector)(x-htic*.414, y+vtic);
    (*t->vector)(x-htic, y+vtic*.414);
    (*t->vector)(x-htic, y-vtic*.414);
    (*t->vector)(x-htic*.414, y-vtic);
    (*t->move)(x-htic*.66, y-vtic*.66);
    (*t->vector)(x+htic*.66, y+vtic*.66);
    (*t->move)(x-htic*.66, y+vtic*.66);
    (*t->vector)(x+htic*.66, y-vtic*.66);
    break;
  case 14: /* do plus box */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x-htic, y-vtic);
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y-vtic);
    (*t->vector)(x, y+vtic);
    break;
  case 15: /* do plus circle */ 
    (*t->move)(x-htic*.414, y-vtic);
    (*t->vector)(x+htic*.414, y-vtic);
    (*t->vector)(x+htic, y-vtic*.414);
    (*t->vector)(x+htic, y+vtic*.414);
    (*t->vector)(x+htic*.414, y+vtic);
    (*t->vector)(x-htic*.414, y+vtic);
    (*t->vector)(x-htic, y+vtic*.414);
    (*t->vector)(x-htic, y-vtic*.414);
    (*t->vector)(x-htic*.414, y-vtic);
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y+vtic);
    (*t->vector)(x, y-vtic);
    break;
  case 16: /* do star */ 
    (*t->move)(x, y+vtic);
    (*t->vector)(x+htic*.5877853, y-vtic*.809017);
    (*t->vector)(x-htic*.9510565, y+vtic*.309017);
    (*t->vector)(x+htic*.9510565, y+vtic*.309017);
    (*t->vector)(x-htic*.5877853, y-vtic*.809017);
    (*t->vector)(x, y+vtic);
    break;
  case 17: /* do star circle */ 
    (*t->move)(x, y+vtic);
    (*t->vector)(x+htic*.5877853, y-vtic*.809017);
    (*t->vector)(x-htic*.9510565, y+vtic*.309017);
    (*t->vector)(x+htic*.9510565, y+vtic*.309017);
    (*t->vector)(x-htic*.5877853, y-vtic*.809017);
    (*t->vector)(x, y+vtic);
    (*t->move)(x-htic*.414, y-vtic);
    (*t->vector)(x+htic*.414, y-vtic);
    (*t->vector)(x+htic, y-vtic*.414);
    (*t->vector)(x+htic, y+vtic*.414);
    (*t->vector)(x+htic*.414, y+vtic);
    (*t->vector)(x-htic*.414, y+vtic);
    (*t->vector)(x-htic, y+vtic*.414);
    (*t->vector)(x-htic, y-vtic*.414);
    (*t->vector)(x-htic*.414, y-vtic);
    break;
  }
  if (oldMode)
    (*term_tbl[term].linetype)(NICE_LINE);
  return 0; 
}



int do_point_fill(int x, int y, int number, double size)
{
  register int htic, vtic, size_exp, oldMode;
  register struct termentry *t = &term_tbl[term];
  register double ratio;
  register long i;
  oldMode = 0;
  if (number < 0) {          /* do dot */
    (*t->move)(x, y);
    (*t->vector)(x, y);
    return 0;
  }

  if (POINT_TYPES>18)
    bomb("programming error--POINT_TYPES>18 (do_point)",  NULL);
  if (size==0) {
    /* this is the old mode, where the point type and size
     * exponent are combined in the number variable
     */
    oldMode = 1;
    size_exp = (number&0x00f0)>>4;
    number &= 0x000f;
    number %= POINT_TYPES;
    if (t->flags&TERM_COLOR && t->flags&TERM_POSTSCRIPT) {
      CPS_symbol_color(size_exp>>2);
      size_exp = 2*(size_exp&3);
    }
    else
      (*term_tbl[term].linetype)(get_linetype());
    size = pow((double)1.1,  (double)(size_exp<0?0:size_exp));
  }
  else {
    number %= POINT_TYPES;
  }
  htic = (size*t->h_tic)/2;
  vtic = (size*t->v_tic)/2;    
	

  switch(number) {
  case 0: /* do diamond */
    ratio = htic*1.0/vtic; 
    for(i=0;i<vtic;i++) {
      (*t->move)(x-(int)(ratio*(vtic-i)),y+i);
      (*t->vector)(x+(int)(ratio*(vtic-i)),y+i);
      (*t->move)(x-(int)(ratio*(vtic-i)),y-i);
      (*t->vector)(x+(int)(ratio*(vtic-i)),y-i);
    }    
    break;
  case 1: /* do plus */ 
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y-vtic);
    (*t->vector)(x, y+vtic);
    break;
  case 2: /* do box */ 
    for(i=-vtic;i<=vtic;i++) {
      (*t->move)(x-htic,y+i);
      (*t->vector)(x+htic,y+i);
    }    
    break;
  case 3: /* do X */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->move)(x-htic, y+vtic);
    (*t->vector)(x+htic, y-vtic);
    break;
  case 4: /* do triangle */ 
    ratio = 2.0*htic/(3.0*vtic);
    for(i=4*vtic/3;i>=-(2*vtic/3);i--) {
      (*t->move)(x-(int)(ratio*(4*vtic/3-i)) ,y+i);
      (*t->vector)(x+(int)(ratio*(4*vtic/3-i)), y+i);
    }    
    break;
  case 5: /* do asterisk */ 
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y-vtic);
    (*t->vector)(x, y+vtic);
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->move)(x-htic, y+vtic);
    (*t->vector)(x+htic, y-vtic);
    break;
  case 6: /* do inverted triangle */ 
    ratio =  2.0*htic/(3.0*vtic);
    for(i=-4*vtic/3;i<=(2*vtic/3);i++) {
      (*t->move)(x-(int)(ratio*(4*vtic/3+i)),y+i);
      (*t->vector)(x+(int)(ratio*(4*vtic/3+i)),y+i);
    }    
    break;
  case 7: /* do bow-tie */ 
    ratio = 1.0*vtic/htic;
    for(i=0;i<=htic;i++) {
      (*t->move)(x-i,y-(int)(ratio*i));
      (*t->vector)(x-i,y+(int)(ratio*i));
      (*t->move)(x+i,y-(int)(ratio*i));
      (*t->vector)(x+i,y+(int)(ratio*i));
    }    
    break;
  case 8: /* do hour-glass */ 
    ratio = 1.0*htic/vtic;
    for(i=0;i<=vtic;i++) {
      (*t->move)(x-(int)(ratio*i),y-i);
      (*t->vector)(x+(int)(ratio*i),y-i);
      (*t->move)(x-(int)(ratio*i),y+i);
      (*t->vector)(x+(int)(ratio*i),y+i);
    }    
    break;
  case 9: /* do circle */
    for(i=-vtic;i<=vtic;i++) {
      ratio = acos((abs(i))*1.0/vtic);
      (*t->move)(x-(int)(sin(ratio)*htic),y+i);
      (*t->vector)(x+(int)(sin(ratio)*htic),y+i);
				    
    }    
    break;		
  case 10: /* do left triangle */ 
    ratio = htic*1.0/vtic; 
    for(i=0;i<vtic;i++) {
      (*t->move)(x-(int)(ratio*(vtic-2*i)),y+i);
      (*t->vector)(x+htic,y+i);
      (*t->move)(x-(int)(ratio*(vtic-2*i)),y-i);
      (*t->vector)(x+htic,y-i);
    }
    break;
  case 11: /* do right triangle */ 
    ratio = htic*1.0/vtic; 
    for(i=0;i<vtic;i++) {
      (*t->move)(x+(int)(ratio*(vtic-2*i)),y+i);
      (*t->vector)(x-htic,y+i);
      (*t->move)(x+(int)(ratio*(vtic-2*i)),y-i);
      (*t->vector)(x-htic,y-i);
    }
    break;
  case 12: /* do x box */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x-htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->move)(x-htic, y+vtic);
    (*t->vector)(x+htic, y-vtic);
    ratio = 1.0*vtic/htic;
    for(i=0;i<=htic;i++) {
      (*t->move)(x-i,y-(int)(ratio*i));
      (*t->vector)(x-i,y+(int)(ratio*i));
      (*t->move)(x+i,y-(int)(ratio*i));
      (*t->vector)(x+i,y+(int)(ratio*i));
    }    
    break;
  case 13: /* do x circle */ 
    (*t->move)(x-htic*.414, y-vtic);
    (*t->vector)(x+htic*.414, y-vtic);
    (*t->vector)(x+htic, y-vtic*.414);
    (*t->vector)(x+htic, y+vtic*.414);
    (*t->vector)(x+htic*.414, y+vtic);
    (*t->vector)(x-htic*.414, y+vtic);
    (*t->vector)(x-htic, y+vtic*.414);
    (*t->vector)(x-htic, y-vtic*.414);
    (*t->vector)(x-htic*.414, y-vtic);
    (*t->move)(x-htic*.66, y-vtic*.66);
    (*t->vector)(x+htic*.66, y+vtic*.66);
    (*t->move)(x-htic*.66, y+vtic*.66);
    (*t->vector)(x+htic*.66, y-vtic*.66);
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y+vtic);
    (*t->vector)(x, y-vtic);
    break;
  case 14: /* do plus box */ 
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->vector)(x-htic, y+vtic);
    (*t->vector)(x-htic, y-vtic);
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y-vtic);
    (*t->vector)(x, y+vtic);
    (*t->move)(x-htic, y-vtic);
    (*t->vector)(x+htic, y+vtic);
    (*t->move)(x+htic, y-vtic);
    (*t->vector)(x-htic, y+vtic);
    break;
  case 15: /* do plus circle */ 
    (*t->move)(x-htic*.414, y-vtic);
    (*t->vector)(x+htic*.414, y-vtic);
    (*t->vector)(x+htic, y-vtic*.414);
    (*t->vector)(x+htic, y+vtic*.414);
    (*t->vector)(x+htic*.414, y+vtic);
    (*t->vector)(x-htic*.414, y+vtic);
    (*t->vector)(x-htic, y+vtic*.414);
    (*t->vector)(x-htic, y-vtic*.414);
    (*t->vector)(x-htic*.414, y-vtic);
    (*t->move)(x-htic*.414*.5, y-vtic*.5);
    (*t->vector)(x+htic*.414*.5, y-vtic*.5);
    (*t->vector)(x+htic*.5, y-vtic*.414*.5);
    (*t->vector)(x+htic*.5, y+vtic*.414*.5);
    (*t->vector)(x+htic*.414*.5, y+vtic*.5);
    (*t->vector)(x-htic*.414*.5, y+vtic*.5);
    (*t->vector)(x-htic*.5, y+vtic*.414*.5);
    (*t->vector)(x-htic*.5, y-vtic*.414*.5);
    (*t->vector)(x-htic*.414*.5, y-vtic*.5);
    (*t->move)(x-htic, y);
    (*t->vector)(x+htic, y);
    (*t->move)(x, y+vtic);
    (*t->vector)(x, y-vtic);
    break;
  case 16: /* do star */ 
    for(ratio=1;ratio>.1;ratio -= .2) {
      (*t->move)(x, y+vtic*ratio);
      (*t->vector)(x+htic*.5877853*ratio, y-vtic*.809017*ratio);
      (*t->vector)(x-htic*.9510565*ratio, y+vtic*.309017*ratio);
      (*t->vector)(x+htic*.9510565*ratio, y+vtic*.309017*ratio);
      (*t->vector)(x-htic*.5877853*ratio, y-vtic*.809017*ratio);
      (*t->vector)(x, y+vtic*ratio);
    }
    break;
  case 17: /* do star circle */ 
    for(ratio=1;ratio>.1;ratio -= .2) {
      (*t->move)(x, y+vtic*ratio);
      (*t->vector)(x+htic*.5877853*ratio, y-vtic*.809017*ratio);
      (*t->vector)(x-htic*.9510565*ratio, y+vtic*.309017*ratio);
      (*t->vector)(x+htic*.9510565*ratio, y+vtic*.309017*ratio);
      (*t->vector)(x-htic*.5877853*ratio, y-vtic*.809017*ratio);
      (*t->vector)(x, y+vtic*ratio);
    }
    (*t->move)(x-htic*.414, y-vtic);
    (*t->vector)(x+htic*.414, y-vtic);
    (*t->vector)(x+htic, y-vtic*.414);
    (*t->vector)(x+htic, y+vtic*.414);
    (*t->vector)(x+htic*.414, y+vtic);
    (*t->vector)(x-htic*.414, y+vtic);
    (*t->vector)(x-htic, y+vtic*.414);
    (*t->vector)(x-htic, y-vtic*.414);
    (*t->vector)(x-htic*.414, y-vtic);
    break;				
  }
  if (oldMode)
    (*term_tbl[term].linetype)(NICE_LINE);
  return 0; 
}

/*
 * general point routine
 */
int line_and_point(int x, int y, int number)
{
  /* temporary(?) kludge to allow terminals with bad linetypes 
     to make nice marks */

  do_point(x, y, number, 0.0);
  return 0;
}

/* 
 * general arrow routine
 */
#define ROOT2 (1.41421)         /* sqrt of 2 */

int do_arrow(int sx, int sy, int ex, int ey)
{
  /* int sx, sy;                        start point */
  /* int ex,  ey;                       end point (point of arrowhead) */
  register struct termentry *t = &term_tbl[term];
  int len = (t->h_tic + t->v_tic)/2; /* arrowhead size = avg of tic sizes */
  /*extern double sqrt();*/

  /* draw the line for the arrow. That's easy. */
  (*t->move)(sx,  sy);
  (*t->vector)(ex,  ey);

  /* now draw the arrow head. */
  /* we put the arrowhead marks at 45 degrees to line */
  if (sx == ex) {
    /* vertical line,  special case */
    int delta = ((float)len / ROOT2 + 0.5);
    if (sy < ey)
      delta = -delta; /* up arrow goes the other way */
    (*t->move)(ex - delta,  ey + delta);
    (*t->vector)(ex, ey);
    (*t->vector)(ex + delta,  ey + delta);
  } else {
    int dx = sx - ex;
    int dy = sy - ey;
    double coeff = len / sqrt(2.0*((double)dx*(double)dx 
                                   + (double)dy*(double)dy));
    int x, y;                     /* one endpoint */

    x = (int)( ex + (dx + dy) * coeff );
    y = (int)( ey + (dy - dx) * coeff );
    (*t->move)(x, y);
    (*t->vector)(ex, ey);

    x = (int)( ex + (dx - dy) * coeff );
    y = (int)( ey + (dy + dx) * coeff );
    (*t->vector)(x, y);
  }
  return 0;
}


#ifdef PC                       /* all PC types */
#include "pc.trm"
#endif

/*
  all TEK types (TEK, BITGRAPH, KERMIT, VTTEK, SELANAR) are ifdef'd in tek.trm, 
  but most require various TEK routines.  Hence TEK must be defined for
  the others to compile.
*/
#ifdef BITGRAPH
# ifndef TEK
#  define TEK
# endif
#endif

#ifdef SELENAR
# ifndef TEK
#  define TEK
# endif
#endif

#ifdef KERMIT
# ifndef TEK
#  define TEK
# endif
#endif

#ifdef VTTEK
# ifndef TEK
#  define TEK
# endif
#endif

#ifdef T410X            /* Tektronix 4106,  4107,  4109 and 420x terminals */
#include "t410x.trm"
#endif

#ifdef TEK                      /* all TEK types,  TEK,  BBN,  SELANAR,  KERMIT,  VTTEK */
#include "tek.trm"
#endif

#ifdef EPSON    /* bit map types,  EPSON,  NEC,  PROPRINTER,  STAR Color */
#include "epson.trm"
#endif

#ifdef HPLJII           /* HP LaserJet II */
#include "hpljii.trm"
#endif

#ifdef FIG                              /* Fig 1.4FS Interactive graphics program */
#include "fig.trm"
#endif

#ifdef IMAGEN           /* IMAGEN printer */
#include "imagen.trm"
#endif

#ifdef EEPIC            /* EEPIC (LATEX) type */
#include "eepic.trm"
# ifndef LATEX
#  define LATEX
# endif
#endif

#ifdef EMTEX            /* EMTEX (LATEX for PC) type */
# ifndef LATEX
#  define LATEX
# endif
#endif

#ifdef LATEX            /* LATEX type */
#include "latex.trm"
#endif

#ifdef POSTSCRIPT       /* POSTSCRIPT type */
#include "post.trm"
#endif

#ifdef MIF              /* MIF type */
#include "mif.trm"
#endif

#ifdef PRESCRIBE        /* PRESCRIBE type */
#include "kyo.trm"
#endif

#ifdef HPLJET           /* hplaserjet */
#include "hpljet.trm"
#endif

#ifdef UNIXPC     /* unix-PC  ATT 7300 or 3b1 machine */
#include "unixpc.trm"
#endif /* UNIXPC */

#ifdef AED
#include "aed.trm"
#endif /* AED */

#ifdef CGI
#include "cgi.trm"
#endif /* CGI */

#ifdef HP2648
/* also works for HP2647 */
#include "hp2648.trm"
#endif /* HP2648 */

#ifdef HP26
#include "hp26.trm"
#endif /* HP26 */

#ifdef HP75
#ifndef HPGL
#define HPGL
#endif
#endif

/* HPGL - includes HP75 */
#ifdef HPGL
#include "hpgl.trm"
#endif /* HPGL */

/* Roland DXY800A plotter driver by Martin Yii,  eln557h@monu3.OZ 
   and Russell Lang,  rjl@monu1.cc.monash.oz */
#ifdef DXY800A
#include "dxy.trm"
#endif /* DXY800A */

#ifdef IRIS4D
#include "iris4d.trm"
#endif /* IRIS4D */

#ifdef QMS
#include "qms.trm"
#endif /* QMS */

#ifdef REGIS
#include "regis.trm"
#endif /* REGIS */

#ifdef SUN
#include "sun.trm"
#endif /* SUN */

#ifdef V384
#include "v384.trm"
#endif /* V384 */

#ifdef UNIXPLOT
#include "unixplot.trm"
#endif /* UNIXPLOT */

#ifdef X11
#include "x11.trm"
#endif /* X11 */

#ifdef USE_GD_LIBRARY
#ifdef PNG
#include "png.trm"
#endif
#endif

/* Dummy functions for unavailable features */

/* Change angle of text.  0 is horizontal left to right.
 * 1 is vertical bottom to top (90 deg rotate)  
 *
 * Change justification of text.  
 * modes are LEFT (flush left),  CENTRE (centred),  RIGHT (flush right)
 *
 * Change scale of plot.
 * Parameters are x, y scaling factors for this plot.
 * Some terminals (eg latex) need to do scaling themselves.
 */

int NULL_init(void) {return FALSE;}
int NULL_reset(void) {return FALSE;}
int NULL_text(void) {return FALSE;}
int NULL_scale(double xs, double ys) {return FALSE;}
int NULL_graphics(void) {return FALSE;}
int NULL_move(int x, int y) {return FALSE;}
int NULL_vector(int x, int y) {return FALSE;}
int NULL_linetype(int linetype) {return FALSE;}
int NULL_put_text(int x, int y, char *str) {return FALSE;}
int NULL_text_angle(int ang) {return FALSE;}
int NULL_justify_text(enum JUSTIFY mode) {return FALSE;}
int NULL_dot(int x, int y, int number) {return FALSE;}
int NULL_arrow(int sx, int sy, int ex, int ey) {return FALSE;}
int NULL_fillbox(int shade, int xl,  int xh, int yl, int yh) {return FALSE;}
int NULL_line_thickness(int line_thickness) {return FALSE;}
int NULL_color(unsigned short red, unsigned short green, unsigned short blue) {return FALSE;}
int NULL_add_color(unsigned short red, unsigned short green, unsigned short blue, short reset) {return FALSE;}
int NULL_sendCoordinates(void) {return FALSE;}
int NULL_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1) {return FALSE;}
int NULL_movie(void) {return FALSE;}
/*
 * term_tbl[] contains an entry for each terminal.  "unknown" must be the
 *   first,  since term is initialized to 0.
 */
struct termentry term_tbl[] = {
  {"unknown",  "Unknown terminal type - not a plotting device", 
   100,  100,  1,  1, 1,  1,  0, 
   NULL_init,  NULL_reset,  
   NULL_text,  NULL_scale,  NULL_graphics,  NULL_move,  NULL_vector,  
   NULL_linetype,  NULL_put_text,  NULL_text_angle,  
   NULL_justify_text,  NULL_dot,  NULL_arrow,  NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#ifdef PC
#if defined(__TURBOC__) && !defined(__BORLANDC__)

  , {"egalib",  "IBM PC/Clone with EGA graphics board", 
     EGALIB_XMAX,  EGALIB_YMAX,  EGALIB_VCHAR,  EGALIB_HCHAR, 
     EGALIB_VTIC,  EGALIB_HTIC,  
     TERM_COLOR|TERM_IBMCLONE, 
     EGALIB_init,  EGALIB_reset, 
     EGALIB_text,  NULL_scale,  EGALIB_graphics,  EGALIB_move,  EGALIB_vector, 
     EGALIB_linetype,  EGALIB_put_text,  EGALIB_text_angle,  
     EGALIB_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

  , {"vgalib",  "IBM PC/Clone with VGA graphics board", 
     VGA_XMAX,  VGA_YMAX,  VGA_VCHAR,  VGA_HCHAR, 
     VGA_VTIC,  VGA_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     VGA_init,  VGA_reset, 
     VGA_text,  NULL_scale,  VGA_graphics,  VGA_move,  VGA_vector, 
     VGA_linetype,  VGA_put_text,  VGA_text_angle,  
     VGA_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

  , {"vgamono",  "IBM PC/Clone with VGA Monochrome graphics board", 
     VGA_XMAX,  VGA_YMAX,  VGA_VCHAR,  VGA_HCHAR, 
     VGA_VTIC,  VGA_HTIC,  
     TERM_IBMCLONE, 
     VGA_init,  VGA_reset, 
     VGA_text,  NULL_scale,  VGA_graphics,  VGA_move,  VGA_vector, 
     VGAMONO_linetype,  VGA_put_text,  VGA_text_angle,  
     VGA_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

  , {"mcga",  "IBM PC/Clone with MCGA graphics board", 
     MCGA_XMAX,  MCGA_YMAX,  MCGA_VCHAR,  MCGA_HCHAR, 
     MCGA_VTIC,  MCGA_HTIC,  
     TERM_COLOR|TERM_IBMCLONE, 
     MCGA_init,  MCGA_reset, 
     MCGA_text,  NULL_scale,  MCGA_graphics,  MCGA_move,  MCGA_vector, 
     MCGA_linetype,  MCGA_put_text,  MCGA_text_angle,  
     MCGA_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

  , {"cga",  "IBM PC/Clone with CGA graphics board", 
     CGA_XMAX,  CGA_YMAX,  CGA_VCHAR,  CGA_HCHAR, 
     CGA_VTIC,  CGA_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     CGA_init,  CGA_reset, 
     CGA_text,  NULL_scale,  CGA_graphics,  CGA_move,  CGA_vector, 
     CGA_linetype,  CGA_put_text,  MCGA_text_angle,  
     CGA_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

  , {"hercules",  "IBM PC/Clone with Hercules graphics board", 
     HERC_XMAX,  HERC_YMAX,  HERC_VCHAR,  HERC_HCHAR, 
     HERC_VTIC,  HERC_HTIC,
     TERM_COLOR|TERM_IBMCLONE,
     HERC_init,  HERC_reset, 
     HERC_text,  NULL_scale,  HERC_graphics,  HERC_move,  HERC_vector, 
     HERC_linetype,  HERC_put_text,  MCGA_text_angle,  
     HERC_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#ifdef ATT6300
  , {"att",  "IBM PC/Clone with AT&T 6300 graphics board", 
     ATT_XMAX,  ATT_YMAX,  ATT_VCHAR,  ATT_HCHAR, 
     ATT_VTIC,  ATT_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     ATT_init,  ATT_reset, 
     ATT_text,  NULL_scale,  ATT_graphics,  ATT_move,  ATT_vector, 
     ATT_linetype,  ATT_put_text,  ATT_text_angle,  
     ATT_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif
#else                                   /* TURBO */

  , {"cga",  "IBM PC/Clone with CGA graphics board", 
     CGA_XMAX,  CGA_YMAX,  CGA_VCHAR,  CGA_HCHAR, 
     CGA_VTIC,  CGA_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     CGA_init,  CGA_reset, 
     CGA_text,  NULL_scale,  CGA_graphics,  CGA_move,  CGA_vector, 
     CGA_linetype,  CGA_put_text,  CGA_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

  , {"egabios",  "IBM PC/Clone with EGA graphics board (BIOS)", 
     EGA_XMAX,  EGA_YMAX,  EGA_VCHAR,  EGA_HCHAR, 
     EGA_VTIC,  EGA_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     EGA_init,  EGA_reset, 
     EGA_text,  NULL_scale,  EGA_graphics,  EGA_move,  EGA_vector, 
     EGA_linetype,  EGA_put_text,  EGA_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

  , {"vgabios",  "IBM PC/Clone with VGA graphics board (BIOS)", 
     VGA_XMAX,  VGA_YMAX,  VGA_VCHAR,  VGA_HCHAR, 
     VGA_VTIC,  VGA_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     VGA_init,  VGA_reset, 
     VGA_text,  NULL_scale,  VGA_graphics,  VGA_move,  VGA_vector, 
     VGA_linetype,  VGA_put_text,  VGA_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

#ifdef EGALIB
  , {"egalib",  "IBM PC/Clone with EGA graphics board (LIB)", 
     EGALIB_XMAX,  EGALIB_YMAX,  EGALIB_VCHAR,  EGALIB_HCHAR, 
     EGALIB_VTIC,  EGALIB_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     EGALIB_init,  EGALIB_reset, 
     EGALIB_text,  NULL_scale,  EGALIB_graphics,  EGALIB_move,  EGALIB_vector, 
     EGALIB_linetype,  EGALIB_put_text,  NULL_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef HERCULES
  , {"hercules",  "IBM PC/Clone with Hercules graphics board", 
     HERC_XMAX,  HERC_YMAX,  HERC_VCHAR,  HERC_HCHAR, 
     HERC_VTIC,  HERC_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     HERC_init,  HERC_reset, 
     HERC_text,  NULL_scale,  HERC_graphics,  HERC_move,  HERC_vector, 
     HERC_linetype,  HERC_put_text,  HERC_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif                                  /* HERCULES */

#ifdef ATT6300
  , {"att",  "AT&T PC/6300 graphics", 
     ATT_XMAX,  ATT_YMAX,  ATT_VCHAR,  ATT_HCHAR, 
     ATT_VTIC,  ATT_HTIC,  
     TERM_COLOR|TERM_IBMCLONE,
     ATT_init,  ATT_reset, 
     ATT_text,  NULL_scale,  ATT_graphics,  ATT_move,  ATT_vector, 
     ATT_linetype,  ATT_put_text,  ATT_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef CORONA
  , {"corona325",  "Corona graphics ???", 
     COR_XMAX,  COR_YMAX,  COR_VCHAR,  COR_HCHAR, 
     COR_VTIC,  COR_HTIC,  
     0,
     COR_init,  COR_reset, 
     COR_text,  NULL_scale,  COR_graphics,  COR_move,  COR_vector, 
     COR_linetype,  COR_put_text,  COR_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif                                  /* CORONA */
#endif                                  /* TURBO */
#endif                                  /* PC */

#ifdef AED
  , {"aed512",  "AED 512 Terminal", 
     AED5_XMAX,  AED_YMAX,  AED_VCHAR,  AED_HCHAR, 
     AED_VTIC,  AED_HTIC,
     0,
     AED_init,  AED_reset, AED_text,  NULL_scale,  
     AED_graphics,  AED_move,  AED_vector,  
     AED_linetype,  AED_put_text,  NULL_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"aed767",  "AED 767 Terminal", 
     AED_XMAX,  AED_YMAX,  AED_VCHAR,  AED_HCHAR, 
     AED_VTIC,  AED_HTIC,  
     0,
     AED_init,  AED_reset,  AED_text,  NULL_scale,  AED_graphics,  AED_move,  AED_vector,  
     AED_linetype,  AED_put_text,  NULL_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef BITGRAPH
  , {"bitgraph",  "BBN Bitgraph Terminal", 
     BG_XMAX, BG_YMAX, BG_VCHAR,  BG_HCHAR,  
     BG_VTIC,  BG_HTIC,
     0,
     BG_init,  BG_reset,  BG_text,  NULL_scale,  
     BG_graphics,  TEK40move,  TEK40vector, 
     TEK40linetype,  BG_put_text,  
     NULL_text_angle, NULL_justify_text,  line_and_point,  
     do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef CGI
  , {"cgi",  "SCO CGI drivers (requires CGIDISP or CGIPRNT env variable)", 
     CGI_XMAX,  CGI_YMAX,  0,  0,  
     CGI_VTIC,  0,  
     0,
     CGI_init,  CGI_reset,  
     CGI_text,  NULL_scale,  CGI_graphics,  CGI_move,  CGI_vector,  
     CGI_linetype,  CGI_put_text,  CGI_text_angle,  
     CGI_justify_text,  CGI_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}

  , {"hcgi",  "SCO CGI drivers (hardcopy,  requires CGIPRNT env variable)", 
     CGI_XMAX,  CGI_YMAX,  0,  0,  
     CGI_VTIC,  0,  
     0,
     HCGI_init,  CGI_reset,  
     CGI_text,  NULL_scale,  CGI_graphics,  CGI_move,  CGI_vector,  
     CGI_linetype,  CGI_put_text,  CGI_text_angle,  
     CGI_justify_text,  CGI_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef DXY800A
  , {"dxy800a",  "Roland DXY800A plotter", 
     DXY_XMAX,  DXY_YMAX,  DXY_VCHAR,  DXY_HCHAR, 
     DXY_VTIC,  DXY_HTIC,
     TERM_HARDCOPY|TERM_NOPROMPT,
     DXY_init,  DXY_reset, 
     DXY_text,  NULL_scale,  DXY_graphics,  DXY_move,  DXY_vector, 
     DXY_linetype,  DXY_put_text,  DXY_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef EEPIC
  , {"eepic",  "EEPIC -- extended LaTeX picture environment", 
     EEPIC_XMAX,  EEPIC_YMAX,  EEPIC_VCHAR,  EEPIC_HCHAR,  
     EEPIC_VTIC,  EEPIC_HTIC,  
     TERM_HARDCOPY|TERM_NOPROMPT,
     EEPIC_init,  EEPIC_reset,  
     EEPIC_text,  EEPIC_scale,  EEPIC_graphics,  EEPIC_move,  EEPIC_vector,  
     EEPIC_linetype,  EEPIC_put_text,  EEPIC_text_angle,  
     EEPIC_justify_text,  EEPIC_point,  EEPIC_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef EMTEX
   
  , {"emtex",  "LATEX picture environment with emTeX specials", 
     LATEX_XMAX,  LATEX_YMAX,  LATEX_VCHAR,  LATEX_HCHAR,  
     LATEX_VTIC,  LATEX_HTIC,  
     TERM_HARDCOPY|TERM_NOPROMPT,
     EMTEX_init,  EMTEX_reset,  
     EMTEX_text,  LATEX_scale,  LATEX_graphics,  LATEX_move,  LATEX_vector,  
     LATEX_linetype,  LATEX_put_text,  LATEX_text_angle,  
     LATEX_justify_text,  LATEX_point,  LATEX_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef EPS60
  , {"epson_60dpi",  "Epson-style 60-dot per inch printers", 
     EPS60XMAX,  EPS60YMAX,  EPSONVCHAR,  EPSONHCHAR, 
     EPSONVTIC,  EPSONHTIC,  
     TERM_HARDCOPY|TERM_NOPROMPT,
     EPSONinit,  EPSONreset, 
     EPS60text,  NULL_scale,  EPS60graphics,  EPSONmove,  EPSONvector, 
     EPSONlinetype,  EPSONput_text,  EPSON_text_angle, 
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef EPSON
  , {"epson_lx800",  "Epson LX-800,  Star NL-10,  NX-1000,  PROPRINTER ...", 
     EPSONXMAX,  EPSONYMAX,  EPSONVCHAR,  EPSONHCHAR,  
     EPSONVTIC,  EPSONHTIC,  
     TERM_HARDCOPY|TERM_NOPROMPT,
     EPSONinit,  EPSONreset,  
     EPSONtext,  NULL_scale,  EPSONgraphics,  EPSONmove,  EPSONvector,  
     EPSONlinetype,  EPSONput_text,  EPSON_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef FIG
  , {"fig",  "FIG graphics language: SunView or X graphics editor", 
     FIG_XMAX,  FIG_YMAX,  FIG_VCHAR,  FIG_HCHAR,  
     FIG_VTIC,  FIG_HTIC,  
     0,
     FIG_init,  FIG_reset,  
     FIG_text,  NULL_scale,  FIG_graphics,  FIG_move,  FIG_vector,  
     FIG_linetype,  FIG_put_text,  FIG_text_angle,  
     FIG_justify_text,  do_dot,  FIG_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef HP26
  , {"hp2623A",  "HP2623A and maybe others", 
     HP26_XMAX,  HP26_YMAX,  HP26_VCHAR,  HP26_HCHAR, 
     HP26_VTIC,  HP26_HTIC,
     0,
     HP26_init,  HP26_reset, 
     HP26_text,  NULL_scale,  HP26_graphics,  HP26_move,  HP26_vector, 
     HP26_linetype,  HP26_put_text,  NULL_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef HP2648
  , {"hp2648",  "HP2648 and HP2647", 
     HP2648XMAX,  HP2648YMAX,  HP2648VCHAR,  HP2648HCHAR,  
     HP2648VTIC,  HP2648HTIC,
     0,
     HP2648init,  HP2648reset,  
     HP2648text,  NULL_scale,  HP2648graphics,  HP2648move,  HP2648vector,  
     HP2648linetype,  HP2648put_text,  HP2648_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef HP75
  , {"hp7580B",  "HP7580,  and probably other HPs (4 pens)", 
     HPGL_XMAX,  HPGL_YMAX,  HPGL_VCHAR,  HPGL_HCHAR, 
     HPGL_VTIC,  HPGL_HTIC,
     TERM_COLOR|TERM_HARDCOPY,
     HPGL_init,  HPGL_reset, 
     HPGL_text,  NULL_scale,  HPGL_graphics,  HPGL_move,  HPGL_vector, 
     HP75_linetype,  HPGL_put_text,  HPGL_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef HPGL
  , {"hpgl",  "HP7475 and (hopefully) lots of others (6 pens)", 
     HPGL_XMAX,  HPGL_YMAX,  HPGL_VCHAR,  HPGL_HCHAR, 
     HPGL_VTIC,  HPGL_HTIC,
     TERM_COLOR|TERM_HARDCOPY,
     HPGL_init,  HPGL_reset, 
     HPGL_text,  NULL_scale,  HPGL_graphics,  HPGL_move,  HPGL_vector, 
     HPGL_linetype,  HPGL_put_text,  HPGL_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef HPLJII
  , {"hpljii_300ppi",  "HP Laserjet series II with 300 pixels per inch", 
     HPLJII_300PPI_XMAX,  HPLJII_300PPI_YMAX,  HPLJII_300PPI_VCHAR, 
     HPLJII_300PPI_HCHAR,  HPLJII_300PPI_VTIC,  HPLJII_300PPI_HTIC, 
     TERM_HARDCOPY|TERM_NOPROMPT,
     HPLJIIinit,  HPLJIIreset,  HPLJIItext,  NULL_scale, 
     HPLJII_300PPIgraphics,  HPLJIImove,  HPLJIIvector,  HPLJIIlinetype, 
     HPLJIIput_text,  HPLJIItext_angle,   NULL_justify_text,  line_and_point, 
     do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"hpljii_150ppi",  "HP Laserjet series II with 150 pixels per inch", 
     HPLJII_150PPI_XMAX,  HPLJII_150PPI_YMAX,  HPLJII_150PPI_VCHAR, 
     HPLJII_150PPI_HCHAR,  HPLJII_150PPI_VTIC,  HPLJII_150PPI_HTIC, 
     TERM_HARDCOPY|TERM_NOPROMPT,
     HPLJIIinit,  HPLJIIreset,  HPLJIItext,  NULL_scale, 
     HPLJII_150PPIgraphics,  HPLJIImove,  HPLJIIvector,  HPLJIIlinetype, 
     HPLJIIput_text,  HPLJIItext_angle,  NULL_justify_text,  line_and_point, 
     do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"hpljii_100ppi",  "HP Laserjet series II with 100 pixels per inch", 
     HPLJII_100PPI_XMAX,  HPLJII_100PPI_YMAX,  HPLJII_100PPI_VCHAR, 
     HPLJII_100PPI_HCHAR,  HPLJII_100PPI_VTIC,  HPLJII_100PPI_HTIC, 
     TERM_HARDCOPY|TERM_NOPROMPT,
     HPLJIIinit,  HPLJIIreset,  HPLJIItext,  NULL_scale, 
     HPLJII_100PPIgraphics,  HPLJIImove,  HPLJIIvector,  HPLJIIlinetype, 
     HPLJIIput_text,  HPLJIItext_angle,  NULL_justify_text,  line_and_point, 
     do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"hpljii_75ppi",  "HP Laserjet series II with 75 pixels per inch", 
     HPLJII_75PPI_XMAX,  HPLJII_75PPI_YMAX,  HPLJII_75PPI_VCHAR, 
     HPLJII_75PPI_HCHAR,  HPLJII_75PPI_VTIC,  HPLJII_75PPI_HTIC, 
     TERM_HARDCOPY|TERM_NOPROMPT,
     HPLJIIinit,  HPLJIIreset,  HPLJIItext,  NULL_scale, 
     HPLJII_75PPIgraphics,  HPLJIImove,  HPLJIIvector,  HPLJIIlinetype, 
     HPLJIIput_text,  HPLJIItext_angle,  NULL_justify_text,  line_and_point, 
     do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef IMAGEN
  , {"imagen",  "Imagen laser printer", 
     IMAGEN_XMAX,  IMAGEN_YMAX,  IMAGEN_VCHAR,  IMAGEN_HCHAR,  
     IMAGEN_VTIC,  IMAGEN_HTIC,
     TERM_HARDCOPY|TERM_NOPROMPT,
     IMAGEN_init,  IMAGEN_reset,  
     IMAGEN_text,  NULL_scale,  IMAGEN_graphics,  IMAGEN_move,  
     IMAGEN_vector,  IMAGEN_linetype,  IMAGEN_put_text,  IMAGEN_text_angle, 
     IMAGEN_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef IRIS4D
  , {"iris4d",  "Silicon Graphics IRIS 4D Series Computer", 
     IRIS4D_XMAX,  IRIS4D_YMAX,  IRIS4D_VCHAR,  IRIS4D_HCHAR,  
     IRIS4D_VTIC,  IRIS4D_HTIC,  
     0,
     IRIS4D_init,  IRIS4D_reset,  
     IRIS4D_text,  NULL_scale,  IRIS4D_graphics,  IRIS4D_move,  IRIS4D_vector, 
     IRIS4D_linetype,  IRIS4D_put_text,  NULL_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef KERMIT
  , {"kc_tek40xx",  "Kermit-MS tek40xx terminal emulator - color", 
     TEK40XMAX, TEK40YMAX, TEK40VCHAR,  KTEK40HCHAR,  
     TEK40VTIC,  TEK40HTIC,  
     TERM_COLOR,
     TEK40init,  KTEK40reset,  
     KTEK40Ctext,  NULL_scale,  KTEK40graphics,  TEK40move,  TEK40vector,  
     KTEK40Clinetype,  TEK40put_text,  NULL_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"km_tek40xx",  "Kermit-MS tek40xx terminal emulator - monochrome", 
     TEK40XMAX, TEK40YMAX, TEK40VCHAR,  KTEK40HCHAR,  
     TEK40VTIC,  TEK40HTIC,
     0,
     TEK40init,  KTEK40reset,  
     TEK40text,  NULL_scale,  KTEK40graphics,  TEK40move,  TEK40vector,  
     KTEK40Mlinetype,  TEK40put_text,  NULL_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef LATEX
  , {"latex",  "LaTeX picture environment", 
     LATEX_XMAX,  LATEX_YMAX,  LATEX_VCHAR,  LATEX_HCHAR,  
     LATEX_VTIC,  LATEX_HTIC,  
     TERM_HARDCOPY|TERM_NOPROMPT,
     LATEX_init,  LATEX_reset,  
     LATEX_text,  LATEX_scale,  LATEX_graphics,  LATEX_move,  LATEX_vector,  
     LATEX_linetype,  LATEX_put_text,  LATEX_text_angle,  
     LATEX_justify_text,  LATEX_point,  LATEX_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef NEC
  , {"nec_cp6m",  "NEC printer CP6,  Epson LQ-800 Monochrome", 
     NECXMAX,  NECYMAX,  NECVCHAR,  NECHCHAR,  
     NECVTIC,  NECHTIC,  
     TERM_HARDCOPY,
     NECinit,  NECreset,  
     NECtext,  NULL_scale,  NECMgraphics,  NECmove,  NECvector,  
     NECMlinetype,  NECput_text,  NEC_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"nec_cp6c",  "NEC printer CP6 Color", 
     NECXMAX,  NECYMAX,  NECVCHAR,  NECHCHAR,  
     NECVTIC,  NECHTIC,  
     TERM_HARDCOPY|TERM_COLOR,
     NECinit,  NECreset,  
     NECtext,  NULL_scale,  NECCgraphics,  NECmove,  NECvector,  
     NECClinetype,  NECput_text,  NEC_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"nec_cp6d",  "NEC printer CP6,  Epson LQ-800 Draft monochrome", 
     NECXMAX,  NECYMAX,  NECVCHAR,  NECHCHAR,  
     NECVTIC,  NECHTIC,  
     TERM_HARDCOPY,
     NECinit,  NECreset,  
     NECdraft_text,  NULL_scale,  NECMgraphics,  NECmove,  NECvector,  
     NECMlinetype,  NECput_text,  NEC_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef POSTSCRIPT
  , {"postscript",  "Postscript graphics language", 
     PS_XMAX,  PS_YMAX,  PS_VCHAR1,  PS_HCHAR1,  
     PS_VTIC,  PS_HTIC,  
     TERM_HARDCOPY|TERM_POLYFILL|TERM_POSTSCRIPT|TERM_NOPROMPT,
     PS1_init,  PS_reset,  
     PS_text,  NULL_scale,  PS_graphics,  PS_move,  PS_vector,  
     PS_linetype,  PS_put_text,  PS_text_angle,  
     PS_justify_text,  PS_dot,  do_arrow, PS_fill_box, PS_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"rpostscript",  "Postscript graphics language,  rotated output", 
     PS_XMAX,  PS_YMAX,  PS_VCHAR1,  PS_HCHAR1,  
     PS_VTIC*1.65/2.9,  PS_HTIC,  
     TERM_HARDCOPY|TERM_POLYFILL|TERM_POSTSCRIPT|TERM_NOPROMPT,
     PS1_init_rot,  PS_reset,  
     PS_text,  NULL_scale,  PS_graphics,  PS_move,  PS_vector,  
     PS_linetype,  PS_put_text,  PS_text_angle,  
     PS_justify_text,  PS_dot,  do_arrow, PS_fill_box, PS_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"eps",  "Encapsulated Postscript graphics language", 
     PS_XMAX,  PS_YMAX,  PS_VCHAR1,  PS_HCHAR1,  
     PS_VTIC,  PS_HTIC,  
     TERM_HARDCOPY|TERM_POLYFILL|TERM_POSTSCRIPT|TERM_NOPROMPT,
     EPSF1_init,  EPSF_reset,  
     EPSF_text,  NULL_scale,  EPSF_graphics,  PS_move,  PS_vector,  
     PS_linetype,  PS_put_text,  PS_text_angle,  
     PS_justify_text,  PS_dot,  do_arrow, PS_fill_box, PS_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"cpostscript",  "Color Postscript graphics language", 
     PS_XMAX,  PS_YMAX,  PS_VCHAR1,  PS_HCHAR1,  
     PS_VTIC,  PS_HTIC,  
     TERM_HARDCOPY|TERM_POLYFILL|TERM_POSTSCRIPT|TERM_NOPROMPT|TERM_COLOR,
     CPS1_init,  PS_reset,  
     PS_text,  NULL_scale,  CPS_graphics,  PS_move,  PS_vector,  
     CPS_linetype,  PS_put_text,  PS_text_angle,  
     PS_justify_text,  PS_dot,  do_arrow, CPS_fill_box, PS_line_thickness, CPS_color, CPS_add_color,NULL_sendCoordinates,CPS_spectral,NULL_movie}
  , {"crpostscript",  "Color Postscript graphics language,  rotated output", 
     PS_XMAX,  PS_YMAX,  PS_VCHAR1,  PS_HCHAR1,  
     PS_VTIC*1.65/2.9,  PS_HTIC,
     TERM_HARDCOPY|TERM_POLYFILL|TERM_POSTSCRIPT|TERM_NOPROMPT|TERM_COLOR,
     CPS1_init_rot,  PS_reset,  
     PS_text,  NULL_scale,  CPS_graphics,  PS_move,  PS_vector,  
     CPS_linetype,  PS_put_text,  PS_text_angle,  
     PS_justify_text,  PS_dot,  do_arrow, CPS_fill_box, PS_line_thickness, CPS_color, CPS_add_color,NULL_sendCoordinates,CPS_spectral,NULL_movie}
  , {"ceps",  "Color Encapsulated Postscript graphics language\n\tPostscript devices accept dashes, linetypetable, onwhite and \n\tonblack device arguments", 
     PS_XMAX,  PS_YMAX,  PS_VCHAR1,  PS_HCHAR1,  
     PS_VTIC,  PS_HTIC,  
     TERM_HARDCOPY|TERM_POLYFILL|TERM_POSTSCRIPT|TERM_NOPROMPT,
     EPSF1_init,  EPSF_reset,  
     EPSF_text,  NULL_scale,  CEPSF_graphics,  PS_move,  PS_vector,  
     CPS_linetype,  PS_put_text,  PS_text_angle,  
     PS_justify_text,  PS_dot,  do_arrow, CPS_fill_box, PS_line_thickness, CPS_color, CPS_add_color,NULL_sendCoordinates,CPS_spectral,NULL_movie}
#endif

#ifdef MIF
  , {"mif",  "Maker Interchange Format for FrameMaker", 
     MIF_XMAX,  MIF_YMAX,  MIF_VCHAR1,  MIF_HCHAR1,  
     MIF_VTIC,  MIF_HTIC,  
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT,
     MIF_init,  MIF_reset,  
     MIF_text,  NULL_scale,  MIF_graphics,  MIF_move,  MIF_vector,  
     MIF_linetype,  MIF_put_text,  MIF_text_angle,  
     MIF_justify_text,  MIF_dot,  do_arrow, MIF_fill_box, MIF_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"cmif",  "Maker Interchange Format for FrameMaker, color version", 
     MIF_XMAX,  MIF_YMAX,  MIF_VCHAR1,  MIF_HCHAR1,  
     MIF_VTIC,  MIF_HTIC,  
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT,
     CMIF_init,  MIF_reset,  
     MIF_text,  NULL_scale,  MIF_graphics,  MIF_move,  MIF_vector,  
     MIF_linetype,  MIF_put_text,  MIF_text_angle,  
     MIF_justify_text,  MIF_dot,  do_arrow, MIF_fill_box, MIF_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef PRESCRIBE
  , {"prescribe",  "Prescribe - for the Kyocera Laser Printer", 
     PRE_XMAX,  PRE_YMAX,  PRE_VCHAR,  PRE_HCHAR,  
     PRE_VTIC,  PRE_HTIC,
     TERM_HARDCOPY|TERM_NOPROMPT,
     PRE_init,  PRE_reset,  
     PRE_text,  NULL_scale,  PRE_graphics,  PRE_move,  PRE_vector,  
     PRE_linetype,  PRE_put_text,  NULL_text_angle,  
     PRE_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"kyo",  "Kyocera Laser Printer with Courier font", 
     PRE_XMAX,  PRE_YMAX,  KYO_VCHAR,  KYO_HCHAR,  
     PRE_VTIC,  PRE_HTIC,
     TERM_HARDCOPY|TERM_NOPROMPT,
     KYO_init,  PRE_reset,  
     PRE_text,  NULL_scale,  PRE_graphics,  PRE_move,  PRE_vector,  
     PRE_linetype,  PRE_put_text,  NULL_text_angle,  
     PRE_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif /* PRESCRIBE */

#ifdef QMS
  , {"qms",  "QMS/QUIC Laser printer (also Talaris 1200 and others)", 
     QMS_XMAX, QMS_YMAX,  QMS_VCHAR,  QMS_HCHAR,  
     QMS_VTIC,  QMS_HTIC,
     TERM_HARDCOPY|TERM_NOPROMPT,
     QMS_init, QMS_reset,  
     QMS_text,  NULL_scale,  QMS_graphics,  QMS_move,  QMS_vector, 
     QMS_linetype, QMS_put_text,  NULL_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef REGIS
  , {"regis",  "REGIS graphics language", 
     REGISXMAX,  REGISYMAX,  REGISVCHAR,  REGISHCHAR,  
     REGISVTIC,  REGISHTIC,  
     TERM_POLYFILL|TERM_FLUSHSTDOUT,
     REGISinit,  REGISreset,  
     REGIStext,  NULL_scale,  REGISgraphics,  REGISmove,  REGISvector, 
     REGISlinetype,  REGISput_text,  REGIStext_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, REGIS_fill_box, NULL_line_thickness, NULL_color,NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif


#ifdef SELANAR
  , {"selanar",  "Selanar", 
     TEK40XMAX,  TEK40YMAX,  TEK40VCHAR,  TEK40HCHAR,  
     TEK40VTIC,  TEK40HTIC,  
     0,
     SEL_init,  SEL_reset,  
     SEL_text,  NULL_scale,  SEL_graphics,  TEK40move,  TEK40vector,  
     TEK40linetype,  TEK40put_text,  NULL_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef STARC
  , {"starc",  "Star Color Printer", 
     STARCXMAX,  STARCYMAX,  STARCVCHAR,  STARCHCHAR,  
     STARCVTIC,  STARCHTIC,
     TERM_HARDCOPY|TERM_COLOR,
     STARCinit,  STARCreset,  
     STARCtext,  NULL_scale,  STARCgraphics,  STARCmove,  STARCvector,  
     STARClinetype,  STARCput_text,  STARC_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef SUN
  , {"sun",  "SunView window system", 
     SUN_XMAX,  SUN_YMAX,  SUN_VCHAR,  SUN_HCHAR,  
     SUN_VTIC,  SUN_HTIC, 
     TERM_WINDOWS,
     SUN_init,  SUN_reset,  
     SUN_text,  NULL_scale,  SUN_graphics,  SUN_move,  SUN_vector, 
     SUN_linetype,  SUN_put_text,  NULL_text_angle,  
     SUN_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef TANDY60
  , {"tandy_60dpi",  "Tandy DMP-130 series 60-dot per inch graphics", 
     EPS60XMAX,  EPS60YMAX,  EPSONVCHAR,  EPSONHCHAR, 
     EPSONVTIC,  EPSONHTIC,  
     TERM_HARDCOPY,
     EPSONinit,  EPSONreset, 
     TANDY60text,  NULL_scale,  EPS60graphics,  EPSONmove,  EPSONvector, 
     EPSONlinetype,  EPSONput_text,  EPSON_text_angle, 
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef T410X
  , {"tek410x",  "Tektronix 4106,  4107,  4109 and 420X terminals", 
     T410XXMAX,  T410XYMAX,  T410XVCHAR,  T410XHCHAR,  
     T410XVTIC,  T410XHTIC,
     0,
     T410X_init,  T410X_reset,  
     T410X_text,  NULL_scale,  T410X_graphics,  T410X_move,  T410X_vector,  
     T410X_linetype,  T410X_put_text,  T410X_text_angle,  
     NULL_justify_text,  T410X_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef TEK
  , {"tek40xx",  "Tektronix 4010 and others; most TEK emulators", 
     TEK40XMAX,  TEK40YMAX,  TEK40VCHAR,  TEK40HCHAR,  
     TEK40VTIC,  TEK40HTIC,
     0,
     TEK40init,  TEK40reset,  
     TEK40text,  NULL_scale,  TEK40graphics,  TEK40move,  TEK40vector,  
     TEK40linetype,  TEK40put_text,  NULL_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef UNIXPLOT
  , {"unixplot",  "Unix plotting standard (see plot(1))", 
     UP_XMAX,  UP_YMAX,  UP_VCHAR,  UP_HCHAR,  
     UP_VTIC,  UP_HTIC,
     TERM_NOPROMPT,
     UP_init,  UP_reset,  
     UP_text,  NULL_scale,  UP_graphics,  UP_move,  UP_vector,  
     UP_linetype,  UP_put_text,  NULL_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif
        
#ifdef UNIXPC
  , {"unixpc",  "AT&T 3b1 or AT&T 7300 Unix PC", 
     uPC_XMAX,  uPC_YMAX,  uPC_VCHAR,  uPC_HCHAR,  
     uPC_VTIC,  uPC_HTIC,
     0,
     uPC_init,  uPC_reset,  
     uPC_text,  NULL_scale,  uPC_graphics,  uPC_move,  uPC_vector, 
     uPC_linetype,  uPC_put_text,  uPC_text_angle,  
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef V384
  , {"vx384",  "Vectrix 384 and Tandy color printer", 
     V384_XMAX,  V384_YMAX,  V384_VCHAR,  V384_HCHAR,  
     V384_VTIC,  V384_HTIC,
     TERM_HARDCOPY|TERM_COLOR|TERM_NOPROMPT,
     V384_init,  V384_reset,  
     V384_text,  NULL_scale,  V384_graphics,  V384_move,  V384_vector,  
     V384_linetype,  V384_put_text,  NULL_text_angle,  
     NULL_justify_text,  do_dot,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef VTTEK
  , {"vttek",  "VT-like tek40xx terminal emulator", 
     TEK40XMAX, TEK40YMAX, TEK40VCHAR,  TEK40HCHAR, 
     TEK40VTIC,  TEK40HTIC,
     0,
     VTTEK40init,  VTTEK40reset, 
     TEK40text,  NULL_scale,  TEK40graphics,  TEK40move,  TEK40vector, 
     VTTEK40linetype,  VTTEK40put_text,  NULL_text_angle, 
     NULL_justify_text,  line_and_point,  do_arrow, NULL_fillbox, NULL_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
#endif

#ifdef X11
  , {"x11",  "X11 Window System", 
     X11_XMAX,  X11_YMAX,  X11_VCHAR,  X11_HCHAR,  
     X11_VTIC,  X11_HTIC,  
     TERM_WINDOWS|TERM_XWINDOWS|TERM_FLUSHSTDOUT,
     X11_init,  X11_reset,  
     X11_text,  NULL_scale,  X11_graphics,  X11_move,  X11_vector,  
     X11_linetype,  X11_put_text,  NULL_text_angle,  
     X11_justify_text,  X11_dot,  do_arrow, NULL_fillbox, X11_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"gx11",  "X11 Window System--grey scale", 
     X11_XMAX,  X11_YMAX,  X11_VCHAR,  X11_HCHAR,  
     X11_VTIC,  X11_HTIC,
     TERM_WINDOWS|TERM_XWINDOWS|TERM_FLUSHSTDOUT,
     X11g_init,  X11_reset,  
     X11_text,  NULL_scale,  X11_graphics,  X11_move,  X11_vector,  
     X11_linetype,  X11_put_text,  NULL_text_angle,  
     X11_justify_text,  X11_dot,  do_arrow, NULL_fillbox, X11_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"cx11",  "X11 Window System--color", 
     X11_XMAX,  X11_YMAX,  X11_VCHAR,  X11_HCHAR,  
     X11_VTIC,  X11_HTIC,
     TERM_WINDOWS|TERM_XWINDOWS|TERM_COLOR|TERM_FLUSHSTDOUT,
     X11c_init,  X11_reset,  
     X11_text,  NULL_scale,  X11_graphics,  X11_move,  X11_vector,  
     X11_linetype,  X11_put_text,  NULL_text_angle,  
     X11_justify_text,  X11_dot,  do_arrow, NULL_fillbox, X11_line_thickness, NULL_color, NULL_add_color,NULL_sendCoordinates,NULL_spectral,NULL_movie}
  , {"motif",  "X11 Window System--Motif\n\tMotif device accepts dashes, linetypetable, doublebuffer, \n\tgreyscale, movie, and spectrum device arguments.", 
     X11_XMAX,  X11_YMAX,  X11_VCHAR,  X11_HCHAR,  
     X11_VTIC,  X11_HTIC,  
     TERM_WINDOWS|TERM_XWINDOWS|TERM_COLOR|TERM_POLYFILL|TERM_NOPROMPT|TERM_FLUSHSTDOUT,
     X11bmotif_init,  X11b_reset,  
     X11b_text,  NULL_scale,  X11b_graphics,  X11b_move,  X11b_vector,  
     X11b_linetype,  X11_put_text,  NULL_text_angle,  
     X11_justify_text,  X11b_dot,  do_arrow, X11b_fill_box, X11b_line_thickness, X11b_color, X11b_add_color,X11b_sendCoordinates,X11b_spectral,NULL_movie}
#endif

#ifdef USE_GD_LIBRARY
#ifdef PNG
  , {"png",  "PNG Format",
     PNG_XMAX, PNG_YMAX, 0, 0, (PNG_YMAX/100.0), (PNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     PNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"pdapng",  "PDA PNG Format",
     PDAPNG_XMAX, PDAPNG_YMAX, 0, 0, (PDAPNG_YMAX/100), (PDAPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     PDAPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"tpng",  "Tiny PNG Format",
     TPNG_XMAX, TPNG_YMAX, 0, 0, (TPNG_YMAX/100), (TPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     TPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"spng",  "Small PNG Format",
     SPNG_XMAX, SPNG_YMAX, 0, 0, (SPNG_YMAX/100), (SPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     SPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"mpng",  "Medium PNG Format",
     MPNG_XMAX, MPNG_YMAX, 0, 0, (MPNG_YMAX/100), (MPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     MPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"lpng",  "Large PNG Format",
     LPNG_XMAX, LPNG_YMAX, 0, 0, (LPNG_YMAX/100), (LPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     LPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness,  PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"hpng",  "Huge PNG Format",
     HPNG_XMAX, HPNG_YMAX, 0, 0, (HPNG_YMAX/100), (HPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     HPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness,  PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"gpng",  "Gigantic PNG Format",
     GPNG_XMAX, GPNG_YMAX, 0, 0, (HPNG_YMAX/100), (HPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     GPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness,  PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"lfgpng",  "640x480 PNG Format",
     SIZE640X480PNG_XMAX, SIZE640X480PNG_YMAX, 0, 0, (SIZE640X480PNG_YMAX/100), (SIZE640X480PNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     SIZE640X480PNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness,  PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"mv200png",  "512x480 PNG Format\n\tPNG devices accept dashes, rootname, template, onwhite, onblack, \n\tand linetypetable device arguments.",
     SIZE512X480PNG_XMAX, SIZE512X480PNG_YMAX, 0, 0, (SIZE512X480PNG_YMAX/100), (SIZE512X480PNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     SIZE512X480PNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness,  PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"gif",  "(No longer suppored, png used instead)",
     PNG_XMAX, PNG_YMAX, 0, 0, (PNG_YMAX/100.0), (PNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     PNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"tgif",  "(No longer suppored, tpng used instead)",
     TPNG_XMAX, TPNG_YMAX, 0, 0, (TPNG_YMAX/100), (TPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     TPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"sgif",  "(No longer suppored, spng used instead)",
     SPNG_XMAX, SPNG_YMAX, 0, 0, (SPNG_YMAX/100), (SPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     SPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"mgif",  "(No longer suppored, mpng used instead)",
     MPNG_XMAX, MPNG_YMAX, 0, 0, (MPNG_YMAX/100), (MPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     MPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness, PNG_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
  , {"lgif",  "(No longer suppored, lpng used instead)",
     LPNG_XMAX, LPNG_YMAX, 0, 0, (LPNG_YMAX/100), (LPNG_YMAX/100), 
     TERM_HARDCOPY|TERM_POLYFILL|TERM_NOPROMPT, 
     LPNG_init, PNG_reset, PNG_text, 
     NULL_scale, PNG_graphics, PNG_move, PNG_vector, 
     PNG_linetype, PNG_put_text, PNG_text_angle, 
     NULL_justify_text, PNG_dot, do_arrow, PNG_fill_box, PNG_line_thickness,  NULL_color, PNG_add_color,NULL_sendCoordinates,PNG_spectral,PNG_movie}
#endif
#endif

};

#define TERMCOUNT (sizeof(term_tbl)/sizeof(struct termentry))


int list_terms(FILE *fp)
{
  register int i;

  fprintf(fp, "\nAvailable terminal types (from GNUPLOT 2.02 drivers):\n");
  for (i = 1; i < TERMCOUNT; i++)
    fprintf(fp,  "  %15s  %s\n", 
            term_tbl[i].name,  term_tbl[i].description);
  fprintf(fp,  "\nYou need only type enough of the terminal name to specify it uniquely.\n");
  return 0;
}


/* set_term: get terminal number from name on command line */
/* will change 'term' variable if successful */
int     term;                                   /* term number */

/* change_term: get terminal number from name and set terminal type */
/* returns -1 if unknown,  -2 if ambiguous,  >=0 is terminal number */
int change_term(char *name, int length)
{
  int i,  t = -1;

  for (i = 0; i < TERMCOUNT; i++) {
    if (!strncmp(name, term_tbl[i].name, length)) {
      if (t != -1)
        return(-2); /* ambiguous */
      t = i;
    }
  }

  if (t == -1)                        /* unknown */
    return(t);

  /* Success: set terminal type now */

  /* Special handling for unixplot term type */
  if (!strncmp("unixplot", name, sizeof(unixplot))) {
    UP_redirect (2);  /* Redirect actual stdout for unixplots */
  } else if (unixplot) {
    UP_redirect (3);  /* Put stdout back together again. */
  }

  term = t;
  term_init = FALSE;
  name = term_tbl[term].name;

  return(t);
}


/*
  This is always defined so we don't have to have command.c know if it
  is there or not.
*/
#ifndef UNIXPLOT
int UP_redirect( int caller)
{
  /*caller = caller;*/        /* to stop Turbo C complaining 
                           * about caller not being used */
  return 0;
}
#else
UP_redirect (int caller)
{
  /*  
   */
  switch (caller) {
  case 1:
    /* Don't save,  just replace stdout w/outfile (save was already done). */
    if (unixplot)
      *(stdout) = *(outfile);  /* Copy FILE structure */
    break;
  case 2:
    if (!unixplot) {
      fflush(stdout);
      save_stdout = *(stdout);
      *(stdout) = *(outfile);  /* Copy FILE structure */
      unixplot = 1;
    }
    break;
  case 3:
    /* New terminal in use--put stdout back to original. */
    closepl();
    fflush(stdout);
    *(stdout) = save_stdout;  /* Copy FILE structure */
    unixplot = 0;
    break;
  case 4:
    /*  User really wants to go to normal output... */
    if (unixplot) {
      fflush(stdout);
      *(stdout) = save_stdout;  /* Copy FILE structure */
    }
    break;
  }
}
#endif



#ifdef PC
/* output for some terminal types must be binary to stop non Unix computers
   changing \n to \r\n. 
   If the output is not STDOUT,  the following code reopens outfile 
   with binary mode. */
int reopen_binary(void)
{
  char filename[MAX_ID_LEN+1];

  if (strcmp(outstr, "STDOUT")) {
    (void) fclose(outfile);
    (void) strcpy_ss(filename, outstr+1);       /* remove quotes */
    filename[strlen(filename)-1] = '\0';
    if ( (outfile = fopen(filename, "wb")) == (FILE *)NULL ) {
      if ( (outfile = fopen(filename, "w")) == (FILE *)NULL ) {
        os_error("cannot reopen file with binary type; output unknown", 
                 NO_CARET);
      } 
      else {
        os_error("cannot reopen file with binary type; output reset to ascii",  
                 NO_CARET);
      }
    }
  }
}
#endif

#ifdef VMS
/* these are needed to modify terminal characteristics */
#include <descrip.h>
#include <iodef.h>
#include <ttdef.h>
#include <tt2def.h>
#include <dcdef.h>
#include <ssdef.h>
#include <stat.h>
#include <fab.h>
static unsigned short   chan;
static int  old_char_buf[3],  cur_char_buf[3];
$DESCRIPTOR(sysoutput_desc, "SYS$OUTPUT");

char *vms_init(void)
{
  /*  
   * Determine if we have a regis terminal
   * and save terminal characteristics
   */
  /* Save terminal characteristics in old_char_buf and
     initialise cur_char_buf to current settings. */
  int i;
  sys$assign(&sysoutput_desc, &chan, 0, 0);
  sys$qiow(0, chan, IO$_SENSEMODE, 0, 0, 0, old_char_buf, 12, 0, 0, 0, 0);
  for (i = 0 ; i < 3 ; ++i) cur_char_buf[i] = old_char_buf[i];
  sys$dassgn(chan);

  /* Test if terminal is regis */
  if ((cur_char_buf[2] & TT2$M_REGIS) == TT2$M_REGIS) return("regis");
  return(NULL);
}

int vms_reset(void)
{
  /*   set terminal to original state */
  int i;
  sys$assign(&sysoutput_desc, &chan, 0, 0);
  sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, old_char_buf, 12, 0, 0, 0, 0);
  for (i = 0 ; i < 3 ; ++i) cur_char_buf[i] = old_char_buf[i];
  sys$dassgn(chan);
}


int term_mode_tek(void)
    
{
  /*   set terminal mode to tektronix */
  long status;
  if (outfile != stdout) return; /* don't modify if not stdout */
  sys$assign(&sysoutput_desc, &chan, 0, 0);
  cur_char_buf[0] = 0x004A0000 | DC$_TERM | (TT$_TEK401X<<8);
  cur_char_buf[1] = (cur_char_buf[1] & 0x00FFFFFF) | 0x18000000;

  cur_char_buf[1] &= ~TT$M_CRFILL;
  cur_char_buf[1] &= ~TT$M_ESCAPE;
  cur_char_buf[1] &= ~TT$M_HALFDUP;
  cur_char_buf[1] &= ~TT$M_LFFILL;
  cur_char_buf[1] &= ~TT$M_MECHFORM;
  cur_char_buf[1] &= ~TT$M_NOBRDCST;
  cur_char_buf[1] &= ~TT$M_NOECHO;
  cur_char_buf[1] &= ~TT$M_READSYNC;
  cur_char_buf[1] &= ~TT$M_REMOTE;
  cur_char_buf[1] |= TT$M_LOWER;
  cur_char_buf[1] |= TT$M_TTSYNC;
  cur_char_buf[1] |= TT$M_WRAP;
  cur_char_buf[1] &= ~TT$M_EIGHTBIT;
  cur_char_buf[1] &= ~TT$M_MECHTAB;
  cur_char_buf[1] &= ~TT$M_SCOPE;
  cur_char_buf[1] |= TT$M_HOSTSYNC;

  cur_char_buf[2] &= ~TT2$M_APP_KEYPAD;
  cur_char_buf[2] &= ~TT2$M_BLOCK;
  cur_char_buf[2] &= ~TT2$M_DECCRT3;
  cur_char_buf[2] &= ~TT2$M_LOCALECHO;
  cur_char_buf[2] &= ~TT2$M_PASTHRU;
  cur_char_buf[2] &= ~TT2$M_REGIS;
  cur_char_buf[2] &= ~TT2$M_SIXEL;
  cur_char_buf[2] |= TT2$M_BRDCSTMBX;
  cur_char_buf[2] |= TT2$M_EDITING;
  cur_char_buf[2] |= TT2$M_INSERT;
  cur_char_buf[2] |= TT2$M_PRINTER;
  cur_char_buf[2] &= ~TT2$M_ANSICRT;
  cur_char_buf[2] &= ~TT2$M_AVO;
  cur_char_buf[2] &= ~TT2$M_DECCRT;
  cur_char_buf[2] &= ~TT2$M_DECCRT2;
  cur_char_buf[2] &= ~TT2$M_DRCS;
  cur_char_buf[2] &= ~TT2$M_EDIT;
  cur_char_buf[2] |= TT2$M_FALLBACK;

  status = sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, cur_char_buf, 12, 0, 0, 0, 0);
  if (status == SS$_BADPARAM) {
    /* terminal fallback utility not installed on system */
    cur_char_buf[2] &= ~TT2$M_FALLBACK;
    sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, cur_char_buf, 12, 0, 0, 0, 0);
  }
  else {
    if (status != SS$_NORMAL)
      lib$signal(status, 0, 0);
  }
  sys$dassgn(chan);
}


int term_mode_native(void)
    
{
  /*   set terminal mode back to native */
  int i;
  if (outfile != stdout) return; /* don't modify if not stdout */
  sys$assign(&sysoutput_desc, &chan, 0, 0);
  sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, old_char_buf, 12, 0, 0, 0, 0);
  for (i = 0 ; i < 3 ; ++i) cur_char_buf[i] = old_char_buf[i];
  sys$dassgn(chan);
}


int term_pasthru(void)
    
{
  /*   set terminal mode pasthru */
  if (outfile != stdout) return; /* don't modify if not stdout */
  sys$assign(&sysoutput_desc, &chan, 0, 0);
  cur_char_buf[2] |= TT2$M_PASTHRU;
  sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, cur_char_buf, 12, 0, 0, 0, 0);
  sys$dassgn(chan);
}


int term_nopasthru(void)
    
{
  /*   set terminal mode nopasthru */
  if (outfile != stdout) return; /* don't modify if not stdout */
  sys$assign(&sysoutput_desc, &chan, 0, 0);
  cur_char_buf[2] &= ~TT2$M_PASTHRU;
  sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, cur_char_buf, 12, 0, 0, 0, 0);
  sys$dassgn(chan);
}


int reopen_binary(void)
{
  /*   close the file outfile outfile and reopen it with binary type
       if not already done or outfile == stdout */
  stat_t stat_buf;
  char filename[MAX_ID_LEN+1];
  if (outfile != stdout) { /* don't modify if not stdout */
    if (!fstat(fileno(outfile), &stat_buf)) {
      if (stat_buf.st_fab_rfm != FAB$C_FIX) {
        /* modify only if not already done */
        (void) fclose(outfile);
        (void) strcpy_ss(filename, outstr+1);   /* remove quotes */
        filename[strlen(filename)-1] = '\0';
        (void) delete(filename);
        if ((outfile = fopen(filename, "wb", "rfm=fix", "bls=512", "mrs=512"))
            == (FILE *)NULL ) {
          if ( (outfile = fopen(filename, "w")) == (FILE *)NULL ) {
            os_error("cannot reopen file with binary type; output unknown", 
                     NO_CARET);
          }
          else {
            os_error("cannot reopen file with binary type; output reset to ascii", 
                     NO_CARET);
          }
        }
      }
    }
    else{
      os_error("cannot reopen file with binary type; output remains ascii", 
               NO_CARET);
    }
  }
}


int fflush_binary(void)
{
  typedef short int INT16;     /* signed 16-bit integers */
  register INT16 k;            /* loop index */
  if (outfile != stdout) {
    /* Stupid VMS fflush() raises error and loses last data block
       unless it is full for a fixed-length record binary file.
       Pad it here with NULL characters. */
    for (k = (INT16)((*outfile)->_cnt); k > 0; --k)
      putc('\0', outfile);
    fflush(outfile);
  }
}
#endif


int os_error(char *str, int t_num)
{
  fprintf(stderr,  "error: %s\n",  str);
  exit(1);
  return(1);
}

/* the following is based on an algorithm from Ken Evans: */
void RGB_values(double *red, double *green, double *blue, double hue)
{
  long ngroups = 5,  nmembers = 45,  ntotal;
  double gh,  ih;
  double high=1,  medium=0.375,  deltax,  hx,  r,  g,  b;
  
  ntotal = ngroups*nmembers;
  deltax = (high-medium)/nmembers;
  hx = hue*ntotal;
  if (hx<0)
    hx = 0;
  if (hx>ntotal)
    hx = ntotal;
  gh = (int)(hx/nmembers);
  ih = (int)(hx);
  switch ((int)gh) {
  case 0: 
    r = medium;
    g = medium + (ih-gh*nmembers)*deltax;
    b = high;
    break;
  case 1:
    r = medium;
    g = high;
    b = high-(ih-gh*nmembers)*deltax;
    break;
  case 2: 
    r = medium + (ih-gh*nmembers)*deltax;
    g = high;
    b = medium;
    break;
  case 3: 
    r = high;
    g = high-(ih-gh*nmembers)*deltax;
    b = medium;
    break;
  case 4: 
    r = high;
    g = medium;
    b = medium+(ih-gh*nmembers)*deltax;
    break;
  default:
    r = high;
    g = medium;
    b = high;
    break;
  }
  
  *red = r;
  *green = g;
  *blue = b;
}

/* the following is based on an algorithm from Robert Soliday: */
void Spectral_RGB_values(double *red, double *green, double *blue, double hue)
{
  double k;
  k = 6 * hue;
  if (k < 1) {
    //red to orange
    *red = (double).9999;
    *green = (double)(.65 * k);
    *blue = (double)(0);
  } else if (k < 2) {
    //orange to yellow
    *red = (double).9999;
    *green = (double)(.65 + (.35 * (k-1)));
    *blue = (double)(0);
  } else if (k < 2.3) {
    //yellow to green (heavy yellow)
    *red = (double)(1.0 - (.20 * ((k-2.0)/.3)));
    *green = (double).9999;
    *blue = (double)(0);
  } else if (k < 3) {
    //yellow to green (heavy green)
    *red = (double)(.80 * (1 - ((k-2.3)/.7)));
    *green = (double).9999;
    *blue = (double)(0);
  } else if (k < 3.4) {
    //green to cyan (heavy green)
    *red = (double)(0);
    *green = (double).9999;
    *blue = (double)(.85 * ((k-3)/.4));
  } else if (k < 4) {
    //green to cyan (heavy cyan)
    *red = (double)(0);
    *green = (double).9999;
    *blue = (double)(.85 + (.15 * ((k-3.4)/.6)));
  } else if (k < 5) {
    //cyan to blue
    *red = (double)(0);
    *green = (double)(1 - (k-4));
    *blue = (double).99999;
  } else {
    //blue to violet
    *red = (double)(k-5);
    *green = (double)(0);
    *blue = (double).99999;
  }
}

void Spectral_RGB_NoMagenta_values(double *red, double *green, double *blue, double hue)
{
  double k;

  k = 5 * hue;
  if (k < 1) {
    //red to orange
    *red = (double).9999;
    *green = (double)(.65 * k);
    *blue = (double)(0);
  } else if (k < 2) {
    //orange to yellow
    *red = (double).9999;
    *green = (double)(.65 + (.35 * (k-1)));
    *blue = (double)(0);
  } else if (k < 2.3) {
    //yellow to green (heavy yellow)
    *red = (double)(1.0 - (.20 * ((k-2.0)/.3)));
    *green = (double).9999;
    *blue = (double)(0);
  } else if (k < 3) {
    //yellow to green (heavy green)
    *red = (double)(.80 * (1 - ((k-2.3)/.7)));
    *green = (double).9999;
    *blue = (double)(0);
  } else if (k < 3.4) {
    //green to cyan (heavy green)
    *red = (double)(0);
    *green = (double).9999;
    *blue = (double)(.85 * ((k-3)/.4));
  } else if (k < 4) {
    //green to cyan (heavy cyan)
    *red = (double)(0);
    *green = (double).9999;
    *blue = (double)(.85 + (.15 * ((k-3.4)/.6)));
  } else {
    //cyan to blue
    *red = (double)(0);
    *green = (double)(1 - (k-4));
    *blue = (double).99999;
  }
}

void Spectral_BGR_values(double *red, double *green, double *blue, double hue)
{
  double k;

  k = 6 * hue;
  if (k < 1) {
    //violet to blue
    *red = (double)(1 - k);
    *green = (double)(0);
    *blue = (double).99999;
  } else if (k < 2) {
    //blue to cyan
    *red = (double)(0);
    *green = (double)(k - 1);
    *blue = (double).99999;
  } else if (k < 2.4) {
    //cyan to green (heavy cyan)
    *red = (double)(0);
    *green = (double).9999;
    *blue = (double)(1.0 - (.15 * ((k-2)/.4)));
  } else if (k < 3) {
    //cyan to green (heavy green)
    *red = (double)(0);
    *green = (double).9999;
    *blue = (double)(.85 * (1 - ((k-2.4)/.6)));
  } else if (k < 3.7) {
    //green to yellow (heavy green)
    *red = (double)(.80 * ((k-3)/.7));
    *green = (double).9999;
    *blue = (double)(0);
  } else if (k < 4) {
    //green to yellow (heavy yellow)
    *red = (double)(.80 + (.20 * ((k-3.7)/.3)));
    *green = (double).9999;
    *blue = (double)(0);
  } else if (k < 5) {
    //yellow to orange
    *red = (double).9999;
    *green = (double)(1.0 - (.35 * ((k-4)/1.0)));
    *blue = (double)(0);
  } else {
    //orange to red
    *red = (double).9999;
    *green = (double)(.65 * (1 - ((k-5)/1.0)));
    *blue = (double)(0);
  }
}

void Spectral_BGR_NoMagenta_values(double *red, double *green, double *blue, double hue)
{
  double k;

  k = 5 * hue;
  if (k < 1) {
    //blue to cyan
    *red = (double)(0);
    *green = (double)k;
    *blue = (double).99999;
  } else if (k < 1.4) {
    //cyan to green (heavy cyan)
    *red = (double)(0);
    *green = (double).9999;
    *blue = (double)(1.0 - (.15 * ((k-1)/.4)));
  } else if (k < 2) {
    //cyan to green (heavy green)
    *red = (double)(0);
    *green = (double).9999;
    *blue = (double)(.85 * (1 - ((k-1.4)/.6)));
  } else if (k < 2.7) {
    //green to yellow (heavy green)
    *red = (double)(.80 * ((k-2)/.7));
    *green = (double).9999;
    *blue = (double)(0);
  } else if (k < 3) {
    //green to yellow (heavy yellow)
    *red = (double)(.80 + (.20 * ((k-2.7)/.3)));
    *green = (double).9999;
    *blue = (double)(0);
  } else if (k < 4) {
    //yellow to orange
    *red = (double).9999;
    *green = (double)(1.0 - (.35 * ((k-3)/1.0)));
    *blue = (double)(0);
  } else {
    //orange to red
    *red = (double).9999;
    *green = (double)(.65 * (1 - ((k-4)/1.0)));
    *blue = (double)(0);
  }
}

/* the following is based on an algorithm from Robert Soliday: */
void Custom_RGB_values(double *red, double *green, double *blue, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1, double hue)
{
  *red = (red0 + hue * (red1 - red0))/65535.0;
  *green = (green0 + hue * (green1 - green0))/65535.0;
  *blue = (blue0 + hue * (blue1 - blue0))/65535.0;
}




