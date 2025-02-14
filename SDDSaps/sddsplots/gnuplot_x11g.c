/*-----------------------------------------------------------------------------
 *   gnuplot_x11 - X11 outboard terminal driver for gnuplot 2
 *
 *   Requires installation of companion inboard x11 driver in gnuplot/term.c
 *
 *   Acknowledgements: 
 *      Chris Peterson (MIT) - original Xlib gnuplot support (and Xaw examples)
 *      Dana Chee (Bellcore)  - mods to original support for gnuplot 2.0
 *      Arthur Smith (Cornell) - graphical-label-widget idea (xplot)
 *      Hendri Hondorp (University of Twente, The Netherlands) - Motif xgnuplot
 *
 *   This code is provided as is and with no warranties of any kind.
 *       
 *   Ed Kubaitis - Computing Services Office -  University of Illinois, Urbana
 *---------------------------------------------------------------------------*/
 
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#if defined(SUNOS4)
#include <X11/Label.h>
#elif defined(HPUX) && defined(MOTIF)
#include <Xm/Label.h>
#else
#include "Label.h"       
#endif

#ifdef MOTIF
#include <Xm.h>             /* use -Idir for location on your system */
#define LabelWC xmLabelWidgetClass
#define LabelBPM XmNbackgroundPixmap
#else
#define LabelWC labelWidgetClass
#define LabelBPM XtNbitmap
#endif

#include "x11_limits.h"
#include "mdb.h"

#define Color (D>1)
#define Ncolors 11
FILE *outfile;
unsigned long colors[Ncolors];
char color_keys[Ncolors][30] =   { "text", "border", "axis", 
   "line1", "line2", "line3", "line4", "line5", "line6", "line7", "line8" };
char color_values[Ncolors][30] = { "black", "white", 
   "grey93", "grey86", "grey79", "grey72", "grey65", "grey58", "grey51", "grey44", "grey37"};

char dashes[10][5] = { {0}, {1,6,0}, 
   {0}, {4,2,0}, {1,3,0}, {4,4,0}, {1,5,0}, {4,4,4,1,0}, {4,2,0}, {1,3,0}
   };

#define Ngreys 101
char grey_values[Ngreys][30];
unsigned long grey[Ngreys];

Widget w_top, w_label; Window win; Display *dpy;
Pixmap pixmap;  GC gc = (GC)NULL;
Dimension W = 512 , H = 512;  int D;
Arg args[5];
static void gnuplot(), resize();

int cx=0, cy=0, vchar, nc = 0;
double xscale, yscale;
#define X(x) (Dimension) (x * xscale)
#define Y(y) (Dimension) ((X11_YMAX-1-y) * yscale)
enum JUSTIFY { LEFT, CENTRE, RIGHT } jmode;
#define Nbuf 1024
char buf[Nbuf];
String *commands = NULL;

typedef struct {       /* See "X Toolkit Intrinsics Programming Manual"      */
  XFontStruct *font;   /* Nye and O'Reilly, O'Reilly & Associates, pp. 80-85 */
  unsigned long fg;
  unsigned long bg;
  } RValues, *RVptr; 
RValues rv;

XtResource resources[] = {
   { XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *), 
     XtOffset(RVptr, font), XtRString, "fixed" },
   { XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel), 
     XtOffset(RVptr, fg), XtRString, XtDefaultForeground },
   { XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel), 
     XtOffset(RVptr, bg), XtRString, XtDefaultBackground },
   };

/*-----------------------------------------------------------------------------
 *   main program - fire up application and callbacks
 *---------------------------------------------------------------------------*/

int main(argc, argv) int argc; char *argv[]; {

   signal(SIGINT, SIG_IGN);
   signal(SIGTSTP, SIG_IGN);

   /* initialize application */
   w_top = XtInitialize("gnuplot", "Gnuplot", NULL, 0, &argc, argv);
   XtSetArg(args[0], XtNwidth, W);
   XtSetArg(args[1], XtNheight, H);
   w_label = XtCreateManagedWidget ("", LabelWC, w_top, args, (Cardinal)2);
   XtRealizeWidget(w_top);

   /* extract needed information */
   dpy = XtDisplay(w_top); win = XtWindow(w_label);
   D = DisplayPlanes(dpy,DefaultScreen(dpy));
   if (Color) {
      char option[20], *value; 
      XColor used, exact; int n;

      for(n=0; n<Ncolors; n++) {
	 strcpy_ss(option, color_keys[n]);
	 strcat(option, "Color");
	 value = XGetDefault(dpy, "gnuplot", option);
	 if (!value) { value = color_values[n]; }
	 if (XAllocNamedColor(dpy, DefaultColormap(dpy,0), value, &used,&exact))
	    colors[n] = used.pixel; 
	 else {
	    fprintf(stderr, "gnuplot: cannot allocate %s:%s\n", option, value);
	    fprintf(stderr, "gnuplot: assuming %s:black\n", option);
	    colors[n] = BlackPixel(dpy,0);
	    }
	 }
      for (n=0; n<Ngreys; n++) {
          sprintf(grey_values[n], "grey%d", n);
          value = grey_values[n];
          if (XAllocNamedColor(dpy, DefaultColormap(dpy,0), value, &used,&exact))
              grey[n] = used.pixel; 
          else {
              fprintf(stderr, "gnuplot: cannot allocate %s\n", value);
              grey[n] = BlackPixel(dpy,0);
              }
          }
      }
   XtSetArg(args[0], XtNwidth, &W);
   XtSetArg(args[1], XtNheight,&H);
   XtGetValues(w_label, args, (Cardinal)2);
   XtGetApplicationResources(w_top, &rv, resources, XtNumber(resources),NULL,0);
   vchar = (rv.font->ascent + rv.font->descent);

   /* add callbacks on input-from-gnuplot-on-stdin & window-resized */
   XtAddInput(0, (XtPointer)XtInputReadMask, gnuplot, NULL);
   XtAddEventHandler(w_label, StructureNotifyMask, FALSE, resize, NULL);

   XtMainLoop();
   return 0;
   }

static int line_style = LineSolid;
/*-----------------------------------------------------------------------------
 *   display - display accumulated commands from inboard driver
 *---------------------------------------------------------------------------*/

int display() {
   int n, x, y, sw, sl, lt, width, type;
   char *buf, *str;

   /* set scaling factor between internal driver & window geometry */
   xscale = (double)W / X11_XMAX;  yscale = (double)H / X11_YMAX;

   /* create new pixmap & GC */
   if (gc) { XFreeGC(dpy, gc); XFreePixmap(dpy, pixmap); }
   pixmap = XCreatePixmap(dpy, RootWindow(dpy,DefaultScreen(dpy)), W, H, D);
   gc = XCreateGC(dpy, pixmap, 0, NULL);
   XSetFont(dpy, gc, rv.font->fid);

   /* erase pixmap */
#ifndef MOTIF
   if (Color) { /* Athena needs different erase for color and mono */
#endif
      XSetForeground(dpy, gc, colors[0]);
      XFillRectangle(dpy, pixmap, gc, 0, 0, W, H);
      XSetForeground(dpy, gc, rv.fg);
      XSetBackground(dpy, gc, rv.bg);
#ifndef MOTIF
      }
   else {  
      XSetFunction(dpy, gc, GXxor);
      XCopyArea(dpy, pixmap, pixmap, gc, 0, 0, W, H, 0, 0);
      XSetFunction(dpy, gc, GXcopyInverted);
      }
#endif

   /* connect new pixmap to label widget */
   XtSetArg(args[0], LabelBPM, pixmap);
   XtSetValues(w_label, args, (Cardinal)1);

   XSetForeground(dpy, gc, colors[0]);

   /* loop over accumulated commands from inboard driver */
   for (n=0; n<nc; n++) {
      buf = commands[n];

      /*   X11_vector(x,y) - draw vector  */
      if (*buf == 'V') { 
	 sscanf(buf, "V%4d%4d", &x, &y);  
	 XDrawLine(dpy, pixmap, gc, X(cx), Y(cy), X(x), Y(y));
	 cx = x; cy = y;
	 }

      /*   X11_move(x,y) - move  */
      else if (*buf == 'M') 
	 sscanf(buf, "M%4d%4d", &cx, &cy);  
      
      /* X11_dot(x, y) -- dot */
      else if (*buf == 'P') {
          sscanf(buf, "P%4d%4d", &cx, &cy);
          XDrawPoint(dpy, pixmap, gc, X(cx), Y(cy));
          }

      /*   X11_put_text(x,y,str) - draw text   */
      else if (*buf == 'T') { 
	 sscanf(buf, "T%4d%4d", &x, &y);  
	 str = buf + 9; sl = strlen(str) - 1;
	 sw = XTextWidth(rv.font, str, sl);
	 switch(jmode) {
	    case LEFT:   sw = 0;     break;
	    case CENTRE: sw = -sw/2; break;
	    case RIGHT:  sw = -sw;   break;
	    }
	 if (!Color) 
	    XDrawString(dpy, pixmap, gc, X(x)+sw, Y(y)+vchar/3, str, sl);
	 else { 
	    XSetForeground(dpy, gc, colors[0]);
	    XDrawString(dpy, pixmap, gc, X(x)+sw, Y(y)+vchar/3, str, sl);
	    XSetForeground(dpy, gc, colors[lt+1]);
	    }
	 }

      /*   X11_justify_text(mode) - set text justification mode  */
      else if (*buf == 'J') 
	 sscanf(buf, "J%4d", &jmode);

      /*   X11_linetype(type) - set line type  */
      else if (*buf == 'L') { 
	 sscanf(buf, "L%4d", &lt);
	 lt = lt%10 + 1;
	 if (Color) {
             type = LineSolid;
            width = 1;
	    XSetForeground(dpy, gc, colors[lt]);
	    }
	 else {
	    type  = (lt == 0 || lt == 2) ? LineSolid : LineOnOffDash;
	    width = 0;
	    if (dashes[lt][0])
	       XSetDashes(dpy, gc, 0, dashes[lt], strlen(dashes[lt]));
	    }
	 XSetLineAttributes( dpy,gc, width, type, CapButt, JoinBevel);
	 line_style = type;
	 }
      /*   X11_line_thickness(thickness) - set line width  */
      else if (*buf == 'W') {
	 sscanf(buf, "W%4d", &lt);
	 if (!Color)
	   lt--;
	 XSetLineAttributes( dpy,gc, lt, line_style, CapButt, JoinBevel);
      }

      /* Fill Box */
      else if (*buf=='B') {
          int shade, xl, xh, yl, yh;
          /* filled box */
          sscanf(buf, "B%04d%04d%04d%04d%04d", 
                 &shade, &xl, &xh, &yl, &yh);
          shade = shade%101;
          xl = X(xl); yl = Y(yl);
          xh = X(xh); yh = Y(yh);
          XSetForeground(dpy, gc, grey[shade]);
          XFillRectangle(dpy, pixmap, gc, xl, yh, xh-xl, yl-yh);
          XSetForeground(dpy, gc, colors[lt+1]);
          }

      }

   /* trigger expose events to display pixmap */
   XClearArea(dpy, win, 0, 0, 0, 0, TRUE);
   return 0;
   }

/*-----------------------------------------------------------------------------
 *   gnuplot - Xt callback on input from gnuplot inboard X11 driver
 *   resize - Xt callback when window resized
 *---------------------------------------------------------------------------*/

static void
gnuplot(cd, s, id) char *cd; int *s; XtInputId *id; {
   int i, nc_max, dnc, len;

   nc_max = 0;
   dnc = 8192;
#define STRING_LENGTH 25

   while (fgets(buf, Nbuf, stdin)) {
     if (buf[(len=strlen(buf))-1]=='\n')
         buf[len--] = 0;
     if (*buf == 'G') {                           /* enter graphics mode */
	 if (commands) {
	    int n; for (n=0; n<nc; n++) XtFree(commands[n]);
	    XtFree((char*)commands);
	    }
	 commands = NULL; nc = 0;
         }
      else if (*buf == 'E') { display(); break; } /* leave graphics mode */
      else if (*buf == 'R') { exit(0); }          /* leave X11/x11 mode  */
      else {
         if (nc_max<=nc) {
             char *ptr; 
            
             commands = (String *)XtRealloc((char*)commands, (nc_max+=dnc) * sizeof(String));
             ptr = (char *)malloc(sizeof(*ptr)*dnc*(STRING_LENGTH+1));
             for (i=nc; i<nc_max; i++)
                 commands[i] = ptr + (i-nc)*(STRING_LENGTH+1);
             }         
         if (len>STRING_LENGTH)
             commands[nc++] = XtNewString(buf);
         else
             strcpy_ss(commands[nc++], buf);
	 }
      }
   if (feof(stdin) || ferror(stdin)) exit(0);
   }

static void
resize(w, cd, e) Widget w; char *cd; XConfigureEvent *e; {
   if (e->type != ConfigureNotify) return;
   W = e->width; H = e->height;
   display(); 
   }

