/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * xwdfunc.c *** Functions to Dump a Window in Xwd Format *

 * Extracted from xwd.c on 1-6-92 and modified * 
 * Routine to be used is windowdump()          * 
 *   Dumps a Drawable as Xwd dumps a window    *
 * $Log: not supported by cvs2svn $
 * Revision 1.5  2000/05/16 19:46:08  soliday
 * Cleaned up some Solaris compiler warnings.
 *
 * Revision 1.4  1999/07/22 18:37:42  soliday
 * Added WIN32 support
 *
 * Revision 1.3  1999/06/03 16:11:25  soliday
 * Removed compiler warnings under linux.
 *
   Revision 1.2  1995/09/05 21:10:32  saunders
   First test release of the SDDS1.5 package.
   */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
/* #include <X11/Xmu/WinUtil.h> */
/* #include "dsimple.h" */
#include "X11/XWDFile.h"
#include "mpl_motif.h"

#define FEEP_VOLUME 0
#define lowbit(x) ((x) & (~(x) + 1))

typedef unsigned long Pixel;     /* Probably as defined in XtIntrinsics */

/* Function Prototypes */

void windowdump(Window window, char *title, Drawable drawable,
  int width, int height, FILE *out);
int Image_Size(XImage *image);
int Get_XColors(XWindowAttributes *win_info, XColor **colors);
void _swapshort(register char *bp, register unsigned n);
void _swaplong(register char *bp, register unsigned n);

/* Global Variables */

static int format = ZPixmap;
static char program_name[]="windowdump";

extern Display *display;
extern int screen;
extern char *string;

/**************************** windowdump **********************************/
void windowdump(Window window, char *title, Drawable drawable,
  int width, int height, FILE *out)
/* Window window:      Used for colormap and other information in header */
/* char *title:        Will appear as the window title with Xwud */
/* Drawable drawable:  Drawable to be dumped to the file */
/* int width,height:   Width and height of Drawable (Starts at (0,0) */
/* FILE *out:          File to which to write (Must be open, Will be closed) */
{
    unsigned long swaptest = 1;
    XColor *colors=NULL;
    unsigned buffer_size;
    int title_size;
    int header_size;
    int ncolors, i;
    XWindowAttributes win_info;
    XImage *image;
    int absx=width,absy=height,xstart=0,ystart=0;
    XWDFileHeader header;

/* Get the parameters of the window being dumped */
    if(!XGetWindowAttributes(display, window, &win_info)) 
      xerrmsg(1,"Cannot get dump window attributes");
/* sizeof(char) is included for the null string terminator. ??? */
    title_size = strlen(title) + sizeof(char);
/* Snarf the pixmap with XGetImage */
    image=XGetImage(display,drawable,xstart,ystart,width,height,
		    AllPlanes,format);
    if(!image) {
	sprintf(string,"%s:  Unable to get image at %dx%d+%d+%d",
		 program_name,width,height,xstart,ystart);
	xerrmsg(1,string);
	fclose(out);
	return;
    }
/* Determine the pixmap size */
    buffer_size = Image_Size(image);
    ncolors = Get_XColors(&win_info, &colors);
/* Calculate header size */
    header_size = sizeof(header) + title_size;
/* Write out header information */
    header.header_size = (CARD32) header_size;
    header.file_version = (CARD32) XWD_FILE_VERSION;
    header.pixmap_format = (CARD32) format;
    header.pixmap_depth = (CARD32) image->depth;
    header.pixmap_width = (CARD32) image->width;
    header.pixmap_height = (CARD32) image->height;
    header.xoffset = (CARD32) image->xoffset;
    header.byte_order = (CARD32) image->byte_order;
    header.bitmap_unit = (CARD32) image->bitmap_unit;
    header.bitmap_bit_order = (CARD32) image->bitmap_bit_order;
    header.bitmap_pad = (CARD32) image->bitmap_pad;
    header.bits_per_pixel = (CARD32) image->bits_per_pixel;
    header.bytes_per_line = (CARD32) image->bytes_per_line;
    header.visual_class = (CARD32) win_info.visual->class;
    header.red_mask = (CARD32) win_info.visual->red_mask;
    header.green_mask = (CARD32) win_info.visual->green_mask;
    header.blue_mask = (CARD32) win_info.visual->blue_mask;
    header.bits_per_rgb = (CARD32) win_info.visual->bits_per_rgb;
    header.colormap_entries = (CARD32) win_info.visual->map_entries;
    header.ncolors = ncolors;
    header.window_width = (CARD32) win_info.width;
    header.window_height = (CARD32) win_info.height;
    header.window_x = absx;
    header.window_y = absy;
    header.window_bdrwidth = (CARD32) win_info.border_width;
/* Change bit and byte order */
    if (*(char *) &swaptest) {
	_swaplong((char *) &header, sizeof(header));
	for (i = 0; i < ncolors; i++) {
	    _swaplong((char *) &colors[i].pixel, sizeof(long));
	    _swapshort((char *) &colors[i].red, 3 * sizeof(short));
	}
    }
/* Write header to file */
    (void) fwrite((char *)&header, sizeof(header), 1, out);
    (void) fwrite(title, title_size, 1, out);
/* Write out the color maps, if any */
    (void) fwrite((char *) colors, sizeof(XColor), ncolors, out);
/* Write out the buffer */
    (void) fwrite(image->data, (int) buffer_size, 1, out);
/* Free things */
    fclose(out);
    if(ncolors > 0) free(colors);
    XDestroyImage(image);
}
/**************************** Image_Size **********************************/
int Image_Size(XImage *image)
{
    if (format != ZPixmap)
    return(image->bytes_per_line * image->height * image->depth);
    return(image->bytes_per_line * image->height);
}
/**************************** Get_XColors *********************************/
int Get_XColors(XWindowAttributes *win_info, XColor **colors)
/* Gets the XColors of all pixels in image - returns # of colors */
{
    int i, ncolors;
    
    if (!win_info->colormap) return(0);

    if (win_info->visual->class == TrueColor)
	return(0);    /* colormap is not needed */

    ncolors = win_info->visual->map_entries;
    if (!(*colors = (XColor *) malloc (sizeof(XColor) * ncolors)))
      xerrmsg(1,"Cannot allocate all needed space for colors");
    
    if (win_info->visual->class == DirectColor) {
	Pixel red, green, blue, red1, green1, blue1;
	
	red = green = blue = 0;
	red1 = lowbit(win_info->visual->red_mask);
	green1 = lowbit(win_info->visual->green_mask);
	blue1 = lowbit(win_info->visual->blue_mask);
	for (i=0; i<ncolors; i++) {
	    (*colors)[i].pixel = red|green|blue;
	    (*colors)[i].pad = 0;
	    red += red1;
	    if (red > win_info->visual->red_mask)
	    red = 0;
	    green += green1;
	    if (green > win_info->visual->green_mask)
	    green = 0;
	    blue += blue1;
	    if (blue > win_info->visual->blue_mask)
	    blue = 0;
	}
    } else {
	for (i=0; i<ncolors; i++) {
	    (*colors)[i].pixel = i;
	    (*colors)[i].pad = 0;
	}
    }
    XQueryColors(display, win_info->colormap, *colors, ncolors);
    return(ncolors);
}
/**************************** _swapshort **********************************/
void _swapshort(register char *bp, register unsigned n)
{
    register char c;
    register char *ep = bp + n;
    
    while (bp < ep) {
	c = *bp;
	*bp = *(bp + 1);
	bp++;
	*bp++ = c;
    }
}
/**************************** _swaplong ***********************************/
void _swaplong(register char *bp, register unsigned n)
{
    register char c;
    register char *ep = bp + n;
    register char *sp;
    
    while (bp < ep) {
	sp = bp + 3;
	c = *sp;
	*sp = *bp;
	*bp++ = c;
	sp = bp + 1;
	c = *sp;
	*sp = *bp;
	*bp++ = c;
	bp += 2;
    }
}
