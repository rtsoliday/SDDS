/**
 * @file main.cc
 * @brief Entry point for the SDDS editor application.
 */

#include "SDDSEditor.h"
#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QStyleFactory>
#include <QLoggingCategory>
#include <QTimer>
#include <QString>

int main(int argc, char **argv) {
  QLoggingCategory::setFilterRules("qt.qpa.xcb.*=false\nqt.qpa.fonts=false");
  QApplication app(argc, argv);
  app.setStyle(QStyleFactory::create("Fusion"));
  QPalette pal = app.palette();
  bool dark = pal.color(QPalette::Window).lightness() < 128;
  QColor textColor = dark ? Qt::white : Qt::black;
  pal.setColor(QPalette::Active, QPalette::Text, textColor);
  pal.setColor(QPalette::Inactive, QPalette::Text, textColor);
  pal.setColor(QPalette::Active, QPalette::WindowText, textColor);
  pal.setColor(QPalette::Inactive, QPalette::WindowText, textColor);
  app.setPalette(pal);
  SDDSEditor editor(dark);
  editor.show();
  if (argc > 1) {
    const QString path = QString::fromLocal8Bit(argv[1]);
    QTimer::singleShot(0, &editor, [&editor, path]() {
      editor.loadFile(path);
    });
  }
  return app.exec();
}
