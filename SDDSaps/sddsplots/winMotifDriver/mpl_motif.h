/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <vfw.h>
#include "resource.h"
#include "mpl_motif.hlp"
#include "gd.h"

#define round(x) ((int)((x)+.5))
#define VTYPE short
#define MAX_LOADSTRING 100
#define DNC 65536
#define WIDTH 750
#define HEIGHT 580
#define X11_XMAX 4096
#define X11_YMAX 3165
#define XMAX X11_XMAX
#define YMAX X11_YMAX
#define X(x) (int)((x)*scalex-offsetx)
#define Y(y) (int)((YMAX-(y))*scaley-offsety)
#define X0(x) (((double)(x)+offsetx0)/scalex)
#define Y0(y) ((double)YMAX-((double)(y)+offsety0)/scaley)
#define MTRACKX(x0) ((x0)*userax+userbx)
#define MTRACKY(y0) ((y0)*useray+userby)
#define PNG_XMAX 656
#define PNG_YMAX 506
#define NSPECT 101
#define NCOLORS 18
#define COLORNAMESIZE 32
#define ZOOMFAC .75
#define MOVEFAC .5
#define Color (nplanes>1)
#define PNG_XMAX 656
#define PNG_YMAX 506
#define PS_XMAX 3600
#define PS_YMAX 2520
#define PS_XLAST (PS_XMAX - 1)
#define PS_YLAST (PS_YMAX - 1)
#define PS_VTIC (PS_YMAX/80)
#define PS_HTIC (PS_YMAX/80)
#define PS_SC (PS_XMAX/360)
#define	PS_LW (0.25*PS_SC)
#define PS_VCHAR1 (7*PS_SC)
#define PS_HCHAR1 (7*PS_SC*6/10)
#define CPS_ONBLACK_BG 0x0001
#define CPS_ONWHITE_BG 0x0002
#define SAVEFILENAME "mpl.png"

struct PLOTREC {
  int nplot;
  int nc;
  char *buffer;
  struct PLOTREC *next;
  struct PLOTREC *prev;
};

typedef struct {
  unsigned short red, green, blue;
} MotifColor;

HINSTANCE     hInst;
TCHAR         szTitle[MAX_LOADSTRING];
TCHAR         szWindowClass[MAX_LOADSTRING];
COLORREF      spectrum[NSPECT];
COLORREF      black,white;
PAGESETUPDLG  psDlg;
VTYPE         cx=0,cy=0;
HWND          display;
HWND          mouseTracker = NULL;
HWND          aboutBox = NULL;
HWND          contentsBox = NULL;
HWND          navigateBox = NULL;
HWND          colorBox = NULL;

int spectral=0;
int customspectral=0;
int nspect=101;
unsigned short red0=0, green0=0, blue0=0, red1=65535, green1=65535, blue1=65535;
int nplots=0;
int keep=0;
int W=WIDTH,H=HEIGHT;
int linecolormax=0;
int domovie=0;
int norestore=0;
int windowmessage=0,doublebuffer=1,killparent=0,spectrumallocated=0;
int grouplines=0;
int greyscale=0;
int dashes=0;
int currentuserstyle = 0;
int currentlinewidth = 1;
int linewidthscale = 1;
int saveborder=0;
int savecolor=1;
int savelandscape=1;
int savetype=1;
char savefilename[MAX_PATH + 2]=SAVEFILENAME;
short spectrumalloc[NSPECT];
double scalex,scaley,x0old,y0old,x1old,y1old,deltax,deltay;
double offsetx,offsety,offsetx0,offsety0;
double userax=0.,userbx=0.,useray=0.,userby=0.;

int pngBlack, pngWhite;
int pngLinetype[NCOLORS];
int png_linetype;
int png_color_index = 0;
int *png_colorList = NULL;
int png_xposition, png_yposition;
long png_xlast, png_ylast, png_xmax, png_ymax;
unsigned long pngFlags;
gdImagePtr pngImage;
MotifColor *png_colors = NULL;

int ps_path_count=0;
int pspage=0;
int force_linetype=0;
int last_PS_linetype = -1;
int cps_color_index = 0;
int last_CPS_linetype = -1;
int cps_color_maxindex = 0;
char **CPS_extra_setups = NULL;
MotifColor *cps_colors = NULL;

COLORREF colors[NCOLORS],colors_orig[NCOLORS],colorsalloc[NCOLORS];
COLORREF currentcolor;
COLORREF currentbrushcolor;

static POINTS ptsBegin;
static POINTS ptsEnd;

struct PLOTREC *last=(struct PLOTREC *)NULL,*cur=(struct PLOTREC *)NULL;
struct PLOTREC *curwrite=(struct PLOTREC *)NULL;

char color_keys[NCOLORS][7]=
{ "bg","fg","line0", 
  "line1","line2","line3","line4","line5","line6","line7","line8",
  "line9","line10","line11","line12","line13","line14","line15" 
};
char color_options[NCOLORS][13]=
{ "-bgColor","-fgColor","-line0Color", 
  "-line1Color","-line2Color","-line3Color","-line4Color",
  "-line5Color","-line6Color","-line7Color","-line8Color",
  "-line9Color","-line10Color","-line11Color","-line12Color",
  "-line13Color","-line14Color","-line15Color" 
};
char color_specifiers[NCOLORS][13]=
{ ".bgColor",".fgColor",".line0Color", 
  ".line1Color",".line2Color",".line3Color",".line4Color",
  ".line5Color",".line6Color",".line7Color",".line8Color",
  ".line9Color",".line10Color",".line11Color",".line12Color",
  ".line13Color",".line14Color",".line15Color" 
};
char color_values[NCOLORS][COLORNAMESIZE]=
{ "Black","White","White",
  "Red","Blue","Green","Yellow","Magenta","Cyan","LimeGreen","Gold",
  "Orange","HotPink","DeepSkyBlue","MediumSpringGreen","Tomato","Tan",
  "Grey75"
};

ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	ErrorDialog(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	Contents(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	NavigateNumber(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	MouseTracker(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	ColorDialog(HWND, UINT, WPARAM, LPARAM);
UINT    CALLBACK    PagePaintHook(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK SaveAsHookProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam);
void                InitDocStruct( DOCINFO* di, char* docname);
void                printcb(HWND hWnd);
struct PLOTREC      *makeplotrec(void);
void                status(HWND hWnd);
void                quit(void);
void                destroyplotrec(struct PLOTREC *rec);
void                displayplot(HDC hdc, RECT rt, int screen);
void                resize(HWND hWnd);
void                restore(void);
void                movie(HWND hWnd);
long                readdata(void);
int                 alloccolors(void);
int                 allocspectrum(void);
void                navigatecb(HWND hWnd, int clientdata, int page);
void                placecb(HWND hWnd, int clientdata);
void                zoom(double x0, double y0, double x1, double y1);
void                zoomcb(HWND hWnd, int clientdata);
void                pngprint(void);
void                psprint(void);
void                savecb(HWND hWnd);

int PNG_init(void);
int PNG_graphics(void);
int PNG_text(FILE *outfile);
int PNG_reset(void);
int PNG_linetype(int linetype);
int PNG_move(int x, int y);
int PNG_vector(int x, int y);
int PNG_dot(int x, int y, int number);
int PNG_fill_box(int shade, int xl, int xh, int yl, int yh);
int PNG_color(unsigned short red, unsigned short green, unsigned short blue);
int PNG_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);

int PS1_init(FILE *outfile);
int CPS1_init(FILE *outfile);
int PS1_init_rot(FILE *outfile);
int CPS1_init_rot(FILE *outfile);
int PS_move(FILE *outfile, int x, int y);
int PS_vector(FILE *outfile, int x, int y);
int PS_linetype(FILE *outfile, int linetype);
int PS_fill_box(FILE *outfile, int shade, int xl, int xh, int yl, int yh);
int PS_dot(FILE *outfile, int x, int y, int number);
int PS_line_thickness(FILE *outfile, int line_thickness);
int PS_graphics(FILE *outfile);
int PS_text(FILE *outfile);
int PS_reset(FILE *outfile);
int CPS_linetype(FILE *outfile, int linetype);
int CPS_fill_box(FILE *outfile, int shade, int xl, int xh, int yl, int yh);
int CPS_graphics(FILE *outfile);
int CPS_color(FILE *outfile, unsigned short red, unsigned short green, unsigned short blue);
int EPSF_graphics(FILE *outfile);
int EPSF_text(FILE *outfile);
int EPSF_reset(void);
int CEPSF_graphics(FILE *outfile);
int PS_ProcessDeviceArgs(void);
int reset_PS_lastlinetypes(void);
int doCPSExtraSetups(FILE *outfile);
int PS_init(FILE *outfile);
int CPS_init(FILE *outfile);
int PS_init_rot(FILE *outfile);
int CPS_init_rot(FILE *outfile);
int CPS_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);

void RGB_values(double *red, double *green, double *blue, double hue);



char *PS_header[] = {
"/vpt2 vpt 2 mul def\n", 
"/hpt2 hpt 2 mul def\n", 
"/M {moveto} def\n", 
"/L {lineto} def\n", 
/* flush left show */
#if defined(PS_EXTRAS)
    "/Lshow { currentpoint stroke moveto\n", 
    "  0 vshift rmoveto show } def\n",  
    /* flush right show */
    "/Rshow { currentpoint stroke moveto\n", 
    "  dup stringwidth pop neg vshift rmoveto show } def\n",  
    /* centred show */
    "/Cshow { currentpoint stroke moveto\n", 
    "  dup stringwidth pop -2 div vshift rmoveto show } def\n",  
    "/D {  LT0  2 copy  vpt add moveto\n",  /* Diamond */
    "  hpt neg vpt neg rlineto  hpt vpt neg rlineto\n", 
    "  hpt vpt rlineto  hpt neg vpt rlineto  closepath  stroke\n", 
    "  P  } def\n", 
    "/A {  LT0  vpt sub moveto  0 vpt2 rlineto\n",  /* Plus (Add) */
    "  currentpoint stroke moveto\n", 
    "  hpt neg vpt neg rmoveto  hpt2 0 rlineto stroke\n", 
    "  } def\n", 
    "/B {  LT0  2 copy  exch hpt sub exch vpt add moveto\n",  /* Box */
    "  0 vpt2 neg rlineto  hpt2 0 rlineto  0 vpt2 rlineto\n", 
    "  hpt2 neg 0 rlineto  closepath  stroke\n", 
    "  P  } def\n", 
    "/C {  LT0  exch hpt sub exch vpt add moveto\n",  /* Cross */
    "  hpt2 vpt2 neg rlineto  currentpoint  stroke  moveto\n", 
    "  hpt2 neg 0 rmoveto  hpt2 vpt2 rlineto stroke  } def\n", 
    "/T {  LT0  2 copy  vpt 1.12 mul add moveto\n",  /* Triangle */
    "  hpt neg vpt -1.62 mul rlineto\n", 
    "  hpt 2 mul 0 rlineto\n", 
    "  hpt neg vpt 1.62 mul rlineto  closepath  stroke\n", 
    "  P  } def\n", 
    "/S {  LT0  2 copy A C} def\n",  /* Star */
#endif  /* PS_EXTRAS */
NULL
};

char *PS_BW_def[] = {
"/BuildRectPath {\n",    /* fillrect emulation as per D.3.2 of Postscript Language Ref. Manual */
" dup type dup /integertype eq exch /realtype eq or {\n", 
"  4 -2 roll moveto\n", 
"  dup 0 exch rlineto exch 0 rlineto neg 0 exch rlineto closepath\n", 
" }{ \n", 
"  dup length 4 sub 0 exch 4 exch \n", 
"  { \n", 
"   1 index exch 4 getinterval aload pop\n", 
"   BuildRectPath\n", 
"  } for \n", 
"   pop\n", 
"  } ifelse\n", 
" } bind def\n", 
"/RF {\n", 
" setgray gsave newpath BuildRectPath fill grestore 0 setgray\n", 
" } bind def\n", 
/* bold lines */
"/BL { stroke gnulinewidth 2.25 mul setlinewidth } def\n", 
"/CL { stroke gnulinewidth 4 mul setlinewidth } def\n", 
/* light lines */
"/LL { stroke gnulinewidth 0.4 mul setlinewidth } def\n", 
"/VL { stroke gnulinewidth 0.15 mul setlinewidth } def\n", 
/* Plot Lines */
"/PL { stroke gnulinewidth setlinewidth } def\n",
/* Line Thickness */
"/LW1 { stroke gnulinewidth 2.5 setlinewidth } def\n",
"/LW2 { stroke gnulinewidth 5.0 setlinewidth } def\n",
"/LW3 { stroke gnulinewidth 7.5 setlinewidth } def\n",
"/LW4 { stroke gnulinewidth 10.0 setlinewidth } def\n",
"/LW5 { stroke gnulinewidth 12.5 setlinewidth } def\n",
"/LW6 { stroke gnulinewidth 15.0 setlinewidth } def\n",
"/LW7 { stroke gnulinewidth 17.5 setlinewidth } def\n",
"/LW8 { stroke gnulinewidth 20.0 setlinewidth } def\n",
"/LW9 { stroke gnulinewidth 22.5 setlinewidth } def\n",
/* Line Types */
"/LT0 { PL [] 0 setdash } def\n"
"/LT1 { PL [1 dl 6 dl] 0 setdash } def\n"
"/LT2 { PL [] 0 setdash } def\n"
"/LT3 { PL [2 dl 2 dl] 0 setdash } def\n"
"/LT4 { PL [6 dl 2 dl] 0 setdash } def\n"
"/LT5 { PL [6 dl 6 dl] 0 setdash } def\n"
"/LT6 { PL [2 dl 6 dl] 0 setdash } def\n"
"/LT7 { PL [2 dl 2 dl 2 dl 6 dl] 0 setdash } def\n"
"/LT8 { PL [6 dl 2 dl 6 dl 6 dl] 0 setdash } def\n"
"/LT9 { PL [2 dl 2 dl 6 dl 2 dl] 0 setdash } def\n"
"/LTa { PL [2 dl 6 dl 6 dl 6 dl] 0 setdash } def\n"
"/LTb { PL [2 dl 2 dl 6 dl 6 dl] 0 setdash } def\n"
"/LTc { PL [6 dl 2 dl 2 dl 6 dl] 0 setdash } def\n"
"/LTd { PL [1 dl 2 dl 1 dl 2 dl 1 dl 6 dl] 0 setdash } def\n"
"/LTe { PL [1 dl 2 dl 1 dl 2 dl 6 dl 2 dl] 0 setdash } def\n"
"/LTf { PL [1 dl 2 dl 1 dl 2 dl 6 dl 6 dl] 0 setdash } def\n"
"/LTg { PL [6 dl 2 dl 1 dl 2 dl 1 dl 6 dl] 0 setdash } def\n"
"/LTh { PL [6 dl 2 dl 1 dl 2 dl 6 dl 6 dl] 0 setdash } def\n"
"/P {  LT0 currentlinewidth 2 div sub moveto 0 currentlinewidth rlineto  stroke } def\n", 
NULL
};

char *PS_color_def[] = {
"/BuildRectPath {\n",    /* fillrect emulation as per D.3.2 of Postscript Language Ref. Manual */
" dup type dup /integertype eq exch /realtype eq or {\n", 
"  4 -2 roll moveto\n", 
"  dup 0 exch rlineto exch 0 rlineto neg 0 exch rlineto closepath\n", 
" }{ \n", 
"  dup length 4 sub 0 exch 4 exch \n", 
"  { \n", 
"   1 index exch 4 getinterval aload pop\n", 
"   BuildRectPath\n", 
"  } for \n", 
"   pop\n", 
"  } ifelse\n", 
" } bind def\n", 
"/RF {\n", 
" setrgbcolor gsave newpath BuildRectPath fill grestore 0 0 0 setrgbcolor\n", 
" } bind def\n", 
"/PL { stroke gnulinewidth setlinewidth } def\n", 
"/THL { stroke gnulinewidth 1.5 mul setlinewidth } def\n",
"/P { THL currentlinewidth 2 div sub moveto 0 currentlinewidth rlineto stroke } def\n",
/* Line Thickness */
"/LW1 { stroke gnulinewidth 2.5 setlinewidth } def\n",
"/LW2 { stroke gnulinewidth 5.0 setlinewidth } def\n",
"/LW3 { stroke gnulinewidth 7.5 setlinewidth } def\n",
"/LW4 { stroke gnulinewidth 10.0 setlinewidth } def\n",
"/LW5 { stroke gnulinewidth 12.5 setlinewidth } def\n",
"/LW6 { stroke gnulinewidth 15.0 setlinewidth } def\n",
"/LW7 { stroke gnulinewidth 17.5 setlinewidth } def\n",
"/LW8 { stroke gnulinewidth 20.0 setlinewidth } def\n",
"/LW9 { stroke gnulinewidth 22.5 setlinewidth } def\n", 
NULL
};

