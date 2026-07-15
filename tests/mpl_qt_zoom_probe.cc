/**
 * @file mpl_qt_zoom_probe.cc
 * @brief Drives repeated mpl_qt zoom gestures for regression testing.
 *
 * The probe is loaded only by the Linux pytest suite.  It schedules mouse
 * events after mpl_qt enters the QApplication event loop, allowing the test to
 * exercise the same Canvas event path used by interactive zooming.
 *
 * @copyright Copyright (c) 2026 The University of Chicago
 * @license Distributed under the Software License Agreement in the repository
 * LICENSE file.
 */

#include <cstring>
#include <dlfcn.h>

#include <QApplication>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

namespace {

QWidget *findCanvas() {
  for (QWidget *topLevel : QApplication::topLevelWidgets()) {
    const QList<QWidget *> widgets = topLevel->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
      if (!std::strcmp(widget->metaObject()->className(), "Canvas"))
        return widget;
    }
  }
  return nullptr;
}

void sendZoom(QWidget *canvas) {
  const QPoint start(canvas->width() * 30 / 100, canvas->height() * 10 / 100);
  const QPoint finish(canvas->width() * 70 / 100, canvas->height() * 90 / 100);

  QMouseEvent press(QEvent::MouseButtonPress, start, Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(canvas, &press);
  QMouseEvent move(QEvent::MouseMove, finish, Qt::NoButton,
                   Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(canvas, &move);
  QMouseEvent release(QEvent::MouseButtonRelease, finish, Qt::LeftButton,
                      Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(canvas, &release);
  QMouseEvent apply(QEvent::MouseButtonPress, finish, Qt::MiddleButton,
                    Qt::MiddleButton, Qt::NoModifier);
  QCoreApplication::sendEvent(canvas, &apply);
}

void runZoomStep() {
  static int step = 0;
  static int canvasRetries = 0;
  QWidget *canvas = findCanvas();
  if (!canvas) {
    if (++canvasRetries < 20) {
      QTimer::singleShot(250, runZoomStep);
      return;
    }
    QCoreApplication::exit(2);
    return;
  }

  canvas->repaint();
  if (step == 6) {
    QCoreApplication::quit();
    return;
  }

  sendZoom(canvas);
  ++step;
  QTimer::singleShot(500, runZoomStep);
}

}  // namespace

int QApplication::exec() {
  using ExecFunction = int (*)();
  ExecFunction realExec = reinterpret_cast<ExecFunction>(
      dlsym(RTLD_NEXT, "_ZN12QApplication4execEv"));
  if (!realExec)
    return 125;
  QTimer::singleShot(1000, runZoomStep);
  return realExec();
}
