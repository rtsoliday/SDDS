/**
 * @file PlotSnapshotProbe.cc
 * @brief Capture the editor's plot input for the offscreen regression target.
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license See LICENSE in the repository root.
 */
#include <QCoreApplication>
#include <QFile>

/** Copy the snapshot and record arguments without launching a plot window. */
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const QString capture = QString::fromLocal8Bit(qgetenv("SDDSEDITOR_PLOT_CAPTURE"));
  if (capture.isEmpty())
    return 1;
  for (const QString &arg : app.arguments()) {
    if (arg.endsWith(".sdds") && QFile::copy(arg, capture)) {
      QFile arguments(capture + ".args");
      if (!arguments.open(QIODevice::WriteOnly))
        return 2;
      arguments.write(app.arguments().join('\n').toUtf8());
      return 0;
    }
  }
  return 3;
}
