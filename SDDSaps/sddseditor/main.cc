/**
 * @file main.cc
 * @brief Entry point for the SDDS editor application.
 */

#include "SDDSEditor.h"
#include <QApplication>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  SDDSEditor editor;
  if (argc > 1)
    editor.loadFile(argv[1]);
  editor.show();
  return app.exec();
}
