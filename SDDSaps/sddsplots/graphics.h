/* file: graphics.h
 * contents: parts of GNUPLOT plot.h and setshow.h
 *           function prototypes and things I've added 
 * Equivalent to plot.h and setshow.h from GNUPLOT.
 * Michael Borland, 1991, 1995.
 $Log: not supported by cvs2svn $
 Revision 1.13  2005/11/04 22:46:45  soliday
 Updated code to be compiled by a 64 bit processor.

 Revision 1.12  2005/03/22 03:33:54  soliday
 Made various changes including fixing a problem with the intensity bar
 colors. Also changed png and ps default colors so they are all
 the same as motif.

 Revision 1.11  2003/11/20 16:03:05  soliday
 Added sendCoordinates to the list of device functions.

 Revision 1.10  2002/11/14 03:03:15  borland
 To fix problems with building mpl_motif:
 Moved SDDS_ReadLineColorTable from sddsplot.c to graphics.c.
 Moved LINE_COLOR_TABLE structure definition from sddsplot.h to
 graphics.h.

 Revision 1.9  2001/06/04 20:03:08  soliday
 Made some changes that only affect sddscontour.

 Revision 1.8  2000/06/07 20:08:10  soliday
 Added line_color and add_color to termentry

 Revision 1.7  2000/04/27 19:26:35  soliday
 Now support line thickness.

 Revision 1.6  1999/07/22 18:34:50  soliday
 Added WIN32 support

 Revision 1.5  1999/06/03 16:06:54  soliday
 Removed compiler warnings under linux.

 Revision 1.4  1998/12/22 16:02:35  borland
 New character scaling algorithm for all labels.  Considerable changes
 to psymbol routine that draws characters and returns the size of
 strings.

 Revision 1.3  1998/08/25 15:05:26  borland
 New version with major changes to allow multiple scales on x and y axes.

 * Revision 1.2  1995/09/05  21:09:23  saunders
 * First test release of the SDDS1.5 package.
 *
 */


#ifndef TERM
#define TERM "unknown"
#endif

#define TRUE 1
#define FALSE 0

#include <float.h>
#ifdef PC
#define VERYLARGE FLT_MAX
#else
#define VERYLARGE DBL_MAX
#endif

typedef int (*FUNC_PTR)();

#if !defined(_WIN32)
typedef int BOOLEAN;
#endif

enum JUSTIFY {
        LEFT, CENTRE, RIGHT
};

#define TERM_WINDOWS     0x00000001
#define TERM_XWINDOWS    0x00000002
#define TERM_HARDCOPY    0x00000004
#define TERM_COLOR       0x00000008
#define TERM_POLYFILL    0x00000010
#define TERM_POSTSCRIPT  0x00000020
#define TERM_NOPROMPT    0x00000040
#define TERM_IBMCLONE    0x00000080
#define TERM_FLUSHSTDOUT 0x00000100

struct termentry {
	char *name;
	char *description;
	int xmax,ymax,v_char,h_char,v_tic,h_tic;
        unsigned long flags;
        int (*init)(void);
        int (*reset)(void);
        int (*text)(void);
        int (*scale)(double xs, double ys);
        int (*graphics)(void);
        int (*move)(int x, int y);
        int (*vector)(int x, int y);
        int (*linetype)(int linetype);
        int (*put_text)(int x, int y, char *str);
        int (*text_angle)(int ang);
        int (*justify_text)(enum JUSTIFY mode);
        int (*dot)(int x, int y, int number);
        int (*arrow)(int sx, int sy, int ex, int ey);
        int (*fillbox)(int shade, int xl,  int xh, int yl, int yh);
        int (*line_thickness)(int line_thickness);
        int (*line_color)(unsigned short red, unsigned short green, unsigned short blue);
        int (*add_color)(unsigned short red, unsigned short green, unsigned short blue, short reset);
        int (*sendCoordinates)(void);
        int (*spectral)(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);
        int (*movie)(void);
};

/* Some key global variables */
extern int screen_ok;
extern int term_init;
extern struct termentry term_tbl[];

extern char *alloc();

extern FILE* outfile;

double unmap_x(double xt);
double unmap_y(double yt);


void jxyplot_string(double *x, double *y, char *s, char xmode, char ymode);

#include "SDDS.h"
typedef struct {
  long nEntries;
  int32_t *red, *green, *blue;
} LINE_COLOR_TABLE;

long SDDS_ReadLineColorTable(LINE_COLOR_TABLE *LCT, char *filename);


#define LINE_TABLE_DEFINE_COLOR 0x0001
#define LINE_TABLE_DEFINE_THICKNESS 0x0002
#define LINE_TABLE_DEFINE_DASH 0x0004
typedef struct {
   char dashArray[5];
} LINE_DASH_ARRAY;   

typedef struct {
  long nEntries;
  unsigned typeFlag;
  int32_t *red, *green, *blue;
  int32_t *thickness;
  LINE_DASH_ARRAY *dash;
}  LINE_TYPE_TABLE;

long SDDS_ReadLineTypeTable(LINE_TYPE_TABLE *LTT, char *filename);
void set_default_font(char* fontname);
char* get_default_font();
