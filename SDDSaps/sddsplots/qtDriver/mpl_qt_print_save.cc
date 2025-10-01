#include "mpl_qt.h"

static constexpr int LPNG_XMAX = 1093;
static constexpr int LPNG_YMAX = 842;

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
    QString filters = QObject::tr("PNG Files (*.png);;JPEG Files (*.jpg *.jpeg);;PDF Files (*.pdf)");
    QString fileName = QFileDialog::getSaveFileName(canvas->window(),
                                                    QObject::tr("Save Image"),
                                                    "",
                                                    filters,
                                                    nullptr,
                                                    QFileDialog::DontUseNativeDialog);
    if (!fileName.isEmpty()) {
        QFileInfo fi(fileName);
        QString ext = fi.suffix().toLower();
        if (ext == "pdf") {
            // Save as PDF using QPrinter and QPainter
            QPrinter printer;
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(fileName);
            onWhite();
            QPainter painter(&printer);
            canvas->render(&painter);
            if (surfaceGraph && surfaceContainer) {
                QImage graphImage = surfaceGraph->renderToImage(0, surfaceContainer->size());
                painter.drawImage(surfaceContainer->geometry().topLeft(), graphImage);
            }
            painter.end();
            onBlack();
        } else {
            onWhite();
            QSize exportSize(LPNG_XMAX, LPNG_YMAX);
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
            onBlack();
            QString format = (ext == "jpg" || ext == "jpeg") ? "JPG" : "PNG";
            plotImage.save(fileName, format.toUtf8().constData());
        }
    }
}
