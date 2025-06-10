/**
 * @file main.cc
 * @brief Entry point for the SDDS editor application.
 */

#include "SDDSEditor.h"
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QLoggingCategory>

int main(int argc, char **argv) {
  QLoggingCategory::setFilterRules("qt.qpa.xcb.*=false");
  QApplication app(argc, argv);
  app.setStyle(QStyleFactory::create("Fusion"));
  QPalette pal = app.palette();
  pal.setColor(QPalette::Active, QPalette::Text, Qt::black);
  pal.setColor(QPalette::Inactive, QPalette::Text, Qt::black);
  pal.setColor(QPalette::Active, QPalette::WindowText, Qt::black);
  pal.setColor(QPalette::Inactive, QPalette::WindowText, Qt::black);
  app.setPalette(pal);
  SDDSEditor editor;
  if (argc > 1)
    editor.loadFile(argv[1]);
  editor.show();
  return app.exec();
}
