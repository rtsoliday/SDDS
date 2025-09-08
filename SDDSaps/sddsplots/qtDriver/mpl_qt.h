#ifndef MPL_QT_H
#define MPL_QT_H

#include <QApplication>
#include <QScreen>
#include <QMainWindow>
#include <QWindow>
#include <QMenuBar>
#include <QMenu>
#include <QWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QObject>
#include <QVector>
#include <QSocketNotifier>
#include <QShortcut>
#include <QRubberBand>
#include <QMouseEvent>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QFileDialog>
#include <QPainter>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QTextEdit>
#include <QToolTip>
#include <QTime>
//#include <QTest>
#include <QDebug>    // for qDebug()
#include <cmath>
#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QAbstract3DGraph>
#include "mdb.h"

#define NSPECT 101
#define NCOLORS 18
#define XMAX 4096
#define YMAX 3165
#define WIDTH 690
#define HEIGHT 480
#define DNC 65536
#define VTYPE short

#define MTRACKX(x0) ((x0) * userax + userbx)
#define MTRACKY(y0) ((y0) * useray + userby)

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

typedef uint32_t COLOR_REF;

extern unsigned short red0, green0, blue0, red1, green1, blue1;
extern int nspect;
extern int spectral;
extern int customspectral;
extern int spectrumallocated;
extern int usecoordn;
extern int ncoords;
extern int nplots;
extern int keep;
extern int W, H;
extern int orig_width, orig_height;
extern int currentPlot;
extern bool domovie;
extern bool replotZoom;
extern bool tracking;
extern char *sddsplotCommandline2;
extern double userx0, userx1, usery0, usery1;
extern double userax, userbx, useray, userby;
extern double movieIntervalTime;

extern COLOR_REF spectrum[NSPECT];
extern COLOR_REF black;
extern COLOR_REF white;
extern COLOR_REF colors[NCOLORS];
extern COLOR_REF colors_orig[NCOLORS];
extern COLOR_REF colorsalloc[NCOLORS];
extern COLOR_REF currentcolor;
extern COLOR_REF foregroundColor;
extern struct PLOTREC *last;
extern struct PLOTREC *cur;
extern struct PLOTREC *curwrite;
extern struct COORDREC *curcoord;
extern struct COORDREC *lastcoord;
extern struct COORDREC *usecoord;
extern FILE *ifp;

extern QWidget *canvas;
extern QAction *replotZoomAction;
extern QtDataVisualization::QAbstract3DGraph *surfaceGraph;
extern QWidget *surfaceContainer;
extern QStackedWidget *plotStack;
extern int current3DPlot;
extern int total3DPlots;
extern QVector<QtDataVisualization::QAbstract3DGraph *> surfaceGraphs;
extern QVector<QWidget *> surfaceContainers;

#define RGB_QT(r, g, b) (                                              \
                      ((static_cast<uint32_t>(r) & 0xFF) << 16) |   \
                      ((static_cast<uint32_t>(g) & 0xFF) << 8) |    \
                      ((static_cast<uint32_t>(b) & 0xFF)))
int allocspectrum();
int alloccolors();
void setup_shortcuts(QMainWindow *mainWindow, bool for3D = false);
struct COORDREC *makecoordrec();
void destroycoordrecs();
void destroyplotrec(struct PLOTREC *rec);
int destroyallplotrec();
struct PLOTREC *makeplotrec();
void quit(void);
void onBlack();
void onWhite();
void newzoom();
long readdata();
void print();
void save();
long readdata();
void nav_next(QMainWindow *mainWindow);
void nav_previous(QMainWindow *mainWindow);
void delete_current(QMainWindow *mainWindow);
void to_number(QMainWindow *mainWindow);
void mouse_tracking(QMainWindow *mainWindow);

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
extern "C" {
  //extern FILE* outfile;
  extern LINE_TYPE_TABLE lineTypeTable;
  extern char *LineTableFile;
  long SDDS_ReadLineTypeTable(LINE_TYPE_TABLE *LTT, char *filename);
}


#endif
