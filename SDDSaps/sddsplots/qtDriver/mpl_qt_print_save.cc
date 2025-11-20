#include "mpl_qt.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStringList>
#include <QPageLayout>
#include <QPageSize>
#include <QImageWriter>
#include <QTextStream>

static constexpr int LPNG_XMAX = 1093;
static constexpr int LPNG_YMAX = 842;
static constexpr int kMaxMetadataLength = 1012;
static constexpr int kEpsResolutionDpi = 600;

static QString commandLineMetadata() {
  if (sddsplotCommandline2 && sddsplotCommandline2[0]) {
    QString command = QString::fromUtf8(sddsplotCommandline2);
    if (command.size() > kMaxMetadataLength)
      command.truncate(kMaxMetadataLength);
    return command;
  }
  return QStringLiteral("mpl_qt");
}

static QString cwdMetadata() {
  return QDir::currentPath();
}

static QImage buildExportImage(const QSize &exportSize) {
  if (!canvas)
    return QImage();
  QImage plotImage = exportCurrentPlotImage(exportSize);
  if (plotImage.isNull()) {
    QPixmap pixmap = canvas->grab();
    plotImage = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
  }
  if (surfaceGraph && surfaceContainer) {
    QImage graphImage = surfaceGraph->renderToImage(0, exportSize);
    if (!graphImage.isNull()) {
      QPainter painter(&plotImage);
      double scaleX = (exportSize.width() > 0 && canvas->width() > 0)
                          ? static_cast<double>(exportSize.width()) / canvas->width()
                          : 1.0;
      double scaleY = (exportSize.height() > 0 && canvas->height() > 0)
                          ? static_cast<double>(exportSize.height()) / canvas->height()
                          : 1.0;
      QPoint topLeft = surfaceContainer->geometry().topLeft();
      QPoint scaledTopLeft(std::lround(topLeft.x() * scaleX), std::lround(topLeft.y() * scaleY));
      QSize scaledSize(std::lround(surfaceContainer->width() * scaleX),
                       std::lround(surfaceContainer->height() * scaleY));
      painter.drawImage(QRect(scaledTopLeft, scaledSize), graphImage);
      painter.end();
    }
  }
  return plotImage;
}

static bool rewritePsHeaders(const QString &filePath, const QString &creator,
                             const QString &cwd) {
  QFile handle(filePath);
  if (!handle.open(QIODevice::ReadWrite | QIODevice::Text))
    return true;

  QString content = QString::fromUtf8(handle.readAll());
  QStringList lines = content.split('\n');
  bool creatorSet = false;
  bool cwdSet = false;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines[i].startsWith(QStringLiteral("%%Creator:"))) {
      lines[i] = QStringLiteral("%%Creator: ") + creator;
      creatorSet = true;
    } else if (lines[i].startsWith(QStringLiteral("%%CWD:"))) {
      lines[i] = QStringLiteral("%%CWD: ") + cwd;
      cwdSet = true;
    }
  }
  if (!creatorSet)
    lines.insert(1, QStringLiteral("%%Creator: ") + creator);
  if (!cwdSet)
    lines.insert(creatorSet ? 2 : 2, QStringLiteral("%%CWD: ") + cwd);

  content = lines.join(QStringLiteral("\n"));
  handle.resize(0);
  handle.seek(0);
  handle.write(content.toUtf8());
  handle.close();
  return true;
}

static bool renderPlotWithPrinter(const QString &fileName,
                                  const QString &creator,
                                  const QString &docName,
                                  QPrinter::OutputFormat format) {
  QPrinter printer;
  printer.setOutputFormat(format);
  printer.setOutputFileName(fileName);
  printer.setCreator(creator);
  printer.setDocName(docName);
  printer.setResolution(kEpsResolutionDpi);
  if (canvas) {
    QSizeF ptsSize(canvas->width(), canvas->height());
    QPageSize pageSize(ptsSize, QPageSize::Point);
    printer.setPageSize(pageSize);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Point);
  }
  printer.setFullPage(true);

  onWhite();
  QPainter painter(&printer);
  QSize target = printer.pageRect(QPrinter::DevicePixel).size().toSize();
  if (!target.isValid())
    target = QSize(LPNG_XMAX, LPNG_YMAX);
  painter.fillRect(QRect(QPoint(0, 0), target), Qt::white);
  bool painted = exportCurrentPlotToPainter(painter, target);
  if (!painted && canvas)
    canvas->render(&painter);
  if (surfaceGraph && surfaceContainer) {
    QImage graphImage = surfaceGraph->renderToImage(0, surfaceContainer->size());
    painter.drawImage(surfaceContainer->geometry().topLeft(), graphImage);
  }
  painter.end();
  onBlack();
  return true;
}

static bool convertPdfToEpsWithGs(const QString &pdfFile,
                                  const QString &epsFile,
                                  const QString &creator,
                                  const QString &cwd) {
  QStringList args;
  args << "-dBATCH" << "-dNOPAUSE" << "-dSAFER"
       << "-sDEVICE=eps2write"
       << QString("-r%1").arg(kEpsResolutionDpi)
       << QString("-sOutputFile=%1").arg(epsFile)
       << pdfFile;

  QProcess gs;
  gs.start(QStringLiteral("gs"), args);
  if (!gs.waitForStarted() || !gs.waitForFinished()) {
    fprintf(stderr, "mpl_qt: ghostscript did not finish EPS conversion.\n");
    return false;
  }
  if (gs.exitStatus() != QProcess::NormalExit || gs.exitCode() != 0) {
    const QByteArray err = gs.readAllStandardError();
    if (!err.isEmpty())
      fwrite(err.constData(), 1, err.size(), stderr);
    fprintf(stderr, "mpl_qt: ghostscript EPS conversion failed.\n");
    return false;
  }

  return rewritePsHeaders(epsFile, creator, cwd);
}

static bool convertPdfToPsWithGs(const QString &pdfFile,
                                 const QString &psFile,
                                 const QString &creator,
                                 const QString &cwd) {
  QStringList args;
  args << "-dBATCH" << "-dNOPAUSE" << "-dSAFER"
       << "-sDEVICE=ps2write"
       << QString("-r%1").arg(kEpsResolutionDpi)
       << "-dLanguageLevel=2"
       << QString("-sOutputFile=%1").arg(psFile)
       << pdfFile;

  QProcess gs;
  gs.start(QStringLiteral("gs"), args);
  if (!gs.waitForStarted() || !gs.waitForFinished()) {
    fprintf(stderr, "mpl_qt: ghostscript did not finish PS conversion.\n");
    return false;
  }
  if (gs.exitStatus() != QProcess::NormalExit || gs.exitCode() != 0) {
    const QByteArray err = gs.readAllStandardError();
    if (!err.isEmpty())
      fwrite(err.constData(), 1, err.size(), stderr);
    fprintf(stderr, "mpl_qt: ghostscript PS conversion failed.\n");
    return false;
  }

  return rewritePsHeaders(psFile, creator, cwd);
}

static bool writeEps(const QString &fileName, const QImage &image,
                     const QString &creator, const QString &cwd) {
  if (image.isNull())
    return false;
  QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
  QFile epsFile(fileName);
  if (!epsFile.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  QTextStream out(&epsFile);
  const int width = rgbImage.width();
  const int height = rgbImage.height();
  out << "%!PS-Adobe-3.0 EPSF-3.0\n";
  if (!creator.isEmpty())
    out << "%%Creator: " << creator << "\n";
  if (!cwd.isEmpty())
    out << "%%CWD: " << cwd << "\n";
  out << "%%Title: " << QFileInfo(fileName).fileName() << "\n";
  out << "%%CreationDate: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
  out << "%%BoundingBox: 0 0 " << width << " " << height << "\n";
  out << "%%LanguageLevel: 2\n";
  out << "%%EndComments\n";
  out << "/pix " << (width * 3) << " string def\n";
  out << width << " " << height << " 8\n";
  out << "[" << width << " 0 0 -" << height << " 0 " << height << "]\n";
  out << "{ currentfile pix readhexstring pop } false 3 colorimage\n";

  int columnCount = 0;
  for (int y = 0; y < height; ++y) {
    const uchar *scan = rgbImage.constScanLine(y);
    for (int x = 0; x < width; ++x) {
      for (int channel = 0; channel < 3; ++channel) {
        unsigned char value = scan[x * 3 + channel];
        out << QString("%1").arg(value, 2, 16, QLatin1Char('0'));
        columnCount += 2;
        if (columnCount >= 64) {
          out << "\n";
          columnCount = 0;
        }
      }
    }
    if (columnCount) {
      out << "\n";
      columnCount = 0;
    }
  }
  out << "showpage\n%%EOF\n";
  return true;
}

void print() {
  QPrinter printer;
  QPrintDialog dialog(&printer, canvas);
  if (dialog.exec() == QDialog::Accepted) {
    onWhite();
    QPainter painter(&printer);
    canvas->render(&painter);
    if (surfaceGraph && surfaceContainer) {
      QImage graphImage = surfaceGraph->renderToImage(0, surfaceContainer->size());
      painter.drawImage(surfaceContainer->geometry().topLeft(), graphImage);
    }
    painter.end();
    onBlack();
  }
}

void save() {
  QString filters = QObject::tr("PNG Files (*.png);;JPEG Files (*.jpg *.jpeg)");
  QString fileName = QFileDialog::getSaveFileName(canvas->window(),
                                                  QObject::tr("Save Image"),
                                                  "",
                                                  filters,
                                                  nullptr,
                                                  QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  QFileInfo fi(fileName);
  QString ext = fi.suffix().toLower();
  if (ext.isEmpty())
    ext = QStringLiteral("png");

  onWhite();
  QImage plotImage = buildExportImage(QSize(LPNG_XMAX, LPNG_YMAX));
  onBlack();

  if (plotImage.isNull()) {
    fprintf(stderr, "mpl_qt: failed to capture plot for saving.\n");
    return;
  }

  QString commentText = QStringLiteral("Command: %1\nCWD: %2")
                            .arg(commandLineMetadata(), cwdMetadata());
  QString format = (ext == "jpg" || ext == "jpeg") ? "JPG" : "PNG";
  QImageWriter writer(fileName, format.toUtf8());
  writer.setText(QStringLiteral("Description"), commandLineMetadata());
  writer.setText(QStringLiteral("Comment"), commentText);
  if (!writer.write(plotImage)) {
    fprintf(stderr, "mpl_qt: failed to save image %s (%s)\n",
            fileName.toLocal8Bit().constData(),
            writer.errorString().toLocal8Bit().constData());
  }
}

void savePdfOrEps() {
  QString filters = QObject::tr("PDF Files (*.pdf);;PS Files (*.ps);;EPS Files (*.eps)");
  QString fileName = QFileDialog::getSaveFileName(canvas->window(),
                                                  QObject::tr("Save as PDF, PS or EPS"),
                                                  "",
                                                  filters,
                                                  nullptr,
                                                  QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  QFileInfo fi(fileName);
  QString ext = fi.suffix().toLower();
  if (ext.isEmpty())
    ext = QStringLiteral("pdf");

  QString creator = commandLineMetadata();
  if (ext == "eps") {
    QTemporaryFile pdfTemp(QStringLiteral("mpl_qt_eps_XXXXXX.pdf"));
    pdfTemp.setAutoRemove(true);
    if (!pdfTemp.open()) {
      fprintf(stderr, "mpl_qt: unable to create temporary PDF for EPS export.\n");
      return;
    }
    QString tempPdfName = pdfTemp.fileName();
    pdfTemp.close();

    if (!renderPlotWithPrinter(tempPdfName, creator, fi.fileName(), QPrinter::PdfFormat)) {
      fprintf(stderr, "mpl_qt: failed to render plot for EPS export.\n");
      return;
    }

    if (!convertPdfToEpsWithGs(tempPdfName, fileName, creator, cwdMetadata()) &&
        [&]() {
          onWhite();
          QSize base = canvas ? canvas->size() : QSize(LPNG_XMAX, LPNG_YMAX);
          QSize highRes(base.width() * 3, base.height() * 3);
          QImage fallbackImage = buildExportImage(highRes);
          onBlack();
          return writeEps(fileName, fallbackImage, creator, cwdMetadata());
        }()) {
      fprintf(stderr, "mpl_qt: unable to write EPS file %s\n",
              fileName.toLocal8Bit().constData());
    }
    return;
  }

  if (ext == "ps") {
    QTemporaryFile pdfTemp(QStringLiteral("mpl_qt_ps_XXXXXX.pdf"));
    pdfTemp.setAutoRemove(true);
    if (!pdfTemp.open()) {
      fprintf(stderr, "mpl_qt: unable to create temporary PDF for PS export.\n");
      return;
    }
    QString tempPdfName = pdfTemp.fileName();
    pdfTemp.close();

    if (!renderPlotWithPrinter(tempPdfName, creator, fi.fileName(), QPrinter::PdfFormat)) {
      fprintf(stderr, "mpl_qt: failed to render plot for PS export.\n");
      return;
    }

    if (!convertPdfToPsWithGs(tempPdfName, fileName, creator, cwdMetadata()))
      fprintf(stderr, "mpl_qt: unable to write PS file %s\n",
              fileName.toLocal8Bit().constData());
    return;
  }

  renderPlotWithPrinter(fileName, creator, fi.fileName(), QPrinter::PdfFormat);
}
