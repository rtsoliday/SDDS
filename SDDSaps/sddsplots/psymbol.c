/*CopyrightNotice001

*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1995 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from June 30, 1995, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY.

CopyrightNotice001
 */

#include "mdb.h"
#include "psymbol.h"
#include "graph.h"
#include "graphics.h"
#include "gnumacros.h"
#include "hersheyfont.h"

extern int term;
extern struct termentry term_tbl[];
int PS_solid_dash(void);


char defaultfont[20];
void set_default_font(char* fontname) {
  sprintf(defaultfont, "%s", fontname);
  return;
}
char* get_default_font() {
  return(defaultfont);
}

float psymbol(
              int x, int y,     /* position in device coordinates */
              char *ktext,
              float size,       /* desired width of 'W' in device coordinates */
              float aspect,     /* desired aspect ratio (character height 
                                   to character width */
              float daspect,    /* ratio of y pixel size to x pixel size */
              float angle, 
              float tilt,
              int text_length, 
              int mode
              )   
{
  double extent[4];

  return psymbol1(x, y, ktext, size, aspect, daspect, angle, tilt, text_length, mode, extent);
}


float psymbol1(
               int x, int y,     /* position in device coordinates */
               char *ktext,
               float size,       /* desired width of 'W' in device coordinates */
               float aspect,     /* desired aspect ratio (character height 
                                    to character width */
               float daspect,    /* ratio of y pixel size to x pixel size */ /* always 1.0 in this code */
               float angle, 
               float tilt,
               int text_length, 
               int mode,
               double *extent    /* output in user coords: horizontal and vertical extent of box surrounding the text, plus horizontal and vertical offset of center from the given coordinates . */
               )   
{
  /************************************************************************
    ROUTINE TO DRAW SOFTWARE CHARACTERS USING      
    THE H E R S H E Y  CHARACTER TABLE             
    
    N.M. WOLCOTT, J. HILSENRATH                    
    A CONTRIBUTION TO COMPUTER TYPESETTING TECHNIQUES    
    TABLE OF COORDINATES FOR HERSHEY'S REPERTORY    
    OF OCCIDENTAL TYPE FONTS AND GRAPHICS SYMBOLS    
    NATIONAL BUREAU OF STANDARDS    
    SPECIAL PUBLICATION NO. 424    
    U.S. GOVERNMENT PRINTING OFFICE    
    WASHINGTON,  JANUARY 1976.    

    Translated into C by Michael Borland, November 1989  

    X,Y ARE COORDINATES FOR BEGINNING OF TEXT

    KTEXT CONTAINS PACKED TEXT                               

    SIZE IS THE SIZE OF THE SYMBOLS IN CM 

    ANGLE IS THE ROTATION ANGLE IN DEGREES

    TEXT_LENGTH  IS THE NR. OF CHARACTERS IN THE TEXT

    IGO (mode) CONTROL PARAMETER
    IGO= -1  TEXT IS LEFT ADJUSTED ON X,Y    
    IGO= 2  RETURN LENGTH OF TEXT IN ANGLE (CM)    

    IX,IY  PACKED SYMBOLS 

    --Version 2, for use with GHOST package--May 1989, Michael Borland:
    I've added 4 new arguments: ASPECT, TILT, XSCALE, YSCALE.
    ASPECT:  The ratio between the height and the width of the 
    characters.
    TILT:    The tilt (or slant) angle of the individual characters,
    in degrees.
    XSCALE, YSCALE:  
    The GHOST point-plotting routine (ghost$ptplot) works in 
    user's coordinates.  The character sizes,  however, are
    best specified in unit (i.e., [0,1]x[0,1]) coordinates.  
    XSCALE and YSCALE provide conversion factors from unit
    coordinates to user's coordinates.
    OTHER CHANGES:
    The meaning of SIZE is changed:  SIZE is the width of the characters
    in unit coordinates.  A character of size 1 with an aspect ratio
    of 1 will fill the entire plotting region.
    The string does NOT need to end with $ or : .
    The string may be up to 256 characters long.
    
    ****************************************************************************/
  static short ascii_index[256];
  int xp[40], yp[40], xInput, yInput;
  double xs,ys;
  double si,co,aspectp,xpp,ypp,rscale,the,tiltp,scale;
  static long first_call = 0;
  static float change_scale = 1;
  long d_kspec = 0, d_iy0 = 9;
  int resetDash = 0;

  if (mode == LEFT_JUSTIFY && (term_tbl[term].flags & TERM_POSTSCRIPT)) {
    PS_solid_dash();
    resetDash = 1;
  }

  /* control characters 
     c     \001=^a   go to normal script level 
     c     \002=^b   go to subscript   
     c     \003=^c   go to superscript 
     c     \004=^d   go to 50% larger characters
     c     \005=^e   go to 2/3-size characters 
     c     \006=^f   go to greek   
     c     \007=^g   go to roman   
     c     \010=^h   backspace one character   
     c     \011=^i   go to special symbols 
     c     \012=^j   end of special symbols
     c     \013=^k   vertical offset 1/2 character height
     c     \014=^l   vertical offset -1/2 character height
     c     \015=^m   double the aspect ratio
     c     \016=^n   halve the aspect ratio
     */
  char *standard_chars = 
    /*                1         2         3         4        */
    /*       12345678901234567890123456789012345678901234567  */
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,/()-+=*$ \001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\032\033\034\035\036\037";
  static long n_standard_chars;

  /*    correspondence between greek alphabet and roman alphabet    */
  static long lgreek[26] = {
    1,2,7,4,5,21,3,22,9,9,10,11,12,13,15,16,8,17,18,
    19,20,22,24,14,23,6
    } ;
  long i, ic, iw, kymin, kymax, kx, kxMin, roman;
  float tan_tiltp, yoff, yoff2, wtot;
  long ndraw, klow, kgreek, kspec, ix0, iy0, lspec, i_ascii, ichr = 0;
  long ky, dky, k, ichar;
  float d_rscale, d_scale, xw = 0, yw = 0, wdth = 0, xpk, ypk;
  float xpMin, xpMax, ypMin, ypMax;
  int romanchar;
  char *fontname;
  struct hershey_font_definition *hfd;
  struct hershey_character *hc;

  fontname = get_default_font();
  xInput = x;
  yInput = y;

  if (daspect<=0)
    daspect = 1;

  if ((aspectp=aspect)==0) 
    aspectp = daspect;
  else
    aspectp = aspect*daspect;

  tan_tiltp = tan(tiltp = tilt*PI/180.);

  if (first_call==0) {
    first_call = 1 ;
    n_standard_chars = strlen(standard_chars);
    for (i=0; i<n_standard_chars; i++) 
      ascii_index[((short)standard_chars[i])%256] = i+1;
  }                  

  if (mode!=2 && mode!=-1)
    bomb("psymbol now accepts modes of 2 or -1 only", NULL);
  
  if (mode==2)
    the = 0.;
  else {
    the = angle;
    if (fabs(the-90.0)<1e-6)
      the = 90.0;
    else if (fabs(the+90.0)<1e-6)
      the = -90.0;
    else if (fabs(the-180.0)<1e-6)
      the = 180.0;
    else if (fabs(the+180.0)<1e-6)
      the = -180.0;
    else if (fabs(the-270.0)<1e-6)
      the = 270.0;
    else if (fabs(the+270.0)<1e-6)
      the = -270.0;
    the *= PI/180.0;
  }

  si = sin(the);
  co = cos(the);
  xs = x;
  ys = y;
  xpp = xs;
  ypp = ys;
  
  xpMin = ypMin = FLT_MAX;
  xpMax = ypMax = 0;

  /* set default values */
  ndraw=0   ;
  klow=0    ;
  kgreek=0  ;
  kspec=0   ;
  d_scale=scale = size/21;
  d_rscale=rscale=scale*change_scale;
  iy0=9 ;
  yoff  = 0.;       /* offset added by subscript/superscript */
  yoff2 = 0.;       /* offset added by up/down vertical control seq */
  wtot  = 0.;  
  
  hfd = hershey_font_load(fontname);
  if (hfd==NULL) {
    fprintf(stderr, "Invalid font name given\n");
    exit(1);
  }
  /* Special Parameter LSPEC added to implement more ASCII's */
  lspec = 0;
  /* start loop for each character to be drawn  */
  for (ichar=0; ichar<text_length; ichar++) {
    /***********************************************************
      c	special handling for lower characters in input buffer
      c	these are translated into upper characters and the
      c	lower case indicator is set.
      c	the control characters for upper and lower characters
      c	are no longer active.
      c***********************************************************/

    i_ascii = ktext[ichar]%256;  
    /* Remove special setting for additional ASCII */

    if (lspec==1) {
      kspec=d_kspec;
      scale=d_scale;
      rscale=d_rscale;
      iy0=d_iy0;
      lspec=0;
    }
    if (i_ascii >= 97 && i_ascii <= 122) {
      i_ascii = i_ascii - 32; 	/* translate to upper case */
      klow = 105;     /* indicate lower case */
    } else if (i_ascii < 0) {
      i_ascii = 39 - i_ascii;		/* translate to upper case */
      klow = 105;		/* indicate lower case */
    } else if (i_ascii == 33) {
      i_ascii = 69;
      lspec = 1;
    } else if (i_ascii == 35) {	/* convert "#" to special("f") */
      i_ascii = 70;
      lspec = 1;
    } else if (i_ascii == 36) {	/* convert "$" to special("d") */
      i_ascii = 68;
      lspec = 1;
    } else if (i_ascii == 37) {	/* convert "%" to special("y") */
      i_ascii = 89;
      lspec = 1;
    } else if (i_ascii == 94) {	/* convert "^" to special("6") */
      i_ascii = 54;
      lspec = 1;
    } else if (i_ascii == 38) {	/* convert "&" to special("w") */
      i_ascii = 87;
      lspec = 1;
    } else if (i_ascii == 95) {	/* convert "_" to special("-") */
      i_ascii = 45;
      lspec = 1;
    } else if (i_ascii ==123) {	/* convert "{" to special("p") */
      i_ascii = 80;
      lspec = 1;
    } else if (i_ascii ==125) {	/* convert "}" to special("q") */
      i_ascii = 81;
      lspec = 1;
    } else if (i_ascii == 91) {	/* convert "[" to special("m") */
      i_ascii = 77;
      lspec = 1;
    } else if (i_ascii == 93) {	/* convert "]" to special("n") */
      i_ascii = 78;
      lspec = 1;
    } else if (i_ascii == 58) {	/*         ":" not scaled */
      ichr = 70;
      lspec = 0;
    } else if (i_ascii == 59) {	/*         ";" not scaled */
      ichr = 71;
      lspec = 0;
    } else if (i_ascii == 60) {	/* convert "<" to special("l") */
      i_ascii = 76;
      lspec = 1;
    } else if (i_ascii == 62) {	/* convert ">" to special("g") */
      i_ascii = 71;
      lspec = 1;
    } else if (i_ascii == 63) {	/* convert "?" to special("h") */
      i_ascii = 72;
      lspec = 1;
    } else if (i_ascii == 64) {	/* new inserted "@" */
      ichr = 211;
      lspec = 1;
    } else if (i_ascii == 34) {	/* new inserted """ */
      ichr = 212;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 16) {	/* new inserted "ae" (16) */
      ichr = 213;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 17) {	/* new inserted "oe" (17) */
      ichr = 214;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 18) {	/* new inserted "ue" (18) */
      ichr = 215;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 19) {	/* new inserted "ae" (19) */
      ichr = 216;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 20) {	/* new inserted "oe" (20) */
      ichr = 217;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 21) {	/* new inserted "ue" (21) */
      ichr = 218;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 39 || i_ascii==96) {	/* new inserted "'" */
      ichr = 219;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 22) {	/* new inserted "sz" (22) */
      ichr = 220;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 23) {	/* new inserted "promille" (23) */
      ichr = 221;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 24) {	/* new inserted "weihnachtsmann" (24) */
      ichr = 222;
      klow = 0;
      lspec = 1;
    } else if (i_ascii == 25) {	/* new inserted "elsa-emblem" (25) */
      ichr = 223;
      klow = 0;
      lspec = 0;
    } else if (i_ascii == 124) { /* | implemented as special(B) */
      i_ascii = 66;
      lspec = 1;
    } else {
      klow = 0;	/* indicate upper case */
    }
    if (lspec==1) {
      d_kspec=kspec;
      d_scale=scale;
      d_rscale=rscale;
      d_iy0=iy0;
      kspec = 1;
      klow = 0;
    }

    roman=0;
    if (!((i_ascii>=16 && i_ascii<=25) || i_ascii==34 || i_ascii==39 || i_ascii==58 || i_ascii==59
          || i_ascii==64)) {
      /* come here for symbols other than: 
         "ae"  (16)  "oe"  (17)  "ue"  (18)  "ae"  (19)   "oe"  (20)  
         "ue"  (21)  "sz"  (22)  "promille"  (23)  "weihnachtsmann"  (24) 
         "elsa-emblem"  (25)  """  "'"   ":" ";" "@" 
         */
      ic = ascii_index[i_ascii];
      if (ic>n_standard_chars || ic==47) {
        /* blank space */
        if (klow!=0)
          wdth = 10.*rscale*1.05;
        else
          wdth = 20.*rscale*1.05;
        wtot += wdth;
        xpp  += wdth*co;
        ypp  += wdth*si;
        continue;
      }
      if (ic>47) {
        /* interpret control characters    */
        switch (ic-47) {
        case 1: 
          /* normal level of script     */
          yoff   = 0.;
          rscale = scale*change_scale;
          break;
        case 2: 
          /* subscript */
          yoff   = -0.5*size*change_scale;
          rscale = 0.66*scale*change_scale ;
          break;
        case 3: 
          /* superscript   */
          yoff   = 0.5*size*change_scale;
          rscale = 0.66*scale*change_scale;
          break;
        case 4: 
          /* increase character size by 50% */
          change_scale = 1.5*change_scale;
          rscale       = change_scale*scale;
          break;
        case 5: 
          /* decrease character size to 2/3 */
          change_scale = change_scale/1.5 ;
          rscale       = change_scale*scale;
          break;
        case 6: 
          /* greek alphabet */
          kgreek = 1;
          break;
        case 7:
          /* end of greek alphabet (start roman alphabet) */
          kgreek = 0;
          hershey_font_free(hfd);
          hfd = hershey_font_load(fontname);
          break;
        case 8: 
          /* backspace one half-character    */
          if (ndraw>0) {
            if (!wdth) {
              wdth = (klow?10:20)*rscale*1.05;
              xw=wdth*co;
              yw=wdth*si ;  
            }
            wtot=wtot-wdth/2;
            xpp=xpp-xw/2;
            ypp=ypp-yw/2;
          }
          break;
        case 9:
          /* go to special symbols  */
          kspec=1    ;
          break;
        case 10: 
          /* end of special symbols     */
          kspec  = 0;
          iy0    = 9;
          scale  = size/21.;
          rscale = scale*change_scale;
          break;
        case 11:
          /* vertical offset 1/2 character height upward */
          yoff2 += 0.5*size*change_scale;
          break;
        case 12:
          /* vertical offset 1/2 character height downward */
          yoff2 += -0.5*size*change_scale;
          break;
        case 13:
          /* double the aspect ratio */
          aspectp *= 2;
          break;
        case 14:
          /* halve the aspect ratio */
          aspectp /= 2;
          break;
        case 15:
          /* switch to cyrillic font */
          kgreek = 0;
          hershey_font_free(hfd);
          hfd = hershey_font_load("cyrillic");
          break;
        case 16:
          /* switch to math font */
          kgreek = 0;
          hershey_font_free(hfd);
          hfd = hershey_font_load("mathlow");
          break;
        case 17:
          /* switch to greek font (not the original greek font) */
          kgreek = 0;
          hershey_font_free(hfd);
          hfd = hershey_font_load("greek");
          break;
        case 18:
          /* switch to symbolic font (not the original special font) */
          kgreek = 0;
          hershey_font_free(hfd);
          hfd = hershey_font_load("symbolic");
          break;
        case 19:
          /* switch to rowmans */
          kgreek = 0;
          hershey_font_free(hfd);
          hfd = hershey_font_load("rowmans");
          break;
        case 20:
          /* switch to rowmans */
          kgreek = 0;
          hershey_font_free(hfd);
          hfd = hershey_font_load("rowmand");
          break;
        case 21:
          /* switch to rowmans */
          kgreek = 0;
          hershey_font_free(hfd);
          hfd = hershey_font_load("rowmant");
          break;
        default: 
          fprintf(stderr, "unknown special symbol code = %ld\n", ic-47); 
          exit(1);  
          break;
        }
        continue;
      }
      if (ic==46) 
        break ;
      if (kspec!=0) {
        /* special symbols    */
        ichr=60+ic+klow;
      }
      else if (ic>36) {
        /* mathematical symbols   */
        ichr=60+klow+ic   ;
        roman=1;
      }
      else if (ic>26) { 
        /* 0->9 */
        ichr=ic-26+klow   ;
        roman=1;
      }
      else if (kgreek!=0) {
        /* greek alphabet     */
        ichr=lgreek[ic-1]+36+klow   ;
      }
      else {
        /* roman alphabet     */
        ichr=ic+10+klow   ;
        roman=1;
      }
    }

    /* define scale and prepare absolute coordinates of vectors   */
    if (ichr<=0 || ichr>MAXCHR) 
      continue  ;

    if (roman) {
        if (klow==0) {
          romanchar = standard_chars[ic-1];
        } else {
          romanchar = standard_chars[ic-1] + 32;
        }
        hc = &hfd->character[romanchar];
    }


    if (!(kspec==0)) {
      /* redefine scale for special symbols */
      iw=istart[ichr-1];
      kymin = 100;
      kymax = -100;
      while ((ky=iy[iw])!=127) {
        if ((kx=ix[iw])!=127) {
          if (ky>kymax)
            kymax=ky;
          if (ky<kymin)
            kymin=ky;
        }
        iw = iw+1;
      }
      iy0=kymax;
      dky=kymax-kymin;
      if (dky==0)dky=21;
      scale=size/dky;
      rscale=scale*change_scale;
    }
    
    if (roman) {
      wdth=rscale*(hc->width) * 1.05;
      ix0=0;
    } else {
      iw=istart[ichr-1];
      kx=ix[iw-1];
      ky=iy[iw-1];
      wdth=rscale*(ky-kx) * 1.05;
      ix0=kx;
    }
    wtot=wtot+wdth;
    
    xw=wdth*co;
    yw=wdth*si;  
#ifdef DEBUG
    fprintf(stderr, "psymbol: adding contributions of %c  xpp, ypp=%le, %le  rscale=%le\n", ktext[ichar],
            xpp, ypp, rscale);
#endif

    if (roman) {
      int i;
      kxMin=LONG_MAX;
      for (i=0; i<hc->nlines; i++) {
        for (iw=0; iw<hc->line[i].ncoords; iw++) {
          if (hc->line[i].x[iw] < kxMin) {
            kxMin = hc->line[i].x[iw];
          }
        }
      }
    } else {
      if (!(ichr==63 || ichr==168)) {
        /* Find minimum kx used in the character so that we can subtract it
         * out.  It results in offsets of characters that make precise centering
         * impossible.
         */
        kxMin=LONG_MAX;
        do {
          iw += 1;
          kx=ix[iw-1] ;
          if (kx!=127) {
            kx=kx-ix0 ;
            if (kx && kx<kxMin)
              kxMin = kx;
            continue;
          }
          ky=iy[iw-1] ;
        } while (ky!=127);
      } else {
        kxMin = 0;
      }
    }    

    if (roman) {
      int i;
      for (i=0; i<hc->nlines; i++) {
        for (iw=0; iw<hc->line[i].ncoords; iw++) {
          kx = hc->line[i].x[iw];
          ky = hc->line[i].y[iw];
          kx=kx-ix0;
          //ky=iy0-ky;
          xpk = (kx-kxMin)*rscale;
          ypk = ky*rscale*aspectp;

          if (tiltp!=0) 
            xpk = xpk + ypk*tan_tiltp + (yoff+yoff2)*tan_tiltp;

          xp[iw] = xpk*co - (ypk+(yoff+yoff2)/daspect)*si + 0.5 + xpp;
          yp[iw] = xpk*si + (ypk+(yoff+yoff2)/daspect)*co + 0.5 + ypp;
#ifdef DEBUG
          fprintf(stderr, "xp, yp[%ld]: %d, %d\n",
                  iw, xp[iw], yp[iw]);
#endif
         
          if (xp[iw]>xpMax)
            xpMax = xp[iw];
          if (xp[iw]<xpMin)
            xpMin = xp[iw];
          if (yp[iw]>ypMax)
            ypMax = yp[iw];
          if (yp[iw]<ypMin)
            ypMin = yp[iw];
        }
        
        if (mode==-1) {
          dplot_lines(xp, yp, hc->line[i].ncoords, PRESET_LINETYPE);
        }
        ndraw = ndraw+1 ;
      }
    } else {
      iw=istart[ichr-1]   ;
      k=0;
      do {
        iw += 1;
        kx=ix[iw-1] ;
        ky=iy[iw-1] ;
        if (kx!=127) {
          k=k+1 ;
          kx=kx-ix0 ;
          ky=iy0-ky ;
          xpk = (kx-kxMin)*rscale;
          ypk = ky*rscale*aspectp;
          
          if (tiltp!=0) 
            xpk = xpk + ypk*tan_tiltp + (yoff+yoff2)*tan_tiltp;
          
          xp[k-1] = xpk*co - (ypk+(yoff+yoff2)/daspect)*si + 0.5 + xpp;
          yp[k-1] = xpk*si + (ypk+(yoff+yoff2)/daspect)*co + 0.5 + ypp;
#ifdef DEBUG
          fprintf(stderr, "xp, yp[%ld]: %d, %d\n",
                  k-1, xp[k-1], yp[k-1]);
#endif
          
          if (xp[k-1]>xpMax)
            xpMax = xp[k-1];
          if (xp[k-1]<xpMin)
            xpMin = xp[k-1];
          if (yp[k-1]>ypMax)
            ypMax = yp[k-1];
          if (yp[k-1]<ypMin)
            ypMin = yp[k-1];
          
          continue;
        }
        
        if (mode==-1) {
          dplot_lines(xp, yp, (long)k, PRESET_LINETYPE);
        }
        ndraw = ndraw+1 ;
        k=0   ;
      } while (ky!=127);
    }

    /* position cursor for next character     */
    xpp=xpp+xw;
    ypp=ypp+yw;
#ifdef DEBUG
    fprintf(stderr, "psymbol: added contributions of %c  xpp, ypp=%le, %le\n", ktext[ichar],
            xpp, ypp);
#endif
  }  
  if (resetDash) {
    PS_solid_dash();
  }
  extent[0] = unmap_x(xpMax)-unmap_x(xpMin);
  extent[1] = unmap_y(ypMax)-unmap_y(ypMin);
  extent[2] = unmap_x((xpMax+xpMin)/2.0)-unmap_x(xInput);
  extent[3] = unmap_y((ypMax+ypMin)/2.0)-unmap_y(yInput);

  hershey_font_free(hfd);


#ifdef DEBUG
  fprintf(stderr, "psymbol: extent: %lf (%f, %f), %lf (%f, %f) @ %lf, %lf\n", 
          extent[0], xpMin, xpMax, extent[1], ypMin, ypMax, extent[2], extent[3]);
#endif

  return(wtot);
}    

