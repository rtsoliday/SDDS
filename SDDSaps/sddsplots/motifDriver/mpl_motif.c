/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\*************************************************************************/

/*-----------------------------------------------------------------------------
 *   mpl_motif -- Motif outboard terminal driver for MPL
 *
 *   Requires installation of companion inboard Motif driver
 *
 *   Acknowledgements:
 *      Chris Peterson (MIT) - original Xlib gnuplot support (and Xaw examples)
 *      Dana Chee (Bellcore)  - mods to original support for gnuplot 2.0
 *      Arthur Smith (Cornell) - graphical-label-widget idea (xplot)
 *      Hendri Hondorp (University of Twente, The Netherlands) - Motif xgnuplot
 *      Ed Kubaitis - (Illinois) - original Gnuplot X11 driver
 *
 *   This code is provided as is and with no warranties of any kind.
 *
 *   Modified from Gnuplot X11 version by K. Evans
 *---------------------------------------------------------------------------*/
#if defined(_WIN32)
#  if defined(__BORLANDC__)
#    include "hclsdds.h"
#  endif
#  include <io.h>
#  include <fcntl.h>
#  if defined(__BORLANDC__)
#    include <string.h>
#    define strcasecmp(s1, s2) stricmp(s1, s2)
#    define _setmode(handle, amode) setmode(handle, amode)
#  else
#    define strcasecmp(s1, s2) _stricmp(s1, s2)
#  endif
#  define popen(command, type) _popen(command, type)
#  define pclose(stream) _pclose(stream)
#endif

#include <X11/Intrinsic.h>
#if defined(_WIN32)
#  include <X11/XlibXtra.h>
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#endif
#include <X11/StringDefs.h>
#include <X11/Xresource.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#if !defined(_WIN32)
#  include <unistd.h>
#  include <sys/errno.h>
#  include <sys/stat.h>
#endif
#include <fcntl.h>
#include <signal.h>

#include <string.h>
#include <math.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/ArrowB.h>
#include <Xm/ArrowBG.h>
#include <Xm/CascadeB.h>
#include <Xm/CascadeBG.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/MainW.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrolledW.h>
#include <Xm/SelectioB.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/ToggleBG.h>
#include <x11_limits.h>
#include <ps_limits.h>
#include <mif_limits.h>
#include "../graph.h"
#include "mpl_motif.h"
#include "resources.h"
#include "mdb.h"

/*#include "../graphics.h"
  #include "../term.h"*/

#define VTYPE short
#define PRINTCOMMAND "lpr"
#define PRINTFILENAME "mpl.ps"
#define DUMPCOMMAND "xwd2ps -L | lpr"
#if defined(_WIN32)
#  define SAVEFILENAME "~/mpl.mif"
#  define PRINTFILEBUFFER ".mpl"
#else
#  define SAVEFILENAME "~/mpl.mif"
#endif
#define DUMPFILENAME "mpl.xwd"
#define DUMPTITLE "MPL Screen Dump"
#define MAXARGS 10
#define WIDTH 750
#define HEIGHT 580
#define NBUF 1024
#define DNC 65536
#define COLORNAMESIZE 32
#undef MTRACKEXTRAS
#define MTRACKPRECISION 4
#define MTRACKMAXPRECISION 10
#define MTRACKVALUESIZE (MTRACKMAXPRECISION + 8)
#define MTRACKFORMATSIZE 32
#define NCOLORS 18
#define NSPECT 101
#define ZOOMFAC .75
#define MOVEFAC .5
#define VERSION "MPL Motif Outboard Driver 1.6"

/* #define XMAX (X11_XMAX-1) */
/* #define YMAX (X11_YMAX-1) */
#define XMAX X11_XMAX
#define YMAX X11_YMAX
#define X(x) (int)((x) * scalex - offsetx)
#define Y(y) (int)((YMAX - (y)) * scaley - offsety)
#define X0(x) (((double)(x) + offsetx0) / scalex)
#define Y0(y) ((double)YMAX - ((double)(y) + offsety0) / scaley)
#define MTRACKX(x0) ((x0) * userax + userbx)
#define MTRACKY(y0) ((y0) * useray + userby)
#define PNG_XMAX 656
#define PNG_YMAX 506
#define MPNG_XMAX ((long)(PNG_XMAX / 0.80))
#define MPNG_YMAX ((long)(PNG_YMAX / 0.80))
#define LPNG_XMAX ((long)(MPNG_XMAX / 0.75))
#define LPNG_YMAX ((long)(MPNG_YMAX / 0.75))
#define HPNG_XMAX ((long)(LPNG_XMAX * 2))
#define HPNG_YMAX ((long)(LPNG_YMAX * 2))
#define GPNG_XMAX ((long)(HPNG_XMAX * 2))
#define GPNG_YMAX ((long)(HPNG_YMAX * 2))

#ifdef XREL5
#  include <X11/Xmu/Editres.h>
#endif

/* Function prototypes */

int main(int argc, char **argv);
int addspectrumcolors(void);
int alloccolors(void);
int allocspectrum(void);
void choosecolors(void);
void colorcb(Widget w, XtPointer clientdata, XtPointer calldata);
void displayplot(Widget w, caddr_t clientdata, caddr_t calldata);
void dump(void);
int expandname(char *oldname, char *newname, int n);
void eventgraph(Widget w, XtPointer clientdata, XEvent *event,
                Boolean *contdispatch);
void eventmtrack(Widget w, XtPointer clientdata, XEvent *event,
                 Boolean *contdispatch);
void expose(Widget w, XtPointer clientdata, XtPointer calldata);
void gnuplot(XtPointer cd, int *s, XtInputId *id);
struct PLOTREC *makeplotrec(void);
void menucb(Widget w, XtPointer clientdata, XtPointer calldata);
void mifprint(int save);
void pngprint(int save);
void movie(void);
void mtracksetup(void);
void zoomfactorcb(Widget w, XtPointer clientdata, XtPointer calldata);
void navigatecb(Widget w, XtPointer clientdata, XtPointer calldata);
void placecb(Widget w, XtPointer clientdata, XtPointer calldata);
void psprint(int save);
void mp4print(int save);
void printcb(Widget w, XtPointer clientdata, XtPointer calldata);
void printsetup(void);
void quitplot(Widget w, XtPointer clientdata, XtPointer calldata);
void readprinttext(void);
void resize(Widget w, XtPointer clientdata, XtPointer calldata);
void zoominout(Widget w, XtPointer clientdata, XtPointer calldata);
void restore(void);
void restore_plot(void);
void rubberband(Window window, Position *xstart, Position *ystart,
                Position *xend, Position *yend);
int setprivatecolormap(void);
void sigpipe(int signum);
void status(void);
void xinfomsg(char *fmt, ...);
void destroyplotrec(struct PLOTREC *rec);
void zoom(double x0, double y0, double x1, double y1);
void zoomcb(Widget w, XtPointer clientdata, XtPointer calldata);
void newzoom(double x0, double y0, double x1, double y1);
char *get_token_t(char *s, char *t);
char *str_tolower(char *s);
char *trim_spaces(char *s);
long is_blank(char *s);
char *tmpname(char *s);
/*int fexists(char *s);*/
int destroyallplotrec(void);
void gnuplot_new(XtPointer cd, int *s, XtInputId *id);
void restore_new(void);
struct COORDREC *makecoordrec(void);
void destroycoordrecs(void);

void windowdump(Window window, char *title, Drawable drawable,
                int width, int height, FILE *out);
void writeCommandToEmacsServer();
void writeImageToEmacsServer();

/* Structures */
struct COORDREC {
  double x0;
  double x1;
  double y0;
  double y1;
  int ncoord;
  struct COORDREC *next;
  struct COORDREC *prev;
};

struct PLOTREC {
  int nplot;
  int nc;
  char *buffer;
  struct PLOTREC *next;
  struct PLOTREC *prev;
};

#define Color (nplanes > 1)
int UseDashes = 0;
unsigned long colors[NCOLORS], colors_orig[NCOLORS], colorsalloc[NCOLORS];
char color_keys[NCOLORS][7] =
  {"bg", "fg", "line0",
   "line1", "line2", "line3", "line4", "line5", "line6", "line7", "line8",
   "line9", "line10", "line11", "line12", "line13", "line14", "line15"};
char color_options[NCOLORS][13] =
  {"-bgColor", "-fgColor", "-line0Color",
   "-line1Color", "-line2Color", "-line3Color", "-line4Color",
   "-line5Color", "-line6Color", "-line7Color", "-line8Color",
   "-line9Color", "-line10Color", "-line11Color", "-line12Color",
   "-line13Color", "-line14Color", "-line15Color"};
char color_specifiers[NCOLORS][13] =
  {".bgColor", ".fgColor", ".line0Color",
   ".line1Color", ".line2Color", ".line3Color", ".line4Color",
   ".line5Color", ".line6Color", ".line7Color", ".line8Color",
   ".line9Color", ".line10Color", ".line11Color", ".line12Color",
   ".line13Color", ".line14Color", ".line15Color"};
char color_values[NCOLORS][COLORNAMESIZE] =
  {"Black", "White", "White",
   "Red", "Blue", "Green", "Yellow", "Magenta", "Cyan", "LimeGreen", "Gold",
   "Orange", "HotPink", "DeepSkyBlue", "MediumSpringGreen", "Tomato", "Tan",
   "Grey75"};

typedef struct
{
  XFontStruct *font;
  Boolean doublebuffer;
  Boolean greyscale;
  Boolean movie;
  Boolean spectrum;
  Boolean newZoom;
} RValues, *RVptr;
RValues rv;

XtResource resources[] = {
  {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
   XtOffset(RVptr, font), XtRString, "fixed"},
  {"doublebuffer", "Doublebuffer", XtRBoolean, sizeof(Boolean),
   XtOffset(RVptr, doublebuffer), XtRString, "TRUE"},
  {"greyscale", "Greyscale", XtRBoolean, sizeof(Boolean),
   XtOffset(RVptr, greyscale), XtRString, "FALSE"},
  {"movie", "Movie", XtRBoolean, sizeof(Boolean),
   XtOffset(RVptr, movie), XtRString, "FALSE"},
  {"spectrum", "Spectrum", XtRBoolean, sizeof(Boolean),
   XtOffset(RVptr, spectrum), XtRString, "FALSE"},
  {"newzoom", "newZoom", XtRBoolean, sizeof(Boolean),
   XtOffset(RVptr, newZoom), XtRString, "FALSE"},
};

#define EXTRAOPTIONS 6
XrmOptionDescRec options[NCOLORS + EXTRAOPTIONS] = {
  {"-doublebuffer", ".doublebuffer", XrmoptionSepArg, "TRUE"},
  {"-greyscale", ".greyscale", XrmoptionSepArg, "FALSE"},
  {"-movie", ".movie", XrmoptionSepArg, "FALSE"},
  {"-spectrum", ".spectrum", XrmoptionSepArg, "FALSE"},
  {"-newzoom", ".newZoom", XrmoptionSepArg, "FALSE"},
  {"-zoomFactor", ".zoomFactor", XrmoptionSepArg, "20"},
};

char dashes[10][5] = {{0},
                      {4, 2, 0},
                      {2, 3, 0},
                      {1, 2, 0},
                      {5, 2, 1, 2, 0},
                      {3, 3, 1, 4, 0},
                      {2, 5, 0},
                      {4, 4, 4, 1, 0},
                      {8, 2, 0},
                      {1, 4, 0}};

XColor xcol;
unsigned long spectrum[NSPECT];
short spectrumalloc[NSPECT];
Colormap cmap;
Visual *defvisual;
int privatecolormap = 0;
Pixel black, white;
Window win;
Pixmap pixmap = (Pixmap)NULL;
GC gc = (GC)NULL;
Dimension W = WIDTH, H = HEIGHT;
int spectral = 0;
int customspectral = 0;
int nspect = 101;

unsigned short red0 = 0, green0 = 0, blue0 = 0, red1 = 65535, green1 = 65535, blue1 = 65535;

VTYPE cx = 0, cy = 0;
int vchar;
double scalex, scaley, x0old, y0old, x1old, y1old, deltax, deltay;
double offsetx, offsety, offsetx0, offsety0;

int screen, nplanes;
XtAppContext appcontext;
#if defined(_WIN32)
XtWorkProcId workprocid = (XtWorkProcId)NULL;
#else
XtInputId inputid = (XtInputId)NULL;
#endif
Display *display;
Widget appshell, mainwin, mainmenu, mainform, warningBox;
Widget grapharea, statusbar, warningbox, infobox;
Widget colorpopup, printpopup = (Widget)NULL, mtrackpopup = (Widget)NULL, zoomfactorpopup = (Widget)NULL;
Widget mtrackxw, mtrackyw, mtrackerrxw, mtrackerryw;
Widget mtrackxrelw, mtrackyrelw, mtrackxrefw, mtrackyrefw;
#ifdef MTRACKEXTRAS
Widget mtrackxpw, mtrackypw, mtrackx0w, mtracky0w;
#endif
Widget mtrackprecisionw, mtrackformattypew, zoomfactorw;
Widget wprintcommand, wprintfilename, wsavefilename, wdumpcommand, wdumpfilename;
Widget spectrumbox, helppopup;
Widget newZoomToggleButton;
Window mainwindow, graphareawindow;
Cursor crosshair, watch;
Pixmap apspix;
Arg args[MAXARGS];
Cardinal nargs;

char mtrackformat[MTRACKFORMATSIZE];
char mtrackformats[3][2] = {"f", "e", "g"};
int mtrackprecision = MTRACKPRECISION, mtrackformattype = 0;
int mtrackon = 0;
double mtrackx = 0., mtracky = 0., mtrackxref = 0., mtrackyref = 0.;
double mtrackxrel = 0., mtrackyrel = 0.;

char string[BUFSIZ];
XmString cstring;
int windowmessage = 0, doublebuffer = 1, killparent = 0, spectrumallocated = 0;
int addspectrum = 0;
int greyscale = 0, grouplines = 0, domovie = 0, dospectrum = 1;
Widget wselected = (Widget)NULL, wres[NCOLORS];
int linecolormax = 0;

FILE *outfile;
char printcommand[BUFSIZ] = PRINTCOMMAND;
char printfilename[BUFSIZ] = PRINTFILENAME;
char savefilename[BUFSIZ] = SAVEFILENAME;
#if defined(_WIN32)
char printfilebuffer[BUFSIZ] = PRINTFILEBUFFER;
#endif
int printcolor = 0, printborder = 0, printlandscape = 1;
int printtofile = 0, printtype = 1;
int savecolor = 0, saveborder = 0, savelandscape = 1;
int savetype = 3;
int confirmfile = 1;
char dumpcommand[BUFSIZ] = DUMPCOMMAND;
char dumpfilename[BUFSIZ] = DUMPFILENAME;
int dumptofile = 0;

struct PLOTREC *last = (struct PLOTREC *)NULL, *cur = (struct PLOTREC *)NULL;
struct PLOTREC *curwrite = (struct PLOTREC *)NULL;
int nplots = 0;
struct COORDREC *curcoord = (struct COORDREC *)NULL, *lastcoord = (struct COORDREC *)NULL;
struct COORDREC *usecoord = (struct COORDREC *)NULL;
int ncoords = 0, usecoordn = 0;

int placetop = 40;
int placebottom = 20;
int placeleft = 20;
int placeright = 20;
int placehspace = 30;
int placevspace = 45;

#include "../graphics.h"
extern char *LineTableFile;
extern LINE_TYPE_TABLE lineTypeTable;
char *sddsplotCommandline2 = NULL;
int newZoom = 0;
double zoomFactor = 0.2;
FILE *ifp = NULL;
char tmpZoomFilename[256];
int currentPlot = 1;

int keep = 0;
double timeoutHours = 8.;
int timeoutSec = 8 * 60 * 60;

double movieIntervalTime = 0.1;

double userax = 0., userbx = 0., useray = 0., userby = 0.;

#include "aps.icon"
#include "mpl_motif.hlp"

void change_extension(char *filename, size_t buffer_size, char *new_extension) {
  size_t filename_len = strlen(filename);
  size_t new_extension_len = strlen(new_extension);
  char *dot_position = NULL;

  // Find the last occurrence of '.'
  for (size_t i = filename_len; i > 0; i--) {
    if (filename[i - 1] == '.') {
      dot_position = &filename[i - 1];
      break;
    }
    // Stop if we encounter a directory separator (Unix or Windows)
    if (filename[i - 1] == '/' || filename[i - 1] == '\\') {
      break;
    }
  }

  // Calculate new length after replacing or adding the extension
  size_t base_name_len = dot_position ? (size_t)(dot_position - filename) : filename_len;
  size_t new_filename_len = base_name_len + new_extension_len;

  // Check if new filename fits in the buffer
  if (new_filename_len + 1 > buffer_size) { // +1 for null terminator
    return;
  }

  // Terminate the string at the position of the dot (if any)
  if (dot_position) {
    filename[base_name_len] = '\0';
  }

  // Append the new extension
  strcat(filename, new_extension);
}

void got_timeout_alarm(int z) {
  exit(0);
}
/**************************** main ****************************************/
int main(int argc, char **argv) {
  Widget wparent, wparentsav, w, w1;
  int i, n;
  char *value;
  lineTypeTable.nEntries = 0;
  lineTypeTable.typeFlag = 0x0000;
  char *tmp;

#if !defined(_WIN32)
  struct sigaction sv = {0};
#endif

#if defined(_WIN32)
  HCLXmInit();
#endif
#if !defined(_WIN32)
  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGPIPE, sigpipe);
  sv.sa_handler = got_timeout_alarm;
  sigaction(SIGALRM, &sv, NULL);
#endif

  /* Fill out the options description */
  for (i = 0; i < NCOLORS; i++) {
    n = i + EXTRAOPTIONS;
    options[n].option = color_options[i];
    options[n].specifier = color_specifiers[i];
    options[n].argKind = XrmoptionSepArg;
    options[n].value = (caddr_t)color_values[i];
  }

  /* Set up top level and get remaining command-line arguments */
  XtToolkitInitialize();
  appcontext = XtCreateApplicationContext();
  XtAppSetFallbackResources(appcontext, fallbackResources);
  /*  display=XtOpenDisplay(appcontext,NULL,"mpl_motif","MPLDriver",
      options,XtNumber(options),&argc,argv);*/
  display = XtOpenDisplay(appcontext, NULL, "mpl_motif", "MPLDriver",
                          options, XtNumber(options), &argc, argv);

  /* Define app shell */
  nargs = 0;
  XtSetArg(args[nargs], XtNiconPixmap, apspix);
  nargs++;
  appshell = XtAppCreateShell("mpl_motif", "MPLDriver", applicationShellWidgetClass,
                              display, args, nargs);

  /* DEBUG    XSynchronize(display,TRUE); */

  /* Check that display was valid */
  if (display == NULL) {
    xerrmsg(0, "MPL Motif Outboard Driver initialization: Cannot open display");
    return (1);
  }
  screen = DefaultScreen(display);
  nplanes = DisplayPlanes(display, screen);
  defvisual = DefaultVisual(display, screen);
  apspix = XCreatePixmapFromBitmapData(display, RootWindow(display, screen),
                                       aps_bits, aps_width, aps_height, 1, 0, 1);

  /* Parse remains of command line */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-' || argv[i][0] == '/') {
      switch (argv[i][1]) {
      case 'c': /*sddsplot Commandline */
        if (!strcmp("command", argv[i] + 1)) {
          sddsplotCommandline2 = argv[++i];
          tmp = malloc(sizeof(char) * (int)(strlen(sddsplotCommandline2) + 1));
          strcpy(tmp, sddsplotCommandline2);
          str_tolower(tmp);
          /*if( strstr(tmp,"-layout=") ) {
            if( !strstr(tmp,"-layout=1,1") ) {
            newZoom = 0;
            }
            } */
          XFree(tmp);
        } else {
          xerrmsg(0, "Invalid option %s", argv[i]);
          exit(8);
        }
        break;
      case 'd': /* -dashes */
        if (!strcmp("dashes", argv[i] + 1)) {
          UseDashes = atoi(argv[++i]);
        } else {
          xerrmsg(0, "Invalid option %s", argv[i]);
          exit(8);
        }
        break;
      case 'l': /* -linetypetable file*/
        if (!strcmp("linetype", argv[i] + 1)) {
          LineTableFile = argv[++i];
          if (!LineTableFile) {
            xerrmsg(0, "Need linetype file names");
            exit(8);
          }
          if (!SDDS_ReadLineTypeTable(&lineTypeTable, LineTableFile)) {
            xerrmsg(0, "Problem to read the line type file.");
            exit(8);
          }
        } else {
          xerrmsg(0, "Invalid option %s", argv[i]);
          exit(8);
        }
        break;
      case 'h': /* Put usage here */
        break;
      case 'i': /* -interval */
        if (!strcmp("interval", argv[i] + 1)) {
          movieIntervalTime = atof(argv[++i]);
          if (movieIntervalTime < 0) {
            xerrmsg(0, "Invalid value (%f) for -interval", movieIntervalTime);
            exit(8);
          }
          if (movieIntervalTime > 60) {
            xerrmsg(0, "-interval value is over 60 seconds");
            exit(8);
          }
        } else {
          xerrmsg(0, "Invalid option %s", argv[i]);
          exit(8);
        }
        break;
      case 'k': /* -keep */
        if (!strcmp("keep", argv[i] + 1)) {
          keep = atof(argv[++i]);
          if (keep < 0) {
            xerrmsg(0, "Invalid value (%d) for -keep", keep);
            exit(8);
          }
        } else {
          xerrmsg(0, "Invalid option %s", argv[i]);
          exit(8);
        }
        break;
      case 't': /* -timeoutHours */
        if (!strcmp("timeoutHours", argv[i] + 1)) {
          timeoutHours = atof(argv[++i]);
          if (timeoutHours < 0) {
            xerrmsg(0, "Invalid value (%f) for -timeoutHours", timeoutHours);
            exit(8);
          }
          if (timeoutHours > 8760) {
            xerrmsg(0, "-timeoutHours value is over 1 year");
            exit(8);
          }
          timeoutSec = timeoutHours * 60 * 60;
        } else {
          xerrmsg(0, "Invalid option %s", argv[i]);
          exit(8);
        }
        break;
      default:
        xerrmsg(0, "Invalid option %s", argv[i]);
        exit(8);
      }
    }
  }

#if !defined(_WIN32)
  alarm(timeoutSec);
#endif

  /* Enable Editres */
#ifdef XREL5
  XtAddEventHandler(appshell, (EventMask)NULL, TRUE, _XEditResCheckMessages, NULL);
#endif

  /* Get other resources */
  XtGetApplicationResources(appshell, &rv, resources, XtNumber(resources),
                            NULL, 0);
  vchar = (rv.font->ascent + rv.font->descent);
  doublebuffer = rv.doublebuffer;
  greyscale = rv.greyscale;
  domovie = rv.movie;
  dospectrum = rv.spectrum;
  newZoom = rv.newZoom;

  /* Note: Do not use Xfree(value) */
  value = XGetDefault(display, "mpl_motif", "printCommand");
  if (value)
    strcpy(printcommand, value);
  value = XGetDefault(display, "mpl_motif", "printFile");
  if (value)
    strcpy(printfilename, value);
  value = XGetDefault(display, "mpl_motif", "saveFile");
  if (value)
    strcpy(savefilename, value);
#if defined(_WIN32)
  value = XGetDefault(display, "mpl_motif", "printFileBuffer");
  if (value)
    strcpy(printfilebuffer, value);
#endif
  value = XGetDefault(display, "mpl_motif", "dumpCommand");
  if (value)
    strcpy(dumpcommand, value);
  value = XGetDefault(display, "mpl_motif", "dumpFile");
  if (value)
    strcpy(dumpfilename, value);
  value = XGetDefault(display, "mpl_motif", "placementTopMargin");
  if (value)
    placetop = atoi(value);
  value = XGetDefault(display, "mpl_motif", "placementBottomMargin");
  if (value)
    placebottom = atoi(value);
  value = XGetDefault(display, "mpl_motif", "zoomFactor");
  if (value)
    zoomFactor = atof(value);
  value = XGetDefault(display, "mpl_motif", "placementLeftMargin");
  if (value)
    placeleft = atoi(value);
  value = XGetDefault(display, "mpl_motif", "placementRightMargin");
  if (value)
    placeright = atoi(value);
  value = XGetDefault(display, "mpl_motif", "placementHorizontalSpacing");
  if (value)
    placehspace = atoi(value);
  value = XGetDefault(display, "mpl_motif", "placementVerticalSpacing");
  if (value)
    placevspace = atoi(value);
  value = XGetDefault(display, "mpl_motif", "printFileType");
  if (value) {
    if (strcasecmp(value, "mif") == 0)
      printtype = 3;
    else if (strcasecmp(value, "epsf") == 0)
      printtype = 2;
    else if (strcasecmp(value, "png") == 0)
      printtype = 4;
    else
      printtype = 1;
  }
  value = XGetDefault(display, "mpl_motif", "saveFileType");
  if (value) {
    if (strcasecmp(value, "mif") == 0)
      savetype = 3;
    else if (strcasecmp(value, "epsf") == 0)
      savetype = 2;
    else if (strcasecmp(value, "png") == 0)
      savetype = 4;
    else if (strcasecmp(value, "mp4") == 0)
      savetype = 5;
    else
      savetype = 1;
  }

  /* Get Boolean resources */
  {
    XrmValue from, to;
    Boolean bvalue; /* Boolean is char (typically '^A' or '\0') */

    to.size = sizeof(bvalue);
    to.addr = &bvalue;

    value = XGetDefault(display, "mpl_motif", "printToFile");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        printtofile = (int)bvalue;
    }
    value = XGetDefault(display, "mpl_motif", "printLandscape");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        printlandscape = (int)bvalue;
    }
    value = XGetDefault(display, "mpl_motif", "printBorder");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        printborder = (int)bvalue;
    }
    value = XGetDefault(display, "mpl_motif", "printColor");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        printcolor = (int)bvalue;
    }
    value = XGetDefault(display, "mpl_motif", "saveLandscape");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        savelandscape = (int)bvalue;
    }
    value = XGetDefault(display, "mpl_motif", "saveBorder");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        saveborder = (int)bvalue;
    }
    value = XGetDefault(display, "mpl_motif", "saveColor");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        savecolor = (int)bvalue;
    }
    value = XGetDefault(display, "mpl_motif", "dumpToFile");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        dumptofile = (int)bvalue;
    }
    value = XGetDefault(display, "mpl_motif", "confirmFile");
    if (value) {
      from.addr = value;
      from.size = strlen(value);
      if (XtConvertAndStore(appshell, XtRString, &from, XtRBoolean, &to))
        confirmfile = (int)bvalue;
    }
  }

  /* Change colormap */
  if (privatecolormap) {
    setprivatecolormap();
  } else {
    cmap = DefaultColormap(display, screen);
    if (!alloccolors()) {
      setprivatecolormap();
    } else if (dospectrum && !allocspectrum())
      setprivatecolormap();
  }

  /* Define main window */
  nargs = 0;
  mainwin = XmCreateMainWindow(appshell, "mainWindow", args, nargs);
  XtManageChild(mainwin);

  /* Define menu bar */
  nargs = 0;
  mainmenu = XmCreateMenuBar(mainwin, "mainMenu", args, nargs);
  XtManageChild(mainmenu);

  wparent = mainmenu;

  /* Define file menu */
  nargs = 0;
#ifdef XREL5
  XtSetArg(args[nargs], XmNtearOffModel, XmTEAR_OFF_ENABLED);
  nargs++;
#endif
  w1 = XmCreatePulldownMenu(wparent, "fileMenu", args, nargs);

  /* Define file button */
  nargs = 0;
  XtSetArg(args[nargs], XmNsubMenuId, w1);
  nargs++;
  w = XmCreateCascadeButton(mainmenu, "File", args, nargs);
  XtManageChild(w);

  /* Colormap */
  /*     nargs=0; */
  /*     w=XmCreatePushButton(w1,"Colormap",args,nargs); */
  /*     XtManageChild(w); */
  /*     XtAddCallback(w,XmNactivateCallback,menucb,(caddr_t)1000); */

  /* Print setup */
  nargs = 0;
  w = XmCreatePushButton(w1, "Setup...", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, printcb, (caddr_t)0);

  /* Print */
  nargs = 0;
  w = XmCreatePushButton(w1, "Print", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)11);

  /* Save */
  nargs = 0;
  w = XmCreatePushButton(w1, "Save", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)12);

  /* Dump */
  nargs = 0;
  w = XmCreatePushButton(w1, "Dump", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)13);

  /* Separator */
  nargs = 0;
  w = XmCreateSeparator(w1, "separator", args, nargs);
  XtManageChild(w);

  /* Quit */
  nargs = 0;
  w = XmCreatePushButton(w1, "Quit", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, quitplot, (caddr_t)0);

  /* Define navigate menu */
  nargs = 0;
#ifdef XREL5
  XtSetArg(args[nargs], XmNtearOffModel, XmTEAR_OFF_ENABLED);
  nargs++;
#endif
  w1 = XmCreatePulldownMenu(wparent, "optionsMenu", args, nargs);

  /* Define navigate button */
  nargs = 0;
  XtSetArg(args[nargs], XmNsubMenuId, w1);
  nargs++;
  w = XmCreateCascadeButton(mainmenu, "Navigate", args, nargs);
  XtManageChild(w);

  /* Next */
  nargs = 0;
  w = XmCreatePushButton(w1, "Next", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, navigatecb, (caddr_t)1);

  /* Previous */
  nargs = 0;
  w = XmCreatePushButton(w1, "Previous", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, navigatecb, (caddr_t)2);

  /* Delete */
  nargs = 0;
  w = XmCreatePushButton(w1, "Delete", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, navigatecb, (caddr_t)3);

  /* Other */
  nargs = 0;
  w = XmCreatePushButton(w1, "To number...", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, navigatecb, (caddr_t)4);

  /* Define options menu */
  nargs = 0;
#ifdef XREL5
  XtSetArg(args[nargs], XmNtearOffModel, XmTEAR_OFF_ENABLED);
  nargs++;
#endif
  w1 = XmCreatePulldownMenu(wparent, "optionsMenu", args, nargs);

  /* Define options button */
  nargs = 0;
  XtSetArg(args[nargs], XmNsubMenuId, w1);
  nargs++;
  w = XmCreateCascadeButton(mainmenu, "Options", args, nargs);
  XtManageChild(w);

  /* Zoom */
  wparentsav = wparent;
  wparent = w1;

  nargs = 0;
#ifdef XREL5
  XtSetArg(args[nargs], XmNtearOffModel, XmTEAR_OFF_ENABLED);
  nargs++;
#endif
  w1 = XmCreatePulldownMenu(wparent, "optionsMenu", args, nargs);

  nargs = 0;
  XtSetArg(args[nargs], XmNsubMenuId, w1);
  nargs++;
  w = XmCreateCascadeButton(wparent, "Zoom", args, nargs);
  XtManageChild(w);

  nargs = 0;

  w = XmCreateToggleButton(w1, "Use Improved Zoom", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, menucb, (caddr_t)0);
  XmToggleButtonSetState(w, newZoom, FALSE);
  newZoomToggleButton = w;

  w = XmCreatePushButton(w1, "In", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomcb, (caddr_t)1);

  nargs = 0;
  w = XmCreatePushButton(w1, "Out", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomcb, (caddr_t)2);

  nargs = 0;
  w = XmCreatePushButton(w1, "Up", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomcb, (caddr_t)3);

  nargs = 0;
  w = XmCreatePushButton(w1, "Down", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomcb, (caddr_t)4);

  nargs = 0;
  w = XmCreatePushButton(w1, "Left", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomcb, (caddr_t)5);

  nargs = 0;
  w = XmCreatePushButton(w1, "Right", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomcb, (caddr_t)6);

  nargs = 0;
  w = XmCreatePushButton(w1, "Restore", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomcb, (caddr_t)0);

  nargs = 0;
  w = XmCreatePushButton(w1, "Help", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomcb, (caddr_t)7);

  w1 = wparent;
  wparent = wparentsav;

  /* Resize */
  wparentsav = wparent;
  wparent = w1;

  nargs = 0;
#ifdef XREL5
  XtSetArg(args[nargs], XmNtearOffModel, XmTEAR_OFF_ENABLED);
  nargs++;
#endif
  w1 = XmCreatePulldownMenu(wparent, "optionsMenu", args, nargs);

  nargs = 0;
  XtSetArg(args[nargs], XmNsubMenuId, w1);
  nargs++;
  w = XmCreateCascadeButton(wparent, "Placement/Size", args, nargs);
  XtManageChild(w);

  nargs = 0;
  w = XmCreatePushButton(w1, "Full Screen", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)0);

  nargs = 0;
  w = XmCreatePushButton(w1, "First Quarter", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)1);

  nargs = 0;
  w = XmCreatePushButton(w1, "Second Quarter", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)2);

  nargs = 0;
  w = XmCreatePushButton(w1, "Third Quarter", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)3);

  nargs = 0;
  w = XmCreatePushButton(w1, "Fourth Quarter", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)4);

  nargs = 0;
  w = XmCreatePushButton(w1, "Top Half", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)5);

  nargs = 0;
  w = XmCreatePushButton(w1, "Bottom Half", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)6);

  nargs = 0;
  w = XmCreatePushButton(w1, "Left Half", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)7);

  nargs = 0;
  w = XmCreatePushButton(w1, "Right Half", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, placecb, (caddr_t)8);

  /*Size */
  wparentsav = wparent;
  wparent = w1;

  nargs = 0;
#ifdef XREL5
  XtSetArg(args[nargs], XmNtearOffModel, XmTEAR_OFF_ENABLED);
  nargs++;
#endif
  w1 = XmCreatePulldownMenu(wparent, "optionsMenu", args, nargs);

  nargs = 0;
  XtSetArg(args[nargs], XmNsubMenuId, w1);
  nargs++;
  w = XmCreateCascadeButton(wparent, "Size", args, nargs);
  XtManageChild(w);

  nargs = 0;
  w = XmCreatePushButton(w1, "Change Factor", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoomfactorcb, (caddr_t)0);

  nargs = 0;
  w = XmCreatePushButton(w1, "Magnify", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoominout, (caddr_t)0);

  nargs = 0;
  w = XmCreatePushButton(w1, "Demagnify", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, zoominout, (caddr_t)1);

  w1 = wparentsav;
  wparent = wparentsav;
  /* Movie */
  nargs = 0;
  w = XmCreatePushButton(w1, "Movie", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)2);

  /* Greyscale */
  nargs = 0;
  w = XmCreateToggleButton(w1, "Greyscale", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, menucb, (caddr_t)3);
  XmToggleButtonSetState(w, greyscale, FALSE);

  /* Double Buffer */
  nargs = 0;
  w = XmCreateToggleButton(w1, "Double Buffer", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, menucb, (caddr_t)1);
  XmToggleButtonSetState(w, doublebuffer, FALSE);

  /* Confirm Filer */
  nargs = 0;
  w = XmCreateToggleButton(w1, "Confirm File", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, menucb, (caddr_t)4);
  XmToggleButtonSetState(w, confirmfile, FALSE);

  /* Mouse tracker */
  nargs = 0;
  w = XmCreatePushButton(w1, "Mouse Tracker...", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)5);

  /* Color */
  nargs = 0;
  w = XmCreatePushButton(w1, "Color...", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)0);

  /* Define help button */
  nargs = 0;
#ifdef XREL5
  XtSetArg(args[nargs], XmNtearOffModel, XmTEAR_OFF_ENABLED);
  nargs++;
#endif
  w1 = XmCreatePulldownMenu(mainmenu, "helpMenu", args, nargs);

  nargs = 0;
  XtSetArg(args[nargs], XmNsubMenuId, w1);
  nargs++;
  w = XmCreateCascadeButton(mainmenu, "Help", args, nargs);
  XtManageChild(w);

  nargs = 0;
  XtSetArg(args[nargs], XmNmenuHelpWidget, w);
  nargs++;
  XtSetValues(mainmenu, args, nargs);

  nargs = 0;
  w = XmCreatePushButton(w1, "Contents", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)21);

  nargs = 0;
  w = XmCreatePushButton(w1, "About", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)22);

  /* Define form for rest of main window */
  nargs = 0;
  mainform = XmCreateForm(mainwin, "mainForm", args, nargs);
  XtManageChild(mainform);

  /* Define status bar */
  cstring = XmStringCreate(" ", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNrecomputeSize, FALSE);
  nargs++;
  statusbar = XmCreateLabel(mainform, "statusBar", args, nargs);
  XtManageChild(statusbar);
  XmStringFree(cstring);

  /* Define graph drawing area */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, statusbar);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNbottomAttachment, XmATTACH_FORM);
  nargs++;
  /*
    XtSetArg(args[nargs],XmNwidth,W); nargs++;
    XtSetArg(args[nargs],XmNheight,H); nargs++;
  */
  XtSetArg(args[nargs], XmNbackground, colors[0]);
  nargs++;
  grapharea = XmCreateDrawingArea(mainform, "graphArea", args, nargs);
  XtManageChild(grapharea);
  XtAddEventHandler(grapharea, ButtonPressMask | ButtonReleaseMask | KeyPressMask,
                    FALSE, eventgraph, (caddr_t)NULL);

  /* Set main window areas */
  XmMainWindowSetAreas(mainwin, mainmenu, NULL, NULL, NULL, mainform);

  /* Realize */
  XtRealizeWidget(appshell);

  /* Get window ids */
  mainwindow = XtWindow(mainwin);

  graphareawindow = XtWindow(grapharea);
  XtAddCallback(grapharea, XmNexposeCallback, expose, NULL);
  XtAddCallback(grapharea, XmNresizeCallback, resize, NULL);

  /* Define cursors */
  crosshair = XCreateFontCursor(display, XC_crosshair);
  watch = XCreateFontCursor(display, XC_watch);

  /* Add callback to handle stdin */
#if defined(_WIN32)
  /*
    inputid=XtAppAddInput(appcontext,fileno(stdin),(XtPointer)XtInputReadWinsock,gnuplot,NULL);
  */
  workprocid = XtAppAddWorkProc(appcontext, gnuplot, NULL);
#else
  inputid = XtAppAddInput(appcontext, 0, (XtPointer)XtInputReadMask, gnuplot, NULL);
#endif

  /* Start */
  windowmessage = 1;
  XtAppMainLoop(appcontext);
  return 0;
}

/**************************** xerrMsg *************************************/
void xerrmsg(int x, const char *fmt, ...) {
#define MAXERRS 25

  Widget child;
  va_list vargs;
  static char lstring[FIXED_MSG_SIZE];
  static int errcount = 0, errflag = 0;

  /*  Stop if too many errors */
  if (errflag) {
    return;
  }
  if (++errcount > MAXERRS) {
    va_start(vargs, fmt);
    (void)vfprintf(stderr, fmt, vargs);
    va_end(vargs);
    sprintf(lstring, "\nToo many errors\nNo more will be displayed this session");
    errflag = 1;
  } else {
    va_start(vargs, fmt);
    (void)vsprintf(lstring, fmt, vargs);
    va_end(vargs);
  }
  if (lstring[0] != '\0') {
    if (windowmessage && x) {
      XBell(display, 50);
      XBell(display, 50);
      XBell(display, 50);
      cstring = XmStringCreateLtoR(lstring, XmSTRING_DEFAULT_CHARSET);
      nargs = 0;
      XtSetArg(args[nargs], XmNtitle, "Warning");
      nargs++;
      XtSetArg(args[nargs], XmNmessageString, cstring);
      nargs++;
      warningBox = XmCreateWarningDialog(mainwin, "warningMessage",
                                         args, nargs);
      XmStringFree(cstring);
      child = XmMessageBoxGetChild(warningBox, XmDIALOG_CANCEL_BUTTON);
      XtDestroyWidget(child);
      child = XmMessageBoxGetChild(warningBox, XmDIALOG_HELP_BUTTON);
      XtDestroyWidget(child);
      XtManageChild(warningBox);
      XmUpdateDisplay(warningBox);
    }
#ifdef WIN32
    lprintf("%s\n", lstring);
#else
    fprintf(stderr, "%s\n", lstring);
#endif
  }
  return;
}
/**************************** addspectrumcolors ***************************/
int addspectrumcolors(void) {
  Widget w;
  Pixel topshadow, bottomshadow;
  int i;

  /* Get shadow colors */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopShadowColor, &topshadow);
  nargs++;
  XtSetArg(args[nargs], XmNbottomShadowColor, &bottomshadow);
  nargs++;
  XtGetValues(colorpopup, args, nargs);

  /* Add colors buttons */
  if (!spectrumallocated)
    allocspectrum();
  for (i = 0; i < nspect; i++) {
    nargs = 0;
    XtSetArg(args[nargs], XmNtopShadowColor, topshadow);
    nargs++;
    XtSetArg(args[nargs], XmNbottomShadowColor, bottomshadow);
    nargs++;
    XtSetArg(args[nargs], XmNarmColor, spectrum[i]);
    nargs++;
    XtSetArg(args[nargs], XmNbackground, spectrum[i]);
    nargs++;
    w = XmCreatePushButton(spectrumbox, " ", args, nargs);
    XtManageChild(w);
    XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)(300UL + i));
  }
  return 0;
}
/**************************** alloccolors *********************************/
int alloccolors(void) {
  int n, windowmessage0 = windowmessage, ok = 1;
  char *value, option[20];
  XColor used, exact;

  /* Allocate black */
  if (XAllocNamedColor(display, cmap, "Black", &used, &exact)) {
    black = used.pixel;
  } else {
    ok = 0;
    xerrmsg(0, "mpl_motif: Cannot allocate Black");
    black = BlackPixel(display, screen);
  }
  /* Allocate white */
  if (XAllocNamedColor(display, cmap, "White", &used, &exact)) {
    white = used.pixel;
  } else {
    ok = 0;
    xerrmsg(0, "mpl_motif: Cannot allocate White");
    white = WhitePixel(display, screen);
  }
  /* Allocate option colors */
  for (n = 0; n < NCOLORS; n++) {
    colorsalloc[n] = 0;
    windowmessage = 0;
    strcpy(option, color_keys[n]);
    strcat(option, "Color");
    value = XGetDefault(display, "mpl_motif", option);
    if (!value)
      value = color_values[n];
    /* 	printf("mpl_motif.%s:\t%s\n",option,color_values[n]); */
    if (XAllocNamedColor(display, cmap, value, &used, &exact)) {
      colors[n] = colors_orig[n] = used.pixel;
      colorsalloc[n] = 1;
    } else {
      ok = 0;
      /* 	    fprintf(stderr,"mpl_motif: Cannot allocate %s: %s\n",option, */
      /* 	      value); */
      /* 	    fprintf(stderr, "mpl_motif: Assuming %s:black\n",option); */
      colors[n] = colors_orig[n] = black;
    }
  }
  windowmessage = windowmessage0;
  if (!ok)
    xerrmsg(1, "Error(s) allocating line colors");
  return ok;
}
/**************************** allocspectrum *******************************/
int allocspectrum(void) {
  int n, ok = 1, windowmessage0 = windowmessage;

  /* Allocate spectrum colors */
  windowmessage = 0;
  if (!greyscale) {
    for (n = 0; n < nspect; n++) {
      spectrumalloc[n] = 0;
      xcol.flags = 0xff;
      if (spectral == 1) {
        Spectral_RGB_short_values(&xcol.red, &xcol.green, &xcol.blue, n / (nspect - 1.0));
      } else if (spectral == 2) {
        Spectral_BGR_short_values(&xcol.red, &xcol.green, &xcol.blue, n / (nspect - 1.0));
      } else if (spectral == 3) {
        Spectral_RGB_NoMagenta_short_values(&xcol.red, &xcol.green, &xcol.blue, n / (nspect - 1.0));
      } else if (spectral == 4) {
        Spectral_BGR_NoMagenta_short_values(&xcol.red, &xcol.green, &xcol.blue, n / (nspect - 1.0));
      } else if (customspectral) {
        Custom_RGB_short_values(&xcol.red, &xcol.green, &xcol.blue, red0, green0, blue0, red1, green1, blue1, n / (nspect - 1.0));
      } else {
        RGB_short_values(&xcol.red, &xcol.green, &xcol.blue, n / (nspect - 1.0));
      }
      if (XAllocColor(display, cmap, &xcol)) {
        spectrum[n] = xcol.pixel;
        spectrumalloc[n] = 1;
      } else {
        /* 		xerrmsg("Cannot allocate hue=%f,RGB=%x:%x:%x\n",  */
        /* 		  n/(NSPECT-1.0),xcol.red,xcol.green,xcol.blue); */
        spectrum[n] = black;
        ok = 0;
      }
    }
  } else {
    XColor used, exact;
    char value[20];

    for (n = 0; n < nspect; n++) {
      spectrumalloc[n] = 0;
      sprintf(value, "grey%d", n);
      if (XAllocNamedColor(display, cmap, value, &used, &exact)) {
        spectrum[n] = used.pixel;
        spectrumalloc[n] = 1;
      } else {
        /* 		xerrmsg("Cannot allocate %s\n",value); */
        spectrum[n] = black;
        ok = 0;
      }
    }
  }
  windowmessage = windowmessage0;
  spectrumallocated = 1;
  if (!ok)
    xerrmsg(1, "Error(s) allocating spectrum colors");
  return ok;
}
/**************************** choosecolors ********************************/
void choosecolors(void) {
  Widget w, w1;
  Pixel topshadow, bottomshadow, bw;
  int i;

  XDefineCursor(display, mainwindow, watch);
  XFlush(display);
  cstring = XmStringCreate("Color", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNdialogTitle, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNautoUnmanage, FALSE);
  nargs++;
  colorpopup = XmCreateFormDialog(mainwin, "optPopup", args, nargs);
  XmStringFree(cstring);

  /* Get shadow colors */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopShadowColor, &topshadow);
  nargs++;
  XtSetArg(args[nargs], XmNbottomShadowColor, &bottomshadow);
  nargs++;
  XtGetValues(colorpopup, args, nargs);

  /* Define row column for buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(colorpopup, "switchBox", args, nargs);
  XtManageChild(w1);

  /* Define remove button */
  nargs = 0;
  w = XmCreatePushButton(w1, "Remove", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)1);

  /* Define black-on-white button */
  nargs = 0;
  w = XmCreatePushButton(w1, "Black on White", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)3);

  /* Define white-on-black button */
  nargs = 0;
  w = XmCreatePushButton(w1, "White on Black", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)4);

  /* Define spectrum lines button */
  nargs = 0;
  w = XmCreatePushButton(w1, "Spectrum Lines", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)6);

  /* Define group button */
  nargs = 0;
  w = XmCreateToggleButton(w1, "Group Lines", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, colorcb, (caddr_t)5);
  XmToggleButtonSetState(w, grouplines, FALSE);

  /* Define resources box */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_COLUMN);
  nargs++;
  XtSetArg(args[nargs], XmNnumColumns, 2);
  nargs++;
  w1 = XmCreateRowColumn(colorpopup, "resourceBox", args, nargs);
  XtManageChild(w1);

  /* Add resources buttons */
  for (i = 0; i < NCOLORS; i++) {
    nargs = 0;
    XtSetArg(args[nargs], XmNtopShadowColor, topshadow);
    nargs++;
    XtSetArg(args[nargs], XmNbottomShadowColor, bottomshadow);
    nargs++;
    XtSetArg(args[nargs], XmNarmColor, colors[i]);
    nargs++;
    XtSetArg(args[nargs], XmNbackground, colors[i]);
    nargs++;
    w = XmCreatePushButton(w1, color_keys[i], args, nargs);
    XtManageChild(w);
    XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)(100UL + i));
    wres[i] = w;
  }

  /* Separator */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  w1 = XmCreateSeparator(colorpopup, "separator", args, nargs);
  XtManageChild(w1);

  /* Define colors box */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_COLUMN);
  nargs++;
  XtSetArg(args[nargs], XmNnumColumns, 1);
  nargs++;
  w1 = XmCreateRowColumn(colorpopup, "colorsBox", args, nargs);
  XtManageChild(w1);

  /* Add colors buttons */
  for (i = 0; i < 2; i++) {
    if (i == 0)
      bw = white;
    else
      bw = black;
    nargs = 0;
    XtSetArg(args[nargs], XmNtopShadowColor, topshadow);
    nargs++;
    XtSetArg(args[nargs], XmNbottomShadowColor, bottomshadow);
    nargs++;
    XtSetArg(args[nargs], XmNarmColor, bw);
    nargs++;
    XtSetArg(args[nargs], XmNbackground, bw);
    nargs++;
    w = XmCreatePushButton(w1, " ", args, nargs);
    XtManageChild(w);
    XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)(500UL + i));
  }
  for (i = 0; i < NCOLORS; i++) {
    nargs = 0;
    XtSetArg(args[nargs], XmNtopShadowColor, topshadow);
    nargs++;
    XtSetArg(args[nargs], XmNbottomShadowColor, bottomshadow);
    nargs++;
    XtSetArg(args[nargs], XmNarmColor, colors_orig[i]);
    nargs++;
    XtSetArg(args[nargs], XmNbackground, colors_orig[i]);
    nargs++;
    w = XmCreatePushButton(w1, " ", args, nargs);
    XtManageChild(w);
    XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)(200UL + i));
  }

  /* Separator */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  w1 = XmCreateSeparator(colorpopup, "separator", args, nargs);
  XtManageChild(w1);

  /* Define spectrum box */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  /*	XtSetArg(args[nargs],XmNrightAttachment,XmATTACH_FORM); nargs++; KLUDGE */
  XtSetArg(args[nargs], XmNbottomAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmVERTICAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_COLUMN);
  nargs++;
  XtSetArg(args[nargs], XmNnumColumns, NCOLORS + 2);
  nargs++; /* BUG */
  w1 = XmCreateRowColumn(colorpopup, "spectrumBox", args, nargs);
  XtManageChild(w1);
  spectrumbox = w1;

  /* Add colors or button depending on addspectrum */
  if (addspectrum) {
    addspectrumcolors();
  } else {
    nargs = 0;
    w = XmCreatePushButton(spectrumbox, "Spectrum...", args, nargs);
    XtManageChild(w);
    XtAddCallback(w, XmNactivateCallback, colorcb, (caddr_t)2);
  }

  /* Manage */
  XtManageChild(colorpopup);
  if (mtrackpopup)
    XDefineCursor(display, mainwindow, crosshair);
  else
    XUndefineCursor(display, mainwindow);
}
/**************************** colorcb *************************************/
void colorcb(Widget w, XtPointer clientdata, XtPointer calldata) {
  long int n = (long int)clientdata;
  static int iselected = 0;
  int iset = 0;
  int i, imax;

  /* Check that wselected is set */
  if ((n >= 200) & !wselected) {
    xerrmsg(1, "Select a resource first");
    return;
  }
  /* Branch depending on n */
  if (n == 0) {
    choosecolors();
  } else if (n == 1) {
    XtDestroyWidget(colorpopup);
    wselected = (Widget)NULL;
  } else if (n == 2) {
    XDefineCursor(display, mainwindow, watch);
    XFlush(display);
    XtUnmanageChild(spectrumbox);
    XtDestroyWidget(w);
    addspectrum = 1;
    addspectrumcolors();
    XtManageChild(spectrumbox);
    if (mtrackpopup)
      XDefineCursor(display, mainwindow, crosshair);
    else
      XUndefineCursor(display, mainwindow);
  } else if (n == 3) {
    for (i = 0; i < NCOLORS; i++) {
      if (i != 0)
        colors[i] = black;
      else
        colors[i] = white;
      nargs = 0;
      XtSetArg(args[nargs], XmNarmColor, colors[i]);
      nargs++;
      XtSetArg(args[nargs], XmNbackground, colors[i]);
      nargs++;
      XtSetValues(wres[i], args, nargs);
    }
    nargs = 0;
    XtSetArg(args[nargs], XmNbackground, colors[0]);
    nargs++;
    XtSetValues(grapharea, args, nargs);
    displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
  } else if (n == 4) {
    for (i = 0; i < NCOLORS; i++) {
      if (i != 0)
        colors[i] = white;
      else
        colors[i] = black;
      nargs = 0;
      XtSetArg(args[nargs], XmNarmColor, colors[i]);
      nargs++;
      XtSetArg(args[nargs], XmNbackground, colors[i]);
      nargs++;
      XtSetValues(wres[i], args, nargs);
    }
    nargs = 0;
    XtSetArg(args[nargs], XmNbackground, colors[0]);
    nargs++;
    XtSetValues(grapharea, args, nargs);
    displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
  } else if (n == 5) {
    grouplines = XmToggleButtonGetState(w) ? 1 : 0;
  } else if (n == 6) {
    if (!spectrumallocated)
      allocspectrum();
    if (grouplines)
      imax = NCOLORS;
    else
      imax = linecolormax + 1;
    if (imax > 4) {
      for (i = 3; i < imax; i++) {
        colors[i] = spectrum[((nspect - 1) * (i - 3)) / (imax - 4)];
        nargs = 0;
        XtSetArg(args[nargs], XmNarmColor, colors[i]);
        nargs++;
        XtSetArg(args[nargs], XmNbackground, colors[i]);
        nargs++;
        XtSetValues(wres[i], args, nargs);
      }
    }
    displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
  } else if (n < 200) {
    iselected = n - 100;
    wselected = w;
  } else if (n < 300) {
    colors[iselected] = colors_orig[n - 200];
    iset = 1;
  } else if (n < 500) {
    colors[iselected] = spectrum[n - 300];
    iset = 1;
  } else if (n == 500) {
    colors[iselected] = white;
    iset = 1;
  } else if (n == 501) {
    colors[iselected] = black;
    iset = 1;
  }
  /* Set and display the new colors */
  if (iset) {
    if (grouplines) {
      if (iselected > 1) {
        for (i = 2; i < NCOLORS; i++) {
          colors[i] = colors[iselected];
          nargs = 0;
          XtSetArg(args[nargs], XmNarmColor, colors[i]);
          nargs++;
          XtSetArg(args[nargs], XmNbackground, colors[i]);
          nargs++;
          XtSetValues(wres[i], args, nargs);
        }
      }
    } else {
      nargs = 0;
      XtSetArg(args[nargs], XmNarmColor, colors[iselected]);
      nargs++;
      XtSetArg(args[nargs], XmNbackground, colors[iselected]);
      nargs++;
      XtSetValues(wselected, args, nargs);
    }
    if (iselected == 0) {
      nargs = 0;
      XtSetArg(args[nargs], XmNbackground, colors[iselected]);
      nargs++;
      XtSetValues(grapharea, args, nargs);
    }
    displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
  }
}
/**************************** destroyplotrec ******************************/
void destroyplotrec(struct PLOTREC *rec)
/* Destroys the specified plot record and adjusts cur if necessary */
{
  /* Check if it is NULL */
  if (!rec)
    return;
  /* Unlink from the list */
  if (rec->prev == (struct PLOTREC *)NULL) { /* First in the list */
    if (rec == last) {                       /* Also last */
      cur = last = (struct PLOTREC *)NULL;
    } else {
      (rec->next)->prev = (struct PLOTREC *)NULL;
      if (rec == cur)
        cur = rec->next;
    }
  } else if (rec == last) {                    /* Last in the list */
    if (rec->prev == (struct PLOTREC *)NULL) { /* Also first */
      cur = last = (struct PLOTREC *)NULL;
    } else {
      last = (rec->prev)->next = rec->prev;
      if (rec == cur)
        cur = last;
    }
  } else {
    (rec->prev)->next = rec->next;
    (rec->next)->prev = rec->prev;
    if (cur == rec)
      cur = rec->next;
  }
  /* Free the commands buffer */
  if (rec->buffer)
    XtFree(rec->buffer);

  /* Free the structure */
  free(rec);
  if (rec == curwrite)
    curwrite = (struct PLOTREC *)NULL;
  rec = (struct PLOTREC *)NULL;
  status();
}

void destroycoordrecs() {
  while (lastcoord) {
    if (lastcoord->prev) {
      lastcoord = lastcoord->prev;
      free(lastcoord->next);
      lastcoord->next = (struct COORDREC *)NULL;
    } else {
      free(lastcoord);
      lastcoord = curcoord = (struct COORDREC *)NULL;
    }
    ncoords--;
  }
  usecoord = (struct COORDREC *)NULL;
}

int destroyallplotrec() {
  int cur_plotn = cur->nplot;
  while (last) {
    XtFree(last->buffer);
    if (last->prev) {
      last = last->prev;
      free((last->next));
      last->next = (struct PLOTREC *)NULL;
    } else {
      free(last);
      last = cur = curwrite = (struct PLOTREC *)NULL;
    }
    nplots--;
  }
  return cur_plotn;
}
static int line_style = LineSolid;
int orig_height, orig_width;

/**************************** displayplot *********************************/
void displayplot(Widget w, caddr_t clientdata, caddr_t calldata)
/* Draws the plot according to the commands in the buffer */
/* Draws to a pixmap and then copies it to the window            */
/* Without double buffer draws directly to the window as well    */
{
  VTYPE x, y, lt = 0, lt2;
  int n, linewidth, type = 0;
  char *bufptr, command;
  static int ifirst = 1;
  unsigned short r, g, b;
  XColor foregroundColor;

  /* Set scaling parameters */
  if (ifirst) {
    Window root;
    unsigned int border, depth, width, height;
    int x0, y0;
    ifirst = 0;
    resize((Widget)0, (caddr_t)0, (caddr_t)0);
    XGetGeometry(display, mainwindow, &root, &x0, &y0, &width, &height,
                 &border, &depth);
    orig_width = width;
    orig_height = height;
    return;
  }

  /* Create new GC */
  if (gc) {
    XFreeGC(display, gc);
  }
  gc = XCreateGC(display, graphareawindow, 0, NULL);
  XSetBackground(display, gc, colors[0]);
  XSetForeground(display, gc, colors[1]);
  XSetFont(display, gc, rv.font->fid);

  /* Create pixmap */
  if (pixmap)
    XFreePixmap(display, pixmap);
  pixmap = XCreatePixmap(display, RootWindow(display, screen),
                         W, H, nplanes);

  /* Clear screen */
  XSetForeground(display, gc, colors[0]);
  XFillRectangle(display, pixmap, gc, 0, 0, W, H);
  XSetForeground(display, gc, colors[1]);
  if (!doublebuffer)
    XClearWindow(display, graphareawindow);

  /* Loop over accumulated commands from inboard driver */
  if (!cur) {
    XCopyArea(display, pixmap, graphareawindow, gc, 0, 0, W, H, 0, 0);
    return;
  }
  bufptr = cur->buffer;

  destroycoordrecs();
  for (n = 0; n < cur->nc;) {
    switch (command = *bufptr++) {
    case 'V': /*   X11_vector(x,y) - draw vector  */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      XDrawLine(display, pixmap, gc, X(cx), Y(cy), X(x), Y(y));
      if (!doublebuffer)
        XDrawLine(display, graphareawindow, gc, X(cx), Y(cy), X(x), Y(y));
      cx = x;
      cy = y;
      break;
    case 'M': /*   X11_move(x,y) - move  */
      memcpy((char *)&cx, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&cy, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      break;
    case 'P': /* X11_dot(x, y) -- dot */
      memcpy((char *)&cx, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&cy, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      XDrawPoint(display, pixmap, gc, X(cx), Y(cy));
      if (!doublebuffer)
        XDrawPoint(display, graphareawindow, gc, X(cx), Y(cy));
      break;
    case 'L': /*   X11_linetype(type) - set line type  */

      memcpy((char *)&lt2, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + sizeof(VTYPE);
      if (Color) {
        if (!lineTypeTable.nEntries) { /* built-in types used */
          lt = lt2 % 16;
          lt += 2; /* Convert to color index */
          if (lt >= NCOLORS)
            lt = NCOLORS - 1;
          if (lt > linecolormax)
            linecolormax = lt;
          type = LineSolid;
          linewidth = 1;
          XSetForeground(display, gc, colors[lt]);
          if (UseDashes) {
            lt = lt2 % 10;
            type = (lt == 0) ? LineSolid : LineOnOffDash;
            if (dashes[lt][0])
              XSetDashes(display, gc, 0, dashes[lt], strlen(dashes[lt]));
          }
        } else { /* user defined line types used */
          lt = lt2 % lineTypeTable.nEntries;
          if (lineTypeTable.typeFlag & LINE_TABLE_DEFINE_THICKNESS)
            linewidth = lineTypeTable.thickness[lt];
          else
            linewidth = 1;
          if (lineTypeTable.typeFlag & LINE_TABLE_DEFINE_DASH) {
            if (lineTypeTable.dash[lt].dashArray[0]) {
              XSetDashes(display, gc, 0, lineTypeTable.dash[lt].dashArray, strlen(lineTypeTable.dash[lt].dashArray));
              type = LineOnOffDash;
            } else
              type = LineSolid;
          }
          if (lineTypeTable.typeFlag && LINE_TABLE_DEFINE_COLOR) {
            foregroundColor.red = (unsigned short)256 * lineTypeTable.red[lt];
            foregroundColor.green = (unsigned short)256 * lineTypeTable.green[lt];
            foregroundColor.blue = (unsigned short)256 * lineTypeTable.blue[lt];
            XAllocColor(display, DefaultColormap(display, screen), &foregroundColor);
            XSetForeground(display, gc, foregroundColor.pixel);
          }
        }
      } else {
        if (!lineTypeTable.nEntries) { /* built-in dashes used */
          lt = lt % 10;
          type = (lt == 0) ? LineSolid : LineOnOffDash;
          linewidth = 0;
          if (dashes[lt][0])
            XSetDashes(display, gc, 0, dashes[lt], strlen(dashes[lt]));
        } else { /* user defined type used */
          lt = lt2 % lineTypeTable.nEntries;
          if (lineTypeTable.typeFlag & LINE_TABLE_DEFINE_THICKNESS)
            linewidth = lineTypeTable.thickness[lt];
          else
            linewidth = 1;
          if (lineTypeTable.typeFlag & LINE_TABLE_DEFINE_DASH) {
            if (lineTypeTable.dash[lt].dashArray[0]) {
              XSetDashes(display, gc, 0, lineTypeTable.dash[lt].dashArray, strlen(lineTypeTable.dash[lt].dashArray));
              type = LineOnOffDash;
            } else
              type = LineSolid;
          }
        }
      }
      XSetLineAttributes(display, gc, linewidth, type, CapButt, JoinBevel);
      line_style = type;
      break;
    case 'W': /*   X11_line_thickness(thickness) - set line width  */
      memcpy((char *)&lt, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + sizeof(VTYPE);
      if (Color) {
        linewidth = lt;
      } else {
        linewidth = lt - 1;
      }
      if (!(lineTypeTable.typeFlag & LINE_TABLE_DEFINE_THICKNESS))
        XSetLineAttributes(display, gc, linewidth, line_style, CapButt, JoinBevel);
      break;
    case 'B': /* Fill Box */
      {
        VTYPE shade, xl, xh, yl, yh;

        memcpy((char *)&shade, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&xl, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&xh, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&yl, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&yh, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 5 * sizeof(VTYPE);
        shade = shade % nspect;
        xl = X(xl);
        yl = Y(yl);
        xh = X(xh);
        yh = Y(yh);
        if (!spectrumallocated)
          allocspectrum();
        XSetForeground(display, gc, spectrum[shade]);
        XFillRectangle(display, pixmap, gc, xl, yh, xh - xl, yl - yh);
        if (!doublebuffer)
          XFillRectangle(display, graphareawindow, gc, xl, yh, xh - xl, yl - yh);
        XSetForeground(display, gc, colors[lt + 2]);
      } break;
    case 'U': /*   X11_user(ax,bx,ay,by) - user coordinate scaling  */
      memcpy((char *)&userax, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&userbx, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&useray, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&userby, bufptr, sizeof(double));
      bufptr += sizeof(double);
      n += sizeof(char) + 4 * sizeof(double);
      curcoord = makecoordrec();
      if (lastcoord) {
        lastcoord->next = curcoord;
      }
      lastcoord = curcoord;
      curcoord->x0 = userax;
      curcoord->x1 = userbx;
      curcoord->y0 = useray;
      curcoord->y1 = userby;
      break;
    case 'G':
    case 'R':
    case 'E':
      n += sizeof(char);
      break;
    case 'C': {
      memcpy((char *)&r, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&g, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&b, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 3 * sizeof(VTYPE);
      /*r = r%65536;*/
      if (Color && !lineTypeTable.nEntries) {
        foregroundColor.red = r;
        foregroundColor.green = g;
        foregroundColor.blue = b;
        XAllocColor(display, DefaultColormap(display, screen), &foregroundColor);
        XSetForeground(display, gc, foregroundColor.pixel);
      }
    } break;
    case 'S': /* Allocate spectral spectrum */
      {
        VTYPE num, spec, r0, g0, b0, r1, g1, b1;
        memcpy((char *)&num, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&spec, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&r0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&g0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&b0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&r1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&g1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&b1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 8 * sizeof(VTYPE);
        nspect = num;
        if (spec == 0) {
          customspectral = 1;
          spectral = 0;
          red0 = r0;
          green0 = g0;
          blue0 = b0;
          red1 = r1;
          green1 = g1;
          blue1 = b1;
        } else if (spec == 1) {
          customspectral = 0;
          spectral = 1;
        } else if (spec == 2) {
          customspectral = 0;
          spectral = 2;
        } else if (spec == 3) {
          customspectral = 0;
          spectral = 3;
        } else if (spec == 4) {
          customspectral = 0;
          spectral = 4;
        } else {
          customspectral = 0;
          spectral = 0;
        }
        allocspectrum();
      } break;
    default:
      xerrmsg(1, "Received invalid draw command in displayplot()\n"
              "  for location %d in plot %d: (%d dec) (%02x hex) %c",
              cur->nc, cur->nplot, command, command,
              (command > 32 && command < 127) ? command : 32);
      break;
    }
  } /* End for loop */
  /* Copy the pixmap to the window */
  if ((usecoordn != 0) && curcoord) {
    while (curcoord->ncoord != usecoordn) {
      if (curcoord->prev)
        curcoord = curcoord->prev;
    }
    usecoord = curcoord;
    userax = curcoord->x0;
    userbx = curcoord->x1;
    useray = curcoord->y0;
    userby = curcoord->y1;
  }
  XCopyArea(display, pixmap, graphareawindow, gc, 0, 0, W, H, 0, 0);
}
/**************************** expandname **********************************/
int expandname(char *oldname, char *newname, int n)
/* Passes name to echo to expand it */
/* oldname can be the same as newname */
{
  FILE *pin;
  char *ptr;

  /* Set an outgoing null string in case of error */
  newname[0] = '\0';
  /* Handle incoming null string */
  if (!oldname[0])
    return 0;
  /* Send oldname to the C-shell for expansion */
  sprintf(string, "csh -c \"echo %s\"", oldname);
  pin = popen(string, "r");
  *string = '\0';
  fgets(string, BUFSIZ, pin);
  pclose(pin);
  /*
    if(ferror(pin) || !*string) {
    xerrmsg(1,"Error expanding name:\n%s",string);
    return 0;
    }
  */
  /* Remove newline */
  if ((ptr = strchr(string, '\n')))
    *ptr = '\0';
  /* Store it in newname */
  strncpy(newname, string, n - 1);
  /* Make sure it is null-terminated */
  newname[n - 1] = '\0';
  return 1;
}
/**************************** dump ****************************************/
void dump(void)
/* Makes an X window dump of the grapharea */
{
  char tempname[BUFSIZ];

  XDefineCursor(display, mainwindow, watch);
  XFlush(display);
  if (printpopup)
    readprinttext();
  /* Open file or pipe */
  if (dumptofile) {
#if defined(_WIN32)
    sprintf(tempname, "%s", dumpfilename);
    outfile = fopen(tempname, "w");
#else
    if (expandname(dumpfilename, tempname, BUFSIZ))
      outfile = fopen(tempname, "w");
    else
      outfile = NULL;
#endif
    if (outfile == NULL) {
      xerrmsg(1, "Unable to open %s", tempname);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
  } else {
    outfile = popen(dumpcommand, "w");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to run %s", dumpcommand);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
  }
  /* Make the dump */
  windowdump(graphareawindow, DUMPTITLE, pixmap, W, H, outfile);
  /* Clean up */
  if (dumptofile) {
    fclose(outfile);
  } else {
    pclose(outfile);
  }
  if (ferror(outfile)) {
    /*xerrmsg(1,"File or Pipe Error");*/
  } else {
    XBell(display, 20);
    if (dumptofile && confirmfile)
      xinfomsg("Screen dump file is %s", tempname);
  }
  if (mtrackpopup)
    XDefineCursor(display, mainwindow, crosshair);
  else
    XUndefineCursor(display, mainwindow);
}
/**************************** eventgraph **********************************/
static FILE *fpOutput = NULL;
void eventgraph(Widget w, XtPointer clientdata, XEvent *event,
                Boolean *contdispatch) {
  static Position xboxstart, yboxstart, xboxnew, yboxnew;
  int temp;

  switch (event->type) {
  case ButtonPress:
    switch (event->xbutton.button) {
    case Button1:
      expose((Widget)0, (caddr_t)0, (caddr_t)0);
      xboxstart = xboxnew = event->xbutton.x;
      yboxstart = yboxnew = event->xbutton.y;
      rubberband(graphareawindow, &xboxstart, &yboxstart, &xboxnew, &yboxnew);
      if (xboxstart > xboxnew) {
        temp = xboxstart;
        xboxstart = xboxnew;
        xboxnew = temp;
      }
      if (yboxstart > yboxnew) {
        temp = yboxstart;
        yboxstart = yboxnew;
        yboxnew = temp;
      }
      break;
    case Button2: {
      double x0, y0, x1, y1;

      x0 = X0(xboxstart);
      x1 = X0(xboxnew);
      y0 = Y0(yboxnew);
      y1 = Y0(yboxstart);
      zoom(x0, y0, x1, y1);
      xboxstart = xboxnew;
      yboxstart = yboxnew;
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    } break;
    case Button3:
      if (sddsplotCommandline2 && newZoom)
        restore_new();
      else
        restore();
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
      break;
    }
    break;
  case KeyPress: {
    char buf[2];

    if (XLookupString((XKeyEvent *)event, buf, 1, NULL, NULL) == 0)
      return;

    switch (buf[0]) {
    case '<': /* First plot */
      while (cur->prev)
        cur = cur->prev;
      usecoordn = 0;
      status();
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
      break;
    case '>': /* Last plot */
      cur = last;
      status();
      usecoordn = 0;
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
      break;
    case ':': /* Set mouse track reference */
      mtrackxref = mtrackx;
      mtrackyref = mtracky;
      mtrackxrel = mtrackx - mtrackxref;
      mtrackyrel = mtracky - mtrackyref;
      if (mtrackpopup) {
        char stringvalue[1024];

        sprintf(stringvalue, mtrackformat, mtrackxref);
        nargs = 0;
        XtSetArg(args[nargs], XmNvalue, stringvalue);
        nargs++;
        XtSetValues(mtrackxrefw, args, nargs);
        sprintf(stringvalue, mtrackformat, mtrackyref);
        nargs = 0;
        XtSetArg(args[nargs], XmNvalue, stringvalue);
        nargs++;
        XtSetValues(mtrackyrefw, args, nargs);
        sprintf(stringvalue, mtrackformat, mtrackxrel);
        nargs = 0;
        XtSetArg(args[nargs], XmNvalue, stringvalue);
        nargs++;
        XtSetValues(mtrackxrelw, args, nargs);
        sprintf(stringvalue, mtrackformat, mtrackyrel);
        nargs = 0;
        XtSetArg(args[nargs], XmNvalue, stringvalue);
        nargs++;
        XtSetValues(mtrackyrelw, args, nargs);
      }
      break;
    case 'o': /* output mouse tracker position to a file */
      if (!fpOutput) {
        long index = 0;
        char fnBuffer[256];
        while (1) {
          snprintf(fnBuffer, 256, "mpl_motif-%06ld.sdds", index);
          if (!fexists(fnBuffer))
            break;
          index++;
        }
        fpOutput = fopen(fnBuffer, "w");
        fprintf(fpOutput, "SDDS1\n&column name=x, type=float &end\n&column name=y, type=float &end\n");
        fprintf(fpOutput, "&data mode=ascii no_row_counts=1 &end\n");
      }
      fprintf(fpOutput, "%le %le\n", mtrackx, mtracky);
      fflush(fpOutput);
      XBell(display, 50);
      break;
    case '.': /* Toggle mouse tracking */
      if (mtrackon) {
        XtRemoveEventHandler(grapharea, PointerMotionMask,
                             FALSE, eventmtrack, (caddr_t)NULL);
        mtrackon = 0;
      } else {
        if (!mtrackpopup)
          mtracksetup();
        else {
          XtAddEventHandler(grapharea, PointerMotionMask,
                            FALSE, eventmtrack, (caddr_t)NULL);
          mtrackon = 1;
        }
      }
      break;
    case ',':
      if (!mtrackpopup)
        mtracksetup();
      else {
        XtAddEventHandler(grapharea, PointerMotionMask,
                          FALSE, eventmtrack, (caddr_t)NULL);
        mtrackon = 1;
      }
      if (lastcoord) {
        if (!usecoord) {
          /* it always has a extra coordinate system, so the minimum list length  is 2 */
          usecoord = lastcoord->prev;
          usecoordn = usecoord->ncoord;
        }
        if (usecoord->prev) {
          usecoord = usecoord->prev;
          usecoordn = usecoord->ncoord;
        } else {
          usecoord = lastcoord->prev;
          usecoordn = usecoord->ncoord;
        }
        userax = usecoord->x0;
        userbx = usecoord->x1;
        useray = usecoord->y0;
        userby = usecoord->y1;
      }
      break;
    case 'q': /* Quit */
      quitplot((Widget)0, (caddr_t)0, (caddr_t)0);
      break;
    case 's': /* Save MIF */
      menucb((Widget)0, (caddr_t)12, (caddr_t)0);
      break;
    case 'n': /* Go to Next plot */
      usecoordn = 0;
      navigatecb((Widget)0, (caddr_t)11, (caddr_t)0);
      break;
    case 'p': /* Go to Previous plot */
      usecoordn = 0;
      navigatecb((Widget)0, (caddr_t)12, (caddr_t)0);
      break;
    case 'd': /* Delete current plot */
      usecoordn = 0;
      navigatecb((Widget)0, (caddr_t)13, (caddr_t)0);
      break;
    case 'f': /* Full screen */
      placecb((Widget)0, (caddr_t)0, (caddr_t)0);
      break;
    case '0': /*restore original plot*/
      restore_plot();

      break;
    case '1': /* Quarter 1 */
      placecb((Widget)0, (caddr_t)1, (caddr_t)0);
      break;
    case '2': /* Quarter 2 */
      placecb((Widget)0, (caddr_t)2, (caddr_t)0);
      break;
    case '3': /* Quarter 3 */
      placecb((Widget)0, (caddr_t)3, (caddr_t)0);
      break;
    case '4': /* Quarter 4 */
      placecb((Widget)0, (caddr_t)4, (caddr_t)0);
      break;
    case 'z': /*Toggle newZoom */
      {
        double x0, y0, x1, y1;

        x0 = X0(xboxstart);
        x1 = X0(xboxnew);
        y0 = Y0(yboxnew);
        y1 = Y0(yboxstart);
        if ((x0 == x1) && (y0 == y1)) {
          newZoom = !newZoom;
          XmToggleButtonSetState(newZoomToggleButton, newZoom, FALSE);
        } else {
          zoom(x0, y0, x1, y1);
          xboxstart = xboxnew;
          yboxstart = yboxnew;
          displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
        }
      } break;
    case '+': /*zoom in*/
      zoominout((Widget)0, (caddr_t)0, (caddr_t)0);
      break;
    case '-': /*zoom out */
      zoominout((Widget)0, (caddr_t)1, (caddr_t)0);
      break;
    case 't': /* Top */
      placecb((Widget)0, (caddr_t)5, (caddr_t)0);
      break;
    case 'b': /* Bottom */
      placecb((Widget)0, (caddr_t)6, (caddr_t)0);
      break;
    case 'l': /* Left */
      placecb((Widget)0, (caddr_t)7, (caddr_t)0);
      break;
    case 'r': /* Right */
      placecb((Widget)0, (caddr_t)8, (caddr_t)0);
      break;
    case 'c': /* Center */
      placecb((Widget)0, (caddr_t)9, (caddr_t)0);
      break;
    case 'm': /* Movie */
      movie();
      break;
    case 'e': /* write command to emacsclient */
      writeCommandToEmacsServer();
      break;
    case 'i': /* write image to emacsclient */
      writeImageToEmacsServer();
      break;
    default:
      if (buf[0] > 0)
        XBell(display, 100);
      break;
    }
  } break;
  }
}
/**************************** eventmtrack *********************************/
void eventmtrack(Widget w, XtPointer clientdata, XEvent *event,
                 Boolean *contdispatch) {
  double errx, erry, errxp, erryp, errxu, erryu;
  char stringvalue[1024];
  if (!mtrackpopup)
    return;
  switch (event->type) {
  case MotionNotify:
    /* x */
    mtrackx = MTRACKX(X0(event->xbutton.x));
    mtrackxrel = mtrackx - mtrackxref;
    sprintf(stringvalue, mtrackformat, mtrackx);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackxw, args, nargs);
    sprintf(stringvalue, mtrackformat, mtrackxrel);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackxrelw, args, nargs);
    sprintf(stringvalue, mtrackformat, mtrackxref);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackxrefw, args, nargs);
    errxp = .5 * userax / scalex;
    errxp = errxp >= 0 ? errxp : -errxp;
    errxu = .5 * userax;
    errxu = errxu >= 0 ? errxu : -errxu;
    errx = errxp + errxu;
    sprintf(stringvalue, mtrackformat, errx);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackerrxw, args, nargs);
    /* y */
    mtracky = MTRACKY(Y0(event->xbutton.y));
    mtrackyrel = mtracky - mtrackyref;
    sprintf(stringvalue, mtrackformat, mtracky);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackyw, args, nargs);
    sprintf(stringvalue, mtrackformat, mtrackyrel);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackyrelw, args, nargs);
    sprintf(stringvalue, mtrackformat, mtrackyref);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackyrefw, args, nargs);
    erryp = .5 * useray / scaley;
    erryp = erryp >= 0 ? erryp : -erryp;
    erryu = .5 * useray;
    erryu = erryu >= 0 ? erryu : -erryu;
    erry = erryp + erryu;
    sprintf(stringvalue, mtrackformat, erry);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackerryw, args, nargs);

#ifdef MTRACKEXTRAS
    /* Extras */
    sprintf(stringvalue, "%d", event->xbutton.x);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackxpw, args, nargs);

    sprintf(stringvalue, "%d", event->xbutton.y);
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackypw, args, nargs);

    sprintf(stringvalue, mtrackformat, X0(event->xbutton.x));
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtrackx0w, args, nargs);

    sprintf(stringvalue, mtrackformat, Y0(event->xbutton.y));
    nargs = 0;
    XtSetArg(args[nargs], XmNvalue, stringvalue);
    nargs++;
    XtSetValues(mtracky0w, args, nargs);
#endif
    break;
  }
}
/**************************** expose **************************************/
void expose(Widget w, XtPointer clientdata, XtPointer calldata) {
  /* Clear any other expose events to prevent excessive redraws */
  /*
    while(XCheckTypedWindowEvent(display,graphareawindow,Expose,
    &calldata)) ;*/
  /* Do nothing */
  /* Copy pixmap */
  if (pixmap)
    XCopyArea(display, pixmap, graphareawindow, gc, 0, 0, W, H, 0, 0);
}
/**************************** gnuplot *************************************/
void gnuplot(XtPointer cd, int *s, XtInputId *id)
/* Reads from stdin and translates into the commands array */
/* stdin is typically a pipe from the inboard driver */
{
  int displayed = 0, numread, numvals, step, newnc;
  static int nc_max = 0;
  int check = 0, savebuffer = 0, savepipe = 0; /* DEBUG */
  char command;
  char *bufptr = NULL;
#if defined(_WIN32)
  int wincount = 0;
  /* Set "stdin" to have binary mode: */
  _setmode(_fileno(stdin), _O_BINARY);
#else
  if (check)
    xerrmsg(0, "Starting gnuplot");
#endif
  /* Copy the input to a file */
  if (savepipe) {
    char c;
    FILE *file;

    file = fopen("mpl_motif.out", "w");
    while (1) {
      fread(&c, sizeof(char), 1, stdin);
      if (ferror(stdin))
        fprintf(stderr, "Error reading input\n");
      if (feof(stdin)) {
        fclose(file);
        quitplot((Widget)0, (caddr_t)0, (caddr_t)0);
      }
      fwrite(&c, sizeof(char), 1, file);
    }
  }
  /* Read lines from stdin */
  while (1) {
    /* Read one character */
    numread = fread(&command, sizeof(char), 1, stdin);
    if (numread != 1)
      break;
#if defined(_WIN32)
    wincount = 1;
#endif
    if (command == 'G') { /* Enter graphics mode */
      if (check) {
        if (curwrite) {
          xerrmsg(0, "Received G for plot %d at nc=%d\n",
                  curwrite->nplot, curwrite->nc);
        } else {
          xerrmsg(0, "Received G -- no plot defined\n");
        }
      }
      /* Trim any old buffer since we are through writing it */
      if (curwrite) {
        if (check) {
          xerrmsg(0, "G -- Allocating: nc_max=%d nc=%d "
                  "for plot %d\n",
                  nc_max, curwrite->nc, curwrite->nplot);
        }
        curwrite->buffer = (char *)XtRealloc(curwrite->buffer,
                                             curwrite->nc);
        if (check) {
          xerrmsg(0, "G -- Allocated: nc_max=%d nc=%d"
                  " for plot %d\n",
                  nc_max, curwrite->nc, curwrite->nplot);
        }
      }
      /* Make a new plot */
      curwrite = makeplotrec();
      nc_max = 0;

      /* If there is no cur, set it to curwrite, else show old movie frame */
      if (!cur) {
        cur = curwrite;
      } else if (domovie) {
        displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
        XmUpdateDisplay(grapharea);
        usleep((long)(1e6 * movieIntervalTime));
        displayed = 1;
        cur = curwrite;
      }
      status();
      bufptr = curwrite->buffer;
    } else if (!curwrite)
      continue;                /* No record, probably deleted */
    else if (command == 'E') { /* Leave graphics mode */
      if (check) {
        xerrmsg(0, "Received E for plot %d at nc=%d\n",
                curwrite->nplot, curwrite->nc);
      }
      /* Save the buffer to a file */
      if (savebuffer) {
        FILE *file;

        file = fopen("mpl_motif.buf", "w");
        fwrite(curwrite->buffer, sizeof(char), curwrite->nc, file);
        fclose(file);
        quitplot((Widget)0, (caddr_t)0, (caddr_t)0);
      }

      /* Trim the buffer since we are (probably) through writing it */
      if (check) {
        xerrmsg(0, "E -- Allocating: nc_max=%d nc=%d for"
                " plot %d\n",
                nc_max, curwrite->nc, curwrite->nplot);
      }
      curwrite->buffer = (char *)XtRealloc(curwrite->buffer,
                                           curwrite->nc);
      nc_max = curwrite->nc;
      if (check) {
        xerrmsg(0, "E -- Allocated: nc_max=%d nc=%d"
                " for plot %d\n",
                nc_max, curwrite->nc, curwrite->nplot);
      }
      /* Display the plot */
      status();
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
      displayed = 1;
      /* Break out of the loop */
      break;
    } else if (command == 'R') { /* Exit */
      if (check) {
        xerrmsg(0, "Received R for plot %d at nc=%d\n",
                curwrite->nplot, curwrite->nc);
      }
      quitplot((Widget)0, (caddr_t)0, (caddr_t)0);
    }
    /* Determine the space required in addition to commands themselves */
    switch (command) {
    case 'V':
    case 'M':
    case 'P':
      numvals = 2;
      break;
    case 'L':
    case 'W':
      numvals = 1;
      break;
    case 'B':
      numvals = 5;
      break;
    case 'U':
      numvals = 4 * sizeof(double) / sizeof(VTYPE);
      break;
    case 'G':
    case 'R':
    case 'E':
      numvals = 0;
      break;
    case 'C':
      numvals = 3;
      break;
    case 'S':
      numvals = 8;
      break;
    default:
      xerrmsg(0, "Received invalid draw command in gnuplot()\n"
              "  for location %d in plot %d: (%d dec) (%02x hex) %c",
              curwrite->nc, curwrite->nplot, command, command,
              (command > 32 && command < 127) ? command : 32);
      exit(1);
      break;
    }
    /* Allocate more buffer space if necessary */
    step = numvals * sizeof(VTYPE);
    newnc = curwrite->nc + sizeof(char) + step;
    if (newnc >= nc_max) {
      if (check) {
        xerrmsg(0, "Allocating: nc_max=%d nc=%d for"
                " plot %d\n",
                nc_max, curwrite->nc, curwrite->nplot);
      }
      /* (Re)allocate */
      curwrite->buffer = (char *)XtRealloc(curwrite->buffer,
                                           (nc_max += DNC));
      bufptr = curwrite->buffer + curwrite->nc;
      if (check) {
        xerrmsg(0, "Allocated: nc_max=%d nc=%d for"
                " plot %d\n",
                nc_max, curwrite->nc, curwrite->nplot);
      }
    }
    /* Store the commands */
    *bufptr = command;
    bufptr += sizeof(char);
    curwrite->nc++;
    if (numvals) {
      fread(bufptr, sizeof(VTYPE) * numvals, 1, stdin);
      bufptr += step;
      curwrite->nc += step;
    }
  }
  /* Check for errors */
#if defined(_WIN32)
  if (wincount) {
    if (check)
      xerrmsg(0, "Out of loop\n");
    if (ferror(stdin))
      xerrmsg(1, "Error reading input");
    if (feof(stdin)) {
      if (check)
        xerrmsg(0, "Received EOF on stdin\n");
      /*
        XtRemoveInput(inputid);
      */
      XtRemoveWorkProc(workprocid);
      if (!displayed && !domovie)
        displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    }
  } else {
    Sleep(100);
  }
#else
  if (check)
    xerrmsg(0, "Out of loop\n");
  if (ferror(stdin))
    xerrmsg(1, "Error reading input");
  if (feof(stdin)) {
    if (check)
      xerrmsg(0, "Received EOF on stdin\n");
    XtRemoveInput(inputid);
    if (!displayed && !domovie)
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
  }
#endif
  /* Display last frame */
  if (domovie) {
    cur = curwrite;
    displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
  }
}

void gnuplot_new(XtPointer cd, int *s, XtInputId *id)
/* Reads from stdin and translates into the commands array */
/* stdin is typically a pipe from the inboard driver */
{
  int displayed = 0, numread, numvals, step, newnc;
  static int nc_max = 0;
  int check = 0, savebuffer = 0, savepipe = 0; /* DEBUG */
  char command;
  char *bufptr = NULL;
  FILE *input = NULL;

  if (!ifp)
    input = stdin;
  else
    input = ifp;
#if defined(_WIN32)
  int wincount = 0;
  /* Set "stdin" to have binary mode: */
  _setmode(_fileno(input), _O_BINARY);
#else
  if (check)
    xerrmsg(0, "Starting gnuplot");
#endif
  /* Copy the input to a file */
  if (savepipe) {
    char c;
    FILE *file;

    file = fopen("mpl_motif.out", "w");
    while (1) {
      fread(&c, sizeof(char), 1, input);
      if (ferror(input))
        fprintf(stderr, "Error reading input\n");
      if (feof(input)) {
        fclose(file);
        quitplot((Widget)0, (caddr_t)0, (caddr_t)0);
      }
      fwrite(&c, sizeof(char), 1, file);
    }
  }
  /* Read lines from stdin */
  while (1) {
    /* Read one character */
    numread = fread(&command, sizeof(char), 1, input);
    if (numread != 1)
      break;
#if defined(_WIN32)
    wincount = 1;
#endif
    if (command == 'G') { /* Enter graphics mode */
      if (check) {
        if (curwrite) {
          xerrmsg(0, "Received G for plot %d at nc=%d\n",
                  curwrite->nplot, curwrite->nc);
        } else {
          xerrmsg(0, "Received G -- no plot defined\n");
        }
      }
      /* Trim any old buffer since we are through writing it */
      if (curwrite) {
        if (check) {
          xerrmsg(0, "G -- Allocating: nc_max=%d nc=%d "
                  "for plot %d\n",
                  nc_max, curwrite->nc, curwrite->nplot);
        }
        curwrite->buffer = (char *)XtRealloc(curwrite->buffer,
                                             curwrite->nc);
        if (check) {
          xerrmsg(0, "G -- Allocated: nc_max=%d nc=%d"
                  " for plot %d\n",
                  nc_max, curwrite->nc, curwrite->nplot);
        }
      }
      /* Make a new plot */
      curwrite = makeplotrec();
      if (curwrite->nplot == currentPlot) {
        cur = curwrite;
      }
      nc_max = 0;
      /* If there is no cur, set it to curwrite, else show old movie frame */
      if (!cur) {
        cur = curwrite;
      } else if (domovie) {
        displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
        XmUpdateDisplay(grapharea);
        usleep((long)(1e6 * movieIntervalTime));
        displayed = 1;
        cur = curwrite;
      }
      status();
      bufptr = curwrite->buffer;
    } else if (!curwrite)
      continue;                /* No record, probably deleted */
    else if (command == 'E') { /* Leave graphics mode */
      if (check) {
        xerrmsg(0, "Received E for plot %d at nc=%d\n",
                curwrite->nplot, curwrite->nc);
      }
      /* Save the buffer to a file */
      if (savebuffer) {
        FILE *file;

        file = fopen("mpl_motif.buf", "w");
        fwrite(curwrite->buffer, sizeof(char), curwrite->nc, file);
        fclose(file);
        quitplot((Widget)0, (caddr_t)0, (caddr_t)0);
      }

      /* Trim the buffer since we are (probably) through writing it */
      if (check) {
        xerrmsg(0, "E -- Allocating: nc_max=%d nc=%d for"
                " plot %d\n",
                nc_max, curwrite->nc, curwrite->nplot);
      }
      curwrite->buffer = (char *)XtRealloc(curwrite->buffer,
                                           curwrite->nc);
      nc_max = curwrite->nc;
      if (check) {
        xerrmsg(0, "E -- Allocated: nc_max=%d nc=%d"
                " for plot %d\n",
                nc_max, curwrite->nc, curwrite->nplot);
      }
      /* Display the plot */
      status();
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
      displayed = 1;
      /* Break out of the loop */
      break;
    } else if (command == 'R') { /* Exit */
      if (check) {
        xerrmsg(0, "Received R for plot %d at nc=%d\n",
                curwrite->nplot, curwrite->nc);
      }
      quitplot((Widget)0, (caddr_t)0, (caddr_t)0);
    }
    /* Determine the space required in addition to commands themselves */
    switch (command) {
    case 'V':
    case 'M':
    case 'P':
      numvals = 2;
      break;
    case 'L':
    case 'W':
      numvals = 1;
      break;
    case 'B':
      numvals = 5;
      break;
    case 'U':
      numvals = 4 * sizeof(double) / sizeof(VTYPE);
      break;
    case 'G':
    case 'R':
    case 'E':
      numvals = 0;
      break;
    case 'C':
      numvals = 3;
      break;
    case 'S':
      numvals = 8;
      break;
    default:
      xerrmsg(0, "Received invalid draw command in gnuplot()\n"
              "  for location %d in plot %d: (%d dec) (%02x hex) %c",
              curwrite->nc, curwrite->nplot, command, command,
              (command > 32 && command < 127) ? command : 32);
      xerrmsg(0, "\n%c\n", command);
      exit(1);
      break;
    }
    /* Allocate more buffer space if necessary */
    step = numvals * sizeof(VTYPE);
    newnc = curwrite->nc + sizeof(char) + step;
    if (newnc >= nc_max) {
      if (check) {
        xerrmsg(0, "Allocating: nc_max=%d nc=%d for"
                " plot %d\n",
                nc_max, curwrite->nc, curwrite->nplot);
      }
      /* (Re)allocate */
      curwrite->buffer = (char *)XtRealloc(curwrite->buffer,
                                           (nc_max += DNC));
      bufptr = curwrite->buffer + curwrite->nc;
      if (check) {
        xerrmsg(0, "Allocated: nc_max=%d nc=%d for"
                " plot %d\n",
                nc_max, curwrite->nc, curwrite->nplot);
      }
    }
    /* Store the commands */
    *bufptr = command;
    bufptr += sizeof(char);
    curwrite->nc++;
    if (numvals) {
      fread(bufptr, sizeof(VTYPE) * numvals, 1, input);
      bufptr += step;
      curwrite->nc += step;
    }
  }
  /* Check for errors */
#if defined(_WIN32)
  if (wincount) {
    if (check)
      xerrmsg(0, "Out of loop\n");
    if (ferror(input))
      xerrmsg(1, "Error reading input");
    if (feof(input)) {
      if (check)
        xerrmsg(0, "Received EOF on stdin\n");

      /*
        XtRemoveInput(inputid);
      */
      XtRemoveWorkProc(workprocid);
      if (input != stdin) {
        fclose(input);
        unlink(tmpZoomFilename);
      }
      if (!displayed && !domovie)
        displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    }
  } else {
    Sleep(100);
  }
#else
  if (check)
    xerrmsg(0, "Out of loop\n");
  if (ferror(input))
    xerrmsg(1, "Error reading input");
  if (feof(input)) {
    if (check)
      xerrmsg(0, "Received EOF on stdin\n");
    XtRemoveInput(inputid);
    if (input != stdin) {
      fclose(input);
      unlink(tmpZoomFilename);
    }
    if (!displayed && !domovie)
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
  }
#endif

  /* Display last frame */
  if (domovie) {
    cur = curwrite;
    displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
  }
}

struct COORDREC *makecoordrec(void) {
  struct COORDREC *coordrec;
  coordrec = (struct COORDREC *)calloc(1, sizeof(struct COORDREC));
  if (!coordrec) {
    xerrmsg(1, "Could not make a record for a new coordinate");
  } else {
    coordrec->ncoord = ++ncoords;
    coordrec->next = (struct COORDREC *)NULL;
    coordrec->prev = lastcoord;
    coordrec->x0 = 0.0;
    coordrec->y0 = 0.0;
    coordrec->x1 = 0.0;
    coordrec->y1 = 0.0;
    if (lastcoord)
      lastcoord->next = coordrec;
    lastcoord = coordrec;
  }
  return coordrec;
}

/**************************** makeplotrec *********************************/
struct PLOTREC *makeplotrec(void)
/* Makes a new plot record, which is also the last */
{
  struct PLOTREC *plotrec, *rec;
  int nkept;

  plotrec = (struct PLOTREC *)calloc(1, sizeof(struct PLOTREC));
  if (!plotrec) {
    xerrmsg(1, "Could not make a record for a new plot");
  } else {
    plotrec->nplot = ++nplots;
    plotrec->nc = 0;
    plotrec->prev = last;
    plotrec->next = plotrec;
    plotrec->buffer = (char *)NULL;
    if (last)
      last->next = plotrec;
    last = plotrec;
  }
  /* Check for keep limit */
  if (keep > 0) {
    rec = last;
    nkept = 0;
    while (rec) {
      nkept++;
      if (nkept > keep) {
        destroyplotrec(rec);
        break;
      }
      rec = rec->prev;
    }
  }
  /* Return */
  return plotrec;
}
/**************************** menucb **************************************/
void menucb(Widget w, XtPointer clientdata, XtPointer calldata) {
  Widget w1, w2;
  int remanage = 0, save;
  char *value;

  long int n = (long int)clientdata;
  switch (n) {
    /*     case 1000: */
    /* 	setprivatecolormap(); */
    /* 	displayplot((Widget)0,(caddr_t)0,(caddr_t)0); */
    /* 	break; */
  case 0:
    newZoom = XmToggleButtonGetState(w) ? 1 : 0;
    break;
  case 1:
    doublebuffer = XmToggleButtonGetState(w) ? 1 : 0;
    displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    break;
  case 2:
    movie();
    break;
  case 3:
    greyscale = XmToggleButtonGetState(w) ? 1 : 0;
    if (spectrumallocated) {
      XFreeColors(display, cmap, spectrum, nspect, 0);
      if (XtIsManaged(colorpopup)) {
        XDefineCursor(display, mainwindow, watch);
        XtUnmanageChild(colorpopup);
        XFlush(display);
        XtDestroyWidget(colorpopup);
        wselected = (Widget)NULL;
        remanage = 1;
      }
      allocspectrum();
      if (remanage)
        choosecolors();
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    }
    break;
  case 4:
    confirmfile = XmToggleButtonGetState(w) ? 1 : 0;
    break;
  case 5:
    mtracksetup();
    break;
  case 6:
    if (mtrackpopup) {
      if (mtrackon)
        XtRemoveEventHandler(grapharea, PointerMotionMask,
                             FALSE, eventmtrack, (caddr_t)NULL);
      mtrackon = 0;
      XtDestroyWidget(mtrackpopup);
      mtrackpopup = (Widget)NULL;
      XUndefineCursor(display, mainwindow);
    }
    break;
  case 7:
    value = XmTextGetString(mtrackprecisionw);
    mtrackprecision = atoi(value);
    mtrackprecision = mtrackprecision >= 0 ? mtrackprecision : 0;
    mtrackprecision = mtrackprecision <= MTRACKMAXPRECISION ? mtrackprecision : MTRACKMAXPRECISION;
    XFree(value);
    sprintf(mtrackformat, "%% .%d%s", mtrackprecision,
            mtrackformats[mtrackformattype]);
    sprintf(string, "%d", mtrackprecision);
    XmTextSetString(mtrackprecisionw, string);
    break;
  case 8:
    mtrackformattype = 0;
    snprintf(mtrackformat, sizeof(mtrackformat), "%% .%d%.*s",
             mtrackprecision, 4, mtrackformats[mtrackformattype]);
    break;
  case 9:
    mtrackformattype = 1;
    sprintf(mtrackformat, "%% .%d%s", mtrackprecision,
            mtrackformats[mtrackformattype]);
    break;
  case 10:
    mtrackformattype = 2;
    sprintf(mtrackformat, "%% .%d%s", mtrackprecision,
            mtrackformats[mtrackformattype]);
    break;
  case 11:
    save = 0;
    if (printtype == 3)
      mifprint(save);
    else if (printtype == 4)
      pngprint(save);
    else
      psprint(save);
    break;
  case 12:
    save = 1;
    if (savetype == 3)
      mifprint(save);
    else if (savetype == 4)
      pngprint(save);
    else if (savetype == 5)
      mp4print(save);
    else
      psprint(save);
    break;
  case 13:
    dump();
    break;
  case 20:
    XtDestroyWidget(helppopup);
    break;
  case 21:
    cstring = XmStringCreate("Help", XmSTRING_DEFAULT_CHARSET);
    nargs = 0;
    XtSetArg(args[nargs], XmNdialogTitle, cstring);
    nargs++;
    helppopup = XmCreateFormDialog(mainwin, "optPopup", args, nargs);
    XmStringFree(cstring);

    nargs = 0;
    XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_FORM);
    nargs++;
    XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
    nargs++;
    w1 = XmCreatePushButton(helppopup, "Remove", args, nargs);
    XtManageChild(w1);
    XtAddCallback(w1, XmNactivateCallback, menucb, (caddr_t)20);

    w2 = w1;
    nargs = 0;
    XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
    nargs++;
    XtSetArg(args[nargs], XmNtopWidget, w2);
    nargs++;
    XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
    nargs++;
    XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
    nargs++;
    XtSetArg(args[nargs], XmNbottomAttachment, XmATTACH_FORM);
    nargs++;
    XtSetArg(args[nargs], XmNeditMode, XmMULTI_LINE_EDIT);
    nargs++;
    XtSetArg(args[nargs], XmNeditable, FALSE);
    nargs++;
    XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
    nargs++;
    XtSetArg(args[nargs], XmNrows, 25);
    nargs++;
    XtSetArg(args[nargs], XmNcolumns, 80);
    nargs++;
    w1 = XmCreateScrolledText(helppopup, "scrolledHelp", args, nargs);
    XmTextSetString(w1, helptext);
    XtManageChild(w1);

    XtManageChild(helppopup);
    break;
  case 22:
    xinfomsg("%s\n\nWritten by Dwarfs in the Waterfall Glen",
             VERSION);
    break;
  }
}
/**************************** mifprint ************************************/
void mifprint(int save)
/* Makes MIF from the buffer */
{
  double convertx, converty, offx, offy;
  VTYPE x, y, lt;
  int n;
  int endflag = 0;
  int ifile, iborder, icolor, ilandscape, itype;
  char *ifilename;
  char tempname[BUFSIZ];
  char *bufptr, command;
  unsigned short r, g, b;
#if defined(_WIN32)
  char tempcommand[BUFSIZ];
#endif

#define CX(x) (int)((double)(x) * convertx + offx)
#define CY(y) (int)((double)(y) * converty + offy)

  XDefineCursor(display, mainwindow, watch);
  XFlush(display);
  if (printpopup)
    readprinttext();
  /* Set switches */
  if (save) {
    ifile = 1;
    iborder = saveborder;
    icolor = savecolor;
    ilandscape = savelandscape;
    itype = savetype;
    ifilename = savefilename;
  } else {
    ifile = printtofile;
    iborder = printborder;
    icolor = printcolor;
    ilandscape = printlandscape;
    itype = printtype;
    ifilename = printfilename;
  }
  /* Open file or pipe */
  if (ifile) {
#if defined(_WIN32)
    sprintf(tempname, "%s", ifilename);
    outfile = fopen(tempname, "w");
#else
    if (expandname(ifilename, tempname, BUFSIZ))
      outfile = fopen(tempname, "w");
    else
      outfile = NULL;
#endif
    if (outfile == NULL) {
      xerrmsg(1, "Unable to open %s", tempname);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
  } else {
#if defined(_WIN32)
    sprintf(tempname, "%s", ifilename);
    outfile = fopen(tempname, "w");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to open %s", tempname);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
#else
    outfile = popen(printcommand, "w");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to run %s", printcommand);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
#endif
  }
  /* Initialize */
  if (itype == 3) { /* MIF */
    if (ilandscape) {
      convertx = (double)MIF_XMAX / (double)X11_XMAX;
      converty = (double)MIF_YMAX / (double)X11_YMAX;
      offx = .5;
      offy = .5;
      if (!icolor)
        MIF_init();
      else
        CMIF_init();
    } else { /* Currently the same as landscape */
      convertx = (double)MIF_XMAX / (double)X11_XMAX;
      converty = (double)MIF_YMAX / (double)X11_YMAX;
      offx = .5;
      offy = .5;
      if (!icolor)
        MIF_init();
      else
        CMIF_init();
    }
  } else {
    xerrmsg(1, "Invalid MIF file type %d", itype);
    if (mtrackpopup)
      XDefineCursor(display, mainwindow, crosshair);
    else
      XUndefineCursor(display, mainwindow);
    return;
  }
  /* Loop over accumulated commands from inboard driver */
  bufptr = cur->buffer;
  for (n = 0; n < cur->nc;) {
    switch (command = *bufptr++) {
    case 'V': /*   X11_vector(x,y) - draw vector  */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      MIF_vector(CX(x), CY(y));
      break;
    case 'M': /*   X11_move(x,y) - move  */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      MIF_move(CX(x), CY(y));
      break;
    case 'P': /* X11_dot(x, y) -- dot */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      MIF_dot(CX(x), CY(y), 0);
      break;
    case 'L': /*   X11_linetype(type) - set line type  */
      memcpy((char *)&lt, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + sizeof(VTYPE);
      MIF_linetype(lt);
      break;
    case 'W': /*   X11_line_thickness(thickness) - set line width  */
      memcpy((char *)&lt, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + sizeof(VTYPE);
      MIF_line_thickness(lt);
      break;
    case 'B': /* Fill Box */
      {
        VTYPE shade, xl, xh, yl, yh;

        memcpy((char *)&shade, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&xl, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&xh, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&yl, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&yh, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 5 * sizeof(VTYPE);
        shade = shade % nspect;
        MIF_fill_box(shade, CX((int)xl), CX((int)xh), CY((int)yl), CY((int)yh));
      } break;
    case 'U': /*   X11_user(ax,bx,ay,by) - user coordinate scaling  */
      memcpy((char *)&userax, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&userbx, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&useray, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&userby, bufptr, sizeof(double));
      bufptr += sizeof(double);
      n += sizeof(char) + 4 * sizeof(double);
      break;
    case 'G':
      n += sizeof(char);
      MIF_graphics();
      if (iborder) {
        MIF_move(0, 0);
        MIF_vector(MIF_XMAX, 0);
        MIF_vector(MIF_XMAX, MIF_YMAX);
        MIF_vector(0, MIF_YMAX);
        MIF_vector(0, 0);
      }
      break;
    case 'E':
      n += sizeof(char);
      endflag = 1;
      MIF_text();
      break;
    case 'R':
      n += sizeof(char);
      break;
    case 'C': /*   X11_linetype(type) - set line type  */
      memcpy((char *)&r, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&g, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&b, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 3 * sizeof(VTYPE);
      break;
    case 'S': /* Allocate spectral spectrum */
      {
        VTYPE num, spec, r0, g0, b0, r1, g1, b1;
        memcpy((char *)&num, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&spec, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&r0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&g0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&b0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&r1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&g1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&b1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 8 * sizeof(VTYPE);
        nspect = num;
        if (spec == 0) {
          customspectral = 1;
          spectral = 0;
          red0 = r0;
          green0 = g0;
          blue0 = b0;
          red1 = r1;
          green1 = g1;
          blue1 = b1;
        } else if (spec == 1) {
          customspectral = 0;
          spectral = 1;
        } else if (spec == 2) {
          customspectral = 0;
          spectral = 2;
        } else if (spec == 3) {
          customspectral = 0;
          spectral = 3;
        } else if (spec == 4) {
          customspectral = 0;
          spectral = 4;
        } else {
          customspectral = 0;
          spectral = 0;
        }
      } break;
    default:
      xerrmsg(1, "Received invalid draw command in mifprint()\n"
              "  for location %d in plot %d: (%d dec) (%02x hex) %c",
              cur->nc, cur->nplot, command, command,
              (command > 32 && command < 127) ? command : 32);
      break;
    }
  } /* End for loop */
  /* Terminate */
  if (!endflag) {
    MIF_text();
    MIF_reset();
  }
  if (ifile)
    fclose(outfile);
  else {
#if defined(_WIN32)
    fclose(outfile);
#else
    pclose(outfile);
#endif
  }
  if (ferror(outfile)) {
    /*xerrmsg(1,"File or Pipe Error");*/
  } else {
#if defined(_WIN32)
    sprintf(tempcommand, "%s %s", printcommand, printfilebuffer);
    outfile = popen(tempcommand, "w");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to run %s", printcommand);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
    pclose(outfile);
#endif
    XBell(display, 20);
    if (ifile && confirmfile)
      xinfomsg("Print file is %s\n  %s\n  %s\n  %s\n  %s",
               tempname,
               itype == 1 ? "Postscript" : (itype == 2 ? "EPSF" : (itype == 3 ? "MIF" : "PNG")),
               ilandscape ? "Landscape" : "Portrait",
               icolor ? "Color" : "B&W",
               iborder ? "Border" : "No Border");
  }
  if (mtrackpopup)
    XDefineCursor(display, mainwindow, crosshair);
  else
    XUndefineCursor(display, mainwindow);
#undef CX
#undef CY
}

/**************************** pngprint ************************************/
void mp4print(int save) {
  char command[10240];
  char tempname[BUFSIZ];
  const char *extension = ".mp4";
  size_t filename_len = strlen(savefilename);
  size_t extension_len = strlen(extension);

#if defined(_WIN32)
  sprintf(tempname, "%s", savefilename);
#else
  expandname(savefilename, tempname, BUFSIZ);
#endif
  filename_len = strlen(tempname);

  if ((filename_len >= extension_len) && (strcmp(tempname + filename_len - extension_len, extension) == 0)) {
    tempname[filename_len - extension_len] = '\0';
  }
  sprintf(command, "%s -device=png,rootname=%s,movie", sddsplotCommandline2, tempname);
  system(command);
}
/**************************** pngprint ************************************/
void pngprint(int save)
/* Makes PNG from the buffer */
{
  double convertx, converty, offx, offy;
  VTYPE x, y, lt;
  int n;
  int endflag = 0;
  int ifile, iborder, icolor, ilandscape, itype;
  char *ifilename;
  char tempname[BUFSIZ];
  char *bufptr, command;
  unsigned short r, g, b;
#if defined(_WIN32)
  char tempcommand[BUFSIZ];
#endif

#define CX(x) (int)((double)(x) * convertx + offx)
#define CY(y) (int)((double)(y) * converty + offy)

  XDefineCursor(display, mainwindow, watch);
  XFlush(display);
  if (printpopup)
    readprinttext();
  /* Set switches */
  if (save) {
    ifile = 1;
    iborder = saveborder;
    icolor = savecolor;
    ilandscape = savelandscape;
    itype = savetype;
    ifilename = savefilename;
  } else {
    ifile = printtofile;
    iborder = printborder;
    icolor = printcolor;
    ilandscape = printlandscape;
    itype = printtype;
    ifilename = printfilename;
  }
  /* Open file or pipe */

  if (ifile) {
#if defined(_WIN32)
    sprintf(tempname, "%s", ifilename);
    outfile = fopen(tempname, "wb");
#else
    if (expandname(ifilename, tempname, BUFSIZ))
      outfile = fopen(tempname, "wb");
    else
      outfile = NULL;
#endif
    if (outfile == NULL) {
      xerrmsg(1, "Unable to open %s", tempname);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
  } else {
#if defined(_WIN32)
    sprintf(tempname, "%s", ifilename);
    outfile = fopen(tempname, "wb");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to open %s", tempname);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
#else
    outfile = popen(printcommand, "wb");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to run %s", printcommand);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
#endif
  }

  if (UseDashes) {
    char **png_devargv;
    png_devargv = (char **)malloc(sizeof(*png_devargv) * 1);
    png_devargv[0] = "dashes";
    setDeviceArgv(png_devargv, 1);
  }
  /* Initialize */
  if (itype == 4) { /* PNG */
    if (ilandscape) {
      convertx = (double)LPNG_XMAX / (double)X11_XMAX;
      converty = (double)LPNG_YMAX / (double)X11_YMAX;
      offx = .5;
      offy = .5;
      LPNG_init();
    } else { /* Currently the same as landscape */
      convertx = (double)LPNG_XMAX / (double)X11_XMAX;
      converty = (double)LPNG_YMAX / (double)X11_YMAX;
      offx = .5;
      offy = .5;
      LPNG_init();
    }
  } else {
    xerrmsg(1, "Invalid PNG file type %d", itype);
    if (mtrackpopup)
      XDefineCursor(display, mainwindow, crosshair);
    else
      XUndefineCursor(display, mainwindow);
    return;
  }
  /* Loop over accumulated commands from inboard driver */
  bufptr = cur->buffer;
  for (n = 0; n < cur->nc;) {
    switch (command = *bufptr++) {
    case 'V': /*   X11_vector(x,y) - draw vector  */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      PNG_vector(CX(x), CY(y));
      break;
    case 'M': /*   X11_move(x,y) - move  */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      PNG_move(CX(x), CY(y));
      break;
    case 'P': /* X11_dot(x, y) -- dot */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      PNG_dot(CX(x), CY(y), 0);
      break;
    case 'L': /*   X11_linetype(type) - set line type  */
      memcpy((char *)&lt, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + sizeof(VTYPE);
      PNG_linetype(lt);
      break;
    case 'W': /*   X11_line_thickness(thickness) - set line width  */
      memcpy((char *)&lt, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + sizeof(VTYPE);
      PNG_line_thickness(lt);
      break;
    case 'B': /* Fill Box */
      {
        VTYPE shade, xl, xh, yl, yh;

        memcpy((char *)&shade, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&xl, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&xh, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&yl, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&yh, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 5 * sizeof(VTYPE);
        shade = shade % nspect;
        PNG_fill_box(shade, CX((int)xl), CX((int)xh), CY((int)yl), CY((int)yh));
      } break;
    case 'U': /*   X11_user(ax,bx,ay,by) - user coordinate scaling  */
      memcpy((char *)&userax, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&userbx, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&useray, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&userby, bufptr, sizeof(double));
      bufptr += sizeof(double);
      n += sizeof(char) + 4 * sizeof(double);
      break;
    case 'G':
      n += sizeof(char);
      PNG_graphics();
      if (iborder) {
        PNG_move(0, 0);
        PNG_vector(GPNG_XMAX, 0);
        PNG_vector(GPNG_XMAX, GPNG_YMAX);
        PNG_vector(0, GPNG_YMAX);
        PNG_vector(0, 0);
      }
      break;
    case 'E':
      n += sizeof(char);
      endflag = 1;
      PNG_passCommandLineToPNGFromMotif(sddsplotCommandline2);
      PNG_text();
      break;
    case 'R':
      n += sizeof(char);
      break;
    case 'C': /*   X11_linetype(type) - set line type  */
      memcpy((char *)&r, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&g, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&b, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 3 * sizeof(VTYPE);
      if (icolor)
        PNG_color(r, g, b);
      break;
    case 'S': /* Allocate spectral spectrum */
      {
        VTYPE num, spec, r0, g0, b0, r1, g1, b1;
        memcpy((char *)&num, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&spec, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&r0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&g0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&b0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&r1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&g1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&b1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 8 * sizeof(VTYPE);
        nspect = num;
        if (spec == 0) {
          customspectral = 1;
          spectral = 0;
          red0 = r0;
          green0 = g0;
          blue0 = b0;
          red1 = r1;
          green1 = g1;
          blue1 = b1;
        } else if (spec == 1) {
          customspectral = 0;
          spectral = 1;
        } else if (spec == 2) {
          customspectral = 0;
          spectral = 2;
        } else if (spec == 3) {
          customspectral = 0;
          spectral = 3;
        } else if (spec == 4) {
          customspectral = 0;
          spectral = 4;
        } else {
          customspectral = 0;
          spectral = 0;
        }
        PNG_spectral(num, spec, r0, g0, b0, r1, g1, b1);
      } break;
    default:
      xerrmsg(1, "Received invalid draw command in pngprint()\n"
              "  for location %d in plot %d: (%d dec) (%02x hex) %c",
              cur->nc, cur->nplot, command, command,
              (command > 32 && command < 127) ? command : 32);
      break;
    }
  } /* End for loop */
  /* Terminate */
  if (!endflag) {
    PNG_passCommandLineToPNGFromMotif(sddsplotCommandline2);
    PNG_text();
    PNG_reset();
  }

  if (ifile)
    fclose(outfile);
  else {
#if defined(_WIN32)
    fclose(outfile);
#else
    pclose(outfile);
#endif
  }
  if (ferror(outfile)) {
    /*xerrmsg(1,"File or Pipe Error");*/
  } else {
#if defined(_WIN32)
    sprintf(tempcommand, "%s %s", printcommand, printfilebuffer);
    outfile = popen(tempcommand, "w");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to run %s", printcommand);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
    pclose(outfile);
#endif
    XBell(display, 20);
    if (ifile && confirmfile)
      xinfomsg("Print file is %s\n  %s\n  %s\n  %s\n  %s",
               tempname,
               itype == 1 ? "Postscript" : (itype == 2 ? "EPSF" : (itype == 3 ? "MIF" : "PNG")),
               ilandscape ? "Landscape" : "Portrait",
               icolor ? "Color" : "B&W",
               iborder ? "Border" : "No Border");
  }

  if (mtrackpopup)
    XDefineCursor(display, mainwindow, crosshair);
  else
    XUndefineCursor(display, mainwindow);

#undef CX
#undef CY
}
/**************************** movie ***************************************/
void movie(void) {
  struct PLOTREC *first, *rec;

  /* Check if there are plots */
  if (cur == (struct PLOTREC *)NULL) {
    xerrmsg(1, "No plots");
  }
  /* Find the first plot */
  rec = first = cur;
  while (rec) {
    first = rec;
    rec = rec->prev;
  }
  /* Clear the screen */
  if (doublebuffer)
    XClearWindow(display, graphareawindow);
  /* Display all the plots */
  rec = first;
  while (1) {
    cur = rec;
    status();
    displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    if (rec == last)
      break;
    rec = rec->next;
    XmUpdateDisplay(grapharea);
    usleep((long)(1e6 * movieIntervalTime));
  }
}

void getzoomfactor(Widget w, XtPointer clientdata, XtPointer calldata) {
  XmSelectionBoxCallbackStruct *cbs;
  char *text;

  cbs = (XmSelectionBoxCallbackStruct *)calldata;
  XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &text);
  zoomFactor = atof(text);

  if (zoomFactor <= 0 || zoomFactor >= 1)
    zoomFactor = 0.2;
}

/*zoom factor setup */
void zoomfactorcb(Widget w, XtPointer clientdata, XtPointer calldata) {
  Widget wdialog;
  XmString prompt, text;

  prompt = XmStringCreateSimple("Enter change factor:");
  sprintf(string, "%.1f", zoomFactor);
  text = XmStringCreateSimple(string);

  nargs = 0;
  XtSetArg(args[nargs], XmNselectionLabelString, prompt);
  nargs++;
  XtSetArg(args[nargs], XmNautoUnmanage, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNtextString, text);
  nargs++;
  XtSetArg(args[nargs], XmNtitle, "Change Factor");
  nargs++;
  wdialog = XmCreatePromptDialog(mainwin, "prompt", args, nargs);
  XmStringFree(prompt);
  XmStringFree(text);
  XtAddCallback(wdialog, XmNokCallback, getzoomfactor, (caddr_t)0);
  XtAddCallback(wdialog, XmNcancelCallback, (XtCallbackProc)XtDestroyWidget,
                (caddr_t)NULL);
  XtSetSensitive(XmSelectionBoxGetChild(wdialog,
                                        XmDIALOG_HELP_BUTTON),
                 FALSE);
  XtManageChild(wdialog);
}

/**************************** mtracksetup *********************************/
void mtracksetup(void) {
  Widget w, w1, w2;
  char stringvalue[1024];

  /* Check if scaling has been defined */
  if (userax == 0. && userbx == 0. &&
      useray == 0. && userby == 0.) {
    xerrmsg(1, "Mouse tracking is not available for this data");
    return;
  }
  XDefineCursor(display, mainwindow, watch);
  XFlush(display);
  cstring = XmStringCreate("Mouse Tracker", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNdialogTitle, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNautoUnmanage, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNdefaultPosition, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNx, 0);
  nargs++;
  XtSetArg(args[nargs], XmNy, 0);
  nargs++;
  mtrackpopup = XmCreateFormDialog(mainwin, "optPopup", args, nargs);
  XmStringFree(cstring);

  /* Define row column for buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(mtrackpopup, "switchBox", args, nargs);
  XtManageChild(w1);

  /* Define remove button */
  nargs = 0;
  w = XmCreatePushButton(w1, "Remove", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)6);

  /* Define precision button */
  /* Use row/column to keep text field the right height */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w2 = XmCreateRowColumn(w1, "mtrackBox", args, nargs);
  XtManageChild(w2);

  cstring = XmStringCreate("Precision:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w2, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  sprintf(mtrackformat, "%% .%d%s", mtrackprecision,
          mtrackformats[mtrackformattype]);
  sprintf(string, "%d", mtrackprecision);
  nargs = 0;
  XtSetArg(args[nargs], XmNmaxLength, 2);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, 2);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, string);
  nargs++;
  w = XmCreateTextField(w2, "mtrackText", args, nargs);
  mtrackprecisionw = w;
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)7);

  /* Define format menu */
  nargs = 0;
  w2 = XmCreatePulldownMenu(w1, "mtrackPulldownMenu", args, nargs);
  /* (Don't manage it) */

  /* Add pushbuttons to format menu */
  nargs = 0;
  w = XmCreatePushButton(w2, mtrackformats[0], args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)8);

  nargs = 0;
  w = XmCreatePushButton(w2, mtrackformats[1], args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)9);

  nargs = 0;
  w = XmCreatePushButton(w2, mtrackformats[2], args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)10);

  /* Define option menu button and attach format menu to it */
  cstring = XmStringCreate("Format:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNsubMenuId, w2);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  w = XmCreateOptionMenu(w1, "mtrackMenu", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  /* Define row column for x */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(mtrackpopup, "mtrackBox", args, nargs);
  XtManageChild(w1);

  /* Define x box */
  cstring = XmStringCreate("x:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackxw = w;
  XtManageChild(w);

  /* Define errorx box */
  cstring = XmStringCreate("+/-", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackerrxw = w;
  XtManageChild(w);

  /* Define row column for y */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(mtrackpopup, "mtrackBox", args, nargs);
  XtManageChild(w1);

  /* Define y box */
  cstring = XmStringCreate("y:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackyw = w;
  XtManageChild(w);

  /* Define errory box */
  cstring = XmStringCreate("+/-", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackerryw = w;
  XtManageChild(w);

  /* Define row column for xrel/xref */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(mtrackpopup, "mtrackBox", args, nargs);
  XtManageChild(w1);

  /* Define xrel box */
  cstring = XmStringCreate("xrel:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackxrelw = w;
  XtManageChild(w);

  /* Define xref box */
  cstring = XmStringCreate("xref:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  sprintf(stringvalue, mtrackformat, mtrackxref);
  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, stringvalue);
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackxrefw = w;
  XtManageChild(w);

  /* Define row column for yrel/yref */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
#ifndef MTRACKEXTRAS
  XtSetArg(args[nargs], XmNbottomAttachment, XmATTACH_FORM);
  nargs++;
#endif
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(mtrackpopup, "mtrackBox", args, nargs);
  XtManageChild(w1);

  /* Define yrel box */
  cstring = XmStringCreate("yrel:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackyrelw = w;
  XtManageChild(w);

  /* Define yref box */
  cstring = XmStringCreate("yref:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  sprintf(stringvalue, mtrackformat, mtrackyref);
  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, stringvalue);
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackyrefw = w;
  XtManageChild(w);

#ifdef MTRACKEXTRAS
  /* Define row column for xpix and ypix */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(mtrackpopup, "mtrackBox", args, nargs);
  XtManageChild(w1);

  /* Define xpix box */
  cstring = XmStringCreate("xpix:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackxpw = w;
  XtManageChild(w);

  /* Define ypix box */
  cstring = XmStringCreate("ypix:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackypw = w;
  XtManageChild(w);

  /* Define row column for x0 and y0 */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNbottomAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(mtrackpopup, "mtrackBox", args, nargs);
  XtManageChild(w1);

  /* Define x0 box */
  cstring = XmStringCreate("x0:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtrackx0w = w;
  XtManageChild(w);

  /* Define y0 box */
  cstring = XmStringCreate("y0:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w = XmCreateLabel(w1, "mtrackLabel", args, nargs);
  XtManageChild(w);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNeditable, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPositionVisible, FALSE);
  nargs++;
  XtSetArg(args[nargs], XmNcolumns, MTRACKVALUESIZE);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, " ");
  nargs++;
  w = XmCreateTextField(w1, "mtrackText", args, nargs);
  mtracky0w = w;
  XtManageChild(w);
#endif

  /* Manage */
  XtManageChild(mtrackpopup);

  /* Add event handler */
  XtAddEventHandler(grapharea, PointerMotionMask,
                    FALSE, eventmtrack, (caddr_t)NULL);
  mtrackon = 1;

  /* Finish */
  if (mtrackpopup)
    XDefineCursor(display, mainwindow, crosshair);
  else
    XUndefineCursor(display, mainwindow);
}
/**************************** navigatecb **********************************/
void navigatecb(Widget w, XtPointer clientdata, XtPointer calldata) {
  long int n;
  int errmsg = 0;

  n = (long int)clientdata;
  switch (n) {
  case 1:
    errmsg = 1;
  case 11:
    if (cur == (struct PLOTREC *)NULL) {
      if (errmsg)
        xerrmsg(1, "No plots");
      else
        XBell(display, 50);
    } else if (cur == last) {
      if (errmsg)
        xerrmsg(1, "This is the last plot");
      else
        XBell(display, 50);
    } else {
      cur = cur->next;
      status();
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    }
    break;
  case 2:
    errmsg = 1;
  case 12:
    if (cur == (struct PLOTREC *)NULL) {
      if (errmsg)
        xerrmsg(1, "No plots");
      else
        XBell(display, 50);
    } else if (cur->prev == (struct PLOTREC *)NULL) {
      if (errmsg)
        xerrmsg(1, "This is the first plot");
      else
        XBell(display, 50);
    } else {
      cur = cur->prev;
      status();
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    }
    break;
  case 3:
    errmsg = 1;
  case 13:
    if (cur == (struct PLOTREC *)NULL) {
      if (errmsg)
        xerrmsg(1, "No plots");
      else
        XBell(display, 50);
    } else {
      destroyplotrec(cur);
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
      status();
    }
    break;
  case 4: {
    Widget wdialog;
    XmString prompt, text;

    prompt = XmStringCreateSimple("Enter plot number:");
    sprintf(string, "%d", (cur->next)->nplot);
    text = XmStringCreateSimple(string);
    nargs = 0;
    XtSetArg(args[nargs], XmNselectionLabelString, prompt);
    nargs++;
    XtSetArg(args[nargs], XmNautoUnmanage, FALSE);
    nargs++;
    XtSetArg(args[nargs], XmNtextString, text);
    nargs++;
    XtSetArg(args[nargs], XmNtitle, "Plot Number");
    nargs++;
    wdialog = XmCreatePromptDialog(w, "prompt", args, nargs);
    XmStringFree(prompt);
    XmStringFree(text);
    XtAddCallback(wdialog, XmNokCallback, navigatecb, (caddr_t)5);
    XtAddCallback(wdialog, XmNcancelCallback, (XtCallbackProc)XtDestroyWidget,
                  (caddr_t)NULL);
    XtSetSensitive(XmSelectionBoxGetChild(wdialog,
                                          XmDIALOG_HELP_BUTTON),
                   FALSE);
    XtManageChild(wdialog);
  } break;
  case 5: {
    XmSelectionBoxCallbackStruct *cbs;
    struct PLOTREC *rec = last;
    char *text;
    int newval, found = 0;

    cbs = (XmSelectionBoxCallbackStruct *)calldata;
    XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &text);
    newval = atoi(text);
    /*  XmStringFree((unsigned char *)text);*/
    while (rec) {
      if (rec->nplot == newval) {
        found = 1;
        break;
      }
      rec = rec->prev;
    }
    if (found) {
      XtDestroyWidget(w);
      cur = rec;
      status();
      displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
    } else {
      xerrmsg(1, "Cannot find Plot %d", newval);
    }
  } break;
  }
}
/**************************** placecb *************************************/
void placecb(Widget w, XtPointer clientdata, XtPointer calldata) {
  int x, y, height, width;
  int screenheight, screenwidth;
  long int n;
  screenheight = DisplayHeight(display, screen);
  screenwidth = DisplayWidth(display, screen);

  n = (long int)clientdata;
  switch (n) {
  case 0:
    width = screenwidth - placeleft - placeright;
    height = screenheight - placetop - placebottom;
    x = placeleft;
    y = placetop;
    break;
  case 1:
    width = (screenwidth - placeleft - placeright - placehspace) / 2;
    height = (screenheight - placetop - placebottom - placevspace) / 2;
    x = placeleft;
    y = placetop;
    break;
  case 2:
    width = (screenwidth - placeleft - placeright - placehspace) / 2;
    height = (screenheight - placetop - placebottom - placevspace) / 2;
    x = placeleft + placehspace + width;
    y = placetop;
    break;
  case 3:
    width = (screenwidth - placeleft - placeright - placehspace) / 2;
    height = (screenheight - placetop - placebottom - placevspace) / 2;
    x = placeleft;
    y = placetop + placevspace + height;
    break;
  case 4:
    width = (screenwidth - placeleft - placeright - placehspace) / 2;
    height = (screenheight - placetop - placebottom - placevspace) / 2;
    x = placeleft + placehspace + width;
    y = placetop + placevspace + height;
    break;
  case 5:
    width = screenwidth - placeleft - placeright;
    height = (screenheight - placetop - placebottom - placevspace) / 2;
    x = placeleft;
    y = placetop;
    break;
  case 6:
    width = screenwidth - placeleft - placeright;
    height = (screenheight - placetop - placebottom - placevspace) / 2;
    x = placeleft;
    y = placetop + placevspace + height;
    break;
  case 7:
    width = (screenwidth - placeleft - placeright - placehspace) / 2;
    height = screenheight - placetop - placebottom;
    x = placeleft;
    y = placetop;
    break;
  case 8:
    width = (screenwidth - placeleft - placeright - placehspace) / 2;
    height = screenheight - placetop - placebottom;
    x = placeleft + placehspace + width;
    y = placetop;
    break;
  case 9:
    width = .75 * screenwidth;
    height = .75 * screenheight;
    x = .125 * screenwidth;
    y = .100 * screenwidth;
    break;
  case 10:
    /*enter zoom factor */
    zoomfactorcb((Widget)0, (caddr_t)0, (caddr_t)0);
    return;
  default:
    xerrmsg(1, "invalid data (placecb)");
    exit(1);
    break;
  }
  nargs = 0;
  XtSetArg(args[nargs], XmNx, x);
  nargs++;
  XtSetArg(args[nargs], XmNy, y);
  nargs++;
  XtSetArg(args[nargs], XmNwidth, width);
  nargs++;
  XtSetArg(args[nargs], XmNheight, height);
  nargs++;
  XtSetValues(appshell, args, nargs);
}
/**************************** psprint *************************************/
void psprint(int save)
/* Makes postscript from the buffer */
{
  double convertx, converty, offx, offy;
  VTYPE x, y, lt;
  int n;
  int endflag = 0;
  int ifile, iborder, icolor, ilandscape, itype;
  char *ifilename;
  char tempname[BUFSIZ];
  char *bufptr, command;
  unsigned short r, g, b;
#if defined(_WIN32)
  char tempcommand[BUFSIZ];
#endif

#define CX(x) (int)((double)(x) * convertx + offx)
#define CY(y) (int)((double)(y) * converty + offy)

  XDefineCursor(display, mainwindow, watch);
  XFlush(display);
  if (printpopup)
    readprinttext();
  /* Set switches */
  if (save) {
    ifile = 1;
    iborder = saveborder;
    icolor = savecolor;
    ilandscape = savelandscape;
    itype = savetype;
    ifilename = savefilename;
  } else {
    ifile = printtofile;
    iborder = printborder;
    icolor = printcolor;
    ilandscape = printlandscape;
    itype = printtype;
    ifilename = printfilename;
  }
  /* Open file or pipe */
  if (ifile) {
#if defined(_WIN32)
    sprintf(tempname, "%s", ifilename);
    outfile = fopen(tempname, "w");
#else
    if (expandname(ifilename, tempname, BUFSIZ))
      outfile = fopen(tempname, "w");
    else
      outfile = NULL;
#endif
    if (outfile == NULL) {
      xerrmsg(1, "Unable to open %s", tempname);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
  } else {
#if defined(_WIN32)
    sprintf(tempname, "%s", printfilebuffer);
    outfile = fopen(tempname, "w");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to open %s", tempname);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
#else
    outfile = popen(printcommand, "w");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to run %s", printcommand);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
#endif
  }
  if (UseDashes) {
    char **ps_devargv;
    ps_devargv = malloc(sizeof(*ps_devargv) * 1);
    ps_devargv[0] = "dashes";
    setDeviceArgv(ps_devargv, 1);
  }
  PS_passCommandLineToPSFromMotif(sddsplotCommandline2);
  /* Initialize */
  if (itype == 1) { /* PS */
    if (ilandscape) {
      convertx = (double)PS_XMAX / (double)X11_XMAX;
      converty = (double)PS_YMAX / (double)X11_YMAX;
      offx = .5;
      offy = .5;
      if (!icolor)
        PS1_init();
      else
        CPS1_init();
    } else {
      convertx = (double)PS_XMAX / (double)X11_XMAX;
      converty = (double)PS_YMAX / (double)X11_YMAX;
      offx = .5;
      offy = .5;
      if (!icolor)
        PS1_init_rot();
      else
        CPS1_init_rot();
    }
  } else if (itype == 2) { /* EPSF */
    if (ilandscape) {
      convertx = (double)PS_XMAX / (double)X11_XMAX;
      converty = (double)PS_YMAX / (double)X11_YMAX;
      offx = .5;
      offy = .5;
      EPSF1_init();
    } else { /* Currently same as landscape */
      convertx = (double)PS_XMAX / (double)X11_XMAX;
      converty = (double)PS_YMAX / (double)X11_YMAX;
      offx = .5;
      offy = .5;
      EPSF1_init();
    }
  } else {
    xerrmsg(1, "Invalid PS or EPSF print mode %d", itype);
    if (mtrackpopup)
      XDefineCursor(display, mainwindow, crosshair);
    else
      XUndefineCursor(display, mainwindow);
    return;
  }
  /* Loop over accumulated commands from inboard driver */
  bufptr = cur->buffer;
  for (n = 0; n < cur->nc;) {
    switch (command = *bufptr++) {
    case 'V': /*   X11_vector(x,y) - draw vector  */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      PS_vector(CX(x), CY(y));
      break;
    case 'M': /*   X11_move(x,y) - move  */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      PS_move(CX(x), CY(y));
      break;
    case 'P': /* X11_dot(x, y) -- dot */
      memcpy((char *)&x, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&y, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 2 * sizeof(VTYPE);
      PS_dot(CX(x), CY(y), 0);
      break;
    case 'L': /*   X11_linetype(type) - set line type  */
      memcpy((char *)&lt, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + sizeof(VTYPE);
      if (!icolor)
        PS_linetype(lt);
      else
        CPS_linetype(lt);
      break;
    case 'W': /*   X11_line_thickness(thickness) - set line thickness  */
      memcpy((char *)&lt, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + sizeof(VTYPE);
      PS_line_thickness(lt);
      break;
    case 'B': /* Fill Box */
      {
        VTYPE shade, xl, xh, yl, yh;

        memcpy((char *)&shade, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&xl, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&xh, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&yl, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&yh, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 5 * sizeof(VTYPE);
        shade = shade % nspect;
        if (!icolor)
          PS_fill_box(shade, CX((int)xl), CX((int)xh), CY((int)yl), CY((int)yh));
        /**/ else
          CPS_fill_box(shade, CX((int)xl), CX((int)xh), CY((int)yl), CY((int)yh));
      } break;
    case 'U': /*   X11_user(ax,bx,ay,by) - user coordinate scaling  */
      memcpy((char *)&userax, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&userbx, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&useray, bufptr, sizeof(double));
      bufptr += sizeof(double);
      memcpy((char *)&userby, bufptr, sizeof(double));
      bufptr += sizeof(double);
      n += sizeof(char) + 4 * sizeof(double);
      break;
    case 'G':
      n += sizeof(char);
      if (itype == 1) { /* PS */
        if (!icolor)
          PS_graphics();
        else
          CPS_graphics();
      } else { /* EPSF */
        if (!icolor)
          EPSF_graphics();
        else
          CEPSF_graphics();
      }
      if (iborder) {
        PS_move(0, 0);
        PS_vector(PS_XMAX, 0);
        PS_vector(PS_XMAX, PS_YMAX);
        PS_vector(0, PS_YMAX);
        PS_vector(0, 0);
      }
      break;
    case 'E':
      n += sizeof(char);
      endflag = 1;
      if (itype == 1)
        PS_text();
      else
        EPSF_text();
      break;
    case 'R':
      n += sizeof(char);
      break;
    case 'C': /*   X11_linetype(type) - set line type  */
      memcpy((char *)&r, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&g, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      memcpy((char *)&b, bufptr, sizeof(VTYPE));
      bufptr += sizeof(VTYPE);
      n += sizeof(char) + 3 * sizeof(VTYPE);
      if (icolor && !lineTypeTable.nEntries)
        CPS_color(r, g, b);
      break;
    case 'S': /* Allocate spectral spectrum */
      {
        VTYPE num, spec, r0, g0, b0, r1, g1, b1;
        memcpy((char *)&num, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&spec, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&r0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&g0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&b0, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&r1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&g1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&b1, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 8 * sizeof(VTYPE);
        nspect = num;
        if (spec == 0) {
          customspectral = 1;
          spectral = 0;
          red0 = r0;
          green0 = g0;
          blue0 = b0;
          red1 = r1;
          green1 = g1;
          blue1 = b1;
        } else if (spec == 1) {
          customspectral = 0;
          spectral = 1;
        } else if (spec == 2) {
          customspectral = 0;
          spectral = 2;
        } else if (spec == 3) {
          customspectral = 0;
          spectral = 3;
        } else if (spec == 4) {
          customspectral = 0;
          spectral = 4;
        } else {
          customspectral = 0;
          spectral = 0;
        }
        CPS_spectral(num, spec, r0, g0, b0, r1, g1, b1);
      } break;
    default:
      xerrmsg(1, "Received invalid draw command in psprint()\n"
              "  for location %d in plot %d: (%d dec) (%02x hex) %c",
              cur->nc, cur->nplot, command, command,
              (command > 32 && command < 127) ? command : 32);
      break;
    }
  } /* End for loop */
  /* Terminate */
  if (!endflag) {
    if (itype == 1) { /* PS */
      PS_text();
      PS_reset();
    } else { /* EPSF */
      EPSF_text();
      EPSF_reset();
    }
  }
  if (ifile)
    fclose(outfile);
  else {
#if defined(_WIN32)
    fclose(outfile);
#else
    pclose(outfile);
#endif
  }
  if (ferror(outfile)) {
    /*xerrmsg(1,"File or Pipe Error");*/
  } else {
#if defined(_WIN32)
    sprintf(tempcommand, "%s %s", printcommand, printfilebuffer);
    outfile = popen(tempcommand, "w");
    if (outfile == NULL) {
      xerrmsg(1, "Unable to run %s", printcommand);
      if (mtrackpopup)
        XDefineCursor(display, mainwindow, crosshair);
      else
        XUndefineCursor(display, mainwindow);
      return;
    }
    pclose(outfile);
#endif
    XBell(display, 20);
    if (ifile && confirmfile)
      xinfomsg("Print file is %s\n  %s\n  %s\n  %s\n  %s",
               tempname,
               itype == 1 ? "Postscript" : (itype == 2 ? "EPSF" : (itype == 3 ? "MIF" : "PNG")),
               ilandscape ? "Landscape" : "Portrait",
               icolor ? "Color" : "B&W",
               iborder ? "Border" : "No Border");
  }
  if (mtrackpopup)
    XDefineCursor(display, mainwindow, crosshair);
  else
    XUndefineCursor(display, mainwindow);
#undef CX
#undef CY
}
/**************************** printcb *************************************/
void printcb(Widget w, XtPointer clientdata, XtPointer calldata) {
  long int n;

  n = (long int)clientdata;
  switch (n) {
  case 0:
    printsetup();
    break;
  case 1:
    if (printpopup) {
      readprinttext();
      XtDestroyWidget(printpopup);
      printpopup = (Widget)NULL;
    }
    break;
  case 10:
    if (printpopup)
      readprinttext();
    break;
  case 21:
    if (XmToggleButtonGetState(w))
      printtofile = 0;
    break;
  case 22:
    if (XmToggleButtonGetState(w))
      printtofile = 1;
    break;
  case 23:
    if (XmToggleButtonGetState(w))
      printlandscape = 1;
    break;
  case 24:
    if (XmToggleButtonGetState(w))
      printlandscape = 0;
    break;
  case 25:
    if (XmToggleButtonGetState(w))
      printcolor = 0;
    break;
  case 26:
    if (XmToggleButtonGetState(w))
      printcolor = 1;
    break;
  case 27:
    printborder = XmToggleButtonGetState(w) ? 1 : 0;
    break;
  case 31:
    if (XmToggleButtonGetState(w))
      dumptofile = 0;
    break;
  case 32:
    if (XmToggleButtonGetState(w))
      dumptofile = 1;
    break;
  case 34:
    if (XmToggleButtonGetState(w))
      dumptofile = 1;
    break;
  case 41:
    if (XmToggleButtonGetState(w))
      printtype = 1;
    change_extension(printfilename, BUFSIZ, ".ps");
    XmTextFieldSetString(wprintfilename, printfilename);
    break;
  case 42:
    if (XmToggleButtonGetState(w))
      printtype = 2;
    change_extension(printfilename, BUFSIZ, ".eps");
    XmTextFieldSetString(wprintfilename, printfilename);
    break;
  case 43:
    if (XmToggleButtonGetState(w))
      printtype = 3;
    change_extension(printfilename, BUFSIZ, ".mif");
    XmTextFieldSetString(wprintfilename, printfilename);
    break;
  case 53:
    if (XmToggleButtonGetState(w))
      savelandscape = 1;
    break;
  case 54:
    if (XmToggleButtonGetState(w))
      savelandscape = 0;
    break;
  case 55:
    if (XmToggleButtonGetState(w))
      savecolor = 0;
    break;
  case 56:
    if (XmToggleButtonGetState(w))
      savecolor = 1;
    break;
  case 57:
    saveborder = XmToggleButtonGetState(w) ? 1 : 0;
    break;
  case 61:
    if (XmToggleButtonGetState(w))
      savetype = 1;
    change_extension(savefilename, BUFSIZ, ".ps");
    XmTextFieldSetString(wsavefilename, savefilename);
    break;
  case 62:
    if (XmToggleButtonGetState(w))
      savetype = 2;
    change_extension(savefilename, BUFSIZ, ".eps");
    XmTextFieldSetString(wsavefilename, savefilename);
    break;
  case 63:
    if (XmToggleButtonGetState(w))
      savetype = 3;
    change_extension(savefilename, BUFSIZ, ".mif");
    XmTextFieldSetString(wsavefilename, savefilename);
    break;
  case 64:
    if (XmToggleButtonGetState(w))
      savetype = 4;
    change_extension(savefilename, BUFSIZ, ".png");
    XmTextFieldSetString(wsavefilename, savefilename);
    break;
  case 65:
    if (XmToggleButtonGetState(w)) {
      savetype = 5;
      change_extension(savefilename, BUFSIZ, ".mp4");
      XmTextFieldSetString(wsavefilename, savefilename);
    }
    break;
  }
}
/**************************** printsetup **********************************/
void printsetup(void) {
  Widget w, w1, w1sav;
  int len;

  XDefineCursor(display, mainwindow, watch);
  XFlush(display);
  cstring = XmStringCreate("Print Setup", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNdialogTitle, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNautoUnmanage, FALSE);
  nargs++;
  printpopup = XmCreateFormDialog(mainwin, "optPopup", args, nargs);
  XmStringFree(cstring);

  /* Define row column for buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(printpopup, "switchBox", args, nargs);
  XtManageChild(w1);

  /* Define remove button */
  nargs = 0;
  w = XmCreatePushButton(w1, "Remove", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, printcb, (caddr_t)1);

  w1sav = w1;

  /* Start of Print options */

  /* Separator */
  w1 = w1sav;
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  w1 = XmCreateSeparator(printpopup, "separator", args, nargs);
  XtManageChild(w1);

  /* Define title box */
  cstring = XmStringCreate("PRINT", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_CENTER);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "saveLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  /* Define print command box */
  cstring = XmStringCreate("Print Command:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "printLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  len = strlen(printcommand);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPosition, len);
  nargs++;
  XtSetArg(args[nargs], XmNmaxLength, BUFSIZ - 1);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, printcommand);
  nargs++;
  w1 = XmCreateTextField(printpopup, "printText", args, nargs);
  wprintcommand = w1;
  XtManageChild(w1);
  XtAddCallback(w1, XmNactivateCallback, printcb, (caddr_t)10);

  /* Define print file box */
  cstring = XmStringCreate("Print File:",
                           XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "printLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  len = strlen(printfilename);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPosition, len);
  nargs++;
  XtSetArg(args[nargs], XmNmaxLength, BUFSIZ - 1);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, printfilename);
  nargs++;
  w1 = XmCreateTextField(printpopup, "printText", args, nargs);
  wprintfilename = w1;
  XtManageChild(w1);
  XtAddCallback(w1, XmNactivateCallback, printcb, (caddr_t)10);

  /* Define first print options box */
  cstring = XmStringCreate("Print Options:",
                           XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "printLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(printpopup, "switchBox", args, nargs);
  XtManageChild(w1);
  w1sav = w1;

  /* Print/File buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_TIGHT);
  nargs++;
  w1 = XmCreateSimpleRadioBox(w1sav, "radioButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Printer", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)21);
  if (!printtofile)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "File", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)22);
  if (printtofile)
    XmToggleButtonSetState(w, TRUE, TRUE);

#if !defined(_WIN32)
  /* Space */
  nargs = 0;
  XtSetArg(args[nargs], XmNseparatorType, XmNO_LINE);
  nargs++;
  w1 = XmCreateSeparator(w1sav, "space", args, nargs);
  XtManageChild(w1);
#endif

  /* Orientation buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_TIGHT);
  nargs++;
  w1 = XmCreateSimpleRadioBox(w1sav, "radioButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Landscape", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)23);
  if (printlandscape)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Portrait", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)24);
  if (!printlandscape)
    XmToggleButtonSetState(w, TRUE, TRUE);

#if !defined(_WIN32)
  /* Space */
  nargs = 0;
  XtSetArg(args[nargs], XmNseparatorType, XmNO_LINE);
  nargs++;
  w1 = XmCreateSeparator(w1sav, "space", args, nargs);
  XtManageChild(w1);
#endif

  /* Border button */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(w1sav, "pushButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Border", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)27);
  if (printborder)
    XmToggleButtonSetState(w, TRUE, TRUE);

  /* Define second print options box */
  w1 = w1sav;
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(printpopup, "switchBox", args, nargs);
  XtManageChild(w1);
  w1sav = w1;

  /* Color buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_TIGHT);
  nargs++;
  w1 = XmCreateSimpleRadioBox(w1sav, "radioButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "B & W", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)25);
  if (!printcolor)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Color", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)26);
  if (printcolor)
    XmToggleButtonSetState(w, TRUE, TRUE);

#if !defined(_WIN32)
  /* Space */
  nargs = 0;
  XtSetArg(args[nargs], XmNseparatorType, XmNO_LINE);
  nargs++;
  w1 = XmCreateSeparator(w1sav, "space", args, nargs);
  XtManageChild(w1);
#endif

  /* Mode buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_TIGHT);
  nargs++;
  w1 = XmCreateSimpleRadioBox(w1sav, "radioButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "PS", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)41);
  if (printtype == 1)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "EPSF", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)42);
  if (printtype == 2)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "MIF", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)43);
  if (printtype == 3)
    XmToggleButtonSetState(w, TRUE, TRUE);

#if !defined(_WIN32)
  /* Space */
  nargs = 0;
  XtSetArg(args[nargs], XmNseparatorType, XmNO_LINE);
  nargs++;
  w1 = XmCreateSeparator(w1sav, "space", args, nargs);
  XtManageChild(w1);
#endif

  /* Print */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(w1sav, "pushButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreatePushButton(w1, "Print", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)11);

  /* End of Print options */
  /* Start of Save options */

  /* Separator */
  w1 = w1sav;
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  w1 = XmCreateSeparator(printpopup, "separator", args, nargs);
  XtManageChild(w1);

  /* Define title box */
  cstring = XmStringCreate("SAVE", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_CENTER);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "saveLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  /* Define save file box */
  cstring = XmStringCreate("Save File:",
                           XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "saveLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  len = strlen(savefilename);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPosition, len);
  nargs++;
  XtSetArg(args[nargs], XmNmaxLength, BUFSIZ - 1);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, savefilename);
  nargs++;
  w1 = XmCreateTextField(printpopup, "saveText", args, nargs);
  wsavefilename = w1;
  XtManageChild(w1);
  XtAddCallback(w1, XmNactivateCallback, printcb, (caddr_t)10);

  /* Define first save options box */
  cstring = XmStringCreate("Save Options:",
                           XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "saveLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(printpopup, "switchBox", args, nargs);
  XtManageChild(w1);
  w1sav = w1;

  /* Orientation buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_TIGHT);
  nargs++;
  w1 = XmCreateSimpleRadioBox(w1sav, "radioButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Landscape", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)53);
  if (savelandscape)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Portrait", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)54);
  if (!savelandscape)
    XmToggleButtonSetState(w, TRUE, TRUE);

#if !defined(_WIN32)
  /* Space */
  nargs = 0;
  XtSetArg(args[nargs], XmNseparatorType, XmNO_LINE);
  nargs++;
  w1 = XmCreateSeparator(w1sav, "space", args, nargs);
  XtManageChild(w1);
#endif

  /* Border button */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(w1sav, "pushButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Border", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)57);
  if (saveborder)
    XmToggleButtonSetState(w, TRUE, TRUE);

  /* Define second save options box */
  w1 = w1sav;
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(printpopup, "switchBox", args, nargs);
  XtManageChild(w1);
  w1sav = w1;

  /* Color buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_TIGHT);
  nargs++;
  w1 = XmCreateSimpleRadioBox(w1sav, "radioButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "B & W", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)55);
  if (!savecolor)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Color", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)56);
  if (savecolor)
    XmToggleButtonSetState(w, TRUE, TRUE);

#if !defined(_WIN32)
  /* Space */
  nargs = 0;
  XtSetArg(args[nargs], XmNseparatorType, XmNO_LINE);
  nargs++;
  w1 = XmCreateSeparator(w1sav, "space", args, nargs);
  XtManageChild(w1);
#endif

  /* Mode buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_TIGHT);
  nargs++;
  w1 = XmCreateSimpleRadioBox(w1sav, "radioButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "PS", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)61);
  if (savetype == 1)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "EPSF", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)62);
  if (savetype == 2)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "MIF", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)63);
  if (savetype == 3)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "PNG", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)64);
  if (savetype == 4)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "MP4", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)65);
  if (savetype == 5)
    XmToggleButtonSetState(w, TRUE, TRUE);

#if !defined(_WIN32)
  /* Space */
  nargs = 0;
  XtSetArg(args[nargs], XmNseparatorType, XmNO_LINE);
  nargs++;
  w1 = XmCreateSeparator(w1sav, "space", args, nargs);
  XtManageChild(w1);
#endif

  /* Save */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(w1sav, "pushButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreatePushButton(w1, "Save", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)12);

  /* End of Save options */
  /* Start of Dump options */

  /* Separator */
  w1 = w1sav;
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  w1 = XmCreateSeparator(printpopup, "separator", args, nargs);
  XtManageChild(w1);

  /* Define title box */
  cstring = XmStringCreate("DUMP", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_CENTER);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "saveLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  /* Define dump command box */
  cstring = XmStringCreate("Screen Dump Command:", XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "printLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  len = strlen(dumpcommand);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPosition, len);
  nargs++;
  XtSetArg(args[nargs], XmNmaxLength, BUFSIZ - 1);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, dumpcommand);
  nargs++;
  w1 = XmCreateTextField(printpopup, "printText", args, nargs);
  wdumpcommand = w1;
  XtManageChild(w1);
  XtAddCallback(w1, XmNactivateCallback, printcb, (caddr_t)10);

  /* Define dump file box */
  cstring = XmStringCreate("Screen Dump File:",
                           XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "printLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  len = strlen(dumpfilename);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNcursorPosition, len);
  nargs++;
  XtSetArg(args[nargs], XmNmaxLength, BUFSIZ - 1);
  nargs++;
  XtSetArg(args[nargs], XmNvalue, dumpfilename);
  nargs++;
  w1 = XmCreateTextField(printpopup, "printText", args, nargs);
  wdumpfilename = w1;
  XtManageChild(w1);
  XtAddCallback(w1, XmNactivateCallback, printcb, (caddr_t)10);

  /* Define dump options box */
  cstring = XmStringCreate("Screen Dump Options:",
                           XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNalignment, XmALIGNMENT_BEGINNING);
  nargs++;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetArg(args[nargs], XmNtraversalOn, FALSE);
  nargs++;
  w1 = XmCreateLabel(printpopup, "printLabel", args, nargs);
  XtManageChild(w1);
  XmStringFree(cstring);

  nargs = 0;
  XtSetArg(args[nargs], XmNtopAttachment, XmATTACH_WIDGET);
  nargs++;
  XtSetArg(args[nargs], XmNtopWidget, w1);
  nargs++;
  XtSetArg(args[nargs], XmNleftAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNrightAttachment, XmATTACH_FORM);
  nargs++;
  XtSetArg(args[nargs], XmNbottomAttachment, XmATTACH_FORM);
  nargs++;
  /* Bottom widget */
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(printpopup, "switchBox", args, nargs);
  XtManageChild(w1);
  w1sav = w1;

  /* Dump/File buttons */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  XtSetArg(args[nargs], XmNpacking, XmPACK_TIGHT);
  nargs++;
  w1 = XmCreateSimpleRadioBox(w1sav, "radioButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreateToggleButton(w1, "Printer", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)31);
  if (printlandscape)
    XmToggleButtonSetState(w, TRUE, TRUE);

  nargs = 0;
  w = XmCreateToggleButton(w1, "File", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNvalueChangedCallback, printcb, (caddr_t)32);
  if (!printlandscape)
    XmToggleButtonSetState(w, TRUE, TRUE);

#if !defined(_WIN32)
  /* Space */
  nargs = 0;
  XtSetArg(args[nargs], XmNseparatorType, XmNO_LINE);
  nargs++;
  w1 = XmCreateSeparator(w1sav, "space", args, nargs);
  XtManageChild(w1);
#endif

  /* Apply */
  nargs = 0;
  XtSetArg(args[nargs], XmNorientation, XmHORIZONTAL);
  nargs++;
  w1 = XmCreateRowColumn(w1sav, "pushButtons", args, nargs);
  XtManageChild(w1);

  nargs = 0;
  w = XmCreatePushButton(w1, "Screen Dump", args, nargs);
  XtManageChild(w);
  XtAddCallback(w, XmNactivateCallback, menucb, (caddr_t)13);

  /* Manage */
  XtManageChild(printpopup);
  if (mtrackpopup)
    XDefineCursor(display, mainwindow, crosshair);
  else
    XUndefineCursor(display, mainwindow);
}
/**************************** quit ****************************************/
void quitplot(Widget w, XtPointer clientdata, XtPointer calldata) {
#if !defined(_WIN32)
  if (killparent)
    kill(getppid(), SIGTERM);
#endif
  XtDestroyWidget(appshell);
  exit(0);
}
/**************************** readprinttext *********************************/
void readprinttext(void)
/* Reads the text fields in the print popup */
{
  char *value;

  value = XmTextFieldGetString(wprintcommand);
  strcpy(printcommand, value);
  XFree(value);
  value = XmTextFieldGetString(wprintfilename);
  strcpy(printfilename, value);
  XFree(value);
  value = XmTextFieldGetString(wsavefilename);
  strcpy(savefilename, value);
  XFree(value);
  value = XmTextFieldGetString(wdumpcommand);
  strcpy(dumpcommand, value);
  XFree(value);
  value = XmTextFieldGetString(wdumpfilename);
  strcpy(dumpfilename, value);
  XFree(value);
}
/**************************** resize ****************************************/
void resize(Widget w, XtPointer clientdata, XtPointer calldata) {
  Window root;
  unsigned int border, depth, width, height;
  int x0, y0;

  /* Get window geometry */
  XGetGeometry(display, graphareawindow, &root, &x0, &y0, &width, &height,
               &border, &depth);
  H = height;
  W = width;

  restore();
  displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
}

void restore_plot(void) {
  /* int x,y,height,width;
     int screenheight,screenwidth;

     screenheight=DisplayHeight(display,screen);
     screenwidth=DisplayWidth(display,screen); */

  H = orig_height;
  W = orig_width;
  restore();

  nargs = 0;
  /*  x=.125*screenwidth;
      y=.100*screenwidth;
      XtSetArg(args[nargs],XmNx,x); nargs++;
      XtSetArg(args[nargs],XmNy,y); nargs++;*/
  XtSetArg(args[nargs], XmNwidth, orig_width);
  nargs++;
  XtSetArg(args[nargs], XmNheight, orig_height);
  nargs++;
  XtSetValues(appshell, args, nargs);
}

/**************************** zoom in out ****************************************/
void zoominout(Widget w, XtPointer clientdata, XtPointer calldata) {
  Window root;
  unsigned int border, depth, width, height;
  int x0, y0, nargs = 0;
  long int opt;
  /* Get window geometry */
  XGetGeometry(display, mainwindow, &root, &x0, &y0, &width, &height,
               &border, &depth);
  opt = (long int)clientdata;
  if (!opt) {
    height *= 1.0 + zoomFactor;
    width *= 1.0 + zoomFactor;
  } else {
    height *= 1.0 - zoomFactor;
    width *= 1.0 - zoomFactor;
  }
  nargs = 0;
  /*  XtSetArg(args[nargs],XmNx,x0); nargs++;
      XtSetArg(args[nargs],XmNy,y0); nargs++; */
  XtSetArg(args[nargs], XmNwidth, width);
  nargs++;
  XtSetArg(args[nargs], XmNheight, height);
  nargs++;
  XtSetValues(appshell, args, nargs);
}

/**************************** restore ***************************************/
void restore(void) {
  scalex = ((double)W - 1.) / (double)XMAX;
  scaley = ((double)H - 1.) / (double)YMAX;
  offsetx0 = offsety0 = 0.;
  offsetx = offsetx0 - .5;
  offsety = offsety0 - .5;
  x0old = 0.;
  y0old = 0.;
  x1old = XMAX;
  y1old = YMAX;
  deltax = x1old - x0old;
  deltay = y1old - y0old;
  /* DEBUG */
  /*     printf("restore: W=%d H=%d scalex=%g offsetx0=%g scaley=%g offsety0=%g deltax=%g deltay=%g\n", */
  /*       W,H,scalex,offsetx0,scaley,offsety0,deltax,deltay); */
}

void restore_new(void) {
  newzoom(0, 0, 0, 0);
}

/**************************** rubberband **********************************/
void rubberband(Window window, Position *xstart, Position *ystart,
                Position *xend, Position *yend) {
  static GC gcxor = (GC)NULL;
  XEvent event;
  Dimension w = 0, h = 0;
  int x, y;
  /* (Dimension is unsigned short, but XDrawRectangle works     */
  /*    if we treat w and h as signed and let them be negative) */

  /* Create gc */
  if (!gcxor)
    gcxor = XCreateGC(display, graphareawindow, 0, NULL);
  XSetBackground(display, gcxor, colors[1]);
  /*    XSetForeground(display,gcxor,~0);*/ /* One's complement */
  XSetForeground(display, gcxor, colors[0] ^ colors[1]);
  XSetFunction(display, gcxor, GXxor);

  /* Initialize */
  x = *xend = *xstart;
  y = *yend = *ystart;
  XGrabPointer(display, window, FALSE,
               ButtonMotionMask | ButtonReleaseMask,
               GrabModeAsync, GrabModeAsync, window,
               crosshair, CurrentTime);
  /*XGrabServer(display);*/ /*This causes problems with VNC*/
  XDrawRectangle(display, window, gcxor, x, y, w, h);
  /* Loop until button released */
  while (TRUE) {
    XtAppNextEvent(appcontext, &event);
    switch (event.type) {
    case MotionNotify:
      XDrawRectangle(display, window, gcxor, x, y, w, h);
      *xend = event.xbutton.x;
      *yend = event.xbutton.y;

      if (*xstart <= *xend) {
        w = *xend - *xstart;
        x = *xstart;
      } else {
        w = *xstart - *xend;
        x = *xend;
      }
      if (*ystart <= *yend) {
        h = *yend - *ystart;
        y = *ystart;
      } else {
        h = *ystart - *yend;
        y = *yend;
      }
      XDrawRectangle(display, window, gcxor, x, y, w, h);
      break;
    case ButtonRelease:
      XDrawRectangle(display, window, gcxor, x, y, w, h);
      *xend = event.xbutton.x;
      *yend = event.xbutton.y;
      if (*xstart <= *xend) {
        w = *xend - *xstart;
        x = *xstart;
      } else {
        w = *xstart - *xend;
        x = *xend;
      }
      if (*ystart <= *yend) {
        h = *yend - *ystart;
        y = *ystart;
      } else {
        h = *ystart - *yend;
        y = *yend;
      }
      XDrawRectangle(display, window, gcxor, x, y, w, h);
      /*XUngrabServer(display);*/
      XUngrabPointer(display, CurrentTime);
      return;
    default:
      XtDispatchEvent(&event);
    }
  }
}
/**************************** setprivatecolormap **************************/
int setprivatecolormap(void) {
  int n, ok = 1;

  fprintf(stderr, "Setting private colormap\n");
  /* Free allocated old colors */
  for (n = 0; n < NCOLORS; n++) {
    if (colorsalloc[n])
      XFreeColors(display, cmap, colors + n, 1, 0);
  }
  for (n = 0; n < nspect; n++) {
    if (spectrumalloc[n])
      XFreeColors(display, cmap, spectrum + n, 1, 0);
  }
  /* Set private colormap */
  cmap = XCreateColormap(display, RootWindow(display, screen), defvisual,
                         AllocNone);
  nargs = 0;
  XtSetArg(args[nargs], XmNcolormap, cmap);
  nargs++;
  XtSetValues(appshell, args, nargs);
  privatecolormap = 1;
  /* Allocate new colors */
  if (!alloccolors())
    ok = 0;
  if (dospectrum && !allocspectrum())
    ok = 0;
  /* Return */
  return ok;
}
/**************************** sigpipe *************************************/
void sigpipe(int signum) {
  static int nerrors = 0;

  nerrors++;
  if (nerrors < 10) {
    xerrmsg(1, "Error with piped command");
  } else if (nerrors == 10) {
    xerrmsg(1, "Error with piped command (Last notification)");
#if !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif
  }
}
/**************************** status **************************************/
void status(void) {
  if (cur) {
    sprintf(string, "Plot %d of %d", cur->nplot, nplots);
  } else {
    sprintf(string, "No plots");
  }
  cstring = XmStringCreateLtoR(string, XmSTRING_DEFAULT_CHARSET);
  nargs = 0;
  XtSetArg(args[nargs], XmNlabelString, cstring);
  nargs++;
  XtSetValues(statusbar, args, nargs);
  XmStringFree(cstring);
}
/**************************** xerrmsg *************************************/
/*
  void xerrmsg(char *fmt, ...)
  {
  #define MAXERRS 25

  Widget child;
  va_list vargs;
  static char lstring[BUFSIZ];
  static int errcount=0,errflag=0;

  // Stop if too many errors
  if(errflag) {
  // 	va_start(vargs,fmt);
  // 	(void)vfprintf(stderr,fmt,vargs);
  // 	va_end(vargs);
  return;
  }
  if(++errcount > MAXERRS) {
  va_start(vargs,fmt);
  (void)vfprintf(stderr,fmt,vargs);
  va_end(vargs);
  sprintf(lstring,"\nToo many errors\nNo more will be displayed this session");
  errflag=1;
  }
  else {
  va_start(vargs,fmt);
  (void)vsprintf(lstring,fmt,vargs);
  va_end(vargs);
  }

  if(lstring[0] != '\0') {
  if(windowmessage) {
  XBell(display,50); XBell(display,50); XBell(display,50);
  cstring=XmStringCreateLtoR(lstring,XmSTRING_DEFAULT_CHARSET);
  nargs=0;
  XtSetArg(args[nargs],XmNtitle,"Warning"); nargs++;
  XtSetArg(args[nargs],XmNmessageString,cstring); nargs++;
  warningbox=XmCreateWarningDialog(mainwin,"warningMessage",
  args,nargs);
  XmStringFree(cstring);
  child=XmMessageBoxGetChild(warningbox,XmDIALOG_CANCEL_BUTTON);
  XtDestroyWidget(child);
  child=XmMessageBoxGetChild(warningbox,XmDIALOG_HELP_BUTTON);
  XtDestroyWidget(child);
  XtManageChild(warningbox);
  XtAddCallback(warningbox,XmNokCallback,(XtCallbackProc)XtDestroyWidget,NULL);
  fprintf(stderr,"%s\n",lstring);
  }
  else {
  fprintf(stderr,"%s\n",lstring);
  }

  }
  }*/
/**************************** xinfomsg ************************************/
void xinfomsg(char *fmt, ...) {
  Widget child;
  va_list vargs;
  static char lstring[BUFSIZ];

  va_start(vargs, fmt);
  (void)vsprintf(lstring, fmt, vargs);
  va_end(vargs);

  if (lstring[0] != '\0') {
    cstring = XmStringCreateLtoR(lstring, XmSTRING_DEFAULT_CHARSET);
    nargs = 0;
    XtSetArg(args[nargs], XmNtitle, "Information");
    nargs++;
    XtSetArg(args[nargs], XmNmessageString, cstring);
    nargs++;
    infobox = XmCreateInformationDialog(mainwin, "informationMessage",
                                        args, nargs);
    XmStringFree(cstring);
    child = XmMessageBoxGetChild(infobox, XmDIALOG_CANCEL_BUTTON);
    XtDestroyWidget(child);
    child = XmMessageBoxGetChild(infobox, XmDIALOG_HELP_BUTTON);
    XtDestroyWidget(child);
    XtManageChild(infobox);
    XtAddCallback(infobox, XmNokCallback, (XtCallbackProc)XtDestroyWidget, NULL);
  }
}
/**************************** zoom ****************************************/
void zoom(double x0, double y0, double x1, double y1) {
  int x0p = X(x0), y0p = Y(y0), x1p = X(x1), y1p = Y(y1);

  /* Check for zero-length box */
  if (x0p == x1p) {
    xerrmsg(1, "Zoom window has zero width");
    return;
  }
  if (y0p == y1p) {
    xerrmsg(1, "Zoom window has zero height");
    return;
  }
  if (sddsplotCommandline2 && newZoom) {
    newzoom(x0, y0, x1, y1);
    return;
  }

  /* x */
  x0old = x0;
  x1old = x1;
  deltax = x1 - x0;
  scalex = ((double)W - 1.) / deltax;
  offsetx0 = scalex * x0;
  offsetx = offsetx0 - .5;
  /* y */
  y0old = y0;
  y1old = y1;
  deltay = y1 - y0;
  scaley = ((double)H - 1.) / deltay;
  offsety0 = scaley * (YMAX - y1);
  offsety = offsety0 - .5;

  /* DEBUG */
  /*     printf("zoom: W=%d H=%d scalex=%g offsetx0=%g scaley=%g offsety0=%g deltax=%g deltay=%g\n", */
  /*       W,H,scalex,offsetx0,scaley,offsety0,deltax,deltay); */
}

void newzoom(double x0, double y0, double x1, double y1) {

  char *cmd = NULL;
  char *original = NULL;
  char *op = NULL;
  char *tmp = NULL;
  char *_tmp1 = NULL;
  char *_tmp2 = NULL;
  char *tmp2 = malloc(sizeof(char) * 256);
  int len0 = strlen(sddsplotCommandline2) + 1024;
  long is_global;
  int limitAdded = 0;

  struct stat fstatus;
  cmd = malloc(sizeof(char) * len0);
  strcpy(cmd, "");
  strcpy(tmp2, "");

  if (fexists(tmpZoomFilename)) {
    xerrmsg(1, "Last plot operations not completed. Try later!");
    return;
  }

  if (!(x0 == 0 && x1 == 0 && y0 == 0 && y1 == 0)) {
    double xminLimit, xmaxLimit, yminLimit, ymaxLimit, xmult, ymult, xoff, yoff;
    double xmult_global, ymult_global, xoff_global, yoff_global;
    int xlog, ylog;
    int xlog_global, ylog_global;
    xminLimit = MTRACKX(x0);
    yminLimit = MTRACKY(y0);
    xmaxLimit = MTRACKX(x1);
    ymaxLimit = MTRACKY(y1);
    xmult = 1.0;
    ymult = 1.0;
    xoff = 0.0;
    yoff = 0.0;
    xlog = 0;
    ylog = 0;
    xmult_global = 1.0;
    ymult_global = 1.0;
    xoff_global = 0.0;
    yoff_global = 0.0;
    xlog_global = 0;
    ylog_global = 0;

    original = malloc(sizeof(char) * (int)(strlen(sddsplotCommandline2) + 256));
    strcpy(original, sddsplotCommandline2);
    is_global = 1;
    while ((op = get_token_t(original, "'"))) {
      tmp = malloc(sizeof(char) * (int)(strlen(op) + 256));
      str_tolower(strcpy(tmp, op));
      if (is_blank(op)) {
        XFree(tmp);
        XFree(op);
        continue;
      }
      op = trim_spaces(op);
      if ((strncmp(tmp, "-lim", 4) == 0) ||
          (strncmp(tmp, "-sc", 3) == 0) ||
          (strncmp(tmp, "-zo", 3) == 0)) {
        XFree(tmp);
        XFree(op);
        continue;
      }

      if (strncmp(tmp, "-mo", 3) == 0) {
        if (strstr(tmp, "=n") || strstr(tmp, "=o") || strstr(tmp, "=e") || strstr(tmp, "=m") || strstr(tmp, "=co") || strstr(tmp, "=ce") || strstr(tmp, "=f")) {
          xerrmsg(1, "New Zoom feature can not handle option -mode with normalize, offset, eoffset,center, meanCenter,coffset,coffset, and fractionalDeviation keywords");
          XFree(tmp);
          XFree(op);
          XFree(cmd);
          XFree(original);
          return;
        }
        if (strstr(tmp, "=lo")) {
          if (strstr(tmp, "y=lo")) {
            ylog = 1;
            if (is_global)
              ylog_global = 1;
          }
          if (strstr(tmp, "x=lo")) {
            xlog = 1;
            if (is_global)
              xlog_global = 1;
          }

          if (!limitAdded) {
            sprintf(tmp, "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                    xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                    xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                    ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                    ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);
            cmd = realloc(cmd, sizeof(char) * (len0 += 128));
            strcat(cmd, tmp);
            limitAdded = 1;
          }
        }
      }

      if ((strncmp(tmp, "-col", 4) == 0) || (strncmp(tmp, "-par", 4) == 0)) {
        if (!limitAdded) {
          sprintf(tmp, "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                  xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                  xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                  ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                  ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);

          cmd = realloc(cmd, sizeof(char) * (len0 += 128));

          strcat(cmd, tmp);
          limitAdded = 1;
        }
        if (is_global)
          is_global = 0;
        xmult = xmult_global;
        ymult = ymult_global;
        yoff = yoff_global;
        xoff = xoff_global;
        xlog = xlog_global;
        ylog = ylog_global;
      }

      if (strncmp(tmp, "-fa", 3) == 0) {
        if ((_tmp1 = strstr(tmp, "ym"))) {
          _tmp2 = strstr(_tmp1, "=");
          ymult = atof(_tmp2 + 1);
          if (is_global)
            ymult_global = atof(_tmp2 + 1);
        }
        if ((_tmp1 = strstr(tmp, "xm"))) {
          _tmp2 = strstr(_tmp1, "=");
          xmult = atof(_tmp2 + 1);
          if (is_global)
            xmult_global = atof(_tmp2 + 1);
        }
        if (!limitAdded) {
          sprintf(tmp, "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                  xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                  xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                  ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                  ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);

          cmd = realloc(cmd, sizeof(char) * (len0 += 128));
          strcat(cmd, tmp);
          limitAdded = 1;
        }
      }
      if (strncmp(tmp, "-of", 3) == 0) {
        if ((_tmp1 = strstr(tmp, "yc"))) {
          _tmp2 = strstr(_tmp1, "=");
          yoff = atof(_tmp2 + 1);
          if (is_global)
            yoff_global = atof(_tmp2 + 1);
        }
        if ((_tmp1 = strstr(tmp, "xc"))) {
          _tmp2 = strstr(_tmp1, "=");
          xoff = atof(_tmp2 + 1);
          if (is_global)
            xoff_global = atof(_tmp2 + 1);
        }
        if (!limitAdded) {
          sprintf(tmp, "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                  xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                  xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                  ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                  ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);

          cmd = realloc(cmd, sizeof(char) * (len0 += 128));
          strcat(cmd, tmp);
          limitAdded = 1;
        }
      }

      if (op[0] != '"') {
        strcat(cmd, "'");
        strcat(cmd, op);
        strcat(cmd, "' ");
      } else {
        strcat(cmd, " ");
        strcat(cmd, op);
        strcat(cmd, " ");
      }
      XFree(tmp);
      XFree(op);
    }

    XFree(original);
    strcat(cmd, tmp2);
  } else { /* restore to original command */
    strcpy(cmd, sddsplotCommandline2);
  }

  strcat(cmd, " -output=");
  strcpy(tmpZoomFilename, "");
  tmpname(tmp2);
#if defined(_WIN32)
  strcat(tmpZoomFilename, tmp2);
#else
  strcat(tmpZoomFilename, "/tmp/");
  strcat(tmpZoomFilename, tmp2);
#endif
  strcat(cmd, tmpZoomFilename);
  /* fprintf(stderr,"%s\n",cmd); */
  system(cmd);
  XFree(cmd);
  XFree(tmp2);
  do {
    if (fexists(tmpZoomFilename))
      break;
  } while (1);

  stat(tmpZoomFilename, &fstatus);

  if (fstatus.st_size) {

    ifp = fopen(tmpZoomFilename, "rb");

    currentPlot = destroyallplotrec();

    inputid = XtAppAddInput(appcontext, fileno(ifp), (XtPointer)XtInputReadMask, gnuplot_new, NULL);
  } else {
    xerrmsg(1, "Unable to create the mpl output file for zooming");
    unlink(tmpZoomFilename);
  }
}
/**************************** zoomcb **************************************/
void zoomcb(Widget w, XtPointer clientdata, XtPointer calldata) {
  long int n;

  n = (long int)clientdata;
  switch (n) {
  case 0:
    if (sddsplotCommandline2 && newZoom)
      restore_new();
    else
      restore();
    break;
  case 1:
  case 2: {
    double x0, y0, x1, y1, fac = .5 * (3 - 2 * n);

    x0 = x0old + fac * (1. - ZOOMFAC) * deltax;
    x1 = x1old - fac * (1. - ZOOMFAC) * deltax;
    y0 = y0old + fac * (1. - ZOOMFAC) * deltay;
    y1 = y1old - fac * (1. - ZOOMFAC) * deltay;
    zoom(x0, y0, x1, y1);
  } break;
  case 3:
    if (sddsplotCommandline2 && newZoom) {
      zoom(x0old, y0old + MOVEFAC * deltay, x1old, y1old + MOVEFAC * deltay);
    } else {
      offsety0 -= MOVEFAC * deltay * scaley;
      offsety = offsety0 - .5;
      y0old += MOVEFAC * deltay;
      y1old += MOVEFAC * deltay;
    }
    break;
  case 4:
    if (sddsplotCommandline2 && newZoom) {
      zoom(x0old, y0old - MOVEFAC * deltay, x1old, y1old - MOVEFAC * deltay);
    } else {
      offsety0 += MOVEFAC * deltay * scaley;
      offsety = offsety0 - .5;
      y0old -= MOVEFAC * deltay;
      y1old -= MOVEFAC * deltay;
    }
    break;
  case 5:
    if (sddsplotCommandline2 && newZoom) {
      zoom(x0old - MOVEFAC * deltax, y0old, x1old - MOVEFAC * deltax, y1old);
    } else {
      offsetx0 -= MOVEFAC * deltax * scalex;
      offsetx = offsetx0 - .5;
      x0old -= MOVEFAC * deltax;
      x1old -= MOVEFAC * deltax;
    }
    break;
  case 6:
    if (sddsplotCommandline2 && newZoom) {
      zoom(x0old + MOVEFAC * deltax, y0old, x1old + MOVEFAC * deltax, y1old);
    } else {
      offsetx0 += MOVEFAC * deltax * scalex;
      offsetx = offsetx0 - .5;
      x0old += MOVEFAC * deltax;
      x1old += MOVEFAC * deltax;
    }
    break;
  case 7:
    xinfomsg("Drag Btn1 to define zoom region.\nClick Btn2 to zoom.\n"
             "Click Btn3 to restore full size.\n"
             "Zoom rectangle is fgColor (when on the background).");
    return;
  }
  displayplot((Widget)0, (caddr_t)0, (caddr_t)0);
}

void writeCommandToEmacsServer() {
  char *cmd, *temp = NULL;
  int len0 = strlen(sddsplotCommandline2) + 1024;
  cmd = malloc(sizeof(char) * len0);
  temp = malloc(sizeof(char) * len0);
  sprintf(temp, "%s", sddsplotCommandline2);
  fprintf(stderr, "%s\n", sddsplotCommandline2);
  /* Most of the double quotes have already been converted into single quotes.
     But emacsclient command won't pass single quotes and causes problem when spaces exist,
     So I am stripping them out and the user will have to add the quotes back in by hand. */
  replace_chars(temp, (char *)"\'", (char *)" ");
  replace_chars(temp, (char *)"\"", (char *)" ");
  sprintf(cmd, "emacsclient -e '(progn (switch-to-buffer (car (buffer-list)))(insert \"%s\n\"))'", temp);
  system(cmd);
  free(cmd);
}

void writeImageToEmacsServer() {
  char cmd[BUFSIZ], dir[BUFSIZ];
  char savefilename[BUFSIZ];
  short year, month, day, shift;
  double hour;
  time_t secondsSinceEpoch;
  time(&secondsSinceEpoch);
  TimeEpochToBreakdown(&year, NULL, &month, &day, &hour, (double)secondsSinceEpoch);
  if (hour < 8) {
    shift = 1;
  } else if (hour < 16) {
    shift = 2;
  } else {
    shift = 3;
  }
  snprintf(dir, sizeof(dir), "%s/daily/%hd/%02hd/%02hd/%hd",
           getenv("HOME"), year, month, day, shift);
  makedir(dir);

  savetype = 4;
  snprintf(savefilename, sizeof(savefilename), "%.*s/plot%.2f.png",
           (int)(sizeof(savefilename) - 15), dir, hour);
  pngprint(1);

  /* Prepare to build the command with a bounded file name */
  const char *cmd_prefix = "emacsclient -e '(progn (switch-to-buffer (car (buffer-list)))"
                           "(insert \"";
  const char *cmd_suffix = "\n\")(turn-on-iimage-mode))'";
  size_t fixed_length = strlen(cmd_prefix) + strlen(cmd_suffix);
  int max_savefilename = (int)(sizeof(cmd) - fixed_length - 1);

  snprintf(cmd, sizeof(cmd),
           "emacsclient -e '(progn (switch-to-buffer (car (buffer-list)))"
           "(insert \"%.*s\n\")(turn-on-iimage-mode))'",
           max_savefilename, savefilename);
  system(cmd);
}
