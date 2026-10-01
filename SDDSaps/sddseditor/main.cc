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
#include <QStringList>
#include <clocale>

int main(int argc, char **argv) {
  QLoggingCategory::setFilterRules("qt.qpa.xcb.*=false\nqt.qpa.fonts=false");
  QApplication app(argc, argv);
  /*
   * On Unix, QApplication adopts the user's locale for C library calls.  In
   * locales with a decimal comma, strtod/strtold (used here and in SDDSlib)
   * would reject "1.5".  SDDS text is always written with a decimal point.
   */
  setlocale(LC_NUMERIC, "C");
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
  // arguments() decodes the command line correctly for non-ASCII paths on Windows.
  const QStringList arguments = QCoreApplication::arguments();
  if (arguments.size() > 1) {
    const QString path = arguments.at(1);
    QTimer::singleShot(0, &editor, [&editor, path]() {
      editor.loadFile(path);
    });
  }
  return app.exec();
}
