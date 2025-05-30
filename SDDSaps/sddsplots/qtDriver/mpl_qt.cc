/**
 * @file mpl_qt.cc
 * @brief MPL Outboard Driver for plotting using Qt.
 *
 * This file implements the main functions and classes required for the MPL outboard driver.
 */

#include "mpl_qt.h"

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
bool replotZoom = true;
bool tracking = false;
bool domovie = false;
double movieIntervalTime = 0.1;
extern "C" {
  FILE* outfile;
}
QAction *replotZoomAction;
QFrame *canvas;
QMainWindow *mainWindowPointer;

/**
 * @brief Reads from standard input and triggers plot updates.
 *
 * This class uses a QSocketNotifier to monitor stdin and calls the global
 * readdata() function when data is available.
 */
class StdinReader : public QObject {
  Q_OBJECT
public:
  StdinReader(QObject *parent = nullptr) : QObject(parent) {
    // Create a notifier for stdin (file descriptor 0)
    notifier = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &StdinReader::handleActivated);
  }
public slots:
  void handleActivated(int) {
    // Call the global readdata() when stdin is ready
    if (domovie)
      notifier->setEnabled(false);
    if (readdata() == 1) {
      notifier->setEnabled(false);
      QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
      mainWindowPointer->setWindowTitle(wtitle);
    } else if (domovie) {
      QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
      QTime dieTime= QTime::currentTime().addSecs(movieIntervalTime);
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
      case 's': /* -spectrum */
        if (!strcmp("spectrum", argv[i] + 1)) {
          if (!spectrumallocated)
            allocspectrum();
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

  // Add a large black frame (serves as a canvas for future graph)
  canvas = new Canvas;
  canvas->resize(WIDTH, HEIGHT);

  layout->addWidget(canvas);
  mainWindow.setCentralWidget(centralWidget);

  mainWindow.resize(WIDTH + 20, HEIGHT + 40);
  mainWindow.show();

  setup_shortcuts(&mainWindow);

  alloccolors();

  new StdinReader(&app);

  return app.exec();
}

#include "mpl_qt_moc.h"
