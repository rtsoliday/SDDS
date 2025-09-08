/**
 * @file mpl_qt.cc
 * @brief MPL Outboard Driver for plotting using Qt.
 *
 * This file implements the main functions and classes required for the MPL outboard driver.
 */

#include "mpl_qt.h"

#include <unistd.h>
#ifndef _WIN32
#  include <fcntl.h>
#endif

#include <QLocalServer>
#include <QLocalSocket>
#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QSurface3DSeries>
#include <QtDataVisualization/QSurfaceDataArray>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/Q3DBars>
#include <QtDataVisualization/QBar3DSeries>
#include <QtDataVisualization/QBarDataArray>
#include <QtDataVisualization/QBarDataProxy>
#include <QtDataVisualization/QCategory3DAxis>
#include <QtDataVisualization/Q3DTheme>
#include <QtDataVisualization/Q3DCamera>
#include <QtDataVisualization/QValue3DAxis>
#include <QtDataVisualization/QLogValue3DAxisFormatter>
#include <QtDataVisualization/QValue3DAxisFormatter>
#include <QLinearGradient>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <QVector3D>
#include <QFont>
#include <QVector>
#include <QStringList>
#include <QShortcut>
#include <QLabel>
#include <QPalette>
#include <QSizePolicy>
#include <QDateTime>
#include <QList>
#include <cstring>
#include <cstdlib>
#include <float.h>
#include <cmath>
#ifdef _WIN32
#  include <windows.h>
#elif defined(__APPLE__)
#  include <ApplicationServices/ApplicationServices.h>
#  include <objc/objc.h>
#  include <objc/runtime.h>
#  include <objc/message.h>
#endif

using namespace QtDataVisualization;

double scalex, scaley;
#define Xpixel(value) (int)(((value) - userx0) * scalex)
#define Ypixel(value) (int)((usery1 - (value)) * scaley)
#define Xvalue(pixel) ((pixel) / scalex + userx0)
#define Yvalue(pixel) (usery1 - ((pixel) / scaley))

struct PLOTREC *last = (struct PLOTREC *)NULL;
struct PLOTREC *cur = (struct PLOTREC *)NULL;
struct PLOTREC *curwrite = (struct PLOTREC *)NULL;
struct COORDREC *curcoord = (struct COORDREC *)NULL;
struct COORDREC *lastcoord = (struct COORDREC *)NULL;
struct COORDREC *usecoord = (struct COORDREC *)NULL;
int ncoords = 0;
int usecoordn = 0;
int W = WIDTH, H = HEIGHT;
int nplots = 0;
int keep = 0;
double timeoutHours = 8.;
int timeoutSec = 8 * 60 * 60 * 1000;
COLOR_REF black, white, foregroundColor;
COLOR_REF colors[NCOLORS], colors_orig[NCOLORS], colorsalloc[NCOLORS];
COLOR_REF currentcolor;
COLOR_REF spectrum[NSPECT];
VTYPE cx = 0, cy = 0;
double userax = 0., userbx = XMAX, useray = 0., userby = YMAX;
double userx0 = 0., userx1 = XMAX, usery0 = 0., usery1 = YMAX;
int currentlinewidth = 1;
int linecolormax = 0;
int UseDashes = 0;
char *sddsplotCommandline2 = NULL;
QVector<QVector<qreal>> dashPatterns(10);
int spectral = 0;
int customspectral = 0;
int nspect = 101;
int spectrumallocated = 0;
unsigned short red0 = 0, green0 = 0, blue0 = 0, red1 = 65535, green1 = 65535, blue1 = 65535;
int currentPlot = 1;
FILE *ifp = NULL;
class StdinReader;
StdinReader *stdinReader = NULL;
bool replotZoom = true;
bool tracking = false;
bool domovie = false;
double movieIntervalTime = 0.1;
extern "C" {
  FILE* outfile;
}
QAction *replotZoomAction;
QWidget *canvas;
QMainWindow *mainWindowPointer;
QAbstract3DGraph *surfaceGraph = nullptr;
QWidget *surfaceContainer = nullptr;
QStackedWidget *plotStack = nullptr;
int current3DPlot = 0;
int total3DPlots = 0;
QVector<QAbstract3DGraph *> surfaceGraphs;
QVector<QWidget *> surfaceContainers;

struct Plot3DArgs {
  QString file;
  QString xlabel;
  QString ylabel;
  QString title;
  QString topline;
  int fontSize;
  bool equalAspect;
  double shadeMin;
  double shadeMax;
  bool shadeRangeSet;
  bool gray;
  double hue0;
  double hue1;
  bool yFlip;
  bool hideAxes;
  bool hideZAxis;
  bool datestamp;
  bool xLog;
  bool xTime;
  bool yTime;
  bool bar;
  Plot3DArgs()
      : fontSize(0), equalAspect(false), shadeMin(0.0), shadeMax(0.0),
        shadeRangeSet(false), gray(false), hue0(0.0), hue1(1.0), yFlip(false),
        hideAxes(false), hideZAxis(false), datestamp(false), xLog(false),
        xTime(false), yTime(false), bar(false) {}
};

class Time3DAxisFormatter : public QValue3DAxisFormatter {
public:
  QString stringForValue(qreal value, const QString &format) const override {
    Q_UNUSED(format);
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(value))
        .toString("yyyy-MM-dd HH:mm:ss");
  }
};

static QWidget *run3dBar(const char *filename, const char *xlabel,
                         const char *ylabel, const char *title,
                         const char *topline, bool datestamp, int fontSize,
                         bool equalAspect, double shadeMin, double shadeMax,
                         bool shadeRangeSet, bool gray, double hue0,
                         double hue1, bool yFlip, bool hideAxes, bool hideZAxis,
                         bool xLog, bool xTime, bool yTime) {
  Q3DBars *graph = new Q3DBars();
  surfaceGraph = graph;
  if (equalAspect) {
    graph->setHorizontalAspectRatio(1.0f);
    graph->setAspectRatio(1.0f);
  }
  Q3DTheme *theme = graph->activeTheme();
  QColor bgColor = Qt::black;
  QColor fgColor = Qt::white;
  theme->setBackgroundEnabled(true);
  theme->setBackgroundColor(bgColor);
  theme->setWindowColor(bgColor);
  theme->setLabelTextColor(fgColor);
  theme->setGridLineColor(fgColor);
  theme->setLabelBackgroundEnabled(true);
  theme->setLabelBackgroundColor(bgColor);
  theme->setLabelBorderEnabled(true);
  QWidget *container = QWidget::createWindowContainer(graph);
  surfaceContainer = container;
  surfaceGraphs.append(graph);
  surfaceContainers.append(container);
  container->setMinimumSize(QSize(640, 480));
  QWidget *widget = new QWidget;
  QVBoxLayout *vbox = new QVBoxLayout(widget);
  vbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(0);
  QPalette widgetPalette = widget->palette();
  widgetPalette.setColor(QPalette::Window, theme->backgroundColor());
  widgetPalette.setColor(QPalette::WindowText, fgColor);
  widget->setPalette(widgetPalette);
  widget->setAutoFillBackground(true);
  if (topline && topline[0]) {
    QLabel *toplineLabel = new QLabel(QString::fromUtf8(topline));
    toplineLabel->setAlignment(Qt::AlignCenter);
    toplineLabel->setFont(theme->font());
    toplineLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toplineLabel->setMaximumHeight(toplineLabel->sizeHint().height());
    QPalette palette = toplineLabel->palette();
    palette.setColor(QPalette::Window, theme->backgroundColor());
    palette.setColor(QPalette::WindowText, fgColor);
    toplineLabel->setPalette(palette);
    toplineLabel->setAutoFillBackground(true);
    toplineLabel->setContentsMargins(0, 0, 0, 0);
    toplineLabel->setMargin(0);
    vbox->addWidget(toplineLabel);
  }
  if (datestamp) {
    QString ds = QDateTime::currentDateTime().toString("ddd MMM d HH:mm:ss yyyy");
    QLabel *dateLabel = new QLabel(ds);
    dateLabel->setAlignment(Qt::AlignCenter);
    dateLabel->setFont(theme->font());
    dateLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dateLabel->setMaximumHeight(dateLabel->sizeHint().height());
    QPalette palette = dateLabel->palette();
    palette.setColor(QPalette::Window, theme->backgroundColor());
    palette.setColor(QPalette::WindowText, fgColor);
    dateLabel->setPalette(palette);
    dateLabel->setAutoFillBackground(true);
    vbox->addWidget(dateLabel);
  }
  if (title && title[0]) {
    QLabel *titleLabel = new QLabel(QString::fromUtf8(title));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFont(theme->font());
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    titleLabel->setMaximumHeight(titleLabel->sizeHint().height());
    QPalette palette = titleLabel->palette();
    palette.setColor(QPalette::Window, theme->backgroundColor());
    palette.setColor(QPalette::WindowText, fgColor);
    titleLabel->setPalette(palette);
    titleLabel->setAutoFillBackground(true);
    vbox->addWidget(titleLabel);
  }
  vbox->addWidget(container);
  widget->setWindowTitle("MPL Outboard Driver 3D");
  QFont font = theme->font();
  if (fontSize > 0)
    font.setPointSize(fontSize);
  else
    font.setPointSize(font.pointSize() + 24);
  theme->setFont(font);
  if (xlabel) {
    graph->rowAxis()->setTitle(QString::fromUtf8(xlabel));
    graph->rowAxis()->setTitleVisible(true);
  }
  if (ylabel) {
    graph->columnAxis()->setTitle(QString::fromUtf8(ylabel));
    graph->columnAxis()->setTitleVisible(true);
  }
  if (hideAxes) {
    theme->setGridEnabled(false);
    graph->rowAxis()->setTitleVisible(false);
    graph->rowAxis()->setLabels(QStringList());
    graph->columnAxis()->setTitleVisible(false);
    graph->columnAxis()->setLabels(QStringList());
    if (hideZAxis) {
      graph->valueAxis()->setTitleVisible(false);
      graph->valueAxis()->setLabelFormat("");
    }
  }
  QFile dataFile(filename);
  if (!dataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    fprintf(stderr, "Unable to open %s\n", filename);
    return NULL;
  }
  QTextStream in(&dataFile);
  int nx, ny;
  double xmin, xmax, ymin, ymax;
  in >> nx >> ny >> xmin >> xmax >> ymin >> ymax;
  double dx = nx > 1 ? (xmax - xmin) / (nx - 1) : 1;
  double dy = ny > 1 ? (ymax - ymin) / (ny - 1) : 1;
  QStringList rowLabels, colLabels;
  for (int i = 0; i < nx; i++) {
    double xval = xLog ? pow(10.0, xmin + i * dx) : (xmin + i * dx);
    if (xTime)
      rowLabels << QDateTime::fromSecsSinceEpoch((qint64)xval).toString("yyyy-MM-dd HH:mm:ss");
    else
      rowLabels << QString::number(xval);
  }
  for (int j = 0; j < ny; j++) {
    double yval = ymin + j * dy;
    if (yTime)
      colLabels << QDateTime::fromSecsSinceEpoch((qint64)yval).toString("yyyy-MM-dd HH:mm:ss");
    else
      colLabels << QString::number(yval);
  }
  graph->rowAxis()->setLabels(rowLabels);
  graph->columnAxis()->setLabels(colLabels);
  if (yFlip)
    ;
  QBarDataArray *dataArray = new QBarDataArray;
  dataArray->reserve(nx);
  double zmin = DBL_MAX, zmax = -DBL_MAX;
  for (int i = 0; i < nx; i++) {
    QBarDataRow *row = new QBarDataRow(ny);
    for (int j = 0; j < ny; j++) {
      double z;
      in >> z;
      (*row)[j].setValue(z);
      if (z < zmin)
        zmin = z;
      if (z > zmax)
        zmax = z;
    }
    dataArray->append(row);
  }
  QBarDataProxy *proxy = new QBarDataProxy;
  proxy->resetArray(dataArray);
  if (shadeRangeSet) {
    zmin = shadeMin;
    zmax = shadeMax;
  }
  QBar3DSeries *series = new QBar3DSeries(proxy);
  if (!gray) {
    if (!spectrumallocated)
      allocspectrum();
  }
  QLinearGradient gradient;
  for (int i = 0; i < nspect; i++) {
    double frac = (double)i / (nspect - 1);
    if (gray) {
      gradient.setColorAt(frac, QColor::fromRgbF(frac, frac, frac));
    } else {
      int index = (int)((hue0 + frac * (hue1 - hue0)) * (nspect - 1) + 0.5);
      if (index < 0)
        index = 0;
      if (index >= nspect)
        index = nspect - 1;
      gradient.setColorAt(frac, QColor::fromRgb(spectrum[index]));
    }
  }
  series->setBaseGradient(gradient);
  series->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
  graph->valueAxis()->setRange(zmin, zmax);
  graph->addSeries(series);
  return widget;
}

static QWidget *run3d(const char *filename, const char *xlabel,
                      const char *ylabel, const char *title,
                      const char *topline, bool barPlot, bool datestamp, int fontSize,
                      bool equalAspect, double shadeMin, double shadeMax,
                      bool shadeRangeSet, bool gray, double hue0,
                      double hue1, bool yFlip, bool hideAxes, bool hideZAxis,
                      bool xLog, bool xTime, bool yTime) {
  if (barPlot)
    return run3dBar(filename, xlabel, ylabel, title, topline, datestamp, fontSize,
                    equalAspect, shadeMin, shadeMax, shadeRangeSet, gray, hue0,
                    hue1, yFlip, hideAxes, hideZAxis, xLog, xTime, yTime);
  Q3DSurface *graph = new Q3DSurface();
  surfaceGraph = graph;
  if (equalAspect) {
    graph->setHorizontalAspectRatio(1.0f);
    graph->setAspectRatio(1.0f);
  }
  Q3DTheme *theme = graph->activeTheme();
  QColor bgColor = Qt::black;
  QColor fgColor = Qt::white;
  theme->setBackgroundEnabled(true);
  theme->setBackgroundColor(bgColor);
  theme->setWindowColor(bgColor);
  theme->setLabelTextColor(fgColor);
  theme->setGridLineColor(fgColor);
  theme->setLabelBackgroundEnabled(true);
  theme->setLabelBackgroundColor(bgColor);
  theme->setLabelBorderEnabled(true);
  Q3DCamera *camera = graph->scene()->activeCamera();
  camera->setCameraPreset(Q3DCamera::CameraPresetIsometricRightHigh); // Default orientation
  float defaultX = camera->xRotation();
  float defaultY = camera->yRotation();
  float defaultZoom = camera->zoomLevel();
  QVector3D defaultTarget = camera->target();
  QWidget *container = QWidget::createWindowContainer(graph);
  surfaceContainer = container;
  surfaceGraphs.append(graph);
  surfaceContainers.append(container);
  container->setMinimumSize(QSize(640, 480));
  QWidget *widget = new QWidget;
  QVBoxLayout *vbox = new QVBoxLayout(widget);
  vbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(0);
  QPalette widgetPalette = widget->palette();
  widgetPalette.setColor(QPalette::Window, theme->backgroundColor());
  widgetPalette.setColor(QPalette::WindowText, fgColor);
  widget->setPalette(widgetPalette);
  widget->setAutoFillBackground(true);
  if (topline && topline[0]) {
    QLabel *toplineLabel = new QLabel(QString::fromUtf8(topline));
    toplineLabel->setAlignment(Qt::AlignCenter);
    toplineLabel->setFont(theme->font());
    toplineLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toplineLabel->setMaximumHeight(toplineLabel->sizeHint().height());
    QPalette palette = toplineLabel->palette();
    palette.setColor(QPalette::Window, theme->backgroundColor());
    palette.setColor(QPalette::WindowText, fgColor);
    toplineLabel->setPalette(palette);
    toplineLabel->setAutoFillBackground(true);
    toplineLabel->setContentsMargins(0, 0, 0, 0);
    toplineLabel->setMargin(0);
    vbox->addWidget(toplineLabel);
  }
  if (datestamp) {
    QString ds = QDateTime::currentDateTime().
                    toString("ddd MMM d HH:mm:ss yyyy");
    QLabel *dateLabel = new QLabel(ds);
    dateLabel->setAlignment(Qt::AlignCenter);
    dateLabel->setFont(theme->font());
    dateLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dateLabel->setMaximumHeight(dateLabel->sizeHint().height());
    QPalette palette = dateLabel->palette();
    palette.setColor(QPalette::Window, theme->backgroundColor());
    palette.setColor(QPalette::WindowText, fgColor);
    dateLabel->setPalette(palette);
    dateLabel->setAutoFillBackground(true);
    dateLabel->setContentsMargins(0, 0, 0, 0);
    dateLabel->setMargin(0);
    vbox->addWidget(dateLabel);
  }
  container->setContentsMargins(0, 0, 0, 0);
  vbox->addWidget(container);
  if (title && title[0]) {
    QLabel *titleLabel = new QLabel(QString::fromUtf8(title));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFont(theme->font());
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    titleLabel->setMaximumHeight(titleLabel->sizeHint().height());
    QPalette palette = titleLabel->palette();
    palette.setColor(QPalette::Window, theme->backgroundColor());
    palette.setColor(QPalette::WindowText, fgColor);
    titleLabel->setPalette(palette);
    titleLabel->setAutoFillBackground(true);
    titleLabel->setContentsMargins(0, 0, 0, 0);
    titleLabel->setMargin(0);
    vbox->addWidget(titleLabel);
  }
  widget->setWindowTitle("MPL Outboard Driver 3D");

  QFont font = theme->font();
  if (fontSize > 0)
    font.setPointSize(fontSize);
  else
    font.setPointSize(font.pointSize() + 24);
  theme->setFont(font);
  if (xlabel) {
    graph->axisX()->setTitle(QString::fromUtf8(xlabel));
    graph->axisX()->setTitleVisible(true);
  }
  if (ylabel) {
    graph->axisZ()->setTitle(QString::fromUtf8(ylabel));
    graph->axisZ()->setTitleVisible(true);
  }
  if (hideAxes) {
    theme->setGridEnabled(false);
    graph->axisX()->setTitleVisible(false);
    graph->axisX()->setLabelFormat("");
    graph->axisZ()->setTitleVisible(false);
    graph->axisZ()->setLabelFormat("");
    if (hideZAxis) {
      graph->axisY()->setTitleVisible(false);
      graph->axisY()->setLabelFormat("");
    }
  }

  QFile dataFile(filename);
  if (!dataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    fprintf(stderr, "Unable to open %s\n", filename);
    return NULL;
  }
  QTextStream in(&dataFile);
  int nx, ny;
  double xmin, xmax, ymin, ymax;
  in >> nx >> ny >> xmin >> xmax >> ymin >> ymax;
  graph->axisZ()->setRange(ymin, ymax);
  if (yTime) {
    Time3DAxisFormatter *formatter = new Time3DAxisFormatter;
    graph->axisZ()->setFormatter(formatter);
  }
  if (yFlip)
    graph->axisZ()->setReversed(true);
  double dx = nx > 1 ? (xmax - xmin) / (nx - 1) : 1;
  double dy = ny > 1 ? (ymax - ymin) / (ny - 1) : 1;
  if (xLog) {
    double xminLinear = pow(10.0, xmin);
    double xmaxLinear = pow(10.0, xmax);
    QLogValue3DAxisFormatter *formatter = new QLogValue3DAxisFormatter;
    graph->axisX()->setFormatter(formatter);
    graph->axisX()->setRange(xminLinear, xmaxLinear);
  } else {
    graph->axisX()->setRange(xmin, xmax);
  }
  if (xTime) {
    Time3DAxisFormatter *formatter = new Time3DAxisFormatter;
    graph->axisX()->setFormatter(formatter);
  }
  QSurfaceDataArray *dataArray = new QSurfaceDataArray;
  dataArray->reserve(ny);
  double zmin = DBL_MAX, zmax = -DBL_MAX;
  for (int j = 0; j < ny; j++) {
    QSurfaceDataRow *row = new QSurfaceDataRow(nx);
    for (int i = 0; i < nx; i++) {
      double z;
      in >> z;
      if (z < zmin)
        zmin = z;
      if (z > zmax)
        zmax = z;
      double xval = xLog ? pow(10.0, xmin + i * dx) : (xmin + i * dx);
      (*row)[i].setPosition(QVector3D(xval, z, ymin + j * dy));
    }
    dataArray->append(row);
  }
  QSurfaceDataProxy *proxy = new QSurfaceDataProxy;
  proxy->resetArray(dataArray);
  if (shadeRangeSet) {
    zmin = shadeMin;
    zmax = shadeMax;
  }
  QSurface3DSeries *series = new QSurface3DSeries(proxy);
  if (!gray) {
    if (!spectrumallocated)
      allocspectrum();
  }
  QLinearGradient gradient;
  for (int i = 0; i < nspect; i++) {
    double frac = (double)i / (nspect - 1);
    if (gray) {
      gradient.setColorAt(frac,
                          QColor::fromRgbF(frac, frac, frac));
    } else {
      int index = (int)((hue0 + frac * (hue1 - hue0)) * (nspect - 1) + 0.5);
      if (index < 0)
        index = 0;
      if (index >= nspect)
        index = nspect - 1;
      gradient.setColorAt(frac, QColor::fromRgb(spectrum[index]));
    }
  }
  series->setBaseGradient(gradient);
  series->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
  series->setDrawMode(QSurface3DSeries::DrawSurface);
  series->setMeshSmooth(true);
  series->setItemLabelFormat(QStringLiteral("(@xLabel, @zLabel, @yLabel)"));
  graph->axisY()->setRange(zmin, zmax);
  QShortcut *toggleLines = new QShortcut(QKeySequence(QStringLiteral("g")), widget);
  QObject::connect(toggleLines, &QShortcut::activated,
                   [series]() {
                     static bool wireframe = false;
                     wireframe = !wireframe;
                     series->setDrawMode(wireframe
                                              ? QSurface3DSeries::DrawSurfaceAndWireframe
                                              : QSurface3DSeries::DrawSurface);
                   });
  QShortcut *resetView = new QShortcut(QKeySequence(QStringLiteral("r")), widget);
  QObject::connect(resetView, &QShortcut::activated,
                   [camera, defaultX, defaultY, defaultZoom, defaultTarget]() {
                     camera->setXRotation(defaultX);
                     camera->setYRotation(defaultY);
                     camera->setZoomLevel(defaultZoom);
                     camera->setTarget(defaultTarget);
                   });
  QShortcut *snapX = new QShortcut(QKeySequence(QStringLiteral("x")), widget);
  QObject::connect(snapX, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(Q3DCamera::CameraPresetRight); });
  QShortcut *snapY = new QShortcut(QKeySequence(QStringLiteral("y")), widget);
  QObject::connect(snapY, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(Q3DCamera::CameraPresetFront); });
  QShortcut *snapZ = new QShortcut(QKeySequence(QStringLiteral("z")), widget);
  QObject::connect(snapZ, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(Q3DCamera::CameraPresetDirectlyAbove); });
  QShortcut *increaseFont =
      new QShortcut(QKeySequence(Qt::Key_Up), widget);
  QObject::connect(increaseFont, &QShortcut::activated,
                   [theme, graph]() {
                     QFont f = theme->font();
                     f.setPointSize(f.pointSize() + 1);
                     theme->setFont(f);
                     graph->axisX()->setTitle(graph->axisX()->title());
                     graph->axisY()->setTitle(graph->axisY()->title());
                     graph->axisZ()->setTitle(graph->axisZ()->title());
                   });
  QShortcut *decreaseFont =
      new QShortcut(QKeySequence(Qt::Key_Down), widget);
  QObject::connect(decreaseFont, &QShortcut::activated,
                   [theme, graph]() {
                     QFont f = theme->font();
                     if (f.pointSize() > 1)
                       f.setPointSize(f.pointSize() - 1);
                     theme->setFont(f);
                     graph->axisX()->setTitle(graph->axisX()->title());
                     graph->axisY()->setTitle(graph->axisY()->title());
                     graph->axisZ()->setTitle(graph->axisZ()->title());
                   });
  graph->addSeries(series);
  return widget;
}

/**
 * @brief Ensure the window is visible and active.
 *
 * Restores and raises the given window across platforms so that it
 * becomes visible even if minimized or on a different workspace.
 *
 * @param window Pointer to the window to make visible.
 */
static void makeWindowVisible(QMainWindow *window) {
#ifdef _WIN32
  HWND hwnd = (HWND)window->winId();
  if (IsIconic(hwnd))
    ShowWindow(hwnd, SW_RESTORE);
  else
    ShowWindow(hwnd, SW_SHOW);
  SetForegroundWindow(hwnd);
#elif defined(__APPLE__)
  ProcessSerialNumber psn = {0, kCurrentProcess};
  TransformProcessType(&psn, kProcessTransformToForegroundApplication);

  /*
   * Use modern Cocoa APIs to bring the application to the foreground.
   * This avoids the deprecated SetFrontProcess call that was previously used.
   */
  Class nsAppClass = objc_getClass("NSApplication");
  id sharedApp = ((id (*)(Class, SEL))objc_msgSend)(nsAppClass, sel_registerName("sharedApplication"));
  ((void (*)(id, SEL, BOOL))objc_msgSend)(sharedApp, sel_registerName("activateIgnoringOtherApps:"), YES);

  window->showNormal();
  window->raise();
  window->activateWindow();
#else
  window->showNormal();
  window->raise();
  window->activateWindow();
#endif
}

/**
 * @brief Reads from standard input and triggers plot updates.
 *
 * This class uses a QSocketNotifier to monitor stdin and calls the global
 * readdata() function when data is available.
 */
class StdinReader : public QObject {
  Q_OBJECT
public:
  StdinReader(int fd, QObject *parent = nullptr) : QObject(parent) {
    notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &StdinReader::handleActivated);
  }
  void setEnabled(bool enable) { notifier->setEnabled(enable); }
public slots:
  void handleActivated(int) {
    if (domovie)
      notifier->setEnabled(false);
    if (readdata() == 1) {
      notifier->setEnabled(false);
      if (ifp && ifp != stdin) {
        fclose(ifp);
        ifp = NULL;
      }
      QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
      mainWindowPointer->setWindowTitle(wtitle);
    } else if (domovie) {
      QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
      QTime dieTime = QTime::currentTime().addSecs(movieIntervalTime);
      mainWindowPointer->setWindowTitle(wtitle);
      while (QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
      notifier->setEnabled(true);
    } else if (keep > 0) {
      QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
      mainWindowPointer->setWindowTitle(wtitle);
    }
  }
private:
  QSocketNotifier *notifier;
};

static void startReader(int fd) {
  if (stdinReader) {
    delete stdinReader;
    stdinReader = NULL;
  }
  if (ifp && ifp != stdin) {
    fclose(ifp);
    ifp = NULL;
  }
  if (fd != 0) {
#ifndef _WIN32
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1)
      fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
  }
  if (fd == 0)
    ifp = stdin;
  else
    ifp = fdopen(fd, "rb");
  stdinReader = new StdinReader(fd);
}

/**
 * @brief Canvas widget for drawing plots.
 *
 * The Canvas class extends QFrame and handles mouse events, painting,
 * and resizing for the plotting area.
 */
class Canvas : public QFrame {
  Q_OBJECT
public:
  Canvas(QWidget *parent = nullptr) : QFrame(parent), rubberBand(nullptr) {
    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    connect(m_resizeTimer, &QTimer::timeout, this, &Canvas::resizeFinished);
    setMouseTracking(true);
  }
protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      origin = event->pos();
      if (!rubberBand) {
        rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
      }
      rubberBand->setGeometry(QRect(origin, QSize()));
      rubberBand->show();
    } else if (event->button() == Qt::MiddleButton) {
      // Hide the rubber band only when the middle mouse button is pressed.
      if (rubberBand && rubberBand->isVisible()) {
        rubberBand->hide();
        QRect selectedRect = rubberBand->geometry();
        double x0 = Xvalue(selectedRect.x());
        double x1 = Xvalue(selectedRect.x() + selectedRect.width());
        double y1 = Yvalue(selectedRect.y());
        double y0 = Yvalue(selectedRect.y() + selectedRect.height());
        if (x1 < x0) std::swap(x0, x1);
        if (y1 < y0) std::swap(y0, y1);
        userx0 = x0;
        userx1 = x1;
        usery0 = y0;
        usery1 = y1;
        if (replotZoom) {
          newzoom();
          userx0 = 0.;
          userx1 = XMAX;
          usery0 = 0.;
          usery1 = YMAX;
        }
        update();
      }
    } else if (event->button() == Qt::RightButton) {
      // Hide the rubber band only when the middle mouse button is pressed.
      userx0 = 0.;
      userx1 = XMAX;
      usery0 = 0.;
      usery1 = YMAX;
      if (replotZoom) {
        userx1 = usery1 = 0;
        newzoom();
      }
      update();
    }
  }
  void mouseMoveEvent(QMouseEvent *event) override {
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPoint pos = event->position().toPoint();
        QPoint globalPos = event->globalPosition().toPoint();
    #else
        QPoint pos = event->pos();
        QPoint globalPos = event->globalPos();
    #endif
    
        if ((event->buttons() & Qt::LeftButton) && rubberBand) {
            rubberBand->setGeometry(QRect(origin, pos).normalized());
        }
        if (tracking) {
            QString tipText = QString("x: %1, y: %2")
                .arg(MTRACKX(Xvalue(pos.x())), 0, 'g', 10)
                .arg(MTRACKY(Yvalue(pos.y())), 0, 'g', 10);
            QToolTip::showText(globalPos, tipText, this);
            QFrame::mouseMoveEvent(event);
        }
    }
  void mouseReleaseEvent(QMouseEvent *event) override {
  }
  void resizeEvent(QResizeEvent *event) override {
    QFrame::resizeEvent(event);
    m_resizing = true;         // Set flag on resize start
    m_resizeTimer->start(250); // Restart timer each time a resize event occurs
  }
  void paintEvent(QPaintEvent *event) override {
    VTYPE x, y, lt, lt2;
    char *bufptr, command;
    unsigned short r, g, b;

    if (m_resizing) {
      // Skip painting during an active resize
      return;
    }

    // Create or update the off-screen buffer
    if (m_buffer.size() != size()) {
      m_buffer = QPixmap(size());
    }
    // Clear the off-screen buffer
    m_buffer.fill(QColor::fromRgb(colors[0]));

    if (!cur) {
      QPainter painter(this);
      painter.drawPixmap(0, 0, m_buffer);
      return;
    }
    QPainter bufferPainter(&m_buffer);
    bufferPainter.setPen(QColor::fromRgb(white));

    if (fabs(userx1 - userx0) < 1e-12) userx1 = userx0 + 1;
    if (fabs(usery1 - usery0) < 1e-12) usery1 = usery0 + 1;
    W = width();
    H = height();
    scalex = (W - 1.0) / (userx1 - userx0);
    scaley = (H - 1.0) / (usery1 - usery0);

    bufptr = cur->buffer;
    destroycoordrecs();
    for (int n = 0; n < cur->nc;) {
      switch (command = *bufptr++) {
      case 'V': // draw vector
        memcpy((char *)&x, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&y, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 2 * sizeof(VTYPE);
        bufferPainter.drawLine(Xpixel(cx), Ypixel(cy), Xpixel(x), Ypixel(y));
        cx = x;
        cy = y;
        break;
      case 'M': // move
        memcpy((char *)&cx, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&cy, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 2 * sizeof(VTYPE);
        break;
      case 'P': // dot
        memcpy((char *)&cx, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&cy, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 2 * sizeof(VTYPE);
        bufferPainter.drawLine(Xpixel(cx), Ypixel(cy), Xpixel(cx), Ypixel(cy));
        cx++;
        cy++;
        break;
      case 'L': // set line type
        {
          QPen pen = bufferPainter.pen();
          pen.setStyle(Qt::SolidLine);
          memcpy((char *)&lt2, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          n += sizeof(char) + sizeof(VTYPE);
          if (!lineTypeTable.nEntries) {
            lt = lt2 % 16;
            lt += 2; /* Convert to color index */
            if (lt >= NCOLORS)
              lt = NCOLORS - 1;
            if (lt > linecolormax)
              linecolormax = lt;
            currentlinewidth = 1;
            currentcolor = colors[lt];
            if (UseDashes) {
              lt = lt2 % 10;
              pen.setDashPattern(dashPatterns[lt]);
            }
          } else {
            lt = lt2 % lineTypeTable.nEntries;
            if (lineTypeTable.typeFlag & LINE_TABLE_DEFINE_THICKNESS)
              currentlinewidth = lineTypeTable.thickness[lt];
            else
              currentlinewidth = 1;
            if (lineTypeTable.typeFlag & LINE_TABLE_DEFINE_DASH) {
              if (lineTypeTable.dash[lt].dashArray[0]) {
                QVector<qreal> dpattern;
                for (int j = 0; j < 5; j++) {
                  if (lineTypeTable.dash[lt].dashArray[j])
                    dpattern.append(static_cast<qreal>(lineTypeTable.dash[lt].dashArray[j]));
                }
                pen.setDashPattern(dpattern);
              }
            }
            if (lineTypeTable.typeFlag && LINE_TABLE_DEFINE_COLOR) {
              currentcolor = RGB_QT(lineTypeTable.red[lt], lineTypeTable.green[lt], lineTypeTable.blue[lt]);
            }
          }

          pen.setWidth(currentlinewidth);
          pen.setColor(currentcolor);
          bufferPainter.setPen(pen);
        } break;
      case 'W': // set line width
        {
          memcpy((char *)&lt, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          n += sizeof(char) + sizeof(VTYPE);
          currentlinewidth = lt;
          QPen pen = bufferPainter.pen();
          pen.setWidth(currentlinewidth);
          bufferPainter.setPen(pen);
        } break;
      case 'B': // Fill Box
        {
          VTYPE shade, xl, xh, yl, yh;
          int x, y, width, height;
          memcpy((char *)&shade, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          memcpy((char *)&xl, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          memcpy((char *)&xh, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          memcpy((char *)&yh, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          memcpy((char *)&yl, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          n += sizeof(char) + 5 * sizeof(VTYPE);
          if (!spectrumallocated)
            allocspectrum();
          shade = shade % nspect;
          x = Xpixel(xl);
          y = Ypixel(yl);
          width = Xpixel(xh) - x;
          height = Ypixel(yh) - y;
          bufferPainter.fillRect(QRect(x, y, width, height), spectrum[shade]);
        } break;
      case 'U': // user coordinate scaling
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
      case 'C': // set line color
        {
          memcpy((char *)&r, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          memcpy((char *)&g, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          memcpy((char *)&b, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          n += sizeof(char) + 3 * sizeof(VTYPE);
          QPen pen = bufferPainter.pen();
          currentcolor = RGB_QT(std::round((255.0 / 65536.0) * r), std::round((255.0 / 65536.0) * g), std::round((255.0 / 65536.0) * b));
          pen.setColor(currentcolor);
          bufferPainter.setPen(pen);
        } break;
      case 'S': // Allocate spectral spectrum
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
        // DialogBoxParam(hInst, (LPCTSTR)IDD_ERRORBOX, display, (DLGPROC)ErrorDialog, IDS_INVALIDDRAWCOMMAND);
        break;
      }
    }
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

    // Now copy the off-screen buffer to the widget
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_buffer);

  }
private slots:
  void resizeFinished() {
    m_resizing = false; // Clear flag when no resize event occurs for a while
    update();           // Force a final repaint
  }

private:
  QRubberBand *rubberBand;
  QPoint origin;
  bool m_resizing;
  QPixmap m_buffer;
  QTimer *m_resizeTimer;
};

class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Help");
        QVBoxLayout *layout = new QVBoxLayout(this);
        QTextEdit *textEdit = new QTextEdit(this);
        textEdit->setReadOnly(true);
        textEdit->setText("Keyboard shortcuts for navigation:\n\
n - next plot\n\
p - previous plot\n\
< - first plot\n\
> - last plot\n\
m - play movie\n\
d - delete plot\n\
\n\
Keyboard shortcuts for placement:\n\
b - bottom half\n\
t - top half\n\
l - left half\n\
r - right half\n\
1 - top-left quadrant\n\
2 - top-right quadrant\n\
3 - bottom-left quadrant\n\
4 - bottom-right quadrant\n\
c - center\n\
f - toggle full screen\n\
0 - original size\n\
\n\
Keyboard shortcuts for zooming:\n\
z - toggle replotting to zoom\n\
+ - increase window size\n\
- - decrease window size\n\
\n\
Other keyboard shortcuts:\n\
w - toggle white/black theme\n\
. - toggle mouse tracking\n\
q - quit");
        layout->addWidget(textEdit);
        setLayout(layout);
    }
};

/**
 * @brief Main entry point of the MPL Outboard Driver application.
 *
 * Sets up the main window, menus, canvas, and event loop.
 *
 * @param argc Number of command line arguments.
 * @param argv Command line arguments.
 * @return int Exit status.
 */
int main(int argc, char *argv[]) {
  lineTypeTable.nEntries = 0;
  lineTypeTable.typeFlag = 0x0000;

  QApplication app(argc, argv);
  QVector<Plot3DArgs> plots;
  Plot3DArgs current;
  bool in3d = false;
  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-3d", 3) && i + 1 < argc) {
      if (in3d)
        plots.append(current);
      current = Plot3DArgs();
      if (argv[i][3] == '=' && argv[i][4]) {
        if (!strcmp(argv[i] + 4, "bar"))
          current.bar = true;
      }
      current.file = argv[++i];
      in3d = true;
    } else if (!strcmp(argv[i], "-xlabel") && i + 1 < argc && in3d)
      current.xlabel = argv[++i];
    else if (!strcmp(argv[i], "-ylabel") && i + 1 < argc && in3d)
      current.ylabel = argv[++i];
    else if (!strcmp(argv[i], "-plottitle") && i + 1 < argc && in3d)
      current.title = argv[++i];
    else if (!strcmp(argv[i], "-topline") && i + 1 < argc && in3d)
      current.topline = argv[++i];
    else if (!strcmp(argv[i], "-fontsize") && i + 1 < argc && in3d)
      current.fontSize = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-equalaspect") && in3d)
      current.equalAspect = true;
    else if (!strcmp(argv[i], "-shade") && i + 1 < argc && in3d) {
      nspect = atoi(argv[++i]);
      spectrumallocated = 0;
      if (i + 1 < argc) {
        char *endptr = NULL;
        double val = strtod(argv[i + 1], &endptr);
        if (endptr != argv[i + 1] && *endptr == '\0') {
          current.shadeMin = val;
          i++;
          val = strtod(argv[i + 1], &endptr);
          if (endptr != argv[i + 1] && *endptr == '\0') {
            current.shadeMax = val;
            current.shadeRangeSet = true;
            i++;
          }
        }
      }
      if (i + 1 < argc && !strcmp(argv[i + 1], "gray")) {
        current.gray = true;
        i++;
      }
    } else if (!strcmp(argv[i], "-mapshade") && i + 2 < argc && in3d) {
      current.hue0 = atof(argv[++i]);
      current.hue1 = atof(argv[++i]);
    } else if (!strcmp(argv[i], "-yflip") && in3d)
      current.yFlip = true;
    else if (!strcmp(argv[i], "-noborder") && in3d)
      current.hideAxes = true;
    else if (!strcmp(argv[i], "-noscale") && in3d) {
      current.hideAxes = true;
      current.hideZAxis = true;
    } else if (!strcmp(argv[i], "-datestamp") && in3d)
      current.datestamp = true;
    else if (!strcmp(argv[i], "-xlog") && in3d)
      current.xLog = true;
    else if (!strncmp(argv[i], "-ticksettings", 13) && in3d) {
      const char *arg = NULL;
      if (argv[i][13] == '=')
        arg = argv[i] + 14;
      else if (i + 1 < argc)
        arg = argv[++i];
      if (arg) {
        if (strstr(arg, "xtime"))
          current.xTime = true;
        if (strstr(arg, "ytime"))
          current.yTime = true;
      }
    }
  }
  if (in3d)
    plots.append(current);

  // Create main window
  QMainWindow mainWindow;
  mainWindowPointer = &mainWindow;
  mainWindow.setWindowTitle("MPL Outboard Driver");

  // Add menus
  QMenu *fileMenu = mainWindow.menuBar()->addMenu("File");
  QMenu *navigateMenu = mainWindow.menuBar()->addMenu("Navigate");
  QMenu *optionsMenu = mainWindow.menuBar()->addMenu("Options");
  QMenu *helpMenu = mainWindow.menuBar()->addMenu("Help");

  QAction *printAction = new QAction("Print...", &mainWindow);
  fileMenu->addAction(printAction);
  QObject::connect(printAction, &QAction::triggered, &app, [](bool){ print(); });

  QAction *saveAction = new QAction("Save as PNG, JPEG or PDF...", &mainWindow);
  fileMenu->addAction(saveAction);
  QObject::connect(saveAction, &QAction::triggered, &app, [](bool){ save(); });

  QAction *quitAction = new QAction("Quit", &mainWindow);
  fileMenu->addAction(quitAction);
  QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

  QAction *nextAction = new QAction("Next", &mainWindow);
  navigateMenu->addAction(nextAction);
  QObject::connect(nextAction, &QAction::triggered, [&mainWindow]() {
    nav_next(&mainWindow);
  });

  QAction *previousAction = new QAction("Previous", &mainWindow);
  navigateMenu->addAction(previousAction);
  QObject::connect(previousAction, &QAction::triggered, [&mainWindow]() {
    nav_previous(&mainWindow);
  });

  QAction *deleteAction = new QAction("Delete", &mainWindow);
  navigateMenu->addAction(deleteAction);
  QObject::connect(deleteAction, &QAction::triggered, [&mainWindow]() {
    delete_current(&mainWindow);
  });

  QAction *toNumberAction = new QAction("To number...", &mainWindow);
  navigateMenu->addAction(toNumberAction);
  QObject::connect(toNumberAction, &QAction::triggered, [&mainWindow]() {
    to_number(&mainWindow);
  });

  replotZoomAction = new QAction("Replot when zooming", &mainWindow);
  replotZoomAction->setCheckable(true);
  replotZoomAction->setChecked(replotZoom);
  optionsMenu->addAction(replotZoomAction);
  QObject::connect(replotZoomAction, &QAction::toggled, &app, [&](bool checked){replotZoom = checked;});

  QAction *contentsAction = new QAction("Contents", &mainWindow);
  helpMenu->addAction(contentsAction);
  QObject::connect(contentsAction, &QAction::triggered, [&mainWindow]() {
    HelpDialog dialog(&mainWindow);
    dialog.exec();
  });

  // Create a central widget with a layout
  QWidget *centralWidget = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(centralWidget);

  if (!plots.isEmpty()) {
    plotStack = new QStackedWidget;
    for (int i = 0; i < plots.size(); ++i) {
      Plot3DArgs &p = plots[i];
      QWidget *plotWidget = run3d(p.file.toUtf8().constData(),
                                  p.xlabel.isEmpty() ? NULL : p.xlabel.toUtf8().constData(),
                                  p.ylabel.isEmpty() ? NULL : p.ylabel.toUtf8().constData(),
                                  p.title.isEmpty() ? NULL : p.title.toUtf8().constData(),
                                  p.topline.isEmpty() ? NULL : p.topline.toUtf8().constData(),
                                  p.bar, p.datestamp, p.fontSize, p.equalAspect,
                                  p.shadeMin, p.shadeMax, p.shadeRangeSet,
                                  p.gray, p.hue0, p.hue1, p.yFlip,
                                  p.hideAxes, p.hideZAxis, p.xLog, p.xTime,
                                  p.yTime);
      if (!plotWidget)
        return 1;
      plotStack->addWidget(plotWidget);
    }
    total3DPlots = plotStack->count();
    if (total3DPlots == 0)
      return 1;
    canvas = plotStack->widget(0);
    layout->addWidget(plotStack);
    mainWindow.setCentralWidget(centralWidget);
    current3DPlot = 0;
    QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(1).arg(total3DPlots);
    mainWindow.setWindowTitle(wtitle);
    surfaceGraph = surfaceGraphs[0];
    surfaceContainer = surfaceContainers[0];
    setup_shortcuts(&mainWindow);
    mainWindow.show();
    return app.exec();
  }

  QString shareName;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' || argv[i][0] == '/') {
      switch (argv[i][1]) {
      case 'c': /* sddsplot Commandline */
        if (!strcmp("command", argv[i] + 1)) {
          sddsplotCommandline2 = argv[++i];
        } else {
          fprintf(stderr, "Invalid option %s\n", argv[i]);
          exit(1);
        }
        break;
      case 'd': /* -dashes */
        if (!strcmp("dashes", argv[i] + 1)) {
          UseDashes = atoi(argv[++i]);
          dashPatterns[0] = QVector<qreal>();
          dashPatterns[1] << 4 << 2;
          dashPatterns[2] << 2 << 3;
          dashPatterns[3] << 1 << 2;
          dashPatterns[4] << 5 << 2 << 1 << 2;
          dashPatterns[5] << 3 << 3 << 1 << 4;
          dashPatterns[6] << 2 << 5;
          dashPatterns[7] << 4 << 4 << 4 << 1;
          dashPatterns[8] << 8 << 2;
          dashPatterns[9] << 1 << 4;
        } else {
          fprintf(stderr, "Invalid option %s\n", argv[i]);
          exit(1);
        }
        break;
      case 'l': /* -linetype file*/
        if (!strcmp("linetype", argv[i] + 1)) {
          LineTableFile = argv[++i];
          if (!LineTableFile) {
            fprintf(stderr, "Need linetype file names\n");
            exit(1);
          }
          if (!SDDS_ReadLineTypeTable(&lineTypeTable, LineTableFile)) {
            fprintf(stderr, "Problem to read the line type file.\n");
            exit(1);
          }
        } else {
          fprintf(stderr, "Invalid option %s\n", argv[i]);
          exit(1);
        }
        break;
      case 'h': /* Put usage here */
        fprintf(stdout, "Usage: mpl_qt [-h]\n"
                        "              [-dashes 1]\n"
                        "              [-linetype <filename>]\n"
                        "              [-movie 1 [-interval 1]]\n"
                        "              [-keep <number>]\n"
                        "              [-share <name>]\n"
                        "              [-timeoutHours <hours>]\n");
        fprintf(stdout, "Example: sddsplot \"-device=qt,-dashes 1 -movie 1 -interval 5\"\n");
        exit(0);
        break;
      case 'i': /* -interval */
        if (!strcmp("interval", argv[i] + 1)) {
          movieIntervalTime = atof(argv[++i]);
          if (movieIntervalTime < 0) {
            fprintf(stderr, "Invalid value (%f) for -interval\n", movieIntervalTime);
            exit(1);
          }
          if (movieIntervalTime > 60) {
            fprintf(stderr, "-interval value is over 60 seconds\n");
            exit(1);
          }
        } else {
          fprintf(stderr, "Invalid option %s\n", argv[i]);
          exit(1);
        }
        break;
      case 'k': /* -keep */
        if (!strcmp("keep", argv[i] + 1)) {
          keep = atof(argv[++i]);
          if (keep < 1) {
            fprintf(stderr, "Invalid value (%d) for -keep\n", keep);
            exit(1);
          }
        } else {
          fprintf(stderr, "Invalid option %s\n", argv[i]);
          exit(1);
        }
        break;
      case 'm': /* -movie */
        if (!strcmp("movie", argv[i] + 1)) {
          domovie = true;
        } else {
          fprintf(stderr, "Invalid option %s\n", argv[i]);
          exit(1);
        }
        break;
      case 's': /* -spectrum or -share */
        if (!strcmp("spectrum", argv[i] + 1)) {
          if (!spectrumallocated)
            allocspectrum();
        } else if (!strcmp("share", argv[i] + 1)) {
          shareName = argv[++i];
        } else {
          fprintf(stderr, "Invalid option %s\n", argv[i]);
          exit(1);
        }
        break;
      case 't': /* -timeoutHours */
        if (!strcmp("timeoutHours", argv[i] + 1)) {
          timeoutHours = atof(argv[++i]);
          if (timeoutHours < 0) {
            fprintf(stderr, "Invalid value (%f) for -timeoutHours\n", timeoutHours);
            exit(1);
          }
          if (timeoutHours > 8760) {
            fprintf(stderr, "-timeoutHours value is over 1 year\n");
            exit(1);
          }
          timeoutSec = timeoutHours * 60 * 60 * 1000;
          QTimer::singleShot(timeoutSec, &app, SLOT(quit()));
        } else {
          fprintf(stderr, "Invalid option %s\n", argv[i]);
          exit(1);
        }
        break;
      default:
        fprintf(stderr, "Invalid option %s\n", argv[i]);
        exit(1);
      }
    }
  }

  QLocalServer *server = NULL;
  if (!shareName.isEmpty()) {
    QLocalSocket sock;
    sock.connectToServer(shareName);
    if (sock.waitForConnected(100)) {
      char buffer[4096];
      size_t n;
      while ((n = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
        sock.write(buffer, n);
        sock.flush();
        sock.waitForBytesWritten(-1);
      }
      sock.flush();
      sock.waitForBytesWritten(-1);
      sock.disconnectFromServer();
      if (sock.state() != QLocalSocket::UnconnectedState)
        sock.waitForDisconnected();
      return 0;
    } else {
      server = new QLocalServer(&mainWindow);
      if (!server->listen(shareName)) {
        fprintf(stderr, "Unable to listen on share %s\n", shareName.toLocal8Bit().constData());
        return 1;
      }
      QObject::connect(server, &QLocalServer::newConnection, [&]() {
        QLocalSocket *socket = server->nextPendingConnection();
        int fd = dup(socket->socketDescriptor());
        if (fd == -1) {
          perror("dup");
          socket->deleteLater();
          return;
        }
        int previousPlots = nplots;
        startReader(fd);
        stdinReader->handleActivated(0);
        if (nplots > previousPlots) {
          currentPlot = previousPlots + 1;
          cur = last;
          while (cur->prev && cur->nplot > currentPlot)
            cur = cur->prev;
          canvas->update();
          QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
          mainWindow.setWindowTitle(wtitle);
        }
        makeWindowVisible(&mainWindow);
        QObject::connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
      });
    }
  }

  // Add a large black frame (serves as a canvas for future graph)
  canvas = new Canvas;
  canvas->resize(WIDTH, HEIGHT);

  layout->addWidget(canvas);
  mainWindow.setCentralWidget(centralWidget);

  mainWindow.resize(WIDTH + 20, HEIGHT + 40);
  mainWindow.show();

  setup_shortcuts(&mainWindow);

  alloccolors();

  startReader(0);

  return app.exec();
}

#include "mpl_qt_moc.h"
