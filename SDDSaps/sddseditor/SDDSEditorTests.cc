/**
 * @file SDDSEditorTests.cc
 * @brief Offscreen regression tests for editor persistence and editing behavior.
 * @details Includes the implementation to test its internal models and parsers
 * without adding a diagnostic interface to the application.
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license See LICENSE in the repository root.
 */
// Match SDDSlib's compression configuration when constructing malformed fixtures.
#ifndef zLib
#define zLib
#endif
#include "SDDSEditor.cc"
extern "C" {
#include "../../SDDSlib/SDDS_internal.h"
}
#undef QMessageBox
#undef QInputDialog
#include <QElapsedTimer>
#include <QBrush>
#include <QCheckBox>
#include <clocale>
#ifndef _WIN32
#include <cerrno>
#include <sys/wait.h>
#endif

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

/** Write invalid headers through the compression APIs used by the SDDS reader. */
static void putCompressedFile(const QString &path, const QByteArray &contents) {
  const QByteArray name = path.toLocal8Bit();
  if (path.endsWith(".gz")) {
    gzFile file = gzopen(name.constData(), "wb");
    require(file && gzwrite(file, contents.constData(), static_cast<unsigned>(contents.size())) == contents.size(),
            "write gzip header fixture");
    require(gzclose(file) == Z_OK, "close gzip header fixture");
  } else if (path.endsWith(".xz")) {
    auto *file = static_cast<struct lzmafile *>(lzma_open(name.constData(), "wb"));
    require(file && lzma_write(file, contents.constData(), static_cast<size_t>(contents.size())) == contents.size(),
            "write xz header fixture");
    require(lzma_close(file) == 0, "close xz header fixture");
  } else {
    putFile(path, contents);
  }
}

/** Create a real symbolic link; Windows allows this with Developer Mode or elevation. */
static bool makeSymbolicLink(const QString &target, const QString &link) {
#ifdef _WIN32
  const std::wstring from = QDir::toNativeSeparators(link).toStdWString();
  const std::wstring to = QDir::toNativeSeparators(target).toStdWString();
  return CreateSymbolicLinkW(from.c_str(), to.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
#else
  return QFile::link(target, link);
#endif
}

/** Accepts warnings so unattended tests never block; a test that answers a prompt itself pauses it. */
static QTimer *messageBoxAccepter = nullptr;

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
  /** User-entered formulas must not exhaust the process stack. */
  static void expressionNesting() {
    const QString nested = QString(1000000, QLatin1Char('(')) + "1" +
                           QString(1000000, QLatin1Char(')'));
    ExpressionContext context = {};
    long double number = 0;
    require(!evaluateExpressionText(nested, context, &number),
            "excessively nested numerical expressions are rejected safely");
    ExactIntegerContext integerContext = {};
    ExactInteger integer = exactInteger(false, 0);
    require(!ExactIntegerExpressionParser(nested, integerContext).parse(&integer),
            "excessively nested exact integer expressions are rejected safely");
    bool pass = false;
    QString error;
    RowFilterParser filter(nested, [](const QString &, bool, QString *) { return false; });
    require(!filter.parse(&pass, &error) && !error.isEmpty(),
            "excessively nested row filters report an error safely");
    require(evaluateExpressionText("-2^2 + abs(-3)", context, &number) && number == -1,
            "ordinary expression precedence and nested functions remain valid");
    require(!evaluateExpressionText(QString(1000, QLatin1Char('-')) + "1", context, &number),
            "unary signs respect the nesting limit");
    QString powers;
    for (int i = 0; i < 1000; ++i)
      powers += "1^";
    require(!evaluateExpressionText(powers + "1", context, &number),
            "right-associative powers respect the nesting limit");
    require(!ExactIntegerExpressionParser(QString(1000, QLatin1Char('-')) + "1", integerContext).parse(&integer),
            "exact integer unary signs respect the nesting limit");
    RowFilterParser negations(QString(1000, QLatin1Char('!')) + "true",
                              [](const QString &, bool, QString *) { return false; });
    require(!negations.parse(&pass, &error), "filter negations respect the nesting limit");
    fprintf(stdout, "PASS bounded expression and filter nesting\n");
  }

  /** Integer fill must not accept fractions rounded away by native long double. */
  static void integerSeriesPrecision() {
    SDDSEditor editor;
    setup(editor);
    editor.dataset.layout.column_definition[0].type = SDDS_ULONG64;
    require(SDDS_SaveLayout(&editor.dataset), "save unsigned series layout");
    editor.columnView->selectColumn(0);
    const int bits = std::numeric_limits<long double>::digits;
    const QString boundary = bits <= 64
        ? QString::number(quint64(1) << (bits - 1)) : QString();
    if (!boundary.isEmpty()) {
      acceptDialog("Fill Series", [boundary](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>();
        fields[0]->setText(boundary + ".25");
        fields[1]->setText("0");
      });
      editor.fillSeries(editor.columnView);
      require(editor.pages[0].columns[0] == QVector<QString>({"3", "1", "2"}) &&
                  editor.undoStack->count() == 0 && !editor.dirty,
              "fractional series input cannot round into a valid large integer");
    }
    acceptDialog("Fill Series", [](QDialog *dialog) {
      const auto fields = dialog->findChildren<QLineEdit *>();
      fields[0]->setText("18446744073709551613");
      fields[1]->setText("1");
    });
    editor.fillSeries(editor.columnView);
    require(editor.pages[0].columns[0] == QVector<QString>({"18446744073709551613",
                "18446744073709551614", "18446744073709551615"}),
            "exact integer series still reaches the unsigned maximum");
    editor.undoStack->undo();
    acceptDialog("Fill Series", [](QDialog *dialog) {
      const auto fields = dialog->findChildren<QLineEdit *>();
      fields[0]->setText("1.0");
      fields[1]->setText("2.0");
    });
    editor.fillSeries(editor.columnView);
    require(editor.pages[0].columns[0] == QVector<QString>({"1", "3", "5"}),
            "ordinary decimal fill inputs retain their behavior");
    fprintf(stdout, "PASS integer fill series precision and exact upper bound\n");
  }

  /** The xz line reader must preserve blank lines, final bytes and EOF semantics. */
  static void xzInputLines(const QString &root) {
    const QString path = root + "/line-reader.xz";
    putCompressedFile(path, "\n \nfirst\nlast");
    auto *file = static_cast<struct lzmafile *>(lzma_open(path.toLocal8Bit().constData(), "rb"));
    require(file, "open xz line reader fixture");
    char buffer[32];
    for (const QByteArray &expected : {QByteArray("\n"), QByteArray(" \n"), QByteArray("first\n"), QByteArray("last")}) {
      require(lzma_gets(buffer, sizeof(buffer), file) && QByteArray(buffer) == expected,
              "xz lines preserve blank lines and a final line without a newline");
    }
    require(!lzma_gets(buffer, sizeof(buffer), file) && !lzma_gets(buffer, sizeof(buffer), file),
            "xz line reader returns null consistently at EOF");
    require(lzma_close(file) == 0, "close xz line reader fixture");
    file = static_cast<struct lzmafile *>(lzma_open(path.toLocal8Bit().constData(), "rb"));
    require(file, "reopen xz fixture for bounded reads");
    QByteArray contents;
    while (lzma_gets(buffer, 3, file))
      contents += buffer;
    require(contents == "\n \nfirst\nlast", "bounded xz line reads preserve every byte");
    require(lzma_close(file) == 0, "close bounded xz line reader fixture");

    for (const QString &suffix : {QString(".sdds"), QString(".sdds.gz"), QString(".sdds.xz")}) {
      const QString input = root + "/blank-page-separator" + suffix;
      putCompressedFile(input, "SDDS1\n&column name=X, type=long, &end\n"
                               "&data mode=ascii, no_row_counts=1, &end\n1\n2\n\n3\n4");
      SDDSEditor editor;
      require(editor.loadFile(input), "load ASCII pages separated by a blank line");
      require(editor.pages.size() == 2 && editor.pages[0].columns[0] == QVector<QString>({"1", "2"}) &&
                  editor.pages[1].columns[0] == QVector<QString>({"3", "4"}),
              "compressed input preserves blank page separators and final rows");
    }
    fprintf(stdout, "PASS xz line boundaries, final rows, page separators and EOF\n");
  }

  /** Failed SDDS initialization must release its stream as well as preserve the current document. */
  static void failedInputCleanup(const QString &root) {
    const QString probePath = root + "/descriptor-probe.txt";
    putFile(probePath, "descriptor probe");
    auto nextDescriptor = [&]() {
      FILE *file = std::fopen(probePath.toLocal8Bit().constData(), "rb");
      require(file, "open stream descriptor probe");
#ifdef _WIN32
      const int descriptor = _fileno(file);
#else
      const int descriptor = fileno(file);
#endif
      std::fclose(file);
      return descriptor;
    };
    SDDSEditor editor;
    setup(editor);
    const QString included = root + "/invalid-include.sdds";
    putFile(included, "&column name=X, type=invalid, &end\n");
    const QVector<QByteArray> headers = {
        "SDDS1\n&column name=X, type=long, &end\n",
        "SDDS1\n&column name=X, type=long, &end\n&data mode=invalid, &end\n",
        "SDDS1\n!# big-endian\n!# little-endian\n&data mode=ascii, &end\n",
        "SDDS1\n&include filename=\"" + included.toLocal8Bit() + "\", &end\n"};
    for (const QString &suffix : {QString(".sdds"), QString(".sdds.gz"), QString(".sdds.xz")}) {
      for (int fixture = 0; fixture < headers.size(); ++fixture) {
        const QString path = root + QString("/invalid-header-%1").arg(fixture) + suffix;
        putCompressedFile(path, headers[fixture]);
        const int before = nextDescriptor();
        for (int attempt = 0; attempt < 3; ++attempt)
          require(!editor.loadFile(path), "reject malformed input header");
        require(editor.pages[0].columns[0] == QVector<QString>({"3", "1", "2"}) && !editor.dirty,
                "failed input preserves the current document and saved state");
        const int after = nextDescriptor();
        fprintf(stdout, "%s failed input releases stream descriptors (%d -> %d): %s\n",
                before == after ? "PASS" : "FAIL", before, after, qPrintable(path));
        require(before == after, "failed input must not leak file descriptors");
      }
    }
#ifndef _WIN32
    // Exercise the same popen ownership used for legacy decompressor streams.
    // The shell reports its PID so the check cannot reap an unrelated child.
    for (const char *command : {"printf '%s\\nnot SDDS\\n' \"$$\"",
                                "printf '%s\\nSDDS1\\n&data mode=invalid, &end\\n' \"$$\""}) {
      fprintf(stdout, "pipe cleanup fixture: %s\n", command);
      FILE *stream = popen(command, "r");
      require(stream, "open malformed layout pipe fixture");
      char pidLine[32];
      long childPid = 0;
      require(fgets(pidLine, sizeof(pidLine), stream) && sscanf(pidLine, "%ld", &childPid) == 1 && childPid > 0,
              "read malformed layout pipe child PID");
      SDDS_DATASET partial = {};
      partial.layout.fp = stream;
      partial.layout.popenUsed = 1;
      require(!SDDS_ReadLayout(&partial, stream), "reject malformed layout from a process pipe");
      require(!partial.layout.fp, "failed pipe layout clears the stream handle");
      int status;
      pid_t waited;
      do {
        errno = 0;
        waited = waitpid(static_cast<pid_t>(childPid), &status, 0);
      } while (waited == -1 && errno == EINTR);
      const int waitError = errno;
      require(terminateIncompleteDataset(&partial), "terminate failed process-pipe input");
      SDDS_ClearErrors();
      require(waited == -1 && waitError == ECHILD, "failed pipe layout must reap its child process");
    }
    fprintf(stdout, "PASS failed process-pipe input reaps its child processes\n");
#endif
  }

  /** Inexact fallback must not round an odd long64 value into a valid integer result. */
  static void integerFallbackPrecision() {
    SDDSEditor editor;
    setup(editor);
    editor.pages[0].columns[0] = {"9007199254740993"};
    editor.populateModels();
    editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
    acceptDialog("Apply Numerical Expression", [](QDialog *dialog) {
      dialog->findChild<QLineEdit *>()->setText("x / 2");
    });
    editor.applyNumericalExpression(editor.columnView);
    require(editor.pages[0].columns[0][0] == "9007199254740993" && editor.undoStack->count() == 0 && !editor.dirty,
            "a fractional long64 formula cannot round into a valid integer and change the cell");
    if (std::numeric_limits<long double>::digits <= 64) {
      const QString boundary = QString::number(quint64(1) << (std::numeric_limits<long double>::digits - 1));
      acceptDialog("Apply Numerical Expression", [boundary](QDialog *dialog) {
        dialog->findChild<QLineEdit *>()->setText(boundary + ".5 - " + boundary);
      });
      editor.applyNumericalExpression(editor.columnView);
      require(editor.pages[0].columns[0][0] == "9007199254740993" && editor.undoStack->count() == 0 && !editor.dirty,
              "a large fractional literal cannot round to an integer before cancellation");
      acceptDialog("Apply Numerical Expression", [boundary](QDialog *dialog) {
        dialog->findChild<QLineEdit *>()->setText("(" + boundary + " + 0.5)^1 - " + boundary);
      });
      editor.applyNumericalExpression(editor.columnView);
      require(editor.pages[0].columns[0][0] == "9007199254740993" && editor.undoStack->count() == 0 && !editor.dirty,
              "a large intermediate cannot round to an integer before cancellation");
    }
    if (std::numeric_limits<long double>::digits < 64) {
      for (const QString &expression : {QString("9007199254740993 - 9007199254740992 + 0.0"),
                                        QString("9007199254740993.0 - 9007199254740992.0"),
                                        QString("9007199254740994 / 1.5")}) {
        acceptDialog("Apply Numerical Expression", [expression](QDialog *dialog) {
          dialog->findChild<QLineEdit *>()->setText(expression);
        });
        editor.applyNumericalExpression(editor.columnView);
        require(editor.pages[0].columns[0][0] == "9007199254740993" && editor.undoStack->count() == 0 && !editor.dirty,
                "floating fallback cannot silently round large integer literals or fractional results");
      }
    }
    acceptDialog("Apply Numerical Expression", [](QDialog *dialog) {
      dialog->findChild<QLineEdit *>()->setText("floor(x) + 2");
    });
    editor.applyNumericalExpression(editor.columnView);
    require(editor.pages[0].columns[0][0] == "9007199254740995", "large integral floor results use exact arithmetic");
    editor.undoStack->undo();
    acceptDialog("Apply Numerical Expression", [](QDialog *dialog) {
      dialog->findChild<QLineEdit *>()->setText("sqrt(16)");
    });
    editor.applyNumericalExpression(editor.columnView);
    require(editor.pages[0].columns[0][0] == "4", "unused large cell values do not prevent a safe floating fallback");
    editor.undoStack->undo();
    acceptDialog("Apply Numerical Expression", [](QDialog *dialog) {
      dialog->findChild<QLineEdit *>()->setText("ceil(x)");
    });
    editor.applyNumericalExpression(editor.columnView);
    require(editor.pages[0].columns[0][0] == "9007199254740993" && editor.undoStack->canRedo(),
            "large integral ceil is an exact no-op that preserves Redo");
    fprintf(stdout, "PASS integer fallback preserves unrepresentable inputs\n");
  }

  /** Attribute dialogs must preserve definitions beyond QLineEdit's default limit. */
  static void longDefinitionText(const QString &root) {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      failures += !ok;
    };
    const QString text = QString(40000, QLatin1Char('a')) + " tail";
    const QString edited = text + " edited";
    QByteArray bytes = text.toLocal8Bit();
    SDDSEditor editor;
    setup(editor);
    require(SDDS_DefineParameter(&editor.dataset, "Fixed", bytes.constData(), bytes.constData(),
                                 bytes.constData(), nullptr, SDDS_STRING, bytes.data()) >= 0,
            "define long fixed string and parameter metadata");
    require(replaceSharedLayoutString(&editor.dataset.layout.column_definition[0].description,
                                      &editor.dataset.original_layout.column_definition[0].description, text),
            "set long column description");
    require(replaceSharedLayoutString(&editor.dataset.layout.array_definition[0].description,
                                      &editor.dataset.original_layout.array_definition[0].description, text),
            "set long array description");
    require(replaceSharedLayoutString(&editor.dataset.layout.array_definition[0].group_name,
                                      &editor.dataset.original_layout.array_definition[0].group_name, text),
            "set long array group");
    require(SDDS_SaveLayout(&editor.dataset), "save long definition fixture layout");
    editor.pages[0].parameters = {text};
    editor.populateModels();
    editor.paramView->setCurrentIndex(editor.parameterValueCell(0));

    acceptDialog("Parameter Attributes", [&](QDialog *dialog) {
      const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
      check(fields[1]->text() == text && fields[2]->text() == text && fields[3]->text() == text &&
                fields[5]->text() == text, "parameter attributes display complete long text and fixed values");
    });
    editor.editParameterAttributes();
    check(editor.undoStack->count() == 0 && !editor.dirty && editor.pages[0].parameters[0] == text,
          "accepting unchanged long parameter attributes preserves data and history");
    acceptDialog("Column Attributes", [&](QDialog *dialog) {
      const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
      check(fields[3]->text() == text, "column attributes display the complete long description");
      fields[2]->setText("m");
      fields[3]->setText(edited);
    });
    editor.editColumnAttributesAt(0);
    check(QString::fromLocal8Bit(editor.dataset.layout.column_definition[0].description) == edited,
          "editing a long column description keeps every character");
    acceptDialog("Array Attributes", [&](QDialog *dialog) {
      const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
      check(fields[3]->text() == text && fields[5]->text() == text,
            "array attributes display complete long descriptions and group names");
      fields[2]->setText("s");
      fields[5]->setText(edited);
    });
    editor.editArrayAttributesAt(0);
    check(QString::fromLocal8Bit(editor.dataset.layout.array_definition[0].description) == text &&
              QString::fromLocal8Bit(editor.dataset.layout.array_definition[0].group_name) == edited,
          "editing a long array group name keeps every character");
    editor.paramView->setCurrentIndex(editor.parameterValueCell(0));
    acceptDialog("Parameter Attributes", [&](QDialog *dialog) {
      const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
      fields[2]->setText("kg");
      fields[5]->setText(edited);
    });
    editor.editParameterAttributes();
    check(editor.pages[0].parameters[0] == edited &&
              QString::fromLocal8Bit(editor.dataset.layout.parameter_definition[0].description) == text,
          "editing a long fixed string keeps every character and unchanged metadata");
    editor.undoStack->undo();
    editor.undoStack->redo();
    check(editor.pages[0].parameters[0] == edited,
          "long fixed strings survive structural undo and redo");
    for (bool ascii : {true, false}) {
      editor.asciiBtn->setChecked(ascii);
      editor.binaryBtn->setChecked(!ascii);
      const QString path = root + (ascii ? "/long-definitions-ascii.sdds" : "/long-definitions-binary.sdds");
      putFile(path, "original file");
      editor.dirty = true;
      check(!editor.writeFile(path) && readFile(path) == "original file" && editor.dirty,
            "an unreadable oversized layout cannot replace the destination or mark the document saved");
    }
    require(failures == 0, "long definition text regressions");
  }

  /** Saving floating-point cells as ASCII must preserve the exact stored values. */
  static void asciiPrecision(const QString &root) {
    const QByteArray originalLongDoubleMode = qgetenv("SDDS_LONGDOUBLE_64BITS");
    if (sizeof(long double) == sizeof(double))
      qputenv("SDDS_LONGDOUBLE_64BITS", "1"); // Native SDDS encoding on MSVC and Apple Silicon.
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      failures += !ok;
    };
    SDDSEditor editor;
    setup(editor);
    editor.dataset.layout.column_definition[0].type = SDDS_DOUBLE;
    editor.dataset.layout.array_definition[0].type = SDDS_LONGDOUBLE;
    require(SDDS_DefineParameter(&editor.dataset, "P", nullptr, nullptr, nullptr,
                                 nullptr, SDDS_DOUBLE, nullptr) >= 0, "define precision parameter");
    require(SDDS_SaveLayout(&editor.dataset), "save floating-point precision fixture layout");
    const QString doubleText = shortestDoubleText(std::nextafter(1.0, 2.0));
    const QString longDoubleText = longDoubleToText(std::nextafter(1.0L, 2.0L));
    editor.pages[0].parameters = {doubleText};
    editor.pages[0].columns[0] = {doubleText, shortestDoubleText(std::nextafter(0.1, 0.0)), "-0"};
    editor.pages[0].arrays[0].values = {longDoubleText, longDoubleToText(-std::nextafter(1.0L, 2.0L)),
                                      longDoubleToText(std::nextafter(0.1L, 0.0L)), "-0"};
    editor.populateModels();
    for (const QString &suffix : {QString(".sdds"), QString(".sdds.gz"), QString(".sdds.xz")}) {
      for (bool ascii : {true, false}) {
        editor.asciiBtn->setChecked(ascii);
        editor.binaryBtn->setChecked(!ascii);
        const QString path = root + (ascii ? "/exact-ascii" : "/exact-binary") + suffix;
        require(editor.writeFile(path), "save exact floating-point fixture");
        SDDSEditor loaded;
        require(loaded.loadFile(path), "reload exact floating-point fixture");
        check(loaded.pages[0].parameters == editor.pages[0].parameters &&
                  loaded.pages[0].columns == editor.pages[0].columns &&
                  loaded.pages[0].arrays[0].values == editor.pages[0].arrays[0].values,
              ascii ? "ASCII saves preserve every floating-point digit and signed zero"
                    : "binary saves preserve every floating-point digit and signed zero");
      }
    }
    if (originalLongDoubleMode.isNull())
      qunsetenv("SDDS_LONGDOUBLE_64BITS");
    else
      qputenv("SDDS_LONGDOUBLE_64BITS", originalLongDoubleMode);
    require(failures == 0, "ASCII floating-point precision regressions");
  }

  /** Long replacement text and formula templates must reach string cells intact. */
  static void longTextTools(const QString &root) {
    const QString text = QString(40000, QLatin1Char('t')) + " tail";
    SDDSEditor editor;
    setup(editor);
    editor.dataset.layout.column_definition[0].type = SDDS_STRING;
    require(SDDS_SaveLayout(&editor.dataset), "save long text tool fixture layout");
    editor.pages[0].columns[0] = {"prefix", "prefix", "prefix"};
    editor.populateModels();
    editor.show();
    editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
    editor.lastTextFormula = text;
    acceptDialog("Apply Text Formula", [&](QDialog *dialog) {
      QLineEdit *field = dialog->findChild<QLineEdit *>();
      require(field && field->text() == text, "formula dialog keeps a long saved template");
      field->setText(text + " formula");
    });
    editor.applyTextFormula(editor.columnView);
    require(editor.pages[0].columns[0][0] == text + " formula", "formula stores the complete long template");
    editor.searchColumn(0);
    QDialog *search = editor.searchColumnDialog.data();
    require(search, "open long replacement dialog");
    const auto fields = search->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
    fields[0]->setText("prefix");
    fields[1]->setText(text);
    for (QPushButton *button : search->findChildren<QPushButton *>())
      if (button->text() == "Replace All")
        button->click();
    require(editor.pages[0].columns[0][1] == text && editor.pages[0].columns[0][2] == text,
            "search replacement stores the complete long text");
    search->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const QString path = root + "/long-text-tools.sdds";
    require(editor.writeFile(path), "save long formula and replacement values");
    SDDSEditor loaded;
    require(loaded.loadFile(path), "reload long formula and replacement values");
    require(loaded.pages[0].columns == editor.pages[0].columns,
            "long formula and replacement values round-trip without truncation");
    fprintf(stdout, "PASS long formula templates, replacements and string round trips\n");
  }

  /** Array shape edits keep data, row filters reject stray signs, and rows need a column. */
  static void shapeFilterAndRowFixes() {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      failures += !ok;
    };
    {
      SDDSEditor editor;
      setup(editor);
      const int large = 1500000;
      editor.pages[0].arrays[0].dims = {large, 1};
      editor.pages[0].arrays[0].values = QVector<QString>(large);
      editor.pages[0].arrays[0].values[large - 1] = "7";
      editor.populateModels();
      editor.dirty = false;
      const int history = editor.undoStack->count();
      acceptDialog("Resize Array", [&](QDialog *dialog) {
        check(dialog->findChildren<QSpinBox *>()[0]->value() == large,
              "the resize dialog shows a dimension above one million");
      });
      editor.resizeArray(0);
      check(editor.pages[0].arrays[0].dims == QVector<int>({large, 1}) &&
                editor.pages[0].arrays[0].values.size() == large &&
                editor.pages[0].arrays[0].values[large - 1] == "7" &&
                editor.undoStack->count() == history && !editor.dirty,
            "accepting an unchanged resize keeps every element of a large array");
    }
    {
      SDDSEditor editor;
      setup(editor);
      auto setDimensions = [&](int count) {
        acceptDialog("Array Attributes", [count](QDialog *dialog) {
          dialog->findChildren<QSpinBox *>()[1]->setValue(count);
        });
        editor.editArrayAttributesAt(0);
      };
      setDimensions(1);
      check(editor.pages[0].arrays[0].dims == QVector<int>({2}) &&
                editor.pages[0].arrays[0].values == QVector<QString>({"10", "30"}),
            "removing a dimension keeps the elements whose dropped index is 0");
      setDimensions(3);
      check(editor.pages[0].arrays[0].dims == QVector<int>({2, 1, 1}) &&
                editor.pages[0].arrays[0].values == QVector<QString>({"10", "30"}),
            "adding dimensions keeps every element at its indices");
      editor.undoStack->undo();
      editor.undoStack->undo();
      check(editor.pages[0].arrays[0].dims == QVector<int>({2, 2}) &&
                editor.pages[0].arrays[0].values == QVector<QString>({"10", "20", "30", "40"}),
            "dimension count edits undo to the original shape and values");
    }
    {
      auto parses = [](const QString &expression) {
        RowFilterParser parser(expression, [](const QString &, bool, QString *value) {
          *value = "5";
          return true;
        });
        bool pass = false;
        QString error;
        return parser.parse(&pass, &error);
      };
      check(!parses("X ->= 1") && !parses("X > 0 .&& X < 9") && !parses("X +== 5"),
            "row filters reject a stray sign or dot before an operator");
      check(parses("X >= -1") && parses("X > .5 && X < +9"),
            "row filters still read signed and fractional numbers");
    }
    {
      SDDSEditor editor;
      require(editor.ensureDataset(), "initialize document without columns");
      // Answer the row count prompt if one appears, so a regression fails instead of blocking.
      QTimer::singleShot(0, []() {
        for (QWidget *widget : QApplication::topLevelWidgets())
          if (QInputDialog *dialog = qobject_cast<QInputDialog *>(widget))
            dialog->accept();
      });
      editor.insertColumnRows();
      check(editor.undoStack->count() == 0 && !editor.dirty,
            "inserting rows without columns adds no Undo step and leaves the document unmodified");
    }
    require(failures == 0, "array shape, row filter and row insertion regressions");
  }

  /** Fixed definitions must preserve literal data, and dialogs must preserve valid metadata. */
  static void fixedTextAndDefinitions(const QString &root) {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      failures += !ok;
    };
    for (const QString &kind : {QString("Parameter"), QString("Column"), QString("Array")}) {
      SDDSEditor editor;
      require(editor.ensureDataset(), "initialize inserted name fixture");
      acceptDialog("New " + kind, [](QDialog *dialog) {
        dialog->findChild<QLineEdit *>()->setText(QString("Prefix") + QChar(0) + "Suffix");
      });
      if (kind == "Parameter")
        editor.insertParameter();
      else if (kind == "Column")
        editor.insertColumn();
      else
        editor.insertArray();
      check(editor.dataset.layout.n_parameters == 0 && editor.dataset.layout.n_columns == 0 &&
                editor.dataset.layout.n_arrays == 0 && editor.undoStack->count() == 0 && !editor.dirty,
            "an inserted name containing NUL is rejected without truncation or mutation");
    }
    for (const QString &kind : {QString("Column"), QString("Array")}) {
      SDDSEditor editor;
      setup(editor);
      if (kind == "Column")
        editor.dataset.layout.column_definition[0].field_length =
            editor.dataset.original_layout.column_definition[0].field_length = 2000000;
      else
        editor.dataset.layout.array_definition[0].field_length =
            editor.dataset.original_layout.array_definition[0].field_length = 2000000;
      applyCellEditWithUndo(editor.undoStack, editor.columnModel, editor.columnModel->index(0, 0), "9");
      editor.undoStack->undo();
      editor.dirty = false;
      acceptDialog(kind + " Attributes", [&](QDialog *dialog) {
        check(dialog->findChild<QSpinBox *>()->value() == 2000000,
              "attribute dialog displays the full valid field length");
      });
      if (kind == "Column")
        editor.editColumnAttributesAt(0);
      else
        editor.editArrayAttributesAt(0);
      const int length = kind == "Column" ? editor.dataset.layout.column_definition[0].field_length
                                           : editor.dataset.layout.array_definition[0].field_length;
      check(length == 2000000 && editor.undoStack->canRedo() && !editor.dirty,
            "accepting unchanged attributes preserves field length, Redo and saved state");
    }
    {
      SDDSEditor editor;
      require(editor.ensureDataset(), "initialize fixed character dialog fixture");
      acceptDialog("New Parameter", [](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        fields[0]->setText("Character");
        fields[5]->setText(QString(QChar(255)));
        for (QRadioButton *button : dialog->findChildren<QRadioButton *>())
          if (button->text() == "character")
            button->setChecked(true);
      });
      editor.insertParameter();
      require(editor.dataset.layout.n_parameters == 1, "insert non-ASCII fixed character through dialog");
      editor.paramView->setCurrentIndex(editor.parameterValueCell(0));
      acceptDialog("Parameter Attributes", [&](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        check(fields[5]->text() == QString(QChar(255)), "fixed character attributes show its Latin-1 byte");
        fields[5]->setText(QString(QChar(0)));
      });
      editor.editParameterAttributes();
      check(editor.pages[0].parameters[0] == QString(QChar(0)), "fixed character attributes accept a zero byte");
      editor.undoStack->undo();
      check(editor.pages[0].parameters[0] == QString(QChar(255)), "fixed character attribute edit undoes correctly");
      editor.undoStack->redo();
      const QString path = root + "/fixed-character-dialog.sdds";
      SDDSEditor loaded;
      check(editor.writeFile(path) && loaded.loadFile(path) && loaded.pages[0].parameters[0].isEmpty(),
            "a zero byte entered through fixed attributes survives save and reload");
      editor.paramView->setCurrentIndex(editor.parameterValueCell(0));
      acceptDialog("Parameter Attributes", [&](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        check(fields[5]->text() == QString(QChar(0)), "zero fixed character remains represented in attributes");
        fields[2]->setText("unit");
      });
      editor.editParameterAttributes();
      check(editor.dataset.layout.parameter_definition[0].fixed_value != nullptr,
            "editing other attributes preserves a fixed zero character");
    }
    {
      SDDSEditor editor;
      require(editor.ensureDataset(), "initialize empty fixed string fixture");
      char empty[] = "";
      require(SDDS_DefineParameter(&editor.dataset, "Empty", nullptr, nullptr, nullptr, nullptr,
                                   SDDS_STRING, empty) >= 0, "define empty fixed string");
      require(SDDS_SaveLayout(&editor.dataset), "save empty fixed string layout");
      editor.pages[0].parameters = {QString()};
      editor.populateModels();
      editor.paramView->setCurrentIndex(editor.parameterValueCell(0));
      acceptDialog("Parameter Attributes", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[2]->setText("unit");
      });
      editor.editParameterAttributes();
      check(editor.dataset.layout.parameter_definition[0].fixed_value != nullptr &&
                editor.dataset.layout.parameter_definition[0].fixed_value[0] == '\0',
            "editing other attributes preserves an empty fixed string");
    }
    for (bool ascii : {true, false}) {
      SDDSEditor editor;
      require(editor.ensureDataset(), "initialize fixed string fixture");
      QString literal = "\"quoted\" C:\\temp\\new ! &end\n\t";
      if (localEncodingPreserves(QString(QChar(0x00E9))))
        literal += QChar(0x00E9);
      acceptDialog("New Parameter", [literal](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        fields[0]->setText("Literal");
        fields[5]->setText(literal);
      });
      editor.insertParameter();
      require(editor.dataset.layout.n_parameters == 1, "insert fixed literal string");
      const int history = editor.undoStack->count();
      editor.dirty = false;
      editor.paramView->setCurrentIndex(editor.parameterValueCell(0));
      acceptDialog("Parameter Attributes", [&](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        check(fields[5]->text() == literal, "fixed string attributes show literal text");
      });
      editor.editParameterAttributes();
      check(editor.undoStack->count() == history && !editor.dirty,
            "accepting unchanged parameter attributes preserves undo and saved state");
      editor.asciiBtn->setChecked(ascii);
      editor.binaryBtn->setChecked(!ascii);
      for (const QString &suffix : {QString("sdds"), QString("sdds.gz"), QString("sdds.xz")}) {
        const QString path = root + QString("/fixed-literal-%1.").arg(ascii ? "ascii" : "binary") + suffix;
        const bool saved = editor.writeFile(path);
        check(saved, "save fixed literal string");
        if (saved) {
          SDDSEditor loaded;
          const bool opened = loaded.loadFile(path);
          check(opened && loaded.pages[0].parameters[0] == literal, "fixed literal string survives save and reload");
          if (opened) {
            const QString second = root + QString("/fixed-literal-again-%1.").arg(ascii ? "ascii" : "binary") + suffix;
            const bool resaved = loaded.writeFile(second);
            SDDSEditor again;
            check(resaved && again.loadFile(second) && again.pages[0].parameters[0] == literal,
                  "loading and resaving a fixed escaped string preserves its literal value");
          }
        }
      }
    }
    for (bool ascii : {true, false}) {
      SDDSEditor editor;
      require(editor.ensureDataset(), "initialize fixed character fixture");
      char initial[] = "A";
      for (int byte = 0; byte < 256; ++byte) {
        const QByteArray name = QString("Byte%1").arg(byte).toLatin1();
        require(SDDS_DefineParameter(&editor.dataset, name.constData(), nullptr, nullptr, nullptr,
                                     nullptr, SDDS_CHARACTER, initial) >= 0, "define fixed character");
        editor.pages[0].parameters.append(QString(QChar(byte)));
      }
      require(SDDS_SaveLayout(&editor.dataset), "save fixed character layout");
      editor.populateModels();
      editor.asciiBtn->setChecked(ascii);
      editor.binaryBtn->setChecked(!ascii);
      for (const QString &suffix : {QString("sdds"), QString("sdds.gz"), QString("sdds.xz")}) {
        const QString path = root + QString("/fixed-bytes-%1.").arg(ascii ? "ascii" : "binary") + suffix;
        const bool saved = editor.writeFile(path);
        check(saved, "save every fixed character byte");
        if (saved) {
          SDDSEditor loaded;
          const bool opened = loaded.loadFile(path);
          bool same = opened && loaded.pages[0].parameters.size() == 256;
          if (same) {
            for (int byte = 0; byte < 256; ++byte) {
              // As in other character fields, the editor displays a zero byte as empty.
              const QString expected = byte == 0 ? QString() : QString(QChar(byte));
              same = same && loaded.pages[0].parameters[byte] == expected;
            }
          }
          check(same, "all 256 fixed character bytes survive plain/gzip/xz round trips");
        }
      }
    }
    require(failures == 0, "fixed text and definition regressions");
  }

  /** Exercise long text, padded array selections and paste through a live delegate. */
  static void longTextAndArrayActions(const QString &root) {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      failures += !ok;
    };
    const QString longText = QString(40000, QLatin1Char('x')) + "tail";
    {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.column_definition[0].type = SDDS_STRING;
      editor.dataset.original_layout.column_definition[0].type = SDDS_STRING;
      editor.dataset.layout.array_definition[0].type = SDDS_STRING;
      editor.dataset.original_layout.array_definition[0].type = SDDS_STRING;
      require(SDDS_DefineParameter(&editor.dataset, "P", nullptr, nullptr, nullptr,
                                   nullptr, SDDS_STRING, nullptr) >= 0, "define long string parameter");
      require(SDDS_SaveLayout(&editor.dataset), "save long string layout");
      editor.pages[0].parameters = {longText};
      editor.pages[0].columns[0][0] = longText;
      editor.pages[0].arrays[0].values[0] = longText;
      editor.populateModels();
      editor.show();
      for (QTableView *view : {editor.paramView, editor.columnView, editor.arrayView}) {
        const QModelIndex index = view == editor.paramView ? editor.parameterValueCell(0) : view->model()->index(0, 0);
        view->openPersistentEditor(index);
        QLineEdit *cell = qobject_cast<QLineEdit *>(view->indexWidget(index));
        require(cell, "open long string cell editor");
        check(cell->text() == longText, "delegate opens the entire string beyond 32767 characters");
        editor.flushPendingEdits();
        check(index.data(Qt::EditRole).toString() == longText && editor.undoStack->count() == 0,
              "opening and committing an unchanged long string preserves data and undo");
      }
      editor.openArrayViewer(0);
      auto *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      const QModelIndex index = viewer->sliceModel()->index(0, 0);
      viewer->table()->openPersistentEditor(index);
      QLineEdit *cell = qobject_cast<QLineEdit *>(viewer->table()->indexWidget(index));
      require(cell, "open long string viewer editor");
      check(cell->text() == longText, "array viewer opens the entire long string");
      editor.flushPendingEdits();
      check(editor.pages[0].arrays[0].values[0] == longText, "viewer commit preserves the long string");
      require(editor.writeFile(root + "/long-text-actions.sdds"), "save long string action fixture");
      SDDSEditor loaded;
      require(loaded.loadFile(root + "/long-text-actions.sdds"), "reload long string action fixture");
      check(loaded.pages[0].parameters[0] == longText && loaded.pages[0].columns[0][0] == longText &&
                loaded.pages[0].arrays[0].values[0] == longText, "long strings survive editing and file round trip");
    }
    for (const QString &kind : {QString("Parameter"), QString("Column"), QString("Array")}) {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineParameter(&editor.dataset, "P", nullptr, nullptr, nullptr,
                                   nullptr, SDDS_STRING, nullptr) >= 0, "define attribute parameter");
      require(SDDS_SaveLayout(&editor.dataset), "save attribute fixture layout");
      editor.pages[0].parameters = {"value"};
      char **description = kind == "Parameter" ? &editor.dataset.layout.parameter_definition[0].description
          : kind == "Column" ? &editor.dataset.layout.column_definition[0].description
                             : &editor.dataset.layout.array_definition[0].description;
      char **savedDescription = kind == "Parameter" ? &editor.dataset.original_layout.parameter_definition[0].description
          : kind == "Column" ? &editor.dataset.original_layout.column_definition[0].description
                             : &editor.dataset.original_layout.array_definition[0].description;
      require(replaceSharedLayoutString(description, savedDescription, longText), "install long description");
      editor.populateModels();
      editor.paramView->setCurrentIndex(editor.parameterValueCell(0));
      acceptDialog(kind + " Attributes", [&](QDialog *dialog) {
        const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
        check(fields[3]->text() == longText, "attribute dialog opens the entire long description");
      });
      if (kind == "Parameter")
        editor.editParameterAttributes();
      else if (kind == "Column")
        editor.editColumnAttributesAt(0);
      else
        editor.editArrayAttributesAt(0);
      check(QString::fromLocal8Bit(*description) == longText, "accepting attributes preserves the long description");
    }
    for (const QString &action : {QString("Fill"), QString("Numerical"), QString("Text"), QString("Delete")}) {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineArray(&editor.dataset, "Short", nullptr, nullptr, nullptr,
                               nullptr, SDDS_DOUBLE, 0, 1, nullptr) >= 0, "define shorter array");
      require(SDDS_SaveLayout(&editor.dataset), "save shorter array layout");
      ArrayStore shortArray;
      shortArray.dims = {2};
      shortArray.values = {"50", "60"};
      editor.pages[0].arrays.append(shortArray);
      editor.populateModels();
      editor.show();
      editor.activateWindow();
      editor.arrayView->setFocus();
      if (action == "Delete") {
        applyCellEditWithUndo(editor.undoStack, editor.arrayModel, editor.arrayModel->index(0, 0), "11");
        editor.undoStack->undo();
        editor.dirty = false;
        editor.arrayView->setCurrentIndex(editor.arrayModel->index(3, 1));
        editor.deleteCells();
        check(editor.undoStack->canRedo() && editor.undoStack->count() == 1 && !editor.dirty,
              "delete on array padding preserves Redo and saved state");
        continue;
      }
      editor.arrayView->selectAll();
      if (action == "Fill") {
        acceptDialog("Fill Series", [](QDialog *dialog) {
          const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
          fields[0]->setText("0");
          fields[1]->setText("1");
        });
        editor.fillSeries(editor.arrayView);
      } else if (action == "Numerical") {
        acceptDialog("Apply Numerical Expression", [](QDialog *dialog) { dialog->findChild<QLineEdit *>()->setText("i"); });
        editor.applyNumericalExpression(editor.arrayView);
      } else {
        acceptDialog("Apply Text Formula", [](QDialog *dialog) { dialog->findChild<QLineEdit *>()->setText("${i}"); });
        editor.applyTextFormula(editor.arrayView);
      }
      check(editor.pages[0].arrays[0].values == QVector<QString>({"0", "2", "4", "5"}) &&
                editor.pages[0].arrays[1].values == QVector<QString>({"1", "3"}),
            "formula sequence numbers count editable cells rather than padding");
      editor.undoStack->undo();
      check(editor.pages[0].arrays[0].values == QVector<QString>({"10", "20", "30", "40"}) &&
                editor.pages[0].arrays[1].values == shortArray.values, "padded-array formula undoes as one step");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.openArrayViewer(0);
      auto *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      const QModelIndex index = viewer->sliceModel()->index(0, 0);
      viewer->table()->setCurrentIndex(index);
      viewer->table()->openPersistentEditor(index);
      QLineEdit *cell = qobject_cast<QLineEdit *>(viewer->table()->indexWidget(index));
      require(cell, "open viewer editor for rectangular paste");
      cell->setText("123");
      QApplication::clipboard()->setText("1\t2\n3\t4");
      const QKeySequence pasteSequence(QKeySequence::Paste);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      const int pasteKey = pasteSequence[0].toCombined();
#else
      const int pasteKey = pasteSequence[0];
#endif
      QKeyEvent paste(QEvent::KeyPress, pasteKey & ~int(Qt::KeyboardModifierMask),
                      Qt::KeyboardModifiers(pasteKey & int(Qt::KeyboardModifierMask)));
      QApplication::sendEvent(cell, &paste);
      // Release modifiers too: Qt table selection consults their global state.
      QKeyEvent release(QEvent::KeyRelease, pasteKey & ~int(Qt::KeyboardModifierMask), Qt::NoModifier);
      QApplication::sendEvent(viewer->table(), &release);
      viewer->finishEditing();
      check(editor.pages[0].arrays[0].values == QVector<QString>({"1", "2", "3", "4"}),
            "keyboard paste in an active viewer editor fills the rectangle");
      editor.undoStack->undo();
      check(editor.pages[0].arrays[0].values == QVector<QString>({"123", "20", "30", "40"}),
            "viewer paste preserves the pending edit as a separate undo step");
      editor.undoStack->undo();
      check(editor.pages[0].arrays[0].values == QVector<QString>({"10", "20", "30", "40"}),
            "viewer pending edit can also be undone");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      editor.openArrayViewer(0);
      auto *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      require(viewer->sliceModel()->setData(viewer->sliceModel()->index(0, 0), "11"), "seed shared undo history");
      const QModelIndex index = editor.columnModel->index(0, 0);
      editor.columnView->openPersistentEditor(index);
      QLineEdit *cell = qobject_cast<QLineEdit *>(editor.columnView->indexWidget(index));
      require(cell, "open main table pending editor for viewer Undo");
      cell->setText("123");
      viewer->findChild<QAction *>("arrayViewerUndo")->trigger();
      editor.flushPendingEdits();
      check(editor.pages[0].columns[0][0] == "3" && editor.pages[0].arrays[0].values[0] == "11",
            "viewer Undo commits all windows and undoes the latest pending edit");
      viewer->findChild<QAction *>("arrayViewerRedo")->trigger();
      check(editor.pages[0].columns[0][0] == "123" && editor.pages[0].arrays[0].values[0] == "11",
            "viewer Redo restores the main table edit");
      editor.openArrayViewer(0);
      auto *other = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      const QModelIndex pending = viewer->sliceModel()->index(0, 1);
      viewer->table()->openPersistentEditor(pending);
      cell = qobject_cast<QLineEdit *>(viewer->table()->indexWidget(pending));
      require(cell, "open pending editor in a second array window");
      cell->setText("222");
      other->findChild<QAction *>("arrayViewerUndo")->trigger();
      editor.flushPendingEdits();
      check(editor.pages[0].arrays[0].values == QVector<QString>({"11", "20", "30", "40"}),
            "viewer Undo also commits pending edits in another viewer");
      other->findChild<QAction *>("arrayViewerRedo")->trigger();
      check(editor.pages[0].arrays[0].values == QVector<QString>({"11", "222", "30", "40"}),
            "viewer Redo restores the other viewer's edit");
    }
    require(failures == 0, "long text and array action regressions");
  }

  /** SDDS names use the same local encoding as values, including on Windows. */
  static void definitionNameEncoding() {
    SDDSEditor editor;
    require(editor.ensureDataset(), "initialize name encoding fixture");
    QString name = QStringLiteral("caf") + QChar(0x00E9);
    if (!localEncodingPreserves(name))
      name = "Name";
    const QByteArray encoded = name.toLocal8Bit();
    // Permit names from legacy layouts independently of the C character locale.
    const uint32_t nameFlags = SDDS_SetNameValidityFlags(SDDS_ALLOW_ANY_NAME);
    require(SDDS_DefineParameter(&editor.dataset, encoded.constData(), nullptr, nullptr,
                                 nullptr, nullptr, SDDS_STRING, nullptr) >= 0, "define local-encoding parameter");
    require(SDDS_DefineColumn(&editor.dataset, encoded.constData(), nullptr, nullptr,
                              nullptr, nullptr, SDDS_STRING, 0) >= 0, "define local-encoding column");
    require(SDDS_DefineArray(&editor.dataset, encoded.constData(), nullptr, nullptr,
                             nullptr, nullptr, SDDS_STRING, 0, 1, nullptr) >= 0, "define local-encoding array");
    require(SDDS_SaveLayout(&editor.dataset), "save name encoding layout");
    SDDS_SetNameValidityFlags(nameFlags);
    editor.pages[0].parameters = {"value"};
    editor.pages[0].columns = {{"value"}};
    ArrayStore array;
    array.dims = {1};
    array.values = {"value"};
    editor.pages[0].arrays = {array};
    editor.populateModels();
    require(editor.paramModel->headerData(0, Qt::Vertical).toString() == name, "parameter name uses local encoding");
    require(editor.columnModel->headerData(0, Qt::Horizontal).toString() == name, "column name uses local encoding");
    require(editor.arrayModel->headerData(0, Qt::Horizontal).toString() == name, "array name uses local encoding");
    fprintf(stdout, "PASS definition names use the SDDS text encoding\n");
  }

  /** Table actions must consume pending delegate text before computing their edits. */
  static void pendingTableActions() {
    int failures = 0;
    for (const QString &action : {QString("Paste"), QString("Delete"), QString("Numerical"), QString("Text"), QString("Fill")}) {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      editor.activateWindow();
      editor.columnView->setFocus();
      const QModelIndex index = editor.columnModel->index(0, 0);
      editor.columnView->setCurrentIndex(index);
      editor.columnView->openPersistentEditor(index);
      QCoreApplication::processEvents();
      QLineEdit *cell = qobject_cast<QLineEdit *>(editor.columnView->indexWidget(index));
      require(cell && !cell->isHidden(), "pending editor for table action");
      cell->setText("123");
      QString expected;
      if (action == "Paste") {
        QApplication::clipboard()->setText("456");
        editor.paste();
        expected = "456";
      } else if (action == "Delete") {
        editor.deleteCells();
      } else if (action == "Numerical") {
        acceptDialog("Apply Numerical Expression", [](QDialog *dialog) {
          dialog->findChild<QLineEdit *>()->setText("x+1");
        });
        editor.applyNumericalExpression(editor.columnView);
        expected = "124";
      } else if (action == "Text") {
        acceptDialog("Apply Text Formula", [](QDialog *dialog) {
          dialog->findChild<QLineEdit *>()->setText("${x}0");
        });
        editor.applyTextFormula(editor.columnView);
        expected = "1230";
      } else {
        acceptDialog("Fill Series", [](QDialog *dialog) {
          const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
          fields[0]->setText("456");
          fields[1]->setText("1");
        });
        editor.fillSeries(editor.columnView);
        expected = "456";
      }
      editor.flushPendingEdits();
      const bool applied = index.data(Qt::EditRole).toString() == expected;
      editor.undoStack->undo();
      const bool undone = index.data(Qt::EditRole).toString() == "123";
      editor.undoStack->undo();
      const bool original = index.data(Qt::EditRole).toString() == "3";
      const bool ok = applied && undone && original;
      fprintf(stdout, "%s pending %s commits text and preserves both undo steps\n", ok ? "PASS" : "FAIL", qPrintable(action));
      failures += !ok;
    }
    require(failures == 0, "pending table action regressions");
  }

  /** A loaded multiline layout must remain writable after deleting columns. */
  static void multilineSave(const QString &root) {
    const QString input = root + "/multiline-input.sdds";
    putFile(input, "SDDS1\n&column name=X, type=long, &end\n"
                   "&column name=Y, type=long, &end\n&column name=Z, type=long, &end\n"
                   "&data mode=ascii, lines_per_row=3, &end\n2\n1\n2\n3\n4\n5\n6\n");
    SDDSEditor editor;
    require(editor.loadFile(input), "load multiline ASCII fixture");
    editor.deleteColumnIndexes({1, 2});
    for (const QString &suffix : {QString(".sdds"), QString(".sdds.gz"), QString(".sdds.xz")}) {
      const QString output = root + "/multiline-output" + suffix;
      require(editor.writeFile(output), "save multiline input after deleting columns");
      SDDSEditor loaded;
      require(loaded.loadFile(output), "reload edited multiline file");
      require(loaded.pages[0].columns == QVector<QVector<QString>>({{"1", "4"}}), "multiline save retains rows");
    }
    fprintf(stdout, "PASS edited multiline ASCII save and compressed round trips\n");
  }

  /** Reproduce pending-copy, stable numeric ordering and stale filter defects. */
  static void additionalBugFixes() {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      if (!ok)
        ++failures;
    };
    {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      editor.activateWindow();
      editor.columnView->setFocus();
      const QModelIndex index = editor.columnModel->index(0, 0);
      editor.columnView->setCurrentIndex(index);
      editor.columnView->openPersistentEditor(index);
      QCoreApplication::processEvents();
      QLineEdit *cell = qobject_cast<QLineEdit *>(editor.columnView->indexWidget(index));
      require(cell && !cell->isHidden(), "open persistent editor for pending copy");
      cell->setText("123");
      editor.copy();
      check(QApplication::clipboard()->text() == "123", "copy includes the pending cell edit");
    }
    for (int type : {SDDS_LONG64, SDDS_ULONG64, SDDS_DOUBLE, SDDS_LONGDOUBLE}) {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.column_definition[0].type = type;
      require(SDDS_DefineColumn(&editor.dataset, "Order", nullptr, nullptr, nullptr,
                                nullptr, SDDS_STRING, 0) >= 0, "define sort row labels");
      require(SDDS_SaveLayout(&editor.dataset), "save stable sort layout");
      const QVector<QString> values = SDDS_INTEGER_TYPE(type)
          ? QVector<QString>({"1", "01", "+1", "", "0"})
          : QVector<QString>({"1", "1.0", "1e0", "", "0"});
      const QVector<QString> labels = {"a", "b", "c", "d", "e"};
      editor.pages[0].columns = {values, labels};
      editor.populateModels();
      editor.sortColumn(0, Qt::AscendingOrder);
      check(editor.pages[0].columns[1] == QVector<QString>({"d", "e", "a", "b", "c"}),
            "ascending numeric sort preserves equal values in original order");
      editor.undoStack->undo();
      editor.sortColumn(0, Qt::DescendingOrder);
      check(editor.pages[0].columns[1] == labels,
            "descending numeric sort preserves equal values in original order");
      editor.undoStack->undo();
      editor.pages[0].columns[0] = {"2", "15x", "10", "1x", "1"};
      editor.sortColumn(0, Qt::AscendingOrder);
      check(editor.pages[0].columns[0] == QVector<QString>({"1", "2", "10", "1x", "15x"}),
            "invalid numeric cells follow valid numbers with a consistent ordering");
      editor.undoStack->undo();
      editor.sortColumn(0, Qt::DescendingOrder);
      check(editor.pages[0].columns[0] == QVector<QString>({"10", "2", "1", "15x", "1x"}),
            "descending numeric sort also places invalid cells after valid numbers");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.pages[0].columns[0][0].clear();
      editor.populateModels();
      editor.rowFilterExpression = "X==0";
      editor.rowFilterActive = true;
      editor.refreshColumnRowFilter(false);
      require(!editor.columnView->isRowHidden(0), "empty numeric value filters as zero");
      acceptDialog("Column Type", [](QDialog *dialog) {
        dialog->findChild<QComboBox *>()->setCurrentText("string");
      });
      editor.changeColumnType(0);
      check(editor.columnView->isRowHidden(0) && editor.visibleColumnRows == 0,
            "changing column type immediately reapplies the row filter");
      editor.undoStack->undo();
      require(!editor.columnView->isRowHidden(0), "undo type change restores numeric filtering");
      acceptDialog("Column Attributes", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[0]->setText("Renamed");
      });
      editor.editColumnAttributesAt(0);
      check(!editor.rowFilterActive && editor.visibleColumnRows == 3,
            "renaming a filtered column immediately disables the invalid filter");
    }
    const long double exact = std::nextafter(1.0L, 2.0L);
    long double parsed = 0;
    check(parseLongDoubleStrict(longDoubleToText(exact), &parsed) && parsed == exact,
          "computed long double text retains enough digits to round trip");
    require(failures == 0, "additional editor bug regressions");
  }

  /** Reproduce data-loss cases across formulas, clipboard, export and pending undo. */
  static void dataPreservation(const QString &root) {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      if (!ok)
        ++failures;
    };
    check(applyTemplateVariables("${x}|${a}|${row}|${unknown}", "${a}", "${row}", 0, 2, 3, 4, 5) ==
              "${a}|${row}|2|${unknown}", "formula substitutes tokens only in the original template");
    const QString nulText = QStringLiteral("before") + QChar(0) + "after";
    check(!validateTextForType(nulText, SDDS_STRING, false), "strings reject embedded NUL instead of silently truncating");
    long double number = 0;
    check(!parseLongDoubleStrict(QStringLiteral("1") + QChar(0) + "garbage", &number),
          "strict numeric parsing rejects text after an embedded NUL");
    check(validateTextForType(QString(QChar(0)), SDDS_CHARACTER, false), "character byte zero remains valid");
    {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      editor.activateWindow();
      editor.columnView->setFocus();
      QCoreApplication::processEvents();
      editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
      QApplication::clipboard()->setText("5\n\n");
      editor.paste();
      check(editor.pages[0].columns[0] == QVector<QString>({"5", "", "2"}),
            "text paste keeps the final empty row before the terminating newline");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.array_definition[0].type = SDDS_STRING;
      editor.dataset.layout.array_definition[0].dimensions = 1;
      require(SDDS_SaveLayout(&editor.dataset), "save text array test layout");
      const QVector<QString> text = {"a\tb", "c\nd"};
      editor.pages[0].arrays[0].dims = {2};
      editor.pages[0].arrays[0].values = text;
      editor.populateModels();
      editor.show();
      editor.activateWindow();
      editor.arrayView->setFocus();
      QCoreApplication::processEvents();
      editor.arrayView->selectAll();
      editor.copy();
      editor.openArrayViewer(0);
      ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      editor.pages[0].arrays[0].values = {"old1", "old2"};
      viewer->table()->setCurrentIndex(viewer->sliceModel()->index(0, 0));
      check(viewer->pasteText(QApplication::clipboard()->text()) && editor.pages[0].arrays[0].values == text,
            "main table to array viewer paste preserves embedded tabs and newlines");
      editor.pages[0].arrays[0].values = text;
      viewer->copySelection(true);
      editor.pages[0].arrays[0].values = {"old1", "old2"};
      editor.activateWindow();
      editor.arrayView->setFocus();
      QCoreApplication::processEvents();
      editor.arrayView->setCurrentIndex(editor.arrayModel->index(0, 0));
      editor.paste();
      check(editor.pages[0].arrays[0].values == text,
            "array viewer to main table paste preserves embedded tabs and newlines");
    }
    {
      SDDSEditor editor;
      setup(editor);
      const QString path = root + "/preserved.csv";
      putFile(path, "original CSV bytes");
      editor.dirty = true;
      editor.pages[0].columns[0][0] = "invalid integer";
      check(!editor.writeCSV(path) && readFile(path) == "original CSV bytes",
            "invalid CSV export fails and preserves the destination");
      check(editor.dirty, "failed CSV export preserves unsaved state");
      editor.pages[0].columns[0][0] = "3";
      require(SDDS_DefineArray(&editor.dataset, "B", nullptr, nullptr, nullptr, nullptr,
                               SDDS_DOUBLE, 0, 1, nullptr) >= 0, "define shorter CSV array");
      require(SDDS_SaveLayout(&editor.dataset), "save shorter array layout");
      ArrayStore shorter;
      shorter.dims = {1};
      shorter.values = {QString()};
      editor.pages[0].arrays.append(shorter);
      editor.populateModels();
      require(editor.writeCSV(path), "export arrays of unequal length");
      check(readFile(path).contains("Arrays\nA,B\n10,0\n20,\n30,\n40,\n"),
            "CSV leaves missing array elements blank while empty numeric elements export as zero");
      check(editor.dirty, "successful CSV export does not mark the SDDS document saved");
#ifndef _WIN32
      const QByteArray exported = readFile(path);
      const QString link = root + "/export-link.csv";
      require(QFile::link(path, link), "create CSV destination link");
      editor.pages[0].columns[0][0] = "8";
      require(editor.writeCSV(link), "export through a CSV symlink");
      check(QFileInfo(link).isSymLink() && readFile(path) != exported && readFile(link) == readFile(path),
            "CSV replacement preserves the destination symlink and updates its target");
#endif
    }
    {
      SDDSEditor editor;
      setup(editor);
      applyCellEditWithUndo(editor.undoStack, editor.columnModel, editor.columnModel->index(0, 0), "9");
      editor.openArrayViewer(0);
      ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      viewer->table()->edit(viewer->sliceModel()->index(0, 0));
      QCoreApplication::processEvents();
      QLineEdit *cell = viewer->table()->viewport()->findChild<QLineEdit *>();
      require(cell && !cell->isHidden(), "open viewer edit before main Undo");
      cell->setText("123");
      QAction *undo = nullptr;
      for (QAction *action : editor.findChildren<QAction *>())
        if (action->text() == "Undo" && action->parent() != viewer)
          undo = action;
      require(undo && undo->isEnabled(), "main Undo action is available");
      undo->trigger();
      check(editor.pages[0].columns[0][0] == "9" && editor.pages[0].arrays[0].values[0] == "10",
            "main Undo commits and undoes the newest pending viewer edit");
      QAction *redo = nullptr;
      for (QAction *action : editor.findChildren<QAction *>())
        if (action->text() == "Redo" && action->parent() != viewer)
          redo = action;
      require(redo && redo->isEnabled(), "main Redo action is available");
      redo->trigger();
      check(editor.pages[0].columns[0][0] == "9" && editor.pages[0].arrays[0].values[0] == "123",
            "main Redo restores the committed viewer edit");
    }
    require(failures == 0, "data preservation regressions");
  }

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
      QTimer answer;
      QObject::connect(&answer, &QTimer::timeout, [&]() {
        for (QWidget *widget : QApplication::topLevelWidgets())
          if (QMessageBox *box = qobject_cast<QMessageBox *>(widget))
            if (box->isVisible()) {
              prompted = true;
              box->done(QMessageBox::Cancel);
            }
      });
      // An overdue global accepter would otherwise answer the prompt first and choose "accept".
      messageBoxAccepter->stop();
      answer.start(0);
      const bool canClose = editor.maybeSave();
      answer.stop();
      messageBoxAccepter->start();
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
    putFile(root + "/version.001", "version one");
    putFile(root + "/version.002", "version two");
    if (makeSymbolicLink(root + "/version.001", root + "/current")) {
      require(editor.writeFile(root + "/current"), "save next unused version");
      require(readFile(root + "/version.001") == "version one", "preserve linked version");
      require(readFile(root + "/version.002") == "version two", "preserve intervening version");
      require(QFileInfo(root + "/current").symLinkTarget() == root + "/version.003", "link advances past collision");
      SDDSEditor reloaded;
      require(reloaded.loadFile(root + "/current"), "reload through the updated version link");
    } else {
#ifdef _WIN32
      fprintf(stdout, "SKIP versioned symlink save: this account cannot create symbolic links\n");
#else
      require(false, "create version link");
#endif
    }
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

  /** Panel search covers its original column scope while cycling visible matches. */
  static void panelSearch() {
    SDDSEditor editor;
    setup(editor);
    require(SDDS_DefineColumn(&editor.dataset, "Y", nullptr, nullptr, nullptr,
                              nullptr, SDDS_STRING, 0) >= 0, "define second search column");
    require(SDDS_DefineColumn(&editor.dataset, "Z", nullptr, nullptr, nullptr,
                              nullptr, SDDS_STRING, 0) >= 0, "define third search column");
    require(SDDS_SaveLayout(&editor.dataset), "save panel search layout");
    editor.pages[0].columns = {{"3", "1", "2"}, {"Needle", "other", "needle"},
                               {"needle", "other", "needle"}};
    editor.populateModels();
    editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
    editor.columnView->clearSelection();
    editor.columnSearchEdit->setText("needle");
    auto find = [&](int row, int column, const char *label) {
      editor.findInColumnPanel();
      require(editor.columnView->currentIndex() == editor.columnModel->index(row, column), label);
    };
    find(0, 1, "no selection searches beyond current first column, ignoring case");
    find(0, 2, "search result selection does not narrow all-column scope");
    find(2, 1, "search advances across rows and columns");
    find(2, 2, "search reaches last column");
    find(0, 1, "all-column search wraps");
    editor.columnView->selectColumn(2);
    find(0, 2, "user selection changes search scope");
    find(2, 2, "selected column search stays in that column");
    find(0, 2, "selected column search wraps");
    editor.columnView->selectColumn(1);
    editor.columnView->selectionModel()->select(editor.columnModel->index(0, 2), QItemSelectionModel::Select);
    find(0, 1, "multiple selected columns search from first match");
    find(0, 2, "multiple selected columns retain scope after first result");
    editor.rowFilterExpression = "X<3";
    editor.rowFilterActive = true;
    editor.applyColumnRowFilter();
    find(2, 1, "search skips filtered rows");
    editor.columnView->clearSelection();
    editor.clearColumnRowFilter();
    editor.columnSearchEdit->setText("3");
    find(0, 0, "clearing selection and changing query searches all columns again");
    editor.pages.append(editor.pages[0]);
    editor.pages[1].columns[1][0] = "page two";
    editor.pageChanged(1);
    editor.columnSearchEdit->setText("page two");
    find(0, 1, "page change resets panel search scope and position");
    fprintf(stdout, "PASS panel search scope, navigation, filtering, and page changes\n");
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
    fields[1]->setText("abc");
    applyCellEditWithUndo(editor.undoStack, editor.columnModel, editor.columnModel->index(1, 0), "redo target");
    editor.undoStack->undo();
    const int undoCount = editor.undoStack->count();
    click("Replace All");
    require(editor.undoStack->count() == undoCount && editor.undoStack->canRedo(),
            "identical Replace All does not create an undo macro or discard redo");
    editor.columnView->selectAll();
    click("Replace Selected");
    require(editor.undoStack->count() == undoCount && editor.undoStack->canRedo(),
            "identical Replace Selected does not create an undo macro or discard redo");
    click("Search");
    QLineEdit *pending = qobject_cast<QLineEdit *>(editor.columnView->indexWidget(editor.columnModel->index(0, 0)));
    require(pending, "search opens persistent cell editor");
    pending->setText("edited abc");
    click("Search");
    require(editor.pages[0].columns[0][0] == "edited abc", "search preserves pending persistent cell edit");
    editor.columnModel->setData(editor.columnModel->index(2, 0), "abc");
    click("Next");
    require(editor.columnView->currentIndex().row() == 0, "Next rebuilds invalidated search matches");
    click("Next");
    require(editor.columnView->currentIndex().row() == 2, "Next advances to another search result");
    click("Previous");
    require(editor.columnView->currentIndex().row() == 0, "Previous returns to the earlier search result");
    editor.columnModel->setData(editor.columnModel->index(2, 0), "tail");
    editor.columnModel->setData(editor.columnModel->index(0, 0), "abc");
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
        edits[1]->setText("abc");
        const int undoCount = editor.undoStack->count();
        press("Replace All");
        require(editor.undoStack->count() == undoCount, "identical array Replace All does not create an undo macro");
        QLineEdit *pending = qobject_cast<QLineEdit *>(editor.arrayView->indexWidget(editor.arrayModel->index(0, 0)));
        require(pending, "array search opens persistent cell editor");
        pending->setText("edited abc");
        press("Search");
        require(editor.pages[1].arrays[0].values[0] == "edited abc", "array search preserves pending persistent cell edit");
        editor.arrayModel->setData(editor.arrayModel->index(2, 0), "abc");
        press("Next");
        press("Next");
        require(editor.arrayView->currentIndex().row() == 2, "array Next advances after rebuilding matches");
        press("Previous");
        require(editor.arrayView->currentIndex().row() == 0, "array Previous returns to the earlier result");
        editor.arrayModel->setData(editor.arrayModel->index(2, 0), "tail");
        editor.arrayModel->setData(editor.arrayModel->index(0, 0), "abc");
        edits[0]->setText("zz");
        edits[1]->setText("new");
        press("Replace");
        edits[0]->setText("abc");
        press("Search");
        pending = qobject_cast<QLineEdit *>(editor.arrayView->indexWidget(editor.arrayModel->index(0, 0)));
        require(pending, "array close test opens persistent editor");
        pending->setText("closed abc");
        arrayDialog->accept();
        return;
      }
      require(false, "array search dialog exists");
    });
    editor.searchArray(0);
    require(editor.pages[1].arrays[0].values[0] == "closed abc" && editor.pages[1].arrays[0].values[1] == "new",
            "array query change replaces correct text and closing commits pending edit");
    fprintf(stdout, "PASS search invalidation, pending edits, and no-op replacements\n");
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

  /** Parameters wrap into side-by-side groups while editing tools keep parameter order. */
  static void parameterGrid(const QString &root) {
    SDDSEditor editor;
    setup(editor);
    for (int i = 0; i < 12; ++i)
      require(SDDS_DefineParameter(&editor.dataset, qPrintable(QString("P%1").arg(i)), nullptr, nullptr, nullptr,
                                   nullptr, i % 3 == 2 ? SDDS_STRING : SDDS_LONG, nullptr) >= 0,
              "define grid parameter");
    require(SDDS_SaveLayout(&editor.dataset), "save grid layout");
    editor.pages[0].parameters.clear();
    for (int i = 0; i < 12; ++i)
      editor.pages[0].parameters.append(i % 3 == 2 ? QString("s%1").arg(i) : QString::number(i));
    editor.populateModels();
    editor.resize(1400, 800);
    editor.show();
    editor.activateWindow();
    QCoreApplication::processEvents();

    ParameterGridModel *grid = editor.paramGrid;
    require(grid->groupCount() > 1, "a wide panel wraps parameters into groups");
    const int per = grid->rowsPerGroup();
    require(per * grid->groupCount() >= 12 && per * (grid->groupCount() - 1) < 12,
            "groups hold every parameter once");
    for (int i = 0; i < 12; ++i) {
      const QModelIndex cell = editor.parameterValueCell(i);
      require(grid->parameterRow(cell) == i && cell.data().toString() == editor.pages[0].parameters[i],
              "grid cells map to their parameters");
      require(grid->mapFromSource(editor.paramModel->index(i, ParameterPageModel::ValueColumn)) == cell,
              "parameter values map back to their grid cells");
    }
    require(!(grid->flags(grid->cellFor(0, ParameterGridModel::NameField)) & Qt::ItemIsSelectable),
            "parameter names are not selectable cells");
    require(editor.paramView->rowHeight(0) * per + editor.paramView->horizontalHeader()->height() <=
                editor.paramView->height(),
            "the compact parameter panel shows every grid row");
    editor.grab().save(root + "/parameter-grid.png");

    // Copy and paste follow parameter order across the boundary between groups.
    editor.paramView->setFocus();
    QCoreApplication::processEvents();
    require(QApplication::focusWidget() == editor.paramView, "parameter grid has focus");
    QItemSelectionModel *selection = editor.paramView->selectionModel();
    selection->clearSelection();
    selection->select(editor.parameterValueCell(per - 1), QItemSelectionModel::Select);
    selection->select(editor.parameterValueCell(per), QItemSelectionModel::Select);
    editor.copy();
    require(QApplication::clipboard()->text() ==
                editor.pages[0].parameters[per - 1] + "\n" + editor.pages[0].parameters[per],
            "copying across groups lists parameters in order");
    const QVector<QString> original = editor.pages[0].parameters;
    QApplication::clipboard()->setText("100\n101");
    editor.paramView->setCurrentIndex(editor.parameterValueCell(per - 1));
    editor.paste();
    require(editor.pages[0].parameters[per - 1] == "100" && editor.pages[0].parameters[per] == "101",
            "paste fills successive parameters across groups");

    // Undo targets the same parameters after the grid reflows.
    const int groupsBefore = grid->groupCount();
    editor.resize(760, 800);
    QCoreApplication::processEvents();
    require(grid->groupCount() < groupsBefore, "a narrower panel uses fewer groups");
    editor.undoStack->undo();
    require(editor.pages[0].parameters == original, "undo after reflow restores the pasted parameters");
    require(applyCellEditWithUndo(editor.undoStack, grid, editor.parameterValueCell(4), "44"),
            "edit through the grid");
    editor.resize(1400, 800);
    QCoreApplication::processEvents();
    editor.undoStack->undo();
    require(editor.pages[0].parameters[4] == "4", "grid edits undo by parameter after reflow");

    // Selection survives a reflow.
    selection->clearSelection();
    selection->select(editor.parameterValueCell(7), QItemSelectionModel::Select);
    editor.resize(760, 800);
    QCoreApplication::processEvents();
    require(editor.selectedParameterRows() == QSet<int>({7}), "reflow keeps the selected parameter");

    // Move Up/Down replaces dragging row headers.
    editor.paramView->selectionModel()->clearSelection();
    editor.paramView->setCurrentIndex(editor.parameterValueCell(1));
    editor.moveParameters(1, -1);
    require(QString(editor.dataset.layout.parameter_definition[0].name) == "P1" &&
                editor.pages[0].parameters[0] == "1",
            "move up reorders definitions and values");
    require(grid->parameterRow(editor.paramView->currentIndex()) == 0, "the moved parameter stays current");
    editor.moveParameters(0, 1);
    require(QString(editor.dataset.layout.parameter_definition[1].name) == "P1", "move down");
    editor.undoStack->undo();
    editor.undoStack->undo();
    require(QString(editor.dataset.layout.parameter_definition[0].name) == "P0" &&
                editor.pages[0].parameters == original,
            "undo restores the parameter order");

    // The outline lists definitions and filters them by name.
    QCoreApplication::processEvents();
    QTreeWidgetItem *parameters = editor.outlineTree->topLevelItem(0);
    require(parameters->childCount() == 12, "outline lists every parameter");
    editor.outlineFilter->setText("P1");
    int shown = 0;
    for (int i = 0; i < parameters->childCount(); ++i)
      shown += parameters->child(i)->isHidden() ? 0 : 1;
    require(shown == 3, "outline filter keeps matching names");
    editor.outlineFilter->clear();
    editor.navigateToOutlineItem(parameters->child(9));
    require(grid->parameterRow(editor.paramView->currentIndex()) == 9, "outline selects the parameter");
    require(editor.cellStatusLabel->text().contains("P9"), "status bar names the current parameter");
    editor.dirty = false;
    fprintf(stdout, "PASS parameter grid wrapping, clipboard, undo, reordering and outline\n");
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
    ParameterGridModel *grid = editor.paramGrid;
    require(grid->fieldCount() == 5 && grid->fieldAt(2) == ParameterGridModel::UnitsField &&
                grid->fieldAt(4) == ParameterGridModel::DescriptionField,
            "used units and description fields are shown");
    require(grid->index(0, 0).data().toString() == "Energy", "parameter grid shows the name");
    const QModelIndex typeCell = params->index(0, ParameterPageModel::TypeColumn);
    require(!(params->flags(typeCell) & (Qt::ItemIsEditable | Qt::ItemIsSelectable)), "metadata cells are read-only");
    const int undoCount = editor.undoStack->count();
    require(!applyCellEditWithUndo(editor.undoStack, params, typeCell, "long"), "metadata edits are rejected");
    require(editor.undoStack->count() == undoCount, "rejected metadata edit adds no undo entry");
    editor.paramView->setCurrentIndex(grid->cellFor(0, ParameterGridModel::TypeField));
    require(grid->fieldAt(editor.paramView->currentIndex().column()) == ParameterGridModel::ValueField,
            "clicking metadata keeps the value cell current");
    editor.paramView->setCurrentIndex(grid->cellFor(0, ParameterGridModel::NameField));
    require(grid->fieldAt(editor.paramView->currentIndex().column()) == ParameterGridModel::ValueField,
            "clicking a name keeps the value cell current");
    require(editor.columnModel->headerData(0, Qt::Horizontal, HeaderTypeRole).toInt() == SDDS_LONG64,
            "column header carries the type for its badge");
    require(editor.arrayModel->headerData(0, Qt::Horizontal, HeaderShapeRole).toString() ==
                QString("2") + QChar(0x00D7) + "2",
            "array header carries the shape chip");
    require(editor.outlineTree->topLevelItem(1)->childCount() == 1 &&
                editor.outlineTree->topLevelItem(1)->child(0)->text(0) == "X",
            "outline lists the columns");
    editor.navigateToOutlineItem(editor.outlineTree->topLevelItem(0)->child(0));
    require(editor.paramGrid->parameterRow(editor.paramView->currentIndex()) == 0,
            "outline entries select their parameter");
    require(editor.outlineVersionValue->text().startsWith("SDDS"), "outline shows the SDDS version");

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
    BadgeToolButton *messages = static_cast<BadgeToolButton *>(editor.messagesButton);
    require(messages->badge() == 1 && messages->accessibleName().contains("1 unread"),
            "hidden log counts unread messages");
    editor.messagesButton->setChecked(true);
    QCoreApplication::processEvents();
    require(editor.consoleDock->isVisible() && messages->badge() == 0,
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

  /** Help opens one non-modal window whose topic list, find bar and theme follow the editor. */
  static void helpWindow(const QString &root) {
    SDDSEditor editor;
    editor.show();
    QCoreApplication::processEvents();
    editor.showHelp();
    QCoreApplication::processEvents();
    EditorHelpDialog *help = editor.helpDialog;
    require(help && help->isVisible() && help->windowModality() == Qt::NonModal,
            "help opens as a non-modal window");
    editor.showHelp();
    require(editor.helpDialog == help, "a second Help request reuses the open window");

    QListWidget *topics = help->findChild<QListWidget *>("helpTopics");
    QTextBrowser *browser = help->findChild<QTextBrowser *>("helpBrowser");
    QLineEdit *find = help->findChild<QLineEdit *>("helpFind");
    QLabel *matches = help->findChild<QLabel *>("helpMatches");
    require(topics && browser && find && matches, "help window has topics, text and a find bar");
    require(topics->count() == editorHelpTopics().size() && topics->currentRow() == 0, "help lists every topic");

    const QString text = browser->toPlainText();
    require(text.contains("%2F and %25") && !text.contains("%1") && !text.contains("%3"),
            "help text has its shortcuts substituted and literal percent signs intact");
    require(text.contains(QKeySequence("Ctrl+Shift+E").toString(QKeySequence::NativeText)) &&
                text.contains("Apply Numerical Expression"),
            "help text shows the formula shortcuts");
    const QString html = browser->toHtml();
    for (const EditorHelpTopic &topic : editorHelpTopics())
      require(html.contains(QString("name=\"%1\"").arg(topic.anchor)), "every help topic has an anchor");

    // Choosing a topic scrolls to it; scrolling selects the topic being read.
    QScrollBar *bar = browser->verticalScrollBar();
    int findRow = -1, formulaRow = -1;
    for (int row = 0; row < topics->count(); ++row) {
      const QString anchor = topics->item(row)->data(Qt::UserRole).toString();
      if (anchor == "find")
        findRow = row;
      else if (anchor == "formula")
        formulaRow = row;
    }
    require(findRow > 0 && formulaRow > findRow, "help has search and formula topics");
    topics->setCurrentRow(findRow);
    const int findTop = bar->value();
    require(findTop > 0 && findTop < bar->maximum(), "choosing a topic scrolls to it");
    bar->setValue(0);
    require(topics->currentRow() == 0, "scrolling to the top selects the first topic");
    bar->setValue(findTop);
    require(topics->currentRow() == findRow, "scrolling to a heading selects its topic");
    browser->setSource(QUrl("#formula"));
    QCoreApplication::processEvents();
    require(browser->toPlainText() == text && topics->currentRow() == formulaRow,
            "links between topics scroll within the help text");

    find->setText("ANCHOR");
    const int found = browser->extraSelections().size();
    require(found > 1 && matches->text().endsWith(QString(" of %1").arg(found)),
            "find highlights every match and counts them");
    const QString before = matches->text();
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(find, &enter);
    require(help->isVisible() && matches->text() != before,
            "Enter in the find field moves to the next match without closing help");
    for (const QTextEdit::ExtraSelection &selection : browser->extraSelections())
      require(selection.cursor.selectedText().compare("anchor", Qt::CaseInsensitive) == 0,
              "find highlights only matching text");
    help->findNext(-1);
    require(matches->text() == before, "find steps back to the previous match");
    help->grab().save(root + "/help-light.png");

    // A theme change restyles the open window and keeps the reading position and matches.
    const int scroll = bar->value();
    editor.applyTheme(true);
    QCoreApplication::processEvents();
    require(help->styleSheet().contains(editorTheme(true).win.name()) &&
                browser->document()->defaultStyleSheet().contains(editorTheme(true).accentText.name()),
            "help follows the dark theme");
    require(bar->value() == scroll && browser->extraSelections().size() == found &&
                matches->text() == before,
            "theme change keeps the help position and matches");
    help->grab().save(root + "/help-dark.png");
    editor.applyTheme(false);

    find->setText("no such help text");
    require(browser->extraSelections().isEmpty() && matches->text() == "No matches",
            "find reports text that is not in the help");

    editor.close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    require(!editor.helpDialog, "closing the editor closes its help window");
    fprintf(stdout, "PASS help window topics, find, links and themes\n");
  }

  /** The About dialog keeps its message and runs an interactive animation only while shown. */
  static void aboutDialog(const QString &root) {
    SDDSEditor editor;
    editor.show();
    QCoreApplication::processEvents();
    bool checked = false;
    QPointer<QTimer> bannerTimer;
    acceptDialog("About SDDS Editor", [&](QDialog *dialog) {
      QLabel *message = dialog->findChild<QLabel *>("aboutMessage");
      require(message && message->text().contains("Robert Soliday") && message->text().contains("caffeine") &&
                  message->text().contains("infinite loop"),
              "about dialog keeps the author message");
      AboutBanner *banner = static_cast<AboutBanner *>(dialog->findChild<QWidget *>("aboutBanner"));
      require(banner && banner->isAnimating(), "about animation runs while the dialog is shown");
      bannerTimer = banner->findChild<QTimer *>();

      const qreal full = banner->coffeeLevel();
      const int fortune = banner->fortuneIndex();
      banner->advance(5.0);
      require(banner->coffeeLevel() < full, "the coffee drains over time");
      require(banner->fortuneIndex() != fortune, "the one-liners rotate");

      const qreal drained = banner->coffeeLevel();
      const QPointF mug = banner->mugRect().center();
      QMouseEvent click(QEvent::MouseButtonPress, mug, QPointF(banner->mapToGlobal(mug.toPoint())),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      QCoreApplication::sendEvent(banner, &click);
      require(banner->particleCount() > 0, "clicking the mug sprays digits");
      banner->advance(0.35);
      require(banner->coffeeLevel() > drained, "clicking the mug refills it");
      banner->grab().save(root + "/about-banner.png");
      dialog->grab().save(root + "/about-light.png");
      banner->advance(3.0);
      require(banner->particleCount() == 0 && banner->coffeeLevel() > 0.95,
              "digits fade away and the refilled mug is nearly full");

      const QPointF corner(20, 20);
      QMouseEvent toss(QEvent::MouseButtonPress, corner, QPointF(banner->mapToGlobal(corner.toPoint())),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      QCoreApplication::sendEvent(banner, &toss);
      require(banner->particleCount() > 0, "clicking elsewhere tosses digits");

      banner->advance(100.0);
      require(banner->coffeeLevel() < 0.12, "an untouched mug runs dry");
      banner->grab().save(root + "/about-banner-empty.png");
      checked = true;
    });
    editor.showAbout();
    require(checked, "about dialog was shown");
    require(!bannerTimer, "closing the about dialog stops and frees its animation");

    editor.applyTheme(true);
    acceptDialog("About SDDS Editor", [&](QDialog *dialog) {
      static_cast<AboutBanner *>(dialog->findChild<QWidget *>("aboutBanner"))->advance(1.0);
      dialog->grab().save(root + "/about-dark.png");
    });
    editor.showAbout();
    editor.applyTheme(false);
    fprintf(stdout, "PASS about dialog message, animation, refills and lifecycle\n");
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

  /** Regressions for CSV newlines, fixed values on inserted pages, field lengths and no-op undo steps. */
  static void definitionAndPageFixes(const QString &root) {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      if (!ok)
        ++failures;
    };
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineColumn(&editor.dataset, "Text", nullptr, nullptr, nullptr, nullptr,
                                SDDS_STRING, 0) >= 0, "define CSV text column");
      require(SDDS_SaveLayout(&editor.dataset), "save CSV text layout");
      editor.pages[0].columns.append({"a\nb", "c", "d"});
      editor.populateModels();
      const QString path = root + "/newline.csv";
      require(editor.writeCSV(path), "export text containing a newline");
      const QByteArray csv = readFile(path);
      check(csv.contains("3,\"a\nb\"\n") && !csv.contains('\r'),
            "CSV keeps newlines inside text values and uses LF on every platform");
    }
    {
      SDDSEditor editor;
      setup(editor);
      char fixedValue[] = "5";
      require(SDDS_DefineParameter(&editor.dataset, "F", nullptr, nullptr, nullptr, nullptr,
                                   SDDS_DOUBLE, fixedValue) >= 0, "define fixed parameter");
      require(SDDS_SaveLayout(&editor.dataset), "save fixed parameter layout");
      editor.pages[0].parameters = {"5"};
      editor.populateModels();
      editor.insertPage();
      require(editor.pages.size() == 2 && editor.currentPage == 1, "inserted page is shown");
      check(editor.pages[1].parameters[0] == "5", "inserted page shows the fixed parameter value");
      const QString path = root + "/inserted-page-fixed.sdds";
      require(editor.writeFile(path), "save document with an inserted page");
      check(QString(editor.dataset.layout.parameter_definition[0].fixed_value) == "5" &&
                editor.pages[0].parameters[0] == "5",
            "showing an inserted page does not overwrite the fixed value");
      SDDSEditor loaded;
      require(loaded.loadFile(path), "reload inserted-page document");
      check(loaded.pages.size() == 2 && loaded.pages[1].parameters[0] == "5" &&
                QString(loaded.dataset.layout.parameter_definition[0].fixed_value) == "5",
            "inserted page saves the fixed parameter value");
    }
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineColumn(&editor.dataset, "S", nullptr, nullptr, nullptr, nullptr,
                                SDDS_STRING, -10) >= 0, "define trimmed string column");
      require(SDDS_DefineArray(&editor.dataset, "T", nullptr, nullptr, nullptr, nullptr,
                               SDDS_STRING, -6, 1, nullptr) >= 0, "define trimmed string array");
      require(SDDS_SaveLayout(&editor.dataset), "save trimmed string layout");
      editor.pages[0].columns.append({"a", "b", "c"});
      ArrayStore text;
      text.dims = {1};
      text.values = {"t"};
      editor.pages[0].arrays.append(text);
      editor.populateModels();
      acceptDialog("Column Attributes", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[2]->setText("m");
      });
      editor.editColumnAttributesAt(1);
      check(editor.dataset.layout.column_definition[1].field_length == -10 &&
                QString(editor.dataset.layout.column_definition[1].units) == "m",
            "column attribute editor keeps a negative string field length");
      acceptDialog("Array Attributes", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[2]->setText("s");
      });
      editor.editArrayAttributesAt(1);
      check(editor.dataset.layout.array_definition[1].field_length == -6 &&
                QString(editor.dataset.layout.array_definition[1].units) == "s",
            "array attribute editor keeps a negative string field length");
      const int undoSteps = editor.undoStack->count();
      acceptDialog("Column Attributes", [](QDialog *dialog) {
        for (QRadioButton *button : dialog->findChildren<QRadioButton *>())
          if (button->text() == "double")
            button->setChecked(true);
      });
      editor.editColumnAttributesAt(1);
      check(editor.dataset.layout.column_definition[1].type == SDDS_STRING &&
                editor.undoStack->count() == undoSteps,
            "a negative field length is refused for numeric columns");
      editor.pages[0].columns[1] = {"1", "2", "3"};
      acceptDialog("Column Type", [](QDialog *dialog) {
        static_cast<QInputDialog *>(dialog)->setTextValue("double");
      });
      editor.changeColumnType(1);
      check(editor.dataset.layout.column_definition[1].type == SDDS_DOUBLE &&
                editor.dataset.layout.column_definition[1].field_length == 10 &&
                editor.undoStack->undoText() == "Change Column Type",
            "changing a trimmed string column to numeric keeps a valid field width and undo");
      acceptDialog("Array Type", [](QDialog *dialog) {
        static_cast<QInputDialog *>(dialog)->setTextValue("long");
      });
      editor.pages[0].arrays[1].values = {"7"};
      editor.changeArrayType(1);
      check(editor.dataset.layout.array_definition[1].field_length == 6 &&
                editor.undoStack->undoText() == "Change Array Type",
            "changing a trimmed string array to numeric keeps a valid field width and undo");
      const QString path = root + "/field-length.sdds";
      check(editor.writeFile(path), "document stays savable after the type changes");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      applyCellEditWithUndo(editor.undoStack, editor.columnModel, editor.columnModel->index(0, 0), "9");
      editor.undoStack->undo();
      require(editor.undoStack->canRedo(), "redo is available before no-op edits");
      editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
      acceptDialog("Apply Text Formula", [](QDialog *dialog) {
        dialog->findChild<QLineEdit *>()->setText("${x}");
      });
      editor.applyTextFormula(editor.columnView);
      check(editor.undoStack->canRedo() && editor.undoStack->index() == 0,
            "a formula that changes nothing adds no undo step and keeps redo");
      editor.openArrayViewer(0);
      ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      viewer->table()->setCurrentIndex(viewer->sliceModel()->index(0, 0));
      require(viewer->pasteText("10"), "paste the unchanged viewer value");
      check(editor.undoStack->canRedo() && editor.undoStack->index() == 0,
            "an array viewer paste that changes nothing adds no undo step and keeps redo");
    }
    require(failures == 0, "definition and page regressions");
  }

  /** Computed underflow, no-op history, viewer dismissal and subnormal heatmaps. */
  static void portabilityAndEditingFixes() {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      failures += !ok;
    };
    bool floatOk = false;
    const float floatTiny = std::numeric_limits<float>::denorm_min();
    const float formattedTiny = numericResultText(floatTiny, SDDS_FLOAT).toFloat(&floatOk);
    check(floatOk && formattedTiny == floatTiny && numericResultText(-0.0L, SDDS_FLOAT) == "-0" &&
              !validateTextForType(numericResultText(static_cast<long double>(std::numeric_limits<float>::max()) * 2,
                                                   SDDS_FLOAT), SDDS_FLOAT, false),
          "computed float formatting preserves subnormals and signed zero and rejects overflow");
    const long double belowFloat = static_cast<long double>(std::numeric_limits<float>::denorm_min()) / 4;
    check(!validateTextForType(numericResultText(belowFloat, SDDS_FLOAT), SDDS_FLOAT, false),
          "computed float underflow is rejected instead of silently becoming zero");
    if (std::numeric_limits<long double>::min_exponent < std::numeric_limits<double>::min_exponent) {
      const long double belowDouble = static_cast<long double>(std::numeric_limits<double>::denorm_min()) / 4;
      check(!validateTextForType(numericResultText(belowDouble, SDDS_DOUBLE), SDDS_DOUBLE, false),
            "computed double underflow is rejected instead of silently becoming zero");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.column_definition[0].type = SDDS_FLOAT;
      require(SDDS_SaveLayout(&editor.dataset), "save float formula fixture layout");
      editor.populateModels();
      editor.columnView->setCurrentIndex(editor.columnModel->index(0, 0));
      const auto before = editor.pages[0].columns;
      const int history = editor.undoStack->count();
      acceptDialog("Apply Numerical Expression", [](QDialog *dialog) {
        dialog->findChild<QLineEdit *>()->setText("1e-50");
      });
      editor.applyNumericalExpression(editor.columnView);
      check(editor.pages[0].columns == before && editor.undoStack->count() == history,
            "a formula below the float range leaves cells and undo history unchanged");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.pages[0].columns[0] = {"1", "2", "3"};
      editor.populateModels();
      applyCellEditWithUndo(editor.undoStack, editor.columnModel, editor.columnModel->index(0, 0), "9");
      editor.undoStack->undo();
      editor.dirty = false;
      editor.sortColumn(0, Qt::AscendingOrder);
      check(editor.undoStack->canRedo() && editor.undoStack->index() == 0 && !editor.dirty,
            "sorting already sorted rows preserves redo and the saved state");
    }
    {
      SDDSEditor editor;
      setup(editor);
      applyCellEditWithUndo(editor.undoStack, editor.arrayModel, editor.arrayModel->index(0, 0), "99");
      editor.undoStack->undo();
      editor.dirty = false;
      acceptDialog("Resize Array", [](QDialog *) {});
      editor.resizeArray(0);
      check(editor.undoStack->canRedo() && editor.undoStack->index() == 0 && !editor.dirty,
            "accepting unchanged array dimensions preserves redo and the saved state");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      editor.openArrayViewer(0);
      ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      const QModelIndex index = viewer->sliceModel()->index(0, 0);
      viewer->table()->openPersistentEditor(index);
      QCoreApplication::processEvents();
      QLineEdit *cell = qobject_cast<QLineEdit *>(viewer->table()->indexWidget(index));
      require(cell && !cell->isHidden(), "pending array editor for dismissal");
      cell->setText("77");
      viewer->reject(); // QDialog's Escape path bypasses closeEvent.
      check(editor.pages[0].arrays[0].values[0] == "77" && editor.undoStack->canUndo(),
            "dismissing the array viewer commits its pending edit");
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.array_definition[0].type = SDDS_LONGDOUBLE;
      require(SDDS_SaveLayout(&editor.dataset), "save subnormal heatmap layout");
      const long double tiny = std::numeric_limits<long double>::denorm_min();
      editor.pages[0].arrays[0].values = {longDoubleToText(tiny), longDoubleToText(2 * tiny), "nan", "inf"};
      editor.populateModels();
      editor.openArrayViewer(0);
      ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      long double minimum = 0, maximum = 0;
      check(viewer->sliceModel()->finiteRange(&minimum, &maximum) && minimum == tiny && maximum == 2 * tiny,
            "heatmap bounds include finite subnormal values");
      viewer->findChild<QCheckBox *>("arrayHeatmap")->setChecked(true);
      viewer->findChild<QComboBox *>("arrayHeatmapScale")->setCurrentIndex(1);
      viewer->findChild<QPushButton *>("arrayHeatmapApply")->click();
      check(parseLongDoubleStrict(viewer->findChild<QLineEdit *>("arrayHeatmapMinimum")->text(), &minimum) &&
                parseLongDoubleStrict(viewer->findChild<QLineEdit *>("arrayHeatmapMaximum")->text(), &maximum) &&
                minimum == tiny && maximum == 2 * tiny &&
                !viewer->findChild<QLabel *>("arrayHeatmapStatus")->text().contains("previous range"),
            "subnormal heatmap limits round-trip through fixed range controls");
    }
    require(failures == 0, "portability and editing regressions");
  }

  /** Bracketed and case-sensitive filter names, literal plot names, exact integers, NaN results. */
  static void namesAndExactNumbers(const QString &root) {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      if (!ok)
        ++failures;
    };
    {
      SDDSEditor editor;
      setup(editor);
      for (const char *name : {"Q[0]", "x", "i"})
        require(SDDS_DefineColumn(&editor.dataset, name, nullptr, nullptr, nullptr, nullptr,
                                  SDDS_DOUBLE, 0) >= 0, "define filter name column");
      require(SDDS_SaveLayout(&editor.dataset), "save filter name layout");
      editor.pages[0].columns = {{"3", "1", "2"}, {"5", "0", "7"}, {"0", "10", "0"}, {"7", "7", "0"}};
      editor.populateModels();
      int visible = 0;
      auto filter = [&](const QString &expression) {
        editor.rowFilterExpression = expression;
        editor.rowFilterActive = true;
        return editor.applyColumnRowFilter(nullptr, &visible);
      };
      check(filter("[Q[0]] > 1") && visible == 2 && editor.columnView->isRowHidden(1),
            "a bracketed filter name may itself contain brackets");
      check(filter("x > 5") && visible == 1 && !editor.columnView->isRowHidden(1),
            "a filter name matches its own case before a differently cased column");
      check(filter("X > 2") && visible == 1 && !editor.columnView->isRowHidden(0),
            "an uppercase filter name keeps its own column");
      check(filter("[i] == 7") && visible == 2 && editor.columnView->isRowHidden(2),
            "a bracketed name is the column even when it matches a row variable");
      check(filter("i == 2") && visible == 1 && !editor.columnView->isRowHidden(2),
            "a bare i is still the row index");
      editor.clearColumnRowFilter();
      for (int column : {1, 3}) {
        editor.rowFilterExpression.clear();
        editor.columnView->setCurrentIndex(editor.columnModel->index(0, column));
        acceptDialog("Filter/View Rows", [](QDialog *) {});
        editor.filterColumnRows();
        if (column == 1)
          check(editor.rowFilterActive && editor.rowFilterExpression == "[Q[0]] > 0" &&
                    editor.visibleColumnRows == 2,
                "the suggested filter for a column named with brackets works");
        else
          check(editor.rowFilterActive && editor.rowFilterExpression == "[i] > 0" &&
                    !editor.columnView->isRowHidden(0) && editor.columnView->isRowHidden(2),
                "the suggested filter for a column named i filters that column");
        editor.clearColumnRowFilter();
      }

      const QString capture = root + "/bracket-plot.sdds";
      const QByteArray oldPath = qgetenv("PATH");
      qputenv("PATH", QFile::encodeName(QCoreApplication::applicationDirPath() + "/test-bin") + QDir::listSeparator().toLatin1() + oldPath);
      qputenv("SDDSEDITOR_PLOT_CAPTURE", QFile::encodeName(capture));
      editor.plotColumn(1);
      QElapsedTimer timer;
      timer.start();
      while (!editor.findChildren<QProcess *>().isEmpty() && timer.elapsed() < 10000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      }
      qputenv("PATH", oldPath);
      qunsetenv("SDDSEDITOR_PLOT_CAPTURE");
      require(QFileInfo::exists(capture + ".args"), "plot helper records bracketed name arguments");
      check(readFile(capture + ".args").split('\n').contains("-col=Q\\[0\\]"),
            "plot escapes sddsplot wildcard characters in a column name");
    }
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineColumn(&editor.dataset, "U", nullptr, nullptr, nullptr, nullptr,
                                SDDS_ULONG64, 0) >= 0, "define ulong64 column");
      require(SDDS_DefineColumn(&editor.dataset, "D", nullptr, nullptr, nullptr, nullptr,
                                SDDS_DOUBLE, 0) >= 0, "define double column");
      require(SDDS_SaveLayout(&editor.dataset), "save exact integer layout");
      editor.pages[0].columns = {{"0", "0", "0"}, {"0", "0", "0"}, {"4", "-1", "0"}};
      editor.populateModels();
      auto selectColumn = [&](int column) {
        editor.columnView->setCurrentIndex(editor.columnModel->index(0, column));
        editor.columnView->selectionModel()->select(
            QItemSelection(editor.columnModel->index(0, column), editor.columnModel->index(2, column)),
            QItemSelectionModel::ClearAndSelect);
      };
      auto fill = [&](int column, const QString &start, const QString &step) {
        selectColumn(column);
        acceptDialog("Fill Series", [start, step](QDialog *dialog) {
          const auto fields = dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
          fields[0]->setText(start);
          fields[1]->setText(step);
        });
        editor.fillSeries(editor.columnView);
      };
      auto apply = [&](int column, const QString &expression) {
        selectColumn(column);
        acceptDialog("Apply Numerical Expression", [expression](QDialog *dialog) {
          dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[0]->setText(expression);
        });
        editor.applyNumericalExpression(editor.columnView);
      };
      fill(0, "9007199254740993", "1");
      check(editor.pages[0].columns[0] ==
                QVector<QString>({"9007199254740993", "9007199254740994", "9007199254740995"}),
            "a long64 fill series beyond 2^53 is exact");
      fill(1, "18446744073709551613", "1");
      check(editor.pages[0].columns[1] ==
                QVector<QString>({"18446744073709551613", "18446744073709551614", "18446744073709551615"}),
            "a ulong64 fill series up to its maximum is exact");
      apply(0, "x + 2*i - 1");
      check(editor.pages[0].columns[0] ==
                QVector<QString>({"9007199254740992", "9007199254740995", "9007199254740998"}),
            "integer expressions keep every digit of long64 values");
      apply(1, "-(a - x) + 0 * row");
      check(editor.pages[0].columns[1] ==
                QVector<QString>({"0", "1", "2"}),
            "integer expressions use the exact anchor value");
      editor.pages[0].columns[0] = {"6", "8", "10"};
      apply(0, "x / 2 + abs(-dr)");
      check(editor.pages[0].columns[0] == QVector<QString>({"3", "5", "7"}),
            "divisible integer expressions and abs are computed exactly");
      apply(2, "sqrt(x)");
      check(editor.pages[0].columns[2] == QVector<QString>({"2", "nan", "0"}),
            "a NaN expression result is stored as nan");
      check(numericResultText(-std::numeric_limits<long double>::quiet_NaN(), SDDS_DOUBLE) == "nan" &&
                numericResultText(-std::numeric_limits<long double>::infinity(), SDDS_FLOAT) == "-inf" &&
                validateTextForType(numericResultText(-std::numeric_limits<long double>::infinity(), SDDS_FLOAT),
                                    SDDS_FLOAT, false) &&
                !validateTextForType(numericResultText(std::numeric_limits<long double>::quiet_NaN(), SDDS_LONG),
                                     SDDS_LONG, false),
            "nonfinite results use text the validators accept for floating types only");
    }
    require(failures == 0, "name and exact number regressions");
  }

  /** Closing with open tool windows, non-ASCII export paths, viewer paste history, raw definition text. */
  static void windowsAndEncodingFixes(const QString &root) {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
      fprintf(stdout, "%s %s\n", ok ? "PASS" : "FAIL", label);
      failures += !ok;
    };
    {
      SDDSEditor editor;
      setup(editor);
      editor.show();
      editor.searchColumn(0);
      editor.openArrayViewer(0);
      QPointer<QDialog> search = editor.searchColumnDialog.data();
      QPointer<QDialog> viewer = editor.arrayViewers.last();
      QCoreApplication::processEvents();
      require(search && search->isVisible() && !search->parentWidget() && viewer && viewer->isVisible(),
              "open the parentless search dialog and an array viewer");
      editor.dirty = false;
      editor.close();
      QCoreApplication::processEvents();
      check(!editor.isVisible() && (!search || !search->isVisible()) && (!viewer || !viewer->isVisible()),
            "closing the editor closes its search dialog and array viewers");
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    {
      // HDF5 decodes Windows file names as UTF-8; the ANSI name fails here.
      SDDSEditor editor;
      setup(editor);
      const QString directory = root + "/hdf-" + QString::fromUtf8("d\xC3\xA9j\xC3\xA0");
      require(QDir().mkpath(directory), "create non-ASCII HDF export directory");
      const QString path = directory + "/export.h5";
      check(editor.writeHDF(path) && QFileInfo(path).size() > 0,
            "HDF export works in a directory with a non-ASCII name");
    }
    {
      SDDSEditor editor;
      setup(editor);
      editor.dataset.layout.column_definition[0].type = SDDS_STRING;
      require(SDDS_SaveLayout(&editor.dataset), "save CSV encoding layout");
      const QString micro = QString::fromUtf8("\xC2\xB5s");
      editor.pages[0].columns[0] = {micro, "a", "b"};
      editor.populateModels();
      const QString path = root + "/encoding.csv";
      require(editor.writeCSV(path), "export non-ASCII CSV text");
      if (localEncodingPreserves(micro))
        check(readFile(path).contains("\n" + micro.toLocal8Bit() + "\n"),
              "CSV text uses the same local encoding as SDDS text with Qt 5 and Qt 6");
    }
    {
      SDDSEditor editor;
      setup(editor);
      applyCellEditWithUndo(editor.undoStack, editor.arrayModel, editor.arrayModel->index(1, 0), "21");
      editor.undoStack->undo();
      editor.openArrayViewer(0);
      ArrayViewer *viewer = static_cast<ArrayViewer *>(editor.arrayViewers.last().data());
      viewer->table()->setCurrentIndex(viewer->sliceModel()->index(0, 0));
      const int history = editor.undoStack->index();
      require(viewer->pasteText("10.0"), "paste differently formatted equal value into the viewer");
      const bool stored = editor.pages[0].arrays[0].values[0] == "10.0" &&
                          editor.undoStack->index() == history + 1;
      editor.undoStack->undo();
      check(stored && editor.pages[0].arrays[0].values[0] == "10",
            "a viewer paste never leaves an empty undo step: pasted text is stored and undoable");
    }
    {
      SDDSEditor editor;
      setup(editor);
      require(SDDS_DefineParameter(&editor.dataset, "P", "sym", "m", "old", "%g", SDDS_DOUBLE, nullptr) >= 0,
              "define attribute text parameter");
      require(SDDS_SaveLayout(&editor.dataset), "save attribute text layout");
      editor.pages[0].parameters = {"1"};
      editor.populateModels();
      const PARAMETER_DEFINITION &parameter = editor.dataset.layout.parameter_definition[0];
      const char *parameterName = parameter.name;
      const char *parameterUnits = parameter.units;
      editor.paramView->setCurrentIndex(editor.parameterValueCell(0));
      acceptDialog("Parameter Attributes", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[3]->setText("new");
      });
      editor.editParameterAttributes();
      check(QByteArray(parameter.description) == "new" && parameter.name == parameterName &&
                parameter.units == parameterUnits,
            "editing a parameter description keeps the other stored attribute bytes");
      const COLUMN_DEFINITION &column = editor.dataset.layout.column_definition[0];
      const char *columnName = column.name;
      acceptDialog("Column Attributes", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[2]->setText("mm");
      });
      editor.editColumnAttributesAt(0);
      check(QByteArray(column.units) == "mm" && column.name == columnName,
            "editing column units keeps the stored column name");
      const ARRAY_DEFINITION &array = editor.dataset.layout.array_definition[0];
      const char *arrayName = array.name;
      acceptDialog("Array Attributes", [](QDialog *dialog) {
        dialog->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)[5]->setText("group");
      });
      editor.editArrayAttributesAt(0);
      check(QByteArray(array.group_name) == "group" && array.name == arrayName &&
                SDDS_GetArrayIndex(&editor.dataset, const_cast<char *>("A")) == 0,
            "editing an array group keeps the stored array name and index");
    }
    {
      // Bytes that do not decode (Latin-1 in a UTF-8 locale) are reported when loading.
      const QString path = root + "/undecodable.sdds";
      putFile(path, "SDDS1\n&column name=S, type=string, &end\n&data mode=ascii, &end\n1\n\xFF\xFE\n");
      const QByteArray stored("\xFF\xFE");
      const bool lossy = QString::fromLocal8Bit(stored).toLocal8Bit() != stored;
      SDDSEditor editor;
      require(editor.loadFile(path), "load text with non-UTF-8 bytes");
      check(editor.consoleEdit->toPlainText().contains("not valid in this system's character encoding") == lossy,
            "loading warns exactly when stored text cannot be preserved in this locale");
    }
    require(failures == 0, "window and encoding regressions");
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
  messageBoxAccepter = &warnings;
  SDDSEditorTests::expressionNesting();
  SDDSEditorTests::integerSeriesPrecision();
  SDDSEditorTests::xzInputLines(artifacts.path());
  SDDSEditorTests::integerFallbackPrecision();
  SDDSEditorTests::failedInputCleanup(artifacts.path());
  SDDSEditorTests::asciiPrecision(artifacts.path());
  SDDSEditorTests::longDefinitionText(artifacts.path());
  SDDSEditorTests::longTextTools(artifacts.path());
  SDDSEditorTests::windowsAndEncodingFixes(artifacts.path());
  SDDSEditorTests::portabilityAndEditingFixes();
  SDDSEditorTests::definitionNameEncoding();
  SDDSEditorTests::pendingTableActions();
  SDDSEditorTests::multilineSave(artifacts.path());
  SDDSEditorTests::additionalBugFixes();
  SDDSEditorTests::dataPreservation(artifacts.path());
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
  SDDSEditorTests::panelSearch();
  SDDSEditorTests::plot(artifacts.path());
  SDDSEditorTests::reviewFixes(artifacts.path());
  SDDSEditorTests::sparseClipboard();
  SDDSEditorTests::parameterGrid(artifacts.path());
  SDDSEditorTests::interfaceChrome(artifacts.path());
  SDDSEditorTests::helpWindow(artifacts.path());
  SDDSEditorTests::aboutDialog(artifacts.path());
  SDDSEditorTests::menuAndTextFixes();
  SDDSEditorTests::displayAndRangeFixes();
  SDDSEditorTests::definitionAndPageFixes(artifacts.path());
  SDDSEditorTests::namesAndExactNumbers(artifacts.path());
  SDDSEditorTests::longTextAndArrayActions(artifacts.path());
  SDDSEditorTests::fixedTextAndDefinitions(artifacts.path());
  SDDSEditorTests::shapeFilterAndRowFixes();
  fprintf(stdout, "PASS all sddseditor regressions\n");
  return 0;
}
