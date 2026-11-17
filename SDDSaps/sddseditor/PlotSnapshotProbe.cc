/**
 * @file PlotSnapshotProbe.cc
 * @brief Capture the editor's plot input for the offscreen regression target.
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license See LICENSE in the repository root.
 */
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

/** Copy the snapshot and record arguments without launching a plot window. */
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QString capture = QString::fromLocal8Bit(qgetenv("SDDSEDITOR_PLOT_CAPTURE"));
  if (capture.isEmpty())
    return 1;
  const QStringList args = app.arguments();
  const bool legacyViewer = qEnvironmentVariableIsSet("SDDSEDITOR_PLOT_LEGACY_VIEWER");
  if (QFileInfo(args.first()).baseName() == "mpl_qt") {
    if (legacyViewer) {
      fputs("Invalid option -commandFile\n", stderr);
      return 1;
    }
    QFile command(args.value(2));
    if (!command.open(QIODevice::ReadOnly))
      return 4;
    QFile recorded(capture + ".viewer");
    if (!recorded.open(QIODevice::WriteOnly))
      return 5;
    recorded.write(command.readAll());
    recorded.close();
    if (qEnvironmentVariableIsSet("SDDSEDITOR_PLOT_VIEWER_HOLD")) {
      QTimer release;
      QObject::connect(&release, &QTimer::timeout, [&]() {
        if (QFileInfo::exists(capture + ".release"))
          app.quit();
      });
      release.start(10);
      return app.exec();
    }
    if (qEnvironmentVariableIsSet("SDDSEDITOR_PLOT_VIEWER_FAIL")) {
      fprintf(stderr, "viewer regression failure\n");
      return 7;
    }
    return 0;
  }
  if (legacyViewer && args.contains("-device=qt"))
    capture += ".qt";
  if (qEnvironmentVariableIsSet("SDDSEDITOR_PLOT_RENDER_FAIL")) {
    fprintf(stderr, "renderer regression failure\n");
    return 6;
  }
  for (const QString &arg : args)
    if (arg.startsWith("-output=")) {
      QFile output(arg.mid(8));
      if (!output.open(QIODevice::WriteOnly) || output.write("plot regression stream") <= 0)
        return 8;
    }
  for (const QString &arg : args) {
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
