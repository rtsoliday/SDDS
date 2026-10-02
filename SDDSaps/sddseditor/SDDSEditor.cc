/**
 * @file SDDSEditor.cc
 * @brief Implementation of the Qt SDDS editor.
 */

// mdb.h includes windows.h; prevent its macros before any Qt/STL headers load.
#if defined(_WIN32) && !defined(NOMINMAX)
#  define NOMINMAX
#endif

#include "SDDSEditor.h"
#include "ArrayViewer.h"
#include "mdb.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QDockWidget>
#include <QInputDialog>
#include <QFont>
#include <QHeaderView>
#include <QScrollBar>
#include <QMenu>
#include <QProcess>
#include <QApplication>
#include <QCloseEvent>
#include <hdf5.h>
#include <QShortcut>
#include <QClipboard>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QButtonGroup>
#include <QSpinBox>
#include <QLineEdit>
#include <QStyledItemDelegate>
#include <QPersistentModelIndex>
#include <QEvent>
#include <QUndoStack>
#include <QRegularExpression>
#include <QItemSelectionModel>
#include <QSet>
#include <QHash>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QCoreApplication>
#include <QProgressDialog>
#include <QAbstractTableModel>
#include <functional>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <limits>
#include <cmath>
#include <QLocale>
#include <QResizeEvent>
#include <QToolBar>
#include <QToolButton>
#include <QStatusBar>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QFontInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QStyleHints>
#include <QSplitterHandle>
#include <QPaintEvent>
#include <QMimeData>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QScopedValueRollback>

/*
 * On Windows, some headers define min/max as macros, which breaks code like
 * std::numeric_limits<T>::min()/max() by macro expansion.
 */
#if defined(_WIN32)
#  if defined(min)
#    undef min
#  endif
#  if defined(max)
#    undef max
#  endif
#endif

static bool validateTextForType(const QString &text, int type, bool showMessage);

/** Text values may exceed QLineEdit's default 32,767-character edit limit. */
class SDDSTextEdit : public QLineEdit {
public:
  explicit SDDSTextEdit(QWidget *parent = nullptr) : QLineEdit(parent) {
    setMaxLength(std::numeric_limits<int>::max());
  }

  SDDSTextEdit(const QString &text, QWidget *parent) : SDDSTextEdit(parent) {
    setText(text);
  }
};

static void configureEditorPopupDialog(QDialog *dialog, const QWidget *owner,
                                       Qt::WindowModality modality = Qt::WindowModal) {
  if (!dialog)
    return;

  const QWidget *anchor = owner ? owner->window() : nullptr;
  if (!anchor)
    anchor = QApplication::activeWindow();

  Qt::WindowFlags flags = dialog->windowFlags();
  flags |= Qt::Window;
  flags |= Qt::Dialog;
  flags &= ~Qt::Sheet;
  dialog->setParent(nullptr, flags);

  auto placeDialog = [dialog, owner]() {
    if (!dialog)
      return;
    const QWidget *anchor = owner ? owner->window() : nullptr;
    if (!anchor)
      anchor = QApplication::activeWindow();
    if (!anchor)
      return;

    QRect parentFrame = anchor->frameGeometry();
    int x = parentFrame.x() + (parentFrame.width() - dialog->width()) / 2;
    int y = parentFrame.y();
    if (y < 0)
      y = 0;
    dialog->move(x, y);
  };

  Qt::WindowModality effectiveModality = modality;
  if (effectiveModality == Qt::WindowModal)
    effectiveModality = Qt::ApplicationModal;
  dialog->setWindowModality(effectiveModality);
  dialog->adjustSize();
  placeDialog();
  QTimer::singleShot(0, dialog, [placeDialog]() { placeDialog(); });
}

class PositionedMessageBox {
public:
  using Icon = ::QMessageBox::Icon;
  using StandardButton = ::QMessageBox::StandardButton;
  using StandardButtons = ::QMessageBox::StandardButtons;

  static constexpr StandardButton NoButton = ::QMessageBox::NoButton;
  static constexpr StandardButton Ok = ::QMessageBox::Ok;
  static constexpr StandardButton Save = ::QMessageBox::Save;
  static constexpr StandardButton Discard = ::QMessageBox::Discard;
  static constexpr StandardButton Cancel = ::QMessageBox::Cancel;

  static StandardButton warning(QWidget *parent, const QString &title,
                                const QString &text,
                                StandardButtons buttons = ::QMessageBox::Ok,
                                StandardButton defaultButton = ::QMessageBox::NoButton) {
    return show(parent, ::QMessageBox::Warning, title, text, buttons,
                defaultButton);
  }

  static StandardButton information(
      QWidget *parent, const QString &title, const QString &text,
      StandardButtons buttons = ::QMessageBox::Ok,
      StandardButton defaultButton = ::QMessageBox::NoButton) {
    return show(parent, ::QMessageBox::Information, title, text, buttons,
                defaultButton);
  }

  static void about(QWidget *parent, const QString &title, const QString &text) {
    show(parent, ::QMessageBox::Information, title, text, ::QMessageBox::Ok,
         ::QMessageBox::Ok);
  }

private:
  static StandardButton show(QWidget *parent, Icon icon, const QString &title,
                             const QString &text, StandardButtons buttons,
                             StandardButton defaultButton) {
    ::QMessageBox box(icon, title, text, buttons, nullptr);
    if (defaultButton != ::QMessageBox::NoButton)
      box.setDefaultButton(defaultButton);
    configureEditorPopupDialog(&box, parent);
    return static_cast<StandardButton>(box.exec());
  }
};

class PositionedInputDialog {
public:
  static int getInt(QWidget *parent, const QString &title, const QString &label,
                    int value = 0, int min = -2147483647,
                    int max = 2147483647, int step = 1, bool *ok = nullptr,
                    Qt::WindowFlags flags = Qt::WindowFlags()) {
    ::QInputDialog dialog(nullptr, flags);
    dialog.setInputMode(::QInputDialog::IntInput);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setIntRange(min, max);
    dialog.setIntStep(step);
    dialog.setIntValue(value);
    configureEditorPopupDialog(&dialog, parent);
    const bool accepted = (dialog.exec() == QDialog::Accepted);
    if (ok)
      *ok = accepted;
    return dialog.intValue();
  }

  static QString getItem(
      QWidget *parent, const QString &title, const QString &label,
      const QStringList &items, int current = 0, bool editable = true,
      bool *ok = nullptr, Qt::WindowFlags flags = Qt::WindowFlags(),
      Qt::InputMethodHints inputMethodHints = Qt::ImhNone) {
    ::QInputDialog dialog(nullptr, flags);
    dialog.setInputMode(::QInputDialog::TextInput);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setComboBoxItems(items);
    dialog.setComboBoxEditable(editable);
    dialog.setInputMethodHints(inputMethodHints);
    if (!items.isEmpty()) {
      int index = current;
      if (index < 0)
        index = 0;
      if (index >= items.size())
        index = items.size() - 1;
      dialog.setTextValue(items.at(index));
    }
    configureEditorPopupDialog(&dialog, parent);
    const bool accepted = (dialog.exec() == QDialog::Accepted);
    if (ok)
      *ok = accepted;
    return dialog.textValue();
  }

  static QString getText(
      QWidget *parent, const QString &title, const QString &label,
      QLineEdit::EchoMode echo = QLineEdit::Normal,
      const QString &text = QString(), bool *ok = nullptr,
      Qt::WindowFlags flags = Qt::WindowFlags(),
      Qt::InputMethodHints inputMethodHints = Qt::ImhNone) {
    ::QInputDialog dialog(nullptr, flags);
    dialog.setInputMode(::QInputDialog::TextInput);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setTextEchoMode(echo);
    if (QLineEdit *line = dialog.findChild<QLineEdit *>())
      line->setMaxLength(std::numeric_limits<int>::max());
    dialog.setTextValue(text);
    dialog.setInputMethodHints(inputMethodHints);
    configureEditorPopupDialog(&dialog, parent);
    const bool accepted = (dialog.exec() == QDialog::Accepted);
    if (ok)
      *ok = accepted;
    return dialog.textValue();
  }
};

#define QMessageBox PositionedMessageBox
#define QInputDialog PositionedInputDialog
static int dimProduct(const QVector<int> &dims);

class SingleClickEditTableView : public QTableView {
public:
  explicit SingleClickEditTableView(QWidget *parent = nullptr) : QTableView(parent) {}

  /** Commit delegate editors even when another window owns keyboard focus. */
  void finishEditing() {
    const auto editors = viewport()->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
    for (QLineEdit *editor : editors) {
      if (!editor->isHidden()) {
        commitData(editor);
        closeEditor(editor, QAbstractItemDelegate::NoHint);
      }
    }
  }

private:
  QPoint pressPos;
  QPersistentModelIndex pressIndex;
  bool leftButtonDown{false};
  bool dragSelecting{false};

private:
  void forwardClickToEditorAt(const QPoint &viewPos, const QPoint &globalPos, int retries = 3) {
    QWidget *target = viewport()->childAt(viewPos);
    if (!target || target == viewport()) {
      if (retries > 0) {
        QTimer::singleShot(0, this, [this, viewPos, globalPos, retries]() {
          forwardClickToEditorAt(viewPos, globalPos, retries - 1);
        });
      }
      return;
    }

    // Prefer setting the caret directly when the editor is a QLineEdit.
    if (QLineEdit *le = qobject_cast<QLineEdit *>(target)) {
      le->setFocus();
      const QPoint localPos = le->mapFromGlobal(globalPos);
      le->setCursorPosition(le->cursorPositionAt(localPos));
      le->deselect();
      return;
    }

    // Otherwise, forward as a normal click so it places the caret.
    const QPoint localPos = target->mapFromGlobal(globalPos);
    QMouseEvent press(QEvent::MouseButtonPress, localPos, globalPos, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, localPos, globalPos, Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &release);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::RightButton) {
      const QModelIndex idx = indexAt(event->pos());
      QItemSelectionModel *selection = selectionModel();
      if (idx.isValid() && selection && selection->isSelected(idx)) {
        selection->setCurrentIndex(idx, QItemSelectionModel::NoUpdate);
        event->accept();
        return;
      }
    }

    if (event->button() == Qt::LeftButton) {
      leftButtonDown = true;
      dragSelecting = false;
      pressPos = event->pos();
      pressIndex = indexAt(event->pos());
    }
    QTableView::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (leftButtonDown && !dragSelecting) {
      const int dist = (event->pos() - pressPos).manhattanLength();
      if (dist >= QApplication::startDragDistance())
        dragSelecting = true;
    }
    QTableView::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    QTableView::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton)
      return;
    leftButtonDown = false;
    if (dragSelecting)
      return;
    if (event->modifiers() != Qt::NoModifier)
      return;
    if (!pressIndex.isValid())
      return;
    if (!(model()->flags(pressIndex) & Qt::ItemIsEditable))
      return;
    edit(pressIndex);
  }

  void mouseDoubleClickEvent(QMouseEvent *event) override {
    // The view normally consumes the second click as a double-click. For single-click-to-edit,
    // we want the second click to land in the editor widget to place the caret.
    if (event->button() == Qt::LeftButton) {
      QModelIndex idx = indexAt(event->pos());
      if (idx.isValid() && (model()->flags(idx) & Qt::ItemIsEditable)) {
        edit(idx);
        const QPoint viewPos = event->pos();
      #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint globalPos = event->globalPosition().toPoint();
      #else
        const QPoint globalPos = event->globalPos();
      #endif
        QTimer::singleShot(0, this, [this, viewPos, globalPos]() {
          forwardClickToEditorAt(viewPos, globalPos);
        });
        event->accept();
        return;
      }
    }
    QTableView::mouseDoubleClickEvent(event);
  }
};

static QString truncateForMessage(const QString &text, int maxLen = 80) {
  QString t = text;
  t.replace('\n', "\\n");
  t.replace('\r', "\\r");
  t.replace('\t', "\\t");
  if (t.size() <= maxLen)
    return t;
  return t.left(maxLen) + QObject::tr("…");
}

/*
 * SDDS text is stored in the local 8-bit encoding, which on Windows is the
 * ANSI code page.  Characters outside it would be written as '?', so text is
 * accepted only if it survives the conversion unchanged.
 */
static bool localEncodingPreserves(const QString &text) {
  // SDDS stores text in NUL-terminated C strings.
  if (text.contains(QChar(0)))
    return false;
  for (QChar ch : text)
    if (ch.unicode() >= 0x80)
      return QString::fromLocal8Bit(text.toLocal8Bit()) == text;
  return true;
}

/*
 * True when stored text changes on a decode/encode round trip, such as
 * Latin-1 bytes in a UTF-8 locale.  Such text is shown with replacement
 * characters, which a save would then store instead of the original bytes.
 */
static bool localTextLosesBytes(const char *text) {
  if (!text)
    return false;
  for (const char *p = text; *p; ++p) {
    if (static_cast<unsigned char>(*p) >= 0x80) {
      const QByteArray bytes(text);
      return QString::fromLocal8Bit(bytes).toLocal8Bit() != bytes;
    }
  }
  return false;
}

static QString unencodableTextMessage() {
  return QObject::tr("Text contains a NUL character or characters that cannot be saved in this system's character encoding");
}

/** Warn about the first definition attribute that cannot be stored. */
static bool definitionTextEncodable(QWidget *parent, const QStringList &fields) {
  for (const QString &field : fields) {
    if (!localEncodingPreserves(field)) {
      QMessageBox::warning(parent, QObject::tr("SDDS"),
                           QObject::tr("%1: %2").arg(unencodableTextMessage(), truncateForMessage(field)));
      return false;
    }
  }
  return true;
}

/*
 * Shortest text that converts back to exactly the same value.  This is as
 * lossless as printing max_digits10 digits, but shows 0.1 rather than
 * 0.10000000000000001.
 */
static QString shortestDoubleText(double value) {
  if (value == 0 && std::signbit(value))
    return QStringLiteral("-0");
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
  return QString::number(value, 'g', QLocale::FloatingPointShortest);
#else
  return QString::number(value, 'g', std::numeric_limits<double>::max_digits10);
#endif
}

/* Qt has no shortest float formatter; try increasing precision until the text round-trips. */
static QString shortestFloatText(float value) {
  if (value == 0 && std::signbit(value))
    return QStringLiteral("-0");
  if (std::isfinite(value)) {
    for (int precision = std::numeric_limits<float>::digits10;
         precision < std::numeric_limits<float>::max_digits10; ++precision) {
      const QString text = QString::number(static_cast<double>(value), 'g', precision);
      if (text.toFloat() == value)
        return text;
    }
  }
  return QString::number(static_cast<double>(value), 'g', std::numeric_limits<float>::max_digits10);
}

/*
 * Enough digits to convert back to the same long double.  QString::asprintf
 * would convert the value to double, losing the extra precision of 80-bit or
 * 128-bit long doubles, so format with the C library.  The C library spells
 * NaN as "-nan" or "-nan(ind)", which the numeric validators reject, so use
 * the spellings that Qt and strtold both accept for nonfinite values.
 */
static QString longDoubleToText(long double value) {
  if (std::isnan(value))
    return QStringLiteral("nan");
  if (std::isinf(value))
    return value < 0 ? QStringLiteral("-inf") : QStringLiteral("inf");
  char buffer[128];
  const int written = snprintf(buffer, sizeof(buffer), "%.*Lg",
                               std::numeric_limits<long double>::max_digits10, value);
  if (written <= 0)
    return QString();
  return QString::fromLatin1(
      buffer, std::min<int>(written, static_cast<int>(sizeof(buffer)) - 1));
}

/*
 * Convert directly from the typed buffers owned by SDDS.  The SDDS
 * Get*InString helpers allocate one C string per value and the editor then
 * immediately copies every one into a QString.  Avoiding that intermediate
 * allocation is particularly important for multi-million-cell files.
 */
static QString sddsValueToString(const void *data, int64_t index, int32_t type) {
  if (!data || index < 0)
    return QString();

  switch (type) {
  case SDDS_SHORT:
    return QString::number(static_cast<qlonglong>(static_cast<const short *>(data)[index]));
  case SDDS_USHORT:
    return QString::number(static_cast<qulonglong>(static_cast<const unsigned short *>(data)[index]));
  case SDDS_LONG:
    return QString::number(static_cast<qlonglong>(static_cast<const int32_t *>(data)[index]));
  case SDDS_ULONG:
    return QString::number(static_cast<qulonglong>(static_cast<const uint32_t *>(data)[index]));
  case SDDS_LONG64:
    return QString::number(static_cast<qlonglong>(static_cast<const int64_t *>(data)[index]));
  case SDDS_ULONG64:
    return QString::number(static_cast<qulonglong>(static_cast<const uint64_t *>(data)[index]));
  case SDDS_FLOAT:
    return shortestFloatText(static_cast<const float *>(data)[index]);
  case SDDS_DOUBLE:
    return shortestDoubleText(static_cast<const double *>(data)[index]);
  case SDDS_LONGDOUBLE:
    return longDoubleToText(static_cast<const long double *>(data)[index]);
  case SDDS_CHARACTER: {
    const char value = static_cast<const char *>(data)[index];
    return value ? QString::fromLatin1(&value, 1) : QString();
  }
  case SDDS_STRING: {
    const char *value = static_cast<char *const *>(data)[index];
    return value ? QString::fromLocal8Bit(value) : QString();
  }
  default:
    return QString();
  }
}

/*
 * strtold may report ERANGE for a subnormal result (glibc does), although the
 * value is finite and exactly what SDDS stored.  Reject only overflow and
 * underflow to zero.
 */
static bool longDoubleOutOfRange(long double value) {
  return errno == ERANGE && (std::isinf(value) || value == 0.0L);
}

static bool parseLongDoubleStrict(const QString &text, long double *out) {
  if (!out || text.contains(QChar(0)))
    return false;
  QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    *out = 0.0L;
    return true;
  }
  QByteArray ba = trimmed.toLocal8Bit();
  const char *start = ba.constData();
  char *end = nullptr;
  errno = 0;
  long double v = strtold(start, &end);
  if (end == start)
    return false;
  while (end && *end && std::isspace(static_cast<unsigned char>(*end)))
    ++end;
  if (end && *end)
    return false;
  if (longDoubleOutOfRange(v))
    return false;
  *out = v;
  return true;
}

/** Encode literal fixed text for SDDS_ScanData, keeping character fields as single bytes. */
static QString fixedValueForDefinition(const QString &text, int32_t type) {
  if (SDDS_NUMERIC_TYPE(type))
    return text.trimmed().isEmpty() ? QStringLiteral("0") : text;
  const QByteArray bytes = type == SDDS_CHARACTER
                              ? (text.isEmpty() ? QByteArray(1, '\0') : text.toLatin1())
                              : text.toLocal8Bit();
  QString result;
  for (unsigned char byte : bytes) {
    // Escape token delimiters, SDDS comments/escapes, controls and non-ASCII bytes.
    if (byte <= 32 || byte > 126 || byte == '\\' || byte == '"' || byte == '!' || byte == '&')
      result += '\\' + QString::number(byte, 8).rightJustified(3, '0');
    else
      result += QLatin1Char(byte);
  }
  return result;
}

/** Decode fixed text exactly as the SDDS reader does before displaying or validating it. */
static QString fixedValueForDisplay(const PARAMETER_DEFINITION &definition) {
  if (!definition.fixed_value)
    return QString();
  if (definition.type != SDDS_STRING && definition.type != SDDS_CHARACTER)
    return QString::fromLocal8Bit(definition.fixed_value);
  QByteArray encoded(definition.fixed_value);
  char character = '\0';
  char *string = nullptr;
  void *value = definition.type == SDDS_CHARACTER ? static_cast<void *>(&character)
                                                : static_cast<void *>(&string);
  const bool ok = SDDS_ScanData(encoded.data(), definition.type, 0, value, 0, 1);
  const QString result = !ok ? QString::fromLocal8Bit(definition.fixed_value)
      : definition.type == SDDS_CHARACTER ? QString(QChar(static_cast<unsigned char>(character)))
                                           : sddsValueToString(value, 0, definition.type);
  free(string);
  return result;
}

/** Treat absent and empty optional metadata identically when checking for a real edit. */
static bool definitionTextMatches(const char *stored, const QString &text) {
  return QString::fromLocal8Bit(stored ? stored : "") == text;
}

static bool validatePageForWrite(const SDDS_LAYOUT &layout, const PageStore &pd,
                                 int pageIndex, QString *errorText) {
  const int pcount = layout.n_parameters;
  const int ccount = layout.n_columns;
  const int acount = layout.n_arrays;

  if (pcount > 0 && pd.parameters.size() < pcount) {
    if (errorText)
      *errorText = QObject::tr("Page %1: internal error: parameter data missing (have %2, need %3)")
                       .arg(pageIndex + 1)
                       .arg(pd.parameters.size())
                       .arg(pcount);
    return false;
  }

  // Parameters
  for (int i = 0; i < pcount; ++i) {
    const PARAMETER_DEFINITION &pdef = layout.parameter_definition[i];
    const QString val = pdef.fixed_value ? fixedValueForDisplay(pdef)
                                         : pd.parameters[i];
    const int32_t type = pdef.type;
    if (!validateTextForType(val, type, false)) {
      if (errorText) {
        *errorText = QObject::tr("Page %1: parameter '%2' has invalid value '%3' for type %4")
                         .arg(pageIndex + 1)
                         .arg(QString::fromLocal8Bit(pdef.name))
                         .arg(truncateForMessage(val))
                         .arg(QString::fromLocal8Bit(SDDS_GetTypeName(type)));
      }
      return false;
    }
  }

  // Columns: require consistent row count across columns.
  if (ccount > 0) {
    if (pd.columns.size() < ccount) {
      if (errorText)
        *errorText = QObject::tr("Page %1: internal error: column data missing (have %2, need %3)")
                         .arg(pageIndex + 1)
                         .arg(pd.columns.size())
                         .arg(ccount);
      return false;
    }
    const int64_t rows = pd.columns[0].size();
    for (int c = 0; c < ccount; ++c) {
      if (pd.columns[c].size() != rows) {
        if (errorText) {
          const char *name = layout.column_definition[c].name;
          *errorText = QObject::tr("Page %1: column '%2' has %3 rows; expected %4")
                           .arg(pageIndex + 1)
                           .arg(QString::fromLocal8Bit(name))
                           .arg(pd.columns[c].size())
                           .arg(rows);
        }
        return false;
      }
      const int32_t type = layout.column_definition[c].type;
      for (int64_t r = 0; r < rows; ++r) {
        const QString &cell = pd.columns[c][r];
        if (!validateTextForType(cell, type, false)) {
          if (errorText) {
            const char *name = layout.column_definition[c].name;
            *errorText = QObject::tr("Page %1: column '%2', row %3 has invalid value '%4' for type %5")
                             .arg(pageIndex + 1)
                             .arg(QString::fromLocal8Bit(name))
                             .arg(r + 1)
                             .arg(truncateForMessage(cell))
                             .arg(QString::fromLocal8Bit(SDDS_GetTypeName(type)));
          }
          return false;
        }
      }
    }
  }

  // Arrays: dims must be valid and consistent with stored element count.
  if (acount > 0) {
    if (pd.arrays.size() < acount) {
      if (errorText)
        *errorText = QObject::tr("Page %1: internal error: array data missing (have %2, need %3)")
                         .arg(pageIndex + 1)
                         .arg(pd.arrays.size())
                         .arg(acount);
      return false;
    }
    for (int a = 0; a < acount; ++a) {
      const ARRAY_DEFINITION &adef = layout.array_definition[a];
      const ArrayStore &as = pd.arrays[a];
      if (as.dims.size() != adef.dimensions) {
        if (errorText) {
          *errorText = QObject::tr("Page %1: array '%2' has %3 dimensions; expected %4")
                           .arg(pageIndex + 1)
                           .arg(QString::fromLocal8Bit(adef.name))
                           .arg(as.dims.size())
                           .arg(adef.dimensions);
        }
        return false;
      }
      const int expected = dimProduct(as.dims);
      if (expected < 0 || expected != as.values.size()) {
        if (errorText) {
          *errorText = QObject::tr("Page %1: array '%2' has %3 elements but dimensions imply %4")
                           .arg(pageIndex + 1)
                           .arg(QString::fromLocal8Bit(adef.name))
                           .arg(as.values.size())
                           .arg(expected);
        }
        return false;
      }

      const int32_t type = adef.type;
      for (int i = 0; i < as.values.size(); ++i) {
        const QString &cell = as.values[i];
        if (!validateTextForType(cell, type, false)) {
          if (errorText) {
            *errorText = QObject::tr("Page %1: array '%2', element %3 has invalid value '%4' for type %5")
                             .arg(pageIndex + 1)
                             .arg(QString::fromLocal8Bit(adef.name))
                             .arg(i + 1)
                             .arg(truncateForMessage(cell))
                             .arg(QString::fromLocal8Bit(SDDS_GetTypeName(type)));
          }
          return false;
        }
      }
    }
  }

  return true;
}

static bool validateTextForType(const QString &text, int type,
                                bool showMessage = true) {
  if (type != SDDS_CHARACTER && text.contains(QChar(0))) {
    if (showMessage)
      QMessageBox::warning(nullptr, QObject::tr("SDDS"),
                           QObject::tr("Field must not contain a NUL character"));
    return false;
  }
  if (SDDS_NUMERIC_TYPE(type)) {
    QString trimmed = text.trimmed();
    if (!trimmed.isEmpty()) {
      bool ok = true;
      if (type == SDDS_LONGDOUBLE) {
        long double tmp;
        ok = parseLongDoubleStrict(trimmed, &tmp);
      } else if (type == SDDS_DOUBLE) {
        trimmed.toDouble(&ok);
      } else if (type == SDDS_FLOAT) {
        trimmed.toFloat(&ok);
      } else if (type == SDDS_USHORT) {
        qulonglong v = trimmed.toULongLong(&ok);
        if (!ok || v > std::numeric_limits<unsigned short>::max())
          ok = false;
      } else if (type == SDDS_ULONG) {
        qulonglong v = trimmed.toULongLong(&ok);
        if (!ok || v > std::numeric_limits<uint32_t>::max())
          ok = false;
      } else if (type == SDDS_ULONG64) {
        trimmed.toULongLong(&ok);
      } else if (type == SDDS_SHORT) {
        qint64 v = trimmed.toLongLong(&ok);
        if (!ok || v < std::numeric_limits<short>::min() || v > std::numeric_limits<short>::max())
          ok = false;
      } else if (type == SDDS_LONG) {
        qint64 v = trimmed.toLongLong(&ok);
        if (!ok || v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max())
          ok = false;
      } else if (type == SDDS_LONG64) {
        trimmed.toLongLong(&ok);
      } else {
        if (SDDS_FLOATING_TYPE(type))
          trimmed.toDouble(&ok);
        else
          trimmed.toLongLong(&ok);
      }
      if (!ok) {
        if (showMessage)
          QMessageBox::warning(nullptr, QObject::tr("SDDS"),
                               QObject::tr("Invalid numeric value"));
        return false;
      }
    }
  } else if (type == SDDS_CHARACTER) {
    if (!text.isEmpty() && (text.size() != 1 || text[0].unicode() > 255)) {
      if (showMessage)
        QMessageBox::warning(nullptr, QObject::tr("SDDS"),
                             QObject::tr("Character field must contain one Latin-1 character (one byte)"));
      return false;
    }
  } else if (type == SDDS_STRING) {
    if (!localEncodingPreserves(text)) {
      if (showMessage)
        QMessageBox::warning(nullptr, QObject::tr("SDDS"), unencodableTextMessage());
      return false;
    }
  }
  return true;
}

static int dimProduct(const QVector<int> &dims) {
  if (dims.isEmpty())
    return 0;
  // A zero in any dimension makes the array empty.
  for (int d : dims)
    if (d < 0)
      return -1;
  if (dims.contains(0))
    return 0;
  int64_t prod = 1;
  for (int d : dims) {
    if (prod > std::numeric_limits<int>::max() / d)
      return -1;
    prod *= d;
  }
  return (int)prod;
}

/**
 * Copy array values into a new shape so every surviving element keeps its
 * coordinates (SDDS stores the last dimension fastest).  New cells are empty.
 * Dimensions added or removed at the end have index 0, matching the length-1
 * dimensions the attribute editor appends.
 */
static QVector<QString> reshapeArrayValues(const QVector<QString> &values,
                                           const QVector<int> &oldDims,
                                           const QVector<int> &newDims,
                                           int newSize) {
  QVector<QString> result(std::max(0, newSize));
  if (oldDims.isEmpty() || newDims.isEmpty() || dimProduct(oldDims) != values.size()) {
    for (int i = 0; i < result.size() && i < values.size(); ++i)
      result[i] = values[i];
    return result;
  }
  const int count = oldDims.size();
  const int axes = std::max(count, static_cast<int>(newDims.size()));
  QVector<int> coord(count, 0);
  for (int flat = 0; flat < values.size(); ++flat) {
    int rest = flat;
    for (int d = count - 1; d >= 0; --d) {
      coord[d] = rest % oldDims[d];
      rest /= oldDims[d];
    }
    int64_t target = 0;
    bool inside = true;
    for (int d = 0; d < axes && inside; ++d) {
      const int index = d < count ? coord[d] : 0;
      if (d >= newDims.size()) {
        inside = index == 0;
        continue;
      }
      inside = index < newDims[d];
      target = target * newDims[d] + index;
    }
    if (inside && target < result.size())
      result[static_cast<int>(target)] = values[flat];
  }
  return result;
}

static bool validateDefinitionName(QWidget *parent, const QString &name,
                                   const char *dataClass,
                                   int selfIndex, int count,
                                   const std::function<const char *(int)> &nameAt) {
  if (!definitionTextEncodable(parent, {name}))
    return false;
  QByteArray ba = name.toLocal8Bit();
  if (!SDDS_IsValidName(ba.constData(), dataClass)) {
    QMessageBox::warning(parent, QObject::tr("SDDS"),
                         QObject::tr("Invalid %1 name: %2")
                             .arg(QString::fromLocal8Bit(dataClass))
                             .arg(name));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }

  for (int i = 0; i < count; ++i) {
    if (i == selfIndex)
      continue;
    const char *existing = nameAt(i);
    if (existing && strcmp(existing, ba.constData()) == 0) {
      QMessageBox::warning(parent, QObject::tr("SDDS"),
                           QObject::tr("Duplicate %1 name: %2")
                               .arg(QString::fromLocal8Bit(dataClass))
                               .arg(name));
      return false;
    }
  }
  return true;
}

/*
 * A negative field length (fixed width with the padding removed on reading)
 * is valid only for strings; SDDS refuses to define or copy any other such
 * column or array, so the document could no longer be saved.
 */
static bool validateFieldLength(QWidget *parent, int32_t fieldLength, int32_t type) {
  if (fieldLength >= 0 || type == SDDS_STRING)
    return true;
  QMessageBox::warning(parent, QObject::tr("SDDS"),
                       QObject::tr("A negative field length is allowed only for string data"));
  return false;
}

/** Keep the width when a type change would leave a negative field length invalid. */
static int32_t fieldLengthForType(int32_t fieldLength, int32_t type) {
  return fieldLength < 0 && type != SDDS_STRING ? -fieldLength : fieldLength;
}

static bool resyncSortedIndexName(SORTED_INDEX **indexes, int count,
                                  int definitionIndex, char *name) {
  if (!indexes || definitionIndex < 0 || definitionIndex >= count)
    return false;

  for (int i = 0; i < count; ++i) {
    if (indexes[i] && indexes[i]->index == definitionIndex) {
      indexes[i]->name = name;
      qsort((char *)indexes, count, sizeof(*indexes), SDDS_CompareIndexedNamesPtr);
      return true;
    }
  }
  return false;
}

/*
 * The working and saved SDDS layouts normally share definition strings, and
 * SDDS_Terminate owns/frees them through original_layout.  Replacing a string
 * in only the working layout either leaks the replacement or, if the shared
 * string is freed first, leaves original_layout with a dangling pointer.
 * Install one replacement into both layouts and release each old allocation
 * exactly once.
 */
static bool replaceSharedLayoutString(char **workingField, char **savedField,
                                      const QString &value,
                                      bool nullWhenEmpty = true) {
  if (!workingField || !savedField)
    return false;

  char *replacement = nullptr;
  if (!nullWhenEmpty || !value.isEmpty()) {
    const QByteArray encoded = value.toLocal8Bit();
    replacement = strdup(encoded.constData());
    if (!replacement)
      return false;
  }

  char *oldWorking = *workingField;
  char *oldSaved = *savedField;
  *workingField = replacement;
  *savedField = replacement;
  if (oldWorking)
    free(oldWorking);
  if (oldSaved && oldSaved != oldWorking)
    free(oldSaved);
  return true;
}

static QVector<int> sortedValidIndexesDescending(const QSet<int> &values,
                                                 int limit) {
  QVector<int> result;
  result.reserve(values.size());
  for (int value : values) {
    if (value >= 0 && value < limit)
      result.append(value);
  }
  std::sort(result.begin(), result.end(), std::greater<int>());
  return result;
}

static void collectSelectedRows(QTableView *view, QSet<int> *rows) {
  if (!view || !rows)
    return;
  QItemSelectionModel *selection = view->selectionModel();
  if (!selection)
    return;

  const QModelIndexList selectedRows = selection->selectedRows();
  for (const QModelIndex &idx : selectedRows)
    if (idx.isValid())
      rows->insert(idx.row());

  const QModelIndexList selectedIndexes = selection->selectedIndexes();
  for (const QModelIndex &idx : selectedIndexes)
    if (idx.isValid())
      rows->insert(idx.row());
}

static void collectSelectedColumns(QTableView *view, QSet<int> *columns) {
  if (!view || !columns)
    return;
  QItemSelectionModel *selection = view->selectionModel();
  if (!selection)
    return;

  const QModelIndexList selectedColumns = selection->selectedColumns();
  for (const QModelIndex &idx : selectedColumns)
    if (idx.isValid())
      columns->insert(idx.column());

  const QModelIndexList selectedIndexes = selection->selectedIndexes();
  for (const QModelIndex &idx : selectedIndexes)
    if (idx.isValid())
      columns->insert(idx.column());
}

/*
 * Selected cells the user can see.  A selection dragged across rows hidden by
 * the row filter also covers those rows in the selection model, so edits and
 * deletions must not use selectionModel()->selectedIndexes() directly.
 */
static QModelIndexList visibleSelectedIndexes(const QTableView *view, bool currentWhenEmpty = false) {
  QModelIndexList visible;
  if (!view || !view->selectionModel())
    return visible;
  const QModelIndexList selected = view->selectionModel()->selectedIndexes();
  visible.reserve(selected.size());
  for (const QModelIndex &idx : selected)
    if (idx.isValid() && !view->isRowHidden(idx.row()) && !view->isColumnHidden(idx.column()))
      visible.append(idx);
  if (visible.isEmpty() && currentWhenEmpty) {
    const QModelIndex idx = view->currentIndex();
    if (idx.isValid() && !view->isRowHidden(idx.row()) && !view->isColumnHidden(idx.column()))
      visible.append(idx);
  }
  return visible;
}

/** Mutation tools must not count read-only cells, including shorter arrays' padding. */
static QModelIndexList editableSelectedIndexes(const QTableView *view, bool currentWhenEmpty = false) {
  QModelIndexList indexes = visibleSelectedIndexes(view, currentWhenEmpty);
  indexes.erase(std::remove_if(indexes.begin(), indexes.end(), [view](const QModelIndex &index) {
    return !(view->model()->flags(index) & Qt::ItemIsEditable);
  }), indexes.end());
  return indexes;
}

static QVector<int> selectedRowsOrFallback(QTableView *view, int maxRows,
                                           int fallbackRow) {
  QSet<int> selected;
  collectSelectedRows(view, &selected);
  if (fallbackRow >= 0 && fallbackRow < maxRows) {
    if (selected.contains(fallbackRow))
      return sortedValidIndexesDescending(selected, maxRows);
    return QVector<int>(1, fallbackRow);
  }
  if (!selected.isEmpty())
    return sortedValidIndexesDescending(selected, maxRows);

  const QModelIndex current = view ? view->currentIndex() : QModelIndex();
  if (current.isValid() && current.row() >= 0 && current.row() < maxRows)
    return QVector<int>(1, current.row());
  return QVector<int>();
}

static QVector<int> selectedColumnsOrFallback(QTableView *view, int maxColumns,
                                              int fallbackColumn) {
  QSet<int> selected;
  collectSelectedColumns(view, &selected);
  if (fallbackColumn >= 0 && fallbackColumn < maxColumns) {
    if (selected.contains(fallbackColumn))
      return sortedValidIndexesDescending(selected, maxColumns);
    return QVector<int>(1, fallbackColumn);
  }
  if (!selected.isEmpty())
    return sortedValidIndexesDescending(selected, maxColumns);

  const QModelIndex current = view ? view->currentIndex() : QModelIndex();
  if (current.isValid() && current.column() >= 0 && current.column() < maxColumns)
    return QVector<int>(1, current.column());
  return QVector<int>();
}

static hid_t hdfTypeForSdds(int32_t type) {
  switch (type) {
  case SDDS_SHORT:
    return H5T_NATIVE_SHORT;
  case SDDS_USHORT:
    return H5T_NATIVE_USHORT;
  case SDDS_LONG:
    return H5T_NATIVE_INT;
  case SDDS_ULONG:
    return H5T_NATIVE_UINT;
  case SDDS_LONG64:
    return H5T_NATIVE_LLONG;
  case SDDS_ULONG64:
    return H5T_NATIVE_ULLONG;
  case SDDS_FLOAT:
    return H5T_NATIVE_FLOAT;
  case SDDS_DOUBLE:
    return H5T_NATIVE_DOUBLE;
  case SDDS_LONGDOUBLE:
    return H5T_NATIVE_LDOUBLE;
  case SDDS_CHARACTER:
    return H5T_NATIVE_CHAR;
  default:
    return H5T_C_S1;
  }
}

static QString canonicalizeForDisplay(const QString &text, int type) {
  if (text.isEmpty())
    return text;
  if (type == SDDS_DOUBLE) {
    bool ok = true;
    double val = text.toDouble(&ok);
    if (ok)
      return shortestDoubleText(val);
  } else if (type == SDDS_LONGDOUBLE) {
    //long double val = std::stold(text.toStdString());
    //return QString::asprintf("%.*Lg", std::numeric_limits<long double>::max_digits10, val);
  } else if (type == SDDS_FLOAT) {
    bool ok = true;
    float val = text.toFloat(&ok);
    if (ok)
      return shortestFloatText(val);
  }
  return text;
}

class SetDataCommand : public QUndoCommand {
public:
  SetDataCommand(QAbstractItemModel *model, const QModelIndex &index,
                 const QString &oldVal, const QString &newVal)
      : m(model), row(index.row()), column(index.column()), oldValue(oldVal), newValue(newVal) {
    setText(QObject::tr("Edit Cell"));
  }

  void undo() override { apply(oldValue); }
  void redo() override { apply(newValue); }

private:
  // Structural undo restores the page and coordinates before older edits run.
  // Persistent indexes cannot survive the model resets used for restoration.
  void apply(const QString &value) {
    if (m)
      m->setData(m->index(row, column), value);
  }
  QPointer<QAbstractItemModel> m;
  int row;
  int column;
  QString oldValue;
  QString newValue;
};

static bool applyCellEditWithUndo(QUndoStack *undoStack, QAbstractItemModel *model,
                                  const QModelIndex &index, const QString &newValue) {
  if (!model || !index.isValid())
    return false;
  // Read-only metadata cells, such as parameter types, never become undo entries.
  if (!(model->flags(index) & Qt::ItemIsEditable))
    return false;
  const QString oldValue = index.data(Qt::EditRole).toString();
  if (oldValue == newValue)
    return false;
  if (undoStack)
    undoStack->push(new SetDataCommand(model, index, oldValue, newValue));
  else
    model->setData(index, newValue);
  return true;
}

/*
 * Apply several {idx, value} edits as one undo step.  The macro is opened only
 * for a real change: an empty one would add a no-op Undo entry and discard Redo.
 */
template <typename Edits>
static bool applyCellEditsWithUndo(QUndoStack *undoStack, QAbstractItemModel *model,
                                   const Edits &edits, const QString &label) {
  bool changed = false;
  bool macroStarted = false;
  for (const auto &edit : edits) {
    if (!(model->flags(edit.idx) & Qt::ItemIsEditable) ||
        edit.idx.data(Qt::EditRole).toString() == edit.value)
      continue;
    if (undoStack && !macroStarted) {
      undoStack->beginMacro(label);
      macroStarted = true;
    }
    changed = applyCellEditWithUndo(undoStack, model, edit.idx, edit.value) || changed;
  }
  if (macroStarted)
    undoStack->endMacro();
  return changed;
}

struct ExpressionContext {
  long double x;
  long double a;
  int i;
  int row;
  int col;
  int dr;
  int dc;
};

static bool integerLiteralFitsLongDouble(const QString &text);

// Bound recursive syntax independently of text length and platform stack size.
static const int MAX_EXPRESSION_NESTING = 256;

class ExpressionParser {
public:
  ExpressionParser(const QString &expression, const ExpressionContext &context,
                   bool exactIntegerLiterals = false)
      : text(expression), ctx(context), pos(0), ok(true), depth(0), exactIntegerLiterals(exactIntegerLiterals) {}

  bool parse(long double *out) {
    if (!out)
      return false;
    skipWs();
    long double value = parseExpression();
    skipWs();
    if (!ok || pos != text.size())
      return false;
    *out = value;
    return true;
  }

private:
  long double parseExpression() {
    long double lhs = parseTerm();
    while (ok) {
      skipWs();
      if (match('+'))
        lhs = checkedResult(lhs + parseTerm());
      else if (match('-'))
        lhs = checkedResult(lhs - parseTerm());
      else
        break;
    }
    return lhs;
  }

  long double parseTerm() {
    long double lhs = parseUnary();
    while (ok) {
      skipWs();
      if (match('*'))
        lhs = checkedResult(lhs * parseUnary());
      else if (match('/')) {
        long double rhs = parseUnary();
        if (rhs == 0.0L) {
          ok = false;
          return 0.0L;
        }
        lhs = checkedResult(lhs / rhs);
      } else
        break;
    }
    return lhs;
  }

  /* '^' binds tighter than a leading sign (-2^2 == -4) and is right
     associative; its exponent may carry a sign (2^-1 == 0.5). */
  long double parsePower() {
    long double base = parsePrimary();
    skipWs();
    if (match('^')) {
      long double expv = parseUnary();
      base = checkedResult(powl(base, expv));
    }
    return base;
  }

  long double parseUnary() {
    if (depth >= MAX_EXPRESSION_NESTING) {
      ok = false;
      return 0.0L;
    }
    QScopedValueRollback<int> depthGuard(depth, depth + 1);
    skipWs();
    if (match('+'))
      return parseUnary();
    if (match('-'))
      return -parseUnary();
    return parsePower();
  }

  long double parsePrimary() {
    skipWs();
    if (match('(')) {
      long double value = parseExpression();
      skipWs();
      if (!match(')'))
        ok = false;
      return value;
    }

    if (pos < text.size() && (text[pos].isDigit() || text[pos] == '.'))
      return parseNumber();

    if (pos < text.size() && (text[pos].isLetter() || text[pos] == '_')) {
      const QString ident = parseIdentifier();
      skipWs();
      if (match('(')) {
        long double arg = parseExpression();
        skipWs();
        if (!match(')')) {
          ok = false;
          return 0.0L;
        }
        return checkedResult(applyFunction(ident, arg));
      }
      return variableValue(ident);
    }

    ok = false;
    return 0.0L;
  }

  /** Reject integer fallback before a large intermediate can round and then cancel. */
  long double checkedResult(long double value) {
    if (exactIntegerLiterals && std::isfinite(value) &&
        fabsl(value) >= ldexpl(1.0L, std::numeric_limits<long double>::digits - 1))
      ok = false;
    return value;
  }

  long double parseNumber() {
    QByteArray tail = text.mid(pos).toLocal8Bit();
    const char *start = tail.constData();
    char *end = nullptr;
    errno = 0;
    long double value = strtold(start, &end);
    if (end == start || longDoubleOutOfRange(value)) {
      ok = false;
      return 0.0L;
    }
    // Large literals can cancel to a small, apparently valid integer after rounding.
    if (exactIntegerLiterals && fabsl(value) >= ldexpl(1.0L, std::numeric_limits<long double>::digits - 1) &&
        !integerLiteralFitsLongDouble(QString::fromLocal8Bit(start, static_cast<int>(end - start)))) {
      ok = false;
      return 0.0L;
    }
    pos += static_cast<int>(end - start);
    return value;
  }

  QString parseIdentifier() {
    const int start = pos;
    while (pos < text.size() && (text[pos].isLetterOrNumber() || text[pos] == '_'))
      ++pos;
    return text.mid(start, pos - start).toLower();
  }

  long double variableValue(const QString &ident) {
    if (ident == "x")
      return ctx.x;
    if (ident == "a")
      return ctx.a;
    if (ident == "i")
      return static_cast<long double>(ctx.i);
    if (ident == "row")
      return static_cast<long double>(ctx.row);
    if (ident == "col")
      return static_cast<long double>(ctx.col);
    if (ident == "dr")
      return static_cast<long double>(ctx.dr);
    if (ident == "dc")
      return static_cast<long double>(ctx.dc);
    if (ident == "pi")
      return acosl(-1.0L);
    if (ident == "e")
      return expl(1.0L);
    ok = false;
    return 0.0L;
  }

  long double applyFunction(const QString &ident, long double arg) {
    if (ident == "abs")
      return fabsl(arg);
    if (ident == "sqrt")
      return sqrtl(arg);
    if (ident == "sin")
      return sinl(arg);
    if (ident == "cos")
      return cosl(arg);
    if (ident == "tan")
      return tanl(arg);
    if (ident == "log")
      return logl(arg);
    if (ident == "exp")
      return expl(arg);
    if (ident == "floor")
      return floorl(arg);
    if (ident == "ceil")
      return ceill(arg);
    ok = false;
    return 0.0L;
  }

  void skipWs() {
    while (pos < text.size() && text[pos].isSpace())
      ++pos;
  }

  bool match(QChar ch) {
    if (pos < text.size() && text[pos] == ch) {
      ++pos;
      return true;
    }
    return false;
  }

  QString text;
  ExpressionContext ctx;
  int pos;
  bool ok;
  int depth;
  bool exactIntegerLiterals;
};

static bool evaluateExpressionText(const QString &expression,
                                   const ExpressionContext &ctx,
                                   long double *out, bool exactIntegerLiterals = false) {
  ExpressionParser parser(expression, ctx, exactIntegerLiterals);
  return parser.parse(out);
}

/*
 * Where long double is no wider than double (MSVC, Apple silicon), it cannot
 * hold every 64-bit integer, so 9007199254740993 + 0 would become ...992.
 * Integer cells are therefore computed exactly as a sign and a 64-bit
 * magnitude, which covers both long64 and ulong64.
 */
struct ExactInteger {
  bool negative;
  quint64 magnitude;
};

static ExactInteger exactInteger(bool negative, quint64 magnitude) {
  return {negative && magnitude != 0, magnitude};
}

/** Accept optionally signed decimal digits; an empty cell is zero, as when saved. */
static bool parseExactInteger(const QString &text, ExactInteger *out) {
  QString digits = text.trimmed();
  if (digits.isEmpty()) {
    *out = exactInteger(false, 0);
    return true;
  }
  const bool negative = digits.startsWith('-');
  if (negative || digits.startsWith('+'))
    digits.remove(0, 1);
  if (digits.isEmpty())
    return false;
  for (QChar ch : digits)
    if (ch < QLatin1Char('0') || ch > QLatin1Char('9'))
      return false;
  bool ok = false;
  const quint64 magnitude = digits.toULongLong(&ok);
  if (!ok)
    return false;
  *out = exactInteger(negative, magnitude);
  return true;
}

static QString exactIntegerText(const ExactInteger &value) {
  const QString digits = QString::number(value.magnitude);
  return value.negative ? QStringLiteral("-") + digits : digits;
}

/** Test the significant bits instead of converting a rounded value back to an integer. */
static bool exactIntegerFitsLongDouble(const ExactInteger &value) {
  quint64 magnitude = value.magnitude;
  if (magnitude == 0)
    return true;
  while ((magnitude & 1) == 0)
    magnitude >>= 1;
  int bits = 0;
  while (magnitude) {
    ++bits;
    magnitude >>= 1;
  }
  return bits <= std::numeric_limits<long double>::digits;
}

/** Only plain integer literals participate in the exact expression grammar. */
static bool integerLiteralFitsLongDouble(const QString &text) {
  ExactInteger value = exactInteger(false, 0);
  return parseExactInteger(text, &value) && exactIntegerFitsLongDouble(value);
}

static bool exactAdd(const ExactInteger &a, const ExactInteger &b, ExactInteger *out) {
  if (a.negative == b.negative) {
    if (a.magnitude > std::numeric_limits<quint64>::max() - b.magnitude)
      return false;
    *out = exactInteger(a.negative, a.magnitude + b.magnitude);
  } else if (a.magnitude >= b.magnitude) {
    *out = exactInteger(a.negative, a.magnitude - b.magnitude);
  } else {
    *out = exactInteger(b.negative, b.magnitude - a.magnitude);
  }
  return true;
}

static bool exactMultiply(const ExactInteger &a, const ExactInteger &b, ExactInteger *out) {
  if (a.magnitude != 0 && b.magnitude > std::numeric_limits<quint64>::max() / a.magnitude)
    return false;
  *out = exactInteger(a.negative != b.negative, a.magnitude * b.magnitude);
  return true;
}

struct ExactIntegerContext {
  ExactInteger x;
  ExactInteger a;
  bool hasX;
  bool hasA;
  int i;
  int row;
  int col;
  int dr;
  int dc;
};

/*
 * The integer subset of ExpressionParser's grammar.  Anything outside it
 * (fractions, '^', functions other than abs/floor/ceil, pi, e, inexact division,
 * overflow or a syntax error) fails, and the caller falls back to
 * ExpressionParser, which computes the value or reports the error.
 */
class ExactIntegerExpressionParser {
public:
  ExactIntegerExpressionParser(const QString &expression, const ExactIntegerContext &context)
      : text(expression), ctx(context), pos(0), ok(true), depth(0) {}

  bool parse(ExactInteger *out) {
    skipWs();
    const ExactInteger value = parseExpression();
    skipWs();
    if (!ok || pos != text.size())
      return false;
    *out = value;
    return true;
  }

private:
  ExactInteger parseExpression() {
    ExactInteger lhs = parseTerm();
    while (ok) {
      skipWs();
      if (match('+'))
        ok = exactAdd(lhs, parseTerm(), &lhs) && ok;
      else if (match('-'))
        ok = exactAdd(lhs, negate(parseTerm()), &lhs) && ok;
      else
        break;
    }
    return lhs;
  }

  ExactInteger parseTerm() {
    ExactInteger lhs = parseUnary();
    while (ok) {
      skipWs();
      if (match('*')) {
        ok = exactMultiply(lhs, parseUnary(), &lhs) && ok;
      } else if (match('/')) {
        const ExactInteger rhs = parseUnary();
        if (rhs.magnitude == 0 || lhs.magnitude % rhs.magnitude != 0) {
          ok = false;
          break;
        }
        lhs = exactInteger(lhs.negative != rhs.negative, lhs.magnitude / rhs.magnitude);
      } else {
        break;
      }
    }
    return lhs;
  }

  ExactInteger parseUnary() {
    if (depth >= MAX_EXPRESSION_NESTING) {
      ok = false;
      return exactInteger(false, 0);
    }
    QScopedValueRollback<int> depthGuard(depth, depth + 1);
    skipWs();
    if (match('+'))
      return parseUnary();
    if (match('-'))
      return negate(parseUnary());
    const ExactInteger value = parsePrimary();
    skipWs();
    if (pos < text.size() && text[pos] == '^')
      ok = false;
    return value;
  }

  ExactInteger parsePrimary() {
    skipWs();
    if (match('(')) {
      const ExactInteger value = parseExpression();
      skipWs();
      if (!match(')'))
        ok = false;
      return value;
    }
    if (pos < text.size() && text[pos] >= QLatin1Char('0') && text[pos] <= QLatin1Char('9')) {
      const int start = pos;
      while (pos < text.size() && text[pos] >= QLatin1Char('0') && text[pos] <= QLatin1Char('9'))
        ++pos;
      // "1.5", "1e3" and "0x10" are not plain integers.
      ExactInteger value = exactInteger(false, 0);
      if ((pos < text.size() && (text[pos] == '.' || text[pos].isLetterOrNumber() || text[pos] == '_')) ||
          !parseExactInteger(text.mid(start, pos - start), &value))
        ok = false;
      return value;
    }
    if (pos < text.size() && (text[pos].isLetter() || text[pos] == '_')) {
      const int start = pos;
      while (pos < text.size() && (text[pos].isLetterOrNumber() || text[pos] == '_'))
        ++pos;
      const QString ident = text.mid(start, pos - start).toLower();
      skipWs();
      if (match('(')) {
        ExactInteger arg = parseExpression();
        skipWs();
        if (!match(')') || (ident != "abs" && ident != "floor" && ident != "ceil"))
          ok = false;
        if (ident == "abs")
          arg.negative = false;
        return arg;
      }
      return variableValue(ident);
    }
    ok = false;
    return exactInteger(false, 0);
  }

  ExactInteger variableValue(const QString &ident) {
    auto fromInt = [](int value) {
      return exactInteger(value < 0, value < 0 ? 0 - static_cast<quint64>(value) : static_cast<quint64>(value));
    };
    if (ident == "x" && ctx.hasX)
      return ctx.x;
    if (ident == "a" && ctx.hasA)
      return ctx.a;
    if (ident == "i")
      return fromInt(ctx.i);
    if (ident == "row")
      return fromInt(ctx.row);
    if (ident == "col")
      return fromInt(ctx.col);
    if (ident == "dr")
      return fromInt(ctx.dr);
    if (ident == "dc")
      return fromInt(ctx.dc);
    ok = false;
    return exactInteger(false, 0);
  }

  static ExactInteger negate(const ExactInteger &value) {
    return exactInteger(!value.negative, value.magnitude);
  }

  void skipWs() {
    while (pos < text.size() && text[pos].isSpace())
      ++pos;
  }

  bool match(QChar ch) {
    if (pos < text.size() && text[pos] == ch) {
      ++pos;
      return true;
    }
    return false;
  }

  QString text;
  ExactIntegerContext ctx;
  int pos;
  bool ok;
  int depth;
};

struct RowFilterValue {
  QString text;
  bool hasNumber;
  long double number;
};

/*
 * Empty text is not a number here, so "" never equals "0" in a string
 * comparison; the resolver supplies "0" for empty numeric cells instead.
 */
static bool parseNumericValueForFilter(const QString &text, long double *out) {
  return !text.trimmed().isEmpty() && parseLongDoubleStrict(text, out);
}

/** Compare integers exactly even on platforms with 64-bit long double. */
static bool compareIntegerText(const QString &left, const QString &right, int *result) {
  auto normalize = [](QString text, QString *digits, bool *negative) {
    text = text.trimmed();
    if (text.isEmpty())
      return false;
    *negative = text.startsWith('-');
    if (text.startsWith('-') || text.startsWith('+'))
      text.remove(0, 1);
    if (text.isEmpty())
      return false;
    for (QChar ch : text)
      if (ch < QLatin1Char('0') || ch > QLatin1Char('9'))
        return false;
    int first = 0;
    while (first + 1 < text.size() && text[first] == QLatin1Char('0'))
      ++first;
    *digits = text.mid(first);
    if (*digits == "0")
      *negative = false;
    return true;
  };
  QString a, b;
  bool an, bn;
  if (!normalize(left, &a, &an) || !normalize(right, &b, &bn))
    return false;
  int cmp = a.size() == b.size() ? QString::compare(a, b)
                                : (a.size() < b.size() ? -1 : 1);
  *result = an != bn ? (an ? -1 : 1) : (an ? -cmp : cmp);
  return true;
}

enum class RowFilterTokenKind {
  Invalid,
  End,
  Identifier,
  Number,
  String,
  LParen,
  RParen,
  And,
  Or,
  Not,
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge
};

struct RowFilterToken {
  RowFilterToken(RowFilterTokenKind kind = RowFilterTokenKind::Invalid,
                 const QString &text = QString(), bool bracketed = false)
      : kind(kind), text(text), bracketed(bracketed) {}

  RowFilterTokenKind kind;
  QString text;
  /* Written as [name]: always a column, never a row variable or true/false. */
  bool bracketed;
};

class RowFilterParser {
public:
  /* Resolve (identifier, column only) to its text for the current row. */
  using Resolver = std::function<bool(const QString &, bool, QString *)>;

  RowFilterParser(const QString &expression, Resolver resolver)
      : input(expression), pos(0), resolver(std::move(resolver)), depth(0) {
    next();
  }

  bool parse(bool *result, QString *errorText) {
    if (!result)
      return false;
    if (current.kind == RowFilterTokenKind::Invalid) {
      if (errorText)
        *errorText = current.text;
      return false;
    }
    bool value = false;
    if (!parseOr(&value, errorText))
      return false;
    if (current.kind == RowFilterTokenKind::Invalid) {
      if (errorText)
        *errorText = current.text;
      return false;
    }
    if (current.kind != RowFilterTokenKind::End) {
      if (errorText)
        *errorText = QObject::tr("Unexpected token '%1'").arg(current.text);
      return false;
    }
    *result = value;
    return true;
  }

private:
  bool parseOr(bool *out, QString *errorText) {
    bool lhs = false;
    if (!parseAnd(&lhs, errorText))
      return false;
    while (current.kind == RowFilterTokenKind::Or) {
      next();
      bool rhs = false;
      if (!parseAnd(&rhs, errorText))
        return false;
      lhs = lhs || rhs;
    }
    *out = lhs;
    return true;
  }

  bool parseAnd(bool *out, QString *errorText) {
    bool lhs = false;
    if (!parseUnary(&lhs, errorText))
      return false;
    while (current.kind == RowFilterTokenKind::And) {
      next();
      bool rhs = false;
      if (!parseUnary(&rhs, errorText))
        return false;
      lhs = lhs && rhs;
    }
    *out = lhs;
    return true;
  }

  bool parseUnary(bool *out, QString *errorText) {
    if (depth >= MAX_EXPRESSION_NESTING) {
      if (errorText)
        *errorText = QObject::tr("Filter expression is nested too deeply");
      return false;
    }
    QScopedValueRollback<int> depthGuard(depth, depth + 1);
    if (current.kind == RowFilterTokenKind::Not) {
      next();
      bool inner = false;
      if (!parseUnary(&inner, errorText))
        return false;
      *out = !inner;
      return true;
    }
    return parsePrimary(out, errorText);
  }

  bool parsePrimary(bool *out, QString *errorText) {
    if (current.kind == RowFilterTokenKind::LParen) {
      next();
      bool inner = false;
      if (!parseOr(&inner, errorText))
        return false;
      if (current.kind != RowFilterTokenKind::RParen) {
        if (errorText)
          *errorText = QObject::tr("Missing closing ')' in filter expression");
        return false;
      }
      next();
      *out = inner;
      return true;
    }
    return parseComparisonOrTruthiness(out, errorText);
  }

  bool parseComparisonOrTruthiness(bool *out, QString *errorText) {
    RowFilterValue left;
    if (!parseValue(&left, errorText))
      return false;

    const RowFilterTokenKind op = current.kind;
    if (op == RowFilterTokenKind::Eq || op == RowFilterTokenKind::Ne ||
        op == RowFilterTokenKind::Lt || op == RowFilterTokenKind::Le ||
        op == RowFilterTokenKind::Gt || op == RowFilterTokenKind::Ge) {
      next();
      RowFilterValue right;
      if (!parseValue(&right, errorText))
        return false;
      *out = compareValues(left, op, right);
      return true;
    }

    *out = toBool(left);
    return true;
  }

  bool parseValue(RowFilterValue *out, QString *errorText) {
    if (!out)
      return false;

    if (current.kind == RowFilterTokenKind::String) {
      out->text = current.text;
      out->hasNumber = parseNumericValueForFilter(out->text, &out->number);
      next();
      return true;
    }

    if (current.kind == RowFilterTokenKind::Number) {
      out->text = current.text;
      out->hasNumber = parseNumericValueForFilter(out->text, &out->number);
      if (!out->hasNumber) {
        if (errorText)
          *errorText = QObject::tr("Invalid numeric literal '%1'").arg(out->text);
        return false;
      }
      next();
      return true;
    }

    if (current.kind == RowFilterTokenKind::Identifier) {
      const QString ident = current.text;
      const QString lowered = current.bracketed ? QString() : ident.toLower();
      if (lowered == "true") {
        out->text = "1";
        out->hasNumber = true;
        out->number = 1.0L;
        next();
        return true;
      }
      if (lowered == "false") {
        out->text = "0";
        out->hasNumber = true;
        out->number = 0.0L;
        next();
        return true;
      }

      QString resolved;
      if (!resolver(ident, current.bracketed, &resolved)) {
        if (errorText)
          *errorText = QObject::tr("Unknown row variable/column '%1'").arg(ident);
        return false;
      }
      out->text = resolved;
      out->hasNumber = parseNumericValueForFilter(out->text, &out->number);
      next();
      return true;
    }

    if (errorText)
      *errorText = QObject::tr("Expected value in filter expression");
    return false;
  }

  static bool toBool(const RowFilterValue &value) {
    if (value.hasNumber)
      return value.number != 0.0L;
    const QString t = value.text.trimmed().toLower();
    return !(t.isEmpty() || t == "0" || t == "false" || t == "no" || t == "off");
  }

  static bool compareValues(const RowFilterValue &left,
                            RowFilterTokenKind op,
                            const RowFilterValue &right) {
    int integerComparison;
    if (compareIntegerText(left.text, right.text, &integerComparison)) {
      switch (op) {
      case RowFilterTokenKind::Eq: return integerComparison == 0;
      case RowFilterTokenKind::Ne: return integerComparison != 0;
      case RowFilterTokenKind::Lt: return integerComparison < 0;
      case RowFilterTokenKind::Le: return integerComparison <= 0;
      case RowFilterTokenKind::Gt: return integerComparison > 0;
      case RowFilterTokenKind::Ge: return integerComparison >= 0;
      default: return false;
      }
    }
    if (left.hasNumber && right.hasNumber) {
      switch (op) {
      case RowFilterTokenKind::Eq:
        return left.number == right.number;
      case RowFilterTokenKind::Ne:
        return left.number != right.number;
      case RowFilterTokenKind::Lt:
        return left.number < right.number;
      case RowFilterTokenKind::Le:
        return left.number <= right.number;
      case RowFilterTokenKind::Gt:
        return left.number > right.number;
      case RowFilterTokenKind::Ge:
        return left.number >= right.number;
      default:
        return false;
      }
    }

    const int cmp = QString::compare(left.text, right.text, Qt::CaseSensitive);
    switch (op) {
    case RowFilterTokenKind::Eq:
      return cmp == 0;
    case RowFilterTokenKind::Ne:
      return cmp != 0;
    case RowFilterTokenKind::Lt:
      return cmp < 0;
    case RowFilterTokenKind::Le:
      return cmp <= 0;
    case RowFilterTokenKind::Gt:
      return cmp > 0;
    case RowFilterTokenKind::Ge:
      return cmp >= 0;
    default:
      return false;
    }
  }

  void skipWs() {
    while (pos < input.size() && input[pos].isSpace())
      ++pos;
  }

  static bool isIdentStart(QChar ch) {
    return ch.isLetter() || ch == '_';
  }

  static bool isIdentPart(QChar ch) {
    return ch.isLetterOrNumber() || ch == '_';
  }

  RowFilterToken readNumber() {
    const int start = pos;
    bool seenDigit = false;
    bool seenDot = false;
    if (pos < input.size() && (input[pos] == '+' || input[pos] == '-'))
      ++pos;
    while (pos < input.size()) {
      QChar ch = input[pos];
      if (ch.isDigit()) {
        seenDigit = true;
        ++pos;
        continue;
      }
      if (ch == '.' && !seenDot) {
        seenDot = true;
        ++pos;
        continue;
      }
      if ((ch == 'e' || ch == 'E') && seenDigit) {
        int look = pos + 1;
        if (look < input.size() && (input[look] == '+' || input[look] == '-'))
          ++look;
        bool expDigits = false;
        while (look < input.size() && input[look].isDigit()) {
          expDigits = true;
          ++look;
        }
        if (!expDigits)
          break;
        pos = look;
        continue;
      }
      break;
    }
    if (!seenDigit)
      return {RowFilterTokenKind::Invalid, QString()};
    return {RowFilterTokenKind::Number, input.mid(start, pos - start)};
  }

  RowFilterToken readString(QChar quote) {
    ++pos;
    QString value;
    while (pos < input.size()) {
      QChar ch = input[pos++];
      if (ch == quote)
        return {RowFilterTokenKind::String, value};
      if (ch == '\\' && pos < input.size()) {
        const QChar esc = input[pos++];
        if (esc == 'n')
          value.append('\n');
        else if (esc == 'r')
          value.append('\r');
        else if (esc == 't')
          value.append('\t');
        else
          value.append(esc);
      } else {
        value.append(ch);
      }
    }
    return {RowFilterTokenKind::Invalid, QString()};
  }

  /* SDDS names may contain brackets ("Q[0]"), so nested pairs belong to the name. */
  RowFilterToken readBracketIdentifier() {
    ++pos;
    const int start = pos;
    int depth = 1;
    for (; pos < input.size(); ++pos) {
      if (input[pos] == '[')
        ++depth;
      else if (input[pos] == ']' && --depth == 0)
        break;
    }
    if (pos >= input.size())
      return {RowFilterTokenKind::Invalid, QString()};
    const QString ident = input.mid(start, pos - start).trimmed();
    ++pos;
    return {RowFilterTokenKind::Identifier, ident, true};
  }

  void next() {
    skipWs();
    if (pos >= input.size()) {
      current = {RowFilterTokenKind::End, QString()};
      return;
    }

    const QChar ch = input[pos];
    if (isIdentStart(ch)) {
      const int start = pos;
      ++pos;
      while (pos < input.size() && isIdentPart(input[pos]))
        ++pos;
      current = {RowFilterTokenKind::Identifier, input.mid(start, pos - start)};
      return;
    }

    if (ch == '[') {
      RowFilterToken token = readBracketIdentifier();
      if (token.kind == RowFilterTokenKind::Invalid) {
        current = token;
        current.text = QObject::tr("Unterminated [column] name");
      } else {
        current = token;
      }
      return;
    }

    if (ch.isDigit() || ch == '.' || ch == '+' || ch == '-') {
      const int start = pos;
      RowFilterToken token = readNumber();
      if (token.kind != RowFilterTokenKind::Invalid) {
        current = token;
        return;
      }
      // Report a stray sign or dot instead of skipping it, so "X ->= 1" is not read as "X >= 1".
      pos = start;
    }

    if (ch == '"' || ch == '\'') {
      RowFilterToken token = readString(ch);
      if (token.kind == RowFilterTokenKind::Invalid) {
        current = token;
        current.text = QObject::tr("Unterminated string literal");
      } else {
        current = token;
      }
      return;
    }

    if (input.mid(pos, 2) == "&&") {
      pos += 2;
      current = {RowFilterTokenKind::And, "&&"};
      return;
    }
    if (input.mid(pos, 2) == "||") {
      pos += 2;
      current = {RowFilterTokenKind::Or, "||"};
      return;
    }
    if (input.mid(pos, 2) == "==") {
      pos += 2;
      current = {RowFilterTokenKind::Eq, "=="};
      return;
    }
    if (input.mid(pos, 2) == "!=") {
      pos += 2;
      current = {RowFilterTokenKind::Ne, "!="};
      return;
    }
    if (input.mid(pos, 2) == "<=") {
      pos += 2;
      current = {RowFilterTokenKind::Le, "<="};
      return;
    }
    if (input.mid(pos, 2) == ">=") {
      pos += 2;
      current = {RowFilterTokenKind::Ge, ">="};
      return;
    }

    ++pos;
    switch (ch.unicode()) {
    case '(':
      current = {RowFilterTokenKind::LParen, "("};
      return;
    case ')':
      current = {RowFilterTokenKind::RParen, ")"};
      return;
    case '!':
      current = {RowFilterTokenKind::Not, "!"};
      return;
    case '<':
      current = {RowFilterTokenKind::Lt, "<"};
      return;
    case '>':
      current = {RowFilterTokenKind::Gt, ">"};
      return;
    default:
      current = {RowFilterTokenKind::Invalid,
                 QObject::tr("Unexpected character '%1'").arg(ch)};
      return;
    }
  }

  QString input;
  int pos;
  Resolver resolver;
  int depth;
  RowFilterToken current;
};

/*
 * Text for a computed value stored in a cell of the given type.  Where long
 * double is no wider than double (MSVC), preserve doubles with their shortest
 * exact representation and format integers without exponent notation that
 * integer columns reject. Values outside the type's range
 * keep the generic text so validation still rejects them.
 */
static QString numericResultText(long double value, int type) {
  if (std::isfinite(value)) {
    if (type == SDDS_DOUBLE ||
        (type == SDDS_LONGDOUBLE &&
         std::numeric_limits<long double>::digits <= std::numeric_limits<double>::digits)) {
      if (fabsl(value) <= std::numeric_limits<double>::max()) {
        const double narrowed = static_cast<double>(value);
        if (value == 0 || narrowed != 0)
          return shortestDoubleText(narrowed);
      }
    } else if (type == SDDS_FLOAT) {
      if (fabsl(value) <= std::numeric_limits<float>::max()) {
        const float narrowed = static_cast<float>(value);
        if (value == 0 || narrowed != 0)
          return shortestFloatText(narrowed);
      }
    } else if (SDDS_INTEGER_TYPE(type) && value == floorl(value)) {
      // 2^63 and 2^64 are exact in every long double format.
      if (value >= -9223372036854775808.0L && value < 9223372036854775808.0L)
        return QString::number(static_cast<qlonglong>(value));
      if (value >= 0 && value < 18446744073709551616.0L)
        return QString::number(static_cast<qulonglong>(value));
    }
  }
  // Keep an out-of-range result as text so validation can reject it before
  // changing cells. In particular, never turn nonzero underflow into valid "0".
  return longDoubleToText(value);
}

static QString applyTemplateVariables(const QString &templ,
                                      const QString &x,
                                      const QString &a,
                                      int i,
                                      int row,
                                      int col,
                                      int dr,
                                      int dc) {
  const QHash<QString, QString> variables = {
      {"x", x}, {"a", a}, {"i", QString::number(i)},
      {"row", QString::number(row)}, {"col", QString::number(col)},
      {"dr", QString::number(dr)}, {"dc", QString::number(dc)}};
  const QRegularExpression tokens(QStringLiteral("\\$\\{(x|a|i|row|col|dr|dc)\\}"));
  auto matches = tokens.globalMatch(templ);
  QString out;
  int offset = 0;
  // Substitute the original template once; cell text can itself contain tokens.
  while (matches.hasNext()) {
    const QRegularExpressionMatch match = matches.next();
    out += templ.mid(offset, match.capturedStart() - offset);
    out += variables.value(match.captured(1));
    offset = match.capturedEnd();
  }
  out += templ.mid(offset);
  return out;
}

struct StructuralSnapshot {
  SDDS_DATASET dataset;
  QVector<PageStore> pages;
  int currentPage;
  bool hasDataset;

  StructuralSnapshot() : currentPage(0), hasDataset(false) {
    memset(&dataset, 0, sizeof(dataset));
  }

  ~StructuralSnapshot() { clear(); }

  StructuralSnapshot(const StructuralSnapshot &) = delete;
  StructuralSnapshot &operator=(const StructuralSnapshot &) = delete;

  StructuralSnapshot(StructuralSnapshot &&other) noexcept
      : dataset(other.dataset), pages(std::move(other.pages)),
        currentPage(other.currentPage), hasDataset(other.hasDataset) {
    memset(&other.dataset, 0, sizeof(other.dataset));
    other.currentPage = 0;
    other.hasDataset = false;
  }

  StructuralSnapshot &operator=(StructuralSnapshot &&other) noexcept {
    if (this == &other)
      return *this;
    clear();
    dataset = other.dataset;
    pages = std::move(other.pages);
    currentPage = other.currentPage;
    hasDataset = other.hasDataset;
    memset(&other.dataset, 0, sizeof(other.dataset));
    other.currentPage = 0;
    other.hasDataset = false;
    return *this;
  }

  void clear() {
    if (hasDataset)
      SDDS_Terminate(&dataset);
    memset(&dataset, 0, sizeof(dataset));
    pages.clear();
    currentPage = 0;
    hasDataset = false;
  }
};

bool captureStructuralSnapshot(SDDSEditor *editor, StructuralSnapshot *snapshot) {
  if (!editor || !snapshot)
    return false;
  snapshot->clear();
  snapshot->pages = editor->pages;
  snapshot->currentPage = editor->currentPage;
  snapshot->hasDataset = editor->datasetLoaded;
  if (!snapshot->hasDataset)
    return true;
  if (!SDDS_InitializeCopy(&snapshot->dataset, &editor->dataset, NULL, (char *)"m")) {
    QMessageBox::warning(editor, QObject::tr("SDDS"),
                         QObject::tr("Failed to capture editor state for undo"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    snapshot->clear();
    return false;
  }
  return true;
}

bool restoreStructuralSnapshot(SDDSEditor *editor, const StructuralSnapshot &snapshot) {
  if (!editor)
    return false;

  if (editor->datasetLoaded) {
    SDDS_Terminate(&editor->dataset);
    memset(&editor->dataset, 0, sizeof(editor->dataset));
    editor->datasetLoaded = false;
  }

  if (snapshot.hasDataset) {
    if (!SDDS_InitializeCopy(&editor->dataset,
                             const_cast<SDDS_DATASET *>(&snapshot.dataset),
                             NULL, (char *)"m")) {
      QMessageBox::warning(editor, QObject::tr("SDDS"),
                           QObject::tr("Failed to restore editor state from undo"));
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return false;
    }
    editor->datasetLoaded = true;
  }

  editor->pages = snapshot.pages;
  if (!editor->pages.isEmpty()) {
    const int lastPageIndex = static_cast<int>(editor->pages.size()) - 1;
    editor->currentPage = std::max(0, std::min(snapshot.currentPage, lastPageIndex));
  } else {
    editor->currentPage = 0;
  }

  // The ASCII/Binary choice is a save option, not document structure; the
  // snapshot layout still holds the mode the file was loaded with, so undo and
  // redo leave the user's current choice alone.

  editor->pageCombo->blockSignals(true);
  editor->pageCombo->clear();
  for (int i = 0; i < editor->pages.size(); ++i)
    editor->pageCombo->addItem(QObject::tr("Page %1").arg(i + 1));
  if (!editor->pages.isEmpty())
    editor->pageCombo->setCurrentIndex(editor->currentPage);
  editor->pageCombo->blockSignals(false);

  editor->populateModels();
  if (editor->datasetLoaded && !editor->pages.isEmpty())
    editor->loadPage(editor->currentPage + 1);
  editor->updateWindowTitle();
  return true;
}

class StructuralChangeCommand : public QUndoCommand {
public:
  StructuralChangeCommand(SDDSEditor *editor,
                          StructuralSnapshot &&before,
                          StructuralSnapshot &&after,
                          const QString &label)
      : editor(editor),
        beforeState(std::move(before)),
        afterState(std::move(after)),
        skipInitialRedo(true) {
    setText(label);
  }

  void undo() override { apply(beforeState); }

  void redo() override {
    if (skipInitialRedo) {
      skipInitialRedo = false;
      return;
    }
    apply(afterState);
  }

private:
  void apply(const StructuralSnapshot &state) {
    if (!editor)
      return;
    editor->applyingStructuralUndo = true;
    bool ok = restoreStructuralSnapshot(editor, state);
    editor->applyingStructuralUndo = false;
    if (ok)
      editor->markDirty();
  }

  SDDSEditor *editor;
  StructuralSnapshot beforeState;
  StructuralSnapshot afterState;
  bool skipInitialRedo;
};

void pushStructuralUndoCommand(SDDSEditor *editor,
                               StructuralSnapshot &&before,
                               const QString &label) {
  if (!editor || !editor->undoStack)
    return;
  StructuralSnapshot after;
  if (!captureStructuralSnapshot(editor, &after))
    return;
  editor->undoStack->push(new StructuralChangeCommand(editor,
                                                      std::move(before),
                                                      std::move(after),
                                                      label));
}

class CaretOnDoubleClickLineEdit : public SDDSTextEdit {
public:
  explicit CaretOnDoubleClickLineEdit(QWidget *parent = nullptr) : SDDSTextEdit(parent) {}

  void setMultiCellPasteHandler(std::function<void()> handler) {
    multiCellPasteHandler = std::move(handler);
  }

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    // QLineEdit normally selects a word on double-click; sddseditor wants double-click
    // to behave like a normal click (place caret).
    setCursorPosition(cursorPositionAt(event->pos()));
    deselect();
    event->accept();
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event && event->matches(QKeySequence::Paste)) {
      const QString text = QApplication::clipboard()->text();
      if ((text.contains('\t') || text.contains('\n') || text.contains('\r')) &&
          multiCellPasteHandler) {
        multiCellPasteHandler();
        event->accept();
        return;
      }
    }
    QLineEdit::keyPressEvent(event);
  }

private:
  std::function<void()> multiCellPasteHandler;
};

/* Header data role holding the muted second header line (type, shape, units). */
static const int HeaderSubtitleRole = Qt::UserRole + 41;

/* Private clipboard format: JSON rows of cell strings, null where a cell was not selected. */
static const char *const editorCellsMime = "application/x-sddseditor-cells";

static QString middleDot() { return QString(QChar(0x00B7)); }

static int textAdvance(const QFontMetrics &fm, const QString &text) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
  return fm.horizontalAdvance(text);
#else
  return fm.width(text);
#endif
}

/** Join non-empty header fragments as "double · m". */
static QString joinSubtitle(const QStringList &parts) {
  QStringList kept;
  for (const QString &part : parts)
    if (!part.isEmpty())
      kept << part;
  return kept.join(QString(" %1 ").arg(middleDot()));
}

/** Pick an installed monospace font, falling back to the platform fixed font. */
static QFont preferredTableFont() {
#if defined(Q_OS_MAC)
  // macOS text is drawn at 72 points per inch, so match its larger UI font.
  const qreal pointSize = 12;
#else
  const qreal pointSize = 10;
#endif
  const char *candidates[] = {"Source Code Pro", "JetBrains Mono", "Cascadia Mono",
                              "Consolas", "Menlo", "DejaVu Sans Mono", "Liberation Mono"};
  for (const char *name : candidates) {
    QFont font(QString::fromLatin1(name));
    font.setPointSizeF(pointSize);
    if (QFontInfo(font).family().compare(QString::fromLatin1(name), Qt::CaseInsensitive) == 0)
      return font;
  }
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSizeF(pointSize);
  return font;
}

/** Color tokens shared by the editor palette, style sheet and icons. */
struct EditorTheme {
  QColor win, chrome, surface, header, border, grid, text, muted;
  QColor accent, accentSoft, accentText, onAccent, zebra, warn, hover;
};

static EditorTheme editorTheme(bool dark) {
  EditorTheme t;
  if (dark) {
    t.win = QColor("#16181C");
    t.chrome = QColor("#1D2025");
    t.surface = QColor("#1B1E23");
    t.header = QColor("#252930");
    t.border = QColor("#363B44");
    t.grid = QColor("#2A2E35");
    t.text = QColor("#E6E8EC");
    t.muted = QColor("#A3ABB8");
    t.accent = QColor("#7AA7FF");
    t.accentSoft = QColor("#22324F");
    t.accentText = QColor("#B9CFFF");
    t.onAccent = QColor("#0B1220");
    t.zebra = QColor("#1F2228");
    t.warn = QColor("#F0A04B");
    t.hover = QColor("#2A2F37");
  } else {
    t.win = QColor("#F4F5F7");
    t.chrome = QColor("#FBFBFC");
    t.surface = QColor("#FFFFFF");
    t.header = QColor("#F1F3F6");
    t.border = QColor("#DCE0E6");
    t.grid = QColor("#ECEEF2");
    t.text = QColor("#1C2230");
    t.muted = QColor("#5A6374");
    t.accent = QColor("#2463D6");
    t.accentSoft = QColor("#E6EEFC");
    t.accentText = QColor("#1A4FB0");
    t.onAccent = QColor("#FFFFFF");
    t.zebra = QColor("#FAFBFC");
    t.warn = QColor("#A6520A");
    t.hover = QColor("#ECEFF3");
  }
  return t;
}

static QPalette editorPalette(const EditorTheme &t, QPalette pal) {
  const QPalette::ColorGroup groups[] = {QPalette::Active, QPalette::Inactive};
  for (QPalette::ColorGroup g : groups) {
    pal.setColor(g, QPalette::Window, t.win);
    pal.setColor(g, QPalette::WindowText, t.text);
    pal.setColor(g, QPalette::Base, t.surface);
    pal.setColor(g, QPalette::AlternateBase, t.zebra);
    pal.setColor(g, QPalette::Text, t.text);
    pal.setColor(g, QPalette::Button, t.chrome);
    pal.setColor(g, QPalette::ButtonText, t.text);
    pal.setColor(g, QPalette::Highlight, t.accent);
    pal.setColor(g, QPalette::HighlightedText, t.onAccent);
    pal.setColor(g, QPalette::ToolTipBase, t.surface);
    pal.setColor(g, QPalette::ToolTipText, t.text);
    pal.setColor(g, QPalette::Light, t.surface);
    pal.setColor(g, QPalette::Midlight, t.header);
    pal.setColor(g, QPalette::Mid, t.border);
    pal.setColor(g, QPalette::Dark, t.border.darker(130));
    pal.setColor(g, QPalette::Link, t.accent);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    pal.setColor(g, QPalette::PlaceholderText, t.muted);
#endif
  }
  pal.setColor(QPalette::Disabled, QPalette::Window, t.win);
  pal.setColor(QPalette::Disabled, QPalette::Base, t.win);
  pal.setColor(QPalette::Disabled, QPalette::Button, t.chrome);
  pal.setColor(QPalette::Disabled, QPalette::Text, t.muted);
  pal.setColor(QPalette::Disabled, QPalette::WindowText, t.muted);
  pal.setColor(QPalette::Disabled, QPalette::ButtonText, t.muted);
  return pal;
}

static QString editorStyleSheet(const EditorTheme &t) {
  QString css = QStringLiteral(
      "#centralArea { background: @win; }"
      "QToolBar#mainToolBar { background: @chrome; border: none; border-bottom: 1px solid @border; padding: 4px 8px; spacing: 2px; }"
      "QToolBar#mainToolBar::separator { background: @border; width: 1px; margin: 6px 6px; }"
      "QToolBar#mainToolBar QToolButton { border: none; border-radius: 6px; padding: 5px 8px; color: @text; background: transparent; }"
      "QToolBar#mainToolBar QToolButton:hover { background: @hover; }"
      "QToolBar#mainToolBar QToolButton:pressed { background: @border; }"
      "QToolBar#mainToolBar QToolButton:checked { background: @accentSoft; color: @accentText; }"
      "QToolBar#mainToolBar QToolButton:disabled { color: @muted; }"
      "QToolBar#mainToolBar QToolButton#pageNav { border: 1px solid @border; background: @surface; padding: 3px; }"
      "QToolBar#mainToolBar QToolButton#pageNav:hover { background: @hover; }"
      "QLabel#toolbarLabel { color: @muted; padding: 0px 4px; }"
      "QFrame#formatSwitch { background: @header; border: 1px solid @border; border-radius: 7px; }"
      "QToolBar#mainToolBar QFrame#formatSwitch QToolButton { border-radius: 5px; padding: 3px 12px; color: @muted; }"
      "QToolBar#mainToolBar QFrame#formatSwitch QToolButton:checked { background: @surface; color: @text; }"
      "QFrame#dataPanel { background: @surface; border: 1px solid @border; border-radius: 8px; }"
      "QFrame#panelHeader { background: transparent; border: none; }"
      "QFrame#panelRule { background: @border; border: none; }"
      "QToolButton#panelToggle { border: none; color: @text; padding: 2px 4px; background: transparent; }"
      "QLabel#countChip { background: @header; color: @muted; border-radius: 9px; padding: 1px 8px; }"
      "QToolButton#panelAction { border: none; border-radius: 5px; padding: 3px 8px; color: @muted; background: transparent; }"
      "QToolButton#panelAction:hover { background: @hover; color: @text; }"
      "QFrame#filterChip { background: @accentSoft; border-radius: 11px; }"
      "QFrame#filterChip QToolButton { border: none; background: transparent; color: @accentText; padding: 1px 4px; }"
      "QFrame#filterChip QToolButton:hover { color: @text; }"
      "QLineEdit#panelSearch { background: @win; border: 1px solid @border; border-radius: 6px; padding: 2px 4px; color: @text; }"
      "QLineEdit#panelSearch:focus { border-color: @accent; }"
      "QFrame#dataPanel QTableView { background: @surface; alternate-background-color: @zebra; gridline-color: @grid;"
      "  border: none; color: @text; selection-background-color: @accentSoft; selection-color: @text; }"
      "QFrame#dataPanel QTableView::item { padding: 0px 6px; }"
      "QFrame#dataPanel QTableView::item:focus { border: 2px solid @accent; padding: 0px 4px; }"
      "QFrame#dataPanel QHeaderView { background: @header; border: none; }"
      "QFrame#dataPanel QHeaderView::section { background: @header; color: @text; border: none;"
      "  border-right: 1px solid @grid; border-bottom: 1px solid @border; padding: 0px 8px; }"
      "QFrame#dataPanel QHeaderView::section:vertical { color: @muted; border-right: 1px solid @border;"
      "  border-bottom: 1px solid @grid; padding: 0px 6px; }"
      "QFrame#dataPanel QHeaderView::section:checked { background: @accentSoft; color: @accentText; }"
      "QFrame#dataPanel QTableCornerButton::section { background: @header; border: none;"
      "  border-right: 1px solid @border; border-bottom: 1px solid @border; }"
      "QStatusBar { background: @chrome; border-top: 1px solid @border; color: @muted; }"
      "QStatusBar::item { border: none; }"
      "QStatusBar QLabel { color: @muted; padding: 0px 6px; }"
      "QStatusBar QLabel#modifiedLabel[modified=\"true\"] { color: @warn; font-weight: bold; }"
      "QToolButton#messagesButton { border: 1px solid @border; border-radius: 5px; background: @surface;"
      "  color: @text; padding: 1px 8px; margin: 2px 4px; }"
      "QToolButton#messagesButton:checked { background: @accentSoft; color: @accentText; }"
      "QDockWidget#messagesDock QPlainTextEdit { background: @surface; color: @text; border: none; }");
  const QList<QPair<QString, QColor>> tokens = {
      {"@win", t.win}, {"@chrome", t.chrome}, {"@surface", t.surface}, {"@header", t.header},
      {"@border", t.border}, {"@grid", t.grid}, {"@text", t.text}, {"@muted", t.muted},
      {"@accentSoft", t.accentSoft}, {"@accentText", t.accentText}, {"@accent", t.accent},
      {"@zebra", t.zebra}, {"@warn", t.warn}, {"@hover", t.hover}};
  // Longer names first, so "@accent" does not consume "@accentSoft".
  for (const auto &token : tokens)
    css.replace(token.first, token.second.name());
  return css;
}

enum EditorIcon {
  IconOpen, IconSave, IconUndo, IconRedo, IconPrev, IconNext, IconFilter, IconPlot,
  IconGrid, IconPlus, IconSliders, IconTerminal, IconClose, IconSearch,
  IconChevronDown, IconChevronRight
};

enum IconTone { ToneText, ToneMuted, ToneAccent };

/** Outline icon geometry on a 24 x 24 grid. */
static QPainterPath editorIconPath(int kind) {
  QPainterPath p;
  switch (kind) {
  case IconOpen:
    p.moveTo(3, 18.5); p.lineTo(3, 6); p.lineTo(9, 6); p.lineTo(11, 8.5);
    p.lineTo(20, 8.5); p.lineTo(20, 18.5); p.closeSubpath();
    break;
  case IconSave:
    p.moveTo(4, 3.5); p.lineTo(16, 3.5); p.lineTo(20.5, 8); p.lineTo(20.5, 20.5);
    p.lineTo(4, 20.5); p.closeSubpath();
    p.moveTo(7.5, 3.5); p.lineTo(7.5, 8.5); p.lineTo(15, 8.5); p.lineTo(15, 3.5);
    p.moveTo(7.5, 20.5); p.lineTo(7.5, 14); p.lineTo(17, 14); p.lineTo(17, 20.5);
    break;
  case IconUndo:
    p.moveTo(9, 14); p.lineTo(4, 9); p.lineTo(9, 4);
    p.moveTo(4, 9); p.lineTo(14, 9); p.arcTo(QRectF(8, 9, 12, 12), 90, -180); p.lineTo(11, 21);
    break;
  case IconRedo:
    p.moveTo(15, 14); p.lineTo(20, 9); p.lineTo(15, 4);
    p.moveTo(20, 9); p.lineTo(10, 9); p.arcTo(QRectF(4, 9, 12, 12), 90, 180); p.lineTo(13, 21);
    break;
  case IconPrev:
    p.moveTo(15, 18); p.lineTo(9, 12); p.lineTo(15, 6);
    break;
  case IconNext:
    p.moveTo(9, 18); p.lineTo(15, 12); p.lineTo(9, 6);
    break;
  case IconFilter:
    p.moveTo(3, 5); p.lineTo(21, 5); p.lineTo(14, 13); p.lineTo(14, 19);
    p.lineTo(10, 17); p.lineTo(10, 13); p.closeSubpath();
    break;
  case IconPlot:
    p.moveTo(3, 3); p.lineTo(3, 21); p.lineTo(21, 21);
    p.moveTo(7, 15); p.lineTo(11, 10); p.lineTo(15, 13); p.lineTo(20, 6);
    break;
  case IconGrid:
    p.addRoundedRect(QRectF(3, 3, 18, 18), 2, 2);
    p.moveTo(3, 9); p.lineTo(21, 9); p.moveTo(3, 15); p.lineTo(21, 15);
    p.moveTo(9, 3); p.lineTo(9, 21); p.moveTo(15, 3); p.lineTo(15, 21);
    break;
  case IconPlus:
    p.moveTo(12, 5); p.lineTo(12, 19); p.moveTo(5, 12); p.lineTo(19, 12);
    break;
  case IconSliders:
    p.moveTo(4, 7); p.lineTo(13, 7); p.moveTo(17, 7); p.lineTo(20, 7);
    p.moveTo(4, 17); p.lineTo(7, 17); p.moveTo(11, 17); p.lineTo(20, 17);
    p.addEllipse(QPointF(15, 7), 2, 2);
    p.addEllipse(QPointF(9, 17), 2, 2);
    break;
  case IconTerminal:
    p.moveTo(4, 17); p.lineTo(10, 12); p.lineTo(4, 7);
    p.moveTo(12, 19); p.lineTo(20, 19);
    break;
  case IconClose:
    p.moveTo(18, 6); p.lineTo(6, 18); p.moveTo(6, 6); p.lineTo(18, 18);
    break;
  case IconSearch:
    p.addEllipse(QPointF(11, 11), 7, 7);
    p.moveTo(20, 20); p.lineTo(16, 16);
    break;
  case IconChevronDown:
    p.moveTo(6, 9); p.lineTo(12, 15); p.lineTo(18, 9);
    break;
  case IconChevronRight:
    p.moveTo(9, 6); p.lineTo(15, 12); p.lineTo(9, 18);
    break;
  default:
    break;
  }
  return p;
}

/*
 * Icons are painted from vector outlines at several pixel sizes so they stay
 * crisp on standard and high-density displays without extra Qt modules.
 */
static QIcon makeEditorIcon(int kind, const QColor &color, const QColor &disabled) {
  const QPainterPath path = editorIconPath(kind);
  QIcon icon;
  const int sizes[] = {14, 16, 20, 24, 28, 32, 40, 48, 64};
  for (int size : sizes) {
    for (int mode = 0; mode < 2; ++mode) {
      QPixmap pm(size, size);
      pm.fill(Qt::transparent);
      QPainter painter(&pm);
      painter.setRenderHint(QPainter::Antialiasing);
      const qreal scale = size / 24.0;
      painter.scale(scale, scale);
      const qreal width = std::max<qreal>(1.75, 1.15 / scale);
      painter.setPen(QPen(mode ? disabled : color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.setBrush(Qt::NoBrush);
      painter.drawPath(path);
      painter.end();
      icon.addPixmap(pm, mode ? QIcon::Disabled : QIcon::Normal);
    }
  }
  return icon;
}

/** Card holding one data table with a header bar that collapses the table. */
class DataPanel : public QFrame {
public:
  static const int HeaderHeight = 34;
  static const int BottomInset = 5;

  DataPanel(const QString &title, QWidget *parent = nullptr)
      : QFrame(parent), body(nullptr) {
    setObjectName("dataPanel");
    outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, BottomInset);
    outer->setSpacing(0);

    header = new QFrame(this);
    header->setObjectName("panelHeader");
    header->setFixedHeight(HeaderHeight);
    headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(6, 0, 8, 0);
    headerRow->setSpacing(6);

    toggle = new QToolButton(header);
    toggle->setObjectName("panelToggle");
    toggle->setText(title);
    toggle->setCheckable(true);
    toggle->setChecked(true);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setIconSize(QSize(14, 14));
    toggle->setToolTip(QObject::tr("Show or hide %1").arg(title));
    QFont titleFont = toggle->font();
    titleFont.setBold(true);
    toggle->setFont(titleFont);
    headerRow->addWidget(toggle);

    count = new QLabel(header);
    count->setObjectName("countChip");
    QFont chipFont = count->font();
    chipFont.setPointSizeF(std::max<qreal>(7.0, chipFont.pointSizeF() - 1));
    count->setFont(chipFont);
    count->setAlignment(Qt::AlignCenter);
    count->setFixedHeight(QFontMetrics(chipFont).height() + 4);
    headerRow->addWidget(count, 0, Qt::AlignVCenter);
    outer->addWidget(header);

    rule = new QFrame(this);
    rule->setObjectName("panelRule");
    rule->setFixedHeight(1);
    outer->addWidget(rule);

    QObject::connect(toggle, &QToolButton::toggled, this, [this](bool on) {
      if (body)
        body->setVisible(on);
      rule->setVisible(on);
      outer->setContentsMargins(0, 0, 0, on ? BottomInset : 0);
      toggle->setIcon(on ? expandedIcon : collapsedIcon);
    });
  }

  void setBody(QWidget *widget) {
    body = widget;
    outer->addWidget(widget, 1);
  }

  void addHeaderWidget(QWidget *widget) { headerRow->addWidget(widget, 0, Qt::AlignVCenter); }
  void addHeaderStretch() { headerRow->addStretch(1); }
  QToolButton *toggleButton() const { return toggle; }

  void setCountText(const QString &text) {
    if (count->text() != text)
      count->setText(text);
  }

  void setToggleIcons(const QIcon &expanded, const QIcon &collapsed) {
    expandedIcon = expanded;
    collapsedIcon = collapsed;
    toggle->setIcon(toggle->isChecked() ? expandedIcon : collapsedIcon);
  }

  void setChecked(bool on) { toggle->setChecked(on); }
  bool isChecked() const { return toggle->isChecked(); }

  /** Height of everything except the body while the panel is expanded. */
  int chromeHeight() {
    ensurePolished();
    const QMargins frame = contentsMargins();
    return frame.top() + frame.bottom() + HeaderHeight + rule->height() + BottomInset;
  }

private:
  QVBoxLayout *outer;
  QFrame *header;
  QHBoxLayout *headerRow;
  QToolButton *toggle;
  QLabel *count;
  QFrame *rule;
  QWidget *body;
  QIcon expandedIcon;
  QIcon collapsedIcon;
};

/** Horizontal header drawing a bold name with a muted type/units line. */
class TwoLineHeaderView : public QHeaderView {
public:
  explicit TwoLineHeaderView(QWidget *parent = nullptr)
      : QHeaderView(Qt::Horizontal, parent) {
    setSectionsClickable(true);
    setHighlightSections(true);
  }

  void setColors(const QColor &text, const QColor &muted, const QColor &accentText) {
    textColor = text;
    mutedColor = muted;
    highlightColor = accentText;
    viewport()->update();
  }

protected:
  void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override {
    if (!rect.isValid() || !model())
      return;
    QStyleOptionHeader opt;
    initStyleOption(&opt);
    opt.rect = rect;
    opt.section = logicalIndex;
    opt.text.clear();
    bool highlighted = false;
    if (highlightSections() && selectionModel()) {
      highlighted = selectionModel()->columnIntersectsSelection(logicalIndex, rootIndex());
      if (highlighted)
        opt.state |= QStyle::State_On;
    }
    const int visual = visualIndex(logicalIndex);
    if (count() == 1)
      opt.position = QStyleOptionHeader::OnlyOneSection;
    else if (visual == 0)
      opt.position = QStyleOptionHeader::Beginning;
    else if (visual == count() - 1)
      opt.position = QStyleOptionHeader::End;
    else
      opt.position = QStyleOptionHeader::Middle;
    painter->save();
    style()->drawControl(QStyle::CE_HeaderSection, &opt, painter, this);
    painter->restore();

    const QString title = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
    const QString subtitle = model()->headerData(logicalIndex, orientation(), HeaderSubtitleRole).toString();
    const QVariant alignValue = model()->headerData(logicalIndex, orientation(), Qt::TextAlignmentRole);
    const Qt::Alignment hAlign = (alignValue.isValid() ? Qt::Alignment(alignValue.toInt()) : defaultAlignment()) &
                                 Qt::AlignHorizontal_Mask;
    const QRect textRect = rect.adjusted(8, 2, -8, -2);
    const QFont titleFont = sectionTitleFont();
    const QFont subFont = subtitleFont();
    const QFontMetrics titleFm(titleFont);
    const QFontMetrics subFm(subFont);

    painter->save();
    painter->setClipRect(rect);
    int top = textRect.top();
    if (subtitle.isEmpty())
      top += (textRect.height() - titleFm.height()) / 2;
    else
      top += (textRect.height() - titleFm.height() - subFm.height()) / 2;
    painter->setFont(titleFont);
    painter->setPen(highlighted && highlightColor.isValid() ? highlightColor
                    : textColor.isValid() ? textColor : palette().color(QPalette::WindowText));
    painter->drawText(QRect(textRect.left(), top, textRect.width(), titleFm.height()),
                      int(hAlign | Qt::AlignVCenter),
                      titleFm.elidedText(title, Qt::ElideRight, textRect.width()));
    if (!subtitle.isEmpty()) {
      painter->setFont(subFont);
      painter->setPen(mutedColor.isValid() ? mutedColor : palette().color(QPalette::Disabled, QPalette::WindowText));
      painter->drawText(QRect(textRect.left(), top + titleFm.height(), textRect.width(), subFm.height()),
                        int(hAlign | Qt::AlignVCenter),
                        subFm.elidedText(subtitle, Qt::ElideRight, textRect.width()));
    }
    painter->restore();
  }

  QSize sectionSizeFromContents(int logicalIndex) const override {
    if (!model())
      return QHeaderView::sectionSizeFromContents(logicalIndex);
    const QString title = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
    const QString subtitle = model()->headerData(logicalIndex, orientation(), HeaderSubtitleRole).toString();
    const QFontMetrics titleFm(sectionTitleFont());
    const QFontMetrics subFm(subtitleFont());
    const int width = std::max(textAdvance(titleFm, title), textAdvance(subFm, subtitle)) + 20;
    const int height = titleFm.height() + subFm.height() + 8;
    return QSize(width, height);
  }

private:
  QFont sectionTitleFont() const {
    QFont f = font();
    f.setBold(true);
    return f;
  }

  QFont subtitleFont() const {
    QFont f = font();
    f.setPointSizeF(std::max<qreal>(7.0, f.pointSizeF() - 1));
    return f;
  }

  QColor textColor;
  QColor mutedColor;
  QColor highlightColor;
};

/** Splitter handle drawn as a small grip between panel cards. */
class PanelSplitterHandle : public QSplitterHandle {
public:
  PanelSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
      : QSplitterHandle(orientation, parent) {
    setAttribute(Qt::WA_Hover);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const char *property = underMouse() ? "gripHoverColor" : "gripColor";
    QColor color = splitter()->property(property).value<QColor>();
    if (!color.isValid())
      color = palette().color(QPalette::Mid);
    const QSizeF grip = orientation() == Qt::Vertical ? QSizeF(36, 3) : QSizeF(3, 36);
    const QRectF r(QPointF((width() - grip.width()) / 2.0, (height() - grip.height()) / 2.0), grip);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(r, 1.5, 1.5);
  }
};

class PanelSplitter : public QSplitter {
public:
  PanelSplitter(Qt::Orientation orientation, QWidget *parent = nullptr)
      : QSplitter(orientation, parent) {}

protected:
  QSplitterHandle *createHandle() override {
    return new PanelSplitterHandle(orientation(), this);
  }
};

class ParameterPageModel : public QAbstractTableModel {
public:
  ParameterPageModel(SDDS_DATASET *dataset, QVector<PageStore> *pages, int *currentPage,
                     QObject *parent = nullptr)
      : QAbstractTableModel(parent), dataset(dataset), pages(pages), currentPage(currentPage) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    Q_UNUSED(parent);
    if (!dataset || !pages || !currentPage)
      return 0;
    if (*currentPage < 0 || *currentPage >= pages->size())
      return 0;
    if (dataset->layout.n_parameters <= 0)
      return 0;
    return dataset->layout.n_parameters;
  }

  /* Column 0 holds the editable value; the others show read-only definition metadata. */
  enum Column { ValueColumn = 0, TypeColumn, UnitsColumn, DescriptionColumn, ColumnTotal };

  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    Q_UNUSED(parent);
    return ColumnTotal;
  }

  /* Display-only; callers repaint the view instead of emitting dataChanged,
     which would mark the document modified. */
  void setMutedColor(const QColor &color) { mutedColor = color; }

  void refreshMetadata(int first, int last) {
    if (first < 0 || last < first)
      return;
    emit dataChanged(index(first, TypeColumn), index(last, DescriptionColumn),
                     {Qt::DisplayRole, Qt::ForegroundRole});
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || !dataset || !pages || !currentPage)
      return QVariant();
    if (index.row() < 0 || index.row() >= dataset->layout.n_parameters)
      return QVariant();
    if (index.column() != ValueColumn) {
      const PARAMETER_DEFINITION &def = dataset->layout.parameter_definition[index.row()];
      if (role == Qt::ForegroundRole && mutedColor.isValid())
        return mutedColor;
      if (role == Qt::ToolTipRole && index.column() == TypeColumn)
        return tr("Double-click to change the type");
      if (role != Qt::DisplayRole)
        return QVariant();
      if (index.column() == TypeColumn)
        return QString::fromLocal8Bit(SDDS_GetTypeName(def.type));
      if (index.column() == UnitsColumn)
        return def.units ? QString::fromLocal8Bit(def.units) : QString();
      return def.description ? QString::fromLocal8Bit(def.description) : QString();
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
      return QVariant();
    if (*currentPage < 0 || *currentPage >= pages->size())
      return QVariant();
    const int r = index.row();
    const PageStore &pd = (*pages)[*currentPage];
    if (r < 0)
      return QVariant();
    if (r >= pd.parameters.size())
      return QVariant();
    return pd.parameters[r];
  }

  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
    if (!index.isValid() || role != Qt::EditRole)
      return false;
    if (!dataset || !pages || !currentPage)
      return false;
    if (*currentPage < 0 || *currentPage >= pages->size())
      return false;
    if (index.column() != 0)
      return false;
    PageStore &pd = (*pages)[*currentPage];
    const int r = index.row();
    if (r < 0)
      return false;
    if (r >= pd.parameters.size())
      return false;
    QString text = value.toString();
    if (pd.parameters[r] == text)
      return false;
    pd.parameters[r] = text;
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override {
    if (!index.isValid())
      return Qt::NoItemFlags;
    if (index.column() != ValueColumn)
      return Qt::ItemIsEnabled;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
  }

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override {
    if (orientation == Qt::Horizontal) {
      if (role == Qt::TextAlignmentRole)
        return int(Qt::AlignLeft | Qt::AlignVCenter);
      if (role != Qt::DisplayRole)
        return QVariant();
      switch (section) {
      case ValueColumn:
        return tr("Value");
      case TypeColumn:
        return tr("Type");
      case UnitsColumn:
        return tr("Units");
      case DescriptionColumn:
        return tr("Description");
      default:
        return QVariant();
      }
    }
    if (!dataset)
      return QVariant();
    if (section < 0 || section >= dataset->layout.n_parameters)
      return QVariant();
    const PARAMETER_DEFINITION &def = dataset->layout.parameter_definition[section];
    if (role == Qt::ToolTipRole && def.description)
      return QString::fromLocal8Bit(def.description);
    if (role != Qt::DisplayRole)
      return QVariant();
    return QString::fromLocal8Bit(def.name);
  }

  void refresh() {
    beginResetModel();
    endResetModel();
  }

  void refreshRowHeaders(int first, int last) {
    emit headerDataChanged(Qt::Vertical, first, last);
  }

private:
  SDDS_DATASET *dataset;
  QVector<PageStore> *pages;
  int *currentPage;
  QColor mutedColor;
};

static QVariant numericAlignment(int32_t type) {
  return SDDS_NUMERIC_TYPE(type) ? int(Qt::AlignRight | Qt::AlignVCenter)
                                 : int(Qt::AlignLeft | Qt::AlignVCenter);
}

static QString definitionToolTip(const char *name, int32_t type, const char *units,
                                 const char *description) {
  QStringList lines;
  lines << QString("%1 (%2)").arg(QString::fromLocal8Bit(name ? name : ""),
                                  QString::fromLocal8Bit(SDDS_GetTypeName(type)));
  if (units && *units)
    lines << QObject::tr("Units: %1").arg(QString::fromLocal8Bit(units));
  if (description && *description)
    lines << QString::fromLocal8Bit(description);
  return lines.join('\n');
}

class ColumnPageModel : public QAbstractTableModel {
public:
  ColumnPageModel(SDDS_DATASET *dataset, QVector<PageStore> *pages, int *currentPage,
                  QObject *parent = nullptr)
      : QAbstractTableModel(parent), dataset(dataset), pages(pages), currentPage(currentPage) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    Q_UNUSED(parent);
    if (!dataset || !pages || !currentPage)
      return 0;
    if (*currentPage < 0 || *currentPage >= pages->size())
      return 0;
    if (dataset->layout.n_columns <= 0)
      return 0;
    const PageStore &pd = (*pages)[*currentPage];
    return pd.columns.size() > 0 ? pd.columns[0].size() : 0;
  }

  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    Q_UNUSED(parent);
    if (!dataset)
      return 0;
    return dataset->layout.n_columns;
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || !dataset || !pages || !currentPage)
      return QVariant();
    if (role == Qt::TextAlignmentRole) {
      if (index.column() < 0 || index.column() >= dataset->layout.n_columns)
        return QVariant();
      return numericAlignment(dataset->layout.column_definition[index.column()].type);
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
      return QVariant();
    if (*currentPage < 0 || *currentPage >= pages->size())
      return QVariant();
    const PageStore &pd = (*pages)[*currentPage];
    int c = index.column();
    int r = index.row();
    if (c < 0 || c >= pd.columns.size())
      return QVariant();
    const QVector<QString> &col = pd.columns[c];
    if (r < 0 || r >= col.size())
      return QVariant();
    return col[r];
  }

  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
    if (!index.isValid() || role != Qt::EditRole)
      return false;
    if (!dataset || !pages || !currentPage)
      return false;
    if (*currentPage < 0 || *currentPage >= pages->size())
      return false;
    PageStore &pd = (*pages)[*currentPage];
    int c = index.column();
    int r = index.row();
    if (c < 0 || c >= pd.columns.size())
      return false;
    QVector<QString> &col = pd.columns[c];
    if (r < 0 || r >= col.size())
      return false;
    QString text = value.toString();
    if (col[r] == text)
      return false;
    col[r] = text;
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override {
    if (!index.isValid())
      return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
  }

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override {
    if (!dataset)
      return QVariant();
    if (orientation == Qt::Horizontal) {
      if (section < 0 || section >= dataset->layout.n_columns)
        return QVariant();
      const COLUMN_DEFINITION &def = dataset->layout.column_definition[section];
      switch (role) {
      case Qt::DisplayRole:
        return QString::fromLocal8Bit(def.name);
      case HeaderSubtitleRole:
        return joinSubtitle({QString::fromLocal8Bit(SDDS_GetTypeName(def.type)),
                             def.units ? QString::fromLocal8Bit(def.units) : QString()});
      case Qt::TextAlignmentRole:
        return numericAlignment(def.type);
      case Qt::ToolTipRole:
        return definitionToolTip(def.name, def.type, def.units, def.description);
      default:
        return QVariant();
      }
    }
    if (role != Qt::DisplayRole)
      return QVariant();
    return QString::number(section + 1);
  }

  void refresh() {
    beginResetModel();
    endResetModel();
  }

  void refreshHeaders(int first, int last) {
    emit headerDataChanged(Qt::Horizontal, first, last);
  }

private:
  SDDS_DATASET *dataset;
  QVector<PageStore> *pages;
  int *currentPage;
};

class ArrayPageModel : public QAbstractTableModel {
public:
  ArrayPageModel(SDDS_DATASET *dataset, QVector<PageStore> *pages, int *currentPage,
                 QObject *parent = nullptr)
      : QAbstractTableModel(parent), dataset(dataset), pages(pages), currentPage(currentPage), maxLen(0) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    Q_UNUSED(parent);
    if (!dataset || !pages || !currentPage)
      return 0;
    if (*currentPage < 0 || *currentPage >= pages->size())
      return 0;
    if (dataset->layout.n_arrays <= 0)
      return 0;
    return maxLen;
  }

  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    Q_UNUSED(parent);
    if (!dataset)
      return 0;
    return dataset->layout.n_arrays;
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || !dataset || !pages || !currentPage)
      return QVariant();
    if (role == Qt::TextAlignmentRole) {
      if (index.column() < 0 || index.column() >= dataset->layout.n_arrays)
        return QVariant();
      return numericAlignment(dataset->layout.array_definition[index.column()].type);
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
      return QVariant();
    if (*currentPage < 0 || *currentPage >= pages->size())
      return QVariant();
    const PageStore &pd = (*pages)[*currentPage];
    int c = index.column();
    int r = index.row();
    if (c < 0 || c >= pd.arrays.size())
      return QVariant();
    const QVector<QString> &vals = pd.arrays[c].values;
    if (r < 0 || r >= vals.size())
      return QVariant();
    return vals[r];
  }

  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
    if (!index.isValid() || role != Qt::EditRole)
      return false;
    if (!dataset || !pages || !currentPage)
      return false;
    if (*currentPage < 0 || *currentPage >= pages->size())
      return false;
    PageStore &pd = (*pages)[*currentPage];
    int c = index.column();
    int r = index.row();
    if (c < 0 || c >= pd.arrays.size())
      return false;
    QVector<QString> &vals = pd.arrays[c].values;
    if (r < 0 || r >= vals.size())
      return false;
    QString text = value.toString();
    if (vals[r] == text)
      return false;
    vals[r] = text;
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override {
    if (!index.isValid() || !pages || !currentPage)
      return Qt::NoItemFlags;
    if (*currentPage < 0 || *currentPage >= pages->size())
      return Qt::NoItemFlags;
    const PageStore &pd = (*pages)[*currentPage];
    int c = index.column();
    int r = index.row();
    if (c < 0 || c >= pd.arrays.size())
      return Qt::NoItemFlags;
    const QVector<QString> &vals = pd.arrays[c].values;
    if (r < 0)
      return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (r < vals.size())
      f |= Qt::ItemIsEditable;
    return f;
  }

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override {
    if (!dataset)
      return QVariant();
    if (orientation == Qt::Horizontal) {
      if (section < 0 || section >= dataset->layout.n_arrays)
        return QVariant();
      const ARRAY_DEFINITION &def = dataset->layout.array_definition[section];
      switch (role) {
      case Qt::DisplayRole:
        return QString::fromLocal8Bit(def.name);
      case HeaderSubtitleRole: {
        QStringList dims;
        if (pages && currentPage && *currentPage >= 0 && *currentPage < pages->size() &&
            section < (*pages)[*currentPage].arrays.size()) {
          for (int d : (*pages)[*currentPage].arrays[section].dims)
            dims << QString::number(d);
        }
        return joinSubtitle({QString::fromLocal8Bit(SDDS_GetTypeName(def.type)),
                             dims.join(QChar(0x00D7)),
                             def.units ? QString::fromLocal8Bit(def.units) : QString()});
      }
      case Qt::TextAlignmentRole:
        return numericAlignment(def.type);
      case Qt::ToolTipRole:
        return definitionToolTip(def.name, def.type, def.units, def.description);
      default:
        return QVariant();
      }
    }
    if (role != Qt::DisplayRole)
      return QVariant();
    return QString::number(section + 1);
  }

  void refresh() {
    beginResetModel();
    recomputeMaxLen();
    endResetModel();
  }

  void refreshHeaders(int first, int last) {
    emit headerDataChanged(Qt::Horizontal, first, last);
  }

private:
  void recomputeMaxLen() {
    maxLen = 0;
    if (!pages || !currentPage)
      return;
    if (*currentPage < 0 || *currentPage >= pages->size())
      return;
    const PageStore &pd = (*pages)[*currentPage];
    for (const ArrayStore &as : pd.arrays)
      if (as.values.size() > maxLen)
        maxLen = as.values.size();
  }

  SDDS_DATASET *dataset;
  QVector<PageStore> *pages;
  int *currentPage;
  int maxLen;
};

class SDDSItemDelegate : public QStyledItemDelegate {
public:
  using TypeFunc = std::function<int(const QModelIndex &)>;
  SDDSItemDelegate(TypeFunc tf, QUndoStack *stack, QObject *parent = nullptr,
                   std::function<void()> pasteHandler = {})
      : QStyledItemDelegate(parent), typeFunc(std::move(tf)), undoStack(stack),
        multiCellPasteHandler(std::move(pasteHandler)) {}

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override {
    Q_UNUSED(option);
    Q_UNUSED(index);
    CaretOnDoubleClickLineEdit *editor = new CaretOnDoubleClickLineEdit(parent);
    if (multiCellPasteHandler) {
      editor->setMultiCellPasteHandler(multiCellPasteHandler);
    } else if (QWidget *window = parent ? parent->window() : nullptr) {
      if (SDDSEditor *sddsEditor = qobject_cast<SDDSEditor *>(window)) {
        editor->setMultiCellPasteHandler([sddsEditor]() {
          QMetaObject::invokeMethod(sddsEditor, "paste", Qt::DirectConnection);
        });
      }
    }
    return editor;
  }

  void initStyleOption(QStyleOptionViewItem *option,
                       const QModelIndex &index) const override {
    QStyledItemDelegate::initStyleOption(option, index);
    option->text = canonicalizeForDisplay(option->text, typeFunc(index));
  }

  void setEditorData(QWidget *editor, const QModelIndex &index) const override {
    QLineEdit *line = qobject_cast<QLineEdit *>(editor);
    if (!line) {
      QStyledItemDelegate::setEditorData(editor, index);
      return;
    }
    line->setText(index.data(Qt::EditRole).toString());
  }

  void setModelData(QWidget *editorWidget, QAbstractItemModel *model,
                    const QModelIndex &index) const override {
    QLineEdit *line = qobject_cast<QLineEdit *>(editorWidget);
    if (!line) {
      QStyledItemDelegate::setModelData(editorWidget, model, index);
      return;
    }

    const QString oldVal = index.data(Qt::EditRole).toString();
    const QString editedText = line->text();
    if (oldVal == editedText)
      return;

    if (!validateTextForType(editedText, typeFunc(index)))
      return;
    QString newVal = canonicalizeForDisplay(editedText, typeFunc(index));
    if (oldVal == newVal)
      return;
    if (undoStack)
      undoStack->push(new SetDataCommand(model, index, oldVal, newVal));
    else
      model->setData(index, newVal);
  }

private:
  TypeFunc typeFunc;
  QUndoStack *undoStack;
  std::function<void()> multiCellPasteHandler;
};

SDDSEditor::SDDSEditor(bool darkPalette, QWidget *parent)
  : QMainWindow(parent), datasetLoaded(false), dirty(false), asciiSave(true),
    currentPage(0), currentFilename(QString()), lastRowAddCount(1),
    lastSearchPattern(QString()), lastReplaceText(QString()),
    lastFillSeriesStart("0"), lastFillSeriesStep("1"),
    lastNumericalExpression("x"), lastTextFormula("${x}"),
    lastRowFilterExpression("X>0 && Status==\"OK\""),
    rowFilterExpression(QString()), rowFilterActive(false),
    undoStack(new QUndoStack(this)), updatingModels(false),
    applyingStructuralUndo(false),
    darkPalette(darkPalette) {
  loadProgressDialog = nullptr;
  loadProgressMin = 0;
  loadProgressMax = 100;

  columnView = nullptr;
  arrayView = nullptr;
  columnHeader = nullptr;
  arrayHeader = nullptr;
  mainToolBar = nullptr;
  pagePrevBtn = nullptr;
  pageNextBtn = nullptr;
  pageCountLabel = nullptr;
  filterAction = nullptr;
  plotAction = nullptr;
  filterChip = nullptr;
  filterChipText = nullptr;
  columnSearchEdit = nullptr;
  modifiedLabel = nullptr;
  pathLabel = nullptr;
  pageStatusLabel = nullptr;
  cellStatusLabel = nullptr;
  rowsStatusLabel = nullptr;
  statusMessageLabel = nullptr;
  undoStatusLabel = nullptr;
  messagesButton = nullptr;
  statusMessageTimer = nullptr;
  unreadMessages = 0;
  visibleColumnRows = 0;
  applyingTheme = false;

  resizeDebounceTimer = new QTimer(this);
  resizeDebounceTimer->setSingleShot(true);
  resizeUpdatesSuspended = false;
  connect(resizeDebounceTimer, &QTimer::timeout, this, [this]() {
    if (!resizeUpdatesSuspended)
      return;
    // Re-enable updates and repaint once resizing has settled.
    if (columnView)
      columnView->setUpdatesEnabled(true);
    if (arrayView)
      arrayView->setUpdatesEnabled(true);
    resizeUpdatesSuspended = false;
    if (columnView)
      columnView->viewport()->update();
    if (arrayView)
      arrayView->viewport()->update();
  });

  // QTableView/QHeaderView may query model headers during construction.
  // Ensure SDDS_DATASET starts in a known-safe state (null pointers, zero counts).
  memset(&dataset, 0, sizeof(dataset));

  // Messages panel: collapsed by default and opened from the status bar.
  consoleEdit = new QPlainTextEdit(this);
  consoleEdit->setReadOnly(true);
  consoleEdit->setFont(preferredTableFont());
  int lineH = consoleEdit->fontMetrics().lineSpacing();
  consoleEdit->setMinimumHeight(lineH * 3 + 2 * consoleEdit->frameWidth());
  consoleDock = new QDockWidget(tr("Messages"), this);
  consoleDock->setObjectName("messagesDock");
  consoleDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
  consoleDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
  consoleDock->setWidget(consoleEdit);
  addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
  consoleDock->hide();

  QWidget *central = new QWidget(this);
  central->setObjectName("centralArea");
  central->setAttribute(Qt::WA_StyledBackground, true);
  QVBoxLayout *mainLayout = new QVBoxLayout(central);
  mainLayout->setContentsMargins(10, 8, 10, 8);

  // Page navigation and save format live in the toolbar (see buildToolBar).
  pageCombo = new QComboBox(this);
  pageCombo->setObjectName("pageCombo");
  pageCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  pageCombo->setToolTip(tr("Current page"));
  connect(pageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SDDSEditor::pageChanged);

  asciiBtn = new QToolButton(this);
  binaryBtn = new QToolButton(this);
  asciiBtn->setText(tr("ASCII"));
  binaryBtn->setText(tr("Binary"));
  asciiBtn->setToolTip(tr("Save as ASCII SDDS"));
  binaryBtn->setToolTip(tr("Save as binary SDDS"));
  QButtonGroup *formatGroup = new QButtonGroup(this);
  formatGroup->setExclusive(true);
  for (QToolButton *button : {asciiBtn, binaryBtn}) {
    button->setCheckable(true);
    button->setAutoRaise(true);
    formatGroup->addButton(button);
  }
  asciiBtn->setChecked(true);
  connect(asciiBtn, &QToolButton::clicked, this, [this]() {
    asciiSave = true;
    if (datasetLoaded)
      markDirty();
  });
  connect(binaryBtn, &QToolButton::clicked, this, [this]() {
    asciiSave = false;
    if (datasetLoaded)
      markDirty();
  });

  QFont tableFont = preferredTableFont();
  const int rowHeight = QFontMetrics(tableFont).height() + 6;

  // container for data panels
  dataSplitter = new PanelSplitter(Qt::Vertical, this);
  dataSplitter->setObjectName("dataSplitter");
  dataSplitter->setHandleWidth(10);
  mainLayout->addWidget(dataSplitter, 1);

  auto makePanelAction = [this](QWidget *parent, const QString &text, int icon,
                                const QString &toolTip) {
    QToolButton *button = new QToolButton(parent);
    button->setObjectName("panelAction");
    button->setText(text);
    button->setToolTip(toolTip);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIconSize(QSize(14, 14));
    button->setAutoRaise(true);
    bindIcon(button, icon, ToneMuted);
    return button;
  };
  // Attribute editors act on the current cell; say so instead of doing nothing.
  auto makeAttributesAction = [this, makePanelAction](DataPanel *panel, const QString &toolTip,
                                                      std::function<QTableView *()> view,
                                                      void (SDDSEditor::*edit)(),
                                                      const QString &hint) {
    QToolButton *button = makePanelAction(panel, tr("Attributes"), IconSliders, toolTip);
    panel->addHeaderWidget(button);
    connect(button, &QToolButton::clicked, this, [this, view, edit, hint]() {
      if (datasetLoaded && !view()->currentIndex().isValid()) {
        message(hint);
        return;
      }
      (this->*edit)();
    });
  };

  // parameters panel
  paramBox = new DataPanel(tr("Parameters"), this);
  paramBox->addHeaderStretch();
  QToolButton *paramInsertBtn = makePanelAction(paramBox, tr("Insert"), IconPlus, tr("Insert a parameter"));
  paramBox->addHeaderWidget(paramInsertBtn);
  connect(paramInsertBtn, &QToolButton::clicked, this, &SDDSEditor::insertParameter);
  makeAttributesAction(paramBox, tr("Edit the current parameter's attributes"),
                       [this]() { return paramView; }, &SDDSEditor::editParameterAttributes,
                       tr("Select a parameter to edit its attributes"));
  paramModel = new ParameterPageModel(&dataset, &pages, &currentPage, this);
  paramView = new SingleClickEditTableView(paramBox);
  paramView->setFont(tableFont);
  paramView->setModel(paramModel);
  paramView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(paramView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this](const QItemSelection &, const QItemSelection &) {
            QSet<int> selected;
            collectSelectedRows(paramView, &selected);
            lastParameterSelectionRows = sortedValidIndexesDescending(
                selected, dataset.layout.n_parameters);
          });
  // A model reset clears the selection without emitting selectionChanged, and
  // the cached rows would then name different (or deleted) parameters.
  connect(paramModel, &QAbstractItemModel::modelReset, this,
          [this]() { lastParameterSelectionRows.clear(); });
  connect(paramModel, &QAbstractItemModel::dataChanged, this,
          [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
            if (!updatingModels)
              markDirty();
          });
  paramView->setItemDelegate(new SDDSItemDelegate(
      [this](const QModelIndex &idx) {
        // Units and descriptions are text; numeric formatting would rewrite "1E3" as "1000".
        return idx.column() == ParameterPageModel::ValueColumn
                   ? dataset.layout.parameter_definition[idx.row()].type
                   : SDDS_STRING;
      },
      undoStack, paramView));
  // Shown as Name | Type | Units | Value | Description.  Only the display order
  // changes; the value stays logical column 0 for editing and clipboard code.
  // Value and description share the width; type and units fit their text.
  QHeaderView *paramHeader = paramView->horizontalHeader();
  paramHeader->setFont(QApplication::font());
  paramHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  paramHeader->moveSection(paramHeader->visualIndex(ParameterPageModel::TypeColumn), 0);
  paramHeader->moveSection(paramHeader->visualIndex(ParameterPageModel::UnitsColumn), 1);
  paramHeader->setSectionResizeMode(ParameterPageModel::ValueColumn, QHeaderView::Stretch);
  paramHeader->setSectionResizeMode(ParameterPageModel::TypeColumn, QHeaderView::ResizeToContents);
  paramHeader->setSectionResizeMode(ParameterPageModel::UnitsColumn, QHeaderView::ResizeToContents);
  paramHeader->setSectionResizeMode(ParameterPageModel::DescriptionColumn, QHeaderView::Stretch);
  paramView->setAlternatingRowColors(true);
  paramView->setShowGrid(false);
  paramView->setWordWrap(false);
  paramView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  paramView->verticalHeader()->setDefaultSectionSize(rowHeight);
  paramView->verticalHeader()->setSectionsMovable(true);
  paramView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(paramView->verticalHeader(), &QHeaderView::sectionDoubleClicked, this,
          &SDDSEditor::changeParameterType);
  connect(paramView->verticalHeader(), &QHeaderView::sectionMoved, this,
          &SDDSEditor::parameterMoved);
  paramView->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  paramView->verticalHeader()->installEventFilter(this);
  connect(paramView->verticalHeader(), &QHeaderView::customContextMenuRequested,
          this, &SDDSEditor::parameterHeaderMenuRequested);
  paramView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(paramView, &QTableView::customContextMenuRequested,
          this, &SDDSEditor::parameterCellMenuRequested);
  // Metadata cells are read-only: keep the value cell current and use
  // double-click to open the matching definition editor.
  connect(paramView->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex &current, const QModelIndex &) {
            if (current.isValid() && current.column() != ParameterPageModel::ValueColumn)
              paramView->selectionModel()->setCurrentIndex(
                  paramModel->index(current.row(), ParameterPageModel::ValueColumn),
                  QItemSelectionModel::ClearAndSelect);
          });
  connect(paramView, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
    if (!index.isValid() || index.column() == ParameterPageModel::ValueColumn)
      return;
    if (index.column() == ParameterPageModel::TypeColumn)
      changeParameterType(index.row());
    else
      editParameterAttributes();
  });
  paramBox->setBody(paramView);
  dataSplitter->addWidget(paramBox);

  // columns panel
  colBox = new DataPanel(tr("Columns"), this);
  filterChip = new QFrame(colBox);
  filterChip->setObjectName("filterChip");
  QHBoxLayout *chipLayout = new QHBoxLayout(filterChip);
  chipLayout->setContentsMargins(6, 0, 2, 0);
  chipLayout->setSpacing(0);
  filterChipText = new QToolButton(filterChip);
  filterChipText->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  filterChipText->setIconSize(QSize(12, 12));
  filterChipText->setToolTip(tr("Edit the row filter"));
  filterChipText->setAutoRaise(true);
  bindIcon(filterChipText, IconFilter, ToneAccent);
  QToolButton *filterChipClear = new QToolButton(filterChip);
  filterChipClear->setIconSize(QSize(11, 11));
  filterChipClear->setToolTip(tr("Clear the row filter"));
  filterChipClear->setAccessibleName(tr("Clear row filter"));
  filterChipClear->setAutoRaise(true);
  bindIcon(filterChipClear, IconClose, ToneAccent);
  chipLayout->addWidget(filterChipText);
  chipLayout->addWidget(filterChipClear);
  filterChip->setFixedHeight(22);
  filterChip->hide();
  connect(filterChipText, &QToolButton::clicked, this, &SDDSEditor::filterColumnRows);
  connect(filterChipClear, &QToolButton::clicked, this, &SDDSEditor::clearColumnRowFilter);
  colBox->addHeaderWidget(filterChip);
  colBox->addHeaderStretch();
  columnSearchEdit = new QLineEdit(colBox);
  columnSearchEdit->setObjectName("panelSearch");
  columnSearchEdit->setClearButtonEnabled(true);
  columnSearchEdit->setFixedWidth(220);
  columnSearchEdit->setPlaceholderText(tr("Find in all columns..."));
  columnSearchEdit->setToolTip(tr("Find text in selected columns, or all columns if none are selected (Enter for next match)"));
  QAction *searchIcon = columnSearchEdit->addAction(QIcon(), QLineEdit::LeadingPosition);
  bindIcon(searchIcon, IconSearch, ToneMuted);
  connect(columnSearchEdit, &QLineEdit::returnPressed, this, &SDDSEditor::findInColumnPanel);
  connect(columnSearchEdit, &QLineEdit::textChanged, this, [this]() { columnSearchMatch = QModelIndex(); });
  colBox->addHeaderWidget(columnSearchEdit);
  QToolButton *colInsertBtn = makePanelAction(colBox, tr("Insert"), IconPlus, tr("Insert a column"));
  colBox->addHeaderWidget(colInsertBtn);
  connect(colInsertBtn, &QToolButton::clicked, this, &SDDSEditor::insertColumn);
  makeAttributesAction(colBox, tr("Edit the current column's attributes"),
                       [this]() { return columnView; }, &SDDSEditor::editColumnAttributes,
                       tr("Select a cell in a column, or right-click its header, to edit its attributes"));
  columnModel = new ColumnPageModel(&dataset, &pages, &currentPage, this);
  columnView = new SingleClickEditTableView(colBox);
  columnHeader = new TwoLineHeaderView(columnView);
  columnHeader->setFont(QApplication::font());
  columnView->setHorizontalHeader(columnHeader);
  columnView->setFont(tableFont);
  columnView->setModel(columnModel);
  columnView->setSelectionBehavior(QAbstractItemView::SelectItems);
  columnView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(columnView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this](const QItemSelection &, const QItemSelection &) {
            QSet<int> selected;
            collectSelectedColumns(columnView, &selected);
            lastColumnSelectionColumns = sortedValidIndexesDescending(
                selected, dataset.layout.n_columns);
          });
  connect(columnModel, &QAbstractItemModel::modelReset, this,
          [this]() { lastColumnSelectionColumns.clear(); });
  // Column names and types affect variable resolution and empty-value semantics.
  connect(columnModel, &QAbstractItemModel::headerDataChanged, this,
          [this]() {
            if (rowFilterActive)
              refreshColumnRowFilter(false);
          });
  connect(columnModel, &QAbstractItemModel::dataChanged, this,
          [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
            if (!updatingModels)
              markDirty();
            if (rowFilterActive)
              refreshColumnRowFilter(false);
          });
  connect(columnModel, &QAbstractItemModel::rowsInserted, this,
          [this](const QModelIndex &, int, int) {
            if (!updatingModels)
              markDirty();
            if (rowFilterActive)
              refreshColumnRowFilter(false);
          });
  connect(columnModel, &QAbstractItemModel::rowsRemoved, this,
          [this](const QModelIndex &, int, int) {
            if (!updatingModels)
              markDirty();
            if (rowFilterActive)
              refreshColumnRowFilter(false);
          });
  columnView->setItemDelegate(new SDDSItemDelegate(
      [this](const QModelIndex &idx) {
        return dataset.layout.column_definition[idx.column()].type;
      },
      undoStack, columnView));
  /*
   * ResizeToContents can scan the model as soon as it is reset.  Keep the
   * header interactive and perform one explicitly bounded sizing pass after
   * load instead.
   */
  columnView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  columnView->horizontalHeader()->setResizeContentsPrecision(200);
  columnView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  columnView->setAlternatingRowColors(true);
  columnView->setWordWrap(false);
  columnView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  columnView->verticalHeader()->setDefaultSectionSize(rowHeight);
  columnView->verticalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
  columnView->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(columnView->verticalHeader(), &QHeaderView::customContextMenuRequested,
          this, &SDDSEditor::columnRowMenuRequested);
  columnView->horizontalHeader()->setSectionsMovable(true);
  columnView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(columnView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeColumnType);
  connect(columnView->horizontalHeader(), &QHeaderView::sectionMoved, this,
          &SDDSEditor::columnMoved);
  columnView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  columnView->horizontalHeader()->installEventFilter(this);
  connect(columnView->horizontalHeader(), &QHeaderView::customContextMenuRequested,
          this, &SDDSEditor::columnHeaderMenuRequested);
  columnView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(columnView, &QTableView::customContextMenuRequested,
          this, &SDDSEditor::columnCellMenuRequested);
  connect(columnView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &SDDSEditor::updateColumnSearchScope);
  connect(columnModel, &QAbstractItemModel::modelReset, this, &SDDSEditor::updateColumnSearchScope);
  colBox->setBody(columnView);
  dataSplitter->addWidget(colBox);

  // arrays panel
  arrayBox = new DataPanel(tr("Arrays"), this);
  arrayBox->addHeaderStretch();
  QToolButton *arrayViewerBtn = makePanelAction(arrayBox, tr("Open in viewer"), IconGrid,
                                                tr("Open the current array in the array viewer"));
  QToolButton *arrayInsertBtn = makePanelAction(arrayBox, tr("Insert"), IconPlus, tr("Insert an array"));
  arrayBox->addHeaderWidget(arrayViewerBtn);
  arrayBox->addHeaderWidget(arrayInsertBtn);
  connect(arrayInsertBtn, &QToolButton::clicked, this, &SDDSEditor::insertArray);
  makeAttributesAction(arrayBox, tr("Edit the current array's attributes"),
                       [this]() { return arrayView; }, &SDDSEditor::editArrayAttributes,
                       tr("Select a cell in an array, or right-click its header, to edit its attributes"));
  arrayModel = new ArrayPageModel(&dataset, &pages, &currentPage, this);
  arrayView = new SingleClickEditTableView(arrayBox);
  arrayHeader = new TwoLineHeaderView(arrayView);
  arrayHeader->setFont(QApplication::font());
  arrayView->setHorizontalHeader(arrayHeader);
  arrayView->setFont(tableFont);
  arrayView->setModel(arrayModel);
  arrayView->setSelectionBehavior(QAbstractItemView::SelectItems);
  arrayView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(arrayView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this](const QItemSelection &, const QItemSelection &) {
            QSet<int> selected;
            collectSelectedColumns(arrayView, &selected);
            lastArraySelectionColumns = sortedValidIndexesDescending(
                selected, dataset.layout.n_arrays);
          });
  connect(arrayModel, &QAbstractItemModel::modelReset, this,
          [this]() { lastArraySelectionColumns.clear(); });
  connect(arrayModel, &QAbstractItemModel::dataChanged, this,
          [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
            if (!updatingModels)
              markDirty();
          });
  connect(arrayModel, &QAbstractItemModel::rowsInserted, this,
          [this](const QModelIndex &, int, int) {
            if (!updatingModels)
              markDirty();
          });
  connect(arrayModel, &QAbstractItemModel::rowsRemoved, this,
          [this](const QModelIndex &, int, int) {
            if (!updatingModels)
              markDirty();
          });
  arrayView->setItemDelegate(new SDDSItemDelegate(
      [this](const QModelIndex &idx) {
        return dataset.layout.array_definition[idx.column()].type;
      },
      undoStack, arrayView));
  arrayView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  arrayView->horizontalHeader()->setResizeContentsPrecision(200);
  arrayView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  arrayView->setAlternatingRowColors(true);
  arrayView->setWordWrap(false);
  arrayView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  arrayView->verticalHeader()->setDefaultSectionSize(rowHeight);
  arrayView->verticalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
  arrayView->horizontalHeader()->setSectionsMovable(true);
  arrayView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(arrayView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeArrayType);
  connect(arrayView->horizontalHeader(), &QHeaderView::sectionMoved, this,
          &SDDSEditor::arrayMoved);
  arrayView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  arrayView->horizontalHeader()->installEventFilter(this);
  connect(arrayView->horizontalHeader(), &QHeaderView::customContextMenuRequested,
          this, &SDDSEditor::arrayHeaderMenuRequested);
  arrayView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(arrayView, &QTableView::customContextMenuRequested,
          this, &SDDSEditor::arrayCellMenuRequested);
  arrayBox->setBody(arrayView);
  dataSplitter->addWidget(arrayBox);

  // Status bar cell readout follows whichever table last moved its cursor.
  for (QTableView *view : {paramView, columnView, arrayView}) {
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this, view](const QModelIndex &, const QModelIndex &) {
              lastCellView = view;
              updateStatusBar();
            });
  }

  // let columns and arrays consume additional space when resizing
  dataSplitter->setStretchFactor(0, 0);
  dataSplitter->setStretchFactor(1, 1);
  dataSplitter->setStretchFactor(2, 1);

  // shortcuts for copy/paste
  QShortcut *copySc = new QShortcut(QKeySequence::Copy, this);
  connect(copySc, &QShortcut::activated, this, &SDDSEditor::copy);
  QShortcut *pasteSc = new QShortcut(QKeySequence::Paste, this);
  connect(pasteSc, &QShortcut::activated, this, &SDDSEditor::paste);
  QShortcut *delSc = new QShortcut(QKeySequence::Delete, this);
  connect(delSc, &QShortcut::activated, this, &SDDSEditor::deleteCells);
#if defined(Q_OS_MACOS)
  // QKeySequence::Delete is forward delete (fn+Delete) on macOS; the key labeled
  // Delete sends Backspace.  Open cell editors still receive it as text editing.
  QShortcut *backspaceSc = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
  connect(backspaceSc, &QShortcut::activated, this, &SDDSEditor::deleteCells);
#endif

  setCentralWidget(central);
  resize(1200, 800);

  // menu bar
  QMenu *fileMenu = menuBar()->addMenu(tr("File"));
  QAction *openAct = fileMenu->addAction(tr("Open"));
  openAct->setShortcut(QKeySequence::Open);
  fileMenu->addSeparator();
  QAction *saveAct = fileMenu->addAction(tr("Save"));
  saveAct->setShortcut(QKeySequence::Save);
  QAction *saveAsAct = fileMenu->addAction(tr("Save as..."));
  saveAsAct->setShortcut(QKeySequence::SaveAs);
  QAction *saveHdfAct = fileMenu->addAction(tr("Export HDF"));
  saveHdfAct->setShortcut(QKeySequence(tr("Ctrl+Shift+H")));
  QAction *csvAct = fileMenu->addAction(tr("Export CSV"));
  csvAct->setShortcut(QKeySequence(tr("Ctrl+Shift+C")));
  fileMenu->addSeparator();
  QAction *restartAct = fileMenu->addAction(tr("Restart"));
  restartAct->setShortcut(QKeySequence(tr("Ctrl+R")));
  QAction *quitAct = fileMenu->addAction(tr("Quit"));
  quitAct->setShortcut(QKeySequence::Quit);
  connect(openAct, &QAction::triggered, this, &SDDSEditor::openFile);
  connect(saveAct, &QAction::triggered, this, &SDDSEditor::saveFile);
  connect(saveAsAct, &QAction::triggered, this, &SDDSEditor::saveFileAs);
  connect(saveHdfAct, &QAction::triggered, this, &SDDSEditor::saveFileAsHDF);
  connect(csvAct, &QAction::triggered, this, &SDDSEditor::exportCSV);
  connect(restartAct, &QAction::triggered, this, &SDDSEditor::restartApp);
  connect(quitAct, &QAction::triggered, this, &QWidget::close);

  QMenu *editMenu = menuBar()->addMenu(tr("Edit"));
  QAction *undoAct = editMenu->addAction(tr("Undo"));
  undoAct->setShortcut(QKeySequence::Undo);
  QAction *redoAct = editMenu->addAction(tr("Redo"));
  redoAct->setShortcut(QKeySequence::Redo);
  connect(undoAct, &QAction::triggered, this, [this]() { flushPendingEdits(); undoStack->undo(); });
  connect(redoAct, &QAction::triggered, this, [this]() { flushPendingEdits(); undoStack->redo(); });
  undoAct->setEnabled(undoStack->canUndo());
  redoAct->setEnabled(undoStack->canRedo());
  connect(undoStack, &QUndoStack::canUndoChanged, undoAct, &QAction::setEnabled);
  connect(undoStack, &QUndoStack::canRedoChanged, redoAct, &QAction::setEnabled);
  connect(undoStack, &QUndoStack::indexChanged, this, [this](int) { updateStatusBar(); });
  editMenu->addSeparator();
  QMenu *paramMenu = editMenu->addMenu(tr("Parameter"));
  QAction *paramAttr = paramMenu->addAction(tr("Attributes"));
  QAction *paramIns = paramMenu->addAction(tr("Insert"));
  QAction *paramDel = paramMenu->addAction(tr("Delete"));
  connect(paramAttr, &QAction::triggered, this,
          &SDDSEditor::editParameterAttributes);
  connect(paramIns, &QAction::triggered, this, &SDDSEditor::insertParameter);
  connect(paramDel, &QAction::triggered, this, &SDDSEditor::deleteParameter);
  QMenu *colMenu = editMenu->addMenu(tr("Column"));
  QAction *colAttr = colMenu->addAction(tr("Attributes"));
  QAction *colIns = colMenu->addAction(tr("Insert"));
  QAction *colDel = colMenu->addAction(tr("Delete"));
  connect(colAttr, &QAction::triggered, this,
          &SDDSEditor::editColumnAttributes);
  connect(colIns, &QAction::triggered, this, &SDDSEditor::insertColumn);
  connect(colDel, &QAction::triggered, this, &SDDSEditor::deleteColumn);
  QMenu *arrayMenu = editMenu->addMenu(tr("Array"));
  QAction *arrayGrid = arrayMenu->addAction(tr("Open Array Viewer..."));
  connect(arrayGrid, &QAction::triggered, this, [this]() {
    int column = arrayView->currentIndex().column();
    if (column < 0 && datasetLoaded && dataset.layout.n_arrays > 0)
      column = 0;
    openArrayViewer(column);
  });
  connect(arrayViewerBtn, &QToolButton::clicked, arrayGrid, &QAction::trigger);
  QAction *arrayAttr = arrayMenu->addAction(tr("Attributes"));
  QAction *arrayIns = arrayMenu->addAction(tr("Insert"));
  QAction *arrayDel = arrayMenu->addAction(tr("Delete"));
  connect(arrayAttr, &QAction::triggered, this,
          &SDDSEditor::editArrayAttributes);
  connect(arrayIns, &QAction::triggered, this, &SDDSEditor::insertArray);
  connect(arrayDel, &QAction::triggered, this, &SDDSEditor::deleteArray);
  editMenu->addSeparator();

  QMenu *columnRowsMenu = editMenu->addMenu(tr("Column Rows"));
  QAction *colRowIns = columnRowsMenu->addAction(tr("Insert"));
  QAction *colRowDel = columnRowsMenu->addAction(tr("Delete"));
  QAction *colRowFilter = columnRowsMenu->addAction(tr("Filter/View..."));
  QAction *colRowClearFilter = columnRowsMenu->addAction(tr("Clear Filter/View"));
  colRowFilter->setShortcut(QKeySequence(tr("Ctrl+Shift+R")));
  connect(colRowIns, &QAction::triggered, this, &SDDSEditor::insertColumnRows);
  connect(colRowDel, &QAction::triggered, this, &SDDSEditor::deleteColumnRows);
  connect(colRowFilter, &QAction::triggered, this, &SDDSEditor::filterColumnRows);
  connect(colRowClearFilter, &QAction::triggered, this, &SDDSEditor::clearColumnRowFilter);

  QMenu *formulaMenu = editMenu->addMenu(tr("Formula / Fill"));
  QAction *fillSeriesAct = formulaMenu->addAction(tr("Fill Series..."));
  QAction *applyExprAct = formulaMenu->addAction(tr("Apply Numerical Expression..."));
  QAction *copyFormulaAct = formulaMenu->addAction(tr("Apply Text Formula..."));
  fillSeriesAct->setShortcut(QKeySequence(tr("Ctrl+Shift+F")));
  applyExprAct->setShortcut(QKeySequence(tr("Ctrl+Shift+E")));
  copyFormulaAct->setShortcut(QKeySequence(tr("Ctrl+Shift+M")));
  connect(fillSeriesAct, &QAction::triggered, this, &SDDSEditor::fillSeriesSelection);
  connect(applyExprAct, &QAction::triggered, this, &SDDSEditor::applyNumericalExpressionSelection);
  connect(copyFormulaAct, &QAction::triggered, this, &SDDSEditor::applyTextFormulaSelection);

  QMenu *pageMenu = editMenu->addMenu(tr("Page"));
  QAction *pageClone = pageMenu->addAction(tr("Insert and clone current page"));
  QAction *pageIns = pageMenu->addAction(tr("Insert"));
  QAction *pageDel = pageMenu->addAction(tr("Delete"));
  connect(pageClone, &QAction::triggered, this, &SDDSEditor::clonePage);
  connect(pageIns, &QAction::triggered, this, &SDDSEditor::insertPage);
  connect(pageDel, &QAction::triggered, this, &SDDSEditor::deletePage);

  buildToolBar(openAct, saveAct, undoAct, redoAct, colRowFilter, arrayGrid);
  buildStatusBar();

  QMenu *viewMenu = menuBar()->addMenu(tr("View"));
  QAction *toolBarAct = mainToolBar->toggleViewAction();
  toolBarAct->setText(tr("Toolbar"));
  viewMenu->addAction(toolBarAct);
  QAction *statusBarAct = viewMenu->addAction(tr("Status Bar"));
  statusBarAct->setCheckable(true);
  statusBarAct->setChecked(true);
  connect(statusBarAct, &QAction::toggled, statusBar(), &QWidget::setVisible);
  QAction *messagesAct = consoleDock->toggleViewAction();
  messagesAct->setText(tr("Messages"));
  messagesAct->setShortcut(QKeySequence(tr("Ctrl+Shift+L")));
  viewMenu->addAction(messagesAct);
  viewMenu->addSeparator();
  for (DataPanel *panel : {paramBox, colBox, arrayBox}) {
    QAction *panelAct = viewMenu->addAction(panel->toggleButton()->text());
    panelAct->setCheckable(true);
    panelAct->setChecked(panel->isChecked());
    connect(panelAct, &QAction::toggled, panel->toggleButton(), &QToolButton::setChecked);
    connect(panel->toggleButton(), &QToolButton::toggled, panelAct, &QAction::setChecked);
  }

  QMenu *infoMenu = menuBar()->addMenu(tr("Info"));
  QAction *aboutAct = infoMenu->addAction(tr("About"));
  QAction *helpAct = infoMenu->addAction(tr("Help"));
  connect(aboutAct, &QAction::triggered, []() {
    QString text =
        QObject::tr("Programmed by Robert Soliday <soliday@anl.gov>\n"
                    "Powered (mostly) by caffeine, stubbornness… and OpenAI Codex.\n\n"
                    "Fun fact: 90% of this code was written by OpenAI Codex, the other 10% was me forcing a square peg into a round hole.\n"
                    "Proceed with caution: may contain puns, dad jokes, and the occasional infinite loop.");
    QMessageBox::about(nullptr, QObject::tr("About"), text);
  });
  connect(helpAct, &QAction::triggered, this, &SDDSEditor::showHelp);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // Follow the desktop's light/dark setting while the editor is running.
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          [this](Qt::ColorScheme scheme) {
            if (scheme != Qt::ColorScheme::Unknown)
              applyTheme(scheme == Qt::ColorScheme::Dark);
          });
#endif
  applyTheme(darkPalette);
  updateFilterIndicator();
  updateWindowTitle();
}

SDDSEditor::~SDDSEditor() {
  clearDataset();
}

void SDDSEditor::bindIcon(QObject *target, int kind, int tone) {
  if (!target)
    return;
  IconBinding binding;
  binding.target = target;
  binding.kind = kind;
  binding.tone = tone;
  iconBindings.append(binding);
}

void SDDSEditor::buildToolBar(QAction *openAct, QAction *saveAct, QAction *undoAct,
                              QAction *redoAct, QAction *filterAct, QAction *arrayViewerAct) {
  mainToolBar = new QToolBar(tr("Main Toolbar"), this);
  mainToolBar->setObjectName("mainToolBar");
  mainToolBar->setMovable(false);
  mainToolBar->setFloatable(false);
  mainToolBar->setIconSize(QSize(16, 16));
  mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  addToolBar(Qt::TopToolBarArea, mainToolBar);

  auto addLabel = [this](const QString &text) {
    QLabel *label = new QLabel(text, mainToolBar);
    label->setObjectName("toolbarLabel");
    mainToolBar->addWidget(label);
    return label;
  };

  openAct->setToolTip(tr("Open an SDDS file (%1)").arg(openAct->shortcut().toString(QKeySequence::NativeText)));
  saveAct->setToolTip(tr("Save (%1)").arg(saveAct->shortcut().toString(QKeySequence::NativeText)));
  undoAct->setToolTip(tr("Undo (%1)").arg(undoAct->shortcut().toString(QKeySequence::NativeText)));
  redoAct->setToolTip(tr("Redo (%1)").arg(redoAct->shortcut().toString(QKeySequence::NativeText)));
  mainToolBar->addAction(openAct);
  mainToolBar->addAction(saveAct);
  mainToolBar->addSeparator();
  mainToolBar->addAction(undoAct);
  mainToolBar->addAction(redoAct);
  for (QAction *action : {undoAct, redoAct}) {
    if (QToolButton *button = qobject_cast<QToolButton *>(mainToolBar->widgetForAction(action)))
      button->setToolButtonStyle(Qt::ToolButtonIconOnly);
  }
  mainToolBar->addSeparator();

  addLabel(tr("Page"));
  pagePrevBtn = new QToolButton(mainToolBar);
  pagePrevBtn->setObjectName("pageNav");
  pagePrevBtn->setToolTip(tr("Previous page"));
  pagePrevBtn->setAccessibleName(tr("Previous page"));
  pagePrevBtn->setAutoRaise(true);
  pageNextBtn = new QToolButton(mainToolBar);
  pageNextBtn->setObjectName("pageNav");
  pageNextBtn->setToolTip(tr("Next page"));
  pageNextBtn->setAccessibleName(tr("Next page"));
  pageNextBtn->setAutoRaise(true);
  bindIcon(pagePrevBtn, IconPrev);
  bindIcon(pageNextBtn, IconNext);
  connect(pagePrevBtn, &QToolButton::clicked, this, [this]() {
    if (pageCombo->currentIndex() > 0)
      pageCombo->setCurrentIndex(pageCombo->currentIndex() - 1);
  });
  connect(pageNextBtn, &QToolButton::clicked, this, [this]() {
    if (pageCombo->currentIndex() + 1 < pageCombo->count())
      pageCombo->setCurrentIndex(pageCombo->currentIndex() + 1);
  });
  mainToolBar->addWidget(pagePrevBtn);
  mainToolBar->addWidget(pageCombo);
  mainToolBar->addWidget(pageNextBtn);
  pageCountLabel = addLabel(QString());
  mainToolBar->addSeparator();

  // Checked while a row filter is active.
  filterAction = new QAction(tr("Filter rows"), this);
  filterAction->setCheckable(true);
  filterAction->setToolTip(tr("Filter the column rows shown (%1)")
                               .arg(filterAct->shortcut().toString(QKeySequence::NativeText)));
  connect(filterAction, &QAction::triggered, this, [this]() {
    filterColumnRows();
    // Triggering toggles the check mark; restore it from the actual state.
    updateFilterIndicator();
  });
  mainToolBar->addAction(filterAction);
  plotAction = new QAction(tr("Plot"), this);
  plotAction->setToolTip(tr("Plot the current column with sddsplot"));
  connect(plotAction, &QAction::triggered, this, [this]() {
    const int column = columnView->currentIndex().column();
    if (column < 0) {
      message(tr("Select a cell in the column to plot"));
      return;
    }
    plotColumn(column);
  });
  mainToolBar->addAction(plotAction);
  arrayViewerAct->setToolTip(tr("Open the current array in the array viewer"));
  QAction *viewerToolAct = new QAction(tr("Array viewer"), this);
  viewerToolAct->setToolTip(arrayViewerAct->toolTip());
  connect(viewerToolAct, &QAction::triggered, arrayViewerAct, &QAction::trigger);
  mainToolBar->addAction(viewerToolAct);

  QWidget *spacer = new QWidget(mainToolBar);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  mainToolBar->addWidget(spacer);
  addLabel(tr("Save as"));
  QFrame *formatSwitch = new QFrame(mainToolBar);
  formatSwitch->setObjectName("formatSwitch");
  QHBoxLayout *formatLayout = new QHBoxLayout(formatSwitch);
  formatLayout->setContentsMargins(2, 2, 2, 2);
  formatLayout->setSpacing(2);
  formatLayout->addWidget(asciiBtn);
  formatLayout->addWidget(binaryBtn);
  mainToolBar->addWidget(formatSwitch);

  bindIcon(openAct, IconOpen);
  bindIcon(saveAct, IconSave);
  bindIcon(undoAct, IconUndo);
  bindIcon(redoAct, IconRedo);
  bindIcon(filterAction, IconFilter);
  bindIcon(plotAction, IconPlot);
  bindIcon(viewerToolAct, IconGrid);
  // Keep menus text-only; the toolbar carries the icons.
  for (QAction *action : {openAct, saveAct, undoAct, redoAct})
    action->setIconVisibleInMenu(false);
}

void SDDSEditor::buildStatusBar() {
  QStatusBar *bar = statusBar();
  bar->setSizeGripEnabled(true);
  auto addLabel = [bar](const QString &name, int stretch = 0) {
    QLabel *label = new QLabel(bar);
    label->setObjectName(name);
    bar->addWidget(label, stretch);
    return label;
  };
  modifiedLabel = addLabel("modifiedLabel");
  pathLabel = addLabel("pathLabel");
  pageStatusLabel = addLabel("pageStatusLabel");
  cellStatusLabel = addLabel("cellStatusLabel");
  rowsStatusLabel = addLabel("rowsStatusLabel");
  statusMessageLabel = addLabel("statusMessageLabel", 1);
  // Long messages must not widen the window; the full text is in the tooltip.
  statusMessageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  undoStatusLabel = new QLabel(bar);
  undoStatusLabel->setObjectName("undoStatusLabel");
  bar->addPermanentWidget(undoStatusLabel);
  messagesButton = new QToolButton(bar);
  messagesButton->setObjectName("messagesButton");
  messagesButton->setCheckable(true);
  messagesButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  messagesButton->setIconSize(QSize(13, 13));
  messagesButton->setToolTip(tr("Show or hide the message log (Ctrl+Shift+L)"));
  bindIcon(messagesButton, IconTerminal);
  bar->addPermanentWidget(messagesButton);
  connect(messagesButton, &QToolButton::toggled, consoleDock, &QWidget::setVisible);
  connect(consoleDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
    // A tabified or minimized dock can report false while still open.
    const bool open = consoleDock->isVisible() || visible;
    if (messagesButton->isChecked() != open) {
      QSignalBlocker blocker(messagesButton);
      messagesButton->setChecked(open);
    }
    if (open)
      unreadMessages = 0;
    updateStatusBar();
  });

  statusMessageTimer = new QTimer(this);
  statusMessageTimer->setSingleShot(true);
  connect(statusMessageTimer, &QTimer::timeout, statusMessageLabel, &QLabel::clear);
}

void SDDSEditor::message(const QString &text) {
  consoleEdit->appendPlainText(text);
  if (statusMessageLabel) {
    statusMessageLabel->setText(text);
    statusMessageLabel->setToolTip(text);
    statusMessageTimer->start(8000);
  }
  if (!consoleDock->isVisible())
    ++unreadMessages;
  updateStatusBar();
}

/** Refresh the toolbar page controls, panel counts and status bar readouts. */
void SDDSEditor::updateStatusBar() {
  if (!modifiedLabel || !paramBox || !colBox || !arrayBox)
    return;
  const QLocale locale;
  const bool loaded = datasetLoaded && !pages.isEmpty();
  const int pageCount = loaded ? pages.size() : 0;
  const int page = loaded ? currentPage : -1;

  pagePrevBtn->setEnabled(loaded && page > 0);
  pageNextBtn->setEnabled(loaded && page + 1 < pageCount);
  pageCombo->setEnabled(loaded);
  pageCountLabel->setText(loaded ? tr("of %1").arg(locale.toString(pageCount)) : QString());

  const int pcount = datasetLoaded ? dataset.layout.n_parameters : 0;
  const int ccount = datasetLoaded ? dataset.layout.n_columns : 0;
  const int acount = datasetLoaded ? dataset.layout.n_arrays : 0;
  const int rows = columnModel->rowCount();
  paramBox->setCountText(locale.toString(pcount));
  colBox->setCountText(ccount > 0 ? tr("%1 %2 %3 rows").arg(locale.toString(ccount), QString(QChar(0x00D7)),
                                                         locale.toString(rows))
                                  : locale.toString(ccount));
  arrayBox->setCountText(locale.toString(acount));

  const bool wasModified = modifiedLabel->property("modified").toBool();
  if (wasModified != dirty || modifiedLabel->text().isEmpty()) {
    modifiedLabel->setProperty("modified", dirty);
    modifiedLabel->setText(dirty ? QString("%1 %2").arg(QChar(0x25CF)).arg(tr("Modified")) : tr("Saved"));
    modifiedLabel->style()->unpolish(modifiedLabel);
    modifiedLabel->style()->polish(modifiedLabel);
  }
  modifiedLabel->setVisible(datasetLoaded);

  const QString path = QDir::toNativeSeparators(currentFilename);
  pathLabel->setToolTip(path);
  pathLabel->setText(path.isEmpty() ? (datasetLoaded ? tr("Untitled") : tr("No file open"))
                                    : pathLabel->fontMetrics().elidedText(path, Qt::ElideMiddle, 360));
  pageStatusLabel->setText(loaded ? tr("Page %1 of %2").arg(locale.toString(page + 1), locale.toString(pageCount))
                                  : QString());
  pageStatusLabel->setVisible(loaded);

  QString cell;
  QTableView *view = lastCellView.data();
  const QModelIndex current = view ? view->currentIndex() : QModelIndex();
  if (loaded && current.isValid()) {
    if (view == paramView && current.row() < pcount)
      cell = tr("Parameter %1").arg(QString::fromLocal8Bit(dataset.layout.parameter_definition[current.row()].name));
    else if (view == columnView && current.column() < ccount)
      cell = tr("Row %1 %2 %3").arg(locale.toString(current.row() + 1), middleDot(),
                                   QString::fromLocal8Bit(dataset.layout.column_definition[current.column()].name));
    else if (view == arrayView && current.column() < acount)
      cell = tr("Element %1 %2 %3").arg(locale.toString(current.row() + 1), middleDot(),
                                       QString::fromLocal8Bit(dataset.layout.array_definition[current.column()].name));
  }
  cellStatusLabel->setText(cell);
  cellStatusLabel->setVisible(!cell.isEmpty());

  if (ccount > 0 && rowFilterActive)
    rowsStatusLabel->setText(tr("%1 of %2 rows").arg(locale.toString(visibleColumnRows), locale.toString(rows)));
  else if (ccount > 0)
    rowsStatusLabel->setText(tr("%1 rows").arg(locale.toString(rows)));
  else
    rowsStatusLabel->clear();
  rowsStatusLabel->setVisible(ccount > 0);

  const QString undoText = undoStack->canUndo() ? undoStack->undoText() : QString();
  undoStatusLabel->setText(undoText.isEmpty() ? QString() : tr("Undo: %1").arg(undoText));
  undoStatusLabel->setVisible(!undoText.isEmpty());

  messagesButton->setText(unreadMessages > 0 ? tr("Messages (%1)").arg(unreadMessages) : tr("Messages"));
}

/** Show the active row filter beside the Columns title and sync the toolbar. */
void SDDSEditor::updateFilterIndicator() {
  if (!filterChip)
    return;
  const bool active = rowFilterActive && !rowFilterExpression.trimmed().isEmpty();
  if (filterAction && filterAction->isChecked() != active)
    filterAction->setChecked(active);
  if (active) {
    const QString expression = filterChipText->fontMetrics().elidedText(
        rowFilterExpression.simplified(), Qt::ElideRight, 260);
    filterChipText->setText(tr("%1 %2 %3 shown").arg(expression, middleDot(),
                                                     QLocale().toString(visibleColumnRows)));
    filterChipText->setToolTip(tr("Row filter: %1\nClick to edit").arg(rowFilterExpression));
  }
  filterChip->setVisible(active);
  updateStatusBar();
}

/** Hide parameter metadata columns that no definition uses. */
void SDDSEditor::updateParameterColumns() {
  bool anyUnits = false;
  bool anyDescription = false;
  if (datasetLoaded) {
    for (int i = 0; i < dataset.layout.n_parameters; ++i) {
      const PARAMETER_DEFINITION &def = dataset.layout.parameter_definition[i];
      anyUnits = anyUnits || (def.units && *def.units);
      anyDescription = anyDescription || (def.description && *def.description);
    }
  }
  paramView->setColumnHidden(ParameterPageModel::UnitsColumn, !anyUnits);
  paramView->setColumnHidden(ParameterPageModel::DescriptionColumn, !anyDescription);
}

/** Capture user-selected columns without letting a search result narrow the scope. */
void SDDSEditor::updateColumnSearchScope() {
  if (selectingColumnSearchMatch)
    return;
  columnSearchMatch = QModelIndex();
  columnSearchColumns.clear();
  QSet<int> selected;
  collectSelectedColumns(columnView, &selected);
  for (int c = 0; c < columnModel->columnCount(); ++c)
    if (selected.contains(c))
      columnSearchColumns.append(c);
  if (columnSearchColumns.size() == 1) {
    const int c = columnSearchColumns.first();
    columnSearchEdit->setPlaceholderText(tr("Find in %1...").arg(QString::fromLocal8Bit(dataset.layout.column_definition[c].name)));
  } else {
    columnSearchEdit->setPlaceholderText(columnSearchColumns.isEmpty() ? tr("Find in all columns...") : tr("Find in selected columns..."));
  }
}

/** Select successive visible matches in row order, wrapping within the search scope. */
void SDDSEditor::findInColumnPanel() {
  const QString needle = columnSearchEdit->text();
  if (needle.isEmpty() || !datasetLoaded)
    return;
  const int rows = columnModel->rowCount();
  if (rows <= 0 || columnModel->columnCount() <= 0)
    return;
  flushPendingEdits();
  QVector<int> columns = columnSearchColumns;
  if (columns.isEmpty())
    for (int c = 0; c < columnModel->columnCount(); ++c)
      columns.append(c);
  const int startRow = columnSearchMatch.isValid() ? columnSearchMatch.row() : 0;
  const int startColumn = columnSearchMatch.isValid() ? columns.indexOf(columnSearchMatch.column()) : -1;
  // Visit the remainder of the starting row after wrapping, without multiplying counts.
  for (qint64 offset = 0; offset <= rows; ++offset) {
    const int row = (static_cast<qint64>(startRow) + offset) % rows;
    if (columnView->isRowHidden(row))
      continue;
    const int first = offset == 0 ? startColumn + 1 : 0;
    const int end = offset == rows ? startColumn + 1 : columns.size();
    for (int c = first; c < end; ++c) {
      const int column = columns[c];
      if (columnView->isColumnHidden(column))
        continue;
      const QModelIndex idx = columnModel->index(row, column);
      if (idx.data(Qt::DisplayRole).toString().contains(needle, Qt::CaseInsensitive)) {
        selectingColumnSearchMatch = true;
        columnView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
        selectingColumnSearchMatch = false;
        columnSearchMatch = idx;
        columnView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        lastCellView = columnView;
        updateStatusBar();
        return;
      }
    }
  }
  message(tr("\"%1\" not found in the searched columns").arg(needle));
}

void SDDSEditor::markDirty() {
  if (updatingModels)
    return;
  dirty = true;
  updateWindowTitle();
}

bool SDDSEditor::maybeSave() {
  commitModels();
  if (!dirty)
    return true;
  QMessageBox::StandardButton ret = QMessageBox::warning(
      this, tr("SDDS"),
      tr("The document has been modified.\nDo you want to save your changes?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (ret == QMessageBox::Save) {
    saveFile();
    return !dirty;
  }
  return ret != QMessageBox::Cancel;
}

void SDDSEditor::updateWindowTitle() {
  QString title = tr("SDDS Editor");
  if (!currentFilename.isEmpty())
    title = QFileInfo(currentFilename).fileName() + " - " + title;
  if (dirty)
    title += " *";
  setWindowTitle(title);
  updateStatusBar();
}

void SDDSEditor::closeEvent(QCloseEvent *event) {
  if (!maybeSave()) {
    event->ignore();
    return;
  }
  // The search dialog has no parent window, so it would otherwise stay open,
  // and keep the application running, after the editor window closes.
  if (searchColumnDialog)
    searchColumnDialog->close();
  const QVector<QPointer<QDialog>> viewers = arrayViewers;
  for (const QPointer<QDialog> &viewer : viewers)
    if (viewer)
      viewer->close();
  event->accept();
}

QTableView *SDDSEditor::focusedTable() const {
  QWidget *w = QApplication::focusWidget();
  if (w && (w == paramView || paramView->isAncestorOf(w)))
    return paramView;
  if (w && (w == columnView || columnView->isAncestorOf(w)))
    return columnView;
  if (w && (w == arrayView || arrayView->isAncestorOf(w)))
    return arrayView;
  return nullptr;
}

bool SDDSEditor::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() == Qt::RightButton) {
      if (watched == paramView->verticalHeader()) {
        pendingParameterHeaderRows.clear();
        int row = paramView->verticalHeader()->logicalIndexAt(mouse->pos());
        QSet<int> selected;
        collectSelectedRows(paramView, &selected);
        QVector<int> rows = sortedValidIndexesDescending(
            selected, dataset.layout.n_parameters);
        if (row >= 0) {
          if (!lastParameterSelectionRows.isEmpty() &&
              lastParameterSelectionRows.contains(row))
            rows = lastParameterSelectionRows;
          if (rows.contains(row))
            pendingParameterHeaderRows = rows;
        }
      } else if (watched == columnView->horizontalHeader()) {
        pendingColumnHeaderColumns.clear();
        int column = columnView->horizontalHeader()->logicalIndexAt(mouse->pos());
        QSet<int> selected;
        collectSelectedColumns(columnView, &selected);
        QVector<int> columns = sortedValidIndexesDescending(
            selected, dataset.layout.n_columns);
        if (column >= 0) {
          if (!lastColumnSelectionColumns.isEmpty() &&
              lastColumnSelectionColumns.contains(column))
            columns = lastColumnSelectionColumns;
          if (columns.contains(column))
            pendingColumnHeaderColumns = columns;
        }
      } else if (watched == arrayView->horizontalHeader()) {
        pendingArrayHeaderColumns.clear();
        int column = arrayView->horizontalHeader()->logicalIndexAt(mouse->pos());
        QSet<int> selected;
        collectSelectedColumns(arrayView, &selected);
        QVector<int> arrays = sortedValidIndexesDescending(
            selected, dataset.layout.n_arrays);
        if (column >= 0) {
          if (!lastArraySelectionColumns.isEmpty() &&
              lastArraySelectionColumns.contains(column))
            arrays = lastArraySelectionColumns;
          if (arrays.contains(column))
            pendingArrayHeaderColumns = arrays;
        }
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void SDDSEditor::parameterMoved(int, int, int) {
  if (!datasetLoaded)
    return;
  if (applyingStructuralUndo)
    return;
  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  QHeaderView *vh = paramView->verticalHeader();
  int count = dataset.layout.n_parameters;
  QVector<int> order(count);
  for (int i = 0; i < count; ++i)
    order[i] = vh->logicalIndex(i);
  QVector<int> oldToNew(count);
  for (int i = 0; i < count; ++i)
    oldToNew[order[i]] = i;
  PARAMETER_DEFINITION *oldDefs = dataset.layout.parameter_definition;
  PARAMETER_DEFINITION *newDefs =
      (PARAMETER_DEFINITION *)malloc(sizeof(PARAMETER_DEFINITION) * count);
  if (!newDefs) {
    QMessageBox::warning(this, tr("SDDS"), tr("Out of memory while reordering parameters"));
    return;
  }
  for (int i = 0; i < count; ++i)
    newDefs[i] = oldDefs[order[i]];
  free(oldDefs);
  dataset.layout.parameter_definition = newDefs;
  for (int i = 0; i < count; ++i)
    dataset.layout.parameter_index[i]->index =
        oldToNew[dataset.layout.parameter_index[i]->index];
  for (PageStore &pd : pages) {
    QVector<QString> newParams(count);
    for (int i = 0; i < count; ++i)
      if (order[i] < pd.parameters.size())
        newParams[i] = pd.parameters[order[i]];
    pd.parameters = newParams;
  }
  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Failed to save reordered parameter layout"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    restoreStructuralSnapshot(this, before);
    return;
  }
  populateModels();

  // Keep the header visual order in sync with the reordered model.
  vh->blockSignals(true);
  for (int logical = 0; logical < count; ++logical) {
    int visual = vh->visualIndex(logical);
    if (visual != logical)
      vh->moveSection(visual, logical);
  }
  vh->blockSignals(false);

  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Reorder Parameters"));
}

void SDDSEditor::columnMoved(int, int, int) {
  if (!datasetLoaded)
    return;
  if (applyingStructuralUndo)
    return;
  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  QHeaderView *hh = columnView->horizontalHeader();
  int count = dataset.layout.n_columns;
  QVector<int> order(count);
  for (int i = 0; i < count; ++i)
    order[i] = hh->logicalIndex(i);
  QVector<int> oldToNew(count);
  for (int i = 0; i < count; ++i)
    oldToNew[order[i]] = i;
  COLUMN_DEFINITION *oldDefs = dataset.layout.column_definition;
  COLUMN_DEFINITION *newDefs =
      (COLUMN_DEFINITION *)malloc(sizeof(COLUMN_DEFINITION) * count);
  if (!newDefs) {
    QMessageBox::warning(this, tr("SDDS"), tr("Out of memory while reordering columns"));
    return;
  }
  for (int i = 0; i < count; ++i)
    newDefs[i] = oldDefs[order[i]];
  free(oldDefs);
  dataset.layout.column_definition = newDefs;
  for (int i = 0; i < count; ++i)
    dataset.layout.column_index[i]->index =
        oldToNew[dataset.layout.column_index[i]->index];
  for (PageStore &pd : pages) {
    QVector<QVector<QString>> newCols(count);
    for (int i = 0; i < count; ++i)
      if (order[i] < pd.columns.size())
        newCols[i] = pd.columns[order[i]];
    pd.columns = newCols;
  }
  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Failed to save reordered column layout"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    restoreStructuralSnapshot(this, before);
    return;
  }
  populateModels();

  // Keep the header visual order in sync with the reordered model.
  hh->blockSignals(true);
  for (int logical = 0; logical < count; ++logical) {
    int visual = hh->visualIndex(logical);
    if (visual != logical)
      hh->moveSection(visual, logical);
  }
  hh->blockSignals(false);

  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Reorder Columns"));
}

void SDDSEditor::arrayMoved(int, int, int) {
  if (!datasetLoaded)
    return;
  if (applyingStructuralUndo)
    return;
  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  QHeaderView *hh = arrayView->horizontalHeader();
  int count = dataset.layout.n_arrays;
  QVector<int> order(count);
  for (int i = 0; i < count; ++i)
    order[i] = hh->logicalIndex(i);
  QVector<int> oldToNew(count);
  for (int i = 0; i < count; ++i)
    oldToNew[order[i]] = i;
  ARRAY_DEFINITION *oldDefs = dataset.layout.array_definition;
  ARRAY_DEFINITION *newDefs =
      (ARRAY_DEFINITION *)malloc(sizeof(ARRAY_DEFINITION) * count);
  if (!newDefs) {
    QMessageBox::warning(this, tr("SDDS"), tr("Out of memory while reordering arrays"));
    return;
  }
  for (int i = 0; i < count; ++i)
    newDefs[i] = oldDefs[order[i]];
  free(oldDefs);
  dataset.layout.array_definition = newDefs;
  for (int i = 0; i < count; ++i)
    dataset.layout.array_index[i]->index =
        oldToNew[dataset.layout.array_index[i]->index];
  for (PageStore &pd : pages) {
    QVector<ArrayStore> newArr(count);
    for (int i = 0; i < count; ++i)
      if (order[i] < pd.arrays.size())
        newArr[i] = pd.arrays[order[i]];
    pd.arrays = newArr;
  }
  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Failed to save reordered array layout"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    restoreStructuralSnapshot(this, before);
    return;
  }
  populateModels();

  // Keep the header visual order in sync with the reordered model.
  hh->blockSignals(true);
  for (int logical = 0; logical < count; ++logical) {
    int visual = hh->visualIndex(logical);
    if (visual != logical)
      hh->moveSection(visual, logical);
  }
  hh->blockSignals(false);

  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Reorder Arrays"));
}

void SDDSEditor::copy() {
  QTableView *view = focusedTable();
  if (!view)
    return;
  flushPendingEdits();
  QModelIndexList indexes = visibleSelectedIndexes(view);
  if (indexes.isEmpty())
    return;

  int minRow = std::numeric_limits<int>::max();
  int maxRow = std::numeric_limits<int>::min();
  int minCol = std::numeric_limits<int>::max();
  int maxCol = std::numeric_limits<int>::min();
  QSet<quint64> selected;
  selected.reserve(indexes.size());

  auto keyFor = [](int r, int c) -> quint64 {
    return (static_cast<quint64>(static_cast<uint32_t>(r)) << 32) |
           static_cast<uint32_t>(c);
  };

  for (const QModelIndex &idx : indexes) {
    if (!idx.isValid())
      continue;
    minRow = std::min(minRow, idx.row());
    maxRow = std::max(maxRow, idx.row());
    minCol = std::min(minCol, idx.column());
    maxCol = std::max(maxCol, idx.column());
    selected.insert(keyFor(idx.row(), idx.column()));
  }

  if (selected.isEmpty())
    return;

  /*
   * Plain text fills unselected cells inside the bounding rectangle with empty
   * fields.  The private format marks those gaps as null so a paste inside the
   * editor leaves the matching target cells unchanged.  Rows hidden by the row
   * filter are left out entirely, matching what the user sees.
   */
  QStringList rowTexts;
  QJsonArray cellRows;
  for (int r = minRow; r <= maxRow; ++r) {
    if (view->isRowHidden(r))
      continue;
    QStringList cols;
    QJsonArray rowCells;
    cols.reserve(maxCol - minCol + 1);
    for (int c = minCol; c <= maxCol; ++c) {
      if (selected.contains(keyFor(r, c))) {
        QModelIndex idx = view->model()->index(r, c);
        const QString value = idx.isValid() ? idx.data().toString() : QString();
        cols << value;
        rowCells.append(value);
      } else {
        cols << QString();
        rowCells.append(QJsonValue());
      }
    }
    rowTexts << cols.join('\t');
    cellRows.append(rowCells);
  }
  QMimeData *mime = new QMimeData;
  mime->setText(rowTexts.join('\n'));
  mime->setData(editorCellsMime, QJsonDocument(cellRows).toJson(QJsonDocument::Compact));
  QApplication::clipboard()->setMimeData(mime);
}

void SDDSEditor::paste() {
  QTableView *view = focusedTable();
  if (!view)
    return;
  flushPendingEdits();
  QModelIndex start = view->currentIndex();
  if (!start.isValid())
    return;
  QString text = QApplication::clipboard()->text();
  // Cells copied from a non-contiguous selection carry null gaps to skip.
  QVector<QStringList> rows;
  QVector<QVector<bool>> gaps;
  const QMimeData *mime = QApplication::clipboard()->mimeData();
  if (mime && mime->hasFormat(editorCellsMime) && mime->text() == text) {
    const QJsonArray cellRows = QJsonDocument::fromJson(mime->data(editorCellsMime)).array();
    for (const QJsonValue &rowValue : cellRows) {
      QStringList cols;
      QVector<bool> rowGaps;
      for (const QJsonValue &cell : rowValue.toArray()) {
        cols << cell.toString();
        rowGaps << cell.isNull();
      }
      rows << cols;
      gaps << rowGaps;
    }
  } else {
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    QStringList lines = text.split('\n');
    if (lines.size() > 1 && lines.last().isEmpty())
      lines.removeLast();
    if (lines.isEmpty())
      lines << QString();
    for (const QString &line : lines) {
      rows << line.split('\t');
      gaps << QVector<bool>(rows.last().size(), false);
    }
  }
  if (rows.isEmpty())
    return;
  bool multiPaste = rows.size() > 1 || rows[0].size() > 1;
  bool changed = false;
  bool warned = false;
  bool macroStarted = false;
  QAbstractItemModel *model = view->model();
  // Fill successive visible rows; rows hidden by the row filter are skipped.
  QVector<int> targetRows;
  for (int r = 0, row = start.row(); r < rows.size(); ++r, ++row) {
    while (row < model->rowCount() && view->isRowHidden(row))
      ++row;
    targetRows << row;
  }
  for (int r = 0; r < rows.size(); ++r) {
    const QStringList &cols = rows[r];
    for (int c = 0; c < cols.size(); ++c) {
      if (gaps[r][c])
        continue;
      QModelIndex idx = model->index(targetRows[r], start.column() + c);
      // Parameter metadata and cells past an array's end are read-only; do not
      // warn that text which will never be stored is invalid for the type.
      if (!idx.isValid() || !(model->flags(idx) & Qt::ItemIsEditable))
        continue;
      int type = SDDS_STRING;
      if (view == paramView)
        type = dataset.layout.parameter_definition[idx.row()].type;
      else if (view == columnView)
        type = dataset.layout.column_definition[idx.column()].type;
      else if (view == arrayView)
        type = dataset.layout.array_definition[idx.column()].type;
      bool show = multiPaste ? !warned : true;
      bool valid = validateTextForType(cols[c], type, show);
      if (valid) {
        if (idx.data(Qt::EditRole).toString() == cols[c])
          continue;
        if (undoStack && !macroStarted) {
          undoStack->beginMacro(tr("Paste"));
          macroStarted = true;
        }
        if (applyCellEditWithUndo(undoStack, model, idx, cols[c]))
          changed = true;
      }
      if (!valid && show)
        warned = true;
    }
  }
  if (macroStarted)
    undoStack->endMacro();
  if (changed)
    markDirty();
}

void SDDSEditor::deleteCells() {
  QTableView *view = focusedTable();
  if (!view)
    return;
  flushPendingEdits();
  QModelIndexList indexes = editableSelectedIndexes(view);
  if (indexes.isEmpty())
    return;
  bool macroStarted = false;
  bool changed = false;
  QAbstractItemModel *model = view->model();
  for (const QModelIndex &idx : indexes) {
    if (!idx.isValid())
      continue;
    if (idx.data(Qt::EditRole).toString().isEmpty())
      continue;
    if (undoStack && !macroStarted) {
      undoStack->beginMacro(tr("Delete Cells"));
      macroStarted = true;
    }
    changed = applyCellEditWithUndo(undoStack, model, idx, QString()) || changed;
  }
  if (macroStarted)
    undoStack->endMacro();
  if (changed)
    markDirty();
}

void SDDSEditor::openFile() {
  if (!maybeSave())
    return;
  QString path = QFileDialog::getOpenFileName(this, tr("Open SDDS"), QString(),
                                             tr("SDDS Files (*.sdds *.sdds.xz *.sdds.gz);;All Files (*)"));
  if (path.isEmpty())
    return;
  loadFile(path);
}

/** Partial initializations own definition strings through the working layout until saved. */
static bool terminateIncompleteDataset(SDDS_DATASET *partial) {
  SDDS_DeferSavingLayout(partial, 0);
  if (!SDDS_SaveLayout(partial))
    return false;
  return SDDS_Terminate(partial);
}

bool SDDSEditor::loadFile(const QString &path) {
  if (!localEncodingPreserves(path)) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Cannot open %1: the path contains characters that cannot be represented in this system's character encoding")
                             .arg(QDir::toNativeSeparators(path)));
    return false;
  }
  SDDS_DATASET in;
  memset(&in, 0, sizeof(in));
  if (!SDDS_InitializeInput(&in,
                            const_cast<char *>(path.toLocal8Bit().constData()))) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to open file"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    terminateIncompleteDataset(&in);
    return false;
  }

  QProgressDialog progress(this);
  progress.setWindowTitle(tr("SDDS"));
  progress.setLabelText(tr("Loading %1…").arg(QFileInfo(path).fileName()));
  progress.setRange(0, 100);
  progress.setValue(0);
  progress.setCancelButton(nullptr);
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setMinimumDuration(0);
  progress.show();
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // During the SDDS file read/copy, we only consume the first ~25% so that
  // the remaining time (Qt model/view setup) can advance progress meaningfully.
  auto setReadProgress = [&](int percent0to100) {
    int p = std::min(99, std::max(0, percent0to100));
    int mapped = (p * 25) / 99;
    mapped = std::min(25, std::max(0, mapped));
    if (mapped != progress.value()) {
      progress.setValue(mapped);
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
  };

  QVector<PageStore> newPages;
  int pageIndex = 0;
  int32_t readResult = 0;
  int64_t undecodableValues = 0;
  auto countUndecodable = [&undecodableValues](const void *data, int64_t count) {
    for (int64_t i = 0; data && i < count; ++i)
      undecodableValues += localTextLosesBytes(static_cast<char *const *>(data)[i]);
  };
  while ((readResult = SDDS_ReadPage(&in)) > 0) {
    ++pageIndex;
    progress.setLabelText(tr("Loading %1 (page %2)…").arg(QFileInfo(path).fileName()).arg(pageIndex));
    progress.setValue(0);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    PageStore pd;
    const int32_t pcount = in.layout.n_parameters;
    const int32_t ccount = in.layout.n_columns;
    const int32_t acount = in.layout.n_arrays;
    const int64_t rows = SDDS_RowCount(&in);
    if (rows < 0 || rows > std::numeric_limits<int>::max()) {
      QMessageBox::warning(this, tr("SDDS"),
                           tr("Page %1 has too many rows for the editor").arg(pageIndex));
      SDDS_Terminate(&in);
      return false;
    }

    int64_t totalUnits = pcount;
    if (ccount > 0 && rows > 0) {
      if (rows > (std::numeric_limits<int64_t>::max() - totalUnits) / ccount)
        totalUnits = std::numeric_limits<int64_t>::max();
      else
        totalUnits += static_cast<int64_t>(ccount) * rows;
    }
    for (int32_t a = 0; a < acount; ++a) {
      const int elements = in.array ? std::max(0, in.array[a].elements) : 0;
      if (elements > std::numeric_limits<int64_t>::max() - totalUnits) {
        totalUnits = std::numeric_limits<int64_t>::max();
        break;
      }
      totalUnits += elements;
    }

    int64_t doneUnits = 0;
    const int64_t progressStride = 16384;
    auto updateConvertedProgress = [&](int64_t completed) {
      if (totalUnits <= 0)
        return;
      const int percent = static_cast<int>(
          (static_cast<long double>(completed) * 100.0L) / totalUnits);
      setReadProgress(percent);
    };

    pd.parameters.resize(pcount);
    for (int32_t i = 0; i < pcount; ++i) {
      pd.parameters[i] = sddsValueToString(
          in.parameter ? in.parameter[i] : nullptr, 0,
          in.layout.parameter_definition[i].type);
      if (in.layout.parameter_definition[i].type == SDDS_STRING)
        countUndecodable(in.parameter ? in.parameter[i] : nullptr, 1);
      ++doneUnits;
      updateConvertedProgress(doneUnits);
    }

    pd.columns.resize(ccount);
    for (int32_t c = 0; c < ccount; ++c) {
      const void *data = in.data ? in.data[c] : nullptr;
      const int32_t type = in.layout.column_definition[c].type;
      pd.columns[c].resize(static_cast<int>(rows));
      for (int64_t r = 0; r < rows; ++r) {
        pd.columns[c][static_cast<int>(r)] = sddsValueToString(data, r, type);
        if (((r + 1) & (progressStride - 1)) == 0)
          updateConvertedProgress(doneUnits + r + 1);
      }
      if (type == SDDS_STRING)
        countUndecodable(data, rows);
      doneUnits += rows;
      updateConvertedProgress(doneUnits);
    }

    pd.arrays.resize(acount);
    for (int32_t a = 0; a < acount; ++a) {
      const ARRAY_DEFINITION &adef = in.layout.array_definition[a];
      const SDDS_ARRAY *source = in.array ? &in.array[a] : nullptr;
      const int dimsCount = std::max(0, adef.dimensions);
      pd.arrays[a].dims.resize(dimsCount);
      for (int d = 0; d < dimsCount; ++d)
        pd.arrays[a].dims[d] = source && source->dimension
                                   ? source->dimension[d]
                                   : 0;

      const int valueCount = source ? std::max(0, source->elements) : 0;
      pd.arrays[a].values.resize(valueCount);
      for (int i = 0; i < valueCount; ++i) {
        pd.arrays[a].values[i] = sddsValueToString(
            source ? source->data : nullptr, i, adef.type);
        if (((static_cast<int64_t>(i) + 1) & (progressStride - 1)) == 0)
          updateConvertedProgress(doneUnits + i + 1);
      }
      if (adef.type == SDDS_STRING)
        countUndecodable(source ? source->data : nullptr, valueCount);
      doneUnits += valueCount;
      updateConvertedProgress(doneUnits);
    }
    newPages.append(std::move(pd));

    // End-of-page: treat SDDS read/copy as 25% complete.
    setReadProgress(99);
  }

  if (readResult == 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed while reading file"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&in);
    return false;
  }
 
  if (newPages.isEmpty()) {
    QMessageBox::warning(this, tr("SDDS"), tr("File contains no pages"));
    SDDS_Terminate(&in);
    return false;
  }

  // Copy layout information for later editing and close the file
  SDDS_DATASET newDataset;
  memset(&newDataset, 0, sizeof(newDataset));
  if (!SDDS_InitializeCopy(&newDataset, &in, NULL, (char *)"m")) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to copy layout"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&in);
    return false;
  }
  SDDS_Terminate(&in);

  clearDataset();
  dataset = newDataset;
  datasetLoaded = true;
  pages = std::move(newPages);

  // Update radio buttons to reflect the file's storage mode
  asciiSave = dataset.layout.data_mode.mode == SDDS_ASCII;
  asciiBtn->setChecked(asciiSave);
  binaryBtn->setChecked(!asciiSave);

  // At this point the file is in memory; now we populate Qt models/views.
  // This can take noticeable time for large datasets, so keep the indicator visible.
  progress.setLabelText(tr("Preparing display…"));
  progress.setValue(25);
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  loadProgressDialog = &progress;
  loadProgressMin = 25;
  loadProgressMax = 99;

  pageCombo->blockSignals(true);
  pageCombo->clear();
  QStringList pageLabels;
  pageLabels.reserve(pages.size());
  for (int i = 0; i < pages.size(); ++i)
    pageLabels.append(tr("Page %1").arg(i + 1));
  pageCombo->addItems(pageLabels);
  pageCombo->setCurrentIndex(0);
  pageCombo->blockSignals(false);
  currentPage = 0;
  loadPage(1);
  currentFilename = path;
  dirty = false;
  message(tr("Loaded %1").arg(path));
  updateWindowTitle();

  progress.setValue(100);
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  loadProgressDialog = nullptr;
  progress.close();
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  if (undecodableValues > 0) {
    const QString warning =
        tr("%1 text value(s) contain bytes that are not valid in this system's character encoding. "
           "They are shown with replacement characters, and saving the file will store those "
           "characters instead of the original bytes.")
            .arg(undecodableValues);
    message(warning);
    QMessageBox::warning(this, tr("SDDS"), warning);
  }
  return true;
}

/*
 * Point the symbolic link at linkPath to target by creating a new link at
 * stagingPath and renaming it over the old one.  On Windows QFile::link makes
 * a .lnk shortcut, which SDDS cannot read, and rename() refuses to replace an
 * existing file, so use the native calls there.
 */
static bool replaceSymbolicLink(const QString &linkPath, const QString &target,
                                const QString &stagingPath) {
#if defined(_WIN32)
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
  const std::wstring staging = QDir::toNativeSeparators(stagingPath).toStdWString();
  const std::wstring destination = QDir::toNativeSeparators(target).toStdWString();
  const std::wstring link = QDir::toNativeSeparators(linkPath).toStdWString();
  // Developer Mode allows unprivileged links; Windows before 10.0.14972 rejects the flag.
  bool created = CreateSymbolicLinkW(staging.c_str(), destination.c_str(),
                                     SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
  if (!created && GetLastError() == ERROR_INVALID_PARAMETER)
    created = CreateSymbolicLinkW(staging.c_str(), destination.c_str(), 0) != 0;
  return created && MoveFileExW(staging.c_str(), link.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
  return QFile::link(target, stagingPath) &&
         std::rename(QFile::encodeName(stagingPath).constData(),
                     QFile::encodeName(linkPath).constData()) == 0;
#endif
}

bool SDDSEditor::writeFile(const QString &path) {
  if (!datasetLoaded)
    return false;
  commitModels();

  QFileInfo fi(path);
  QString finalPath = fi.isSymLink() ? fi.symLinkTarget() : fi.absoluteFilePath();
  bool updateSymlink = false;
  if (fi.isSymLink()) {
    QRegularExpression re("(.*?)([.-])(\\d+)$");
    QRegularExpressionMatch match = re.match(finalPath);
    if (match.hasMatch()) {
      bool ok = false;
      qulonglong version = match.captured(3).toULongLong(&ok);
      if (!ok) {
        QMessageBox::warning(this, tr("SDDS"), tr("Invalid version number in symlink target"));
        return false;
      }
      do {
        if (version == std::numeric_limits<qulonglong>::max()) {
          QMessageBox::warning(this, tr("SDDS"), tr("No available version number"));
          return false;
        }
        finalPath = match.captured(1) + match.captured(2) +
                    QString::number(++version).rightJustified(match.captured(3).size(), '0');
      } while (QFileInfo::exists(finalPath) || QFileInfo(finalPath).isSymLink());
      updateSymlink = true;
    }
  }

  if (SDDS_FileIsLocked(QFile::encodeName(finalPath).constData())) {
    QMessageBox::warning(this, tr("SDDS"), tr("Output file is locked"));
    return false;
  }

  // Keep compression suffixes and stage on the destination filesystem.
  QTemporaryDir staging(QFileInfo(finalPath).absolutePath() + "/.sddseditor-XXXXXX");
  if (!staging.isValid()) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to create staging directory"));
    return false;
  }
  const QString stagedPath = staging.filePath(QFileInfo(finalPath).fileName());
  if (!writeDatasetFile(stagedPath))
    return false;

  if (updateSymlink) {
    // Exclusive creation also protects a version created after our scan.
    QFile input(stagedPath);
    QFile version(finalPath);
    if (!input.open(QIODevice::ReadOnly) ||
        !version.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
      QMessageBox::warning(this, tr("SDDS"), tr("Failed to create new file version"));
      return false;
    }
    bool copied = true;
    while (!input.atEnd()) {
      QByteArray chunk = input.read(1024 * 1024);
      if (input.error() != QFileDevice::NoError || version.write(chunk) != chunk.size()) {
        copied = false;
        break;
      }
    }
    copied = version.flush() && copied;
    version.close();
    if (!copied || version.error() != QFileDevice::NoError) {
      version.remove();
      QMessageBox::warning(this, tr("SDDS"), tr("Failed to write new file version"));
      return false;
    }
    // Rename the replacement link over the old one; never remove it first.
    QTemporaryDir links(fi.absolutePath() + "/.sddseditor-link-XXXXXX");
    const QString linkPath = links.filePath("link");
    if (!links.isValid() || !replaceSymbolicLink(fi.absoluteFilePath(), finalPath, linkPath)) {
      QMessageBox::warning(this, tr("SDDS"),
                           tr("Saved %1, but could not update the original symlink").arg(finalPath));
      return false;
    }
  } else {
    QFile input(stagedPath);
    QSaveFile output(finalPath);
    output.setDirectWriteFallback(false);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
      QMessageBox::warning(this, tr("SDDS"), tr("Failed to prepare output replacement"));
      return false;
    }
    while (!input.atEnd()) {
      QByteArray chunk = input.read(1024 * 1024);
      if (input.error() != QFileDevice::NoError || output.write(chunk) != chunk.size()) {
        QMessageBox::warning(this, tr("SDDS"), tr("Failed while copying staged output"));
        return false;
      }
    }
    if (!output.commit()) {
      QMessageBox::warning(this, tr("SDDS"), tr("Failed to replace output file"));
      return false;
    }
  }
  dirty = false;
  updateWindowTitle();
  message(tr("Saved %1").arg(finalPath));
  return true;
}

bool SDDSEditor::writeDatasetFile(const QString &path) {
  if (!datasetLoaded)
    return false;
  commitModels();
  QString errorText;
  for (int pg = 0; pg < pages.size(); ++pg) {
    if (!validatePageForWrite(dataset.layout, pages[pg], pg, &errorText)) {
      QMessageBox::warning(this, tr("SDDS"), errorText);
      return false;
    }
  }

  auto validFormat = [this](const char *name, const char *format, int type) {
    if (!format || SDDS_VerifyPrintfFormat(format, type))
      return true;
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Invalid format for '%1'").arg(QString::fromLocal8Bit(name)));
    return false;
  };
  for (int i = 0; i < dataset.layout.n_parameters; ++i) {
    const auto &def = dataset.layout.parameter_definition[i];
    if (!validFormat(def.name, def.format_string, def.type))
      return false;
  }
  for (int i = 0; i < dataset.layout.n_columns; ++i) {
    const auto &def = dataset.layout.column_definition[i];
    if (!validFormat(def.name, def.format_string, def.type))
      return false;
  }
  for (int i = 0; i < dataset.layout.n_arrays; ++i) {
    const auto &def = dataset.layout.array_definition[i];
    if (!validFormat(def.name, def.format_string, def.type))
      return false;
  }

  if (!localEncodingPreserves(path)) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Cannot write %1: the path contains characters that cannot be represented in this system's character encoding")
                             .arg(QDir::toNativeSeparators(path)));
    return false;
  }

  SDDS_DATASET out;
  memset(&out, 0, sizeof(out));
  if (!SDDS_InitializeCopy(&out, &dataset,
                           const_cast<char *>(path.toLocal8Bit().constData()), (char *)"w")) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to open output"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }
  out.layout.data_mode.mode = asciiBtn->isChecked() ? SDDS_ASCII : SDDS_BINARY;
  // Editing columns invalidates the input's multiline row layout. Always emit
  // one complete ASCII row per line, including for compressed output.
  out.layout.data_mode.lines_per_row = 1;
  if (!SDDS_WriteLayout(&out)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to write layout"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&out);
    return false;
  }

  int pcount = dataset.layout.n_parameters;
  int ccount = dataset.layout.n_columns;
  int acount = dataset.layout.n_arrays;

  auto failWriteCall = [&](const QString &context) -> bool {
    QMessageBox::warning(this, tr("SDDS"), context);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&out);
    return false;
  };

  for (int pg = 0; pg < pages.size(); ++pg) {
    const PageStore &pd = pages[pg];
    int64_t rows = 0;
    if (ccount > 0 && pd.columns.size() > 0)
      rows = pd.columns[0].size();
    if (!SDDS_StartPage(&out, rows)) {
      QMessageBox::warning(this, tr("SDDS"), tr("Failed to start page"));
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Terminate(&out);
      return false;
    }

    for (int i = 0; i < pcount; ++i) {
      const PARAMETER_DEFINITION &pdef = dataset.layout.parameter_definition[i];
      if (pdef.fixed_value)
        continue;
      QString text = (i < pd.parameters.size()) ? pd.parameters[i] : QString();
      const char *name = pdef.name;
      int32_t type = pdef.type;
      bool ok = true;
      switch (type) {
      case SDDS_SHORT:
        {
          qint64 v = text.trimmed().isEmpty() ? 0 : text.toLongLong(&ok);
          if (!ok || v < std::numeric_limits<short>::min() || v > std::numeric_limits<short>::max())
            ok = false;
          if (!ok) {
            QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type short")
                                                   .arg(pg + 1)
                                                   .arg(QString::fromLocal8Bit(name))
                                                   .arg(truncateForMessage(text)));
            SDDS_Terminate(&out);
            return false;
          }
          if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (short)v, NULL))
            return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }
        break;
      case SDDS_USHORT:
        {
          qulonglong v = text.trimmed().isEmpty() ? 0 : text.toULongLong(&ok);
          if (!ok || v > std::numeric_limits<unsigned short>::max())
            ok = false;
          if (!ok) {
            QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type ushort")
                                                   .arg(pg + 1)
                                                   .arg(QString::fromLocal8Bit(name))
                                                   .arg(truncateForMessage(text)));
            SDDS_Terminate(&out);
            return false;
          }
          if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (unsigned short)v, NULL))
            return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }
        break;
      case SDDS_LONG:
        {
          qint64 v = text.trimmed().isEmpty() ? 0 : text.toLongLong(&ok);
          if (!ok || v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max())
            ok = false;
          if (!ok) {
            QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type long")
                                                   .arg(pg + 1)
                                                   .arg(QString::fromLocal8Bit(name))
                                                   .arg(truncateForMessage(text)));
            SDDS_Terminate(&out);
            return false;
          }
          if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (int32_t)v, NULL))
            return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }
        break;
      case SDDS_ULONG:
        {
          qulonglong v = text.trimmed().isEmpty() ? 0 : text.toULongLong(&ok);
          if (!ok || v > std::numeric_limits<uint32_t>::max())
            ok = false;
          if (!ok) {
            QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type ulong")
                                                   .arg(pg + 1)
                                                   .arg(QString::fromLocal8Bit(name))
                                                   .arg(truncateForMessage(text)));
            SDDS_Terminate(&out);
            return false;
          }
          if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (uint32_t)v, NULL))
            return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }
        break;
      case SDDS_LONG64:
        {
          qint64 v = text.trimmed().isEmpty() ? 0 : text.toLongLong(&ok);
          if (!ok) {
            QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type long64")
                                                   .arg(pg + 1)
                                                   .arg(QString::fromLocal8Bit(name))
                                                   .arg(truncateForMessage(text)));
            SDDS_Terminate(&out);
            return false;
          }
          if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (int64_t)v, NULL))
            return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }
        break;
      case SDDS_ULONG64:
        {
          qulonglong v = text.trimmed().isEmpty() ? 0 : text.toULongLong(&ok);
          if (!ok) {
            QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type ulong64")
                                                   .arg(pg + 1)
                                                   .arg(QString::fromLocal8Bit(name))
                                                   .arg(truncateForMessage(text)));
            SDDS_Terminate(&out);
            return false;
          }
          if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (uint64_t)v, NULL))
            return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }
        break;
      case SDDS_FLOAT:
        {
          float v = text.trimmed().isEmpty() ? 0.0f : text.toFloat(&ok);
          if (!ok) {
            QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type float")
                                                   .arg(pg + 1)
                                                   .arg(QString::fromLocal8Bit(name))
                                                   .arg(truncateForMessage(text)));
            SDDS_Terminate(&out);
            return false;
          }
          if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, v, NULL))
            return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }
        break;
      case SDDS_DOUBLE:
        {
          double v = text.trimmed().isEmpty() ? 0.0 : text.toDouble(&ok);
          if (!ok) {
            QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type double")
                                                   .arg(pg + 1)
                                                   .arg(QString::fromLocal8Bit(name))
                                                   .arg(truncateForMessage(text)));
            SDDS_Terminate(&out);
            return false;
          }
          if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, v, NULL))
            return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }
        break;
      case SDDS_LONGDOUBLE: {
        long double v;
        if (!parseLongDoubleStrict(text, &v)) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: parameter '%2' value '%3' is invalid for type long double")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name))
                                                 .arg(truncateForMessage(text)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, v, NULL))
          return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        break;
      }
      case SDDS_STRING:
        if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, text.toLocal8Bit().data(), NULL))
          return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        break;
      case SDDS_CHARACTER: {
        QByteArray ba = text.toLatin1();
        char ch = ba.isEmpty() ? '\0' : ba.at(0);
        if (!SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name,
                                ch, NULL))
          return failWriteCall(tr("Failed to set parameter '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        break;
      }
      default:
        break;
      }
    }

    out.n_rows = rows;
    for (int c = 0; c < ccount && c < pd.columns.size(); ++c) {
      const char *name = dataset.layout.column_definition[c].name;
      int32_t type = dataset.layout.column_definition[c].type;
      bool ok = true;
      if (type == SDDS_STRING) {
        QVector<QByteArray> encoded(static_cast<int>(rows));
        QVector<char *> arr(static_cast<int>(rows));
        for (int64_t r = 0; r < rows; ++r) {
          encoded[static_cast<int>(r)] = pd.columns[c][static_cast<int>(r)].toLocal8Bit();
          arr[static_cast<int>(r)] = encoded[static_cast<int>(r)].data();
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_CHARACTER) {
        QVector<char> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          QByteArray ba = text.toLatin1();
          arr[r] = ba.isEmpty() ? '\0' : ba.at(0);
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_LONGDOUBLE) {
        QVector<long double> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          if (!parseLongDoubleStrict(text, &arr[r]))
            ok = false;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type long double")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_LONG64) {
        QVector<int64_t> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          arr[r] = text.trimmed().isEmpty() ? 0 : text.toLongLong(&ok);
          if (!ok)
            break;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type long64")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_ULONG64) {
        QVector<uint64_t> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          arr[r] = text.trimmed().isEmpty() ? 0 : text.toULongLong(&ok);
          if (!ok)
            break;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type ulong64")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_DOUBLE) {
        QVector<double> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          arr[r] = text.trimmed().isEmpty() ? 0.0 : text.toDouble(&ok);
          if (!ok)
            break;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type double")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_FLOAT) {
        QVector<float> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          arr[r] = text.trimmed().isEmpty() ? 0.0f : text.toFloat(&ok);
          if (!ok)
            break;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type float")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_LONG) {
        QVector<int32_t> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          qint64 v = text.trimmed().isEmpty() ? 0 : text.toLongLong(&ok);
          if (!ok || v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max())
            ok = false;
          if (!ok)
            break;
          arr[r] = (int32_t)v;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type long")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_ULONG) {
        QVector<uint32_t> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          qulonglong v = text.trimmed().isEmpty() ? 0 : text.toULongLong(&ok);
          if (!ok || v > std::numeric_limits<uint32_t>::max())
            ok = false;
          if (!ok)
            break;
          arr[r] = (uint32_t)v;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type ulong")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_SHORT) {
        QVector<short> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          qint64 v = text.trimmed().isEmpty() ? 0 : text.toLongLong(&ok);
          if (!ok || v < std::numeric_limits<short>::min() || v > std::numeric_limits<short>::max())
            ok = false;
          if (!ok)
            break;
          arr[r] = (short)v;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type short")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_USHORT) {
        QVector<unsigned short> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          qulonglong v = text.trimmed().isEmpty() ? 0 : text.toULongLong(&ok);
          if (!ok || v > std::numeric_limits<unsigned short>::max())
            ok = false;
          if (!ok)
            break;
          arr[r] = (unsigned short)v;
        }
        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: column '%2' contains a value that is invalid for type ushort")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name)));
          SDDS_Terminate(&out);
          return false;
        }
        if (!SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name))
          return failWriteCall(tr("Failed to set column '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      }
    }

    for (int a = 0; a < acount && a < pd.arrays.size(); ++a) {
      const char *name = dataset.layout.array_definition[a].name;
      int32_t type = dataset.layout.array_definition[a].type;
      const ArrayStore &as = pd.arrays[a];
      int elements = as.values.size();
      QVector<int32_t> dims = QVector<int32_t>::fromList(as.dims.toList());
      bool ok = true;
      if (elements == 0) {
        char unused = 0;
        if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA,
                           &unused, dims.data()))
          return failWriteCall(tr("Failed to set empty array '%1'").arg(QString::fromLocal8Bit(name)));
        continue;
      }
      if (type == SDDS_STRING) {
        QVector<QByteArray> encoded(elements);
        QVector<char *> arr(elements);
        for (int i = 0; i < elements; ++i) {
          encoded[i] = as.values[i].toLocal8Bit();
          arr[i] = encoded[i].data();
        }
        if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, arr.data(), dims.data()))
          return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else if (type == SDDS_CHARACTER) {
        QVector<char> arr(elements);
        for (int i = 0; i < elements; ++i) {
          QString cell = as.values[i];
          QByteArray ba = cell.toLatin1();
          arr[i] = ba.isEmpty() ? '\0' : ba.at(0);
        }
        if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA,
                           arr.data(), dims.data()))
          return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
      } else {
        if (type == SDDS_LONGDOUBLE) {
          QVector<long double> buffer(elements);
          for (int i = 0; i < elements; ++i)
            if (!parseLongDoubleStrict(as.values[i], &buffer[i]))
              ok = false;
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        } else if (type == SDDS_DOUBLE) {
          QVector<double> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            buffer[i] = cell.trimmed().isEmpty() ? 0.0 : cell.toDouble(&ok);
            if (!ok)
              break;
          }
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        } else if (type == SDDS_FLOAT) {
          QVector<float> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            buffer[i] = cell.trimmed().isEmpty() ? 0.0f : cell.toFloat(&ok);
            if (!ok)
              break;
          }
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        } else if (type == SDDS_LONG64) {
          QVector<int64_t> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            buffer[i] = cell.trimmed().isEmpty() ? 0 : cell.toLongLong(&ok);
            if (!ok)
              break;
          }
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        } else if (type == SDDS_ULONG64) {
          QVector<uint64_t> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            buffer[i] = cell.trimmed().isEmpty() ? 0 : cell.toULongLong(&ok);
            if (!ok)
              break;
          }
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        } else if (type == SDDS_LONG) {
          QVector<int32_t> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            qint64 v = cell.trimmed().isEmpty() ? 0 : cell.toLongLong(&ok);
            if (!ok || v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max())
              ok = false;
            if (!ok)
              break;
            buffer[i] = (int32_t)v;
          }
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        } else if (type == SDDS_ULONG) {
          QVector<uint32_t> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            qulonglong v = cell.trimmed().isEmpty() ? 0 : cell.toULongLong(&ok);
            if (!ok || v > std::numeric_limits<uint32_t>::max())
              ok = false;
            if (!ok)
              break;
            buffer[i] = (uint32_t)v;
          }
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        } else if (type == SDDS_SHORT) {
          QVector<short> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            qint64 v = cell.trimmed().isEmpty() ? 0 : cell.toLongLong(&ok);
            if (!ok || v < std::numeric_limits<short>::min() || v > std::numeric_limits<short>::max())
              ok = false;
            if (!ok)
              break;
            buffer[i] = (short)v;
          }
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        } else if (type == SDDS_USHORT) {
          QVector<unsigned short> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            qulonglong v = cell.trimmed().isEmpty() ? 0 : cell.toULongLong(&ok);
            if (!ok || v > std::numeric_limits<unsigned short>::max())
              ok = false;
            if (!ok)
              break;
            buffer[i] = (unsigned short)v;
          }
          if (ok)
            if (!SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data()))
              return failWriteCall(tr("Failed to set array '%1' on page %2").arg(QString::fromLocal8Bit(name)).arg(pg + 1));
        }

        if (!ok) {
          QMessageBox::warning(this, tr("SDDS"), tr("Page %1: array '%2' contains a value that is invalid for type %3")
                                                 .arg(pg + 1)
                                                 .arg(QString::fromLocal8Bit(name))
                                                 .arg(QString::fromLocal8Bit(SDDS_GetTypeName(type))));
          SDDS_Terminate(&out);
          return false;
        }
      }
    }

    if (!SDDS_WritePage(&out)) {
      QMessageBox::warning(this, tr("SDDS"), tr("Failed to write page"));
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Terminate(&out);
      return false;
    }
  }

  if (!SDDS_Terminate(&out)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to finish output file"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }
  // SDDS can write definition text larger than its header parser accepts.
  // Check the staged header before an unreadable file replaces the destination.
  SDDS_DATASET verification;
  memset(&verification, 0, sizeof(verification));
  const bool readable = SDDS_InitializeInput(
      &verification, const_cast<char *>(path.toLocal8Bit().constData()));
  if (!readable) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  }
  const bool closed = readable ? SDDS_Terminate(&verification)
                               : terminateIncompleteDataset(&verification);
  if (!readable || !closed) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("The saved layout cannot be read by SDDS. Definition text or fixed values may be too long. "
                            "Shorten these fields or store long text in a non-fixed string parameter. "
                            "The destination file has not been replaced."));
    return false;
  }
  return true;
}

bool SDDSEditor::writeHDF(const QString &path) {
  if (!datasetLoaded)
    return false;
  commitModels();

  QString errorText;
  for (int pg = 0; pg < pages.size(); ++pg) {
    if (!validatePageForWrite(dataset.layout, pages[pg], pg, &errorText)) {
      QMessageBox::warning(this, tr("SDDS"), errorText);
      return false;
    }
  }

  const QFileInfo destination(path);
  const QString finalPath = destination.isSymLink() ? destination.symLinkTarget() : destination.absoluteFilePath();
  QTemporaryDir staging(QFileInfo(finalPath).absolutePath() + "/.sddseditor-hdf-XXXXXX");
  if (!staging.isValid()) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to create HDF staging directory"));
    return false;
  }
  const QString stagedPath = staging.filePath("export.h5");
  hid_t file = -1;
#ifdef _WIN32
  /*
   * HDF5 1.10.6 and later decode Windows file names as UTF-8, so the ANSI
   * name fails in a directory such as C:\Users\José.  Older versions use the
   * ANSI name; a misdecoded name cannot match the existing staging directory.
   */
  const QByteArray utf8Name = stagedPath.toUtf8();
  if (utf8Name != QFile::encodeName(stagedPath)) {
    H5E_auto2_t errorReport = nullptr;
    void *errorData = nullptr;
    H5Eget_auto2(H5E_DEFAULT, &errorReport, &errorData);
    H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);
    file = H5Fcreate(utf8Name.constData(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    H5Eset_auto2(H5E_DEFAULT, errorReport, errorData);
  }
#endif
  if (file < 0)
    file = H5Fcreate(QFile::encodeName(stagedPath).constData(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  if (file < 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to create HDF file"));
    return false;
  }

  int pcount = dataset.layout.n_parameters;
  int ccount = dataset.layout.n_columns;
  int acount = dataset.layout.n_arrays;

  auto failHdf = [&](const QString &context) -> bool {
    QMessageBox::warning(this, tr("SDDS"), context);
    H5Fclose(file);
    return false;
  };

  auto writeDataset = [&](hid_t grp, const char *name, hid_t dtype,
                          hid_t space, const void *data) -> bool {
    /*
     * HDF5 treats '/' as a path separator, but SDDS names such as "dnux/dp"
     * may contain it.  Percent-encode '%' and '/' so every name is one object.
     */
    QByteArray objectName(name ? name : "");
    objectName.replace("%", "%25");
    objectName.replace("/", "%2F");
    if (objectName == ".")
      objectName = "%2E";
    hid_t ds = H5Dcreate1(grp, objectName.constData(), dtype, space, H5P_DEFAULT);
    if (ds < 0)
      return false;
    herr_t writeStatus = H5Sget_simple_extent_npoints(space) == 0
                             ? 0 : H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    herr_t closeStatus = H5Dclose(ds);
    return writeStatus >= 0 && closeStatus >= 0;
  };

  for (int pg = 0; pg < pages.size(); ++pg) {
    const PageStore &pd = pages[pg];
    QByteArray gname = QString("page%1").arg(pg + 1).toLocal8Bit();
    hid_t page = H5Gcreate1(file, gname.constData(), 0);
    if (page < 0) {
      return failHdf(tr("Failed to create page group %1").arg(pg + 1));
    }

    if (pcount > 0) {
      hid_t grp = H5Gcreate1(page, "parameters", 0);
      if (grp < 0) {
        H5Gclose(page);
        return failHdf(tr("Failed to create parameter group for page %1").arg(pg + 1));
      }
      for (int i = 0; i < pcount && i < pd.parameters.size(); ++i) {
        const char *name = dataset.layout.parameter_definition[i].name;
        int32_t type = dataset.layout.parameter_definition[i].type;
        QString val = pd.parameters[i];
        hid_t space = H5Screate(H5S_SCALAR);
        if (space < 0) {
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Failed to create parameter dataspace for '%1' on page %2")
                             .arg(QString::fromLocal8Bit(name))
                             .arg(pg + 1));
        }
        if (type == SDDS_STRING) {
          QByteArray ba = val.toLocal8Bit();
          hid_t dtype = H5Tcopy(H5T_C_S1);
          bool ok = dtype >= 0 && H5Tset_size(dtype, ba.size() + 1) >= 0 &&
                    writeDataset(grp, name, dtype, space, ba.constData());
          if (dtype >= 0)
            H5Tclose(dtype);
          if (!ok) {
            H5Sclose(space);
            H5Gclose(grp);
            H5Gclose(page);
            return failHdf(tr("Failed to write parameter '%1' on page %2")
                               .arg(QString::fromLocal8Bit(name))
                               .arg(pg + 1));
          }
        } else if (type == SDDS_CHARACTER) {
          QByteArray ba = val.toLatin1();
          char ch = ba.isEmpty() ? '\0' : ba.at(0);
          if (!writeDataset(grp, name, H5T_NATIVE_CHAR, space, &ch)) {
            H5Sclose(space);
            H5Gclose(grp);
            H5Gclose(page);
            return failHdf(tr("Failed to write parameter '%1' on page %2")
                               .arg(QString::fromLocal8Bit(name))
                               .arg(pg + 1));
          }
        } else {
          hid_t dtype = hdfTypeForSdds(type);
          long double ldbuf;
          double dbuf;
          float fbuf;
          int64_t i64buf;
          uint64_t u64buf;
          int32_t i32buf;
          uint32_t u32buf;
          short s16buf;
          unsigned short u16buf;
          void *buf = nullptr;
          switch (type) {
          case SDDS_LONGDOUBLE:
            ldbuf = strtold(val.toLocal8Bit().constData(), nullptr);
            buf = &ldbuf;
            break;
          case SDDS_DOUBLE:
            dbuf = val.toDouble();
            buf = &dbuf;
            break;
          case SDDS_FLOAT:
            fbuf = val.toFloat();
            buf = &fbuf;
            break;
          case SDDS_LONG64:
            i64buf = val.toLongLong();
            buf = &i64buf;
            break;
          case SDDS_ULONG64:
            u64buf = val.toULongLong();
            buf = &u64buf;
            break;
          case SDDS_LONG:
            i32buf = val.toInt();
            buf = &i32buf;
            break;
          case SDDS_ULONG:
            u32buf = val.toUInt();
            buf = &u32buf;
            break;
          case SDDS_SHORT:
            s16buf = (short)val.toInt();
            buf = &s16buf;
            break;
          case SDDS_USHORT:
            u16buf = (unsigned short)val.toUInt();
            buf = &u16buf;
            break;
          default:
            dbuf = val.toDouble();
            buf = &dbuf;
            break;
          }
          if (!writeDataset(grp, name, dtype, space, buf)) {
            H5Sclose(space);
            H5Gclose(grp);
            H5Gclose(page);
            return failHdf(tr("Failed to write parameter '%1' on page %2")
                               .arg(QString::fromLocal8Bit(name))
                               .arg(pg + 1));
          }
        }
        if (H5Sclose(space) < 0) {
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Failed to close parameter dataspace on page %1").arg(pg + 1));
        }
      }
      if (H5Gclose(grp) < 0) {
        H5Gclose(page);
        return failHdf(tr("Failed to close parameter group on page %1").arg(pg + 1));
      }
    }

    if (ccount > 0) {
      hid_t grp = H5Gcreate1(page, "columns", 0);
      if (grp < 0) {
        H5Gclose(page);
        return failHdf(tr("Failed to create column group for page %1").arg(pg + 1));
      }
      int64_t rows = (ccount > 0 && pd.columns.size() > 0) ? pd.columns[0].size() : 0;
      hsize_t dims[1] = { (hsize_t)rows };
      for (int c = 0; c < ccount && c < pd.columns.size(); ++c) {
        const char *name = dataset.layout.column_definition[c].name;
        int32_t type = dataset.layout.column_definition[c].type;
        hid_t space = H5Screate_simple(1, dims, NULL);
        if (space < 0) {
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Failed to create column dataspace for '%1' on page %2")
                             .arg(QString::fromLocal8Bit(name))
                             .arg(pg + 1));
        }
        bool ok = true;
        if (type == SDDS_STRING) {
          QVector<QByteArray> store(rows);
          QVector<char *> ptrs(rows);
          for (int64_t r = 0; r < rows; ++r) {
            QString txt = r < pd.columns[c].size() ? pd.columns[c][r] : QString();
            store[r] = txt.toLocal8Bit();
            ptrs[r] = store[r].data();
          }
          hid_t dtype = H5Tcopy(H5T_C_S1);
          ok = dtype >= 0 && H5Tset_size(dtype, H5T_VARIABLE) >= 0 &&
               writeDataset(grp, name, dtype, space, ptrs.data());
          if (dtype >= 0)
            H5Tclose(dtype);
        } else if (type == SDDS_CHARACTER) {
          QVector<char> arr(rows);
          for (int64_t r = 0; r < rows; ++r) {
            QByteArray ba = (r < pd.columns[c].size()) ? pd.columns[c][r].toLatin1() : QByteArray();
            arr[r] = ba.isEmpty() ? '\0' : ba.at(0);
          }
          ok = writeDataset(grp, name, H5T_NATIVE_CHAR, space, arr.data());
        } else if (type == SDDS_LONGDOUBLE) {
          QVector<long double> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? strtold(pd.columns[c][r].toLocal8Bit().constData(), nullptr) : 0.0L;
          ok = writeDataset(grp, name, H5T_NATIVE_LDOUBLE, space, arr.data());
        } else if (type == SDDS_LONG64) {
          QVector<int64_t> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toLongLong() : 0;
          hid_t dtype = hdfTypeForSdds(type);
          ok = writeDataset(grp, name, dtype, space, arr.data());
        } else if (type == SDDS_ULONG64) {
          QVector<uint64_t> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toULongLong() : 0;
          hid_t dtype = hdfTypeForSdds(type);
          ok = writeDataset(grp, name, dtype, space, arr.data());
        } else if (type == SDDS_DOUBLE) {
          QVector<double> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toDouble() : 0.0;
          hid_t dtype = hdfTypeForSdds(type);
          ok = writeDataset(grp, name, dtype, space, arr.data());
        } else if (type == SDDS_FLOAT) {
          QVector<float> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toFloat() : 0.0f;
          hid_t dtype = hdfTypeForSdds(type);
          ok = writeDataset(grp, name, dtype, space, arr.data());
        } else if (type == SDDS_LONG) {
          QVector<int32_t> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toInt() : 0;
          hid_t dtype = hdfTypeForSdds(type);
          ok = writeDataset(grp, name, dtype, space, arr.data());
        } else if (type == SDDS_ULONG) {
          QVector<uint32_t> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toUInt() : 0;
          hid_t dtype = hdfTypeForSdds(type);
          ok = writeDataset(grp, name, dtype, space, arr.data());
        } else if (type == SDDS_SHORT) {
          QVector<short> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? (short)pd.columns[c][r].toInt() : (short)0;
          hid_t dtype = hdfTypeForSdds(type);
          ok = writeDataset(grp, name, dtype, space, arr.data());
        } else if (type == SDDS_USHORT) {
          QVector<unsigned short> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? (unsigned short)pd.columns[c][r].toUInt() : (unsigned short)0;
          hid_t dtype = hdfTypeForSdds(type);
          ok = writeDataset(grp, name, dtype, space, arr.data());
        } else {
          QVector<double> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toDouble() : 0.0;
          ok = writeDataset(grp, name, H5T_NATIVE_DOUBLE, space, arr.data());
        }

        if (!ok) {
          H5Sclose(space);
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Failed to write column '%1' on page %2")
                             .arg(QString::fromLocal8Bit(name))
                             .arg(pg + 1));
        }
        if (H5Sclose(space) < 0) {
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Failed to close column dataspace on page %1").arg(pg + 1));
        }
      }
      if (H5Gclose(grp) < 0) {
        H5Gclose(page);
        return failHdf(tr("Failed to close column group on page %1").arg(pg + 1));
      }
    }

    if (acount > 0) {
      hid_t grp = H5Gcreate1(page, "arrays", 0);
      if (grp < 0) {
        H5Gclose(page);
        return failHdf(tr("Failed to create array group for page %1").arg(pg + 1));
      }
      for (int a = 0; a < acount && a < pd.arrays.size(); ++a) {
        const char *name = dataset.layout.array_definition[a].name;
        int32_t type = dataset.layout.array_definition[a].type;
        const ArrayStore &as = pd.arrays[a];
        int dimsCount = as.dims.size();
        if (dimsCount <= 0) {
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Array '%1' on page %2 has invalid dimensions")
                             .arg(QString::fromLocal8Bit(name))
                             .arg(pg + 1));
        }
        for (int i = 0; i < dimsCount; ++i) {
          if (as.dims[i] < 0) {
            H5Gclose(grp);
            H5Gclose(page);
            return failHdf(tr("Array '%1' on page %2 has negative dimension %3")
                               .arg(QString::fromLocal8Bit(name))
                               .arg(pg + 1)
                               .arg(i + 1));
          }
        }
        QVector<hsize_t> dims(dimsCount);
        for (int i = 0; i < dimsCount; ++i)
          dims[i] = as.dims[i];
        hid_t space = H5Screate_simple(dimsCount, dims.data(), NULL);
        if (space < 0) {
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Failed to create array dataspace for '%1' on page %2")
                             .arg(QString::fromLocal8Bit(name))
                             .arg(pg + 1));
        }
        int elements = as.values.size();
        bool ok = true;
        if (type == SDDS_STRING) {
          QVector<QByteArray> store(elements);
          QVector<char *> ptrs(elements);
          for (int i = 0; i < elements; ++i) {
            store[i] = as.values[i].toLocal8Bit();
            ptrs[i] = store[i].data();
          }
          hid_t dtype = H5Tcopy(H5T_C_S1);
          ok = dtype >= 0 && H5Tset_size(dtype, H5T_VARIABLE) >= 0 &&
               writeDataset(grp, name, dtype, space, ptrs.data());
          if (dtype >= 0)
            H5Tclose(dtype);
        } else if (type == SDDS_CHARACTER) {
          QVector<char> arr(elements);
          for (int i = 0; i < elements; ++i) {
            QByteArray ba = as.values[i].toLatin1();
            arr[i] = ba.isEmpty() ? '\0' : ba.at(0);
          }
          ok = writeDataset(grp, name, H5T_NATIVE_CHAR, space, arr.data());
        } else if (type == SDDS_LONGDOUBLE) {
          QVector<long double> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            if (!parseLongDoubleStrict(as.values[i], &buffer[i])) {
              ok = false;
              break;
            }
          }
          if (ok)
            ok = writeDataset(grp, name, H5T_NATIVE_LDOUBLE, space,
                              buffer.data());
        } else if (type == SDDS_DOUBLE) {
          QVector<double> buffer(elements);
          for (int i = 0; i < elements; ++i)
            buffer[i] = as.values[i].toDouble();
          ok = writeDataset(grp, name, H5T_NATIVE_DOUBLE, space,
                            buffer.data());
        } else if (type == SDDS_FLOAT) {
          QVector<float> buffer(elements);
          for (int i = 0; i < elements; ++i)
            buffer[i] = as.values[i].toFloat();
          ok = writeDataset(grp, name, H5T_NATIVE_FLOAT, space,
                            buffer.data());
        } else if (type == SDDS_LONG64) {
          QVector<int64_t> buffer(elements);
          for (int i = 0; i < elements; ++i)
            buffer[i] = as.values[i].toLongLong();
          ok = writeDataset(grp, name, hdfTypeForSdds(type), space,
                            buffer.data());
        } else if (type == SDDS_ULONG64) {
          QVector<uint64_t> buffer(elements);
          for (int i = 0; i < elements; ++i)
            buffer[i] = as.values[i].toULongLong();
          ok = writeDataset(grp, name, hdfTypeForSdds(type), space,
                            buffer.data());
        } else if (type == SDDS_LONG) {
          QVector<int32_t> buffer(elements);
          for (int i = 0; i < elements; ++i)
            buffer[i] = as.values[i].toInt();
          ok = writeDataset(grp, name, hdfTypeForSdds(type), space,
                            buffer.data());
        } else if (type == SDDS_ULONG) {
          QVector<uint32_t> buffer(elements);
          for (int i = 0; i < elements; ++i)
            buffer[i] = as.values[i].toUInt();
          ok = writeDataset(grp, name, hdfTypeForSdds(type), space,
                            buffer.data());
        } else if (type == SDDS_SHORT) {
          QVector<short> buffer(elements);
          for (int i = 0; i < elements; ++i)
            buffer[i] = static_cast<short>(as.values[i].toInt());
          ok = writeDataset(grp, name, hdfTypeForSdds(type), space,
                            buffer.data());
        } else if (type == SDDS_USHORT) {
          QVector<unsigned short> buffer(elements);
          for (int i = 0; i < elements; ++i)
            buffer[i] = static_cast<unsigned short>(as.values[i].toUInt());
          ok = writeDataset(grp, name, hdfTypeForSdds(type), space,
                            buffer.data());
        } else {
          ok = false;
        }

        if (!ok) {
          H5Sclose(space);
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Failed to write array '%1' on page %2")
                             .arg(QString::fromLocal8Bit(name))
                             .arg(pg + 1));
        }
        if (H5Sclose(space) < 0) {
          H5Gclose(grp);
          H5Gclose(page);
          return failHdf(tr("Failed to close array dataspace on page %1").arg(pg + 1));
        }
      }
      if (H5Gclose(grp) < 0) {
        H5Gclose(page);
        return failHdf(tr("Failed to close array group on page %1").arg(pg + 1));
      }
    }

    if (H5Gclose(page) < 0)
      return failHdf(tr("Failed to close page group %1").arg(pg + 1));
  }

  if (H5Fclose(file) < 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to close HDF file"));
    return false;
  }

  QFile input(stagedPath);
  QSaveFile output(finalPath);
  output.setDirectWriteFallback(false);
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to prepare HDF output replacement"));
    return false;
  }
  while (!input.atEnd()) {
    const QByteArray chunk = input.read(1024 * 1024);
    if (input.error() != QFileDevice::NoError || output.write(chunk) != chunk.size()) {
      QMessageBox::warning(this, tr("SDDS"), tr("Failed while copying staged HDF output"));
      return false;
    }
  }
  if (!output.commit()) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to replace HDF output file"));
    return false;
  }

  return true;
}

bool SDDSEditor::writeCSV(const QString &path) {
  if (!datasetLoaded)
    return false;
  commitModels();

  QString errorText;
  for (int pg = 0; pg < pages.size(); ++pg) {
    if (!validatePageForWrite(dataset.layout, pages[pg], pg, &errorText)) {
      QMessageBox::warning(this, tr("SDDS"), errorText);
      return false;
    }
  }

  const QFileInfo destination(path);
  const QString finalPath = destination.isSymLink() ? destination.symLinkTarget() : destination.absoluteFilePath();
  QSaveFile file(finalPath);
  file.setDirectWriteFallback(false);
  // Not QIODevice::Text: on Windows it would rewrite newlines inside quoted text values as CRLF.
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to open output"));
    return false;
  }

  QTextStream out(&file);
  // Write the local 8-bit encoding used for SDDS text, as Qt 5 streams do by
  // default; Qt 6 streams default to UTF-8, which differs on Windows.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  out.setEncoding(QStringConverter::System);
#endif

  auto escape = [](const QString &txt) {
    QString t = txt;
    t.replace('"', "\"\"");
    bool need = t.contains(',') || t.contains('"') || t.contains('\n') ||
                t.contains('\r');
    if (need)
      t = '"' + t + '"';
    return t;
  };

  int32_t pcount = dataset.layout.n_parameters;
  int32_t ccount = dataset.layout.n_columns;
  int32_t acount = dataset.layout.n_arrays;

  for (int pg = 0; pg < pages.size(); ++pg) {
    const PageStore &pd = pages[pg];

    if (pg > 0)
      out << '\n';

    if (pcount > 0) {
      out << "Parameters" << '\n';
      for (int32_t i = 0; i < pcount; ++i) {
        const char *name = dataset.layout.parameter_definition[i].name;
        QString value = (i < pd.parameters.size()) ? pd.parameters[i] : QString();
        const int32_t type = dataset.layout.parameter_definition[i].type;
        if (SDDS_NUMERIC_TYPE(type) && value.trimmed().isEmpty())
          value = "0";
        out << escape(QString::fromLocal8Bit(name)) << ',' << escape(value) << '\n';
      }
      out << '\n';
    }

    if (ccount > 0) {
      out << "Columns" << '\n';
      for (int32_t i = 0; i < ccount; ++i) {
        const char *name = dataset.layout.column_definition[i].name;
        out << escape(QString::fromLocal8Bit(name));
        if (i != ccount - 1)
          out << ',';
      }
      out << '\n';

      int64_t rows = (pd.columns.size() > 0) ? pd.columns[0].size() : 0;
      for (int64_t r = 0; r < rows; ++r) {
        for (int32_t c = 0; c < ccount; ++c) {
          QString cell = (c < pd.columns.size() && r < pd.columns[c].size()) ? pd.columns[c][r] : QString();
          const int32_t type = dataset.layout.column_definition[c].type;
          if (SDDS_NUMERIC_TYPE(type) && cell.trimmed().isEmpty())
            cell = "0";
          out << escape(cell);
          if (c != ccount - 1)
            out << ',';
        }
        out << '\n';
      }
      out << '\n';
    }

    if (acount > 0) {
      out << "Arrays" << '\n';
      for (int32_t a = 0; a < acount; ++a) {
        const char *name = dataset.layout.array_definition[a].name;
        out << escape(QString::fromLocal8Bit(name));
        if (a != acount - 1)
          out << ',';
      }
      out << '\n';

      int maxLen = 0;
      for (int a = 0; a < acount && a < pd.arrays.size(); ++a)
        if (pd.arrays[a].values.size() > maxLen)
          maxLen = pd.arrays[a].values.size();

      for (int r = 0; r < maxLen; ++r) {
        for (int a = 0; a < acount; ++a) {
          const bool present = a < pd.arrays.size() && r < pd.arrays[a].values.size();
          QString cell = present
                             ? pd.arrays[a].values[r]
                             : QString();
          const int32_t type = dataset.layout.array_definition[a].type;
          if (present && SDDS_NUMERIC_TYPE(type) && cell.trimmed().isEmpty())
            cell = "0";
          out << escape(cell);
          if (a != acount - 1)
            out << ',';
        }
        out << '\n';
      }
      out << '\n';
    }
  }

  out.flush();
  if (out.status() != QTextStream::Ok || file.error() != QFileDevice::NoError) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed while writing CSV output"));
    file.cancelWriting();
    return false;
  }
  if (!file.commit()) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to replace CSV output file"));
    return false;
  }

  return true;
}

void SDDSEditor::saveFile() {
  if (currentFilename.isEmpty()) {
    saveFileAs();
    return;
  }
  writeFile(currentFilename);
}

void SDDSEditor::saveFileAs() {
  QString path = QFileDialog::getSaveFileName(this, tr("Save SDDS"), currentFilename,
                                             tr("SDDS Files (*.sdds);;All Files (*)"));
  if (path.isEmpty())
    return;
  if (writeFile(path)) {
    currentFilename = path;
    updateWindowTitle();
  }
}

/** Name an export after the open file; proposing the SDDS file itself risks overwriting it. */
static QString exportDefaultPath(const QString &filename, const QString &suffix) {
  if (filename.isEmpty())
    return QString();
  const QFileInfo fi(filename);
  return fi.path() + '/' + fi.completeBaseName() + suffix;
}

void SDDSEditor::saveFileAsHDF() {
  QString path = QFileDialog::getSaveFileName(this, tr("Save HDF"), exportDefaultPath(currentFilename, ".h5"),
                                             tr("HDF Files (*.h5 *.hdf);;All Files (*)"));
  if (path.isEmpty())
    return;
  if (writeHDF(path))
    message(tr("Saved %1").arg(path));
}

void SDDSEditor::exportCSV() {
  QString def = exportDefaultPath(currentFilename, ".csv");
  QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), def,
                                             tr("CSV Files (*.csv);;All Files (*)"));
  if (path.isEmpty())
    return;
  if (writeCSV(path))
    message(tr("Saved %1").arg(path));
}

void SDDSEditor::pageChanged(int value) {
  if (!datasetLoaded)
    return;
  commitModels();
  if (value < 0 || value >= pages.size())
    return;
  /*
   * Cell undo commands contain indexes in the page-backed models.  Those
   * indexes are reused when another page is displayed, so retaining them
   * across a page switch would make Undo modify the wrong page.
   */
  if (value != currentPage)
    undoStack->clear();
  loadPage(value + 1);
}

void SDDSEditor::loadPage(int page) {
  currentPage = page - 1;
  populateModels();
}

void SDDSEditor::flushPendingEdits() {
  for (QTableView *view : {paramView, columnView, arrayView})
    static_cast<SingleClickEditTableView *>(view)->finishEditing();
  for (const QPointer<QDialog> &viewer : arrayViewers)
    if (viewer)
      static_cast<ArrayViewer *>(viewer.data())->finishEditing();
}

void SDDSEditor::populateModels() {
  if (!datasetLoaded || pages.isEmpty() || currentPage < 0 || currentPage >= pages.size())
    return;

  QProgressDialog *progress = loadProgressDialog.data();
  const int progressMin = loadProgressMin;
  const int progressMax = loadProgressMax;
  int64_t totalUnits = 0;
  int64_t doneUnits = 0;
  auto updateProgress = [&](bool force) {
    if (!progress)
      return;
    int span = progressMax - progressMin;
    int value = progressMin;
    if (span > 0 && totalUnits > 0) {
      value = progressMin + static_cast<int>((doneUnits * span) / totalUnits);
      value = std::min(progressMax, std::max(progressMin, value));
    }
    if (force || value != progress->value()) {
      progress->setValue(value);
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
  };

  updatingModels = true;

  // Pre-compute rough work units so progress is monotonic.
  int32_t pcount = dataset.layout.n_parameters;
  int32_t ccount = dataset.layout.n_columns;
  int32_t acount = dataset.layout.n_arrays;
  // Note: columns/arrays now use virtual models (no per-cell allocation).
  // We treat model resets and (optional) sizing passes as the main work units.
  totalUnits = 5;

  const PageStore &pd = pages[currentPage];
  int64_t rows = (ccount > 0 && pd.columns.size() > 0) ? pd.columns[0].size() : 0;
  int64_t maxArrayElements = 0;
  for (const ArrayStore &array : pd.arrays)
    maxArrayElements = std::max<int64_t>(maxArrayElements, array.values.size());
  const bool hugeColumnTable =
      rows > 0 && ccount > 0 && rows * static_cast<int64_t>(ccount) > 500000;
  const bool hugeArrayTable = maxArrayElements > 0 && acount > 0 &&
                              maxArrayElements * static_cast<int64_t>(acount) > 500000;

  // parameters
  if (progress) {
    progress->setLabelText(tr("Preparing display… (parameters)"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  paramModel->refresh();
  updateParameterColumns();
  ++doneUnits;
  if (progress)
    updateProgress(false);

  // columns
  if (progress) {
    progress->setLabelText(tr("Preparing display… (columns)"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  columnModel->refresh();
  ++doneUnits;
  if (progress)
    updateProgress(false);


  // Resize columns to fit their contents first so initial widths are reasonable;
  // the user can then adjust them.
  if (progress) {
    progress->setLabelText(tr("Preparing display… (sizing columns)"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  // For very large tables, computing size-to-contents can be very expensive.
  // During initial load (when progress dialog exists), skip it to speed up load.
  if (!(progress && hugeColumnTable))
    columnView->resizeColumnsToContents();
  // Keep content widths: stretching the last section would push right-aligned
  // numbers away from their neighbors.
  columnView->horizontalHeader()->setStretchLastSection(false);
  columnView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);
  ++doneUnits;
  if (progress)
    updateProgress(false);

  // arrays
  if (progress) {
    progress->setLabelText(tr("Preparing display… (arrays)"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  arrayModel->refresh();
  ++doneUnits;
  if (progress)
    updateProgress(false);


  // Similar treatment for arrays table.
  if (progress) {
    progress->setLabelText(tr("Preparing display… (sizing arrays)"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  if (!(progress && hugeArrayTable))
    arrayView->resizeColumnsToContents();
  arrayView->horizontalHeader()->setStretchLastSection(false);
  arrayView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);

  ++doneUnits;
  if (progress)
    updateProgress(true);

  updatePanelSizing(pcount, ccount, acount);
  updatingModels = false;
  refreshColumnRowFilter(false);
}

void SDDSEditor::updatePanelSizing(int32_t pcount, int32_t ccount, int32_t acount) {
  const bool anyExist = (pcount > 0) || (ccount > 0) || (acount > 0);

  auto applyPanelState = [&](DataPanel *box, QTableView *view, bool hasData) {
    if (!anyExist) {
      box->setChecked(true);
      view->setVisible(true);
      box->setMinimumHeight(0);
      box->setMaximumHeight(QWIDGETSIZE_MAX);
      box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
      return;
    }

    if (hasData) {
      box->setChecked(true);
      view->setVisible(true);
      box->setMinimumHeight(0);
      box->setMaximumHeight(QWIDGETSIZE_MAX);
      box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    } else {
      box->setChecked(false);
      view->setVisible(false);
      box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
      box->setMinimumHeight(box->sizeHint().height());
      box->setMaximumHeight(QWIDGETSIZE_MAX);
    }
  };

  applyPanelState(paramBox, paramView, pcount > 0);
  applyPanelState(colBox, columnView, ccount > 0);
  applyPanelState(arrayBox, arrayView, acount > 0);

  const bool columnData = ccount > 0 && columnModel->rowCount() > 0;
  const bool arrayData = acount > 0 && arrayModel->rowCount() > 0;
  const bool fitParameters = pcount > 0 && (columnData || arrayData);
  paramView->setSizePolicy(QSizePolicy::Expanding,
                           fitParameters ? QSizePolicy::Ignored : QSizePolicy::Expanding);
  int parameterHeight = QWIDGETSIZE_MAX;
  if (fitParameters) {
    // Fit the initial panel to its rows, header, frame, and panel title bar.
    // Ignore the scroll area's minimum-size hint so a small parameter set fits,
    // but do not impose a maximum height that would prevent manual expansion.
    paramView->ensurePolished();
    int64_t height = static_cast<int64_t>(paramView->verticalHeader()->length()) +
                     paramView->horizontalHeader()->sizeHint().height() +
                     2 * paramView->frameWidth() +
                     paramBox->chromeHeight();
    if (paramView->horizontalScrollBar()->isVisible())
      height += paramView->horizontalScrollBar()->sizeHint().height();
    parameterHeight = static_cast<int>(std::min<int64_t>(height, QWIDGETSIZE_MAX));
  }

  // Only panels with rows/elements receive the space released by parameters.
  dataSplitter->setStretchFactor(0, columnData || arrayData ? 0 : 1);
  dataSplitter->setStretchFactor(1, columnData ? 1 : 0);
  dataSplitter->setStretchFactor(2, arrayData ? 1 : 0);

  // Hidden splitters have no usable geometry; showEvent applies these sizes.
  if ((!columnData && !arrayData) || !dataSplitter->isVisible())
    return;

  // Assign compact sizes through the splitter rather than widget height limits,
  // leaving every panel free to expand when the user drags a splitter handle.
  QList<int> sizes = dataSplitter->sizes();
  const int columnWeight = std::max(1, sizes[1]);
  const int arrayWeight = std::max(1, sizes[2]);
  if (pcount == 0)
    sizes[0] = paramBox->sizeHint().height();
  if (ccount == 0)
    sizes[1] = colBox->sizeHint().height();
  if (acount == 0)
    sizes[2] = arrayBox->sizeHint().height();
  int available = dataSplitter->contentsRect().height();
  for (int i = 1; i < dataSplitter->count(); ++i)
    available -= dataSplitter->handle(i)->height();
  // Initial layout can assign less than a row to a view with Ignored policy.
  // Allow up to a third of the height to fit parameters before sharing the rest.
  if (fitParameters)
    sizes[0] = std::min(parameterHeight, std::max(sizes[0], available / 3));
  const int remaining = std::max(0, available - sizes[0] -
                                   (columnData ? 0 : sizes[1]) -
                                   (arrayData ? 0 : sizes[2]));
  if (columnData && arrayData) {
    sizes[1] = static_cast<int>(static_cast<int64_t>(remaining) * columnWeight /
                                (static_cast<int64_t>(columnWeight) + arrayWeight));
    sizes[2] = remaining - sizes[1];
  } else {
    sizes[columnData ? 1 : 2] = remaining;
  }
  dataSplitter->setSizes(sizes);
}

/** Reapply panel sizing with final geometry for files loaded before show(). */
void SDDSEditor::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
  if (datasetLoaded) {
    QTimer::singleShot(0, this, [this]() {
      if (datasetLoaded)
        updatePanelSizing(dataset.layout.n_parameters, dataset.layout.n_columns, dataset.layout.n_arrays);
    });
  }
}

void SDDSEditor::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);

  // When the user drags to resize/maximizes the window, Qt can trigger a large number
  // of intermediate repaints. For large datasets this makes interactive resizing feel
  // sluggish. We temporarily suspend updates on the heavy tables, then repaint once
  // the resize settles.
  if (!isVisible())
    return;

  // Only suspend when a dataset is present; otherwise it just makes the UI feel blank.
  if (!datasetLoaded)
    return;

  // Only apply the repaint debounce for very large tables where full repaints during
  // interactive resizing become noticeably expensive. For typical datasets, allow
  // normal repainting to avoid visual distortion while resizing.
  const int32_t ccount = dataset.layout.n_columns;
  int64_t rows = 0;
  if (ccount > 0 && currentPage >= 0 && currentPage < pages.size()) {
    const PageStore &pd = pages[currentPage];
    if (pd.columns.size() > 0)
      rows = pd.columns[0].size();
  }
  const bool hugeTable = (rows > 0 && ccount > 0 && (rows * static_cast<int64_t>(ccount)) > 250000);
  if (!hugeTable)
    return;

  if (!resizeUpdatesSuspended) {
    if (columnView)
      columnView->setUpdatesEnabled(false);
    if (arrayView)
      arrayView->setUpdatesEnabled(false);
    resizeUpdatesSuspended = true;
  }

  // Restart debounce each time we get another resize event.
  resizeDebounceTimer->start(80);
}

void SDDSEditor::clearDataset() {
  // Viewer callbacks refer to this dataset; close them before replacing it.
  for (const QPointer<QDialog> &viewer : arrayViewers)
    delete viewer.data();
  arrayViewers.clear();

  if (datasetLoaded) {
    SDDS_Terminate(&dataset);
    memset(&dataset, 0, sizeof(dataset));
    datasetLoaded = false;
    pageCombo->clear();
    pages.clear();
    currentPage = 0;
    paramModel->refresh();
    columnModel->refresh();
    arrayModel->refresh();
  }
  rowFilterActive = false;
  rowFilterExpression.clear();
  undoStack->clear();
}

/**
 * @brief Ensure an SDDS dataset exists, creating an empty one if necessary.
 * @return true if a dataset is available, false otherwise.
 */
bool SDDSEditor::ensureDataset() {
  if (datasetLoaded)
    return true;

  memset(&dataset, 0, sizeof(dataset));
  if (!SDDS_InitializeOutput(&dataset, asciiSave ? SDDS_ASCII : SDDS_BINARY, 1,
                             NULL, NULL, NULL)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to initialize dataset"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }

  datasetLoaded = true;
  pages.clear();
  pages.append(PageStore());
  currentPage = 0;

  pageCombo->blockSignals(true);
  pageCombo->clear();
  pageCombo->addItem(tr("Page %1").arg(1));
  pageCombo->blockSignals(false);

  undoStack->clear();

  populateModels();
  return true;
}
void SDDSEditor::commitModels() {
  flushPendingEdits();

  if (!datasetLoaded || pages.isEmpty() || currentPage < 0 || currentPage >= pages.size())
    return;

  PageStore &pd = pages[currentPage];

  int32_t pcount = dataset.layout.n_parameters;
  pd.parameters.resize(pcount);
  for (int32_t i = 0; i < pcount && i < paramModel->rowCount(); ++i) {
    QString val = paramModel->index(i, 0).data(Qt::EditRole).toString();
    pd.parameters[i] = val;
    PARAMETER_DEFINITION *def = &dataset.layout.parameter_definition[i];
    if (def->fixed_value) {
      PARAMETER_DEFINITION *savedDef =
          dataset.original_layout.parameter_definition &&
                  i < dataset.original_layout.n_parameters
              ? &dataset.original_layout.parameter_definition[i]
              : nullptr;
      const QString fixedValue = fixedValueForDefinition(val, def->type);
      if (savedDef && QString::fromLocal8Bit(def->fixed_value) != fixedValue) {
        if (!replaceSharedLayoutString(&def->fixed_value,
                                       &savedDef->fixed_value,
                                       fixedValue, false)) {
          QMessageBox::warning(this, tr("SDDS"),
                               tr("Out of memory while updating fixed parameter '%1'")
                                   .arg(QString::fromLocal8Bit(def->name)));
        }
      }
      for (int pg = 0; pg < pages.size(); ++pg) {
        if (pg == currentPage)
          continue;
        if (pages[pg].parameters.size() < pcount)
          pages[pg].parameters.resize(pcount);
        pages[pg].parameters[i] = val;
      }
    }
  }

  // Columns/arrays are edited directly in PageStore via the virtual models.
  // Keep array storage consistent with its dimensions.
  int32_t acount = dataset.layout.n_arrays;
  pd.arrays.resize(acount);
  for (int32_t a = 0; a < acount && a < pd.arrays.size(); ++a) {
    ArrayStore &as = pd.arrays[a];
    int expected = dimProduct(as.dims);
    if (expected < 0)
      continue;
    if (expected != as.values.size())
      as.values.resize(expected);
  }
}

void SDDSEditor::editParameterAttributes() {
  if (!datasetLoaded)
    return;
  commitModels();
  QModelIndex idx = paramView->currentIndex();
  if (!idx.isValid())
    return;
  int row = idx.row();
  PARAMETER_DEFINITION *def = &dataset.layout.parameter_definition[row];
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Parameter Attributes"));
  configureEditorPopupDialog(&dlg, this);
  QFormLayout form(&dlg);
  // Decode as the fields are encoded on OK (local 8-bit), or unchanged non-ASCII text is corrupted.
  SDDSTextEdit name(QString::fromLocal8Bit(def->name ? def->name : ""), &dlg);
  SDDSTextEdit symbol(QString::fromLocal8Bit(def->symbol ? def->symbol : ""), &dlg);
  SDDSTextEdit units(QString::fromLocal8Bit(def->units ? def->units : ""), &dlg);
  SDDSTextEdit desc(QString::fromLocal8Bit(def->description ? def->description : ""), &dlg);
  SDDSTextEdit fmt(QString::fromLocal8Bit(def->format_string ? def->format_string : ""), &dlg);
  SDDSTextEdit fixed(fixedValueForDisplay(*def), &dlg);
  QHBoxLayout *typeLayout = new QHBoxLayout();
  QButtonGroup typeGroup(&dlg);
  QMap<int, QRadioButton *> btns;
  auto addBtn = [&](const QString &text, int id) {
    QRadioButton *b = new QRadioButton(text, &dlg);
    typeGroup.addButton(b, id);
    typeLayout->addWidget(b);
    btns[id] = b;
  };
  addBtn(tr("short"), SDDS_SHORT);
  addBtn(tr("ushort"), SDDS_USHORT);
  addBtn(tr("long"), SDDS_LONG);
  addBtn(tr("ulong"), SDDS_ULONG);
  addBtn(tr("long64"), SDDS_LONG64);
  addBtn(tr("ulong64"), SDDS_ULONG64);
  addBtn(tr("float"), SDDS_FLOAT);
  addBtn(tr("double"), SDDS_DOUBLE);
  addBtn(tr("long double"), SDDS_LONGDOUBLE);
  addBtn(tr("string"), SDDS_STRING);
  addBtn(tr("character"), SDDS_CHARACTER);
  if (btns.contains(def->type))
    btns[def->type]->setChecked(true);
  form.addRow(tr("Name"), &name);
  form.addRow(tr("Symbol"), &symbol);
  form.addRow(tr("Units"), &units);
  form.addRow(tr("Description"), &desc);
  form.addRow(tr("Format"), &fmt);
  form.addRow(tr("Fixed value"), &fixed);
  form.addRow(tr("Type"), typeLayout);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dlg);
  form.addRow(&buttons);
  connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted)
    return;
  if (!validateDefinitionName(this, name.text(), "parameter", row,
                              dataset.layout.n_parameters,
                              [this](int i) {
                                return dataset.layout.parameter_definition[i].name;
                              }))
    return;
  if (!definitionTextEncodable(this, {symbol.text(), units.text(), desc.text(), fmt.text()}))
    return;
  const int32_t tval = typeGroup.checkedId();
  if (!fixed.text().isEmpty() &&
      !validateTextForType(fixed.text(), tval, false)) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Fixed value is invalid for type %1")
                             .arg(QString::fromLocal8Bit(SDDS_GetTypeName(tval))));
    return;
  }

  if (!dataset.original_layout.parameter_definition ||
      row >= dataset.original_layout.n_parameters) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Saved parameter layout is inconsistent"));
    return;
  }

  if (definitionTextMatches(def->name, name.text()) &&
      definitionTextMatches(def->symbol, symbol.text()) &&
      definitionTextMatches(def->units, units.text()) &&
      definitionTextMatches(def->description, desc.text()) &&
      definitionTextMatches(def->format_string, fmt.text()) &&
      fixedValueForDisplay(*def) == fixed.text() && def->type == tval)
    return;

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  PARAMETER_DEFINITION *savedDef =
      &dataset.original_layout.parameter_definition[row];
  auto replaceField = [&](char **workingField, char **savedField,
                          const QString &value,
                          bool nullWhenEmpty = true) -> bool {
    if (replaceSharedLayoutString(workingField, savedField, value,
                                  nullWhenEmpty))
      return true;
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Out of memory while updating parameter attributes"));
    restoreStructuralSnapshot(this, before);
    return false;
  };
  // Rewrite only edited text.  Unchanged bytes that do not decode in this
  // locale (Latin-1 in a UTF-8 locale) would come back as replacement characters.
  auto replaceText = [&](char **workingField, char **savedField, const QString &value) {
    return definitionTextMatches(*workingField, value) ||
           replaceField(workingField, savedField, value);
  };

  if (!definitionTextMatches(def->name, name.text())) {
    if (!replaceField(&def->name, &savedDef->name, name.text(), false))
      return;
    if (!resyncSortedIndexName(dataset.layout.parameter_index,
                               dataset.layout.n_parameters, row,
                               def->name) ||
        !resyncSortedIndexName(dataset.original_layout.parameter_index,
                               dataset.original_layout.n_parameters, row,
                               savedDef->name)) {
      QMessageBox::warning(this, tr("SDDS"),
                           tr("Failed to update parameter name index"));
      restoreStructuralSnapshot(this, before);
      return;
    }
  }
  if (!replaceText(&def->symbol, &savedDef->symbol, symbol.text()))
    return;
  if (!replaceText(&def->units, &savedDef->units, units.text()))
    return;
  if (!replaceText(&def->description, &savedDef->description, desc.text()))
    return;
  if (!replaceText(&def->format_string, &savedDef->format_string, fmt.text()))
    return;
  const bool hasFixedValue = !fixed.text().isEmpty() ||
                            (def->fixed_value && fixedValueForDisplay(*def).isEmpty());
  if (!replaceField(&def->fixed_value, &savedDef->fixed_value,
                    hasFixedValue ? fixedValueForDefinition(fixed.text(), tval) : QString(), !hasFixedValue))
    return;
  def->type = savedDef->type = tval;
  if (def->fixed_value) {
    for (PageStore &pd : pages) {
      if (pd.parameters.size() < dataset.layout.n_parameters)
        pd.parameters.resize(dataset.layout.n_parameters);
      pd.parameters[row] = fixed.text();
    }
  }
  paramModel->refresh();
  updateParameterColumns();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            tr("Edit Parameter Attributes"));
}

void SDDSEditor::editColumnAttributes() {
  const QModelIndex idx = columnView->currentIndex();
  if (idx.isValid())
    editColumnAttributesAt(idx.column());
}

/** Edit one column definition; reachable from its header even with no rows. */
void SDDSEditor::editColumnAttributesAt(int col) {
  if (!datasetLoaded || col < 0 || col >= dataset.layout.n_columns)
    return;
  commitModels();
  COLUMN_DEFINITION *def = &dataset.layout.column_definition[col];
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Column Attributes"));
  configureEditorPopupDialog(&dlg, this);
  QFormLayout form(&dlg);
  SDDSTextEdit name(QString::fromLocal8Bit(def->name ? def->name : ""), &dlg);
  SDDSTextEdit symbol(QString::fromLocal8Bit(def->symbol ? def->symbol : ""), &dlg);
  SDDSTextEdit units(QString::fromLocal8Bit(def->units ? def->units : ""), &dlg);
  SDDSTextEdit desc(QString::fromLocal8Bit(def->description ? def->description : ""), &dlg);
  SDDSTextEdit fmt(QString::fromLocal8Bit(def->format_string ? def->format_string : ""), &dlg);
  QSpinBox length(&dlg);
  // Negative string field lengths are valid; clamping them would change the definition.
  length.setRange(-std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max());
  length.setValue(def->field_length);
  QHBoxLayout *typeLayout = new QHBoxLayout();
  QButtonGroup typeGroup(&dlg);
  QMap<int, QRadioButton *> btns;
  auto addBtn = [&](const QString &text, int id) {
    QRadioButton *b = new QRadioButton(text, &dlg);
    typeGroup.addButton(b, id);
    typeLayout->addWidget(b);
    btns[id] = b;
  };
  addBtn(tr("short"), SDDS_SHORT);
  addBtn(tr("ushort"), SDDS_USHORT);
  addBtn(tr("long"), SDDS_LONG);
  addBtn(tr("ulong"), SDDS_ULONG);
  addBtn(tr("long64"), SDDS_LONG64);
  addBtn(tr("ulong64"), SDDS_ULONG64);
  addBtn(tr("float"), SDDS_FLOAT);
  addBtn(tr("double"), SDDS_DOUBLE);
  addBtn(tr("long double"), SDDS_LONGDOUBLE);
  addBtn(tr("string"), SDDS_STRING);
  addBtn(tr("character"), SDDS_CHARACTER);
  if (btns.contains(def->type))
    btns[def->type]->setChecked(true);
  form.addRow(tr("Name"), &name);
  form.addRow(tr("Symbol"), &symbol);
  form.addRow(tr("Units"), &units);
  form.addRow(tr("Description"), &desc);
  form.addRow(tr("Format"), &fmt);
  form.addRow(tr("Field length"), &length);
  form.addRow(tr("Type"), typeLayout);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dlg);
  form.addRow(&buttons);
  connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted)
    return;
  if (!validateDefinitionName(this, name.text(), "column", col,
                              dataset.layout.n_columns,
                              [this](int i) {
                                return dataset.layout.column_definition[i].name;
                              }))
    return;
  if (!definitionTextEncodable(this, {symbol.text(), units.text(), desc.text(), fmt.text()}))
    return;
  if (!validateFieldLength(this, length.value(), typeGroup.checkedId()))
    return;
  if (!dataset.original_layout.column_definition ||
      col >= dataset.original_layout.n_columns) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Saved column layout is inconsistent"));
    return;
  }

  if (definitionTextMatches(def->name, name.text()) &&
      definitionTextMatches(def->symbol, symbol.text()) &&
      definitionTextMatches(def->units, units.text()) &&
      definitionTextMatches(def->description, desc.text()) &&
      definitionTextMatches(def->format_string, fmt.text()) &&
      def->field_length == length.value() && def->type == typeGroup.checkedId())
    return;

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  COLUMN_DEFINITION *savedDef = &dataset.original_layout.column_definition[col];
  auto replaceField = [&](char **workingField, char **savedField,
                          const QString &value,
                          bool nullWhenEmpty = true) -> bool {
    if (replaceSharedLayoutString(workingField, savedField, value,
                                  nullWhenEmpty))
      return true;
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Out of memory while updating column attributes"));
    restoreStructuralSnapshot(this, before);
    return false;
  };
  // Rewrite only edited text, as for parameters.
  auto replaceText = [&](char **workingField, char **savedField, const QString &value) {
    return definitionTextMatches(*workingField, value) ||
           replaceField(workingField, savedField, value);
  };

  if (!definitionTextMatches(def->name, name.text())) {
    if (!replaceField(&def->name, &savedDef->name, name.text(), false))
      return;
    if (!resyncSortedIndexName(dataset.layout.column_index,
                               dataset.layout.n_columns, col,
                               def->name) ||
        !resyncSortedIndexName(dataset.original_layout.column_index,
                               dataset.original_layout.n_columns, col,
                               savedDef->name)) {
      QMessageBox::warning(this, tr("SDDS"),
                           tr("Failed to update column name index"));
      restoreStructuralSnapshot(this, before);
      return;
    }
  }
  if (!replaceText(&def->symbol, &savedDef->symbol, symbol.text()))
    return;
  if (!replaceText(&def->units, &savedDef->units, units.text()))
    return;
  if (!replaceText(&def->description, &savedDef->description, desc.text()))
    return;
  if (!replaceText(&def->format_string, &savedDef->format_string, fmt.text()))
    return;
  def->field_length = savedDef->field_length = length.value();
  def->type = savedDef->type = typeGroup.checkedId();
  columnModel->refreshHeaders(col, col);
  columnView->viewport()->update(); // alignment and formatting follow the type
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            tr("Edit Column Attributes"));
}

void SDDSEditor::editArrayAttributes() {
  const QModelIndex idx = arrayView->currentIndex();
  if (idx.isValid())
    editArrayAttributesAt(idx.column());
}

/** Edit one array definition; reachable from its header even when empty. */
void SDDSEditor::editArrayAttributesAt(int col) {
  if (!datasetLoaded || col < 0 || col >= dataset.layout.n_arrays)
    return;
  commitModels();
  ARRAY_DEFINITION *def = &dataset.layout.array_definition[col];
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Array Attributes"));
  configureEditorPopupDialog(&dlg, this);
  QFormLayout form(&dlg);
  SDDSTextEdit name(QString::fromLocal8Bit(def->name ? def->name : ""), &dlg);
  SDDSTextEdit symbol(QString::fromLocal8Bit(def->symbol ? def->symbol : ""), &dlg);
  SDDSTextEdit units(QString::fromLocal8Bit(def->units ? def->units : ""), &dlg);
  SDDSTextEdit desc(QString::fromLocal8Bit(def->description ? def->description : ""), &dlg);
  SDDSTextEdit fmt(QString::fromLocal8Bit(def->format_string ? def->format_string : ""), &dlg);
  SDDSTextEdit group(QString::fromLocal8Bit(def->group_name ? def->group_name : ""), &dlg);
  QSpinBox length(&dlg);
  // Negative string field lengths are valid; clamping them would change the definition.
  length.setRange(-std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max());
  length.setValue(def->field_length);
  QSpinBox dimsCount(&dlg);
  dimsCount.setRange(1, 1000000);
  dimsCount.setValue(def->dimensions);
  QHBoxLayout *typeLayout = new QHBoxLayout();
  QButtonGroup typeGroup(&dlg);
  QMap<int, QRadioButton *> btns;
  auto addBtn = [&](const QString &text, int id) {
    QRadioButton *b = new QRadioButton(text, &dlg);
    typeGroup.addButton(b, id);
    typeLayout->addWidget(b);
    btns[id] = b;
  };
  addBtn(tr("short"), SDDS_SHORT);
  addBtn(tr("ushort"), SDDS_USHORT);
  addBtn(tr("long"), SDDS_LONG);
  addBtn(tr("ulong"), SDDS_ULONG);
  addBtn(tr("long64"), SDDS_LONG64);
  addBtn(tr("ulong64"), SDDS_ULONG64);
  addBtn(tr("float"), SDDS_FLOAT);
  addBtn(tr("double"), SDDS_DOUBLE);
  addBtn(tr("long double"), SDDS_LONGDOUBLE);
  addBtn(tr("string"), SDDS_STRING);
  addBtn(tr("character"), SDDS_CHARACTER);
  if (btns.contains(def->type))
    btns[def->type]->setChecked(true);
  form.addRow(tr("Name"), &name);
  form.addRow(tr("Symbol"), &symbol);
  form.addRow(tr("Units"), &units);
  form.addRow(tr("Description"), &desc);
  form.addRow(tr("Format"), &fmt);
  form.addRow(tr("Group"), &group);
  form.addRow(tr("Field length"), &length);
  form.addRow(tr("Dimensions"), &dimsCount);
  form.addRow(tr("Type"), typeLayout);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dlg);
  form.addRow(&buttons);
  connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted)
    return;
  if (!validateDefinitionName(this, name.text(), "array", col,
                              dataset.layout.n_arrays,
                              [this](int i) {
                                return dataset.layout.array_definition[i].name;
                              }))
    return;
  if (!definitionTextEncodable(this, {symbol.text(), units.text(), desc.text(), fmt.text(), group.text()}))
    return;
  if (!validateFieldLength(this, length.value(), typeGroup.checkedId()))
    return;
  int32_t dimCnt = dimsCount.value();
  for (const PageStore &pd : pages) {
    if (col >= pd.arrays.size())
      continue;
    QVector<int> newDims = pd.arrays[col].dims;
    int old = newDims.size();
    newDims.resize(dimCnt);
    for (int i = old; i < dimCnt; ++i)
      newDims[i] = 1;
    if (dimProduct(newDims) < 0) {
      QMessageBox::warning(this, tr("SDDS"),
                           tr("Array dimensions are too large."));
      return;
    }
  }
  if (!dataset.original_layout.array_definition ||
      col >= dataset.original_layout.n_arrays) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Saved array layout is inconsistent"));
    return;
  }

  if (definitionTextMatches(def->name, name.text()) &&
      definitionTextMatches(def->symbol, symbol.text()) &&
      definitionTextMatches(def->units, units.text()) &&
      definitionTextMatches(def->description, desc.text()) &&
      definitionTextMatches(def->format_string, fmt.text()) &&
      definitionTextMatches(def->group_name, group.text()) &&
      def->field_length == length.value() && def->dimensions == dimCnt &&
      def->type == typeGroup.checkedId())
    return;

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  ARRAY_DEFINITION *savedDef = &dataset.original_layout.array_definition[col];
  auto replaceField = [&](char **workingField, char **savedField,
                          const QString &value,
                          bool nullWhenEmpty = true) -> bool {
    if (replaceSharedLayoutString(workingField, savedField, value,
                                  nullWhenEmpty))
      return true;
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Out of memory while updating array attributes"));
    restoreStructuralSnapshot(this, before);
    return false;
  };
  // Rewrite only edited text, as for parameters.
  auto replaceText = [&](char **workingField, char **savedField, const QString &value) {
    return definitionTextMatches(*workingField, value) ||
           replaceField(workingField, savedField, value);
  };

  if (!definitionTextMatches(def->name, name.text())) {
    if (!replaceField(&def->name, &savedDef->name, name.text(), false))
      return;
    if (!resyncSortedIndexName(dataset.layout.array_index,
                               dataset.layout.n_arrays, col,
                               def->name) ||
        !resyncSortedIndexName(dataset.original_layout.array_index,
                               dataset.original_layout.n_arrays, col,
                               savedDef->name)) {
      QMessageBox::warning(this, tr("SDDS"),
                           tr("Failed to update array name index"));
      restoreStructuralSnapshot(this, before);
      return;
    }
  }
  if (!replaceText(&def->symbol, &savedDef->symbol, symbol.text()))
    return;
  if (!replaceText(&def->units, &savedDef->units, units.text()))
    return;
  if (!replaceText(&def->description, &savedDef->description, desc.text()))
    return;
  if (!replaceText(&def->format_string, &savedDef->format_string, fmt.text()))
    return;
  if (!replaceText(&def->group_name, &savedDef->group_name, group.text()))
    return;
  def->dimensions = savedDef->dimensions = dimCnt;
  for (PageStore &pd : pages) {
    if (col >= pd.arrays.size())
      continue;
    ArrayStore &as = pd.arrays[col];
    const int old = as.dims.size();
    if (old == dimCnt)
      continue;
    QVector<int> newDims = as.dims;
    newDims.resize(dimCnt);
    for (int i = old; i < dimCnt; ++i)
      newDims[i] = 1;
    // Keep each element at its indices, as Resize does; dropped dimensions keep index 0.
    const int newSize = dimProduct(newDims);
    if (newSize >= 0)
      as.values = reshapeArrayValues(as.values, as.dims, newDims, newSize);
    as.dims = newDims;
  }
  def->field_length = savedDef->field_length = length.value();
  def->type = savedDef->type = typeGroup.checkedId();
  arrayModel->refreshHeaders(col, col);
  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            tr("Edit Array Attributes"));
}
void SDDSEditor::changeParameterType(int row) {
  if (!datasetLoaded)
    return;
  commitModels();
  QStringList types;
  types << "short" << "ushort" << "long" << "ulong" << "long64"
      << "ulong64" << "float" << "double" << "longdouble" << "string"
        << "character";
  if (row < 0 || row >= dataset.layout.n_parameters)
    return;
  QString current = SDDS_GetTypeName(dataset.layout.parameter_definition[row].type);
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Parameter Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  if (sddsType <= 0) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Invalid type selection: %1").arg(newType));
    return;
  }
  if (!dataset.original_layout.parameter_definition ||
      row >= dataset.original_layout.n_parameters) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Saved parameter layout is inconsistent"));
    return;
  }
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  dataset.layout.parameter_definition[row].type = sddsType;
  dataset.original_layout.parameter_definition[row].type = sddsType;
  // Type affects validation/formatting and the Type column.
  paramModel->refreshMetadata(row, row);
  paramView->viewport()->update();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            tr("Change Parameter Type"));
}

void SDDSEditor::showParameterMenu(QTableView *view, int row,
                                   const QPoint &globalPos) {
  if (row < 0 || row >= dataset.layout.n_parameters)
    return;
  QVector<int> rowsForDelete = selectedRowsOrFallback(
      paramView, dataset.layout.n_parameters, row);
  if (!pendingParameterHeaderRows.isEmpty() &&
      pendingParameterHeaderRows.contains(row))
    rowsForDelete = pendingParameterHeaderRows;
  else if (!lastParameterSelectionRows.isEmpty() &&
           lastParameterSelectionRows.contains(row))
    rowsForDelete = lastParameterSelectionRows;
  pendingParameterHeaderRows.clear();

  QMenu menu(view);
  QAction *attrAct = menu.addAction(tr("Attributes..."));
  menu.addSeparator();
  QAction *delAct = menu.addAction(tr("Delete"));
  QAction *chosen = menu.exec(globalPos);
  if (chosen == attrAct) {
    paramView->setCurrentIndex(paramModel->index(row, ParameterPageModel::ValueColumn));
    editParameterAttributes();
  } else if (chosen == delAct)
    deleteParameterRows(rowsForDelete);
}

void SDDSEditor::parameterHeaderMenuRequested(const QPoint &pos) {
  int row = paramView->verticalHeader()->logicalIndexAt(pos);
  showParameterMenu(paramView, row,
                    paramView->verticalHeader()->mapToGlobal(pos));
}

void SDDSEditor::parameterCellMenuRequested(const QPoint &pos) {
  QModelIndex idx = paramView->indexAt(pos);
  if (!idx.isValid())
    return;
  pendingParameterHeaderRows.clear();
  showParameterMenu(paramView, idx.row(), paramView->viewport()->mapToGlobal(pos));
}

void SDDSEditor::changeColumnType(int column) {
  if (!datasetLoaded)
    return;
  commitModels();
  QStringList types;
  types << "short" << "ushort" << "long" << "ulong" << "long64"
      << "ulong64" << "float" << "double" << "longdouble" << "string"
        << "character";
  if (column < 0 || column >= dataset.layout.n_columns)
    return;
  QString current = SDDS_GetTypeName(dataset.layout.column_definition[column].type);
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Column Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  if (sddsType <= 0) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Invalid type selection: %1").arg(newType));
    return;
  }
  if (!dataset.original_layout.column_definition ||
      column >= dataset.original_layout.n_columns) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Saved column layout is inconsistent"));
    return;
  }
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  dataset.layout.column_definition[column].type = sddsType;
  dataset.original_layout.column_definition[column].type = sddsType;
  dataset.layout.column_definition[column].field_length =
      dataset.original_layout.column_definition[column].field_length =
          fieldLengthForType(dataset.layout.column_definition[column].field_length, sddsType);
  columnModel->refreshHeaders(column, column);
  columnView->viewport()->update(); // alignment and formatting follow the type
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            tr("Change Column Type"));
}

void SDDSEditor::showColumnMenu(QTableView *view, int column,
                                const QPoint &globalPos) {
  if (column < 0 || column >= dataset.layout.n_columns)
    return;
  QVector<int> columnsForDelete = selectedColumnsOrFallback(
      columnView, dataset.layout.n_columns, column);
  if (!pendingColumnHeaderColumns.isEmpty() &&
      pendingColumnHeaderColumns.contains(column))
    columnsForDelete = pendingColumnHeaderColumns;
  else if (!lastColumnSelectionColumns.isEmpty() &&
           lastColumnSelectionColumns.contains(column))
    columnsForDelete = lastColumnSelectionColumns;
  pendingColumnHeaderColumns.clear();

  QMenu menu(view);
  QAction *attrAct = menu.addAction(tr("Attributes..."));
  menu.addSeparator();
  QAction *plotAct = menu.addAction(tr("Plot from file"));
  QAction *ascAct = menu.addAction(tr("Sort ascending"));
  QAction *descAct = menu.addAction(tr("Sort descending"));
  QAction *searchAct = menu.addAction(tr("Search/Replace"));
  menu.addSeparator();
  QAction *filterAct = menu.addAction(tr("Filter/View..."));
  QAction *clearFilterAct = menu.addAction(tr("Clear Filter/View"));
  menu.addSeparator();
  QAction *fillSeriesAct = menu.addAction(tr("Fill Series..."));
  QAction *applyExprAct = menu.addAction(tr("Apply Numerical Expression..."));
  QAction *copyFormulaAct = menu.addAction(tr("Apply Text Formula..."));
  filterAct->setShortcut(QKeySequence(tr("Ctrl+Shift+R")));
  fillSeriesAct->setShortcut(QKeySequence(tr("Ctrl+Shift+F")));
  applyExprAct->setShortcut(QKeySequence(tr("Ctrl+Shift+E")));
  copyFormulaAct->setShortcut(QKeySequence(tr("Ctrl+Shift+M")));
  menu.addSeparator();
  QAction *delAct = menu.addAction(tr("Delete"));
  QAction *chosen = menu.exec(globalPos);
  if (chosen == attrAct)
    editColumnAttributesAt(column);
  else if (chosen == plotAct)
    plotColumn(column);
  else if (chosen == ascAct)
    sortColumn(column, Qt::AscendingOrder);
  else if (chosen == descAct)
    sortColumn(column, Qt::DescendingOrder);
  else if (chosen == searchAct)
    searchColumn(column);
  else if (chosen == filterAct) {
    columnView->setFocus();
    filterColumnRows();
  } else if (chosen == clearFilterAct)
    clearColumnRowFilter();
  // A header never takes focus, so name the table instead of using the focused one.
  else if (chosen == fillSeriesAct)
    fillSeries(columnView);
  else if (chosen == applyExprAct)
    applyNumericalExpression(columnView);
  else if (chosen == copyFormulaAct)
    applyTextFormula(columnView);
  else if (chosen == delAct)
    deleteColumnIndexes(columnsForDelete);
}

void SDDSEditor::columnHeaderMenuRequested(const QPoint &pos) {
  int column = columnView->horizontalHeader()->logicalIndexAt(pos);
  showColumnMenu(columnView, column,
                 columnView->horizontalHeader()->mapToGlobal(pos));
}

void SDDSEditor::columnCellMenuRequested(const QPoint &pos) {
  QModelIndex idx = columnView->indexAt(pos);
  if (!idx.isValid())
    return;
  pendingColumnHeaderColumns.clear();
  showColumnMenu(columnView, idx.column(),
                 columnView->viewport()->mapToGlobal(pos));
}

void SDDSEditor::columnRowMenuRequested(const QPoint &pos) {
  int row = columnView->verticalHeader()->logicalIndexAt(pos);
  if (row < 0)
    return;
  QMenu menu(columnView);
  QAction *insAct = menu.addAction(tr("Insert"));
  QAction *delAct = menu.addAction(tr("Delete"));
  menu.addSeparator();
  QAction *filterAct = menu.addAction(tr("Filter/View..."));
  QAction *clearFilterAct = menu.addAction(tr("Clear Filter/View"));
  menu.addSeparator();
  QAction *fillSeriesAct = menu.addAction(tr("Fill Series..."));
  QAction *applyExprAct = menu.addAction(tr("Apply Numerical Expression..."));
  QAction *copyFormulaAct = menu.addAction(tr("Apply Text Formula..."));
  filterAct->setShortcut(QKeySequence(tr("Ctrl+Shift+R")));
  fillSeriesAct->setShortcut(QKeySequence(tr("Ctrl+Shift+F")));
  applyExprAct->setShortcut(QKeySequence(tr("Ctrl+Shift+E")));
  copyFormulaAct->setShortcut(QKeySequence(tr("Ctrl+Shift+M")));
  QAction *chosen = menu.exec(columnView->verticalHeader()->mapToGlobal(pos));
  if (chosen == insAct) {
    columnView->setCurrentIndex(columnModel->index(row, 0));
    insertColumnRows();
  } else if (chosen == delAct) {
    QItemSelectionModel *sel = columnView->selectionModel();
    if (!sel->isRowSelected(row, QModelIndex()))
      sel->select(columnModel->index(row, 0),
                  QItemSelectionModel::Rows |
                      QItemSelectionModel::ClearAndSelect);
    deleteColumnRows();
  } else if (chosen == filterAct) {
    columnView->setFocus();
    filterColumnRows();
  } else if (chosen == clearFilterAct) {
    clearColumnRowFilter();
  } else if (chosen == fillSeriesAct) {
    fillSeries(columnView);
  } else if (chosen == applyExprAct) {
    applyNumericalExpression(columnView);
  } else if (chosen == copyFormulaAct) {
    applyTextFormula(columnView);
  }
}

void SDDSEditor::showArrayMenu(QTableView *view, int column,
                               const QPoint &globalPos) {
  if (column < 0 || column >= dataset.layout.n_arrays)
    return;
  QVector<int> arraysForDelete = selectedColumnsOrFallback(
      arrayView, dataset.layout.n_arrays, column);
  if (!pendingArrayHeaderColumns.isEmpty() &&
      pendingArrayHeaderColumns.contains(column))
    arraysForDelete = pendingArrayHeaderColumns;
  else if (!lastArraySelectionColumns.isEmpty() &&
           lastArraySelectionColumns.contains(column))
    arraysForDelete = lastArraySelectionColumns;
  pendingArrayHeaderColumns.clear();

  QMenu menu(view);
  QAction *attrAct = menu.addAction(tr("Attributes..."));
  QAction *viewerAct = menu.addAction(tr("Open Array Viewer..."));
  QAction *searchAct = menu.addAction(tr("Search"));
  QAction *resizeAct = menu.addAction(tr("Resize"));
  menu.addSeparator();
  QAction *fillSeriesAct = menu.addAction(tr("Fill Series..."));
  QAction *applyExprAct = menu.addAction(tr("Apply Numerical Expression..."));
  QAction *copyFormulaAct = menu.addAction(tr("Apply Text Formula..."));
  fillSeriesAct->setShortcut(QKeySequence(tr("Ctrl+Shift+F")));
  applyExprAct->setShortcut(QKeySequence(tr("Ctrl+Shift+E")));
  copyFormulaAct->setShortcut(QKeySequence(tr("Ctrl+Shift+M")));
  menu.addSeparator();
  QAction *delAct = menu.addAction(tr("Delete"));
  QAction *chosen = menu.exec(globalPos);
  if (chosen == attrAct)
    editArrayAttributesAt(column);
  else if (chosen == viewerAct)
    openArrayViewer(column);
  else if (chosen == searchAct)
    searchArray(column);
  else if (chosen == resizeAct)
    resizeArray(column);
  // A header never takes focus, so name the table instead of using the focused one.
  else if (chosen == fillSeriesAct)
    fillSeries(arrayView);
  else if (chosen == applyExprAct)
    applyNumericalExpression(arrayView);
  else if (chosen == copyFormulaAct)
    applyTextFormula(arrayView);
  else if (chosen == delAct)
    deleteArrayIndexes(arraysForDelete);
}

/** Open a slice viewer using the same array model and undo stack as the editor. */
void SDDSEditor::openArrayViewer(int column) {
  if (!datasetLoaded || column < 0 || column >= dataset.layout.n_arrays)
    return;
  flushPendingEdits();
  const QString name = QString::fromLocal8Bit(dataset.layout.array_definition[column].name);
  auto state = [this, name]() {
    ArrayViewerState result;
    result.name = name;
    result.page = currentPage;
    if (!datasetLoaded || currentPage < 0 || currentPage >= pages.size())
      return result;
    for (int i = 0; i < dataset.layout.n_arrays; ++i) {
      if (name != QString::fromLocal8Bit(dataset.layout.array_definition[i].name) || i >= pages[currentPage].arrays.size())
        continue;
      result.column = i;
      result.dimensions = pages[currentPage].arrays[i].dims;
      result.elements = pages[currentPage].arrays[i].values.size();
      result.numeric = SDDS_NUMERIC_TYPE(dataset.layout.array_definition[i].type);
      result.units = QString::fromLocal8Bit(dataset.layout.array_definition[i].units);
      break;
    }
    return result;
  };
  auto type = [this, state]() {
    const int index = state().column;
    return index >= 0 ? dataset.layout.array_definition[index].type : SDDS_STRING;
  };
  // The delegate canonicalizes typed text.  A paste stores its text as the main
  // tables do, so the viewer's unchanged-cell check (made before it opens an
  // undo macro) agrees with this edit; otherwise "1.50" over "1.5" would leave
  // an empty Undo step and discard Redo.
  ArrayViewer *viewer = new ArrayViewer(arrayModel, undoStack, state,
      [this](const QModelIndex &index, const QString &text) {
        return applyCellEditWithUndo(undoStack, arrayModel, index, text);
      },
      [type](const QString &text) { return validateTextForType(text, type(), false); },
      [this]() { flushPendingEdits(); }, this);
  // The slice model creates undo commands against flat source coordinates.
  viewer->table()->setItemDelegate(new SDDSItemDelegate(
      [type](const QModelIndex &) { return type(); }, nullptr, viewer->table(),
      [viewer]() { viewer->pasteText(QApplication::clipboard()->text()); }));
  QShortcut *save = new QShortcut(QKeySequence::Save, viewer);
  connect(save, &QShortcut::activated, this, &SDDSEditor::saveFile);
  arrayViewers.erase(std::remove_if(arrayViewers.begin(), arrayViewers.end(),
                                   [](const QPointer<QDialog> &v) { return v.isNull(); }), arrayViewers.end());
  arrayViewers.append(viewer);
  viewer->show();
}

void SDDSEditor::arrayHeaderMenuRequested(const QPoint &pos) {
  int column = arrayView->horizontalHeader()->logicalIndexAt(pos);
  showArrayMenu(arrayView, column,
                arrayView->horizontalHeader()->mapToGlobal(pos));
}

void SDDSEditor::arrayCellMenuRequested(const QPoint &pos) {
  QModelIndex idx = arrayView->indexAt(pos);
  if (!idx.isValid())
    return;
  pendingArrayHeaderColumns.clear();
  showArrayMenu(arrayView, idx.column(),
                arrayView->viewport()->mapToGlobal(pos));
}

void SDDSEditor::plotColumn(int column) {
  if (!datasetLoaded || column < 0 || column >= dataset.layout.n_columns)
    return;
  auto snapshot = std::make_shared<QTemporaryDir>();
  if (!snapshot->isValid() || !writeDatasetFile(snapshot->filePath("plot.sdds")))
    return;

  QString colName = QString::fromLocal8Bit(dataset.layout.column_definition[column].name);
  bool hasTime = false;
  for (int c = 0; c < dataset.layout.n_columns; ++c) {
    if (QString::fromLocal8Bit(dataset.layout.column_definition[c].name) == QLatin1String("Time")) {
      hasTime = true;
      break;
    }
  }

  // sddsplot treats *, ? and [...] in names as wildcards, so "Q[0]" would
  // match "Q0" instead; escaped, the name matches only itself.
  QString literalName;
  for (QChar ch : colName) {
    if (ch == '*' || ch == '?' || ch == '[' || ch == ']')
      literalName += '\\';
    literalName += ch;
  }

  QStringList args;
  args << "-split=page" << "-sep=page" << snapshot->filePath("plot.sdds");
  if (hasTime) {
    args << QString("-col=Time,%1").arg(literalName) << "-tick=xtime";
  } else {
    args << QString("-col=%1").arg(literalName);
  }

  QProcess *process = new QProcess(this);
  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          process, [process, snapshot](int, QProcess::ExitStatus) {
            process->deleteLater();
          });
  connect(process, &QProcess::errorOccurred, this,
          [this, process, snapshot](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
              QMessageBox::warning(this, tr("SDDS"), tr("Failed to start sddsplot"));
              process->deleteLater();
            }
          });
  process->start("sddsplot", args);
}

void SDDSEditor::sortColumn(int column, Qt::SortOrder order) {
  if (!datasetLoaded || currentPage < 0 || currentPage >= pages.size())
    return;

  commitModels();

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  PageStore &pd = pages[currentPage];
  if (column < 0 || column >= pd.columns.size())
    return;

  int rows = pd.columns[column].size();
  QVector<int> idx(rows);
  for (int i = 0; i < rows; ++i)
    idx[i] = i;

  int type = dataset.layout.column_definition[column].type;
  auto cmp = [&](int a, int b) {
    QString av = a < pd.columns[column].size() ? pd.columns[column][a] : QString();
    QString bv = b < pd.columns[column].size() ? pd.columns[column][b] : QString();
    if (SDDS_NUMERIC_TYPE(type)) {
      const QString at = av.trimmed();
      const QString bt = bv.trimmed();

      if (type == SDDS_LONG64 || type == SDDS_LONG || type == SDDS_SHORT) {
        bool aok = true;
        bool bok = true;
        qint64 ai = 0;
        qint64 bi = 0;
        if (!at.isEmpty())
          ai = at.toLongLong(&aok);
        if (!bt.isEmpty())
          bi = bt.toLongLong(&bok);
        if (type == SDDS_LONG) {
          if (aok && (ai < std::numeric_limits<int32_t>::min() || ai > std::numeric_limits<int32_t>::max()))
            aok = false;
          if (bok && (bi < std::numeric_limits<int32_t>::min() || bi > std::numeric_limits<int32_t>::max()))
            bok = false;
        } else if (type == SDDS_SHORT) {
          if (aok && (ai < std::numeric_limits<short>::min() || ai > std::numeric_limits<short>::max()))
            aok = false;
          if (bok && (bi < std::numeric_limits<short>::min() || bi > std::numeric_limits<short>::max()))
            bok = false;
        }
        if (aok != bok)
          return aok;
        if (aok && bok)
          return order == Qt::AscendingOrder ? ai < bi : ai > bi;
      } else if (type == SDDS_ULONG64 || type == SDDS_ULONG || type == SDDS_USHORT) {
        bool aok = true;
        bool bok = true;
        qulonglong ai = 0;
        qulonglong bi = 0;
        if (!at.isEmpty())
          ai = at.toULongLong(&aok);
        if (!bt.isEmpty())
          bi = bt.toULongLong(&bok);
        if (type == SDDS_ULONG) {
          if (aok && ai > std::numeric_limits<uint32_t>::max())
            aok = false;
          if (bok && bi > std::numeric_limits<uint32_t>::max())
            bok = false;
        } else if (type == SDDS_USHORT) {
          if (aok && ai > std::numeric_limits<unsigned short>::max())
            aok = false;
          if (bok && bi > std::numeric_limits<unsigned short>::max())
            bok = false;
        }
        if (aok != bok)
          return aok;
        if (aok && bok)
          return order == Qt::AscendingOrder ? ai < bi : ai > bi;
      } else {
        long double ai = 0.0L;
        long double bi = 0.0L;
        bool aok = true;
        bool bok = true;
        if (type == SDDS_LONGDOUBLE) {
          aok = parseLongDoubleStrict(at, &ai);
          bok = parseLongDoubleStrict(bt, &bi);
        } else if (type == SDDS_DOUBLE) {
          bool ok = true;
          ai = at.isEmpty() ? 0.0L : static_cast<long double>(at.toDouble(&ok));
          aok = ok;
          ok = true;
          bi = bt.isEmpty() ? 0.0L : static_cast<long double>(bt.toDouble(&ok));
          bok = ok;
        } else if (type == SDDS_FLOAT) {
          bool ok = true;
          ai = at.isEmpty() ? 0.0L : static_cast<long double>(at.toFloat(&ok));
          aok = ok;
          ok = true;
          bi = bt.isEmpty() ? 0.0L : static_cast<long double>(bt.toFloat(&ok));
          bok = ok;
        } else {
          bool ok = true;
          ai = at.isEmpty() ? 0.0L : static_cast<long double>(at.toDouble(&ok));
          aok = ok;
          ok = true;
          bi = bt.isEmpty() ? 0.0L : static_cast<long double>(bt.toDouble(&ok));
          bok = ok;
        }
        // NaN compares unequal to everything, which would break the sort's
        // ordering; place NaN after all numbers in either direction.
        if (aok && bok && (std::isnan(ai) || std::isnan(bi))) {
          if (std::isnan(ai) != std::isnan(bi))
            return std::isnan(bi);
          return false;
        }
        if (aok != bok)
          return aok;
        if (aok && bok)
          return order == Qt::AscendingOrder ? ai < bi : ai > bi;
      }

      QByteArray aa = at.toUtf8();
      QByteArray bb = bt.toUtf8();
      int r = strcmp_nh(aa.constData(), bb.constData());
      return order == Qt::AscendingOrder ? r < 0 : r > 0;
    } else if (type == SDDS_STRING) {
      QByteArray aa = av.toUtf8();
      QByteArray bb = bv.toUtf8();
      int r = strcmp_nh(aa.constData(), bb.constData());
      return order == Qt::AscendingOrder ? r < 0 : r > 0;
    }
    return order == Qt::AscendingOrder ? av < bv : av > bv;
  };

  std::stable_sort(idx.begin(), idx.end(), cmp);

  bool reordered = false;
  for (int i = 0; i < rows; ++i)
    reordered = reordered || idx[i] != i;
  if (!reordered)
    return; // A no-op must not push an undo command and discard pending Redo.

  for (int c = 0; c < pd.columns.size(); ++c) {
    QVector<QString> sorted(rows);
    for (int i = 0; i < rows; ++i)
      sorted[i] = idx[i] < pd.columns[c].size() ? pd.columns[c][idx[i]] : QString();
    pd.columns[c] = sorted;
  }

  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Sort Rows"));
}

void SDDSEditor::searchColumn(int column) {
  if (!datasetLoaded)
    return;
  if (column < 0 || column >= dataset.layout.n_columns)
    return;
  flushPendingEdits();

  if (searchColumnDialog)
    searchColumnDialog->close();

  QDialog *dlg = new QDialog(this);
  searchColumnDialog = dlg;
  dlg->setWindowTitle(tr("Search Column"));
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  configureEditorPopupDialog(dlg, this, Qt::NonModal);
  connect(this, &QObject::destroyed, dlg, &QObject::deleteLater);

  QVBoxLayout *layout = new QVBoxLayout(dlg);
  QFormLayout *form = new QFormLayout();
  layout->addLayout(form);

  QLineEdit *patternEdit = new SDDSTextEdit(dlg);
  patternEdit->setText(lastSearchPattern);
  QLineEdit *replaceEdit = new SDDSTextEdit(dlg);
  replaceEdit->setText(lastReplaceText);
  form->addRow(tr("Find"), patternEdit);
  form->addRow(tr("Replace With"), replaceEdit);

  QHBoxLayout *btnLayout = new QHBoxLayout();
  QPushButton *searchBtn = new QPushButton(tr("Search"), dlg);
  QPushButton *replaceBtn = new QPushButton(tr("Replace"), dlg);
  QPushButton *replaceSelectedBtn = new QPushButton(tr("Replace Selected"), dlg);
  QPushButton *replaceAllBtn = new QPushButton(tr("Replace All"), dlg);
  QPushButton *prevBtn = new QPushButton(tr("Previous"), dlg);
  QPushButton *nextBtn = new QPushButton(tr("Next"), dlg);
  QPushButton *closeBtn = new QPushButton(tr("Close"), dlg);
  btnLayout->addWidget(searchBtn);
  btnLayout->addWidget(replaceBtn);
  btnLayout->addWidget(replaceSelectedBtn);
  btnLayout->addWidget(replaceAllBtn);
  btnLayout->addWidget(prevBtn);
  btnLayout->addWidget(nextBtn);
  btnLayout->addWidget(closeBtn);
  layout->addLayout(btnLayout);

  struct Match {
    int row;
    int start;
  };

  struct SearchState {
    QVector<Match> matches;
    int matchIndex;
    QPersistentModelIndex activeEditor;
  };

  auto state = std::make_shared<SearchState>();
  state->matchIndex = -1;
  const int columnType = dataset.layout.column_definition[column].type;
  auto invalidateMatches = [state]() {
    state->matches.clear();
    state->matchIndex = -1;
  };
  connect(patternEdit, &QLineEdit::textChanged, dlg, invalidateMatches);
  connect(columnModel, &QAbstractItemModel::dataChanged, dlg, invalidateMatches);
  connect(this, &SDDSEditor::columnRowVisibilityChanged, dlg, invalidateMatches);
  // A model/header change may replace the page, column identity, or data type.
  connect(columnModel, &QAbstractItemModel::modelReset, dlg, &QDialog::close);
  connect(columnModel, &QAbstractItemModel::headerDataChanged, dlg, &QDialog::close);

  std::function<void()> focusMatch;
  std::function<void(bool, bool)> runSearch;

  focusMatch = [this, column, patternEdit, state]() {
    if (state->matchIndex < 0 || state->matchIndex >= state->matches.size())
      return;
    if (state->activeEditor.isValid())
      columnView->closePersistentEditor(state->activeEditor);
    QModelIndex idx = columnModel->index(state->matches[state->matchIndex].row, column);
    if (!idx.isValid())
      return;
    columnView->setCurrentIndex(idx);
    columnView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    columnView->openPersistentEditor(idx);
    if (QWidget *w = columnView->indexWidget(idx)) {
      if (QLineEdit *line = qobject_cast<QLineEdit *>(w))
        line->setSelection(state->matches[state->matchIndex].start, patternEdit->text().length());
    }
    state->activeEditor = idx;
  };

  runSearch = [this, column, dlg, patternEdit, replaceEdit, state, focusMatch](bool showInfo, bool refocus) {
    flushPendingEdits();
    QString pat = patternEdit->text();
    state->matches.clear();
    state->matchIndex = -1;
    if (state->activeEditor.isValid()) {
      columnView->closePersistentEditor(state->activeEditor);
      state->activeEditor = QModelIndex();
    }
    if (pat.isEmpty())
      return;
    lastSearchPattern = pat;
    lastReplaceText = replaceEdit->text();
    for (int r = 0; r < columnModel->rowCount(); ++r) {
      QModelIndex idx = columnModel->index(r, column);
      if (!idx.isValid() || columnView->isRowHidden(r))
        continue;
      QString val = idx.data(Qt::EditRole).toString();
      int pos = 0;
      while ((pos = val.indexOf(pat, pos, Qt::CaseSensitive)) >= 0) {
        state->matches.append({r, pos});
        pos += pat.length();
      }
    }
    if (!state->matches.isEmpty()) {
      state->matchIndex = 0;
      if (refocus)
        focusMatch();
    } else if (showInfo) {
      QMessageBox::information(dlg, tr("Search"), tr("No matches found"));
    }
  };

  auto replaceCurrent = [this, column, patternEdit, replaceEdit, state, runSearch, columnType]() {
    flushPendingEdits();
    if (state->matches.isEmpty())
      runSearch(true, true);
    if (state->matches.isEmpty())
      return;
    if (state->matchIndex < 0 || state->matchIndex >= state->matches.size())
      return;
    Match m = state->matches[state->matchIndex];
    QModelIndex idx = columnModel->index(m.row, column);
    if (!idx.isValid())
      return;
    QString val = idx.data(Qt::EditRole).toString();
    const QString pattern = patternEdit->text();
    if (pattern.isEmpty() || val.mid(m.start, pattern.size()) != pattern) {
      runSearch(true, true);
      return;
    }
    val.replace(m.start, pattern.size(), replaceEdit->text());
    if (!validateTextForType(val, columnType, true))
      return;
    if (applyCellEditWithUndo(undoStack, columnModel, idx, val))
      markDirty();
    runSearch(true, true);
  };

  auto replaceAll = [this, column, patternEdit, replaceEdit, state, runSearch, columnType]() {
    flushPendingEdits();
    if (state->matches.isEmpty())
      runSearch(true, true);
    if (state->matches.isEmpty())
      return;
    QString pat = patternEdit->text();
    if (pat.isEmpty())
      return;
    QString repl = replaceEdit->text();
    int replaced = 0;
    bool warned = false;
    bool macroStarted = false;
    // Edits can change the filter. Keep the target rows fixed for this operation.
    QVector<int> targetRows;
    for (int r = 0; r < columnModel->rowCount(); ++r)
      if (!columnView->isRowHidden(r))
        targetRows.append(r);
    for (int r : targetRows) {
      QModelIndex idx = columnModel->index(r, column);
      if (!idx.isValid())
        continue;
      QString val = idx.data(Qt::EditRole).toString();
      int pos = 0;
      bool changed = false;
      int rowReplaced = 0;
      while ((pos = val.indexOf(pat, pos, Qt::CaseSensitive)) >= 0) {
        val.replace(pos, pat.length(), repl);
        pos += repl.length();
        ++rowReplaced;
        changed = true;
      }
      if (changed) {
        if (val == idx.data(Qt::EditRole).toString())
          continue;
        bool show = !warned;
        if (validateTextForType(val, columnType, show)) {
          if (undoStack && !macroStarted) {
            undoStack->beginMacro(tr("Replace All"));
            macroStarted = true;
          }
          if (applyCellEditWithUndo(undoStack, columnModel, idx, val))
            replaced += rowReplaced;
        } else if (show) {
          warned = true;
        }
      }
    }
    if (macroStarted)
      undoStack->endMacro();
    if (replaced > 0)
      markDirty();
    runSearch(replaced == 0, true);
  };

  auto replaceSelected = [this, column, patternEdit, replaceEdit, state, runSearch, columnType]() {
    flushPendingEdits();
    QString pat = patternEdit->text();
    if (pat.isEmpty())
      return;
    QModelIndexList indexes = visibleSelectedIndexes(columnView);
    if (indexes.isEmpty())
      return;
    QString repl = replaceEdit->text();
    int replaced = 0;
    bool warned = false;
    bool macroStarted = false;
    for (const QModelIndex &idx : indexes) {
      if (!idx.isValid() || idx.column() != column)
        continue;
      QString val = idx.data(Qt::EditRole).toString();
      int pos = 0;
      bool changed = false;
      int rowReplaced = 0;
      while ((pos = val.indexOf(pat, pos, Qt::CaseSensitive)) >= 0) {
        val.replace(pos, pat.length(), repl);
        pos += repl.length();
        ++rowReplaced;
        changed = true;
      }
      if (changed) {
        if (val == idx.data(Qt::EditRole).toString())
          continue;
        bool show = !warned;
        if (validateTextForType(val, columnType, show)) {
          if (undoStack && !macroStarted) {
            undoStack->beginMacro(tr("Replace Selected"));
            macroStarted = true;
          }
          if (applyCellEditWithUndo(undoStack, columnModel, idx, val))
            replaced += rowReplaced;
        } else if (show) {
          warned = true;
        }
      }
    }
    if (macroStarted)
      undoStack->endMacro();
    if (replaced > 0)
      markDirty();
    runSearch(replaced == 0, false);
  };

  QObject::connect(searchBtn, &QPushButton::clicked, dlg, [runSearch]() { runSearch(true, true); });
  QObject::connect(replaceBtn, &QPushButton::clicked, dlg, replaceCurrent);
  QObject::connect(replaceSelectedBtn, &QPushButton::clicked, dlg, replaceSelected);
  QObject::connect(replaceAllBtn, &QPushButton::clicked, dlg, replaceAll);
  QObject::connect(nextBtn, &QPushButton::clicked, dlg, [this, state, focusMatch, runSearch]() {
    flushPendingEdits();
    if (state->matches.isEmpty()) {
      runSearch(true, true);
      return;
    }
    state->matchIndex = (state->matchIndex + 1) % state->matches.size();
    focusMatch();
  });
  QObject::connect(prevBtn, &QPushButton::clicked, dlg, [this, state, focusMatch, runSearch]() {
    flushPendingEdits();
    if (state->matches.isEmpty()) {
      runSearch(true, false);
      if (state->matches.isEmpty())
        return;
      state->matchIndex = 0;
    }
    state->matchIndex = (state->matchIndex - 1 + state->matches.size()) % state->matches.size();
    focusMatch();
  });
  QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
  connect(dlg, &QDialog::finished, this, [this, state]() {
    flushPendingEdits();
    if (state->activeEditor.isValid())
      columnView->closePersistentEditor(state->activeEditor);
    state->activeEditor = QModelIndex();
  });

  QObject::connect(dlg, &QDialog::destroyed, this, [this, state, dlg]() {
    if (state->activeEditor.isValid())
      columnView->closePersistentEditor(state->activeEditor);
    if (searchColumnDialog == dlg)
      searchColumnDialog = nullptr;
  });

  dlg->show();
  dlg->raise();
  dlg->activateWindow();
}

void SDDSEditor::resizeArray(int column) {
  if (!datasetLoaded || currentPage < 0 || currentPage >= pages.size())
    return;
  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  PageStore &pd = pages[currentPage];
  if (column < 0 || column >= pd.arrays.size())
    return;
  if (column >= dataset.layout.n_arrays)
    return;

  ARRAY_DEFINITION *def = &dataset.layout.array_definition[column];

  ArrayStore &as = pd.arrays[column];

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Resize Array"));
  configureEditorPopupDialog(&dlg, this);
  QFormLayout form(&dlg);
  QVector<QSpinBox *> boxes(def->dimensions);
  for (int i = 0; i < def->dimensions; ++i) {
    QSpinBox *sb = new QSpinBox(&dlg);
    // A lower cap would silently shrink larger loaded dimensions on OK; dimProduct checks the total.
    sb->setRange(0, std::numeric_limits<int>::max());
    sb->setValue(i < as.dims.size() ? as.dims[i] : 1);
    form.addRow(tr("Dim %1").arg(i + 1), sb);
    boxes[i] = sb;
  }

  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dlg);
  form.addRow(&buttons);
  connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted)
    return;

  QVector<int> newDims = as.dims;
  newDims.resize(def->dimensions);
  for (int i = 0; i < def->dimensions; ++i)
    newDims[i] = boxes[i]->value();
  int newSize = dimProduct(newDims);
  if (newSize < 0) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Array dimensions are too large."));
    return;
  }
  if (newDims == as.dims && newSize == as.values.size())
    return; // Keep history and the saved state when the shape is unchanged.
  as.values = reshapeArrayValues(as.values, as.dims, newDims, newSize);
  as.dims = newDims;

  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Resize Array"));
}

void SDDSEditor::searchArray(int column) {
  if (!datasetLoaded)
    return;
  if (column < 0 || column >= dataset.layout.n_arrays)
    return;
  flushPendingEdits();

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Search Array"));
  configureEditorPopupDialog(&dlg, this);
  QVBoxLayout layout(&dlg);
  QFormLayout form;
  SDDSTextEdit patternEdit(&dlg);
  patternEdit.setText(lastSearchPattern);
  SDDSTextEdit replaceEdit(&dlg);
  replaceEdit.setText(lastReplaceText);
  form.addRow(tr("Find"), &patternEdit);
  form.addRow(tr("Replace With"), &replaceEdit);
  layout.addLayout(&form);
  QHBoxLayout btnLayout;
  QPushButton searchBtn(tr("Search"), &dlg);
  QPushButton replaceBtn(tr("Replace"), &dlg);
  QPushButton replaceAllBtn(tr("Replace All"), &dlg);
  QPushButton prevBtn(tr("Previous"), &dlg);
  QPushButton nextBtn(tr("Next"), &dlg);
  QPushButton closeBtn(tr("Close"), &dlg);
  btnLayout.addWidget(&searchBtn);
  btnLayout.addWidget(&replaceBtn);
  btnLayout.addWidget(&replaceAllBtn);
  btnLayout.addWidget(&prevBtn);
  btnLayout.addWidget(&nextBtn);
  btnLayout.addWidget(&closeBtn);
  layout.addLayout(&btnLayout);

  struct Match { int row; int start; };
  QVector<Match> matches;
  int matchIndex = -1;
  QPersistentModelIndex activeEditor;
  const int arrayType = dataset.layout.array_definition[column].type;
  auto invalidateMatches = [&]() {
    matches.clear();
    matchIndex = -1;
  };
  connect(&patternEdit, &QLineEdit::textChanged, &dlg, invalidateMatches);
  connect(arrayModel, &QAbstractItemModel::dataChanged, &dlg, invalidateMatches);

  auto focusMatch = [&]() {
    if (matchIndex < 0 || matchIndex >= matches.size())
      return;
    if (activeEditor.isValid())
      arrayView->closePersistentEditor(activeEditor);
    QModelIndex idx = arrayModel->index(matches[matchIndex].row, column);
    arrayView->setCurrentIndex(idx);
    arrayView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    arrayView->openPersistentEditor(idx);
    if (QWidget *w = arrayView->indexWidget(idx)) {
      if (QLineEdit *line = qobject_cast<QLineEdit *>(w))
        line->setSelection(matches[matchIndex].start, patternEdit.text().length());
    }
    activeEditor = idx;
  };

  auto runSearch = [&](bool showInfo) {
    flushPendingEdits();
    QString pat = patternEdit.text();
    matches.clear();
    matchIndex = -1;
    if (activeEditor.isValid()) {
      arrayView->closePersistentEditor(activeEditor);
      activeEditor = QModelIndex();
    }
    if (pat.isEmpty())
      return;
    lastSearchPattern = pat;
    lastReplaceText = replaceEdit.text();
    for (int r = 0; r < arrayModel->rowCount(); ++r) {
      QString val;
      QModelIndex idx = arrayModel->index(r, column);
      if (idx.isValid())
        val = idx.data(Qt::EditRole).toString();
      int pos = 0;
      while ((pos = val.indexOf(pat, pos, Qt::CaseSensitive)) >= 0) {
        matches.append({r, pos});
        pos += pat.length();
      }
    }
    if (!matches.isEmpty()) {
      matchIndex = 0;
      focusMatch();
    } else if (showInfo) {
      QMessageBox::information(&dlg, tr("Search"), tr("No matches found"));
    }
  };

  auto replaceCurrent = [&]() {
    flushPendingEdits();
    if (matches.isEmpty())
      runSearch(true);
    if (matches.isEmpty())
      return;
    if (matchIndex < 0 || matchIndex >= matches.size())
      return;
    Match m = matches[matchIndex];
    QModelIndex idx = arrayModel->index(m.row, column);
    if (!idx.isValid())
      return;
    QString val = idx.data(Qt::EditRole).toString();
    const QString pattern = patternEdit.text();
    if (pattern.isEmpty() || val.mid(m.start, pattern.size()) != pattern) {
      runSearch(true);
      return;
    }
    val.replace(m.start, pattern.size(), replaceEdit.text());
    if (!validateTextForType(val, arrayType, true))
      return;
    if (applyCellEditWithUndo(undoStack, arrayModel, idx, val))
      markDirty();
    runSearch(true);
  };

  auto replaceAll = [&]() {
    flushPendingEdits();
    if (matches.isEmpty())
      runSearch(true);
    if (matches.isEmpty())
      return;
    QString pat = patternEdit.text();
    if (pat.isEmpty())
      return;
    QString repl = replaceEdit.text();
    int replaced = 0;
    bool warned = false;
    bool macroStarted = false;
    for (int r = 0; r < arrayModel->rowCount(); ++r) {
      QModelIndex idx = arrayModel->index(r, column);
      if (!idx.isValid())
        continue;
      QString val = idx.data(Qt::EditRole).toString();
      int pos = 0;
      bool changed = false;
      int rowReplaced = 0;
      while ((pos = val.indexOf(pat, pos, Qt::CaseSensitive)) >= 0) {
        val.replace(pos, pat.length(), repl);
        pos += repl.length();
        ++rowReplaced;
        changed = true;
      }
      if (changed) {
        if (val == idx.data(Qt::EditRole).toString())
          continue;
        bool show = !warned;
        if (validateTextForType(val, arrayType, show)) {
          if (undoStack && !macroStarted) {
            undoStack->beginMacro(tr("Replace All"));
            macroStarted = true;
          }
          if (applyCellEditWithUndo(undoStack, arrayModel, idx, val))
            replaced += rowReplaced;
        } else if (show) {
          warned = true;
        }
      }
    }
    if (macroStarted)
      undoStack->endMacro();
    if (replaced > 0)
      markDirty();
    runSearch(replaced == 0);
  };

  QObject::connect(&searchBtn, &QPushButton::clicked, [&]() { runSearch(true); });
  QObject::connect(&replaceBtn, &QPushButton::clicked, replaceCurrent);
  QObject::connect(&replaceAllBtn, &QPushButton::clicked, replaceAll);
  QObject::connect(&nextBtn, &QPushButton::clicked, [&]() {
    flushPendingEdits();
    if (matches.isEmpty()) {
      runSearch(true);
      return;
    }
    matchIndex = (matchIndex + 1) % matches.size();
    focusMatch();
  });
  QObject::connect(&prevBtn, &QPushButton::clicked, [&]() {
    flushPendingEdits();
    if (matches.isEmpty()) {
      runSearch(true);
      if (matches.isEmpty())
        return;
    }
    matchIndex = (matchIndex - 1 + matches.size()) % matches.size();
    focusMatch();
  });
  QObject::connect(&closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
  connect(&dlg, &QDialog::finished, &dlg, [&]() { flushPendingEdits(); });

  dlg.exec();
  if (activeEditor.isValid())
    arrayView->closePersistentEditor(activeEditor);
}

void SDDSEditor::changeArrayType(int column) {
  if (!datasetLoaded)
    return;
  commitModels();
  QStringList types;
  types << "short" << "ushort" << "long" << "ulong" << "long64"
      << "ulong64" << "float" << "double" << "longdouble" << "string"
        << "character";
  if (column < 0 || column >= dataset.layout.n_arrays)
    return;
  QString current = SDDS_GetTypeName(dataset.layout.array_definition[column].type);
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Array Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  if (sddsType <= 0) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Invalid type selection: %1").arg(newType));
    return;
  }
  if (!dataset.original_layout.array_definition ||
      column >= dataset.original_layout.n_arrays) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Saved array layout is inconsistent"));
    return;
  }
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  dataset.layout.array_definition[column].type = sddsType;
  dataset.original_layout.array_definition[column].type = sddsType;
  dataset.layout.array_definition[column].field_length =
      dataset.original_layout.array_definition[column].field_length =
          fieldLengthForType(dataset.layout.array_definition[column].field_length, sddsType);
  arrayModel->refreshHeaders(column, column);
  arrayView->viewport()->update(); // alignment and formatting follow the type
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            tr("Change Array Type"));
}

static void removeParameterFromLayout(SDDS_LAYOUT *layout, int row) {
  PARAMETER_DEFINITION *defs = layout->parameter_definition;
  SORTED_INDEX **indexes = layout->parameter_index;
  int count = layout->n_parameters;

  int k = -1;
  for (int i = 0; i < count; ++i) {
    if (indexes[i]->index == row) {
      k = i;
      break;
    }
  }

  if (defs[row].name)
    free(defs[row].name);
  if (defs[row].symbol)
    free(defs[row].symbol);
  if (defs[row].units)
    free(defs[row].units);
  if (defs[row].description)
    free(defs[row].description);
  if (defs[row].format_string)
    free(defs[row].format_string);
  if (defs[row].fixed_value)
    free(defs[row].fixed_value);

  for (int i = row + 1; i < count; ++i)
    defs[i - 1] = defs[i];

  if (count - 1 > 0)
  {
    PARAMETER_DEFINITION *newDefs =
        (PARAMETER_DEFINITION *)realloc(defs, sizeof(PARAMETER_DEFINITION) * (count - 1));
    if (newDefs)
      layout->parameter_definition = newDefs;
    else
      layout->parameter_definition = defs;
  }
  else {
    free(defs);
    layout->parameter_definition = nullptr;
  }

  if (k >= 0) {
    free(indexes[k]);
    for (int i = k + 1; i < count; ++i)
      indexes[i - 1] = indexes[i];
  }
  for (int i = 0; i < count - 1; ++i)
    if (indexes[i]->index > row)
      indexes[i]->index--;

  if (count - 1 > 0)
  {
    SORTED_INDEX **newIndexes =
        (SORTED_INDEX **)realloc(indexes, sizeof(SORTED_INDEX *) * (count - 1));
    if (newIndexes)
      layout->parameter_index = newIndexes;
    else
      layout->parameter_index = indexes;
  }
  else {
    free(indexes);
    layout->parameter_index = nullptr;
  }

  layout->n_parameters = count - 1;
}

static void removeColumnFromLayout(SDDS_LAYOUT *layout, int col) {
  COLUMN_DEFINITION *defs = layout->column_definition;
  SORTED_INDEX **indexes = layout->column_index;
  int count = layout->n_columns;

  int k = -1;
  for (int i = 0; i < count; ++i) {
    if (indexes[i]->index == col) {
      k = i;
      break;
    }
  }

  if (defs[col].name)
    free(defs[col].name);
  if (defs[col].symbol)
    free(defs[col].symbol);
  if (defs[col].units)
    free(defs[col].units);
  if (defs[col].description)
    free(defs[col].description);
  if (defs[col].format_string)
    free(defs[col].format_string);

  for (int i = col + 1; i < count; ++i)
    defs[i - 1] = defs[i];

  if (count - 1 > 0)
  {
    COLUMN_DEFINITION *newDefs =
        (COLUMN_DEFINITION *)realloc(defs, sizeof(COLUMN_DEFINITION) * (count - 1));
    if (newDefs)
      layout->column_definition = newDefs;
    else
      layout->column_definition = defs;
  }
  else {
    free(defs);
    layout->column_definition = nullptr;
  }

  if (k >= 0) {
    free(indexes[k]);
    for (int i = k + 1; i < count; ++i)
      indexes[i - 1] = indexes[i];
  }
  for (int i = 0; i < count - 1; ++i)
    if (indexes[i]->index > col)
      indexes[i]->index--;

  if (count - 1 > 0)
  {
    SORTED_INDEX **newIndexes =
        (SORTED_INDEX **)realloc(indexes, sizeof(SORTED_INDEX *) * (count - 1));
    if (newIndexes)
      layout->column_index = newIndexes;
    else
      layout->column_index = indexes;
  }
  else {
    free(indexes);
    layout->column_index = nullptr;
  }

  layout->n_columns = count - 1;
}

static void removeArrayFromLayout(SDDS_LAYOUT *layout, int col) {
  ARRAY_DEFINITION *defs = layout->array_definition;
  SORTED_INDEX **indexes = layout->array_index;
  int count = layout->n_arrays;

  int k = -1;
  for (int i = 0; i < count; ++i) {
    if (indexes[i]->index == col) {
      k = i;
      break;
    }
  }

  if (defs[col].name)
    free(defs[col].name);
  if (defs[col].symbol)
    free(defs[col].symbol);
  if (defs[col].units)
    free(defs[col].units);
  if (defs[col].description)
    free(defs[col].description);
  if (defs[col].format_string)
    free(defs[col].format_string);
  if (defs[col].group_name)
    free(defs[col].group_name);

  for (int i = col + 1; i < count; ++i)
    defs[i - 1] = defs[i];

  if (count - 1 > 0)
  {
    ARRAY_DEFINITION *newDefs =
        (ARRAY_DEFINITION *)realloc(defs, sizeof(ARRAY_DEFINITION) * (count - 1));
    if (newDefs)
      layout->array_definition = newDefs;
    else
      layout->array_definition = defs;
  }
  else {
    free(defs);
    layout->array_definition = nullptr;
  }

  if (k >= 0) {
    free(indexes[k]);
    for (int i = k + 1; i < count; ++i)
      indexes[i - 1] = indexes[i];
  }
  for (int i = 0; i < count - 1; ++i)
    if (indexes[i]->index > col)
      indexes[i]->index--;

  if (count - 1 > 0)
  {
    SORTED_INDEX **newIndexes =
        (SORTED_INDEX **)realloc(indexes, sizeof(SORTED_INDEX *) * (count - 1));
    if (newIndexes)
      layout->array_index = newIndexes;
    else
      layout->array_index = indexes;
  }
  else {
    free(indexes);
    layout->array_index = nullptr;
  }

  layout->n_arrays = count - 1;
}

void SDDSEditor::insertParameter() {
  if (!ensureDataset())
    return;

  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("New Parameter"));
  configureEditorPopupDialog(&dlg, this);
  QFormLayout form(&dlg);
  SDDSTextEdit name(&dlg);
  SDDSTextEdit symbol(&dlg);
  SDDSTextEdit units(&dlg);
  SDDSTextEdit desc(&dlg);
  SDDSTextEdit fmt(&dlg);
  SDDSTextEdit fixed(&dlg);
  QHBoxLayout *typeLayout = new QHBoxLayout();
  QButtonGroup typeGroup(&dlg);
  QMap<int, QRadioButton *> btns;
  auto addBtn = [&](const QString &text, int id) {
    QRadioButton *b = new QRadioButton(text, &dlg);
    typeGroup.addButton(b, id);
    typeLayout->addWidget(b);
    btns[id] = b;
  };
  addBtn(tr("short"), SDDS_SHORT);
  addBtn(tr("ushort"), SDDS_USHORT);
  addBtn(tr("long"), SDDS_LONG);
  addBtn(tr("ulong"), SDDS_ULONG);
  addBtn(tr("long64"), SDDS_LONG64);
  addBtn(tr("ulong64"), SDDS_ULONG64);
  addBtn(tr("float"), SDDS_FLOAT);
  addBtn(tr("double"), SDDS_DOUBLE);
  addBtn(tr("long double"), SDDS_LONGDOUBLE);
  addBtn(tr("string"), SDDS_STRING);
  addBtn(tr("character"), SDDS_CHARACTER);
  if (btns.contains(SDDS_STRING))
    btns[SDDS_STRING]->setChecked(true);
  form.addRow(tr("Name"), &name);
  form.addRow(tr("Symbol"), &symbol);
  form.addRow(tr("Units"), &units);
  form.addRow(tr("Description"), &desc);
  form.addRow(tr("Format"), &fmt);
  form.addRow(tr("Fixed value"), &fixed);
  form.addRow(tr("Type"), typeLayout);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dlg);
  form.addRow(&buttons);
  connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted || name.text().isEmpty())
    return;
  if (!validateDefinitionName(this, name.text(), "parameter", -1, dataset.layout.n_parameters,
                              [this](int i) { return dataset.layout.parameter_definition[i].name; }))
    return;
  if (!definitionTextEncodable(this, {symbol.text(), units.text(), desc.text(), fmt.text()}))
    return;
  // Same check as the attribute editor; otherwise the file cannot be saved later.
  if (!fixed.text().isEmpty() &&
      !validateTextForType(fixed.text(), typeGroup.checkedId(), false)) {
    QMessageBox::warning(this, tr("SDDS"),
                         tr("Fixed value is invalid for type %1")
                             .arg(QString::fromLocal8Bit(SDDS_GetTypeName(typeGroup.checkedId()))));
    return;
  }

  QByteArray baName = name.text().toLocal8Bit();
  QByteArray baSym = symbol.text().toLocal8Bit();
  QByteArray baUnits = units.text().toLocal8Bit();
  QByteArray baDesc = desc.text().toLocal8Bit();
  QByteArray baFmt = fmt.text().toLocal8Bit();
  QByteArray baFixed = fixedValueForDefinition(fixed.text(), typeGroup.checkedId()).toLocal8Bit();

  if (SDDS_DefineParameter(&dataset, baName.constData(),
                           symbol.text().isEmpty() ? NULL : baSym.constData(),
                           units.text().isEmpty() ? NULL : baUnits.constData(),
                           desc.text().isEmpty() ? NULL : baDesc.constData(),
                           fmt.text().isEmpty() ? NULL : baFmt.constData(),
                           typeGroup.checkedId(),
                           fixed.text().isEmpty() ? NULL : baFixed.data()) < 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to add parameter"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }

  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to update layout after adding parameter"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  for (PageStore &pd : pages)
    pd.parameters.append(fixed.text());

  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Insert Parameter"));
}

void SDDSEditor::insertColumn() {
  if (!ensureDataset())
    return;

  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("New Column"));
  configureEditorPopupDialog(&dlg, this);
  QFormLayout form(&dlg);
  SDDSTextEdit name(&dlg);
  SDDSTextEdit symbol(&dlg);
  SDDSTextEdit units(&dlg);
  SDDSTextEdit desc(&dlg);
  SDDSTextEdit fmt(&dlg);
  QSpinBox length(&dlg);
  length.setRange(-1000000, 1000000);
  length.setValue(0);
  QHBoxLayout *typeLayout = new QHBoxLayout();
  QButtonGroup typeGroup(&dlg);
  QMap<int, QRadioButton *> btns;
  auto addBtn = [&](const QString &text, int id) {
    QRadioButton *b = new QRadioButton(text, &dlg);
    typeGroup.addButton(b, id);
    typeLayout->addWidget(b);
    btns[id] = b;
  };
  addBtn(tr("short"), SDDS_SHORT);
  addBtn(tr("ushort"), SDDS_USHORT);
  addBtn(tr("long"), SDDS_LONG);
  addBtn(tr("ulong"), SDDS_ULONG);
  addBtn(tr("long64"), SDDS_LONG64);
  addBtn(tr("ulong64"), SDDS_ULONG64);
  addBtn(tr("float"), SDDS_FLOAT);
  addBtn(tr("double"), SDDS_DOUBLE);
  addBtn(tr("long double"), SDDS_LONGDOUBLE);
  addBtn(tr("string"), SDDS_STRING);
  addBtn(tr("character"), SDDS_CHARACTER);
  if (btns.contains(SDDS_DOUBLE))
    btns[SDDS_DOUBLE]->setChecked(true);
  form.addRow(tr("Name"), &name);
  form.addRow(tr("Symbol"), &symbol);
  form.addRow(tr("Units"), &units);
  form.addRow(tr("Description"), &desc);
  form.addRow(tr("Format"), &fmt);
  form.addRow(tr("Field length"), &length);
  form.addRow(tr("Type"), typeLayout);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dlg);
  form.addRow(&buttons);
  connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted || name.text().isEmpty())
    return;
  if (!validateDefinitionName(this, name.text(), "column", -1, dataset.layout.n_columns,
                              [this](int i) { return dataset.layout.column_definition[i].name; }))
    return;
  if (!definitionTextEncodable(this, {symbol.text(), units.text(), desc.text(), fmt.text()}))
    return;
  if (!validateFieldLength(this, length.value(), typeGroup.checkedId()))
    return;

  QByteArray baName = name.text().toLocal8Bit();
  QByteArray baSym = symbol.text().toLocal8Bit();
  QByteArray baUnits = units.text().toLocal8Bit();
  QByteArray baDesc = desc.text().toLocal8Bit();
  QByteArray baFmt = fmt.text().toLocal8Bit();

  if (SDDS_DefineColumn(&dataset, baName.constData(),
                        symbol.text().isEmpty() ? NULL : baSym.constData(),
                        units.text().isEmpty() ? NULL : baUnits.constData(),
                        desc.text().isEmpty() ? NULL : baDesc.constData(),
                        fmt.text().isEmpty() ? NULL : baFmt.constData(),
                        typeGroup.checkedId(), length.value()) < 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to add column"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }

  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to update layout after adding column"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  for (PageStore &pd : pages) {
    int rows = pd.columns.size() ? pd.columns[0].size() : 0;
    pd.columns.append(QVector<QString>(rows));
  }

  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Insert Column"));
}

void SDDSEditor::insertArray() {
  if (!ensureDataset())
    return;

  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("New Array"));
  configureEditorPopupDialog(&dlg, this);
  QFormLayout form(&dlg);
  SDDSTextEdit name(&dlg);
  SDDSTextEdit symbol(&dlg);
  SDDSTextEdit units(&dlg);
  SDDSTextEdit desc(&dlg);
  SDDSTextEdit fmt(&dlg);
  SDDSTextEdit group(&dlg);
  QSpinBox length(&dlg);
  length.setRange(-1000000, 1000000);
  length.setValue(0);
  QHBoxLayout *typeLayout = new QHBoxLayout();
  QButtonGroup typeGroup(&dlg);
  QMap<int, QRadioButton *> btns;
  auto addBtn = [&](const QString &text, int id) {
    QRadioButton *b = new QRadioButton(text, &dlg);
    typeGroup.addButton(b, id);
    typeLayout->addWidget(b);
    btns[id] = b;
  };
  addBtn(tr("short"), SDDS_SHORT);
  addBtn(tr("ushort"), SDDS_USHORT);
  addBtn(tr("long"), SDDS_LONG);
  addBtn(tr("ulong"), SDDS_ULONG);
  addBtn(tr("long64"), SDDS_LONG64);
  addBtn(tr("ulong64"), SDDS_ULONG64);
  addBtn(tr("float"), SDDS_FLOAT);
  addBtn(tr("double"), SDDS_DOUBLE);
  addBtn(tr("long double"), SDDS_LONGDOUBLE);
  addBtn(tr("string"), SDDS_STRING);
  addBtn(tr("character"), SDDS_CHARACTER);
  if (btns.contains(SDDS_DOUBLE))
    btns[SDDS_DOUBLE]->setChecked(true);
  form.addRow(tr("Name"), &name);
  form.addRow(tr("Symbol"), &symbol);
  form.addRow(tr("Units"), &units);
  form.addRow(tr("Description"), &desc);
  form.addRow(tr("Format"), &fmt);
  form.addRow(tr("Group"), &group);
  form.addRow(tr("Field length"), &length);
  form.addRow(tr("Type"), typeLayout);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dlg);
  form.addRow(&buttons);
  connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted || name.text().isEmpty())
    return;
  if (!validateDefinitionName(this, name.text(), "array", -1, dataset.layout.n_arrays,
                              [this](int i) { return dataset.layout.array_definition[i].name; }))
    return;
  if (!definitionTextEncodable(this, {symbol.text(), units.text(), desc.text(), fmt.text(), group.text()}))
    return;
  if (!validateFieldLength(this, length.value(), typeGroup.checkedId()))
    return;

  QByteArray baName = name.text().toLocal8Bit();
  QByteArray baSym = symbol.text().toLocal8Bit();
  QByteArray baUnits = units.text().toLocal8Bit();
  QByteArray baDesc = desc.text().toLocal8Bit();
  QByteArray baFmt = fmt.text().toLocal8Bit();
  QByteArray baGroup = group.text().toLocal8Bit();

  if (SDDS_DefineArray(&dataset, baName.constData(),
                       symbol.text().isEmpty() ? NULL : baSym.constData(),
                       units.text().isEmpty() ? NULL : baUnits.constData(),
                       desc.text().isEmpty() ? NULL : baDesc.constData(),
                       fmt.text().isEmpty() ? NULL : baFmt.constData(),
                       typeGroup.checkedId(), length.value(), 1,
                       group.text().isEmpty() ? NULL : baGroup.constData()) < 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to add array"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }

  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to update layout after adding array"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  for (PageStore &pd : pages) {
    ArrayStore as;
    as.dims = QVector<int>(1, 5);
    as.values.resize(5);
    pd.arrays.append(as);
  }

  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Insert Array"));
}

void SDDSEditor::deleteParameter() {
  deleteParameterSelection(-1);
}

void SDDSEditor::deleteParameterSelection(int fallbackRow) {
  if (!datasetLoaded)
    return;
  commitModels();
  SDDS_LAYOUT *layout = &dataset.layout;
  QVector<int> rows = selectedRowsOrFallback(paramView, layout->n_parameters,
                                             fallbackRow);
  deleteParameterRows(rows);
}

void SDDSEditor::deleteParameterRows(const QVector<int> &selectedRows) {
  if (!datasetLoaded)
    return;
  commitModels();
  QSet<int> selected;
  for (int row : selectedRows)
    selected.insert(row);
  QVector<int> rows = sortedValidIndexesDescending(selected,
                                                   dataset.layout.n_parameters);
  if (rows.isEmpty())
    return;

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  for (int row : rows)
    removeParameterFromLayout(&dataset.layout, row);
  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to update layout after deleting parameter"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  for (PageStore &pd : pages) {
    for (int row : rows)
      if (row < pd.parameters.size())
        pd.parameters.remove(row);
  }

  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            rows.size() == 1
                                ? tr("Delete Parameter")
                                : tr("Delete %1 Parameters").arg(rows.size()));
}

void SDDSEditor::deleteColumn() {
  deleteColumnSelection(-1);
}

void SDDSEditor::deleteColumnSelection(int fallbackColumn) {
  if (!datasetLoaded)
    return;
  commitModels();
  SDDS_LAYOUT *layout = &dataset.layout;
  QVector<int> columns = selectedColumnsOrFallback(columnView, layout->n_columns,
                                                   fallbackColumn);
  deleteColumnIndexes(columns);
}

void SDDSEditor::deleteColumnIndexes(const QVector<int> &selectedColumns) {
  if (!datasetLoaded)
    return;
  commitModels();
  QSet<int> selected;
  for (int column : selectedColumns)
    selected.insert(column);
  QVector<int> columns = sortedValidIndexesDescending(selected,
                                                      dataset.layout.n_columns);
  if (columns.isEmpty())
    return;

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  for (int col : columns)
    removeColumnFromLayout(&dataset.layout, col);
  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to update layout after deleting column"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  for (PageStore &pd : pages) {
    for (int col : columns)
      if (col < pd.columns.size())
        pd.columns.remove(col);
  }

  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            columns.size() == 1
                                ? tr("Delete Column")
                                : tr("Delete %1 Columns").arg(columns.size()));
}

void SDDSEditor::deleteArray() {
  deleteArraySelection(-1);
}

void SDDSEditor::deleteArraySelection(int fallbackColumn) {
  if (!datasetLoaded)
    return;
  commitModels();
  SDDS_LAYOUT *layout = &dataset.layout;
  QVector<int> arrays = selectedColumnsOrFallback(arrayView, layout->n_arrays,
                                                  fallbackColumn);
  deleteArrayIndexes(arrays);
}

void SDDSEditor::deleteArrayIndexes(const QVector<int> &selectedArrays) {
  if (!datasetLoaded)
    return;
  commitModels();
  QSet<int> selected;
  for (int array : selectedArrays)
    selected.insert(array);
  QVector<int> arrays = sortedValidIndexesDescending(selected,
                                                     dataset.layout.n_arrays);
  if (arrays.isEmpty())
    return;

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  for (int col : arrays)
    removeArrayFromLayout(&dataset.layout, col);
  if (!SDDS_SaveLayout(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to update layout after deleting array"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  for (PageStore &pd : pages) {
    for (int col : arrays)
      if (col < pd.arrays.size())
        pd.arrays.remove(col);
  }

  populateModels();
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            arrays.size() == 1
                                ? tr("Delete Array")
                                : tr("Delete %1 Arrays").arg(arrays.size()));
}

void SDDSEditor::insertColumnRows() {
  if (!datasetLoaded)
    return;
  // Rows belong to columns; without one the insertion would only add an empty Undo step.
  if (dataset.layout.n_columns <= 0) {
    QMessageBox::information(this, tr("Insert Rows"), tr("Insert a column before inserting rows."));
    return;
  }

  commitModels();

  bool ok = false;
  int rowsToAdd =
      QInputDialog::getInt(this, tr("Insert Rows"), tr("Number of rows"),
                           lastRowAddCount, 1, 1000000, 1, &ok);
  if (!ok || rowsToAdd <= 0)
    return;
  lastRowAddCount = rowsToAdd;

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  QModelIndex idx = columnView->currentIndex();
  int insertPos = idx.isValid() ? idx.row() + 1 : -1;

  if (currentPage >= 0 && currentPage < pages.size()) {
    PageStore &pd = pages[currentPage];
    if (!pd.columns.isEmpty()) {
      int pos = insertPos >= 0 && insertPos <= pd.columns[0].size()
                   ? insertPos
                   : pd.columns[0].size();
      for (QVector<QString> &col : pd.columns)
        col.insert(pos, rowsToAdd, QString());
    }
  }

  populateModels();
  columnView->clearSelection();
  columnView->setCurrentIndex(QModelIndex());
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            tr("Insert %1 Rows").arg(rowsToAdd));
}

void SDDSEditor::deleteColumnRows() {
  if (!datasetLoaded)
    return;

  commitModels();

  // Never delete rows that the row filter is hiding.
  QModelIndexList selection = visibleSelectedIndexes(columnView);
  if (selection.isEmpty())
    return;

  QSet<int> uniqueRows;
  uniqueRows.reserve(selection.size());
  for (const QModelIndex &idx : selection)
    if (idx.isValid())
      uniqueRows.insert(idx.row());
  if (uniqueRows.isEmpty())
    return;

  QVector<int> rows = uniqueRows.values().toVector();
  std::sort(rows.begin(), rows.end(), std::greater<int>());

  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  if (currentPage >= 0 && currentPage < pages.size()) {
    PageStore &pd = pages[currentPage];
    for (QVector<QString> &col : pd.columns) {
      for (int r : rows) {
        if (r < col.size())
          col.remove(r);
      }
    }
  }

  populateModels();
  columnView->clearSelection();
  columnView->setCurrentIndex(QModelIndex());
  markDirty();
  pushStructuralUndoCommand(this, std::move(before),
                            tr("Delete %1 Rows").arg(rows.size()));
}

void SDDSEditor::filterColumnRows() {
  if (!datasetLoaded) {
    QMessageBox::information(this, tr("Row Filter"), tr("Load a file first."));
    return;
  }
  if (dataset.layout.n_columns <= 0) {
    QMessageBox::information(this, tr("Row Filter"), tr("No columns available to filter."));
    return;
  }

  commitModels();

  QString selectedColumnDefault;
  int selectedColumn = -1;
  if (columnView) {
    const QModelIndex current = columnView->currentIndex();
    if (current.isValid())
      selectedColumn = current.column();
    else if (QItemSelectionModel *sel = columnView->selectionModel()) {
      const QModelIndexList cols = sel->selectedColumns();
      if (!cols.isEmpty())
        selectedColumn = cols.first().column();
    }
  }
  if (selectedColumn >= 0 && selectedColumn < dataset.layout.n_columns) {
    QString columnName =
        QString::fromLocal8Bit(dataset.layout.column_definition[selectedColumn].name);
    const QRegularExpression identRe(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    // A bare row, i, true or false would mean the row index or a constant.
    const QString lowered = columnName.toLower();
    if (!identRe.match(columnName).hasMatch() || lowered == "row" || lowered == "i" ||
        lowered == "true" || lowered == "false")
      columnName = QStringLiteral("[%1]").arg(columnName);
    selectedColumnDefault = QStringLiteral("%1 > 0").arg(columnName);
  }

  QString defaultExpression = rowFilterExpression;
  if (defaultExpression.isEmpty()) {
    if (!selectedColumnDefault.isEmpty())
      defaultExpression = selectedColumnDefault;
    else
      defaultExpression = lastRowFilterExpression;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Filter/View Rows"));
  configureEditorPopupDialog(&dlg, this);
  QVBoxLayout layout(&dlg);

  QLabel prompt(tr("Expression (non-destructive view filter):"), &dlg);
  SDDSTextEdit exprEdit(defaultExpression, &dlg);
  QLabel help(tr("Examples: X>0 && Status==\"OK\"    or    [Beam Current] >= 100"), &dlg);

  layout.addWidget(&prompt);
  layout.addWidget(&exprEdit);
  layout.addWidget(&help);

  QDialogButtonBox box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg);
  QPushButton *clearBtn = box.addButton(tr("Clear"), QDialogButtonBox::ActionRole);
  layout.addWidget(&box);
  bool cleared = false;

  connect(&box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  connect(clearBtn, &QPushButton::clicked, &dlg, [this, &dlg, &cleared]() {
    cleared = true;
    clearColumnRowFilter();
    dlg.accept();
  });

  if (dlg.exec() != QDialog::Accepted)
    return;
  if (cleared)
    return;

  const QString expr = exprEdit.text().trimmed();
  if (expr.isEmpty()) {
    clearColumnRowFilter();
    return;
  }

  rowFilterExpression = expr;
  lastRowFilterExpression = expr;
  rowFilterActive = true;
  refreshColumnRowFilter(true);
}

void SDDSEditor::clearColumnRowFilter() {
  rowFilterActive = false;
  rowFilterExpression.clear();
  refreshColumnRowFilter(false);
  message(tr("Row filter cleared"));
}

bool SDDSEditor::applyColumnRowFilter(QString *errorText, int *visibleRows) {
  if (!columnView || !columnModel)
    return false;

  const int rows = columnModel->rowCount();
  int visible = 0;

  if (!rowFilterActive || rowFilterExpression.trimmed().isEmpty()) {
    for (int r = 0; r < rows; ++r)
      columnView->setRowHidden(r, false);
    if (visibleRows)
      *visibleRows = rows;
    return true;
  }

  if (currentPage < 0 || currentPage >= pages.size()) {
    for (int r = 0; r < rows; ++r)
      columnView->setRowHidden(r, false);
    if (visibleRows)
      *visibleRows = rows;
    return true;
  }

  const PageStore &pd = pages[currentPage];
  // SDDS names are case sensitive, so "x" must not find column "X" when both
  // exist; fall back to ignoring case only when no name matches exactly.
  QHash<QString, int> exactColumnLookup;
  QHash<QString, int> columnLookup;
  exactColumnLookup.reserve(dataset.layout.n_columns + 1);
  columnLookup.reserve(dataset.layout.n_columns + 1);
  for (int c = 0; c < dataset.layout.n_columns; ++c) {
    const char *name = dataset.layout.column_definition[c].name;
    if (!name)
      continue;
    const QString n = QString::fromLocal8Bit(name).trimmed();
    if (n.isEmpty())
      continue;
    exactColumnLookup.insert(n, c);
    if (!columnLookup.contains(n.toLower()))
      columnLookup.insert(n.toLower(), c);
  }

  auto resolverForRow = [&](int row, const QString &ident, bool columnOnly, QString *outText) -> bool {
    if (!outText)
      return false;
    const QString lowered = ident.toLower();
    if (!columnOnly && (lowered == "row" || lowered == "i")) {
      *outText = QString::number(row);
      return true;
    }

    auto it = exactColumnLookup.constFind(ident);
    if (it == exactColumnLookup.constEnd()) {
      it = columnLookup.constFind(lowered);
      if (it == columnLookup.constEnd())
        return false;
    }

    const int col = it.value();
    if (col < 0 || col >= pd.columns.size()) {
      *outText = QString();
      return true;
    }
    const QVector<QString> &values = pd.columns[col];
    *outText = (row >= 0 && row < values.size()) ? values[row] : QString();
    // An empty numeric cell is saved as zero, so filter it as zero.
    if (SDDS_NUMERIC_TYPE(dataset.layout.column_definition[col].type) &&
        outText->trimmed().isEmpty())
      *outText = QStringLiteral("0");
    return true;
  };

  const QString expression = rowFilterExpression;
  for (int r = 0; r < rows; ++r) {
    RowFilterParser parser(expression,
                           [&](const QString &ident, bool columnOnly, QString *value) {
                             return resolverForRow(r, ident, columnOnly, value);
                           });
    bool pass = false;
    QString parseError;
    if (!parser.parse(&pass, &parseError)) {
      if (errorText) {
        *errorText = tr("Row filter error at row %1: %2")
                         .arg(r + 1)
                         .arg(parseError.isEmpty() ? tr("invalid expression") : parseError);
      }
      return false;
    }
    columnView->setRowHidden(r, !pass);
    if (pass)
      ++visible;
  }

  if (visibleRows)
    *visibleRows = visible;
  return true;
}

void SDDSEditor::refreshColumnRowFilter(bool showMessageOnError) {
  if (!columnView || !columnModel)
    return;

  QString errorText;
  int visibleRows = 0;
  if (!applyColumnRowFilter(&errorText, &visibleRows)) {
    for (int r = 0; r < columnModel->rowCount(); ++r)
      columnView->setRowHidden(r, false);
    rowFilterActive = false;
    visibleColumnRows = columnModel->rowCount();
    updateFilterIndicator();
    emit columnRowVisibilityChanged();
    if (showMessageOnError) {
      QMessageBox::warning(this, tr("Row Filter"),
                           errorText.isEmpty() ? tr("Failed to apply row filter") : errorText);
    }
    if (!errorText.isEmpty())
      message(tr("Row filter disabled: %1").arg(errorText));
    return;
  }

  visibleColumnRows = visibleRows;
  updateFilterIndicator();
  emit columnRowVisibilityChanged();
  if (rowFilterActive && showMessageOnError) {
    const int totalRows = columnModel->rowCount();
    message(tr("Row filter active: %1/%2 rows visible")
                .arg(visibleRows)
                .arg(totalRows));
  }
}

void SDDSEditor::fillSeriesSelection() {
  fillSeries(focusedTable());
}

void SDDSEditor::fillSeries(QTableView *view) {
  if (!view || !view->selectionModel()) {
    QMessageBox::information(this, tr("Fill Series"), tr("Select one or more cells first."));
    return;
  }

  flushPendingEdits();
  QModelIndexList selection = editableSelectedIndexes(view, true);
  if (selection.isEmpty()) {
    QMessageBox::information(this, tr("Fill Series"), tr("Select one or more cells first."));
    return;
  }

  std::sort(selection.begin(), selection.end(), [](const QModelIndex &a, const QModelIndex &b) {
    return a.row() == b.row() ? a.column() < b.column() : a.row() < b.row();
  });

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Fill Series"));
  configureEditorPopupDialog(&dlg, this);
  QFormLayout form(&dlg);
  SDDSTextEdit startEdit(lastFillSeriesStart, &dlg);
  SDDSTextEdit stepEdit(lastFillSeriesStep, &dlg);
  form.addRow(tr("Start"), &startEdit);
  form.addRow(tr("Step"), &stepEdit);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dlg);
  form.addRow(&buttons);
  connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted)
    return;

  long double start = 0.0L;
  long double step = 0.0L;
  if (!parseLongDoubleStrict(startEdit.text(), &start) ||
      !parseLongDoubleStrict(stepEdit.text(), &step)) {
    QMessageBox::warning(this, tr("Fill Series"), tr("Start and step must be valid numbers."));
    return;
  }

  lastFillSeriesStart = startEdit.text();
  lastFillSeriesStep = stepEdit.text();
  ExactInteger exactStart = exactInteger(false, 0);
  ExactInteger exactStep = exactStart;
  const bool exactSeries = parseExactInteger(startEdit.text(), &exactStart) &&
                           parseExactInteger(stepEdit.text(), &exactStep);

  struct Pending { QModelIndex idx; QString value; };
  QVector<Pending> updates;
  updates.reserve(selection.size());

  for (int i = 0; i < selection.size(); ++i) {
    const QModelIndex idx = selection[i];
    if (!idx.isValid())
      continue;

    int type = SDDS_STRING;
    if (view == paramView)
      type = dataset.layout.parameter_definition[idx.row()].type;
    else if (view == columnView)
      type = dataset.layout.column_definition[idx.column()].type;
    else if (view == arrayView)
      type = dataset.layout.array_definition[idx.column()].type;

    if (!SDDS_NUMERIC_TYPE(type)) {
      QMessageBox::warning(this, tr("Fill Series"),
                           tr("Selected cells include non-numeric fields."));
      return;
    }

    QString newText;
    ExactInteger exactValue = exactStart;
    if (SDDS_INTEGER_TYPE(type) && exactSeries &&
        exactMultiply(exactStep, exactInteger(false, static_cast<quint64>(i)), &exactValue) &&
        exactAdd(exactStart, exactValue, &exactValue))
      newText = exactIntegerText(exactValue);
    else {
      long double result = start + step * static_cast<long double>(i);
      if (SDDS_INTEGER_TYPE(type)) {
        // As in numerical expressions, check literals and intermediates before
        // a fractional or rounded large value can become a valid integer.
        ExpressionContext ctx = {};
        ctx.i = i;
        const QString expression = QStringLiteral("(%1) + (%2) * i")
            .arg(startEdit.text().trimmed().isEmpty() ? QStringLiteral("0") : startEdit.text(),
                 stepEdit.text().trimmed().isEmpty() ? QStringLiteral("0") : stepEdit.text());
        if (!evaluateExpressionText(expression, ctx, &result, true)) {
          QMessageBox::warning(this, tr("Fill Series"),
                               tr("This series cannot be evaluated without losing integer precision. "
                                  "Use integer start and step values, or change the destination type to floating point."));
          return;
        }
      }
      newText = numericResultText(result, type);
    }
    if (!validateTextForType(newText, type, true))
      return;
    updates.append({idx, newText});
  }

  if (applyCellEditsWithUndo(undoStack, view->model(), updates, tr("Fill Series")))
    markDirty();
}

void SDDSEditor::applyNumericalExpressionSelection() {
  applyNumericalExpression(focusedTable());
}

void SDDSEditor::applyNumericalExpression(QTableView *view) {
  if (!view || !view->selectionModel()) {
    QMessageBox::information(this, tr("Apply Numerical Expression"), tr("Select one or more cells first."));
    return;
  }

  flushPendingEdits();
  QModelIndexList selection = editableSelectedIndexes(view, true);
  if (selection.isEmpty()) {
    QMessageBox::information(this, tr("Apply Numerical Expression"), tr("Select one or more cells first."));
    return;
  }

  std::sort(selection.begin(), selection.end(), [](const QModelIndex &a, const QModelIndex &b) {
    return a.row() == b.row() ? a.column() < b.column() : a.row() < b.row();
  });

  QDialog exprDlg(nullptr);
  exprDlg.setWindowModality(Qt::ApplicationModal);
  exprDlg.setWindowFlag(Qt::Window, true);
  exprDlg.setWindowTitle(tr("Apply Numerical Expression"));
  QVBoxLayout exprLayout(&exprDlg);
  QLabel exprLabel(tr("Expression:"), &exprDlg);
  SDDSTextEdit exprEdit(lastNumericalExpression, &exprDlg);
  exprLayout.addWidget(&exprLabel);
  exprLayout.addWidget(&exprEdit);
  QPlainTextEdit exprHelp(&exprDlg);
  exprHelp.setReadOnly(true);
  exprHelp.setLineWrapMode(QPlainTextEdit::NoWrap);
  exprHelp.setPlainText(tr(
      "Variables reference\n"
      " - x: current cell value\n"
      " - a: anchor value (first selected cell)\n"
      " - i: index in selected cells (0-based)\n"
      " - row, col: absolute row/column index (0-based)\n"
      " - dr, dc: row/column offset from anchor\n\n"
      "Functions: abs, sqrt, sin, cos, tan, log, exp, floor, ceil\n\n"
      "Examples\n"
      " - x + 10\n"
      " - a + i*0.5\n"
      " - x * (1 + dr*0.01)\n"
      " - sin(x) * exp(-i/10)"));
  exprHelp.setMinimumHeight(220);
  exprLayout.addWidget(&exprHelp);
  QDialogButtonBox exprButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                               Qt::Horizontal, &exprDlg);
  connect(&exprButtons, &QDialogButtonBox::accepted, &exprDlg, &QDialog::accept);
  connect(&exprButtons, &QDialogButtonBox::rejected, &exprDlg, &QDialog::reject);
  exprLayout.addWidget(&exprButtons);
  exprDlg.resize(900, 420);

  if (exprDlg.exec() != QDialog::Accepted)
    return;
  const QString expr = exprEdit.text().trimmed();
  if (expr.isEmpty())
    return;
  lastNumericalExpression = expr;
  const bool usesX = expr.contains(QRegularExpression(QStringLiteral("\\bx\\b"), QRegularExpression::CaseInsensitiveOption));
  const bool usesA = expr.contains(QRegularExpression(QStringLiteral("\\ba\\b"), QRegularExpression::CaseInsensitiveOption));

  const QModelIndex anchor = selection.first();
  const QString anchorText = anchor.data(Qt::EditRole).toString();
  long double aValue = 0.0L;
  parseLongDoubleStrict(anchorText, &aValue);
  ExactInteger exactA = exactInteger(false, 0);
  const bool hasExactA = parseExactInteger(anchorText, &exactA);
  const int minRow = anchor.row();
  const int minCol = anchor.column();

  struct Pending { QModelIndex idx; QString value; };
  QVector<Pending> updates;
  updates.reserve(selection.size());

  for (int i = 0; i < selection.size(); ++i) {
    const QModelIndex idx = selection[i];
    if (!idx.isValid())
      continue;

    int type = SDDS_STRING;
    if (view == paramView)
      type = dataset.layout.parameter_definition[idx.row()].type;
    else if (view == columnView)
      type = dataset.layout.column_definition[idx.column()].type;
    else if (view == arrayView)
      type = dataset.layout.array_definition[idx.column()].type;

    if (!SDDS_NUMERIC_TYPE(type)) {
      QMessageBox::warning(this, tr("Apply Numerical Expression"),
                           tr("Selected cells include non-numeric fields."));
      return;
    }

    long double xValue = 0.0L;
    if (!parseLongDoubleStrict(idx.data(Qt::EditRole).toString(), &xValue)) {
        QMessageBox::warning(this, tr("Apply Numerical Expression"),
                           tr("Cell contains invalid numeric value: %1")
                               .arg(truncateForMessage(idx.data(Qt::EditRole).toString())));
      return;
    }

    ExpressionContext ctx;
    ctx.x = xValue;
    ctx.a = aValue;
    ctx.i = i;
    ctx.row = idx.row();
    ctx.col = idx.column();
    ctx.dr = idx.row() - minRow;
    ctx.dc = idx.column() - minCol;

    QString newText;
    bool roundedIntegerOperand = false;
    if (SDDS_INTEGER_TYPE(type)) {
      ExactIntegerContext exactCtx;
      exactCtx.x = exactA;
      exactCtx.hasX = parseExactInteger(idx.data(Qt::EditRole).toString(), &exactCtx.x);
      exactCtx.hasA = hasExactA;
      exactCtx.a = exactA;
      exactCtx.i = ctx.i;
      exactCtx.row = ctx.row;
      exactCtx.col = ctx.col;
      exactCtx.dr = ctx.dr;
      exactCtx.dc = ctx.dc;
      ExactInteger exactResult = exactA;
      if (ExactIntegerExpressionParser(expr, exactCtx).parse(&exactResult))
        newText = exactIntegerText(exactResult);
      roundedIntegerOperand = (usesX && exactCtx.hasX && !exactIntegerFitsLongDouble(exactCtx.x)) ||
                              (usesA && exactCtx.hasA && !exactIntegerFitsLongDouble(exactCtx.a));
    }
    if (newText.isEmpty()) {
      if (roundedIntegerOperand) {
        QMessageBox::warning(this, tr("Apply Numerical Expression"),
                             tr("This expression would lose precision in a 64-bit integer operand. "
                                "Use integer literals with +, -, *, exact division, or abs, "
                                "or change the destination type to floating point."));
        return;
      }
      long double result = 0.0L;
      if (!evaluateExpressionText(expr, ctx, &result, SDDS_INTEGER_TYPE(type))) {
        QMessageBox::warning(this, tr("Apply Numerical Expression"),
                             SDDS_INTEGER_TYPE(type)
                                 ? tr("The expression is invalid or cannot be evaluated without losing integer precision.")
                                 : tr("Failed to evaluate expression."));
        return;
      }
      // Beyond this bound the spacing is at least one, so a fractional result
      // may have rounded into an integer that the cell validator would accept.
      if (SDDS_INTEGER_TYPE(type) && std::isfinite(result) &&
          fabsl(result) >= ldexpl(1.0L, std::numeric_limits<long double>::digits - 1)) {
        QMessageBox::warning(this, tr("Apply Numerical Expression"),
                             tr("This integer result is too large to verify using floating-point arithmetic. "
                                "Use an exact integer expression."));
        return;
      }
      newText = numericResultText(result, type);
    }
    if (!validateTextForType(newText, type, true))
      return;
    updates.append({idx, newText});
  }

  if (applyCellEditsWithUndo(undoStack, view->model(), updates, tr("Apply Numerical Expression")))
    markDirty();
}

void SDDSEditor::applyTextFormulaSelection() {
  applyTextFormula(focusedTable());
}

void SDDSEditor::applyTextFormula(QTableView *view) {
  if (!view || !view->selectionModel()) {
    QMessageBox::information(this, tr("Apply Text Formula"), tr("Select one or more cells first."));
    return;
  }

  flushPendingEdits();
  QModelIndexList selection = editableSelectedIndexes(view, true);
  if (selection.isEmpty()) {
    QMessageBox::information(this, tr("Apply Text Formula"), tr("Select one or more cells first."));
    return;
  }

  std::sort(selection.begin(), selection.end(), [](const QModelIndex &a, const QModelIndex &b) {
    return a.row() == b.row() ? a.column() < b.column() : a.row() < b.row();
  });

  bool ok = false;
  QString templ = QInputDialog::getText(
      this, tr("Apply Text Formula"),
      tr("Template (tokens: ${x}, ${a}, ${i}, ${row}, ${col}, ${dr}, ${dc}):"),
      QLineEdit::Normal, lastTextFormula, &ok);
  if (!ok || templ.isEmpty()) {
    return;
  }
    lastTextFormula = templ;

  const QModelIndex anchor = selection.first();
  const QString anchorText = anchor.data(Qt::EditRole).toString();
  const int minRow = anchor.row();
  const int minCol = anchor.column();

  struct Pending { QModelIndex idx; QString value; int type; };
  QVector<Pending> updates;
  updates.reserve(selection.size());

  for (int i = 0; i < selection.size(); ++i) {
    const QModelIndex idx = selection[i];
    if (!idx.isValid())
      continue;

    int type = SDDS_STRING;
    if (view == paramView)
      type = dataset.layout.parameter_definition[idx.row()].type;
    else if (view == columnView)
      type = dataset.layout.column_definition[idx.column()].type;
    else if (view == arrayView)
      type = dataset.layout.array_definition[idx.column()].type;

    QString x = idx.data(Qt::EditRole).toString();
    QString newText = applyTemplateVariables(templ, x, anchorText, i,
                                             idx.row(), idx.column(),
                                             idx.row() - minRow,
                                             idx.column() - minCol);
    if (!validateTextForType(newText, type, true))
      return;
    updates.append({idx, newText, type});
  }

  if (applyCellEditsWithUndo(undoStack, view->model(), updates, tr("Apply Text Formula")))
    markDirty();
}

void SDDSEditor::clonePage() {
  if (!datasetLoaded || currentPage < 0 || currentPage >= pages.size())
    return;

  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  PageStore pd = pages[currentPage];
  int insertPos = currentPage + 1;
  pages.insert(insertPos, pd);

  pageCombo->blockSignals(true);
  pageCombo->clear();
  for (int i = 0; i < pages.size(); ++i)
    pageCombo->addItem(tr("Page %1").arg(i + 1));
  pageCombo->blockSignals(false);
  pageCombo->setCurrentIndex(insertPos);

  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Clone Page"));
}

void SDDSEditor::insertPage() {
  if (!ensureDataset())
    return;

  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;

  PageStore pd;
  pd.parameters = QVector<QString>(dataset.layout.n_parameters);

  int ccount = dataset.layout.n_columns;
  pd.columns.resize(ccount);
  int rows = 0;
  if (currentPage >= 0 && currentPage < pages.size() && !pages[currentPage].columns.isEmpty())
    rows = pages[currentPage].columns[0].size();
  for (int c = 0; c < ccount; ++c)
    pd.columns[c].resize(rows);

  int acount = dataset.layout.n_arrays;
  pd.arrays.resize(acount);
  for (int a = 0; a < acount; ++a) {
    int dims = dataset.layout.array_definition[a].dimensions;
    if (currentPage >= 0 && currentPage < pages.size() && a < pages[currentPage].arrays.size())
      pd.arrays[a].dims = pages[currentPage].arrays[a].dims;
    else
      pd.arrays[a].dims = QVector<int>(dims, 1);
    int size = dimProduct(pd.arrays[a].dims);
    if (size < 0) {
      QMessageBox::warning(this, tr("SDDS"),
                           tr("Array dimensions are too large."));
      return;
    }
    pd.arrays[a].values.resize(size);
  }

  int insertPos = currentPage + 1;
  pages.insert(insertPos, pd);

  pageCombo->blockSignals(true);
  pageCombo->clear();
  for (int i = 0; i < pages.size(); ++i)
    pageCombo->addItem(tr("Page %1").arg(i + 1));
  pageCombo->blockSignals(false);
  pageCombo->setCurrentIndex(insertPos);

  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Insert Page"));
}

void SDDSEditor::deletePage() {
  if (!datasetLoaded || pages.size() <= 1 || currentPage < 0 ||
      currentPage >= pages.size())
    return;

  commitModels();
  StructuralSnapshot before;
  if (!captureStructuralSnapshot(this, &before))
    return;
  pages.remove(currentPage);

  if (currentPage >= pages.size())
    currentPage = pages.size() - 1;

  pageCombo->blockSignals(true);
  pageCombo->clear();
  for (int i = 0; i < pages.size(); ++i)
    pageCombo->addItem(tr("Page %1").arg(i + 1));
  pageCombo->setCurrentIndex(currentPage);
  pageCombo->blockSignals(false);

  loadPage(currentPage + 1);
  markDirty();
  pushStructuralUndoCommand(this, std::move(before), tr("Delete Page"));
}

/*
 * Apply the light or dark editor theme: an application palette (so dialogs and
 * array viewers match), the main window style sheet, and recolored icons.
 */
void SDDSEditor::applyTheme(bool dark) {
  if (applyingTheme)
    return;
  applyingTheme = true;
  darkPalette = dark;
  const EditorTheme t = editorTheme(dark);
  qApp->setPalette(editorPalette(t, qApp->palette()));
  setStyleSheet(editorStyleSheet(t));

  dataSplitter->setProperty("gripColor", t.border);
  dataSplitter->setProperty("gripHoverColor", t.accent);
  for (int i = 0; i < dataSplitter->count(); ++i)
    if (QSplitterHandle *handle = dataSplitter->handle(i))
      handle->update();
  columnHeader->setColors(t.text, t.muted, t.accentText);
  arrayHeader->setColors(t.text, t.muted, t.accentText);
  paramModel->setMutedColor(t.muted);
  paramView->viewport()->update();

  const QColor disabled = t.muted.lighter(dark ? 70 : 140);
  for (const IconBinding &binding : iconBindings) {
    if (!binding.target)
      continue;
    const QColor color = binding.tone == ToneMuted    ? t.muted
                         : binding.tone == ToneAccent ? t.accentText
                                                      : t.text;
    const QIcon icon = makeEditorIcon(binding.kind, color, disabled);
    if (QAction *action = qobject_cast<QAction *>(binding.target.data()))
      action->setIcon(icon);
    else if (QAbstractButton *button = qobject_cast<QAbstractButton *>(binding.target.data()))
      button->setIcon(icon);
  }
  const QIcon expanded = makeEditorIcon(IconChevronDown, t.muted, disabled);
  const QIcon collapsed = makeEditorIcon(IconChevronRight, t.muted, disabled);
  for (DataPanel *panel : {paramBox, colBox, arrayBox})
    panel->setToggleIcons(expanded, collapsed);
  if (modifiedLabel) {
    modifiedLabel->style()->unpolish(modifiedLabel);
    modifiedLabel->style()->polish(modifiedLabel);
  }
  applyingTheme = false;
}

void SDDSEditor::changeEvent(QEvent *event) {
  QMainWindow::changeEvent(event);
  // The editor installs its own palette; react only to outside palette changes.
  if (applyingTheme || event->type() != QEvent::ApplicationPaletteChange)
    return;
  const bool dark = qApp->palette().color(QPalette::Window).lightness() < 128;
  if (dark != darkPalette)
    applyTheme(dark);
}

void SDDSEditor::restartApp() {
  if (!maybeSave())
    return;
  // The user already chose Save or Discard.  Qt 6 quit() closes windows, so a
  // still-dirty document would ask again, and Cancel would leave two editors.
  dirty = false;
  QString program = QCoreApplication::applicationFilePath();
  QStringList args = QCoreApplication::arguments();
  if (!args.isEmpty())
    args.removeFirst();
  QProcess::startDetached(program, args);
  QCoreApplication::quit();
}

void SDDSEditor::showHelp() {
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Help"));
  configureEditorPopupDialog(&dlg, this);
  QVBoxLayout layout(&dlg);
  QPlainTextEdit text(&dlg);
  text.setReadOnly(true);
  text.setPlainText(tr("Open a file using File->Open or the toolbar.\n"
                       "Select a page with the toolbar arrows or list, and edit parameters, columns or arrays in the tables.\n"
                       "Click a panel title to collapse or expand it. Double-click a parameter's Type to change it.\n"
                       "The status bar shows the save state, file, page, current cell and visible rows;\n"
                       "its Messages button (Ctrl+Shift+L) opens the message log.\n"
                       "Type in the Columns search box and press Enter to find the next match in selected columns, or all columns if none are selected.\n"
                       "Right click headers for more actions such as:\n"
                       " - Plotting a column\n"
                       " - Sorting column or array data\n"
                       " - Searching or replacing values in columns or arrays\n"
                       " - Resizing arrays\n"
                       " - Open Array Viewer: edit a 2D slice, choose axes, and navigate remaining dimensions\n"
                       "   Array Viewer uses zero-based indices and shares edits and Undo/Redo with this editor.\n"
                       "   Copy/Paste works on rectangular selections; Copy slice copies the displayed plane.\n"
                       "   The viewer follows page changes; slice indices adjust when the shape changes.\n"
                       "   Heatmap colors numeric values using Current slice or a Fixed range; gray marks nonfinite/missing values.\n"
                       "Use the Edit menu to insert or delete items, and File->Save to commit changes.\n\n"
                       "Formula / Fill tools (Edit->Formula / Fill):\n"
                       " - Fill Series... (Ctrl+Shift+F): fill selected cells with start + step*i\n"
                       " - Apply Numerical Expression... (Ctrl+Shift+E): evaluate expression per selected cell\n"
                       " - Apply Text Formula... (Ctrl+Shift+M): apply text template to selection\n"
                       "   Tokens: ${x}, ${a}, ${i}, ${row}, ${col}, ${dr}, ${dc}\n\n"
                       "Column row view filter (Edit->Column Rows):\n"
                       " - Filter/View... (Ctrl+Shift+R): show rows matching expression without deleting data\n"
                       " - Clear Filter/View: return to full row view\n"
                       " - Expression operators: &&, ||, !, ==, !=, <, <=, >, >=\n"
                       " - Use [Column Name] for names containing spaces or punctuation, such as [Q[0]];\n"
                       "   a bracketed name is always a column, even if it is row, i, true or false\n\n"
                       "Variables reference\n"
                       " - x / ${x}: current cell value\n"
                       " - a / ${a}: anchor value (first selected cell)\n"
                       " - i / ${i}: index in selected cells (0-based)\n"
                       " - row / ${row}: absolute row index (0-based)\n"
                       " - col / ${col}: absolute column index (0-based)\n"
                       " - dr / ${dr}: row offset from anchor\n"
                       " - dc / ${dc}: column offset from anchor\n\n"
                       "Selection ordering\n"
                       " - Cells are processed top-to-bottom, then left-to-right\n"
                       " - The first selected cell in that order is the anchor\n\n"
                       "Apply Numerical Expression examples\n"
                       " - x + 10\n"
                       " - a + i*0.5\n"
                       " - x * (1 + dr*0.01)\n"
                       " - sin(x) * exp(-i/10)"));
  text.setMinimumSize(400, 300);
  layout.addWidget(&text);
  QDialogButtonBox box(QDialogButtonBox::Ok, Qt::Horizontal, &dlg);
  connect(&box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  layout.addWidget(&box);
  dlg.exec();
}

#include "SDDSEditor_moc.h"
