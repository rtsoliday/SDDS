#include "mpl_qt.h"
#include <QtDataVisualization/Q3DTheme>
#include <QColor>
#include <QLabel>
#include <QList>
#include <QPalette>

int allocspectrum() {
  int n, ok = 1, k;
  double hue;
  uint32_t red, green, blue;

  /* Allocate spectrum colors */
  for (n = 0; n < nspect; n++) {
    if ((spectral == 1) || (spectral == 3)) {
      if (spectral == 1)
        hue = 6 * n / (nspect - 1.0);
      if (spectral == 3)
        hue = 5 * n / (nspect - 1.0);
      if (hue < 1) {
        // red to orange
        red = (uint32_t)255;
        green = (uint32_t)(255.999 * (.65 * hue));
        blue = (uint32_t)0;
      } else if ((hue >= 1) && (hue < 2)) {
        // orange to yellow
        red = (uint32_t)255;
        green = (uint32_t)(255.999 * (.65 + .35 * (hue - 1)));
        blue = (uint32_t)0;
      } else if ((hue >= 2) && (hue < 2.3)) {
        // yellow to green (heavy yellow)
        red = (uint32_t)(255.999 * (1 - (.2 * (hue - 2) / .3)));
        green = (uint32_t)255;
        blue = (uint32_t)0;
      } else if (hue < 3) {
        // yellow to green (heavy green)
        red = (uint32_t)(255.999 * (.8 * (1 - (hue - 2.3) / .7)));
        green = (uint32_t)255;
        blue = (uint32_t)0;
      } else if ((hue >= 3) && (hue < 3.4)) {
        // green to cyan (heavy green)
        red = (uint32_t)0;
        green = (uint32_t)255;
        blue = (uint32_t)(255.999 * (.85 * ((hue - 3) / .4)));
      } else if (hue < 4) {
        // green to cyan (heavy cyan)
        red = (uint32_t)0;
        green = (uint32_t)255;
        blue = (uint32_t)(255.999 * (.85 + (.15 * ((hue - 3.4) / .6))));
      } else if (hue < 5) {
        // cyan to blue
        red = (uint32_t)0;
        green = (uint32_t)(255.999 * (1 - (hue - 4)));
        blue = (uint32_t)255;
      } else {
        // blue to violet
        red = (uint32_t)(255.999 * (hue - 5));
        green = (uint32_t)0;
        blue = (uint32_t)255;
      }
      spectrum[n] = RGB_QT(red, green, blue);
    } else if ((spectral == 2) || (spectral == 4)) {
      if (spectral == 2)
        hue = 6 * n / (nspect - 1.0);
      if (spectral == 4)
        hue = 1 + (5 * n / (nspect - 1.0));
      if (hue < 1) {
        // violet to blue
        red = (uint32_t)(255.999 * (1 - hue));
        green = (uint32_t)0;
        blue = (uint32_t)255;
      } else if (hue < 2) {
        // blue to cyan
        red = (uint32_t)0;
        green = (uint32_t)(255.999 * (hue - 1));
        blue = (uint32_t)255;
      } else if (hue < 2.4) {
        // cyan to green (heavy cyan)
        red = (uint32_t)0;
        green = (uint32_t)255;
        blue = (uint32_t)(255.999 * (1.0 - (.15 * ((hue - 2) / .4))));
      } else if (hue < 3) {
        // cyan to green (heavy green)
        red = (uint32_t)0;
        green = (uint32_t)255;
        blue = (uint32_t)(255.999 * (.85 * (1 - ((hue - 2.4) / .6))));
      } else if (hue < 3.7) {
        // green to yellow (heavy green)
        red = (uint32_t)(255.999 * (.80 * ((hue - 3) / .7)));
        green = (uint32_t)255;
        blue = (uint32_t)0;
      } else if (hue < 4) {
        // green to yellow (heavy yellow)
        red = (uint32_t)(255.999 * (.80 + (.20 * ((hue - 3.7) / .3))));
        green = (uint32_t)255;
        blue = (uint32_t)0;
      } else if (hue < 5) {
        // yellow to orange
        red = (uint32_t)255;
        green = (uint32_t)(255.999 * (1.0 - (.35 * ((hue - 4) / 1.0))));
        blue = (uint32_t)0;
      } else {
        // orange to red
        red = (uint32_t)255;
        green = (uint32_t)(255.999 * (.65 * (1 - ((hue - 5) / 1.0))));
        blue = (uint32_t)0;
      }
      spectrum[n] = RGB_QT(red, green, blue);
    } else if (customspectral) {
      hue = n / (nspect - 1.0);
      red = (uint32_t)((red0 + hue * (red1 - red0)) / 256.0);
      green = (uint32_t)((green0 + hue * (green1 - green0)) / 256.0);
      blue = (uint32_t)((blue0 + hue * (blue1 - blue0)) / 256.0);
      spectrum[n] = RGB_QT(red, green, blue);
    } else {
      k = 0;
      for (n = 0; n < nspect; n++) {
        if (k < 256) {
          spectrum[n] = RGB_QT((uint32_t)(0),
                            (uint32_t)(k),
                            (uint32_t)(255));
        } else if ((k >= 256) && (k < 512)) {
          spectrum[n] = RGB_QT((uint32_t)(0),
                            (uint32_t)(255),
                            (uint32_t)(511 - k));
        } else if ((k >= 512) && (k < 768)) {
          spectrum[n] = RGB_QT((uint32_t)(k - 512),
                            (uint32_t)(255),
                            (uint32_t)(0));
        } else if ((k >= 768) && (k < 1024)) {
          spectrum[n] = RGB_QT((uint32_t)(255),
                            (uint32_t)(1023 - k),
                            (uint32_t)(0));
        } else {
          spectrum[n] = RGB_QT((uint32_t)(255),
                            (uint32_t)(0),
                            (uint32_t)(k - 1024));
        }
        k += 1279.0 / (nspect - 1);
      }
    }
  }

  spectrumallocated = 1;
  return ok;
}

int alloccolors() {
  int n;
  /* Allocate black */
  black = RGB_QT(0, 0, 0);
  /* Allocate white */
  white = RGB_QT(255, 255, 255);
  /* Allocate option colors */

  colors[0] = black;
  colors[1] = colors[2] = white;
  colors[3] = RGB_QT(255, 0, 0);
  colors[4] = RGB_QT(0, 0, 255);
  colors[5] = RGB_QT(0, 255, 0);
  colors[6] = RGB_QT(255, 255, 0);
  colors[7] = RGB_QT(255, 0, 255);
  colors[8] = RGB_QT(0, 255, 255);
  colors[9] = RGB_QT(50, 205, 50);
  colors[10] = RGB_QT(255, 215, 0);
  colors[11] = RGB_QT(255, 165, 0);
  colors[12] = RGB_QT(255, 105, 180);
  colors[13] = RGB_QT(0, 191, 255);
  colors[14] = RGB_QT(0, 250, 154);
  colors[15] = RGB_QT(255, 99, 71);
  colors[16] = RGB_QT(210, 180, 140);
  colors[17] = RGB_QT(128, 128, 128);
  for (n = 0; n < NCOLORS; n++) {
    colors_orig[n] = colors[n];
    colorsalloc[n] = 1;
  }
  currentcolor = white;
  return 1;
}

void onBlack() {
  colors[0] = black;
  colors[1] = colors[2] = white;
  for (int n = 0; n < NCOLORS; n++) {
    colors_orig[n] = colors[n];
    colorsalloc[n] = 1;
  }
  currentcolor = white;
  canvas->update();
}

void onWhite() {
  colors[0] = white;
  colors[1] = colors[2] = black;
  for (int n = 0; n < NCOLORS; n++) {
    colors_orig[n] = colors[n];
    colorsalloc[n] = 1;
  }
  currentcolor = black;
  canvas->update();
}

struct COORDREC *makecoordrec() {
  struct COORDREC *coordrec;
  coordrec = (struct COORDREC *)calloc(1, sizeof(struct COORDREC));
  if (!coordrec) {
    fprintf(stderr, "Could not make a record for a new coordinate\n");
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
    free(last->buffer);
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
    free(rec->buffer);
  /* Free the structure */
  free(rec);
  if (rec == curwrite)
    curwrite = (struct PLOTREC *)NULL;
  rec = (struct PLOTREC *)NULL;
}

struct PLOTREC *makeplotrec()
/* Makes a new plot record, which is also the last */
{
  struct PLOTREC *plotrec, *rec;
  int nkept;

  plotrec = (struct PLOTREC *)calloc(1, sizeof(struct PLOTREC));
  if (plotrec) {
    nplots++;
    plotrec->nplot = nplots;
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

void quit() {
  exit(0);
}

void nav_next(QMainWindow *mainWindow) {
  if (plotStack) {
    if (current3DPlot >= total3DPlots - 1) {
      QApplication::beep();
    } else {
      current3DPlot++;
      plotStack->setCurrentIndex(current3DPlot);
      canvas = plotStack->currentWidget();
      surfaceGraph = surfaceGraphs[current3DPlot];
      surfaceContainer = surfaceContainers[current3DPlot];
      QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(current3DPlot + 1).arg(total3DPlots);
      mainWindow->setWindowTitle(wtitle);
    }
    return;
  }
  if (cur == (struct PLOTREC *)NULL) {
    QApplication::beep();
  } else if (cur == last) {
    QApplication::beep();
  } else {
    cur = cur->next;
    canvas->update();
    QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
    mainWindow->setWindowTitle(wtitle);
  }
}

void nav_previous(QMainWindow *mainWindow) {
  if (plotStack) {
    if (current3DPlot == 0) {
      QApplication::beep();
    } else {
      current3DPlot--;
      plotStack->setCurrentIndex(current3DPlot);
      canvas = plotStack->currentWidget();
      surfaceGraph = surfaceGraphs[current3DPlot];
      surfaceContainer = surfaceContainers[current3DPlot];
      QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(current3DPlot + 1).arg(total3DPlots);
      mainWindow->setWindowTitle(wtitle);
    }
    return;
  }
  if (cur == (struct PLOTREC *)NULL) {
    QApplication::beep();
  } else if (cur->prev == (struct PLOTREC *)NULL) {
    QApplication::beep();
  } else {
    cur = cur->prev;
    canvas->update();
    QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
    mainWindow->setWindowTitle(wtitle);
  }
}

void delete_current(QMainWindow *mainWindow) {
  if (plotStack) {
    QApplication::beep();
    return;
  }
  if (cur == (struct PLOTREC *)NULL) {
    QApplication::beep();
  } else {
    destroyplotrec(cur);
    canvas->update();
    QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
    mainWindow->setWindowTitle(wtitle);
  }
}

void mouse_tracking(QMainWindow *mainWindow) {
  tracking = !tracking;
}

void to_number(QMainWindow *mainWindow) {
  if (plotStack) {
    bool ok;
    int number = QInputDialog::getInt(mainWindow, "Enter a Plot Number",
                                      "Enter a Plot Number:", current3DPlot + 1, 1,
                                      total3DPlots, 1, &ok);
    if (ok) {
      if (number >= 1 && number <= total3DPlots) {
        current3DPlot = number - 1;
        plotStack->setCurrentIndex(current3DPlot);
        canvas = plotStack->currentWidget();
        surfaceGraph = surfaceGraphs[current3DPlot];
        surfaceContainer = surfaceContainers[current3DPlot];
        QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(current3DPlot + 1).arg(total3DPlots);
        mainWindow->setWindowTitle(wtitle);
      }
    }
    return;
  }
  bool ok;
  int number = QInputDialog::getInt(
                                    mainWindow,
                                    "Enter a Plot Number",      // Dialog title
                                    "Enter a Plot Number:", // Label text
                                    cur->nplot,                        // Default value
                                    1,                   // Minimum value
                                    nplots,                    // Maximum value
                                    1,                        // Step
                                    &ok                     // Returned status
                                    );
  if (ok) {
    cur = last;
    while (number <  cur->nplot)
      cur = cur->prev;
    canvas->update();
    QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
    mainWindow->setWindowTitle(wtitle);
  }
}

void setup_shortcuts(QMainWindow *mainWindow) {
  QShortcut *shortcut_B = new QShortcut(QKeySequence(Qt::Key_B), mainWindow);
  QShortcut *shortcut_C = new QShortcut(QKeySequence(Qt::Key_C), mainWindow);
  QShortcut *shortcut_D = new QShortcut(QKeySequence(Qt::Key_D), mainWindow);
  QShortcut *shortcut_F = new QShortcut(QKeySequence(Qt::Key_F), mainWindow);
  QShortcut *shortcut_L = new QShortcut(QKeySequence(Qt::Key_L), mainWindow);
  QShortcut *shortcut_N = new QShortcut(QKeySequence(Qt::Key_N), mainWindow);
  QShortcut *shortcut_P = new QShortcut(QKeySequence(Qt::Key_P), mainWindow);
  QShortcut *shortcut_Q = new QShortcut(QKeySequence(Qt::Key_Q), mainWindow);
  QShortcut *shortcut_R = new QShortcut(QKeySequence(Qt::Key_R), mainWindow);
  QShortcut *shortcut_T = new QShortcut(QKeySequence(Qt::Key_T), mainWindow);
  QShortcut *shortcut_W = new QShortcut(QKeySequence(Qt::Key_W), mainWindow);
  QShortcut *shortcut_Z = new QShortcut(QKeySequence(Qt::Key_Z), mainWindow);
  QShortcut *shortcut_0 = new QShortcut(QKeySequence(Qt::Key_0), mainWindow);
  QShortcut *shortcut_1 = new QShortcut(QKeySequence(Qt::Key_1), mainWindow);
  QShortcut *shortcut_2 = new QShortcut(QKeySequence(Qt::Key_2), mainWindow);
  QShortcut *shortcut_3 = new QShortcut(QKeySequence(Qt::Key_3), mainWindow);
  QShortcut *shortcut_4 = new QShortcut(QKeySequence(Qt::Key_4), mainWindow);
  QShortcut *shortcut_Plus = new QShortcut(QKeySequence(Qt::Key_Plus), mainWindow);
  QShortcut *shortcut_Minus = new QShortcut(QKeySequence(Qt::Key_Minus), mainWindow);
  QShortcut *shortcut_Period = new QShortcut(QKeySequence(Qt::Key_Period), mainWindow);
  QShortcut *shortcut_M = new QShortcut(QKeySequence(Qt::Key_M), mainWindow);
  QShortcut *shortcut_LT = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Comma), mainWindow);
  QShortcut *shortcut_GT = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Period), mainWindow);
  QObject::connect(shortcut_B, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width();
    int height = screenGeometry.height() / 2;
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x(), screenGeometry.y() + height, width, height);
  });
  QObject::connect(shortcut_C, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width() / 2;
    int height = screenGeometry.height() / 2;
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x() + (width / 2), screenGeometry.y() + (height / 2), width, height);
  });
  QObject::connect(shortcut_D, &QShortcut::activated, [mainWindow](){
    delete_current(mainWindow);
  });
  QObject::connect(shortcut_F, &QShortcut::activated, [mainWindow](){
    if (mainWindow->isMaximized()) {
      mainWindow->showNormal();
    } else {
      mainWindow->showMaximized();
    }
  });
  QObject::connect(shortcut_L, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width() / 2;
    int height = screenGeometry.height();
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x(), screenGeometry.y(), width, height);
  });
  QObject::connect(shortcut_N, &QShortcut::activated, [mainWindow](){
    nav_next(mainWindow);
  });
  QObject::connect(shortcut_P, &QShortcut::activated, [mainWindow](){
    nav_previous(mainWindow);
  });
  QObject::connect(shortcut_R, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width() / 2;
    int height = screenGeometry.height();
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x() + width, screenGeometry.y(), width, height);
  });
  QObject::connect(shortcut_Q, &QShortcut::activated, [](){
    quit();
  });
  QObject::connect(shortcut_T, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width();
    int height = screenGeometry.height() / 2;
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x(), screenGeometry.y(), width, height);
  });
  shortcut_W->setContext(Qt::ApplicationShortcut);
  QObject::connect(shortcut_W, &QShortcut::activated, []() {
    static bool whiteTheme = false;
    if (surfaceGraph) {
      QtDataVisualization::Q3DTheme *theme = surfaceGraph->activeTheme();
      QColor bg = whiteTheme ? Qt::black : Qt::white;
      QColor fg = whiteTheme ? Qt::white : Qt::black;
      theme->setBackgroundColor(bg);
      theme->setWindowColor(bg);
      theme->setLabelTextColor(fg);
      theme->setGridLineColor(fg);
      theme->setLabelBackgroundColor(bg);
      if (surfaceContainer) {
        QPalette pal = surfaceContainer->palette();
        pal.setColor(QPalette::Window, bg);
        pal.setColor(QPalette::WindowText, fg);
        surfaceContainer->setPalette(pal);
        QWidget *parent = surfaceContainer->parentWidget();
        if (parent) {
          parent->setPalette(pal);
          const QList<QLabel *> labels = parent->findChildren<QLabel *>();
          for (QLabel *label : labels) {
            QPalette lp = label->palette();
            lp.setColor(QPalette::Window, bg);
            lp.setColor(QPalette::WindowText, fg);
            label->setPalette(lp);
          }
        }
      }
    } else {
      if (whiteTheme)
        onBlack();
      else
        onWhite();
    }
    whiteTheme = !whiteTheme;
  });
  QObject::connect(shortcut_Z, &QShortcut::activated, [](){
    replotZoom = !replotZoom;
    replotZoomAction->setChecked(replotZoom);
  });
  QObject::connect(shortcut_0, &QShortcut::activated, [mainWindow](){
    mainWindow->showNormal();
    mainWindow->resize(WIDTH + 20, HEIGHT + 40);
  });
  QObject::connect(shortcut_1, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width() / 2;
    int height = screenGeometry.height() / 2;
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x(), screenGeometry.y(), width, height);
  });
  QObject::connect(shortcut_2, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width() / 2;
    int height = screenGeometry.height() / 2;
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x() + width, screenGeometry.y(), width, height);
  });
  QObject::connect(shortcut_3, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width() / 2;
    int height = screenGeometry.height() / 2;
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x(), screenGeometry.y() + height, width, height);
  });
  QObject::connect(shortcut_4, &QShortcut::activated, [mainWindow](){
    QScreen *screen = mainWindow->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int width = screenGeometry.width() / 2;
    int height = screenGeometry.height() / 2;
    mainWindow->showNormal();
    mainWindow->setGeometry(screenGeometry.x() + width, screenGeometry.y() + height, width, height);
  });
  QObject::connect(shortcut_Plus, &QShortcut::activated, [mainWindow](){
    mainWindow->showNormal();
    mainWindow->resize(mainWindow->size().width() * 1.2, mainWindow->size().height() * 1.2);
  });
  QObject::connect(shortcut_Minus, &QShortcut::activated, [mainWindow](){
    mainWindow->showNormal();
    mainWindow->resize(mainWindow->size().width() * .8, mainWindow->size().height() * .8);
  });
  QObject::connect(shortcut_Period, &QShortcut::activated, [mainWindow](){
    mouse_tracking(mainWindow);
  });
  QObject::connect(shortcut_M, &QShortcut::activated, [mainWindow](){
    // Play movie: iterate through all plots
    // go to first plot
    while (cur->prev)
        cur = cur->prev;
    usecoordn = 0;
    for (;;) {
        canvas->update();
        QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
        mainWindow->setWindowTitle(wtitle);
        QTime dieTime = QTime::currentTime().addMSecs(100);
        while (QTime::currentTime() < dieTime)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        if (cur->next && cur->next != cur)
            cur = cur->next;
        else
            break;
    }
  });
  QObject::connect(shortcut_LT, &QShortcut::activated, [mainWindow](){
    while (cur->prev)
      cur = cur->prev;
    usecoordn = 0;
    canvas->update();
    QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
    mainWindow->setWindowTitle(wtitle);
  });
  QObject::connect(shortcut_GT, &QShortcut::activated, [mainWindow](){
    cur = last;
    usecoordn = 0;
    canvas->update();
    QString wtitle = QString("MPL Outboard Driver (Plot %1 of %2)").arg(cur->nplot).arg(nplots);
    mainWindow->setWindowTitle(wtitle);
  });
}
