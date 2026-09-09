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
};

/** Run the named regressions and retain fixtures under the build directory. */
int main(int argc, char **argv) {
  setbuf(stdout, nullptr);
  fprintf(stdout, "sddseditor_tests: initializing Qt offscreen\n");
  QApplication app(argc, argv);
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
  SDDSEditorTests::panelSizing(artifacts.path());
  const QString layoutInput = QFile::decodeName(qgetenv("SDDSEDITOR_LAYOUT_INPUT"));
  if (!layoutInput.isEmpty())
    SDDSEditorTests::filePanelSizing(layoutInput, artifacts.path());
  SDDSEditorTests::safeSave(artifacts.path());
  SDDSEditorTests::fixedParameter(artifacts.path());
  SDDSEditorTests::undo();
  SDDSEditorTests::emptyArrays(artifacts.path());
  SDDSEditorTests::filters();
  SDDSEditorTests::search();
  SDDSEditorTests::plot(artifacts.path());
  fprintf(stdout, "PASS all sddseditor regressions\n");
  return 0;
}
