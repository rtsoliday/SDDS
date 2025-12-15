/**
 * @file mpl_qt.cc
 * @brief MPL Outboard Driver for plotting using Qt.
 *
 * This file implements the main functions and classes required for the MPL outboard driver.
 */

#include "mpl_qt.h"
static bool isEqualAspectArgument(const char *arg) {
  if (!arg)
    return false;
  const char *options[] = {"-equalaspect", "-equalAspect"};
  for (const char *option : options) {
    const size_t len = strlen(option);
    if (!strncmp(arg, option, len) && (arg[len] == '\0' || arg[len] == '='))
      return true;
  }
  return false;
}

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
#include <QtDataVisualization/Q3DScatter>
#include <QtDataVisualization/QScatter3DSeries>
#include <QtDataVisualization/QScatterDataArray>
#include <QtDataVisualization/QScatterDataProxy>
#include <QtDataVisualization/Q3DInputHandler>
#include <QtDataVisualization/QAbstract3DSeries>
#include <QtDataVisualization/QAbstract3DGraph>
#include <QtDataVisualization/QAbstract3DAxis>
#include <QtDataVisualization/QAbstract3DInputHandler>
#include <QtDataVisualization/QCategory3DAxis>
#include <QtDataVisualization/Q3DTheme>
#include <QtDataVisualization/Q3DCamera>
#include <QtDataVisualization/QValue3DAxis>
#include <QtDataVisualization/QLogValue3DAxisFormatter>
#include <QtDataVisualization/QValue3DAxisFormatter>
#include <QLinearGradient>
#include <QCursor>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <QByteArray>
#include <QVector3D>
#include <QFont>
#include <QVector>
#include <QStringList>
#include <QRegularExpression>
#include <QShortcut>
#include <QLabel>
#include <QPalette>
#include <QSizePolicy>
#include <QDateTime>
#include <QList>
#include <QMatrix4x4>
#include <QWheelEvent>
#include <QtGlobal>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <float.h>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <limits>
#ifdef _WIN32
#  include <windows.h>
#elif defined(__APPLE__)
#  include <ApplicationServices/ApplicationServices.h>
#  include <objc/objc.h>
#  include <objc/runtime.h>
#  include <objc/message.h>
#endif

using QT_DATAVIS_NAMESPACE::Q3DInputHandler;

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
QAction *mouseTrackerAction = nullptr;
QWidget *canvas;
QMainWindow *mainWindowPointer;
QT_DATAVIS_NAMESPACE::QAbstract3DGraph *surfaceGraph = nullptr;
QWidget *surfaceContainer = nullptr;
QStackedWidget *plotStack = nullptr;
int current3DPlot = 0;
int total3DPlots = 0;
QVector<QT_DATAVIS_NAMESPACE::QAbstract3DGraph *> surfaceGraphs;
QVector<QWidget *> surfaceContainers;

enum Plot3DMode { PLOT3D_SURFACE, PLOT3D_BAR, PLOT3D_SCATTER };

struct Plot3DArgs {
  QString file;
  QString xlabel;
  QString ylabel;
  QString zlabel;
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
  bool hideGrid;
  bool hideXAxisScale;
  bool hideYAxisScale;
  bool hideZAxisScale;
  bool datestamp;
  bool xLog;
  bool xTime;
  bool yTime;
  int mode;
  Plot3DArgs()
      : fontSize(0), equalAspect(false), shadeMin(0.0), shadeMax(0.0),
        shadeRangeSet(false), gray(false), hue0(0.0), hue1(1.0), yFlip(false),
        hideAxes(false), hideGrid(false), hideXAxisScale(false),
        hideYAxisScale(false), hideZAxisScale(false), datestamp(false),
        xLog(false), xTime(false), yTime(false), mode(PLOT3D_SURFACE) {}
};

struct Plot3DGridData {
  int nx = 0;
  int ny = 0;
  double xmin = 0.0;
  double xmax = 0.0;
  double ymin = 0.0;
  double ymax = 0.0;
  QVector<double> values;
};

static const long long kLargeDatasetThreshold = 100000;
static const long long kAggressiveOptimizationThreshold = 200000;
static const int kMax3DBarAxisResolution = 256;

static void applyLargeDatasetGraphSettings(
    QT_DATAVIS_NAMESPACE::QAbstract3DGraph *graph, long long totalPoints) {
  if (!graph)
    return;
  if (totalPoints > kLargeDatasetThreshold) {
    graph->setShadowQuality(
        QT_DATAVIS_NAMESPACE::QAbstract3DGraph::ShadowQualityNone);
    graph->setOptimizationHints(
        QT_DATAVIS_NAMESPACE::QAbstract3DGraph::OptimizationStatic);
  }
  if (totalPoints > kAggressiveOptimizationThreshold)
    graph->setSelectionMode(QT_DATAVIS_NAMESPACE::QAbstract3DGraph::SelectionNone);
}

static bool parseTicksettingsArgument(int argc, char **argv, int &index,
                                      const char **value) {
  if (!argv || index < 0 || index >= argc)
    return false;
  const char *arg = argv[index];
  if (!arg)
    return false;
  const char *valueStart = nullptr;
  if (!strncmp(arg, "-ticksettings", 13))
    valueStart = arg + 13;
  else if (!strncmp(arg, "-tick", 5) &&
           (arg[5] == '\0' || arg[5] == '='))
    valueStart = arg + 5;
  else
    return false;

  if (valueStart && *valueStart == '\0') {
    if (index + 1 < argc)
      valueStart = argv[++index];
    else
      valueStart = nullptr;
  } else if (valueStart && *valueStart == '=')
    valueStart++;

  if (value)
    *value = (valueStart && *valueStart) ? valueStart : nullptr;
  return true;
}

static bool loadGridData(const char *filename, Plot3DGridData &data,
                         QString *errorMessage = nullptr) {
  QFile dataFile(filename);
  if (!dataFile.open(QIODevice::ReadOnly)) {
    if (errorMessage)
      *errorMessage =
          QStringLiteral("Unable to open %1").arg(QString::fromUtf8(filename));
    return false;
  }

  QByteArray content = dataFile.readAll();
  if (content.isEmpty()) {
    if (errorMessage)
      *errorMessage =
          QStringLiteral("No data in %1").arg(QString::fromUtf8(filename));
    return false;
  }

  const char *ptr = content.constData();
  const char *end = ptr + content.size();
  auto skipWhitespace = [&]() {
    while (ptr < end && std::isspace(static_cast<unsigned char>(*ptr)))
      ++ptr;
  };
  auto readInt = [&](int &value) -> bool {
    skipWhitespace();
    if (ptr >= end)
      return false;
    char *nextPtr = nullptr;
    long parsed = std::strtol(ptr, &nextPtr, 10);
    if (nextPtr == ptr)
      return false;
    if (parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max())
      return false;
    value = static_cast<int>(parsed);
    ptr = nextPtr;
    return true;
  };
  auto readDouble = [&](double &value) -> bool {
    skipWhitespace();
    if (ptr >= end)
      return false;
    char *nextPtr = nullptr;
    value = std::strtod(ptr, &nextPtr);
    if (nextPtr == ptr)
      return false;
    ptr = nextPtr;
    return true;
  };

  if (!readInt(data.nx) || !readInt(data.ny) || !readDouble(data.xmin) ||
      !readDouble(data.xmax) || !readDouble(data.ymin) ||
      !readDouble(data.ymax)) {
    if (errorMessage)
      *errorMessage = QStringLiteral("Invalid grid header in %1")
                           .arg(QString::fromUtf8(filename));
    return false;
  }
  if (data.nx <= 0 || data.ny <= 0) {
    if (errorMessage)
      *errorMessage = QStringLiteral("Invalid grid dimensions in %1")
                           .arg(QString::fromUtf8(filename));
    return false;
  }

  const qint64 totalPoints =
      static_cast<qint64>(data.nx) * static_cast<qint64>(data.ny);
  if (totalPoints <= 0 ||
      totalPoints > static_cast<qint64>(std::numeric_limits<int>::max())) {
    if (errorMessage)
      *errorMessage = QStringLiteral("Grid size is too large in %1")
                           .arg(QString::fromUtf8(filename));
    return false;
  }

  data.values.resize(static_cast<int>(totalPoints));
  for (int j = 0; j < data.ny; ++j) {
    for (int i = 0; i < data.nx; ++i) {
      double z = 0.0;
      if (!readDouble(z)) {
        if (errorMessage)
          *errorMessage = QStringLiteral("Insufficient grid data in %1")
                               .arg(QString::fromUtf8(filename));
        return false;
      }
      data.values[j * data.nx + i] = z;
    }
  }

  return true;
}

class Time3DAxisFormatter : public QT_DATAVIS_NAMESPACE::QValue3DAxisFormatter {
public:
  QString stringForValue(qreal value, const QString &format) const override {
    Q_UNUSED(format);
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(value))
        .toString("yyyy-MM-dd HH:mm:ss");
  }
};

class Pan3DInputHandler : public Q3DInputHandler {
  Q_OBJECT
public:
  explicit Pan3DInputHandler(QT_DATAVIS_NAMESPACE::QAbstract3DGraph *graph,
                             QObject *parent = nullptr)
      : Q3DInputHandler(parent), m_graph(graph),
        m_isPanning(false) {}

protected:
  void mousePressEvent(QMouseEvent *event,
                       const QPoint &mousePos) override {
    if (event->button() == Qt::RightButton &&
        (event->modifiers() & Qt::ControlModifier)) {
      m_isPanning = true;
      m_lastPos = mousePos;
      setInputPosition(mousePos);
      setPreviousInputPos(mousePos);
      event->accept();
      return;
    }
    Q3DInputHandler::mousePressEvent(event, mousePos);
  }

  void mouseReleaseEvent(QMouseEvent *event,
                         const QPoint &mousePos) override {
    if (m_isPanning && event->button() == Qt::RightButton) {
      m_isPanning = false;
      setInputPosition(mousePos);
      setPreviousInputPos(mousePos);
      event->accept();
      return;
    }
    Q3DInputHandler::mouseReleaseEvent(event, mousePos);
  }

  void wheelEvent(QWheelEvent *event) override {
    if (!m_graph) {
      Q3DInputHandler::wheelEvent(event);
      return;
    }
    if ((event->modifiers() & Qt::ControlModifier)) {
      auto *scatter =
          qobject_cast<QT_DATAVIS_NAMESPACE::Q3DScatter *>(m_graph);
      if (scatter) {
        QPoint degrees = event->angleDelta();
        float stepCount = 0.0f;
        if (!degrees.isNull())
          stepCount = static_cast<float>(degrees.y()) / 120.0f;
        else {
          QPoint pixels = event->pixelDelta();
          if (!pixels.isNull())
            stepCount = static_cast<float>(pixels.y()) / 40.0f;
        }
        if (!qFuzzyIsNull(stepCount)) {
          adjustScatterItemSize(scatter, stepCount);
          event->accept();
          return;
        }
      }
    }
    Q3DInputHandler::wheelEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event, const QPoint &mousePos) override {
    if (m_isPanning) {
      if (!(event->buttons() & Qt::RightButton) ||
          !(event->modifiers() & Qt::ControlModifier)) {
        m_isPanning = false;
        Q3DInputHandler::mouseMoveEvent(event, mousePos);
        return;
      }
      QPoint delta = mousePos - m_lastPos;
      if (!delta.isNull())
        panCamera(delta);
      m_lastPos = mousePos;
      setInputPosition(mousePos);
      setPreviousInputPos(mousePos);
      event->accept();
      return;
    }
    Q3DInputHandler::mouseMoveEvent(event, mousePos);
  }

private:
  void adjustScatterItemSize(QT_DATAVIS_NAMESPACE::Q3DScatter *scatter,
                             float wheelSteps) {
    if (!scatter || qFuzzyIsNull(wheelSteps))
      return;
    const qreal devicePixelRatio = scatter->devicePixelRatio();
    qreal width = scatter->size().width() * devicePixelRatio;
    qreal height = scatter->size().height() * devicePixelRatio;
    qreal reference = qMin(width, height);
    if (reference <= 0.0)
      reference = 1.0;
    const float minPixelSize = 1.0f;
    const float pixelStep = 2.0f;
    float deltaPixels = wheelSteps * pixelStep;
    if (qFuzzyIsNull(deltaPixels))
      return;
    QList<QT_DATAVIS_NAMESPACE::QScatter3DSeries *> seriesList =
        scatter->seriesList();
    if (seriesList.isEmpty())
      return;
    for (QT_DATAVIS_NAMESPACE::QScatter3DSeries *series : seriesList) {
      if (!series)
        continue;
      float currentSize = series->itemSize();
      if (currentSize <= 0.0f)
        currentSize = 0.05f;
      float currentPixels = currentSize * reference;
      float newPixelSize = currentPixels + deltaPixels;
      if (newPixelSize < minPixelSize)
        newPixelSize = minPixelSize;
      float newSize = newPixelSize / reference;
      float minSize = minPixelSize / reference;
      if (newSize < minSize)
        newSize = minSize;
      if (newSize > 1.0f)
        newSize = 1.0f;
      series->setItemSize(newSize);
    }
  }

  double axisSpan(QT_DATAVIS_NAMESPACE::QAbstract3DAxis *axis) const {
    if (!axis)
      return 1.0;
    if (auto valueAxis = qobject_cast<QT_DATAVIS_NAMESPACE::QValue3DAxis *>(axis))
      return qMax(1e-9, static_cast<double>(valueAxis->max() - valueAxis->min()));
    if (auto categoryAxis =
            qobject_cast<QT_DATAVIS_NAMESPACE::QCategory3DAxis *>(axis))
      return qMax(1.0, static_cast<double>(categoryAxis->labels().size()));
    return 1.0;
  }

  void panCamera(const QPoint &delta) {
    if (!m_graph)
      return;
    QT_DATAVIS_NAMESPACE::Q3DScene *graphScene = scene();
    if (!graphScene)
      return;
    QT_DATAVIS_NAMESPACE::Q3DCamera *camera = graphScene->activeCamera();
    if (!camera)
      return;

    QT_DATAVIS_NAMESPACE::QAbstract3DAxis *axisX = nullptr;
    QT_DATAVIS_NAMESPACE::QAbstract3DAxis *axisY = nullptr;
    QT_DATAVIS_NAMESPACE::QAbstract3DAxis *axisZ = nullptr;
    if (auto surface =
            qobject_cast<QT_DATAVIS_NAMESPACE::Q3DSurface *>(m_graph)) {
      axisX = surface->axisX();
      axisY = surface->axisY();
      axisZ = surface->axisZ();
    } else if (auto scatter =
                   qobject_cast<QT_DATAVIS_NAMESPACE::Q3DScatter *>(m_graph)) {
      axisX = scatter->axisX();
      axisY = scatter->axisY();
      axisZ = scatter->axisZ();
    } else if (auto bars =
                   qobject_cast<QT_DATAVIS_NAMESPACE::Q3DBars *>(m_graph)) {
      axisX = bars->columnAxis();
      axisY = bars->valueAxis();
      axisZ = bars->rowAxis();
    }
    double spanX = axisSpan(axisX);
    double spanY = axisSpan(axisY);
    double spanZ = axisSpan(axisZ);
    double spanAvg = (spanX + spanY + spanZ) / 3.0;
    if (spanAvg <= 0.0)
      spanAvg = 1.0;

    float zoomLevel = camera->zoomLevel();
    double zoomFactor = 100.0 / qMax(1.0f, zoomLevel);
    double panFactor = spanAvg * 0.002 * zoomFactor;

    QMatrix4x4 rotation;
    rotation.rotate(camera->yRotation(), 0.0f, 1.0f, 0.0f);
    rotation.rotate(camera->xRotation(), 1.0f, 0.0f, 0.0f);
    QVector3D forward = rotation * QVector3D(0.0f, 0.0f, -1.0f);
    QVector3D up = rotation * QVector3D(0.0f, 1.0f, 0.0f);
    QVector3D right = QVector3D::crossProduct(forward, up).normalized();
    up = QVector3D::crossProduct(right, forward).normalized();

    QVector3D translation = (-delta.x() * panFactor * right) +
                            (delta.y() * panFactor * up);
    QVector3D target = camera->target();
    target += translation;

    if (auto valueAxis =
            qobject_cast<QT_DATAVIS_NAMESPACE::QValue3DAxis *>(axisX))
      target.setX(qBound(valueAxis->min(), target.x(), valueAxis->max()));
    if (auto valueAxis =
            qobject_cast<QT_DATAVIS_NAMESPACE::QValue3DAxis *>(axisY))
      target.setY(qBound(valueAxis->min(), target.y(), valueAxis->max()));
    if (auto valueAxis =
            qobject_cast<QT_DATAVIS_NAMESPACE::QValue3DAxis *>(axisZ))
      target.setZ(qBound(valueAxis->min(), target.z(), valueAxis->max()));

    camera->setTarget(target);
  }

  QT_DATAVIS_NAMESPACE::QAbstract3DGraph *m_graph;
  bool m_isPanning;
  QPoint m_lastPos;
};

static void installPanHandler(QT_DATAVIS_NAMESPACE::QAbstract3DGraph *graph) {
  if (!graph)
    return;
  QT_DATAVIS_NAMESPACE::QAbstract3DInputHandler *baseHandler =
      graph->activeInputHandler();
  Q3DInputHandler *existingHandler = qobject_cast<Q3DInputHandler *>(baseHandler);
  Pan3DInputHandler *panHandler = new Pan3DInputHandler(graph);
  panHandler->setScene(graph->scene());
  if (existingHandler) {
    panHandler->setInputView(existingHandler->inputView());
    panHandler->setRotationEnabled(existingHandler->isRotationEnabled());
    panHandler->setZoomEnabled(existingHandler->isZoomEnabled());
    panHandler->setSelectionEnabled(existingHandler->isSelectionEnabled());
    panHandler->setZoomAtTargetEnabled(existingHandler->isZoomAtTargetEnabled());
  } else if (baseHandler) {
    panHandler->setInputView(baseHandler->inputView());
  }
  graph->setActiveInputHandler(panHandler);
}

static QWidget *run3dBar(const char *filename, const char *xlabel,
                         const char *ylabel, const char *zlabel,
                         const char *title, const char *topline,
                         bool datestamp, int fontSize, bool equalAspect,
                         double shadeMin, double shadeMax,
                         bool shadeRangeSet, bool gray, double hue0,
                         double hue1, bool yFlip, bool hideAxes, bool hideGrid,
                         bool hideXAxisScale, bool hideYAxisScale,
                         bool hideZAxisScale, bool xLog, bool xTime,
                         bool yTime) {
  QT_DATAVIS_NAMESPACE::Q3DBars *graph = new QT_DATAVIS_NAMESPACE::Q3DBars();
  surfaceGraph = graph;
  if (equalAspect) {
    graph->setHorizontalAspectRatio(1.0f);
    graph->setAspectRatio(1.0f);
  }
  QT_DATAVIS_NAMESPACE::Q3DTheme *theme = graph->activeTheme();
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
  QT_DATAVIS_NAMESPACE::Q3DCamera *camera = graph->scene()->activeCamera();
  camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetIsometricRightHigh); // Default orientation
  float defaultX = camera->xRotation();
  float defaultY = camera->yRotation();
  float defaultZoom = camera->zoomLevel();
  QVector3D defaultTarget = camera->target();
  installPanHandler(graph);
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
    graph->columnAxis()->setTitle(QString::fromUtf8(xlabel));
    graph->columnAxis()->setTitleVisible(true);
  }
  if (ylabel) {
    graph->rowAxis()->setTitle(QString::fromUtf8(ylabel));
    graph->rowAxis()->setTitleVisible(true);
  }
  if (zlabel) {
    graph->valueAxis()->setTitle(QString::fromUtf8(zlabel));
    graph->valueAxis()->setTitleVisible(true);
  }
  if (hideGrid) {
    theme->setGridEnabled(false);
  }
  if (hideAxes || hideYAxisScale) {
    graph->rowAxis()->setTitleVisible(false);
    graph->rowAxis()->setLabels(QStringList());
  }
  if (hideAxes || hideXAxisScale) {
    graph->columnAxis()->setTitleVisible(false);
    graph->columnAxis()->setLabels(QStringList());
  }
  if (hideAxes || hideZAxisScale) {
    graph->valueAxis()->setTitleVisible(false);
    graph->valueAxis()->setLabelFormat("");
  }
  Plot3DGridData gridData;
  QString loadError;
  if (!loadGridData(filename, gridData, &loadError)) {
    fprintf(stderr, "%s\n", loadError.toLocal8Bit().constData());
    return NULL;
  }

  int nx = gridData.nx;
  int ny = gridData.ny;
  double xmin = gridData.xmin;
  double xmax = gridData.xmax;
  double ymin = gridData.ymin;
  double ymax = gridData.ymax;

  const long long totalPoints =
      static_cast<long long>(nx) * static_cast<long long>(ny);
  applyLargeDatasetGraphSettings(graph, totalPoints);
  const bool largeDataset = totalPoints > kLargeDatasetThreshold;
  const bool disableItemLabels =
      totalPoints > kAggressiveOptimizationThreshold;

  const int originalNx = nx;
  const int originalNy = ny;
  const double originalDx =
      originalNx > 1 ? (xmax - xmin) / (originalNx - 1) : 0.0;
  const double originalDy =
      originalNy > 1 ? (ymax - ymin) / (originalNy - 1) : 0.0;
  const int downsampleStepX =
      originalNx > kMax3DBarAxisResolution
          ? (originalNx + kMax3DBarAxisResolution - 1) /
                kMax3DBarAxisResolution
          : 1;
  const int downsampleStepY =
      originalNy > kMax3DBarAxisResolution
          ? (originalNy + kMax3DBarAxisResolution - 1) /
                kMax3DBarAxisResolution
          : 1;
  QVector<int> xSampleIndices;
  QVector<int> ySampleIndices;
  if (downsampleStepX > 1 || downsampleStepY > 1) {
    int newNx = (originalNx + downsampleStepX - 1) / downsampleStepX;
    int newNy = (originalNy + downsampleStepY - 1) / downsampleStepY;
    QVector<double> downsampledValues(newNx * newNy);
    xSampleIndices.resize(newNx);
    ySampleIndices.resize(newNy);
    const double *originalValues = gridData.values.constData();
    for (int j = 0; j < newNy; ++j) {
      int origYStart = j * downsampleStepY;
      int origYEnd = std::min(origYStart + downsampleStepY, originalNy);
      if (origYStart >= origYEnd)
        origYEnd = std::min(origYStart + 1, originalNy);
      ySampleIndices[j] = (origYStart + origYEnd - 1) / 2;
      for (int i = 0; i < newNx; ++i) {
        int origXStart = i * downsampleStepX;
        int origXEnd = std::min(origXStart + downsampleStepX, originalNx);
        if (origXStart >= origXEnd)
          origXEnd = std::min(origXStart + 1, originalNx);
        if (j == 0)
          xSampleIndices[i] = (origXStart + origXEnd - 1) / 2;
        double sum = 0.0;
        int count = 0;
        for (int yIndex = origYStart; yIndex < origYEnd; ++yIndex) {
          const double *row = originalValues + yIndex * originalNx;
          for (int xIndex = origXStart; xIndex < origXEnd; ++xIndex) {
            sum += row[xIndex];
            ++count;
          }
        }
        downsampledValues[j * newNx + i] = count ? sum / count : 0.0;
      }
    }
    gridData.values = std::move(downsampledValues);
    nx = newNx;
    ny = newNy;
    fprintf(stderr,
            "mpl_qt: downsampled 3D bar grid from %d x %d to %d x %d to improve performance.\n",
            originalNx, originalNy, nx, ny);
  } else {
    xSampleIndices.resize(nx);
    ySampleIndices.resize(ny);
    for (int i = 0; i < nx; ++i)
      xSampleIndices[i] = i;
    for (int j = 0; j < ny; ++j)
      ySampleIndices[j] = j;
  }

  if (equalAspect && nx > 0 && ny > 0) {
    QSizeF defaultSpacing = graph->barSpacing();
    double baseSpacing = std::max(defaultSpacing.width(), defaultSpacing.height());
    double ratio = static_cast<double>(ny) / static_cast<double>(nx);
    double sqrtRatio = std::sqrt(ratio);
    double invSqrtRatio = sqrtRatio > 0 ? 1.0 / sqrtRatio : 1.0;
    double minScale = std::max(sqrtRatio, invSqrtRatio);
    double spacingScale = std::max(baseSpacing + 1.0, minScale);
    double spacingX = spacingScale * sqrtRatio - 1.0;
    double spacingY = spacingScale * invSqrtRatio - 1.0;
    spacingX = std::max(spacingX, 0.0);
    spacingY = std::max(spacingY, 0.0);
    graph->setBarSpacingRelative(true);
    graph->setBarSpacing(QSizeF(spacingX, spacingY));
    graph->setBarThickness(1.0f);
  }
  QStringList xLabels, yLabels;
  xLabels.reserve(nx);
  yLabels.reserve(ny);
  const int maxLabels = largeDataset ? 4 : 6;
  int xLabelStep = 1;
  if (nx > maxLabels)
    xLabelStep = std::max(1, (nx - 1) / (maxLabels - 1));
  for (int i = 0; i < nx; ++i) {
    bool drawLabel = (nx <= maxLabels) || i == 0 || i == nx - 1 || (i % xLabelStep == 0);
    if (!drawLabel) {
      xLabels.append(QString());
      continue;
    }
    int sampleIndex = xSampleIndices[i];
    double xval = xmin;
    if (originalNx > 1)
      xval = xmin + sampleIndex * originalDx;
    if (xLog)
      xval = pow(10.0, xval);
    if (xTime)
      xLabels.append(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(xval))
                         .toString("yyyy-MM-dd HH:mm:ss"));
    else
      xLabels.append(QString::number(xval));
  }
  int yLabelStep = 1;
  if (ny > maxLabels)
    yLabelStep = std::max(1, (ny - 1) / (maxLabels - 1));
  for (int j = 0; j < ny; ++j) {
    bool drawLabel = (ny <= maxLabels) || j == 0 || j == ny - 1 || (j % yLabelStep == 0);
    if (!drawLabel) {
      yLabels.append(QString());
      continue;
    }
    int sampleIndex = ySampleIndices[j];
    double yval = ymin;
    if (originalNy > 1)
      yval = ymin + sampleIndex * originalDy;
    if (yTime)
      yLabels.append(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(yval))
                         .toString("yyyy-MM-dd HH:mm:ss"));
    else
      yLabels.append(QString::number(yval));
  }
  graph->columnAxis()->setLabels(xLabels);
  graph->rowAxis()->setLabels(yLabels);
  if (yFlip)
    ;
  QT_DATAVIS_NAMESPACE::QBarDataArray *dataArray = new QT_DATAVIS_NAMESPACE::QBarDataArray;
  dataArray->reserve(ny);
  double zmin = DBL_MAX, zmax = -DBL_MAX;
  const double *gridPtr = gridData.values.constData();
  for (int j = 0; j < ny; j++) {
    QT_DATAVIS_NAMESPACE::QBarDataRow *row = new QT_DATAVIS_NAMESPACE::QBarDataRow(nx);
    const double *rowValues = gridPtr + j * nx;
    for (int i = 0; i < nx; i++) {
      double z = rowValues[i];
      (*row)[i].setValue(z);
      if (z < zmin)
        zmin = z;
      if (z > zmax)
        zmax = z;
    }
    dataArray->append(row);
  }
  QT_DATAVIS_NAMESPACE::QBarDataProxy *proxy = new QT_DATAVIS_NAMESPACE::QBarDataProxy;
  proxy->resetArray(dataArray);
  if (shadeRangeSet) {
    zmin = shadeMin;
    zmax = shadeMax;
  }
  QT_DATAVIS_NAMESPACE::QBar3DSeries *series = new QT_DATAVIS_NAMESPACE::QBar3DSeries(proxy);
  if (disableItemLabels)
    series->setItemLabelFormat(QString());
  if (largeDataset) {
    series->setMesh(QT_DATAVIS_NAMESPACE::QAbstract3DSeries::MeshCube);
    series->setMeshSmooth(false);
  } else {
    series->setMesh(QT_DATAVIS_NAMESPACE::QAbstract3DSeries::MeshBevelBar);
    series->setMeshSmooth(true);
  }
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
  series->setColorStyle(QT_DATAVIS_NAMESPACE::Q3DTheme::ColorStyleRangeGradient);
  graph->valueAxis()->setRange(zmin, zmax);
  graph->setFloorLevel(zmin);
  graph->addSeries(series);
  QShortcut *resetView = new QShortcut(QKeySequence(QStringLiteral("i")), widget);
  QObject::connect(resetView, &QShortcut::activated,
                   [camera, defaultX, defaultY, defaultZoom, defaultTarget]() {
                     camera->setXRotation(defaultX);
                     camera->setYRotation(defaultY);
                     camera->setZoomLevel(defaultZoom);
                     camera->setTarget(defaultTarget);
                   });
  QShortcut *snapX = new QShortcut(QKeySequence(QStringLiteral("x")), widget);
  QObject::connect(snapX, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetRight); });
  QShortcut *snapY = new QShortcut(QKeySequence(QStringLiteral("y")), widget);
  QObject::connect(snapY, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetFront); });
  QShortcut *snapZ = new QShortcut(QKeySequence(QStringLiteral("z")), widget);
  QObject::connect(snapZ, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetDirectlyAbove); });
  QShortcut *increaseFont =
      new QShortcut(QKeySequence(Qt::Key_Up), widget);
  QObject::connect(increaseFont, &QShortcut::activated,
                   [theme, graph]() {
                     QFont f = theme->font();
                     f.setPointSize(f.pointSize() + 1);
                     theme->setFont(f);
                     graph->rowAxis()->setTitle(graph->rowAxis()->title());
                     graph->columnAxis()->setTitle(graph->columnAxis()->title());
                     graph->valueAxis()->setTitle(graph->valueAxis()->title());
                   });
  QShortcut *decreaseFont =
      new QShortcut(QKeySequence(Qt::Key_Down), widget);
  QObject::connect(decreaseFont, &QShortcut::activated,
                   [theme, graph]() {
                     QFont f = theme->font();
                     if (f.pointSize() > 1)
                       f.setPointSize(f.pointSize() - 1);
                     theme->setFont(f);
                     graph->rowAxis()->setTitle(graph->rowAxis()->title());
                     graph->columnAxis()->setTitle(graph->columnAxis()->title());
                     graph->valueAxis()->setTitle(graph->valueAxis()->title());
                   });
  return widget;
}

static QWidget *run3dScatter(const char *filename, const char *xlabel,
                             const char *ylabel, const char *zlabel,
                             const char *title, const char *topline,
                             bool datestamp, int fontSize, bool equalAspect,
                             double shadeMin, double shadeMax,
                             bool shadeRangeSet, bool gray, double hue0,
                             double hue1, bool yFlip, bool hideAxes, bool hideGrid,
                             bool hideXAxisScale, bool hideYAxisScale,
                             bool hideZAxisScale, bool xLog, bool xTime,
                             bool yTime) {
  QT_DATAVIS_NAMESPACE::Q3DScatter *graph = new QT_DATAVIS_NAMESPACE::Q3DScatter();
  surfaceGraph = graph;
  if (equalAspect) {
    graph->setHorizontalAspectRatio(1.0f);
    graph->setAspectRatio(1.0f);
  }
  QT_DATAVIS_NAMESPACE::Q3DTheme *theme = graph->activeTheme();
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
  QT_DATAVIS_NAMESPACE::Q3DCamera *camera = graph->scene()->activeCamera();
  camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetIsometricRightHigh);
  float defaultX = camera->xRotation();
  float defaultY = camera->yRotation();
  float defaultZoom = camera->zoomLevel();
  QVector3D defaultTarget = camera->target();
  installPanHandler(graph);
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
  if (zlabel) {
    graph->axisY()->setTitle(QString::fromUtf8(zlabel));
    graph->axisY()->setTitleVisible(true);
  }
  if (hideGrid) {
    theme->setGridEnabled(false);
  }
  if (hideAxes || hideXAxisScale) {
    graph->axisX()->setTitleVisible(false);
    graph->axisX()->setLabelFormat("");
  }
  if (hideAxes || hideYAxisScale) {
    graph->axisZ()->setTitleVisible(false);
    graph->axisZ()->setLabelFormat("");
  }
  if (hideAxes || hideZAxisScale) {
    graph->axisY()->setTitleVisible(false);
    graph->axisY()->setLabelFormat("");
  }
  QFile dataFile(filename);
  if (!dataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    fprintf(stderr, "Unable to open %s\n", filename);
    return NULL;
  }
  QTextStream in(&dataFile);
  QT_DATAVIS_NAMESPACE::QScatterDataArray *dataArray = new QT_DATAVIS_NAMESPACE::QScatterDataArray;
  long long totalPoints = 0;
  double xmin = DBL_MAX, xmax = -DBL_MAX;
  double ymin = DBL_MAX, ymax = -DBL_MAX;
  double zmin = DBL_MAX, zmax = -DBL_MAX;
  // Optional 4th column (intensity) is used to color via series binning.
  bool hasIntensity = false;
  double imin = DBL_MAX, imax = -DBL_MAX;
  bool outsideDefaultIntensityRange = false;
  const double defaultImin = 0.0, defaultImax = 100.0;
  QVector<double> intensities;
  QVector<QVector3D> points;
  QString firstLine = in.readLine();
  QStringList header = firstLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  if (header.size() == 1) {
    int n = header[0].toInt();
    dataArray->reserve(n);
    intensities.reserve(n);
    points.reserve(n);
    for (int i = 0; i < n; i++) {
      QString line = in.readLine();
      while (line.trimmed().isEmpty() && !in.atEnd())
        line = in.readLine();
      QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
      if (parts.size() < 3)
        continue;
      double x = parts[0].toDouble();
      double y = parts[1].toDouble();
      double z = parts[2].toDouble();
      double xval = xLog ? pow(10.0, x) : x;
      if (x < xmin) xmin = x;
      if (x > xmax) xmax = x;
      if (y < ymin) ymin = y;
      if (y > ymax) ymax = y;
      if (z < zmin) zmin = z;
      if (z > zmax) zmax = z;
      QVector3D vec(xval, z, y);
      points.append(vec);
      QT_DATAVIS_NAMESPACE::QScatterDataItem item(vec);
      dataArray->append(item);
      if (parts.size() >= 4) {
        double inten = parts[3].toDouble();
        intensities.append(inten);
        hasIntensity = true;
        if (inten < imin) imin = inten;
        if (inten > imax) imax = inten;
        if (inten < defaultImin || inten > defaultImax)
          outsideDefaultIntensityRange = true;
      } else {
        intensities.append(qQNaN());
      }
    }
  } else {
    int nx = header[0].toInt();
    int ny = header[1].toInt();
    xmin = header[2].toDouble();
    xmax = header[3].toDouble();
    ymin = header[4].toDouble();
    ymax = header[5].toDouble();
    double dx = nx > 1 ? (xmax - xmin) / (nx - 1) : 1;
    double dy = ny > 1 ? (ymax - ymin) / (ny - 1) : 1;
    totalPoints = static_cast<long long>(nx) * static_cast<long long>(ny);
    dataArray->resize(static_cast<int>(totalPoints));
    QVector<double> xPositions(nx);
    QVector<double> yPositions(ny);
    for (int i = 0; i < nx; i++)
      xPositions[i] = xLog ? pow(10.0, xmin + i * dx) : (xmin + i * dx);
    for (int j = 0; j < ny; j++)
      yPositions[j] = ymin + j * dy;
    int index = 0;
    bool readFailed = false;
    QT_DATAVIS_NAMESPACE::QScatterDataItem *itemPtr = dataArray->data();
    for (int j = 0; j < ny && !readFailed; j++) {
      for (int i = 0; i < nx; i++) {
        double z;
        in >> z;
        if (in.status() != QTextStream::Ok) {
          readFailed = true;
          break;
        }
        if (z < zmin)
          zmin = z;
        if (z > zmax)
          zmax = z;
        itemPtr->setPosition(QVector3D(xPositions[i], z, yPositions[j]));
        itemPtr++;
        index++;
      }
    }
    if (readFailed && index < totalPoints) {
      dataArray->resize(index);
      totalPoints = index;
    }
  }
  if (!totalPoints)
    totalPoints = dataArray->size();
  graph->axisZ()->setRange(ymin, ymax);
  if (yTime) {
    Time3DAxisFormatter *formatter = new Time3DAxisFormatter;
    graph->axisZ()->setFormatter(formatter);
  }
  if (yFlip)
    graph->axisZ()->setReversed(true);
  if (xLog) {
    double xminLinear = pow(10.0, xmin);
    double xmaxLinear = pow(10.0, xmax);
    QT_DATAVIS_NAMESPACE::QLogValue3DAxisFormatter *formatter =
        new QT_DATAVIS_NAMESPACE::QLogValue3DAxisFormatter;
    graph->axisX()->setFormatter(formatter);
    graph->axisX()->setRange(xminLinear, xmaxLinear);
  } else {
    graph->axisX()->setRange(xmin, xmax);
  }
  if (xTime) {
    Time3DAxisFormatter *formatter = new Time3DAxisFormatter;
    graph->axisX()->setFormatter(formatter);
  }
  applyLargeDatasetGraphSettings(graph, totalPoints);
  bool largeDataset = totalPoints > kLargeDatasetThreshold;
  bool enableItemLabels = totalPoints <= kAggressiveOptimizationThreshold;
  if (hasIntensity && points.size() == intensities.size()) {
    double lo = defaultImin, hi = defaultImax;
    if (outsideDefaultIntensityRange) {
      lo = imin;
      hi = imax;
      if (hi == lo)
        hi = lo + 1.0;
    }
    if (!gray) {
      if (!spectrumallocated)
        allocspectrum();
    }
    const int bins = qMin(nspect, 64);
    QVector<QT_DATAVIS_NAMESPACE::QScatterDataArray *> binArrays(bins);
    int reservePerBin = largeDataset ? std::max(1, static_cast<int>(totalPoints / bins)) : 0;
    for (int b = 0; b < bins; b++) {
      binArrays[b] = new QT_DATAVIS_NAMESPACE::QScatterDataArray;
      if (reservePerBin)
        binArrays[b]->reserve(reservePerBin);
    }
    for (int i = 0; i < points.size(); i++) {
      double inten = intensities[i];
      if (std::isnan(inten))
        continue;
      double frac = (inten - lo) / (hi - lo);
      if (frac < 0)
        frac = 0;
      if (frac > 1)
        frac = 1;
      int bin = (int)floor(frac * (bins - 1) + 0.5);
      binArrays[bin]->append(QT_DATAVIS_NAMESPACE::QScatterDataItem(points[i]));
    }
    for (int b = 0; b < bins; b++) {
      if (binArrays[b]->size() == 0) {
        delete binArrays[b];
        continue;
      }
      QT_DATAVIS_NAMESPACE::QScatterDataProxy *p = new QT_DATAVIS_NAMESPACE::QScatterDataProxy;
      p->resetArray(binArrays[b]);
      QT_DATAVIS_NAMESPACE::QScatter3DSeries *s = new QT_DATAVIS_NAMESPACE::QScatter3DSeries(p);
      double frac = (double)b / (bins - 1);
      QColor c;
      if (gray)
        c = QColor::fromRgbF(frac, frac, frac);
      else {
        int index = (int)((hue0 + frac * (hue1 - hue0)) * (nspect - 1) + 0.5);
        if (index < 0)
          index = 0;
        if (index >= nspect)
          index = nspect - 1;
        c = QColor::fromRgb(spectrum[index]);
      }
      s->setBaseColor(c);
      if (largeDataset) {
        s->setMesh(QT_DATAVIS_NAMESPACE::QAbstract3DSeries::MeshPoint);
        s->setMeshSmooth(false);
        s->setItemSize(0.02f);
      } else {
        s->setMesh(QT_DATAVIS_NAMESPACE::QAbstract3DSeries::MeshSphere);
        s->setMeshSmooth(true);
        s->setItemSize(0.08f);
      }
      if (enableItemLabels)
        s->setItemLabelFormat(QStringLiteral("(@xLabel, @zLabel, @yLabel)"));
      else
        s->setItemLabelFormat(QString());
      graph->addSeries(s);
    }
  } else {
    QT_DATAVIS_NAMESPACE::QScatterDataProxy *proxy = new QT_DATAVIS_NAMESPACE::QScatterDataProxy;
    proxy->resetArray(dataArray);
    if (shadeRangeSet) {
      zmin = shadeMin;
      zmax = shadeMax;
    }
    QT_DATAVIS_NAMESPACE::QScatter3DSeries *series = new QT_DATAVIS_NAMESPACE::QScatter3DSeries(proxy);
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
    series->setColorStyle(QT_DATAVIS_NAMESPACE::Q3DTheme::ColorStyleRangeGradient);
    if (largeDataset) {
      series->setMesh(QT_DATAVIS_NAMESPACE::QAbstract3DSeries::MeshPoint);
      series->setMeshSmooth(false);
      series->setItemSize(0.02f);
    } else {
      series->setMesh(QT_DATAVIS_NAMESPACE::QAbstract3DSeries::MeshSphere);
      series->setMeshSmooth(true);
      // Make scatter points small spheres (barely larger than a point)
      // This only affects 3D scatter plots invoked by sddsplot.
      // Default item size is larger; reduce to improve readability in dense clouds.
      series->setItemSize(0.08f);
    }
    if (enableItemLabels)
      series->setItemLabelFormat(QStringLiteral("(@xLabel, @zLabel, @yLabel)"));
    else
      series->setItemLabelFormat(QString());
    graph->addSeries(series);
  }
  graph->axisY()->setRange(zmin, zmax);
  QShortcut *resetView = new QShortcut(QKeySequence(QStringLiteral("i")), widget);
  QObject::connect(resetView, &QShortcut::activated,
                   [camera, defaultX, defaultY, defaultZoom, defaultTarget]() {
                     camera->setXRotation(defaultX);
                     camera->setYRotation(defaultY);
                     camera->setZoomLevel(defaultZoom);
                     camera->setTarget(defaultTarget);
                   });
  QShortcut *snapX = new QShortcut(QKeySequence(QStringLiteral("x")), widget);
  QObject::connect(snapX, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetRight); });
  QShortcut *snapY = new QShortcut(QKeySequence(QStringLiteral("y")), widget);
  QObject::connect(snapY, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetFront); });
  QShortcut *snapZ = new QShortcut(QKeySequence(QStringLiteral("z")), widget);
  QObject::connect(snapZ, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetDirectlyAbove); });
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
  return widget;
}

static QWidget *run3d(const char *filename, const char *xlabel,
                      const char *ylabel, const char *zlabel,
                      const char *title, const char *topline, int mode,
                      bool datestamp, int fontSize, bool equalAspect,
                      double shadeMin, double shadeMax, bool shadeRangeSet,
                      bool gray, double hue0, double hue1, bool yFlip,
                      bool hideAxes, bool hideGrid, bool hideXAxisScale,
                      bool hideYAxisScale, bool hideZAxisScale, bool xLog,
                      bool xTime, bool yTime) {
  if (mode == PLOT3D_BAR)
    return run3dBar(filename, xlabel, ylabel, zlabel, title, topline,
                    datestamp, fontSize, equalAspect, shadeMin, shadeMax,
                    shadeRangeSet, gray, hue0, hue1, yFlip, hideAxes,
                    hideGrid, hideXAxisScale, hideYAxisScale, hideZAxisScale,
                    xLog, xTime, yTime);
  if (mode == PLOT3D_SCATTER)
    return run3dScatter(filename, xlabel, ylabel, zlabel, title, topline,
                        datestamp, fontSize, equalAspect, shadeMin,
                        shadeMax, shadeRangeSet, gray, hue0, hue1, yFlip,
                        hideAxes, hideGrid, hideXAxisScale, hideYAxisScale,
                        hideZAxisScale, xLog, xTime, yTime);
  QT_DATAVIS_NAMESPACE::Q3DSurface *graph = new QT_DATAVIS_NAMESPACE::Q3DSurface();
  surfaceGraph = graph;
  if (equalAspect) {
    graph->setHorizontalAspectRatio(1.0f);
    graph->setAspectRatio(1.0f);
  }
  QT_DATAVIS_NAMESPACE::Q3DTheme *theme = graph->activeTheme();
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
  QT_DATAVIS_NAMESPACE::Q3DCamera *camera = graph->scene()->activeCamera();
  camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetIsometricRightHigh); // Default orientation
  float defaultX = camera->xRotation();
  float defaultY = camera->yRotation();
  float defaultZoom = camera->zoomLevel();
  QVector3D defaultTarget = camera->target();
  installPanHandler(graph);
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
  if (zlabel) {
    graph->axisY()->setTitle(QString::fromUtf8(zlabel));
    graph->axisY()->setTitleVisible(true);
  }
  if (hideGrid) {
    theme->setGridEnabled(false);
  }
  if (hideAxes || hideXAxisScale) {
    graph->axisX()->setTitleVisible(false);
    graph->axisX()->setLabelFormat("");
  }
  if (hideAxes || hideYAxisScale) {
    graph->axisZ()->setTitleVisible(false);
    graph->axisZ()->setLabelFormat("");
  }
  if (hideAxes || hideZAxisScale) {
    graph->axisY()->setTitleVisible(false);
    graph->axisY()->setLabelFormat("");
  }

  Plot3DGridData gridData;
  QString loadError;
  if (!loadGridData(filename, gridData, &loadError)) {
    fprintf(stderr, "%s\n", loadError.toLocal8Bit().constData());
    return NULL;
  }

  int nx = gridData.nx;
  int ny = gridData.ny;
  double xmin = gridData.xmin;
  double xmax = gridData.xmax;
  double ymin = gridData.ymin;
  double ymax = gridData.ymax;

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
    QT_DATAVIS_NAMESPACE::QLogValue3DAxisFormatter *formatter =
        new QT_DATAVIS_NAMESPACE::QLogValue3DAxisFormatter;
    graph->axisX()->setFormatter(formatter);
    graph->axisX()->setRange(xminLinear, xmaxLinear);
  } else {
    graph->axisX()->setRange(xmin, xmax);
  }
  if (xTime) {
    Time3DAxisFormatter *formatter = new Time3DAxisFormatter;
    graph->axisX()->setFormatter(formatter);
  }

  QT_DATAVIS_NAMESPACE::QSurfaceDataArray *dataArray =
      new QT_DATAVIS_NAMESPACE::QSurfaceDataArray;
  dataArray->reserve(ny);
  double zmin = DBL_MAX, zmax = -DBL_MAX;
  const double *gridPtr = gridData.values.constData();
  for (int j = 0; j < ny; j++) {
    QT_DATAVIS_NAMESPACE::QSurfaceDataRow *row =
        new QT_DATAVIS_NAMESPACE::QSurfaceDataRow(nx);
    const double *rowValues = gridPtr + j * nx;
    double yval = ymin + j * dy;
    for (int i = 0; i < nx; i++) {
      double z = rowValues[i];
      if (z < zmin)
        zmin = z;
      if (z > zmax)
        zmax = z;
      double xval = xLog ? pow(10.0, xmin + i * dx) : (xmin + i * dx);
      (*row)[i].setPosition(QVector3D(xval, z, yval));
    }
    dataArray->append(row);
  }
  QT_DATAVIS_NAMESPACE::QSurfaceDataProxy *proxy = new QT_DATAVIS_NAMESPACE::QSurfaceDataProxy;
  proxy->resetArray(dataArray);
  if (shadeRangeSet) {
    zmin = shadeMin;
    zmax = shadeMax;
  }
  QT_DATAVIS_NAMESPACE::QSurface3DSeries *series = new QT_DATAVIS_NAMESPACE::QSurface3DSeries(proxy);
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
  series->setColorStyle(QT_DATAVIS_NAMESPACE::Q3DTheme::ColorStyleRangeGradient);
  series->setDrawMode(QT_DATAVIS_NAMESPACE::QSurface3DSeries::DrawSurface);
  series->setMeshSmooth(true);
  series->setItemLabelFormat(QStringLiteral("(@xLabel, @zLabel, @yLabel)"));
  graph->axisY()->setRange(zmin, zmax);
  QShortcut *toggleLines = new QShortcut(QKeySequence(QStringLiteral("g")), widget);
  QObject::connect(toggleLines, &QShortcut::activated,
                   [series]() {
                     static int mode = 0;
                     mode = (mode + 1) % 3;
                     switch (mode) {
                     case 0:
                       series->setDrawMode(QT_DATAVIS_NAMESPACE::QSurface3DSeries::DrawSurface);
                       break;
                     case 1:
                       series->setDrawMode(QT_DATAVIS_NAMESPACE::QSurface3DSeries::DrawSurfaceAndWireframe);
                       break;
                     case 2:
                       series->setDrawMode(QT_DATAVIS_NAMESPACE::QSurface3DSeries::DrawWireframe);
                       break;
                     }
                   });
  QShortcut *resetView = new QShortcut(QKeySequence(QStringLiteral("i")), widget);
  QObject::connect(resetView, &QShortcut::activated,
                   [camera, defaultX, defaultY, defaultZoom, defaultTarget]() {
                     camera->setXRotation(defaultX);
                     camera->setYRotation(defaultY);
                     camera->setZoomLevel(defaultZoom);
                     camera->setTarget(defaultTarget);
                   });
  QShortcut *snapX = new QShortcut(QKeySequence(QStringLiteral("x")), widget);
  QObject::connect(snapX, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetRight); });
  QShortcut *snapY = new QShortcut(QKeySequence(QStringLiteral("y")), widget);
  QObject::connect(snapY, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetFront); });
  QShortcut *snapZ = new QShortcut(QKeySequence(QStringLiteral("z")), widget);
  QObject::connect(snapZ, &QShortcut::activated,
                   [camera]() { camera->setCameraPreset(QT_DATAVIS_NAMESPACE::Q3DCamera::CameraPresetDirectlyAbove); });
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
  QImage renderPlotToImage(const QSize &targetSize) {
    if (targetSize.isEmpty())
      return QImage();

    QImage image(targetSize, QImage::Format_ARGB32);
    image.fill(QColor::fromRgb(colors[0]));

    if (!cur)
      return image;

    QPainter bufferPainter(&image);
    bufferPainter.setPen(QColor::fromRgb(white));
    drawPlotToPainter(bufferPainter, targetSize, false);
    bufferPainter.end();
    return image;
  }
  bool renderPlotToPainter(QPainter &painter, const QSize &targetSize) {
    if (targetSize.isEmpty())
      return false;
    drawPlotToPainter(painter, targetSize, false);
    return true;
  }
  void setRelativeAnchorFromGlobal(const QPoint &globalPos) {
    QPoint localPos = mapFromGlobal(globalPos);
    if (!rect().contains(localPos)) {
      m_hasRelativeAnchor = false;
      return;
    }
    double userX = Xvalue(localPos.x());
    double userY = Yvalue(localPos.y());
    m_anchorX = MTRACKX(userX);
    m_anchorY = MTRACKY(userY);
    m_hasRelativeAnchor = true;
  }
  void clearRelativeAnchor() {
    m_hasRelativeAnchor = false;
    m_anchorX = 0.0;
    m_anchorY = 0.0;
  }
  bool hasRelativeAnchor() const {
    return m_hasRelativeAnchor;
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
            double displayX = MTRACKX(Xvalue(pos.x()));
            double displayY = MTRACKY(Yvalue(pos.y()));
            QString tipText = QString("x: %1, y: %2")
                .arg(displayX, 0, 'g', 10)
                .arg(displayY, 0, 'g', 10);
            if (m_hasRelativeAnchor) {
                double relX = displayX - m_anchorX;
                double relY = displayY - m_anchorY;
                tipText += QString("\nxrel: %1, yrel: %2")
                               .arg(relX, 0, 'g', 10)
                               .arg(relY, 0, 'g', 10);
            }
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
    drawPlotToPainter(bufferPainter, m_buffer.size(), true);

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
  bool m_resizing = false;
  QPixmap m_buffer;
  QTimer *m_resizeTimer;
  bool m_hasRelativeAnchor = false;
  double m_anchorX = 0.0;
  double m_anchorY = 0.0;

  void drawPlotToPainter(QPainter &bufferPainter, const QSize &targetSize, bool updateState) {
    if (!cur)
      return;

    VTYPE x, y, lt, lt2;
    char *bufptr, command;
    unsigned short r, g, b;

    int savedW = W;
    int savedH = H;
    double savedScalex = scalex;
    double savedScaley = scaley;
    double savedUserx0 = userx0;
    double savedUserx1 = userx1;
    double savedUsery0 = usery0;
    double savedUsery1 = usery1;
    double savedUserax = userax;
    double savedUserbx = userbx;
    double savedUseray = useray;
    double savedUserby = userby;
    VTYPE savedCx = cx;
    VTYPE savedCy = cy;
    int savedCurrentlinewidth = currentlinewidth;
    COLOR_REF savedCurrentcolor = currentcolor;
    int savedLinecolormax = linecolormax;
    int savedUseDashes = UseDashes;
    int savedNspect = nspect;
    int savedSpectral = spectral;
    int savedCustomspectral = customspectral;
    unsigned short savedRed0 = red0;
    unsigned short savedGreen0 = green0;
    unsigned short savedBlue0 = blue0;
    unsigned short savedRed1 = red1;
    unsigned short savedGreen1 = green1;
    unsigned short savedBlue1 = blue1;
    int savedSpectrumallocated = spectrumallocated;
    struct COORDREC *savedCurcoord = curcoord;
    struct COORDREC *savedLastcoord = lastcoord;
    struct COORDREC *savedUsecoord = usecoord;
    int savedNcoords = ncoords;

    if (updateState)
      destroycoordrecs();

    if (fabs(userx1 - userx0) < 1e-12)
      userx1 = userx0 + 1;
    if (fabs(usery1 - usery0) < 1e-12)
      usery1 = usery0 + 1;

    int targetW = std::max(1, targetSize.width());
    int targetH = std::max(1, targetSize.height());

    W = targetW;
    H = targetH;
    scalex = (W - 1.0) / (userx1 - userx0);
    scaley = (H - 1.0) / (usery1 - usery0);

    bufptr = cur->buffer;
    for (int n = 0; n < cur->nc;) {
      switch (command = *bufptr++) {
      case 'V':
        memcpy((char *)&x, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&y, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 2 * sizeof(VTYPE);
        bufferPainter.drawLine(Xpixel(cx), Ypixel(cy), Xpixel(x), Ypixel(y));
        cx = x;
        cy = y;
        break;
      case 'M':
        memcpy((char *)&cx, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&cy, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 2 * sizeof(VTYPE);
        break;
      case 'P':
        memcpy((char *)&cx, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        memcpy((char *)&cy, bufptr, sizeof(VTYPE));
        bufptr += sizeof(VTYPE);
        n += sizeof(char) + 2 * sizeof(VTYPE);
        bufferPainter.drawLine(Xpixel(cx), Ypixel(cy), Xpixel(cx), Ypixel(cy));
        cx++;
        cy++;
        break;
      case 'L':
        {
          QPen pen = bufferPainter.pen();
          pen.setStyle(Qt::SolidLine);
          memcpy((char *)&lt2, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          n += sizeof(char) + sizeof(VTYPE);
          if (!lineTypeTable.nEntries) {
            lt = lt2 % 16;
            lt += 2;
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
        }
        break;
      case 'W':
        {
          memcpy((char *)&lt, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          n += sizeof(char) + sizeof(VTYPE);
          currentlinewidth = lt;
          QPen pen = bufferPainter.pen();
          pen.setWidth(currentlinewidth);
          bufferPainter.setPen(pen);
        }
        break;
      case 'B':
        {
          VTYPE shade, xl, xh, yl, yh;
          int px, py, width, height;
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
          px = Xpixel(xl);
          py = Ypixel(yl);
          width = Xpixel(xh) - px;
          height = Ypixel(yh) - py;
          bufferPainter.fillRect(QRect(px, py, width, height), spectrum[shade]);
        }
        break;
      case 'U':
        memcpy((char *)&userax, bufptr, sizeof(double));
        bufptr += sizeof(double);
        memcpy((char *)&userbx, bufptr, sizeof(double));
        bufptr += sizeof(double);
        memcpy((char *)&useray, bufptr, sizeof(double));
        bufptr += sizeof(double);
        memcpy((char *)&userby, bufptr, sizeof(double));
        bufptr += sizeof(double);
        n += sizeof(char) + 4 * sizeof(double);
        if (updateState) {
          curcoord = makecoordrec();
          if (lastcoord)
            lastcoord->next = curcoord;
          lastcoord = curcoord;
          curcoord->x0 = userax;
          curcoord->x1 = userbx;
          curcoord->y0 = useray;
          curcoord->y1 = userby;
        }
        break;
      case 'G':
      case 'R':
      case 'E':
        n += sizeof(char);
        break;
      case 'C':
        {
          memcpy((char *)&r, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          memcpy((char *)&g, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          memcpy((char *)&b, bufptr, sizeof(VTYPE));
          bufptr += sizeof(VTYPE);
          n += sizeof(char) + 3 * sizeof(VTYPE);
          QPen pen = bufferPainter.pen();
          currentcolor = RGB_QT(std::round((255.0 / 65536.0) * r),
                               std::round((255.0 / 65536.0) * g),
                               std::round((255.0 / 65536.0) * b));
          pen.setColor(currentcolor);
          bufferPainter.setPen(pen);
        }
        break;
      case 'S':
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
        }
        break;
      default:
        break;
      }
    }

    if (updateState) {
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
    } else {
      W = savedW;
      H = savedH;
      scalex = savedScalex;
      scaley = savedScaley;
      userx0 = savedUserx0;
      userx1 = savedUserx1;
      usery0 = savedUsery0;
      usery1 = savedUsery1;
      userax = savedUserax;
      userbx = savedUserbx;
      useray = savedUseray;
      userby = savedUserby;
      cx = savedCx;
      cy = savedCy;
      currentlinewidth = savedCurrentlinewidth;
      currentcolor = savedCurrentcolor;
      linecolormax = savedLinecolormax;
      UseDashes = savedUseDashes;
      nspect = savedNspect;
      spectral = savedSpectral;
      customspectral = savedCustomspectral;
      red0 = savedRed0;
      green0 = savedGreen0;
      blue0 = savedBlue0;
      red1 = savedRed1;
      green1 = savedGreen1;
      blue1 = savedBlue1;
      spectrumallocated = savedSpectrumallocated;
      curcoord = savedCurcoord;
      lastcoord = savedLastcoord;
      usecoord = savedUsecoord;
      ncoords = savedNcoords;
    }
  }
};

QImage exportCurrentPlotImage(const QSize &targetSize) {
  if (!canvas)
    return QImage();
  Canvas *plotCanvas = qobject_cast<Canvas *>(canvas);
  if (!plotCanvas)
    return QImage();
  return plotCanvas->renderPlotToImage(targetSize);
}

bool exportCurrentPlotToPainter(QPainter &painter, const QSize &targetSize) {
  if (!canvas)
    return false;
  Canvas *plotCanvas = qobject_cast<Canvas *>(canvas);
  if (!plotCanvas)
    return false;
  return plotCanvas->renderPlotToPainter(painter, targetSize);
}

void captureRelativeMouseAnchor() {
  if (!canvas)
    return;
  Canvas *plotCanvas = qobject_cast<Canvas *>(canvas);
  if (!plotCanvas)
    return;
  plotCanvas->setRelativeAnchorFromGlobal(QCursor::pos());
}

void clearRelativeMouseAnchor() {
  if (!canvas)
    return;
  Canvas *plotCanvas = qobject_cast<Canvas *>(canvas);
  if (!plotCanvas)
    return;
  plotCanvas->clearRelativeAnchor();
}

bool hasRelativeMouseAnchor() {
  if (!canvas)
    return false;
  Canvas *plotCanvas = qobject_cast<Canvas *>(canvas);
  if (!plotCanvas)
    return false;
  return plotCanvas->hasRelativeAnchor();
}

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
z - toggle replotting to zoom (2D)\n\
+ - increase window size\n\
- - decrease window size\n\
\n\
Mouse controls for 2D plots:\n\
Left mouse drag - draw zoom rectangle\n\
Middle mouse click - zoom to selection\n\
Right mouse click - reset zoom to full extent\n\
\n\
Keyboard shortcuts for 3D plots:\n\
g - cycle surface/wireframe modes\n\
i - reset view\n\
x - snap view to X axis\n\
y - snap view to Y axis\n\
z - snap view to Z axis\n\
Up arrow - increase font size\n\
Down arrow - decrease font size\n\
\n\
Mouse controls for 3D plots:\n\
Left mouse drag - rotate view\n\
Right mouse drag - zoom view\n\
Mouse wheel - zoom view\n\
Ctrl + Right mouse drag - pan camera\n\
Ctrl + Mouse wheel (scatter plots) - adjust point size\n\
\n\
Other keyboard shortcuts:\n\
w - toggle white/black theme\n\
. - toggle mouse tracking\n\
: - set mouse tracker origin\n\
Ctrl + r - replot current data\n\
q - quit");
        layout->addWidget(textEdit);
        setLayout(layout);
    }
};

/**
 * @brief Custom Qt message handler to suppress XCB warning messages.
 *
 * Filters out "qt.qpa.xcb: QXcbConnection: XCB error" messages that occur
 * during normal operation and are not indicative of actual problems.
 *
 * @param type The message type.
 * @param context The message log context.
 * @param msg The message string.
 */
static void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
  if (msg.contains("QXcbConnection") && msg.contains("XCB error"))
    return;
  QByteArray localMsg = msg.toLocal8Bit();
  switch (type) {
  case QtDebugMsg:
    fprintf(stderr, "Debug: %s\n", localMsg.constData());
    break;
  case QtInfoMsg:
    fprintf(stderr, "Info: %s\n", localMsg.constData());
    break;
  case QtWarningMsg:
    fprintf(stderr, "Warning: %s\n", localMsg.constData());
    break;
  case QtCriticalMsg:
    fprintf(stderr, "Critical: %s\n", localMsg.constData());
    break;
  case QtFatalMsg:
    fprintf(stderr, "Fatal: %s\n", localMsg.constData());
    abort();
  }
}

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
  qInstallMessageHandler(customMessageHandler);
  lineTypeTable.nEntries = 0;
  lineTypeTable.typeFlag = 0x0000;

  QApplication app(argc, argv);
  QVector<Plot3DArgs> plots;
  Plot3DArgs current;
  bool in3d = false;
  for (int i = 1; i < argc; i++) {
    if (in3d) {
      const char *tickArg = nullptr;
      int savedIndex = i;
      if (parseTicksettingsArgument(argc, argv, i, &tickArg)) {
        if (tickArg) {
          if (strstr(tickArg, "xtime"))
            current.xTime = true;
          if (strstr(tickArg, "ytime"))
            current.yTime = true;
        }
        continue;
      }
      i = savedIndex;
    }
    if (!strncmp(argv[i], "-3d", 3) && i + 1 < argc) {
      if (in3d)
        plots.append(current);
      current = Plot3DArgs();
      if (argv[i][3] == '=' && argv[i][4]) {
        if (!strcmp(argv[i] + 4, "bar"))
          current.mode = PLOT3D_BAR;
        else if (!strcmp(argv[i] + 4, "scatter"))
          current.mode = PLOT3D_SCATTER;
      }
      current.file = argv[++i];
      in3d = true;
    } else if (!strcmp(argv[i], "-xlabel") && i + 1 < argc && in3d)
      current.xlabel = QString::fromUtf8(argv[++i]);
    else if (!strncmp(argv[i], "-xlabel=", 8) && in3d)
      current.xlabel = QString::fromUtf8(argv[i] + 8);
    else if (!strcmp(argv[i], "-ylabel") && i + 1 < argc && in3d)
      current.ylabel = QString::fromUtf8(argv[++i]);
    else if (!strncmp(argv[i], "-ylabel=", 8) && in3d)
      current.ylabel = QString::fromUtf8(argv[i] + 8);
    else if (!strcmp(argv[i], "-zlabel") && i + 1 < argc && in3d)
      current.zlabel = QString::fromUtf8(argv[++i]);
    else if (!strncmp(argv[i], "-zlabel=", 8) && in3d)
      current.zlabel = QString::fromUtf8(argv[i] + 8);
    else if (!strcmp(argv[i], "-plottitle") && i + 1 < argc && in3d)
      current.title = QString::fromUtf8(argv[++i]);
    else if (!strncmp(argv[i], "-plottitle=", 11) && in3d)
      current.title = QString::fromUtf8(argv[i] + 11);
    else if (!strcmp(argv[i], "-topline") && i + 1 < argc && in3d)
      current.topline = QString::fromUtf8(argv[++i]);
    else if (!strncmp(argv[i], "-topline=", 9) && in3d)
      current.topline = QString::fromUtf8(argv[i] + 9);
    else if (!strcmp(argv[i], "-fontsize") && i + 1 < argc && in3d)
      current.fontSize = atoi(argv[++i]);
    else if (isEqualAspectArgument(argv[i]) && in3d)
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
      current.hideGrid = true;
    else if (!strcmp(argv[i], "-noscale") && in3d) {
      current.hideAxes = true;
      current.hideXAxisScale = true;
      current.hideYAxisScale = true;
      current.hideZAxisScale = true;
    } else if (in3d && !strncmp(argv[i], "-noscales", 9)) {
      const char *arg = NULL;
      if (argv[i][9] == '=')
        arg = argv[i] + 10;
      else if (!argv[i][9] && i + 1 < argc && argv[i + 1][0] != '-')
        arg = argv[++i];
      if (!arg || !arg[0]) {
        current.hideAxes = true;
        current.hideXAxisScale = true;
        current.hideYAxisScale = true;
        current.hideZAxisScale = true;
      } else {
        bool recognized = false;
        bool warned = false;
        for (const char *p = arg; *p; ++p) {
          unsigned char uc = static_cast<unsigned char>(*p);
          if (uc == ',' || isspace(uc))
            continue;
          switch (tolower(uc)) {
            case 'x':
              current.hideXAxisScale = true;
              recognized = true;
              break;
            case 'y':
              current.hideYAxisScale = true;
              recognized = true;
              break;
            case 'z':
              current.hideZAxisScale = true;
              recognized = true;
              break;
            default:
              if (!warned) {
                fprintf(stderr,
                        "mpl_qt: ignoring unrecognized axis specifier '%c' for -noscales.\n",
                        *p);
                warned = true;
              }
              break;
          }
        }
        if (current.hideXAxisScale && current.hideYAxisScale &&
            current.hideZAxisScale)
          current.hideAxes = true;
        if (!recognized && !warned) {
          current.hideAxes = true;
          current.hideXAxisScale = true;
          current.hideYAxisScale = true;
          current.hideZAxisScale = true;
        }
      }
    } else if (!strcmp(argv[i], "-datestamp") && in3d)
      current.datestamp = true;
    else if (!strcmp(argv[i], "-xlog") && in3d)
      current.xLog = true;
    else if (in3d) {
      const char *tickArg = nullptr;
      if (parseTicksettingsArgument(argc, argv, i, &tickArg)) {
        if (tickArg) {
          if (strstr(tickArg, "xtime"))
            current.xTime = true;
          if (strstr(tickArg, "ytime"))
            current.yTime = true;
        }
      } else
        continue;
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
  QMenu *optionsMenu = nullptr;
  if (plots.isEmpty())
    optionsMenu = mainWindow.menuBar()->addMenu("Options");
  QMenu *helpMenu = mainWindow.menuBar()->addMenu("Help");

  QAction *printAction = new QAction("Print...", &mainWindow);
  fileMenu->addAction(printAction);
  QObject::connect(printAction, &QAction::triggered, &app, [](bool){ print(); });

  QAction *saveAction = new QAction("Save as PNG or JPEG...", &mainWindow);
  fileMenu->addAction(saveAction);
  QObject::connect(saveAction, &QAction::triggered, &app, [](bool){ save(); });

  QAction *savePdfEpsAction = new QAction("Save as PDF, PS or EPS...", &mainWindow);
  fileMenu->addAction(savePdfEpsAction);
  QObject::connect(savePdfEpsAction, &QAction::triggered, &app, [](bool){ savePdfOrEps(); });

  QAction *quitAction = new QAction("Quit (Q)", &mainWindow);
  fileMenu->addAction(quitAction);
  QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

  QAction *nextAction = new QAction("Next (N)", &mainWindow);
  navigateMenu->addAction(nextAction);
  QObject::connect(nextAction, &QAction::triggered, [&mainWindow]() {
    nav_next(&mainWindow);
  });

  QAction *previousAction = new QAction("Previous (P)", &mainWindow);
  navigateMenu->addAction(previousAction);
  QObject::connect(previousAction, &QAction::triggered, [&mainWindow]() {
    nav_previous(&mainWindow);
  });

  QAction *deleteAction = new QAction("Delete (D)", &mainWindow);
  navigateMenu->addAction(deleteAction);
  QObject::connect(deleteAction, &QAction::triggered, [&mainWindow]() {
    delete_current(&mainWindow);
  });

  QAction *toNumberAction = new QAction("To number...", &mainWindow);
  navigateMenu->addAction(toNumberAction);
  QObject::connect(toNumberAction, &QAction::triggered, [&mainWindow]() {
    to_number(&mainWindow);
  });

  if (optionsMenu) {
    replotZoomAction = new QAction("Replot when zooming (Z)", &mainWindow);
    replotZoomAction->setCheckable(true);
    replotZoomAction->setChecked(replotZoom);
    optionsMenu->addAction(replotZoomAction);
    QObject::connect(replotZoomAction, &QAction::toggled, &app, [&](bool checked){replotZoom = checked;});
    mouseTrackerAction = new QAction("Mouse Tracker (.) / Origin (:)", &mainWindow);
    mouseTrackerAction->setCheckable(true);
    mouseTrackerAction->setChecked(tracking);
    optionsMenu->addAction(mouseTrackerAction);
    QObject::connect(mouseTrackerAction, &QAction::toggled, &app,
                     [&](bool checked) {
                       tracking = checked;
                       if (!tracking)
                         clearRelativeMouseAnchor();
                     });
    QMenu *placementMenu = optionsMenu->addMenu("Placement/Size");
    QAction *topHalfAction = placementMenu->addAction("Top Half (T)");
    QObject::connect(topHalfAction, &QAction::triggered,
                     [&mainWindow]() { placeTopHalf(&mainWindow); });
    QAction *bottomHalfAction = placementMenu->addAction("Bottom Half (B)");
    QObject::connect(bottomHalfAction, &QAction::triggered,
                     [&mainWindow]() { placeBottomHalf(&mainWindow); });
    QAction *leftHalfAction = placementMenu->addAction("Left Half (L)");
    QObject::connect(leftHalfAction, &QAction::triggered,
                     [&mainWindow]() { placeLeftHalf(&mainWindow); });
    QAction *rightHalfAction = placementMenu->addAction("Right Half (R)");
    QObject::connect(rightHalfAction, &QAction::triggered,
                     [&mainWindow]() { placeRightHalf(&mainWindow); });
    QAction *centerAction = placementMenu->addAction("Center (C)");
    QObject::connect(centerAction, &QAction::triggered,
                     [&mainWindow]() { placeCenter(&mainWindow); });
    placementMenu->addSeparator();
    QAction *topLeftAction = placementMenu->addAction("Top-Left Quadrant (1)");
    QObject::connect(topLeftAction, &QAction::triggered,
                     [&mainWindow]() { placeQuadrant(&mainWindow, 1); });
    QAction *topRightAction = placementMenu->addAction("Top-Right Quadrant (2)");
    QObject::connect(topRightAction, &QAction::triggered,
                     [&mainWindow]() { placeQuadrant(&mainWindow, 2); });
    QAction *bottomLeftAction = placementMenu->addAction("Bottom-Left Quadrant (3)");
    QObject::connect(bottomLeftAction, &QAction::triggered,
                     [&mainWindow]() { placeQuadrant(&mainWindow, 3); });
    QAction *bottomRightAction = placementMenu->addAction("Bottom-Right Quadrant (4)");
    QObject::connect(bottomRightAction, &QAction::triggered,
                     [&mainWindow]() { placeQuadrant(&mainWindow, 4); });
    placementMenu->addSeparator();
    QAction *originalSizeAction = placementMenu->addAction("Original Size (0)");
    QObject::connect(originalSizeAction, &QAction::triggered,
                     [&mainWindow]() { restoreOriginalSize(&mainWindow); });
    QAction *increaseSizeAction = placementMenu->addAction("Increase Size (+)");
    QObject::connect(increaseSizeAction, &QAction::triggered,
                     [&mainWindow]() { increaseWindowSize(&mainWindow); });
    QAction *decreaseSizeAction = placementMenu->addAction("Decrease Size (-)");
    QObject::connect(decreaseSizeAction, &QAction::triggered,
                     [&mainWindow]() { decreaseWindowSize(&mainWindow); });
    placementMenu->addSeparator();
    QAction *toggleFullScreenAction = placementMenu->addAction("Toggle Full Screen (F)");
    QObject::connect(toggleFullScreenAction, &QAction::triggered,
                     [&mainWindow]() { toggleFullScreen(&mainWindow); });
  }
  
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
      QWidget *plotWidget = run3d(
          p.file.toUtf8().constData(),
          p.xlabel.isEmpty() ? NULL : p.xlabel.toUtf8().constData(),
          p.ylabel.isEmpty() ? NULL : p.ylabel.toUtf8().constData(),
          p.zlabel.isEmpty() ? NULL : p.zlabel.toUtf8().constData(),
          p.title.isEmpty() ? NULL : p.title.toUtf8().constData(),
          p.topline.isEmpty() ? NULL : p.topline.toUtf8().constData(),
          p.mode, p.datestamp, p.fontSize, p.equalAspect, p.shadeMin,
          p.shadeMax, p.shadeRangeSet, p.gray, p.hue0, p.hue1, p.yFlip,
          p.hideAxes, p.hideGrid, p.hideXAxisScale, p.hideYAxisScale,
          p.hideZAxisScale, p.xLog, p.xTime, p.yTime);
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
    setup_shortcuts(&mainWindow, true);
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
          refreshCanvas();
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
