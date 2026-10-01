/**
 * @file SDDSEditorTests.cc
 * @brief Offscreen regression tests for editor persistence and editing behavior.
 * @details Includes the implementation to test its internal models and parsers
 * without adding a diagnostic interface to the application.
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license See LICENSE in the repository root.
 */
#include "SDDSEditor.cc"
#undef QMessageBox
#undef QInputDialog
#include <QElapsedTimer>
#include <QBrush>
#include <QCheckBox>
#include <clocale>

/** Fail with a named assertion that is attributable to this test program. */
static void require(bool condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL sddseditor_tests: %s\n", message);
    std::exit(1);
  }
}

/** Read an artifact without changing it. */
static QByteArray readFile(const QString &path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "open artifact for reading");
  return file.readAll();
}

/** Write a known fixture. */
static void putFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly), "open fixture for writing");
  require(file.write(contents) == contents.size(), "write fixture");
}

/** Operate a real editor dialog through Qt's event loop. */
static void acceptDialog(const QString &title, std::function<void(QDialog *)> configure) {
  QTimer::singleShot(0, [title, configure]() {
    for (QWidget *widget : QApplication::topLevelWidgets()) {
      QDialog *dialog = qobject_cast<QDialog *>(widget);
      if (dialog && dialog->windowTitle() == title) {
        configure(dialog);
        dialog->accept();
        return;
      }
    }
    require(false, "expected editor dialog exists");
  });
}

class SDDSEditorTests {
public:
  /** Regressions for pending edits, filtered operations and lossless byte/number storage. */
  static void editingSafety(const QString &root) {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      if (!ok)
        ++failures;
    };
    auto acceptOptional = [](const QString &title, std::function<void(QDialog *)> configure) {
      QTimer::singleShot(0, [title, configure]() {
        for (QWidget *widget : QApplication::topLevelWidgets())
          if (QDialog *dialog = qobject_cast<QDialog *>(widget))
            if (!qobject_cast<QMessageBox *>(dialog) && dialog->isVisible() && dialog->windowTitle() == title) {
              configure(dialog);
              dialog->accept();
              return;
            }
      });
    };
    {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      editor.activateWindow();
      editor.columnView->setFocus();
      QCoreApplication::processEvents();
      editor.columnView->edit(editor.columnModel->index(0, 0));
      QCoreApplication::processEvents();
      QLineEdit *cell = editor.columnView->viewport()->findChild<QLineEdit *>();
      require(cell, "open pending cell editor");
      require(!cell->isHidden(), "pending cell editor is shown");
      cell->setText("123");
      require(!editor.dirty && editor.pages[0].columns[0][0] == "3", "cell edit is still pending");
      bool prompted = false;
      QTimer::singleShot(0, [&]() {
        for (QWidget *widget : QApplication::topLevelWidgets())
          if (QMessageBox *box = qobject_cast<QMessageBox *>(widget)) {
            prompted = true;
            box->done(QMessageBox::Cancel);
          }
      });
      const bool canClose = editor.maybeSave();
      QCoreApplication::processEvents();
      check(prompted && !canClose && editor.pages[0].columns[0][0] == "123",
            "closing checks and commits the pending cell before asking to save");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.openArrayViewer(0);
      ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      viewer->table()->edit(viewer->sliceModel()->index(0, 0));
      QCoreApplication::processEvents();
      QLineEdit *cell = viewer->table()->viewport()->findChild<QLineEdit *>();
      require(cell && !cell->isHidden(), "open pending array viewer editor");
      cell->setText("456");
      const QString path = root + "/pending-array-edit.sdds";
      require(editor.writeFile(path), "save with a pending array viewer edit");
      SDDSEditor loaded;
      require(loaded.loadFile(path), "reload pending viewer edit");
      check(loaded.pages[0].arrays[0].values[0] == "456", "Save commits array viewer editors");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.pages[0].columns[0] = {"1", "1", "1"};
      editor.populateModels();
      editor.rowFilterActive = true;
      editor.rowFilterExpression = "row == 1";
      editor.refreshColumnRowFilter(false);
      editor.searchColumn(0);
      QDialog *dialog = editor.searchColumnDialog;
      const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
      fields[0]->setText("1");
      fields[1]->setText("2");
      for (QPushButton *button : dialog->findChildren<QPushButton *>())
        if (button->text() == "Replace All")
          button->click();
      check(editor.pages[0].columns[0] == QVector<QString>({"1", "2", "1"}),
            "Replace All preserves hidden rows");
      editor.undoStack->undo();
      check(editor.pages[0].columns[0] == QVector<QString>({"1", "1", "1"}),
            "filtered Replace All is undone as one operation");
      editor.undoStack->redo();
      check(editor.pages[0].columns[0] == QVector<QString>({"1", "2", "1"}),
            "filtered Replace All redo preserves hidden rows");
      fields[0]->setText("2");
      fields[1]->setText("3");
      for (QPushButton *button : dialog->findChildren<QPushButton *>())
        if (button->text() == "Search")
          button->click();
      editor.pages[0].columns[0] = {"1", "2", "2"};
      editor.rowFilterExpression = "row == 2";
      editor.refreshColumnRowFilter(false);
      for (QPushButton *button : dialog->findChildren<QPushButton *>())
        if (button->text() == "Replace")
          button->click();
      check(editor.pages[0].columns[0] == QVector<QString>({"1", "2", "3"}),
            "changing filters invalidates search matches before Replace");
      editor.rowFilterExpression = "row == 1";
      editor.refreshColumnRowFilter(false);
      dialog->close();
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      editor.pages[0].columns[0] = {"1", "2", "1"};
      editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
      acceptOptional("Fill Series", [](QDialog *dlg) {
        const auto edits = dlg->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        edits[0]->setText("9");
        edits[1]->setText("1");
      });
      editor.fillSeries(editor.columnView);
      check(editor.pages[0].columns[0][0] == "1", "Fill Series ignores a hidden current cell");
      editor.pages[0].columns[0][0] = "1";
      acceptOptional("Apply Numerical Expression", [](QDialog *dlg) {
        dlg->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[0]->setText("x + 10");
      });
      editor.applyNumericalExpression(editor.columnView);
      check(editor.pages[0].columns[0][0] == "1", "numerical expressions ignore a hidden current cell");
      editor.pages[0].columns[0][0] = "1";
      acceptOptional("Apply Text Formula", [](QDialog *dlg) {
        dlg->findChild<QLineEdit *>()->setText("8");
      });
      editor.applyTextFormula(editor.columnView);
      check(editor.pages[0].columns[0][0] == "1", "text formulas ignore a hidden current cell");
    }
    check(!validateTextForType(QString(QChar(0x03A9)), SDDS_CHARACTER, false),
          "character fields reject characters that cannot fit in one SDDS byte");
    check(validateTextForType(QString(QChar(0x00E9)), SDDS_CHARACTER, false),
          "character fields accept all Latin-1 bytes");
    {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.column_definition[0].type = SDDS_CHARACTER;
      require(SDDS_SaveLayout(&editor.dataset), "save character layout");
      editor.pages[0].columns[0] = {QString(QChar(0x03A9)), "a", "b"};
      const QString path = root + "/preserved-character.sdds";
      putFile(path, "original character bytes");
      check(!editor.writeFile(path) && readFile(path) == "original character bytes",
            "unrepresentable characters cannot silently overwrite saved data");
      require(SDDS_DefineParameter(&editor.dataset, "Text", nullptr, nullptr, nullptr, nullptr,
                                   SDDS_STRING, nullptr) >= 0, "define Unicode text parameter");
      require(SDDS_SaveLayout(&editor.dataset), "save text layout");
      editor.pages[0].columns[0].clear();
      for (int byte = 0; byte < 256; ++byte)
        editor.pages[0].columns[0].append(byte ? QString(QChar(byte)) : QString());
      bool loadedBytesSave = true;
      for (int byte = 1; byte < 256; ++byte)
        loadedBytesSave = loadedBytesSave &&
                          localEncodingPreserves(QString::fromLocal8Bit(QByteArray(1, char(byte))));
      check(loadedBytesSave, "text decoded from any file byte can be saved again");
      QString text = QString::fromUtf8("café Ω");
      if (!localEncodingPreserves(text)) {
        // A legacy code page such as Windows-1252 has no Ω; refuse rather than write '?'.
        editor.pages[0].parameters = {text};
        editor.populateModels();
        const QString textPath = root + "/preserved-text.sdds";
        putFile(textPath, "original text bytes");
        check(!validateTextForType(text, SDDS_STRING, false) &&
                  !editor.writeFile(textPath) && readFile(textPath) == "original text bytes",
              "text outside the system encoding cannot silently overwrite saved data");
        text = QString::fromUtf8("café");
        if (!localEncodingPreserves(text))
          text = "cafe";
      }
      editor.pages[0].parameters = {text};
      editor.populateModels();
      for (bool ascii : {false, true}) {
        editor.asciiBtn->setChecked(ascii);
        editor.binaryBtn->setChecked(!ascii);
        for (const QString &suffix : {QString(".sdds"), QString(".sdds.gz"), QString(".sdds.xz")}) {
          const QString output = root + QString("/character-bytes-%1").arg(ascii) + suffix;
          require(editor.writeFile(output), "save character bytes and Unicode strings");
          SDDSEditor loaded;
          require(loaded.loadFile(output), "reload character bytes and Unicode strings");
          check(loaded.pages[0].columns[0] == editor.pages[0].columns[0] &&
                loaded.pages[0].parameters == editor.pages[0].parameters,
                "all 256 character bytes and Unicode strings round-trip");
        }
      }
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.array_definition[0].dimensions = 33;
      require(SDDS_SaveLayout(&editor.dataset), "save high-rank array layout");
      editor.pages[0].arrays[0].dims = QVector<int>(33, 1);
      editor.pages[0].arrays[0].values = {"10"};
      editor.populateModels();
      const QString path = root + "/preserved-export.h5";
      putFile(path, "original export bytes");
      check(!editor.writeHDF(path) && readFile(path) == "original export bytes",
            "failed HDF export preserves the destination");
    }
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineColumn(&editor.dataset, ".", nullptr, nullptr, nullptr, nullptr,
                                SDDS_LONG, 0) >= 0, "define dot-named column");
      require(SDDS_DefineColumn(&editor.dataset, ".%2E", nullptr, nullptr, nullptr, nullptr,
                                SDDS_LONG, 0) >= 0, "define distinct encoded column");
      require(SDDS_SaveLayout(&editor.dataset), "save dot-named layout");
      editor.pages[0].columns.append({"4", "5", "6"});
      editor.pages[0].columns.append({"7", "8", "9"});
      editor.populateModels();
      const QString path = root + "/dot-names.h5";
      const bool saved = editor.writeHDF(path);
      check(saved, "HDF export supports the valid SDDS name dot");
      if (saved) {
        hid_t file = H5Fopen(QFile::encodeName(path).constData(), H5F_ACC_RDONLY, H5P_DEFAULT);
        require(file >= 0, "read dot-name HDF export");
        hid_t data = H5Dopen1(file, "page1/columns/%2E");
        require(data >= 0, "dot name has an encoded HDF dataset");
        int32_t values[3] = {};
        require(H5Dread(data, H5T_NATIVE_INT32, H5S_ALL, H5S_ALL, H5P_DEFAULT, values) >= 0 &&
                values[0] == 4 && values[2] == 6, "dot dataset contains its column values");
        H5Dclose(data);
        H5Fclose(file);
      }
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.column_definition[0].type = SDDS_DOUBLE;
      editor.dataset.layout.array_definition[0].type = SDDS_FLOAT;
      require(SDDS_SaveLayout(&editor.dataset), "save signed-zero types");
      editor.pages[0].columns[0] = {"-0", "0", "-0"};
      editor.pages[0].arrays[0].values = {"-0", "0", "-0", "0"};
      editor.populateModels();
      check(canonicalizeForDisplay("-0", SDDS_DOUBLE) == "-0" &&
            canonicalizeForDisplay("-0", SDDS_FLOAT) == "-0", "cell formatting preserves signed zero");
      for (bool ascii : {false, true}) {
        editor.asciiBtn->setChecked(ascii);
        editor.binaryBtn->setChecked(!ascii);
        const QString path = root + QString("/signed-zero-%1.sdds").arg(ascii);
        require(editor.writeFile(path), "save signed-zero fixture");
        SDDSEditor loaded;
        require(loaded.loadFile(path), "reload signed-zero fixture");
        check(loaded.pages[0].columns[0] == editor.pages[0].columns[0] &&
              loaded.pages[0].arrays[0].values == editor.pages[0].arrays[0].values,
              "file loading preserves negative zero in columns and arrays");
      }
    }
    require(failures == 0, "editing safety regressions");
  }

  /** Initialize a small dataset used by the integration tests. */
  static void setup(SDDSEditor &editor) {
    require(editor.ensureDataset(), "initialize editor");
    require(SDDS_DefineColumn(&editor.dataset, "X", nullptr, nullptr, nullptr,
                              nullptr, SDDS_LONG64, 0) >= 0, "define column");
    require(SDDS_DefineArray(&editor.dataset, "A", nullptr, nullptr, nullptr,
                             nullptr, SDDS_DOUBLE, 0, 2, nullptr) >= 0, "define array");
    require(SDDS_SaveLayout(&editor.dataset), "save test layout");
    editor.pages[0].columns = {{"3", "1", "2"}};
    ArrayStore array;
    array.dims = {2, 2};
    array.values = {"10", "20", "30", "40"};
    editor.pages[0].arrays = {array};
    editor.populateModels();
  }

  /** Verify actual widget geometry, since splitter sizes can hide unused gaps. */
  static void panelsFillSplitter(const SDDSEditor &viewer) {
    const int panels = viewer.paramBox->height() + viewer.colBox->height() + viewer.arrayBox->height();
    const int handles = viewer.dataSplitter->handle(1)->height() + viewer.dataSplitter->handle(2)->height();
    require(panels + handles == viewer.dataSplitter->contentsRect().height(),
            "panels fill all available splitter height without gaps");
  }

  /** Drag a real splitter handle through the same Qt mouse events as the UI. */
  static void dragPanelHandle(SDDSEditor &viewer, int index, int distance) {
    QSplitterHandle *handle = viewer.dataSplitter->handle(index);
    const QPoint start = handle->rect().center();
    const QPoint globalStart = handle->mapToGlobal(start);
    const QPoint globalEnd = globalStart + QPoint(0, distance);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(start), QPointF(globalStart),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(handle, &press);
    QMouseEvent move(QEvent::MouseMove, QPointF(start + QPoint(0, distance)), QPointF(globalEnd),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(handle, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(handle->mapFromGlobal(globalEnd)), QPointF(globalEnd),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(handle, &release);
    QCoreApplication::processEvents();
  }

  /** Automatic compact sizing must allow later manual expansion in both directions. */
  static void manualPanelSizing(SDDSEditor &viewer) {
    const int parameters = viewer.paramBox->height();
    dragPanelHandle(viewer, 1, 80);
    require(viewer.paramBox->height() == parameters + 80, "drag expands parameters beyond their content height");
    panelsFillSplitter(viewer);
    viewer.resize(viewer.width(), viewer.height() + 60);
    QCoreApplication::processEvents();
    require(viewer.paramBox->height() == parameters + 80, "window resize preserves expanded parameter size");
    panelsFillSplitter(viewer);
    dragPanelHandle(viewer, 1, -80);
    require(viewer.paramBox->height() == parameters, "drag restores compact parameter height");

    const int arrays = viewer.arrayBox->height();
    dragPanelHandle(viewer, 2, -80);
    require(viewer.arrayBox->height() == arrays + 80, "drag expands arrays even without array definitions");
    panelsFillSplitter(viewer);
    const int expandedParameters = viewer.paramBox->height();
    const int expandedArrays = viewer.arrayBox->height();
    viewer.resize(viewer.width(), viewer.height() + 60);
    QCoreApplication::processEvents();
    require(viewer.paramBox->height() == expandedParameters, "window resize preserves manual parameter size");
    if (viewer.arrayModel->rowCount() == 0)
      require(viewer.arrayBox->height() == expandedArrays, "window resize preserves manual empty array size");
    panelsFillSplitter(viewer);
    const int beforeShrink = viewer.arrayBox->height();
    dragPanelHandle(viewer, 2, 80);
    require(viewer.arrayBox->height() == beforeShrink - 80, "array panel can shrink after expansion");
    panelsFillSplitter(viewer);
  }

  /** Optionally check a supplied dataset without saving or modifying it. */
  static void filePanelSizing(const QString &path, const QString &root) {
    const QByteArray original = readFile(path);
    for (bool loadBeforeShow : {false, true}) {
      SDDSEditor viewer;
      if (!loadBeforeShow) {
        viewer.show();
        QCoreApplication::processEvents();
      }
      require(viewer.loadFile(path), "load supplied layout test file");
      viewer.show();
      QCoreApplication::processEvents();
      for (int growth : {0, 120}) {
        viewer.resize(viewer.width(), viewer.height() + growth);
        QCoreApplication::processEvents();
        fprintf(stdout, "supplied file %s (load before show=%d): splitter=%d, parameters=%d, columns=%d, arrays=%d\n",
                qPrintable(path), loadBeforeShow, viewer.dataSplitter->height(), viewer.paramBox->height(),
                viewer.colBox->height(), viewer.arrayBox->height());
        panelsFillSplitter(viewer);
        if (viewer.paramModel->rowCount() == 1 &&
            (viewer.columnModel->rowCount() > 0 || viewer.arrayModel->rowCount() > 0)) {
          const int rowHeight = viewer.paramView->rowHeight(0);
          require(viewer.paramView->viewport()->height() >= rowHeight &&
                  viewer.paramView->viewport()->height() <= rowHeight + 2,
                  "supplied file parameter row fits without unused rows");
        }
      }
      viewer.grab().save(root + QString("/supplied-file-%1.png").arg(loadBeforeShow));
      manualPanelSizing(viewer);
    }
    require(readFile(path) == original, "supplied file remains unchanged");
    fprintf(stdout, "PASS supplied file panel sizing\n");
  }

  /** Opening files gives unused parameter space to populated data panels. */
  static void panelSizing(const QString &root) {
    SDDSEditor viewer;
    viewer.show();
    QCoreApplication::processEvents();
    for (int scenario : {3, 1, 2, 0, 7, 5, 6, 4}) {
      const int mask = scenario & 3;
      SDDSEditor source;
      setup(source);
      require(SDDS_DefineParameter(&source.dataset, "Parameter", nullptr, nullptr,
                                   nullptr, nullptr, SDDS_STRING, nullptr) >= 0,
              "define panel sizing parameter");
      require(SDDS_SaveLayout(&source.dataset), "save panel sizing layout");
      source.pages[0].parameters = {"value"};
      if (!(mask & 1))
        source.pages[0].columns[0].clear();
      if (!(mask & 2)) {
        source.pages[0].arrays[0].dims = {0, 2};
        source.pages[0].arrays[0].values.clear();
      }
      if (!(scenario & 4)) {
        if (!(mask & 1)) {
          removeColumnFromLayout(&source.dataset.layout, 0);
          source.pages[0].columns.clear();
        }
        if (!(mask & 2)) {
          removeArrayFromLayout(&source.dataset.layout, 0);
          source.pages[0].arrays.clear();
        }
        require(SDDS_SaveLayout(&source.dataset), "save absent panel layout");
      }
      source.populateModels();
      const QString path = root + QString("/panels%1.sdds").arg(scenario);
      require(source.writeFile(path), "save panel sizing fixture");
      require(viewer.loadFile(path), "load panel sizing fixture");
      QCoreApplication::processEvents();
      viewer.grab().save(root + QString("/panels%1.png").arg(scenario));
      fprintf(stdout, "panels %d: splitter=%d, parameters=%d at %d, columns=%d at %d, arrays=%d at %d\n",
              scenario, viewer.dataSplitter->height(), viewer.paramBox->height(), viewer.paramBox->y(),
              viewer.colBox->height(), viewer.colBox->y(), viewer.arrayBox->height(), viewer.arrayBox->y());
      const int rowHeight = viewer.paramView->rowHeight(0);
      if (mask) {
        panelsFillSplitter(viewer);
        require(viewer.paramView->viewport()->height() >= rowHeight,
                "parameter row remains fully visible");
        require(viewer.paramView->viewport()->height() <= rowHeight + 2,
                "parameter panel has no unused rows");
        const int beforeParameters = viewer.paramBox->height();
        const int beforeColumns = viewer.colBox->height();
        const int beforeArrays = viewer.arrayBox->height();
        viewer.resize(viewer.width(), viewer.height() + 120);
        QCoreApplication::processEvents();
        panelsFillSplitter(viewer);
        require(viewer.paramBox->height() == beforeParameters,
                "window growth does not add blank parameter space");
        if (mask & 1)
          require(viewer.colBox->height() > beforeColumns, "populated columns receive extra space");
        else
          require(viewer.colBox->height() == beforeColumns, "empty columns do not receive extra space");
        if (mask & 2)
          require(viewer.arrayBox->height() > beforeArrays, "populated arrays receive extra space");
        else
          require(viewer.arrayBox->height() == beforeArrays, "empty arrays do not receive extra space");
        if (mask & 1)
          manualPanelSizing(viewer);
      } else {
        require(viewer.paramView->viewport()->height() > 3 * rowHeight,
                "parameter panel can expand when other panels have no data");
      }
    }
    fprintf(stdout, "PASS automatic panel sizing and manual splitter dragging\n");
  }

  /** Exercise slice mapping, editing, clipboard, pages and structural undo. */
  static void arrayViewer(const QString &root) {
    SDDSEditor editor;
    setup(editor);
    editor.dataset.layout.array_definition[0].dimensions = 3;
    require(SDDS_SaveLayout(&editor.dataset), "save 3D array layout");
    editor.pages[0].arrays[0].dims = {2, 3, 4};
    editor.pages[0].arrays[0].values.clear();
    for (int i = 0; i < 24; ++i)
      editor.pages[0].arrays[0].values.append(QString::number(i));
    editor.populateModels();
    editor.openArrayViewer(0);
    require(!editor.arrayViewers.isEmpty(), "open array viewer");
    ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
    ArraySliceModel *grid = viewer->sliceModel();
    require(!viewer->isModal(), "array viewer remains nonmodal");
    require(grid->rowCount() == 3 && grid->columnCount() == 4, "3D array defaults to last two axes");
    require(grid->index(2, 3).data().toString() == "11", "last SDDS dimension varies fastest");
    require(grid->headerData(0, Qt::Vertical).toString() == "0", "array headers use zero-based indices");
    viewer->findChild<QSpinBox *>("arraySliceIndex0")->setValue(1);
    require(grid->index(2, 3).data().toString() == "23", "slice navigator maps to next plane");
    require(grid->coordinates(grid->index(2, 3)) == QVector<int>({1, 2, 3}), "complete coordinate readout");
    require(grid->setData(grid->index(2, 3), "123.5"), "edit a slice cell");
    require(editor.pages[0].arrays[0].values[23] == "123.5" && editor.dirty, "slice edits update original data and dirty state");
    viewer->findChild<QSpinBox *>("arraySliceIndex0")->setValue(0);
    editor.undoStack->undo();
    require(editor.pages[0].arrays[0].values[23] == "23", "undo targets original element after slice change");
    editor.undoStack->redo();
    viewer->findChild<QComboBox *>("arrayRowDimension")->setCurrentIndex(2);
    require(grid->rowCount() == 4 && grid->columnCount() == 3, "selecting the column axis swaps grid axes");
    require(grid->index(3, 2).data().toString() == "11", "transposed grid preserves element mapping");
    viewer->findChild<QComboBox *>("arrayRowDimension")->setCurrentIndex(0);
    require(grid->rowCount() == 2 && grid->columnCount() == 3, "arbitrary row axis selection");
    viewer->findChild<QSpinBox *>("arraySliceIndex2")->setValue(3);
    require(grid->index(1, 2).data().toString() == "123.5", "slice coordinates survive arbitrary axis selection");
    applyCellEditWithUndo(editor.undoStack, editor.arrayModel, editor.arrayModel->index(23, 0), "99");
    require(grid->index(1, 2).data().toString() == "99", "main editor edits immediately appear in viewer");

    viewer->findChild<QComboBox *>("arrayRowDimension")->setCurrentIndex(1);
    viewer->findChild<QComboBox *>("arrayColumnDimension")->setCurrentIndex(2);
    viewer->table()->setCurrentIndex(grid->index(0, 1));
    viewer->table()->edit(grid->index(0, 1));
    QCoreApplication::processEvents();
    QLineEdit *cellEditor = viewer->table()->findChild<QLineEdit *>();
    require(cellEditor != nullptr, "slice grid opens an actual cell editor");
    cellEditor->setFocus();
    cellEditor->setText("77.25");
    viewer->findChild<QSpinBox *>("arraySliceIndex0")->setValue(1);
    require(editor.pages[0].arrays[0].values[1] == "77.25" && editor.pages[0].arrays[0].values[13] == "13",
            "slice navigation commits active edit to the original plane");
    editor.undoStack->undo();
    require(editor.pages[0].arrays[0].values[1] == "1", "undo restores the active editor's original element");
    viewer->findChild<QSpinBox *>("arraySliceIndex0")->setValue(0);
    viewer->table()->setCurrentIndex(grid->index(0, 0));
    const QVector<QString> before = editor.pages[0].arrays[0].values;
    require(viewer->pasteText("101\t102\n103\t104"), "paste rectangular slice selection");
    require(editor.pages[0].arrays[0].values[0] == "101" && editor.pages[0].arrays[0].values[5] == "104", "paste respects slice row stride");
    editor.undoStack->undo();
    require(editor.pages[0].arrays[0].values == before, "paste is one undo operation");
    require(!viewer->pasteText("100\tinvalid"), "reject invalid numeric paste");
    require(editor.pages[0].arrays[0].values == before, "invalid paste is atomic");
    viewer->table()->setCurrentIndex(grid->index(2, 3));
    require(!viewer->pasteText("1\t2"), "reject paste outside slice");
    viewer->copySelection(true);
    require(QApplication::clipboard()->text() == "0\t1\t2\t3\n4\t5\t6\t7\n8\t9\t10\t11", "copy current slice in displayed order");
    viewer->refresh();
    viewer->table()->setCurrentIndex(grid->index(2, 3));
    QCoreApplication::processEvents();
    viewer->grab().save(root + "/array-viewer-3d.png");
    const QString file = root + "/viewer-3d.sdds";
    require(editor.writeFile(file), "save viewer-edited array");
    SDDSEditor reloaded;
    require(reloaded.loadFile(file), "reload viewer-edited array");
    require(reloaded.pages[0].arrays[0].values[23] == "99", "viewer edits survive SDDS save and reload");

    acceptDialog("Resize Array", [](QDialog *dialog) {
      for (QSpinBox *box : dialog->findChildren<QSpinBox *>())
        box->setValue(1);
    });
    editor.resizeArray(0);
    require(grid->rowCount() == 1 && grid->columnCount() == 1, "viewer refreshes after array resize");
    editor.undoStack->undo();
    require(grid->rowCount() == 3 && grid->columnCount() == 4, "structural undo restores grid shape");
    editor.undoStack->undo();
    require(editor.pages[0].arrays[0].values[23] == "123.5", "cell undo still works after structural restoration");

    // Switching pages uses the same undo-history policy as the main editor.
    PageStore next = editor.pages[0];
    next.arrays[0].dims = {1, 2, 2};
    next.arrays[0].values = {"40", "41", "42", "43"};
    editor.pages.append(next);
    viewer->findChild<QSpinBox *>("arraySliceIndex0")->setValue(1);
    editor.pageChanged(1);
    require(grid->rowCount() == 2 && grid->columnCount() == 2, "viewer follows page shape changes");
    require(viewer->findChild<QSpinBox *>("arraySliceIndex0")->value() == 0, "smaller page clamps slice index");
    require(grid->index(1, 1).data().toString() == "43", "viewer reads selected page");
    require(!editor.undoStack->canUndo(), "page switch clears stale coordinate undo history");
    editor.pages[1].arrays[0].dims = {0, 2, 2};
    editor.pages[1].arrays[0].values.clear();
    editor.populateModels();
    require(grid->rowCount() == 0 && grid->columnCount() == 0, "empty slice exposes no editable cells");
    editor.pageChanged(0);
    viewer->table()->setCurrentIndex(grid->index(0, 0));
    viewer->table()->edit(grid->index(0, 0));
    QCoreApplication::processEvents();
    cellEditor = nullptr;
    for (QLineEdit *line : viewer->table()->findChildren<QLineEdit *>())
      if (!line->isHidden())
        cellEditor = line;
    require(cellEditor != nullptr, "open cell editor before closing viewer");
    cellEditor->setFocus();
    cellEditor->setText("500");
    QPointer<QDialog> closed = viewer;
    viewer->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    require(closed.isNull(), "closing viewer releases its window");
    require(editor.pages[0].arrays[0].values[0] == "500", "closing viewer commits the active cell");
    editor.undoStack->undo();
    require(editor.pages[0].arrays[0].values[0] == "0", "undo remains valid after viewer closes");

    // One-dimensional text arrays preserve embedded delimiters via clipboard.
    editor.dataset.layout.array_definition[0].dimensions = 1;
    editor.dataset.layout.array_definition[0].type = SDDS_STRING;
    require(SDDS_SaveLayout(&editor.dataset), "save 1D string layout");
    editor.pages[0].arrays[0].dims = {2};
    editor.pages[0].arrays[0].values = {"a\tb", "c\nd"};
    editor.populateModels();
    editor.openArrayViewer(0);
    viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
    grid = viewer->sliceModel();
    require(grid->rowCount() == 2 && grid->columnCount() == 1, "1D arrays use a single column");
    viewer->copySelection(true);
    const QString clipboard = QApplication::clipboard()->text();
    viewer->table()->setCurrentIndex(grid->index(0, 0));
    require(grid->setData(grid->index(0, 0), "changed"), "edit string slice");
    require(viewer->pasteText(clipboard), "paste lossless copied string array");
    require(editor.pages[0].arrays[0].values == QVector<QString>({"a\tb", "c\nd"}), "clipboard preserves tabs and newlines inside cells");

    // Arbitrary dimension counts use one control per undisplayed dimension.
    editor.dataset.layout.array_definition[0].dimensions = 4;
    require(SDDS_SaveLayout(&editor.dataset), "save 4D layout");
    editor.pages[0].arrays[0].dims = {2, 2, 2, 3};
    editor.pages[0].arrays[0].values.clear();
    for (int i = 0; i < 24; ++i)
      editor.pages[0].arrays[0].values.append(QString::number(i));
    editor.populateModels();
    viewer->findChild<QComboBox *>("arrayRowDimension")->setCurrentIndex(2);
    viewer->findChild<QComboBox *>("arrayColumnDimension")->setCurrentIndex(3);
    viewer->findChild<QSpinBox *>("arraySliceIndex0")->setValue(1);
    viewer->findChild<QSpinBox *>("arraySliceIndex1")->setValue(1);
    require(grid->index(1, 2).data().toString() == "23", "4D slicing maps both fixed dimensions correctly");
    editor.deleteArrayIndexes({0});
    require(grid->rowCount() == 0, "deleting an array disables its viewer");
    editor.undoStack->undo();
    require(grid->rowCount() > 0, "undoing deletion reconnects the named array");
    QPointer<QDialog> replaced = viewer;
    require(editor.loadFile(file), "replace document while viewer is open");
    require(replaced.isNull(), "loading another file closes stale viewers");
    fprintf(stdout, "PASS multidimensional array viewer mapping, edits, clipboard, pages, and lifecycle\n");
  }

  /** Verify heatmap scaling, special values, live edits, and display-only state. */
  static void arrayHeatmap(const QString &root) {
    SDDSEditor editor;
    setup(editor);
    editor.dataset.layout.array_definition[0].dimensions = 3;
    require(SDDS_SaveLayout(&editor.dataset), "save heatmap array layout");
    auto &values = editor.pages[0].arrays[0].values;
    editor.pages[0].arrays[0].dims = {2, 3, 4};
    values.clear();
    for (int i = 0; i < 24; ++i)
      values.append(QString::number(i));
    editor.populateModels();
    editor.openArrayViewer(0);
    ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
    ArraySliceModel *grid = viewer->sliceModel();
    QCheckBox *heatmap = viewer->findChild<QCheckBox *>("arrayHeatmap");
    QComboBox *scale = viewer->findChild<QComboBox *>("arrayHeatmapScale");
    QLineEdit *minimum = viewer->findChild<QLineEdit *>("arrayHeatmapMinimum");
    QLineEdit *maximum = viewer->findChild<QLineEdit *>("arrayHeatmapMaximum");
    QPushButton *apply = viewer->findChild<QPushButton *>("arrayHeatmapApply");
    QLabel *status = viewer->findChild<QLabel *>("arrayHeatmapStatus");
    auto color = [grid](int r, int c) { return qvariant_cast<QBrush>(grid->index(r, c).data(Qt::BackgroundRole)).color(); };
    require(!heatmap->isChecked() && !grid->index(0, 0).data(Qt::BackgroundRole).isValid(), "normal table is the default");
    editor.dirty = false;
    const auto original = values;
    const int history = editor.undoStack->count();
    heatmap->setChecked(true);
    require(minimum->text().toInt() == 0 && maximum->text().toInt() == 11, "automatic range scans the current slice only");
    const QColor low = color(0, 0), high = color(2, 3);
    require(low != high && color(1, 1) != low && color(1, 1) != high, "heatmap has low, middle and high colors");
    require(qvariant_cast<QBrush>(grid->index(0, 0).data(Qt::ForegroundRole)).color() == QColor(Qt::white) &&
            qvariant_cast<QBrush>(grid->index(2, 3).data(Qt::ForegroundRole)).color() == QColor(Qt::black), "heatmap text contrasts with dark and light cells");
    viewer->findChild<QSpinBox *>("arraySliceIndex0")->setValue(1);
    require(minimum->text().toInt() == 12 && maximum->text().toInt() == 23, "automatic range follows slice navigation");
    require(color(0, 0) == low && color(2, 3) == high, "automatic slices use the full color scale");
    scale->setCurrentIndex(1);
    require(minimum->text().toInt() == 12 && maximum->text().toInt() == 23, "fixed range starts with current slice limits");
    viewer->findChild<QSpinBox *>("arraySliceIndex0")->setValue(0);
    require(minimum->text().toInt() == 12 && maximum->text().toInt() == 23 && color(2, 3) == low, "fixed range persists and clips values below its minimum");
    minimum->setText("0");
    maximum->setText("23");
    apply->click();
    require(color(0, 0) == low && color(2, 3) != high, "manual limits control the color mapping");
    const QColor beforeInvalid = color(2, 3);
    minimum->setText("20");
    maximum->setText("10");
    apply->click();
    require(color(2, 3) == beforeInvalid && status->text().contains("previous range"), "reversed limits preserve the active range");
    minimum->setText("nan");
    maximum->setText("23");
    apply->click();
    require(color(2, 3) == beforeInvalid, "nonfinite limits are rejected");
    minimum->setText("0");
    apply->click();
    require(values == original && !editor.dirty && editor.undoStack->count() == history, "heatmap controls do not change document or undo history");
    require(grid->index(2, 3).data(Qt::ToolTipRole).toString().contains("= 11"), "heatmap tooltip retains the exact value");
    viewer->copySelection(true);
    require(QApplication::clipboard()->text().endsWith("8\t9\t10\t11"), "heatmap copy preserves numeric text");
    viewer->findChild<QComboBox *>("arrayRowDimension")->setCurrentIndex(2);
    require(color(3, 2) == beforeInvalid, "heatmap follows transposed axes");
    viewer->findChild<QComboBox *>("arrayRowDimension")->setCurrentIndex(1);
    scale->setCurrentIndex(0);
    require(grid->setData(grid->index(2, 3), "100"), "heatmap cells remain editable");
    QCoreApplication::processEvents();
    require(maximum->text().toInt() == 100 && color(2, 3) == high, "automatic range updates after edits");
    editor.undoStack->undo();
    QCoreApplication::processEvents();
    require(maximum->text().toInt() == 11, "undo updates heatmap scale");
    // Applying an edit can detach Qt vectors; access storage afresh after undo.
    editor.pages[0].arrays[0].values[0] = "";
    editor.pages[0].arrays[0].values[1] = "NaN";
    editor.pages[0].arrays[0].values[2] = "Inf";
    editor.pages[0].arrays[0].values[3] = "-Inf";
    editor.populateModels();
    require(minimum->text().toInt() == 4 && maximum->text().toInt() == 11, "missing and nonfinite values are excluded from the scale");
    const QColor missing = color(0, 0);
    require(missing == QColor(160, 160, 160) && color(0, 1) == missing && color(0, 2) == missing && color(0, 3) == missing, "all missing/nonfinite values share a distinct gray color");
    editor.pages[0].arrays[0].values.fill("7");
    editor.populateModels();
    require(minimum->text().toInt() == 7 && maximum->text().toInt() == 7 && color(0, 0) == color(2, 3) && color(0, 0) != missing, "constant slices use a valid uniform color");
    editor.pages[0].arrays[0].values.fill("NaN");
    editor.populateModels();
    require(minimum->text().isEmpty() && maximum->text().isEmpty() && color(1, 1) == missing && status->text().contains("no finite"), "all-nonfinite slices have no fabricated numeric range");
    // Preserve numeric precision in the parser and prevent overflow in normalization.
    editor.pages[0].arrays[0].values.fill("0");
    const long double extreme = std::numeric_limits<long double>::max() / 1.1L;
    char boundText[128];
    std::snprintf(boundText, sizeof(boundText), "%.*Lg", std::numeric_limits<long double>::max_digits10, extreme);
    const QString bound = QString::fromLatin1(boundText);
    editor.pages[0].arrays[0].values[0] = "-" + bound;
    editor.pages[0].arrays[0].values[11] = bound;
    editor.populateModels();
    require(color(0, 0) == low && color(2, 3) == high && color(1, 1) != missing, "extreme signed values normalize without overflow");
    const QString extremeMinimum = minimum->text(), extremeMaximum = maximum->text();
    scale->setCurrentIndex(1);
    minimum->setText(extremeMinimum);
    maximum->setText(extremeMaximum);
    apply->click();
    require(!status->text().contains("previous range") && color(0, 0) == low && color(2, 3) == high, "displayed extreme limits round-trip through fixed range controls");
    scale->setCurrentIndex(0);
    if (std::numeric_limits<long double>::digits >= 64) {
      editor.pages[0].arrays[0].values.fill("18446744073709551614");
      editor.pages[0].arrays[0].values[11] = "18446744073709551615";
      editor.populateModels();
      require(color(0, 0) == low && color(2, 3) == high, "adjacent uint64 values retain distinct colors");
      require(minimum->text() == "18446744073709551614" && maximum->text() == "18446744073709551615", "range controls retain full uint64 precision");
    }
    // A shape change to an empty page must clear automatic bounds safely.
    editor.pages[0].arrays[0].dims = {0, 3, 4};
    editor.pages[0].arrays[0].values.clear();
    editor.populateModels();
    require(grid->rowCount() == 0 && minimum->text().isEmpty(), "empty arrays have no heatmap range");
    editor.pages[0].arrays[0].dims = {2, 3, 4};
    for (int i = 0; i < 24; ++i)
      editor.pages[0].arrays[0].values.append(QString::number(i));
    editor.populateModels();
    // Page changes preserve user-specified color limits.
    scale->setCurrentIndex(1);
    minimum->setText("0");
    maximum->setText("23");
    apply->click();
    PageStore next = editor.pages[0];
    next.arrays[0].values.fill("100");
    editor.pages.append(next);
    editor.pageChanged(1);
    require(minimum->text().toInt() == 0 && maximum->text().toInt() == 23 && color(0, 0) == high, "fixed limits survive page changes and clip high values");
    editor.pageChanged(0);
    editor.dataset.layout.array_definition[0].type = SDDS_STRING;
    require(SDDS_SaveLayout(&editor.dataset), "save string type for heatmap gating");
    editor.populateModels();
    require(!heatmap->isEnabled() && !grid->index(0, 0).data(Qt::BackgroundRole).isValid(), "numeric-looking string arrays do not receive a heatmap");
    editor.dataset.layout.array_definition[0].type = SDDS_DOUBLE;
    require(SDDS_SaveLayout(&editor.dataset), "restore numeric heatmap type");
    editor.populateModels();
    require(heatmap->isEnabled() && grid->index(0, 0).data(Qt::BackgroundRole).isValid(), "restoring numeric type restores heatmap availability");
    heatmap->setChecked(false);
    require(!grid->index(0, 0).data(Qt::BackgroundRole).isValid() && !grid->index(0, 0).data(Qt::ForegroundRole).isValid(), "turning heatmap off restores ordinary table colors");
    heatmap->setChecked(true);
    viewer->table()->clearSelection();
    QCoreApplication::processEvents();
    viewer->grab().save(root + "/array-heatmap.png");
    require(editor.writeFile(root + "/heatmap-3d.sdds"), "save a usable heatmap sample");
    fprintf(stdout, "PASS array heatmap scaling, colors, edits, precision, pages, and numeric type gating\n");
  }

  /** A format-copy failure must leave the destination intact. */
  static void safeSave(const QString &root) {
    SDDSEditor editor;
    setup(editor);
    const QString path = root + "/preserved.sdds";
    putFile(path, "original bytes");
    require(replaceSharedLayoutString(&editor.dataset.layout.column_definition[0].format_string,
                                      &editor.dataset.original_layout.column_definition[0].format_string,
                                      "%s"), "install incompatible format");
    editor.dirty = true;
    require(!editor.writeFile(path), "invalid layout save fails");
    require(readFile(path) == "original bytes", "failed save preserves original bytes");
    require(editor.dirty, "failed save preserves dirty state");
    require(replaceSharedLayoutString(&editor.dataset.layout.column_definition[0].format_string,
                                      &editor.dataset.original_layout.column_definition[0].format_string,
                                      QString()), "clear incompatible format");
    for (const QString &suffix : {QString(".sdds"), QString(".sdds.gz"), QString(".sdds.xz")}) {
      for (bool ascii : {true, false}) {
        editor.asciiBtn->setChecked(ascii);
        editor.binaryBtn->setChecked(!ascii);
        const QString output = root + (ascii ? "/ascii" : "/binary") + suffix;
        require(editor.writeFile(output), "save plain or compressed output");
        SDDSEditor reloaded;
        require(reloaded.loadFile(output), "reload saved output");
        require(reloaded.pages[0].columns == editor.pages[0].columns, "column round trip");
        require(reloaded.pages[0].arrays[0].values == editor.pages[0].arrays[0].values, "array round trip");
      }
    }
#ifndef _WIN32
    putFile(root + "/version.001", "version one");
    putFile(root + "/version.002", "version two");
    require(QFile::link(root + "/version.001", root + "/current"), "create version link");
    require(editor.writeFile(root + "/current"), "save next unused version");
    require(readFile(root + "/version.001") == "version one", "preserve linked version");
    require(readFile(root + "/version.002") == "version two", "preserve intervening version");
    require(QFileInfo(root + "/current").symLinkTarget() == root + "/version.003", "link advances past collision");
#endif
    fprintf(stdout, "PASS transactional saves, compression, version collisions\n");
  }

  /** The insertion dialog's fixed value must survive commits and reloads. */
  static void fixedParameter(const QString &root) {
    SDDSEditor editor;
    setup(editor);
    acceptDialog("New Parameter", [](QDialog *dialog) {
      const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
      require(fields.size() == 6, "parameter dialog fields");
      fields[0]->setText("Fixed");
      fields[5]->setText("42");
      for (QRadioButton *button : dialog->findChildren<QRadioButton *>())
        if (button->text() == "long")
          button->setChecked(true);
    });
    editor.insertParameter();
    require(editor.pages[0].parameters[0] == "42", "fixed value initializes cell");
    editor.insertPage();
    editor.commitModels();
    for (const PageStore &page : editor.pages)
      require(page.parameters[0] == "42", "fixed value survives page insertion");
    require(editor.writeFile(root + "/fixed.sdds"), "save fixed parameter");
    SDDSEditor reloaded;
    require(reloaded.loadFile(root + "/fixed.sdds"), "reload fixed parameter");
    require(reloaded.pages[0].parameters[0] == "42", "fixed value round trip");
    fprintf(stdout, "PASS fixed parameter insertion\n");
  }

  /** Exercise cell edits across structural undo and redo, plus resize and sort. */
  static void undo() {
    SDDSEditor editor;
    setup(editor);
    applyCellEditWithUndo(editor.undoStack, editor.columnModel, editor.columnModel->index(0, 0), "9");
    acceptDialog("Insert Rows", [](QDialog *) {});
    editor.insertColumnRows();
    require(editor.pages[0].columns[0].size() == 4, "insert row");
    editor.undoStack->undo();
    editor.undoStack->undo();
    require(editor.pages[0].columns[0][0] == "3", "cell undo after structural undo");
    editor.undoStack->redo();
    editor.undoStack->redo();
    require(editor.pages[0].columns[0][0] == "9" && editor.pages[0].columns[0].size() == 4, "redo both operations");
    editor.undoStack->undo();
    const auto original = editor.pages[0].columns;
    editor.sortColumn(0, Qt::AscendingOrder);
    require(editor.pages[0].columns[0] == QVector<QString>({"1", "2", "9"}), "sort rows");
    editor.undoStack->undo();
    require(editor.pages[0].columns == original, "undo sort");
    editor.undoStack->redo();
    require(editor.pages[0].columns[0][0] == "1", "redo sort");
    acceptDialog("Resize Array", [](QDialog *dialog) {
      for (QSpinBox *box : dialog->findChildren<QSpinBox *>())
        box->setValue(1);
    });
    editor.resizeArray(0);
    require(editor.pages[0].arrays[0].values.size() == 1, "shrink array");
    editor.undoStack->undo();
    require(editor.pages[0].arrays[0].values == QVector<QString>({"10", "20", "30", "40"}), "undo restores truncated elements");
    editor.undoStack->redo();
    require(editor.pages[0].arrays[0].dims == QVector<int>({1, 1}), "redo resize");
    fprintf(stdout, "PASS cell and structural undo/redo, sort, resize\n");
  }

  /** Empty arrays remain empty through SDDS and HDF output. */
  static void emptyArrays(const QString &root) {
    SDDSEditor editor;
    setup(editor);
    require(dimProduct({0, -1}) == -1, "negative dimensions remain invalid");
    require(dimProduct({INT_MAX, INT_MAX, 0}) == 0, "zero dimension prevents spurious overflow");
    int n = 0;
    for (const QVector<int> &dims : {QVector<int>({0, 2}), QVector<int>({2, 0})}) {
      editor.pages[0].arrays[0].dims = dims;
      editor.pages[0].arrays[0].values.clear();
      for (bool ascii : {true, false}) {
        editor.asciiBtn->setChecked(ascii);
        editor.binaryBtn->setChecked(!ascii);
        const QString path = root + QString("/empty%1.sdds").arg(++n);
        require(editor.writeFile(path), "save empty array");
        SDDSEditor reloaded;
        require(reloaded.loadFile(path), "reload empty array");
        require(reloaded.pages[0].arrays[0].dims == dims && reloaded.pages[0].arrays[0].values.isEmpty(), "empty array round trip");
      }
      const QString hdf = root + QString("/empty%1.h5").arg(n);
      require(editor.writeHDF(hdf), "export empty array to HDF");
      hid_t file = H5Fopen(QFile::encodeName(hdf).constData(), H5F_ACC_RDONLY, H5P_DEFAULT);
      hid_t data = H5Dopen1(file, "/page1/arrays/A");
      hid_t space = H5Dget_space(data);
      require(H5Sget_simple_extent_npoints(space) == 0, "HDF array has zero elements");
      H5Sclose(space);
      H5Dclose(data);
      H5Fclose(file);
    }
    fprintf(stdout, "PASS empty arrays in SDDS and HDF\n");
  }

  /** Check both numeric range and exact integer comparisons in real filters. */
  static void filters() {
    SDDSEditor editor;
    setup(editor);
    editor.pages[0].columns[0] = {"9007199254740992", "9007199254740993", "18446744073709551615"};
    editor.populateModels();
    editor.rowFilterActive = true;
    editor.rowFilterExpression = "X == 9007199254740993";
    int visible = 0;
    require(editor.applyColumnRowFilter(nullptr, &visible) && visible == 1, "large integer equality is exact");
    require(editor.columnView->isRowHidden(0) && !editor.columnView->isRowHidden(1), "correct large integer selected");
    editor.rowFilterExpression = "X == 18446744073709551615";
    require(editor.applyColumnRowFilter(nullptr, &visible) && visible == 1, "unsigned maximum equality");
    long double value;
    if (std::numeric_limits<long double>::max_exponent10 > 400)
      require(parseNumericValueForFilter("1e400", &value) && value > 1e300L, "retain long double range");
    if (std::numeric_limits<long double>::digits > 53)
      require(parseNumericValueForFilter("1.000000000000000001", &value) && value > 1.0L, "retain long double precision");
    fprintf(stdout, "PASS exact integer and long double filters\n");
  }

  /** Changing the query, data, or page cannot reuse old search positions. */
  static void search() {
    SDDSEditor editor;
    setup(editor);
    editor.dataset.layout.column_definition[0].type = SDDS_STRING;
    editor.dataset.original_layout.column_definition[0].type = SDDS_STRING;
    editor.pages[0].columns[0] = {"abc", "zz", "tail"};
    editor.populateModels();
    editor.searchColumn(0);
    QDialog *dialog = editor.searchColumnDialog;
    require(dialog, "column search dialog opens");
    const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
    require(fields.size() == 2, "search fields");
    auto click = [dialog](const QString &label) {
      for (QPushButton *button : dialog->findChildren<QPushButton *>())
        if (button->text() == label) {
          button->click();
          return;
        }
      require(false, "search button exists");
    };
    fields[0]->setText("abc");
    click("Search");
    fields[0]->setText("zz");
    fields[1]->setText("new");
    click("Replace");
    require(editor.pages[0].columns[0][0] == "abc" && editor.pages[0].columns[0][1] == "new", "query change replaces correct text");
    fields[0]->setText("abc");
    click("Search");
    editor.columnModel->setData(editor.columnModel->index(0, 0), "gone");
    editor.columnModel->setData(editor.columnModel->index(2, 0), "abc");
    click("Replace");
    require(editor.pages[0].columns[0][0] == "gone" && editor.pages[0].columns[0][2] == "new", "data change invalidates matches");
    editor.pages.append(editor.pages[0]);
    editor.pageChanged(1);
    require(!dialog->isVisible(), "page change closes search dialog");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    editor.dataset.layout.array_definition[0].type = SDDS_STRING;
    editor.dataset.original_layout.array_definition[0].type = SDDS_STRING;
    editor.pages[1].arrays[0].values = {"abc", "zz", "tail", "end"};
    QTimer::singleShot(0, [&]() {
      for (QWidget *widget : QApplication::topLevelWidgets()) {
        QDialog *arrayDialog = qobject_cast<QDialog *>(widget);
        if (!arrayDialog || arrayDialog->windowTitle() != "Search Array")
          continue;
        const auto edits = arrayDialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        auto press = [arrayDialog](const QString &label) {
          for (QPushButton *button : arrayDialog->findChildren<QPushButton *>())
            if (button->text() == label)
              button->click();
        };
        edits[0]->setText("abc");
        press("Search");
        edits[0]->setText("zz");
        edits[1]->setText("new");
        press("Replace");
        arrayDialog->accept();
        return;
      }
      require(false, "array search dialog exists");
    });
    editor.searchArray(0);
    require(editor.pages[1].arrays[0].values[0] == "abc" && editor.pages[1].arrays[0].values[1] == "new", "array query change replaces correct text");
    fprintf(stdout, "PASS search query, data, and page invalidation\n");
  }

  /** Capture the actual QProcess input using the checked-in plot probe. */
  static void plot(const QString &root) {
    SDDSEditor editor;
    setup(editor);
    editor.currentFilename = root + "/disk.sdds";
    require(editor.writeFile(editor.currentFilename), "save plotting baseline");
    editor.pages[0].columns[0][0] = "999";
    require(replaceSharedLayoutString(&editor.dataset.layout.column_definition[0].name,
                                      &editor.dataset.original_layout.column_definition[0].name,
                                      "Renamed", false), "rename plotted column");
    require(resyncSortedIndexName(editor.dataset.layout.column_index, 1, 0,
                                  editor.dataset.layout.column_definition[0].name), "update plotted name index");
    require(resyncSortedIndexName(editor.dataset.original_layout.column_index, 1, 0,
                                  editor.dataset.original_layout.column_definition[0].name), "update saved name index");
    editor.markDirty();
    const QString capture = root + "/plot-capture.sdds";
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", QFile::encodeName(QCoreApplication::applicationDirPath() + "/test-bin") + QDir::listSeparator().toLatin1() + oldPath);
    qputenv("SDDSEDITOR_PLOT_CAPTURE", QFile::encodeName(capture));
    editor.plotColumn(0);
    QElapsedTimer timer;
    timer.start();
    while (!editor.findChildren<QProcess *>().isEmpty() && timer.elapsed() < 10000) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    qputenv("PATH", oldPath);
    qunsetenv("SDDSEDITOR_PLOT_CAPTURE");
    require(QFileInfo::exists(capture), "plot helper captures snapshot");
    require(editor.dirty, "plot preserves dirty state");
    SDDSEditor snapshot, disk;
    require(snapshot.loadFile(capture) && disk.loadFile(editor.currentFilename), "read plot snapshot and disk file");
    require(snapshot.pages[0].columns[0][0] == "999", "plot sees unsaved edit");
    char renamed[] = "Renamed";
    require(SDDS_GetColumnIndex(&snapshot.dataset, renamed) == 0, "plot sees unsaved rename");
    require(disk.pages[0].columns[0][0] == "3", "plot does not save document");
    const auto arguments = readFile(capture + ".args").split('\n');
    require(arguments.contains("-col=Renamed"), "plot arguments use current column name");
    for (const QByteArray &argument : arguments)
      if (argument.endsWith("plot.sdds"))
        require(!QFileInfo::exists(QString::fromUtf8(argument)), "plot snapshot cleaned after process exits");
    fprintf(stdout, "PASS plotting unsaved snapshot and cleanup\n");
  }

  /** Regressions found in the code review: parsing, undo, export, resize, sort, filters. */
  static void reviewFixes(const QString &root) {
    ExpressionContext ctx = {};
    long double value = 0;
    require(evaluateExpressionText("-2^2", ctx, &value) && value == -4.0L, "power binds tighter than unary minus");
    require(evaluateExpressionText("2^-1", ctx, &value) && value == 0.5L, "negative exponent");
    require(evaluateExpressionText("2^3^2", ctx, &value) && value == 512.0L, "power is right associative");
    require(evaluateExpressionText("-3*2", ctx, &value) && value == -6.0L, "leading minus still works");

    const double tenth = 0.1;
    const float tenthF = 0.1f;
    require(sddsValueToString(&tenth, 0, SDDS_DOUBLE) == "0.1", "doubles load as shortest exact text");
    require(sddsValueToString(&tenthF, 0, SDDS_FLOAT) == "0.1", "floats load as shortest exact text");
    require(QString("0.1").toDouble() == tenth && canonicalizeForDisplay("0.10000000000000001", SDDS_DOUBLE) == "0.1",
            "display canonicalization is shortest and exact");

    // The save format is not part of structural undo.
    {
      SDDSEditor editor;
      setup(editor);
      editor.binaryBtn->setChecked(true);
      acceptDialog("Insert Rows", [](QDialog *) {});
      editor.insertColumnRows();
      editor.undoStack->undo();
      require(editor.binaryBtn->isChecked() && !editor.asciiBtn->isChecked(), "undo keeps the binary save choice");
    }

    // HDF object names may not contain '/'.
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineParameter(&editor.dataset, "dnux/dp", nullptr, nullptr, nullptr,
                                   nullptr, SDDS_DOUBLE, nullptr) >= 0, "define slash parameter");
      require(SDDS_SaveLayout(&editor.dataset), "save slash layout");
      editor.pages[0].parameters = {"2.5"};
      editor.populateModels();
      const QString path = root + "/slash.h5";
      require(editor.writeHDF(path), "export names containing '/' to HDF");
      hid_t file = H5Fopen(QFile::encodeName(path).constData(), H5F_ACC_RDONLY, H5P_DEFAULT);
      require(file >= 0, "reopen HDF export");
      require(H5Lexists(file, "/page1/parameters/dnux%2Fdp", H5P_DEFAULT) > 0, "slash is percent-encoded");
      H5Fclose(file);
    }

    // Resizing keeps elements at their coordinates.
    {
      SDDSEditor editor;
      setup(editor);
      acceptDialog("Resize Array", [](QDialog *dialog) {
        const auto boxes = dialog->findChildren<QSpinBox *>();
        require(boxes.size() == 2, "two dimension boxes");
        boxes[0]->setValue(2);
        boxes[1]->setValue(3);
      });
      editor.resizeArray(0);
      require(editor.pages[0].arrays[0].values == QVector<QString>({"10", "20", "", "30", "40", ""}),
              "growing the last dimension keeps rows aligned");
    }

    // NaN sorts last in both directions.
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineColumn(&editor.dataset, "D", nullptr, nullptr, nullptr, nullptr, SDDS_DOUBLE, 0) >= 0,
              "define double column");
      require(SDDS_SaveLayout(&editor.dataset), "save double column");
      editor.pages[0].columns = {{"1", "2", "3", "4"}, {"3", "nan", "1", "2"}};
      editor.populateModels();
      editor.sortColumn(1, Qt::AscendingOrder);
      require(editor.pages[0].columns[1] == QVector<QString>({"1", "2", "3", "nan"}), "ascending sort puts NaN last");
      editor.sortColumn(1, Qt::DescendingOrder);
      require(editor.pages[0].columns[1] == QVector<QString>({"3", "2", "1", "nan"}), "descending sort puts NaN last");
    }

    // Edits never reach rows hidden by the row filter.
    {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      editor.activateWindow();
      editor.columnView->setFocus();
      QCoreApplication::processEvents();
      editor.rowFilterExpression = "X!=1";
      editor.rowFilterActive = true;
      editor.refreshColumnRowFilter(false);
      require(editor.columnView->isRowHidden(1), "filter hides the middle row");
      editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
      QApplication::clipboard()->setText("7\n8");
      editor.paste();
      require(editor.pages[0].columns[0] == QVector<QString>({"7", "1", "8"}), "paste skips hidden rows");
      editor.columnView->selectionModel()->select(
          QItemSelection(editor.columnModel->index(0, 0), editor.columnModel->index(2, 0)),
          QItemSelectionModel::ClearAndSelect);
      editor.deleteColumnRows();
      require(editor.pages[0].columns[0] == QVector<QString>({"1"}), "deleting rows keeps hidden rows");
    }

    // A fixed value must match the parameter type.
    {
      SDDSEditor editor;
      setup(editor);
      acceptDialog("New Parameter", [](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        fields[0]->setText("Bad");
        fields[5]->setText("abc");
        for (QRadioButton *button : dialog->findChildren<QRadioButton *>())
          if (button->text() == "long")
            button->setChecked(true);
      });
      editor.insertParameter();
      require(editor.dataset.layout.n_parameters == 0, "invalid fixed value is rejected");
    }

    // Columns without rows can still have their attributes edited.
    {
      SDDSEditor editor;
      setup(editor);
      editor.pages[0].columns[0].clear();
      editor.populateModels();
      require(editor.columnModel->rowCount() == 0, "column has no rows");
      acceptDialog("Column Attributes", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[2]->setText("mm");
      });
      editor.editColumnAttributesAt(0);
      require(QString(editor.dataset.layout.column_definition[0].units) == "mm", "zero-row column attributes");
    }
    fprintf(stdout, "PASS review fixes: expressions, number text, undo format, HDF names, resize, sort, filters\n");
  }

  /** Pasting a non-contiguous copy leaves cells that were not selected unchanged. */
  static void sparseClipboard() {
    SDDSEditor editor;
    setup(editor);
    editor.show();
    editor.activateWindow();
    editor.columnView->setFocus();
    QCoreApplication::processEvents();
    require(QApplication::focusWidget() == editor.columnView, "column table has focus for clipboard test");
    QItemSelectionModel *selection = editor.columnView->selectionModel();
    selection->clearSelection();
    selection->select(editor.columnModel->index(0, 0), QItemSelectionModel::Select);
    selection->select(editor.columnModel->index(2, 0), QItemSelectionModel::Select);
    editor.copy();
    require(QApplication::clipboard()->text() == "3\n\n2", "sparse copy keeps a text gap for other programs");
    editor.pages[0].columns[0] = {"7", "8", "9"};
    editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
    editor.paste();
    require(editor.pages[0].columns[0] == QVector<QString>({"3", "8", "2"}),
            "sparse paste leaves unselected cells unchanged");
    editor.undoStack->undo();
    require(editor.pages[0].columns[0] == QVector<QString>({"7", "8", "9"}), "sparse paste is one undo step");
    QApplication::clipboard()->setText("5\n\n6");
    editor.paste();
    require(editor.pages[0].columns[0] == QVector<QString>({"5", "", "6"}),
            "external text still pastes empty fields");

    editor.openArrayViewer(0);
    ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
    ArraySliceModel *grid = viewer->sliceModel();
    QItemSelectionModel *gridSelection = viewer->table()->selectionModel();
    gridSelection->clearSelection();
    gridSelection->select(grid->index(0, 0), QItemSelectionModel::Select);
    gridSelection->select(grid->index(1, 1), QItemSelectionModel::Select);
    viewer->copySelection(false);
    const QString copied = QApplication::clipboard()->text();
    require(copied == "10\t\n\t40", "array viewer sparse copy text");
    editor.pages[0].arrays[0].values = {"1", "2", "3", "4"};
    viewer->table()->setCurrentIndex(grid->index(0, 0));
    require(viewer->pasteText(copied), "array viewer sparse paste");
    require(editor.pages[0].arrays[0].values == QVector<QString>({"10", "2", "3", "40"}),
            "array viewer sparse paste leaves unselected cells unchanged");
    fprintf(stdout, "PASS sparse copy and paste\n");
  }

  /** Toolbar, panel headers, filter chip, status bar and themes reflect editor state. */
  static void interfaceChrome(const QString &root) {
    SDDSEditor editor;
    setup(editor);
    require(SDDS_DefineParameter(&editor.dataset, "Energy", nullptr, "MeV", "Beam energy",
                                 nullptr, SDDS_DOUBLE, nullptr) >= 0, "define described parameter");
    require(SDDS_SaveLayout(&editor.dataset), "save described parameter layout");
    editor.pages[0].parameters = {"7000"};
    editor.populateModels();
    editor.show();
    QCoreApplication::processEvents();
    require(!editor.dirty, "building the interface leaves the document unmodified");

    ParameterPageModel *params = editor.paramModel;
    require(params->columnCount() == 4, "parameter table shows type, units and description");
    require(params->index(0, ParameterPageModel::TypeColumn).data().toString() == "double", "parameter type column");
    require(params->index(0, ParameterPageModel::UnitsColumn).data().toString() == "MeV", "parameter units column");
    require(params->index(0, ParameterPageModel::DescriptionColumn).data().toString() == "Beam energy",
            "parameter description column");
    require(!editor.paramView->isColumnHidden(ParameterPageModel::UnitsColumn), "used units column is shown");
    const QModelIndex typeCell = params->index(0, ParameterPageModel::TypeColumn);
    require(!(params->flags(typeCell) & (Qt::ItemIsEditable | Qt::ItemIsSelectable)), "metadata cells are read-only");
    const int undoCount = editor.undoStack->count();
    require(!applyCellEditWithUndo(editor.undoStack, params, typeCell, "long"), "metadata edits are rejected");
    require(editor.undoStack->count() == undoCount, "rejected metadata edit adds no undo entry");
    editor.paramView->setCurrentIndex(typeCell);
    require(editor.paramView->currentIndex().column() == ParameterPageModel::ValueColumn,
            "clicking metadata keeps the value cell current");

    require(editor.columnModel->headerData(0, Qt::Horizontal, HeaderSubtitleRole).toString() == "long64",
            "column header subtitle shows type");
    require(editor.columnModel->headerData(0, Qt::Horizontal, Qt::TextAlignmentRole).toInt() ==
                int(Qt::AlignRight | Qt::AlignVCenter), "numeric columns align right");
    require(editor.arrayModel->headerData(0, Qt::Horizontal, HeaderSubtitleRole).toString().contains(
                QString("2%12").arg(QChar(0x00D7))), "array header subtitle shows dimensions");
    require(!editor.pagePrevBtn->isEnabled() && !editor.pageNextBtn->isEnabled(), "single page disables page arrows");
    require(editor.colBox->isChecked() && editor.arrayBox->isChecked(), "populated panels are expanded");

    editor.rowFilterExpression = "X>1";
    editor.rowFilterActive = true;
    editor.refreshColumnRowFilter(false);
    require(editor.filterChip->isVisibleTo(editor.colBox), "active filter shows the filter chip");
    require(editor.filterAction->isChecked(), "active filter checks the toolbar action");
    require(editor.visibleColumnRows == 2, "filter chip counts visible rows");
    require(editor.rowsStatusLabel->text() == "2 of 3 rows", "status bar shows filtered row count");
    editor.clearColumnRowFilter();
    require(!editor.filterChip->isVisibleTo(editor.colBox) && !editor.filterAction->isChecked(),
            "clearing the filter hides the chip");

    editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
    editor.columnSearchEdit->setText("2");
    editor.findInColumnPanel();
    require(editor.columnView->currentIndex().row() == 2, "column search selects the next match");
    require(editor.cellStatusLabel->text().startsWith("Row 3"), "status bar follows the current cell");

    editor.unreadMessages = 0;
    editor.message("checked");
    require(editor.messagesButton->text() == "Messages (1)", "hidden log counts unread messages");
    editor.messagesButton->setChecked(true);
    QCoreApplication::processEvents();
    require(editor.consoleDock->isVisible() && editor.messagesButton->text() == "Messages",
            "opening the log clears the unread count");

    editor.grab().save(root + "/interface-light.png");
    editor.applyTheme(true);
    QCoreApplication::processEvents();
    require(editor.darkPalette && QApplication::palette().color(QPalette::Window).lightness() < 128,
            "dark theme installs a dark palette");
    editor.grab().save(root + "/interface-dark.png");
    editor.applyTheme(false);
    require(!editor.dirty, "theme changes leave the document unmodified");
    fprintf(stdout, "PASS toolbar, panel headers, filter chip, status bar, and themes\n");
  }

  /** Accept a dialog once it is shown, including one opened after a context menu closes. */
  static void acceptDialogWhenShown(const QString &title, std::function<void(QDialog *)> configure) {
    QTimer *timer = new QTimer;
    auto attempts = std::make_shared<int>(0);
    QObject::connect(timer, &QTimer::timeout, [timer, attempts, title, configure]() {
      for (QWidget *widget : QApplication::topLevelWidgets()) {
        QDialog *dialog = qobject_cast<QDialog *>(widget);
        if (dialog && dialog->isVisible() && dialog->windowTitle() == title) {
          timer->stop();
          timer->deleteLater();
          configure(dialog);
          dialog->accept();
          return;
        }
      }
      require(++*attempts < 500, "expected editor dialog is shown");
    });
    timer->start(10);
  }

  /** Pick a context-menu entry through the menu's own keyboard activation. */
  static void chooseMenuItem(const QString &text) {
    QTimer::singleShot(0, [text]() {
      QMenu *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
      require(menu, "context menu is open");
      for (QAction *action : menu->actions()) {
        if (action->text() == text) {
          menu->setActiveAction(action);
          QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
          QApplication::sendEvent(menu, &press);
          return;
        }
      }
      require(false, "context menu item exists");
    });
  }

  /** Regressions for header menus, numeric text, filters and attribute encoding. */
  static void menuAndTextFixes() {
    // A model reset clears the selection silently; header menus must not reuse it.
    {
      SDDSEditor editor;
      setup(editor);
      for (const char *name : {"Y", "Z"})
        require(SDDS_DefineColumn(&editor.dataset, name, nullptr, nullptr, nullptr, nullptr,
                                  SDDS_LONG64, 0) >= 0, "define extra column");
      require(SDDS_SaveLayout(&editor.dataset), "save extra columns");
      editor.pages[0].columns = {{"1"}, {"2"}, {"3"}};
      editor.populateModels();
      editor.show();
      QCoreApplication::processEvents();
      QItemSelectionModel *selection = editor.columnView->selectionModel();
      selection->select(editor.columnModel->index(0, 1), QItemSelectionModel::Select);
      selection->select(editor.columnModel->index(0, 2), QItemSelectionModel::Select);
      editor.populateModels();
      require(selection->selectedIndexes().isEmpty(), "model reset clears the selection");
      chooseMenuItem("Delete");
      editor.showColumnMenu(editor.columnView, 1, editor.columnView->mapToGlobal(QPoint(5, 5)));
      require(editor.dataset.layout.n_columns == 2 &&
              QString(editor.dataset.layout.column_definition[1].name) == "Z",
              "header delete after a reset removes only the clicked column");
    }

    // Formula tools started from a header menu act on that table, not the focused one.
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineParameter(&editor.dataset, "P", nullptr, nullptr, nullptr, nullptr,
                                   SDDS_DOUBLE, nullptr) >= 0, "define focus parameter");
      require(SDDS_SaveLayout(&editor.dataset), "save focus parameter");
      editor.pages[0].parameters = {"1"};
      editor.populateModels();
      editor.show();
      editor.activateWindow();
      editor.paramView->setFocus();
      QCoreApplication::processEvents();
      require(QApplication::focusWidget() == editor.paramView, "parameter table has focus");
      editor.columnView->selectionModel()->select(
          QItemSelection(editor.columnModel->index(0, 0), editor.columnModel->index(2, 0)),
          QItemSelectionModel::ClearAndSelect);
      chooseMenuItem("Fill Series...");
      acceptDialogWhenShown("Fill Series", [](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        fields[0]->setText("5");
        fields[1]->setText("1");
      });
      editor.showColumnMenu(editor.columnView, 0, editor.columnView->mapToGlobal(QPoint(5, 5)));
      require(editor.pages[0].columns[0] == QVector<QString>({"5", "6", "7"}) &&
              editor.pages[0].parameters[0] == "1",
              "header menu fill series fills the column selection");
    }

    // Computed values keep the precision of the target type.
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineColumn(&editor.dataset, "D", nullptr, nullptr, nullptr, nullptr,
                                SDDS_DOUBLE, 0) >= 0, "define double column");
      require(SDDS_SaveLayout(&editor.dataset), "save double column");
      editor.pages[0].columns = {{"1", "2", "3"}, {"0.30000000000000004", "0.1", "1.2345678901234567e+300"}};
      editor.populateModels();
      editor.show();
      editor.activateWindow();
      editor.columnView->setFocus();
      QCoreApplication::processEvents();
      editor.columnView->selectionModel()->select(
          QItemSelection(editor.columnModel->index(0, 1), editor.columnModel->index(2, 1)),
          QItemSelectionModel::ClearAndSelect);
      acceptDialogWhenShown("Apply Numerical Expression", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[0]->setText("x");
      });
      editor.applyNumericalExpressionSelection();
      require(editor.pages[0].columns[1] == QVector<QString>({"0.30000000000000004", "0.1", "1.2345678901234567e+300"}),
              "identity expression leaves doubles unchanged");
      editor.columnView->selectionModel()->select(
          QItemSelection(editor.columnModel->index(0, 0), editor.columnModel->index(2, 0)),
          QItemSelectionModel::ClearAndSelect);
      acceptDialogWhenShown("Fill Series", [](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        fields[0]->setText("100000000000000000");
        fields[1]->setText("64");
      });
      editor.fillSeries(editor.columnView);
      require(editor.pages[0].columns[0] ==
                  QVector<QString>({"100000000000000000", "100000000000000064", "100000000000000128"}),
              "large integer series is written as integers");
      require(numericResultText(0.1f, SDDS_FLOAT) == "0.1", "float results use shortest text");
      long double tooLargeForFloat = 0;
      require(parseLongDoubleStrict("1e40", &tooLargeForFloat), "parse float overflow value");
      require(!validateTextForType(numericResultText(tooLargeForFloat, SDDS_FLOAT), SDDS_FLOAT, false),
              "float overflow is rejected rather than turned into infinity");
    }

    // Empty text is not the number zero in string comparisons.
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineColumn(&editor.dataset, "S", nullptr, nullptr, nullptr, nullptr,
                                SDDS_STRING, 0) >= 0, "define string column");
      require(SDDS_SaveLayout(&editor.dataset), "save string column");
      editor.pages[0].columns = {{"", "1", "2"}, {"0", "", "a"}};
      editor.populateModels();
      editor.rowFilterActive = true;
      int visible = 0;
      editor.rowFilterExpression = "S == \"\"";
      require(editor.applyColumnRowFilter(nullptr, &visible) && visible == 1 &&
              !editor.columnView->isRowHidden(1), "empty string matches only empty cells");
      editor.rowFilterExpression = "S == \"0\"";
      require(editor.applyColumnRowFilter(nullptr, &visible) && visible == 1 &&
              !editor.columnView->isRowHidden(0), "zero string matches only zero");
      editor.rowFilterExpression = "X == 0";
      require(editor.applyColumnRowFilter(nullptr, &visible) && visible == 1 &&
              !editor.columnView->isRowHidden(0), "empty numeric cells compare as their saved zero");
      editor.rowFilterExpression = "X < 1";
      require(editor.applyColumnRowFilter(nullptr, &visible) && visible == 1, "empty numeric cells order as zero");
    }

    // Accepting an attribute dialog unchanged keeps non-ASCII text byte for byte.
    {
      SDDSEditor editor;
      setup(editor);
      const QString micro = QString(QChar(0x00B5)) + "m";
      require(replaceSharedLayoutString(&editor.dataset.layout.column_definition[0].units,
                                        &editor.dataset.original_layout.column_definition[0].units,
                                        micro), "set non-ASCII units");
      const QByteArray before(editor.dataset.layout.column_definition[0].units);
      acceptDialog("Column Attributes", [](QDialog *) {});
      editor.editColumnAttributesAt(0);
      require(QByteArray(editor.dataset.layout.column_definition[0].units) == before,
              "attribute dialog preserves non-ASCII units");
    }
    fprintf(stdout, "PASS header menus, computed number text, empty filter values, attribute encoding\n");
  }

  /** Regressions for parameter metadata display, export names and tiny long doubles. */
  static void displayAndRangeFixes() {
    // Units and descriptions are text even when the parameter value is numeric.
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineParameter(&editor.dataset, "P", nullptr, "1E3", "0.50", nullptr,
                                   SDDS_DOUBLE, nullptr) >= 0, "define described parameter");
      require(SDDS_SaveLayout(&editor.dataset), "save described parameter");
      editor.pages[0].parameters = {"0.10"};
      editor.populateModels();
      SDDSItemDelegate *delegate = static_cast<SDDSItemDelegate *>(editor.paramView->itemDelegate());
      auto shown = [&](int column) {
        QStyleOptionViewItem option;
        delegate->initStyleOption(&option, editor.paramModel->index(0, column));
        return option.text;
      };
      require(shown(ParameterPageModel::ValueColumn) == "0.1", "parameter value uses numeric display");
      require(shown(ParameterPageModel::UnitsColumn) == "1E3", "parameter units are shown verbatim");
      require(shown(ParameterPageModel::DescriptionColumn) == "0.50", "parameter description is shown verbatim");
    }

    // Export dialogs never propose the open SDDS file as the destination.
    require(exportDefaultPath("C:/data/run.sdds", ".h5") == "C:/data/run.h5", "HDF export proposes an .h5 name");
    require(exportDefaultPath("C:/data/run.sdds", ".csv") == "C:/data/run.csv", "CSV export proposes a .csv name");
    require(exportDefaultPath(QString(), ".h5").isEmpty(), "untitled export has no proposed name");

    // Subnormal long doubles load, validate, save and evaluate like other finite values.
    {
      const long double tiny = std::numeric_limits<long double>::denorm_min();
      const QString text = sddsValueToString(&tiny, 0, SDDS_LONGDOUBLE);
      long double parsed = 0;
      require(parseLongDoubleStrict(text, &parsed) && parsed == tiny, "subnormal long double parses");
      require(validateTextForType(text, SDDS_LONGDOUBLE, false), "subnormal long double is valid");
      ExpressionContext ctx = {};
      long double result = 0;
      require(evaluateExpressionText(text, ctx, &result) && result == tiny, "subnormal literal evaluates");
      require(!parseLongDoubleStrict("1e999999", &parsed), "long double overflow is rejected");
      require(!parseLongDoubleStrict("1e-999999", &parsed), "long double underflow to zero is rejected");
    }
    // Attribute text the system encoding cannot hold is refused instead of saved as '?'.
    const QString omega(QChar(0x03A9));
    if (!localEncodingPreserves(omega)) {
      SDDSEditor editor;
      setup(editor);
      acceptDialog("Column Attributes", [omega](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[2]->setText(omega);
      });
      editor.editColumnAttributesAt(0);
      require(!editor.dataset.layout.column_definition[0].units && !editor.undoStack->canUndo(),
              "unencodable column units are refused");
    }
    fprintf(stdout, "PASS parameter metadata display, export names, subnormal long doubles, attribute encoding\n");
  }
};

/** Run the named regressions and retain fixtures under the build directory. */
int main(int argc, char **argv) {
  setbuf(stdout, nullptr);
  fprintf(stdout, "sddseditor_tests: initializing Qt offscreen\n");
  QApplication app(argc, argv);
  setlocale(LC_NUMERIC, "C"); // Match main.cc: SDDS numbers always use a decimal point.
  QTemporaryDir artifacts(QCoreApplication::applicationDirPath() + "/test-artifacts-XXXXXX");
  require(artifacts.isValid(), "create test artifacts in object directory");
  artifacts.setAutoRemove(false);
  fprintf(stdout, "sddseditor_tests artifacts: %s\n", qPrintable(artifacts.path()));
  QTimer warnings;
  QObject::connect(&warnings, &QTimer::timeout, []() {
    for (QWidget *widget : QApplication::topLevelWidgets())
      if (QMessageBox *box = qobject_cast<QMessageBox *>(widget)) {
        fprintf(stdout, "dialog: %s\n", qPrintable(box->text()));
        box->accept();
      }
  });
  warnings.start(10);
  SDDSEditorTests::editingSafety(artifacts.path());
  SDDSEditorTests::panelSizing(artifacts.path());
  const QString layoutInput = QFile::decodeName(qgetenv("SDDSEDITOR_LAYOUT_INPUT"));
  if (!layoutInput.isEmpty())
    SDDSEditorTests::filePanelSizing(layoutInput, artifacts.path());
  SDDSEditorTests::arrayViewer(artifacts.path());
  SDDSEditorTests::arrayHeatmap(artifacts.path());
  SDDSEditorTests::safeSave(artifacts.path());
  SDDSEditorTests::fixedParameter(artifacts.path());
  SDDSEditorTests::undo();
  SDDSEditorTests::emptyArrays(artifacts.path());
  SDDSEditorTests::filters();
  SDDSEditorTests::search();
  SDDSEditorTests::plot(artifacts.path());
  SDDSEditorTests::reviewFixes(artifacts.path());
  SDDSEditorTests::sparseClipboard();
  SDDSEditorTests::interfaceChrome(artifacts.path());
  SDDSEditorTests::menuAndTextFixes();
  SDDSEditorTests::displayAndRangeFixes();
  fprintf(stdout, "PASS all sddseditor regressions\n");
  return 0;
}
