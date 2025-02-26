/* GNUPLOT - term.h */
/*
 * Copyright (C) 1986, 1987, 1990   Thomas Williams, Colin Kelley
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted, 
 * provided that the above copyright notice appear in all copies and 
 * that both that copyright notice and this permission notice appear 
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the modified code.  Modifications are to be distributed 
 * as patches to released version.
 *  
 * This software  is provided "as is" without express or implied warranty.
 * 
 *
 * AUTHORS
 * 
 *   Original Software:
 *     Thomas Williams,  Colin Kelley.
 * 
 *   Gnuplot 2.0 additions:
 *       Russell Lang, Dave Kotz, John Campbell.
 * 
 * send your comments or suggestions to (pixar!info-gnuplot@sun.com).
 * 
 **************************************
 * term.h: terminal support definitions
 *   Edit this file depending on the set of terminals you wish to support.
 * Comment out the terminal types that you don't want or don't have, and
 * uncomment those that you want included. Be aware that some terminal 
 * types (eg, SUN, UNIXPLOT) will require changes in the makefile 
 * LIBS definition. 
 */

/* These terminals are not relevant for MSDOS */
#ifndef MSDOS

#define AED         /* AED 512 and AED 767 */
#define BITGRAPH    /* BBN BitGraph */
/* #define CGI          SCO CGI */
/* #define IRIS4D       IRIS4D series computer */
#define KERMIT      /* MS-Kermit Tektronix 4010 emulator */
/* #define FIG 	  	 Fig graphics language (requires object.h from TransFig) */
#define REGIS       /* ReGis graphics (vt125, vt220, vt240, Gigis...) */
#define SELANAR     /* Selanar */
/* #define SUN          Sun Microsystems Workstation */
#define T410X       /* Tektronix 4106, 4107, 4109 and 420x terminals */
#define TEK         /* Tektronix 4010, and probably others */
/* #define UNIXPLOT     unixplot */
#define VTTEK       /* VT-like tek40xx emulators */
/* #define X11          X11R4 window system */

#endif 

#ifdef PORT_TERMS
/* These terminals can be used on any system */
#define DXY800A		/* Roland DXY800A plotter */
#define EEPIC		/* EEPIC-extended LaTeX driver, for EEPIC users */
#define EMTEX		/* LATEX picture environment with EMTEX specials */
#define EPS60		/* Epson-style 60-dot per inch printers */
#define EPSON		/* Epson LX-800, Star NL-10, NX-1000 and lots of others */
#define HP2648		/* HP2648, HP2647 */
#define HP26		/* HP2623A and maybe others */
#define HP75		/* HP7580, and probably other HPs */
#define HPGL		/* HP7475, HP7220 plotters, and (hopefully) lots of others */
#define HPLJII		/* HP LaserJet II */
#define IMAGEN  	/* Imagen laser printers (300dpi) (requires -Iterm also) */
#define LATEX       /* LATEX picture environment */
#define NEC         /* NEC CP6 pinwriter printer */
#define POSTSCRIPT  /* Postscript */
#define MIF         /* MIF for FrameMaker */
#define PRESCRIBE   /* Kyocera Laser printer */
#define QMS         /* QMS/QUIC laserprinter (Talaris 1200 and others) */
#define STARC       /* Star Color Printer */
#define TANDY60		/* Tandy DMP-130 series 60-dot per inch graphics */
#define V384        /* Vectrix 384 and tandy color printer */
#endif

/* These are for MSDOS only */
#ifdef MSDOS
#ifdef __TURBOC__
#define ATT6300	    /* AT&T 6300 graphics */
#else
#define ATT6300	    /* AT&T 6300 graphics */
#define CORONA      /* Corona graphics 325 */
#define HERCULES    /* IBM PC/Clone with Hercules graphics board */
#endif
#endif

/* prototypes for code in file aed.trm */
int AED_init(void);
int AED_graphics(void);
int AED_text(void);
int AED_linetype(int linetype);
int AED_move(int x, int y);
int AED_vector(int x, int y);
int AED_put_text(int x, int y, char *str);
int AED_reset(void);
/* prototypes for code in file cgi.trm */
char	*cgidriver;
#if !defined(_WIN32)
char *getenv();
#endif
int CGI_init(void);
int HCGI_init(void);
int CGI_graphics(void);
int CGI_text(void);
int CGI_reset(void);
int CGI_move(int x, int y);
int CGI_vector(int x, int y);
int CGI_linetype(int linetype);
int CGI_put_text(int x, int y, char *str);
int CGI_text_angle(int angle);
int CGI_justify_text(enum JUSTIFY mode);
int CGI_point(int x, int y, int num);
/* prototypes for code in file dxy.trm */
int DXY_init(void);
int DXY_graphics(void);
int DXY_text(void);
int DXY_linetype(int linetype);
int DXY_move(int x, int y);
int DXY_vector(int x, int y);
int DXY_put_text(int x, int y, char *str);
int DXY_text_angle(int ang);
int DXY_reset(void);
/* prototypes for code in file eepic.trm */
int EEPIC_endline(void);	
int EEPIC_init(void);
int EEPIC_scale(double xs, double ys);
int EEPIC_graphics(void);
int EEPIC_text(void);
int EEPIC_linetype(int linetype);
int EEPIC_move(int x, int y);
int EEPIC_point(int x, int y, int number);
int EEPIC_vector(int ux, int uy);
int EEPIC_endline(void);
int EEPIC_arrow(int sx, int sy, int ex, int ey);
int EEPIC_put_text(int x, int y, char *str);
int EEPIC_justify_text(enum JUSTIFY mode);
int EEPIC_text_angle(int angle);
int EEPIC_reset(void);
/* prototypes for code in file epson.trm */
int EPSONinit(void);
int EPSONgraphics(void);
int EPSONtext(void);
int EPSONreset(void);
int epson_dump(void);
int NECinit(void);
int NECMgraphics(void);
int NECCgraphics(void);
int NECdraft_text(void);
int NECtext(void);
int NECClinetype(int linetype);
int NECreset(void);
int nec_dump(void);
int nec_draft_dump(void);
int STARCinit(void);
int STARCgraphics(void);
int STARCtext(void);
int STARClinetype(int linetype);
int STARCreset(void);
int STARC_dump(void);
int EPS60graphics(void);
int EPS60text(void);
int eps60_dump(void);
int TANDY60text(void);
/* prototypes for code in file fig.trm */
int FIG_init(void);
int FIG_graphics(void);
int FIG_text(void);
int FIG_linetype(int linetype);
int FIG_move(int x, int y);
int FIG_vector(int ux, int uy);
int FIG_arrow(int sx, int sy, int ex, int ey);
int FIG_put_text(int x, int y, char *str);
int FIG_justify_text(enum JUSTIFY mode);
int FIG_text_angle(int angle);
int FIG_reset(void);
/* prototypes for code in file hp26.trm */
int HP26_init(void);
int HP26_graphics(void);
int HP26_text(void);
int HP26_linetype(int linetype);
int HP26_move(int x, int y);
int HP26_vector(int x, int y);
int HP26_put_text(int x, int y, char *str);
int HP26_reset(void);
/* prototypes for code in file hp2648.trm */
int HP2648init(void);
int HP2648graphics(void);
int HP2648text(void);
int HP2648linetype(int linetype);
int HP2648move(int x, int y);
int HP2648vector(int x, int y);
int HP2648put_text(int x, int y, char *str);
int HP2648_text_angle(int ang);
int HP2648reset(void);
/* prototypes for code in file hpgl.trm */
int HPGL_init(void);
int HPGL_graphics(void);
int HPGL_text(void);
int HPGL_linetype(int linetype);
int HP75_linetype(int linetype);
int HPGL_move(int x, int y);
int HPGL_vector(int x, int y);
int HPGL_put_text(int x, int y, char *str);
int HPGL_text_angle(int ang);
int HPGL_reset(void);
/* prototypes for code in file hpljii.trm */
int HPLJIIinit(void);
int HPLJII_300PPIgraphics(void);
int HPLJII_150PPIgraphics(void);
int HPLJII_100PPIgraphics(void);
int HPLJII_75PPIgraphics(void);
int HPLJIIgraphics(void);
int HPLJIItext(void);
int HPLJIIlinetype(int linetype);
int HPLJIIput_text(int x, int y, char *str);
int HPLJIIputc(int x, int y, char c, int angle);
int HPLJIIreset(void);
/* prototypes for code in file iris4d.trm */
int IRIS4D_init(void);
int IRIS4D_graphics(void);
int IRIS4D_text(void);
int IRIS4D_linetype(int linetype);
int IRIS4D_move(int x, int y);
int IRIS4D_cmove(int x, int y);
int IRIS4D_vector(int x, int y);
int IRIS4D_put_text(int x, int y, char *str);
int IRIS4D_reset(void);
/* prototypes for code in file kyo.trm */
int PRE_init(void);
int KYO_init(void);
int PRE_graphics(void);
int PRE_text(void);
int PRE_linetype(int linetype);
int PRE_move(int x, int y);
int PRE_vector(int x, int y);
int PRE_put_text(int x, int y, char *str);
int PRE_justify_text(enum JUSTIFY mode);
int PRE_reset(void);
/* prototypes for code in file latex.trm */
int LATEX_init(void);
int LATEX_scale(double xs, double ys);
int LATEX_graphics(void);
int LATEX_text(void);
int LATEX_linetype(int linetype);
int LATEX_move(int x, int y);
int LATEX_point(int x, int y, int number);
int LATEX_vector(int ux, int uy);
int LATEX_solid_line(int x1, int x2, int y1, int y2);
int LATEX_rule(int code, float x, float y, float width, float height);
int LATEX_dot_line(int x1, int x2, int y1, int y2);
int LATEX_arrow(int sx, int sy, int ex, int ey);
int best_latex_arrow(int sx, int sy, int ex, int ey, int who);
int LATEX_put_text(int x, int y, char *str);
int LATEX_justify_text(enum JUSTIFY mode);
int LATEX_text_angle(int angle);
int LATEX_reset(void);
int EMTEX_init(void);
int EMTEX_reset(void);
int EMTEX_text(void);
int EMTEX_solid_line(int x1, int x2, int y1, int y2);
/* prototypes for code in file pc.trm */
int get_path(void);
int PC_text(void);
int PC_reset(void);
int PC_putc(int x, int y, char c, int angle, 
    FUNC_PTR line_func);
int PC_text_angle(int ang);
int CGA_init(void);
int CGA_graphics(void);
int CGA_linetype(int linetype);
int CGA_move(int x, int y);
int CGA_vector(int x, int y);
int CGA_put_text(int x, int y, char *str);
int EGA_init(void);
int EGA_graphics(void);
int EGA_linetype(int linetype);
int EGA_move(int x, int y);
int EGA_vector(int x, int y);
int EGA_put_text(int x, int y, char *str);
int VGA_init(void);
int VGA_graphics(void);
int VGA_linetype(int linetype);
int VGA_move(int x, int y);
int VGA_vector(int x, int y);
int VGA_put_text(int x, int y, char *str);
int EGALIB_init(void);
int EGALIB_graphics(void);
int EGALIB_text(void);
int EGALIB_linetype(int linetype);
int EGALIB_move(int x, int y);
int EGALIB_vector(int x, int y);
int EGALIB_put_text(int x, int y, char *str);
int HERC_init(void);
int HERC_graphics(void);
int HERC_text(void);
int HERC_linetype(int linetype);
int HERC_move(int x, int y);
int HERC_vector(int x, int y);
int HERC_put_text(int x, int y, char *str);
int ATT_graphics(void);
int ATT_vector(int x, int y);
int ATT_put_text(int x, int y, char *str);
int COR_init(void);
int COR_graphics(void);
int COR_text(void);
int COR_linetype(int linetype);
int COR_move(int x, int y);
int COR_vector(int x, int y);
int COR_put_text(int x, int y, char *str);
int PC_text_angle(int ang);
int PC_justify_text(enum JUSTIFY mode);
int VGA_init(void);
int VGA_graphics(void);
int VGA_linetype(int linetype);
int VGA_put_text(int x, int y, char *str);
int VGAMONO_linetype(int linetype);
int MCGA_init(void);
int MCGA_graphics(void);
int MCGA_put_text(int x, int y, char *str);
int MCGA_linetype(int linetype);
int CGA_init(void);
int CGA_graphics(void);
int CGA_linetype(int linetype);
int CGA_move(int x, int y);
int CGA_vector(int x, int y);
int CGA_put_text(int x, int y, char *str);
int HERC_init(void);
int HERC_graphics(void);
int HERC_text(void);
int HERC_linetype(int linetype);
int HERC_move(int x, int y);
int HERC_vector(int x, int y);
int HERC_put_text(int x, int y, char *str);
int ATT_init(void);
int ATT_graphics(void);
int ATT_text(void);
int ATT_linetype(int linetype);
int ATT_move(int x, int y);
int ATT_vector(int x, int y);
int ATT_put_text(int x, int y, char *str);
/* prototypes for code in file post.trm */
int EPSF1_init(void);
int EPSF2_init(void);
int EPSF_graphics(void);
int CEPSF_graphics(void);
int EPSF_text(void);
int EPSF_reset(void);
int PS1_init(void);
int CPS1_init(void);
int PS1_init_rot(void);
int CPS1_init_rot(void);
int PS2_init(void);
int PS_init(void);
int CPS_init(void);
int PS_init_rot(void);
int CPS_init_rot(void);
int PS_graphics(void);
int CPS_graphics(void);
int PS_text(void);
int PS_reset(void);
int PS_linetype(int linetype);
int CPS_linetype(int linetype);
int PS_line_thickness(int line_thickness);
int PS_move(int x, int y);
int PS_vector(int x, int y);
int PS_put_text(int x, int y, char *str);
int PS_text_angle(int ang);
int PS_justify_text(enum JUSTIFY mode);
int PS_point(int x, int y, int number);
int PS_dot(int x, int y, int number);
int PS_fill_box(int shade, int xl, int xh, int yl, int yh);
int CPS_fill_box(int shade, int xl, int xh, int yl, int yh);
int CPS_color(unsigned short red, unsigned short green, unsigned short blue);
int CPS_add_color(unsigned short red, unsigned short green, unsigned short blue, short reset);
void RGB_values(double *red, double *green, double *blue, double hue);
int CPS_symbol_color(int color);
int CPS_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);

/* prototypes for code in file mif.trm */
int MIF_init(void);
int CMIF_init(void);
int MIF_graphics(void);
int MIF_text(void);
int MIF_reset(void);
int MIF_linetype(int linetype);
int MIF_line_thickness(int line_thickness);
int MIF_move(int x, int y);
int MIF_vector(int x, int y);
int MIF_put_text(int x, int y, char *str);
int MIF_text_angle(int ang);
int MIF_justify_text(enum JUSTIFY mode);
int MIF_point(int x, int y, int number, double size);
int MIF_dot(int x, int y, int number);
int MIF_fill_box(int shade, int xl, int xh, int yl, int yh);
int MIF_startpolyline(void);
int MIF_endpolyline(void);
int MIF_endpage(void);
int MIF_setlinetype(void);
int MIF_setcolorcatalog(void);
/* prototypes for code in file qms.trm */
int QMS_init(void);
int QMS_graphics(void);
int QMS_text(void);
int QMS_linetype(int linetype);
int QMS_move(int x, int y);
int QMS_vector(int x2, int y2);
int QMS_put_text(int x, int y, char *str);
int QMS_reset(void);
/* prototypes for code in file regis.trm */
int REGISinit(void);
int REGISgraphics(void);
int REGIStext(void);
int REGISlinetype(int linetype);
int REGISmove(int x, int y);
int REGISvector(int x, int y);
int REGISput_text(int x, int y, char *str);
int REGIStext_angle(int ang);
int REGISreset(void);
int REGIS_fill_box(int shade, int xl, int xh, int yl, int yh);
/* prototypes for code in file sun.trm */
int SUN_init(void);
int SUN_graphics(void);
int SUN_text(void);
int SUN_linetype(int linetype);
int SUN_move(int x, int y);
int SUN_vector(int x, int y);
int SUN_put_text(int x, int y, char *str);
int SUN_justify_text(enum JUSTIFY mode);
int SUN_reset(void);
int sun_setmaskpixel(int x, int y, int value);
int sun_line(int x1, int x2, int y1, 
    int y2);
/* prototypes for code in file t410x.trm */
int T410X_init(void);
int T410X_reset(void);
int T410X_graphics(void);
int T410X_text(void);
int T410X_move(int x, int y);
int T410X_vector(int x, int y);
int T410X_point(int x, int y, int number);
int T410X_linetype(int linetype);
int T410X_put_text(int x, int y, char *str);
int T410X_text_angle(int ang);
int T410X_encode_x_y(int x, int y);
int T410X_encode_int(int number);
/* prototypes for code in file tek.trm */
int TEK40init(void);
int TEK40graphics(void);
int TEK40text(void);
int TEK40linetype(int linetype);
int TEK40move(int x, int y);
int TEK40vector(int x, int y);
int TEK40put_text(int x, int y, char *str);
int TEK40reset(void);
int BG_text(void);
int BG_put_text(int x, int y, char *str);
int KTEK40graphics(void);
int KTEK40Ctext(void);
int KTEK40Clinetype(int linetype);
int KTEK40Mlinetype(int linetype);
int KTEK40reset(void);
int SEL_init(void);
int SEL_graphics(void);
int SEL_text(void);
int SEL_reset(void);
int VTTEK40init(void);
int VTTEK40reset(void);
int VTTEK40linetype(int linetype);
int VTTEK40put_text(int x, int y, char *str);
/* prototypes for code in file term.c */
char *turboc_init(void);
int reopen_binary(void);
char *vms_init(void);
int vms_reset(void);
int term_mode_tek(void);
int term_mode_native(void);
int term_pasthru(void);
int term_nopasthru(void);
int fflush_binary(void);
int do_point(int x, int y, int number, double scale);
int do_point_fill(int x, int y, int number, double scale);
int do_dot(int x, int y, int number);
/* extern double ipow(void); */
int line_and_point(int x, int y, int number);
int do_arrow(int sx, int sy, int ex, int ey);
int NULL_init(void);
int NULL_reset(void);
int NULL_text(void);
int NULL_scale(double xs, double ys);
int NULL_graphics(void);
int NULL_move(int x, int y);
int NULL_vector(int x, int y);
int NULL_linetype(int linetype);
int NULL_put_text(int x, int y, char *str);
int NULL_text_angle(int ang);
int NULL_justify_text(enum JUSTIFY mode);
int NULL_dot(int x, int y, int number);
int NULL_arrow(int sx, int sy, int ex, int ey);
int NULL_fillbox(int shade, int xl,  int xh, int yl, int yh);
int NULL_line_thickness(int line_thickness);
int NULL_color(unsigned short red, unsigned short green, unsigned short blue);
int NULL_add_color(unsigned short red, unsigned short green, unsigned short blue, short reset);
int NULL_sendCoordinates(void);
int NULL_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);
int NULL_movie(void);
int change_term(char *name, int length);
int UP_redirect( int caller);
int UP_redirect (int caller);
int os_error(char *str, int t_num);
/* prototypes for code in file unixplot.trm */
int UP_init(void);
int UP_graphics(void);
int UP_text(void);
int UP_linetype(int linetype);
int UP_move(int x, int y);
int UP_vector(int x, int y);
int UP_put_text(int x, int y, char *str);
int UP_reset(void);
/* prototypes for code in file v384.trm */
int V384_init(void);
int V384_graphics(void);
int V384_text(void);
int V384_linetype(int linetype);
int V384_move(int x, int y);
int V384_vector(int x, int y);
int V384_put_text (int x, int y, char *str);
int V384_reset(void);
/* prototypes for code in file x11.trm */
FILE *X11_pipe = NULL;
FILE *QT_pipe = NULL;
//FILE *popen();
void parseCommandlineToMotif(int argc, char **argv);
void parseCommandlineToQT(int argc, char **argv);
void passCommandlineToPNG(int argc, char **argv);
void passCommandlineToPS(int argc, char **argv);
int contains_whitespace(char *s);

int X11_args(int argc, char **argv);
int X11_init(void);
int X11g_init(void);
int X11c_init(void);
int X11motif_init(void);
int X11bmotif_init(void);
int X11bmotifPCM_init(void);
int X11_reset(void);
int X11b_reset(void);
int X11_text(void);
int X11b_text(void);
int X11_graphics(void);
int X11b_graphics(void);
int X11_move(int x, int y);
int X11b_move(int x, int y);
int X11_vector(int x, int y);
int X11b_vector(int x, int y);
int X11_linetype(int lt);
int X11b_linetype(int lt);
int X11_line_thickness(int lt);
int X11b_line_thickness(int lt);
int X11_put_text(int x, int y, char *str);
int X11_justify_text(enum JUSTIFY mode);
int X11_fill_box(int shade, int xl, int xh, 
    int yl, int yh);
int X11b_fill_box(int shade, int xl, int xh, 
    int yl, int yh);
int X11_dot(int x, int y, int number);
int X11b_dot(int x, int y, int number);
int X11b_color(unsigned short red, unsigned short green, unsigned short blue);
int X11b_add_color(unsigned short red, unsigned short green, unsigned short blue, short reset);
int X11b_sendCoordinates(void);
int X11b_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);

void RGB_short_values(unsigned short *red, unsigned short *green, 
    unsigned short *blue, double hue);
void Spectral_RGB_short_values(unsigned short *red, unsigned short *green, 
    unsigned short *blue, double hue);
void Spectral_RGB_NoMagenta_short_values(unsigned short *red, unsigned short *green, 
    unsigned short *blue, double hue);

int PNG_init(void);
int PNG_graphics(void);
int PNG_text(void);
int PNG_reset(void);
int PNG_linetype(int linetype);
int PNG_move(int x, int y);
int PNG_dot(int x, int y, int number);
int PNG_vector(int x, int y);
int PNG_put_text(int x, int y, char *str);
int PNG_text_angle(int ang);
int PNG_fill_box(int shade, int xl, int xh, int yl, int yh);
int PNG_add_color(unsigned short red, unsigned short green, unsigned short blue, short reset);
int PNG_line_thickness(int lt);
int PNG_color(unsigned short red, unsigned short green, unsigned short blue);
int PNG_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);
int PNG_movie(void);
void PNG_passCommandLineToPNGFromMotif(char *command);

int QT_args(int argc, char **argv);
int QT_init(void);
int QT_reset(void);
int QT_text(void);
int QT_graphics(void);
int QT_move(int x, int y);
int QT_vector(int x, int y);
int QT_linetype(int lt);
int QT_line_thickness(int lt);
int QT_fill_box(int shade, int xl, int xh, int yl, int yh);
int QT_dot(int x, int y, int number);
int QT_color(unsigned short red, unsigned short green, unsigned short blue);
int QT_add_color(unsigned short red, unsigned short green, unsigned short blue, short reset);
int QT_sendCoordinates(void);
int QT_spectral(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);
