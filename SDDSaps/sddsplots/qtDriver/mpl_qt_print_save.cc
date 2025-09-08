#include "mpl_qt.h"

void print() {
    QPrinter printer;
    QPrintDialog dialog(&printer, canvas);
    if (dialog.exec() == QDialog::Accepted) {
        QPainter painter(&printer);
        // Render the frame’s contents onto the printer
        onWhite();
        canvas->render(&painter);
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
            painter.end();
            onBlack();
        } else {
            // Save as image using QPixmap::save()
            onWhite();
            QPixmap pixmap = canvas->grab();
            if (surfaceGraph && surfaceContainer) {
                QImage graphImage = surfaceGraph->renderToImage(0, surfaceContainer->size());
                QPainter painter(&pixmap);
                painter.drawImage(surfaceContainer->geometry().topLeft(), graphImage);
                painter.end();
            }
            onBlack();
            QString format = (ext == "jpg" || ext == "jpeg") ? "JPG" : "PNG";
            pixmap.save(fileName, format.toUtf8().constData());
        }
    }
}
