/**
 * @file SDDSEditor.cc
 * @brief Implementation of the Qt SDDS editor.
 */

#include "SDDSEditor.h"
#include "mdb.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
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
#include <QMouseEvent>
#include <QTimer>
#include <QCoreApplication>
#include <QProgressDialog>
#include <QAbstractTableModel>
#include <functional>
#include <memory>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <algorithm>
#include <limits>
#include <QLocale>

static bool validateTextForType(const QString &text, int type, bool showMessage);
static int dimProduct(const QVector<int> &dims);

class SingleClickEditTableView : public QTableView {
public:
  explicit SingleClickEditTableView(QWidget *parent = nullptr) : QTableView(parent) {}

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
    QTableView::mousePressEvent(event);
    if (event->button() != Qt::LeftButton)
      return;
    QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid())
      return;
    if (QItemSelectionModel *sel = selectionModel())
      sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
    if (!(model()->flags(idx) & Qt::ItemIsEditable))
      return;
    edit(idx);
  }

  void mouseDoubleClickEvent(QMouseEvent *event) override {
    // The view normally consumes the second click as a double-click. For single-click-to-edit,
    // we want the second click to land in the editor widget to place the caret.
    if (event->button() == Qt::LeftButton) {
      QModelIndex idx = indexAt(event->pos());
      if (idx.isValid() && (model()->flags(idx) & Qt::ItemIsEditable)) {
        if (QItemSelectionModel *sel = selectionModel())
          sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
        edit(idx);
        const QPoint viewPos = event->pos();
        const QPoint globalPos = event->globalPos();
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

static bool parseLongDoubleStrict(const QString &text, long double *out) {
  if (!out)
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
  if (errno == ERANGE)
    return false;
  *out = v;
  return true;
}

static bool validatePageForWrite(const SDDS_LAYOUT &layout, const PageStore &pd,
                                 int pageIndex, QString *errorText) {
  const int pcount = layout.n_parameters;
  const int ccount = layout.n_columns;
  const int acount = layout.n_arrays;

  // Parameters
  for (int i = 0; i < pcount; ++i) {
    const PARAMETER_DEFINITION &pdef = layout.parameter_definition[i];
    if (pdef.fixed_value)
      continue;
    const QString val = (i < pd.parameters.size()) ? pd.parameters[i] : QString();
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
    if (type == SDDS_LONGDOUBLE) {
      long double tmp;
      if (!parseLongDoubleStrict(val, &tmp)) {
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
        const QString cell = pd.columns[c][r];
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
        if (type == SDDS_LONGDOUBLE) {
          long double tmp;
          if (!parseLongDoubleStrict(cell, &tmp)) {
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
        const QString cell = as.values[i];
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
        if (type == SDDS_LONGDOUBLE) {
          long double tmp;
          if (!parseLongDoubleStrict(cell, &tmp)) {
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
  }

  return true;
}

static bool validateTextForType(const QString &text, int type,
                                bool showMessage = true) {
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
    if (!text.isEmpty() && text.size() != 1) {
      if (showMessage)
        QMessageBox::warning(nullptr, QObject::tr("SDDS"),
                             QObject::tr("Character field must have length 1"));
      return false;
    }
  }
  return true;
}

static int dimProduct(const QVector<int> &dims) {
  if (dims.isEmpty())
    return 0;
  int prod = 1;
  for (int d : dims) {
    if (d <= 0)
      return 0;
    prod *= d;
  }
  return prod;
}

static void normalizeEmptyNumericsToZero(const SDDS_LAYOUT &layout, QVector<PageStore> &pages) {
  const int pcount = layout.n_parameters;
  const int ccount = layout.n_columns;
  const int acount = layout.n_arrays;

  for (int pg = 0; pg < pages.size(); ++pg) {
    PageStore &pd = pages[pg];

    if (pcount > 0 && pd.parameters.size() < pcount)
      pd.parameters.resize(pcount);
    for (int i = 0; i < pcount; ++i) {
      const PARAMETER_DEFINITION &pdef = layout.parameter_definition[i];
      if (pdef.fixed_value)
        continue;
      const int32_t type = pdef.type;
      if (!SDDS_NUMERIC_TYPE(type))
        continue;
      if (i >= 0 && i < pd.parameters.size() && pd.parameters[i].trimmed().isEmpty())
        pd.parameters[i] = "0";
    }

    if (pd.columns.size() < ccount)
      continue;
    for (int c = 0; c < ccount; ++c) {
      const int32_t type = layout.column_definition[c].type;
      if (!SDDS_NUMERIC_TYPE(type))
        continue;
      for (int r = 0; r < pd.columns[c].size(); ++r) {
        if (pd.columns[c][r].trimmed().isEmpty())
          pd.columns[c][r] = "0";
      }
    }

    if (pd.arrays.size() < acount)
      continue;
    for (int a = 0; a < acount; ++a) {
      const int32_t type = layout.array_definition[a].type;
      if (!SDDS_NUMERIC_TYPE(type))
        continue;
      for (int i = 0; i < pd.arrays[a].values.size(); ++i) {
        if (pd.arrays[a].values[i].trimmed().isEmpty())
          pd.arrays[a].values[i] = "0";
      }
    }
  }
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
      return QLocale::c().toString(val, 'g', std::numeric_limits<double>::max_digits10 - 2);
  } else if (type == SDDS_LONGDOUBLE) {
    //long double val = std::stold(text.toStdString());
    //return QString::asprintf("%.*Lg", std::numeric_limits<long double>::max_digits10, val);
  } else if (type == SDDS_FLOAT) {
    bool ok = true;
    float val = text.toFloat(&ok);
    if (ok)
      return QLocale::c().toString(val, 'g', std::numeric_limits<float>::max_digits10 - 2);
  }
  return text;
}

class SetDataCommand : public QUndoCommand {
public:
  SetDataCommand(QAbstractItemModel *model, const QModelIndex &index,
                 const QString &oldVal, const QString &newVal)
      : m(model), idx(index), oldValue(oldVal), newValue(newVal) {}

  void undo() override { m->setData(idx, oldValue); }
  void redo() override { m->setData(idx, newValue); }

private:
  QAbstractItemModel *m;
  QModelIndex idx;
  QString oldValue;
  QString newValue;
};

class CaretOnDoubleClickLineEdit : public QLineEdit {
public:
  explicit CaretOnDoubleClickLineEdit(QWidget *parent = nullptr) : QLineEdit(parent) {}

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    // QLineEdit normally selects a word on double-click; sddseditor wants double-click
    // to behave like a normal click (place caret).
    setCursorPosition(cursorPositionAt(event->pos()));
    deselect();
    event->accept();
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

  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    Q_UNUSED(parent);
    return 1;
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || !dataset || !pages || !currentPage)
      return QVariant();
    if (role != Qt::DisplayRole && role != Qt::EditRole)
      return QVariant();
    if (*currentPage < 0 || *currentPage >= pages->size())
      return QVariant();
    if (index.column() != 0)
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
    if (index.column() != 0)
      return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
  }

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override {
    if (role != Qt::DisplayRole)
      return QVariant();
    if (orientation == Qt::Horizontal) {
      if (section != 0)
        return QVariant();
      return tr("Value");
    }
    if (!dataset)
      return QVariant();
    if (section < 0 || section >= dataset->layout.n_parameters)
      return QVariant();
    const char *name = dataset->layout.parameter_definition[section].name;
    return name ? QString(name) : QString();
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
};

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
    if (!dataset || role != Qt::DisplayRole)
      return QVariant();
    if (orientation == Qt::Horizontal) {
      if (section < 0 || section >= dataset->layout.n_columns)
        return QVariant();
      return QString(dataset->layout.column_definition[section].name);
    }
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
    if (!dataset || role != Qt::DisplayRole)
      return QVariant();
    if (orientation == Qt::Horizontal) {
      if (section < 0 || section >= dataset->layout.n_arrays)
        return QVariant();
      return QString(dataset->layout.array_definition[section].name);
    }
    return QString::number(section + 1);
  }

  void refresh() {
    recomputeMaxLen();
    beginResetModel();
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
  SDDSItemDelegate(TypeFunc tf, QUndoStack *stack, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), typeFunc(std::move(tf)), undoStack(stack) {}

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return new CaretOnDoubleClickLineEdit(parent);
  }

  void initStyleOption(QStyleOptionViewItem *option,
                       const QModelIndex &index) const override {
    QStyledItemDelegate::initStyleOption(option, index);
    option->text = canonicalizeForDisplay(option->text, typeFunc(index));
  }

  void setEditorData(QWidget *editor, const QModelIndex &index) const override {
    QStyledItemDelegate::setEditorData(editor, index);
    QLineEdit *line = qobject_cast<QLineEdit *>(editor);
    if (line)
      line->setText(canonicalizeForDisplay(line->text(), typeFunc(index)));
  }

  void setModelData(QWidget *editorWidget, QAbstractItemModel *model,
                    const QModelIndex &index) const override {
    QLineEdit *line = qobject_cast<QLineEdit *>(editorWidget);
    if (!line) {
      QStyledItemDelegate::setModelData(editorWidget, model, index);
      return;
    }

    if (!validateTextForType(line->text(), typeFunc(index)))
      return;
    QString oldVal = index.data(Qt::EditRole).toString();
    QString newVal = canonicalizeForDisplay(line->text(), typeFunc(index));
    if (oldVal == newVal) {
      QStyledItemDelegate::setModelData(editorWidget, model, index);
      return;
    }
    if (undoStack)
      undoStack->push(new SetDataCommand(model, index, oldVal, newVal));
    else
      model->setData(index, newVal);
  }

private:
  TypeFunc typeFunc;
  QUndoStack *undoStack;
};

SDDSEditor::SDDSEditor(bool darkPalette, QWidget *parent)
  : QMainWindow(parent), datasetLoaded(false), dirty(false), asciiSave(true),
    currentPage(0), currentFilename(QString()), lastRowAddCount(1),
    lastSearchPattern(QString()), lastReplaceText(QString()),
    undoStack(new QUndoStack(this)), updatingModels(false),
    darkPalette(darkPalette) {
  loadProgressDialog = nullptr;
  loadProgressMin = 0;
  loadProgressMax = 100;

  // QTableView/QHeaderView may query model headers during construction.
  // Ensure SDDS_DATASET starts in a known-safe state (null pointers, zero counts).
  memset(&dataset, 0, sizeof(dataset));

  // console dock
  consoleEdit = new QPlainTextEdit(this);
  consoleEdit->setReadOnly(true);
  // make the console dock roughly five text lines tall
  int lineH = consoleEdit->fontMetrics().lineSpacing();
  consoleEdit->setFixedHeight(lineH * 5 + 2 * consoleEdit->frameWidth());
  QDockWidget *dock = new QDockWidget(QString(), this);
  dock->setTitleBarWidget(new QWidget());
  dock->setWidget(consoleEdit);
  addDockWidget(Qt::TopDockWidgetArea, dock);

  QWidget *central = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(central);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  // page selector bar
  QHBoxLayout *pageLayout = new QHBoxLayout();
  pageLayout->setContentsMargins(0, 0, 0, 0);
  pageLayout->addWidget(new QLabel(tr("Page"), this));
  pageCombo = new QComboBox(this);
  connect(pageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SDDSEditor::pageChanged);
  pageLayout->addWidget(pageCombo);

  pageLayout->addStretch(1);
  asciiBtn = new QRadioButton(tr("ASCII"), this);
  binaryBtn = new QRadioButton(tr("Binary"), this);
  asciiBtn->setChecked(true);
  pageLayout->addWidget(asciiBtn);
  pageLayout->addWidget(binaryBtn);
  mainLayout->addLayout(pageLayout);

  QFont tableFont("Source Code Pro");
  tableFont.setStyleName("Regular");
  tableFont.setPointSize(10);

  // container for data panels
  dataSplitter = new QSplitter(Qt::Vertical, this);
  //dataSplitter->setChildrenCollapsible(false);
  dataSplitter->setHandleWidth(4);
  dataSplitter->setStyleSheet("QSplitter::handle { background-color: lightgrey; }");
  mainLayout->addWidget(dataSplitter, 1);

  // parameters panel
  paramBox = new QGroupBox(tr("Parameters"), this);
  paramBox->setCheckable(true);
  paramBox->setChecked(true);
  QVBoxLayout *paramLayout = new QVBoxLayout(paramBox);
  paramLayout->setContentsMargins(0, 0, 0, 0);
  paramModel = new ParameterPageModel(&dataset, &pages, &currentPage, this);
  paramView = new SingleClickEditTableView(paramBox);
  paramView->setFont(tableFont);
  paramView->setModel(paramModel);
  connect(paramModel, &QAbstractItemModel::dataChanged, this,
          [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
            if (!updatingModels)
              markDirty();
          });
  paramView->setItemDelegate(new SDDSItemDelegate(
      [this](const QModelIndex &idx) {
        return dataset.layout.parameter_definition[idx.row()].type;
      },
      undoStack, paramView));
  // Let the single value column expand to take up the available space.
  // This keeps the parameter table readable even when the window is wide.
  paramView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  paramView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  paramView->verticalHeader()->setDefaultSectionSize(18);
  paramView->verticalHeader()->setSectionsMovable(true);
  paramView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(paramView->verticalHeader(), &QHeaderView::sectionDoubleClicked, this,
          &SDDSEditor::changeParameterType);
  connect(paramView->verticalHeader(), &QHeaderView::sectionMoved, this,
          &SDDSEditor::parameterMoved);
  paramView->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(paramView->verticalHeader(), &QHeaderView::customContextMenuRequested,
          this, &SDDSEditor::parameterHeaderMenuRequested);
  paramView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(paramView, &QTableView::customContextMenuRequested,
          this, &SDDSEditor::parameterCellMenuRequested);
  connect(paramBox, &QGroupBox::toggled, paramView, &QWidget::setVisible);
  paramLayout->addWidget(paramView);
  dataSplitter->addWidget(paramBox);

  // columns panel
  colBox = new QGroupBox(tr("Columns"), this);
  colBox->setCheckable(true);
  colBox->setChecked(true);
  QVBoxLayout *colLayout = new QVBoxLayout(colBox);
  colLayout->setContentsMargins(0, 0, 0, 0);
  columnModel = new ColumnPageModel(&dataset, &pages, &currentPage, this);
  columnView = new SingleClickEditTableView(colBox);
  columnView->setFont(tableFont);
  columnView->setModel(columnModel);
  connect(columnModel, &QAbstractItemModel::dataChanged, this,
          [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
            if (!updatingModels)
              markDirty();
          });
  connect(columnModel, &QAbstractItemModel::rowsInserted, this,
          [this](const QModelIndex &, int, int) {
            if (!updatingModels)
              markDirty();
          });
  connect(columnModel, &QAbstractItemModel::rowsRemoved, this,
          [this](const QModelIndex &, int, int) {
            if (!updatingModels)
              markDirty();
          });
  columnView->setItemDelegate(new SDDSItemDelegate(
      [this](const QModelIndex &idx) {
        return dataset.layout.column_definition[idx.column()].type;
      },
      undoStack, columnView));
  columnView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  columnView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  columnView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  columnView->verticalHeader()->setDefaultSectionSize(18);
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
  connect(columnView->horizontalHeader(), &QHeaderView::customContextMenuRequested,
          this, &SDDSEditor::columnHeaderMenuRequested);
  columnView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(columnView, &QTableView::customContextMenuRequested,
          this, &SDDSEditor::columnCellMenuRequested);
  connect(colBox, &QGroupBox::toggled, columnView, &QWidget::setVisible);
  colLayout->addWidget(columnView);
  dataSplitter->addWidget(colBox);

  // arrays panel
  arrayBox = new QGroupBox(tr("Arrays"), this);
  arrayBox->setCheckable(true);
  arrayBox->setChecked(true);
  QVBoxLayout *arrayLayout = new QVBoxLayout(arrayBox);
  arrayLayout->setContentsMargins(0, 0, 0, 0);
  arrayModel = new ArrayPageModel(&dataset, &pages, &currentPage, this);
  arrayView = new SingleClickEditTableView(arrayBox);
  arrayView->setFont(tableFont);
  arrayView->setModel(arrayModel);
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
  arrayView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  arrayView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  arrayView->verticalHeader()->setDefaultSectionSize(18);
  arrayView->horizontalHeader()->setSectionsMovable(true);
  arrayView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(arrayView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeArrayType);
  connect(arrayView->horizontalHeader(), &QHeaderView::sectionMoved, this,
          &SDDSEditor::arrayMoved);
  arrayView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(arrayView->horizontalHeader(), &QHeaderView::customContextMenuRequested,
          this, &SDDSEditor::arrayHeaderMenuRequested);
  arrayView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(arrayView, &QTableView::customContextMenuRequested,
          this, &SDDSEditor::arrayCellMenuRequested);
  connect(arrayBox, &QGroupBox::toggled, arrayView, &QWidget::setVisible);
  arrayLayout->addWidget(arrayView);
  dataSplitter->addWidget(arrayBox);

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
  connect(undoAct, &QAction::triggered, undoStack, &QUndoStack::undo);
  connect(redoAct, &QAction::triggered, undoStack, &QUndoStack::redo);
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
  connect(colRowIns, &QAction::triggered, this, &SDDSEditor::insertColumnRows);
  connect(colRowDel, &QAction::triggered, this, &SDDSEditor::deleteColumnRows);

  QMenu *pageMenu = editMenu->addMenu(tr("Page"));
  QAction *pageClone = pageMenu->addAction(tr("Insert and clone current page"));
  QAction *pageIns = pageMenu->addAction(tr("Insert"));
  QAction *pageDel = pageMenu->addAction(tr("Delete"));
  connect(pageClone, &QAction::triggered, this, &SDDSEditor::clonePage);
  connect(pageIns, &QAction::triggered, this, &SDDSEditor::insertPage);
  connect(pageDel, &QAction::triggered, this, &SDDSEditor::deletePage);

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
  applyTheme(darkPalette);
  updateWindowTitle();
}

SDDSEditor::~SDDSEditor() {
  clearDataset();
}

void SDDSEditor::message(const QString &text) {
  consoleEdit->appendPlainText(text);
}

void SDDSEditor::markDirty() {
  if (updatingModels)
    return;
  dirty = true;
  updateWindowTitle();
}

bool SDDSEditor::maybeSave() {
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
}

void SDDSEditor::closeEvent(QCloseEvent *event) {
  if (maybeSave())
    event->accept();
  else
    event->ignore();
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

void SDDSEditor::parameterMoved(int, int, int) {
  if (!datasetLoaded)
    return;
  commitModels();
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
}

void SDDSEditor::columnMoved(int, int, int) {
  if (!datasetLoaded)
    return;
  commitModels();
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
}

void SDDSEditor::arrayMoved(int, int, int) {
  if (!datasetLoaded)
    return;
  commitModels();
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
}

void SDDSEditor::copy() {
  QTableView *view = focusedTable();
  if (!view)
    return;
  QModelIndexList indexes = view->selectionModel()->selectedIndexes();
  if (indexes.isEmpty())
    return;
  std::sort(indexes.begin(), indexes.end(), [](const QModelIndex &a,
                                               const QModelIndex &b) {
    return a.row() == b.row() ? a.column() < b.column() : a.row() < b.row();
  });
  int prevRow = indexes.first().row();
  QStringList rowTexts;
  QString rowText;
  for (const QModelIndex &idx : indexes) {
    if (idx.row() != prevRow) {
      rowTexts << rowText;
      rowText.clear();
      prevRow = idx.row();
    } else if (!rowText.isEmpty()) {
      rowText += '\t';
    }
    rowText += idx.data().toString();
  }
  rowTexts << rowText;
  QApplication::clipboard()->setText(rowTexts.join('\n'));
}

void SDDSEditor::paste() {
  QTableView *view = focusedTable();
  if (!view)
    return;
  QModelIndex start = view->currentIndex();
  if (!start.isValid())
    return;
  QString text = QApplication::clipboard()->text();
  QStringList rows = text.split('\n');
  bool multiPaste = rows.size() > 1 || text.contains('\t');
  bool changed = false;
  bool warned = false;
  for (int r = 0; r < rows.size(); ++r) {
    QStringList cols = rows[r].split('\t');
    for (int c = 0; c < cols.size(); ++c) {
      QModelIndex idx = view->model()->index(start.row() + r, start.column() + c);
      if (!idx.isValid())
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
        view->model()->setData(idx, cols[c]);
        changed = true;
      }
      if (!valid && show)
        warned = true;
    }
  }
  if (changed)
    markDirty();
}

void SDDSEditor::deleteCells() {
  QTableView *view = focusedTable();
  if (!view)
    return;
  QModelIndexList indexes = view->selectionModel()->selectedIndexes();
  if (indexes.isEmpty())
    return;
  for (const QModelIndex &idx : indexes) {
    if (idx.isValid())
      view->model()->setData(idx, QString());
  }
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

bool SDDSEditor::loadFile(const QString &path) {
  clearDataset();
  SDDS_DATASET in;
  memset(&in, 0, sizeof(in));
  if (!SDDS_InitializeInput(&in,
                            const_cast<char *>(path.toLocal8Bit().constData()))) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to open file"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }

  currentFilename = path;
  dirty = false;
  message(tr("Loaded %1").arg(path));

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

  pages.clear();
  int pageIndex = 0;
  while (SDDS_ReadPage(&in) > 0) {
    ++pageIndex;
    progress.setLabelText(tr("Loading %1 (page %2)…").arg(QFileInfo(path).fileName()).arg(pageIndex));
    progress.setValue(0);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    PageStore pd;
    int64_t totalUnits = 0;
    int64_t doneUnits = 0;

    int32_t pcount;
    char **pnames = SDDS_GetParameterNames(&in, &pcount);
    if (pcount > 0)
      totalUnits += pcount;
    for (int32_t i = 0; i < pcount; ++i) {
      char *val = SDDS_GetParameterAsString(&in, pnames[i], NULL);
      pd.parameters.append(QString(val ? val : ""));
      free(val);

      ++doneUnits;
      if (totalUnits > 0) {
        int percent = static_cast<int>((doneUnits * 100) / totalUnits);
        if (percent != progress.value()) {
          setReadProgress(percent);
        }
      }
    }
    SDDS_FreeStringArray(pnames, pcount);
    int32_t ccount;
    char **cnames = SDDS_GetColumnNames(&in, &ccount);
    int64_t rows = SDDS_RowCount(&in);
    if (ccount > 0 && rows > 0)
      totalUnits += static_cast<int64_t>(ccount) * rows;

    pd.columns.resize(ccount);
    const int64_t updateEvery = std::max<int64_t>(1, totalUnits / 200);
    for (int32_t c = 0; c < ccount; ++c) {
      char **data = SDDS_GetColumnInString(&in, cnames[c]);
      pd.columns[c].resize(rows);
      for (int64_t r = 0; r < rows; ++r) {
        pd.columns[c][r] = QString(data ? data[r] : "");
        ++doneUnits;
        if (totalUnits > 0 && (doneUnits % updateEvery) == 0) {
          int percent = static_cast<int>((doneUnits * 100) / totalUnits);
          if (percent != progress.value()) {
            setReadProgress(percent);
          }
        }
      }
      SDDS_FreeStringArray(data, rows);
    }
    SDDS_FreeStringArray(cnames, ccount);
    int32_t acount;
    char **anames = SDDS_GetArrayNames(&in, &acount);

    // Add array work units up front so progress stays monotonic.
    if (acount > 0) {
      for (int32_t a = 0; a < acount; ++a) {
        ARRAY_DEFINITION *adef = SDDS_GetArrayDefinition(&in, anames[a]);
        if (!adef || adef->dimensions <= 0)
          continue;
        int64_t elements = 1;
        for (int d = 0; d < adef->dimensions; ++d) {
          int dim = 0;
          if (in.array && in.array[a].dimension)
            dim = in.array[a].dimension[d];
          if (dim <= 0) {
            elements = 0;
            break;
          }
          if (elements > std::numeric_limits<int64_t>::max() / dim) {
            elements = std::numeric_limits<int64_t>::max();
            break;
          }
          elements *= dim;
        }
        if (elements > 0 && elements < std::numeric_limits<int64_t>::max())
          totalUnits += elements;
      }
    }

    pd.arrays.resize(acount);
    for (int32_t a = 0; a < acount; ++a) {
      ARRAY_DEFINITION *adef = SDDS_GetArrayDefinition(&in, anames[a]);
      int32_t dim;
      char **vals = SDDS_GetArrayInString(&in, anames[a], &dim);
      pd.arrays[a].dims.resize(adef->dimensions);
      for (int d = 0; d < adef->dimensions; ++d)
        pd.arrays[a].dims[d] = in.array[a].dimension[d];
      pd.arrays[a].values.resize(dim);
      for (int i = 0; i < dim; ++i) {
        pd.arrays[a].values[i] = QString(vals[i]);
        ++doneUnits;
        if (totalUnits > 0 && (doneUnits % updateEvery) == 0) {
          int percent = static_cast<int>((doneUnits * 100) / totalUnits);
          if (percent != progress.value()) {
            setReadProgress(percent);
          }
        }
      }
      SDDS_FreeStringArray(vals, dim);
    }
    SDDS_FreeStringArray(anames, acount);
    pages.append(pd);

    // End-of-page: treat SDDS read/copy as 25% complete.
    setReadProgress(99);
  }
 
  // Copy layout information for later editing and close the file
  memset(&dataset, 0, sizeof(dataset));
  if (!SDDS_InitializeCopy(&dataset, &in, NULL, (char *)"m")) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to copy layout"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&in);
    return false;
  }
  SDDS_Terminate(&in);
  datasetLoaded = true;

  // Update radio buttons to reflect the file's storage mode
  asciiSave = dataset.layout.data_mode.mode == SDDS_ASCII;
  asciiBtn->setChecked(asciiSave);
  binaryBtn->setChecked(!asciiSave);

  if (pages.isEmpty()) {
    QMessageBox::warning(this, tr("SDDS"), tr("File contains no pages"));
    return false;
  }

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
  for (int i = 0; i < pages.size(); ++i)
    pageCombo->addItem(tr("Page %1").arg(i + 1));
  pageCombo->setCurrentIndex(0);
  pageCombo->blockSignals(false);
  currentPage = 0;
  loadPage(1);
  updateWindowTitle();

  progress.setValue(100);
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  loadProgressDialog = nullptr;
  progress.close();
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  return true;
}

bool SDDSEditor::writeFile(const QString &path) {
  if (!datasetLoaded)
    return false;
  commitModels();

  // Validate everything up front so we don't partially write a file.
  QString errorText;
  for (int pg = 0; pg < pages.size(); ++pg) {
    if (!validatePageForWrite(dataset.layout, pages[pg], pg, &errorText)) {
      QMessageBox::warning(this, tr("SDDS"), errorText);
      return false;
    }
  }

  QString finalPath = path;
  bool updateSymlink = false;
  QFileInfo fi(path);
  if (fi.isSymLink()) {
    QString target = fi.symLinkTarget();
    QRegularExpression re("(.*?)([.-])(\\d+)$");
    QRegularExpressionMatch m = re.match(target);
    if (m.hasMatch()) {
      QString prefix = m.captured(1);
      QString sep = m.captured(2);
      QString digits = m.captured(3);
      bool ok = false;
      int num = digits.toInt(&ok);
      if (ok) {
        QString newDigits = QString::number(num + 1).rightJustified(digits.length(), '0');
        finalPath = prefix + sep + newDigits;
        updateSymlink = true;
      }
    }
  }

  SDDS_DATASET out;
  memset(&out, 0, sizeof(out));
  if (!SDDS_InitializeCopy(&out, &dataset,
                           const_cast<char *>(finalPath.toLocal8Bit().constData()), (char *)"w")) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to open output"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }
  out.layout.data_mode.mode = asciiBtn->isChecked() ? SDDS_ASCII : SDDS_BINARY;
  if (!SDDS_WriteLayout(&out)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to write layout"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&out);
    return false;
  }

  int pcount = dataset.layout.n_parameters;
  int ccount = dataset.layout.n_columns;
  int acount = dataset.layout.n_arrays;

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
          SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (short)v, NULL);
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
          SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (unsigned short)v, NULL);
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
          SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (int32_t)v, NULL);
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
          SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (uint32_t)v, NULL);
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
          SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (int64_t)v, NULL);
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
          SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (uint64_t)v, NULL);
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
          SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, v, NULL);
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
          SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, v, NULL);
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
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, v, NULL);
        break;
      }
      case SDDS_STRING:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, text.toLocal8Bit().data(), NULL);
        break;
      case SDDS_CHARACTER: {
        QByteArray ba = text.toLatin1();
        char ch = ba.isEmpty() ? '\0' : ba.at(0);
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name,
                           ch, NULL);
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
        QVector<char *> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          arr[r] = strdup(text.toLocal8Bit().constData());
        }
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
        for (auto p : arr)
          free(p);
      } else if (type == SDDS_CHARACTER) {
        QVector<char> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          const QString text = pd.columns[c][r];
          QByteArray ba = text.toLatin1();
          arr[r] = ba.isEmpty() ? '\0' : ba.at(0);
        }
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
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
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
      }
    }

    for (int a = 0; a < acount && a < pd.arrays.size(); ++a) {
      const char *name = dataset.layout.array_definition[a].name;
      int32_t type = dataset.layout.array_definition[a].type;
      const ArrayStore &as = pd.arrays[a];
      int elements = as.values.size();
      QVector<int32_t> dims = QVector<int32_t>::fromList(as.dims.toList());
      bool ok = true;
      if (type == SDDS_STRING) {
        QVector<char *> arr(elements);
        for (int i = 0; i < elements; ++i) {
          QString cell = as.values[i];
          arr[i] = strdup(cell.toLocal8Bit().constData());
        }
        SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, arr.data(), dims.data());
        for (auto p : arr)
          free(p);
      } else if (type == SDDS_CHARACTER) {
        QVector<char> arr(elements);
        for (int i = 0; i < elements; ++i) {
          QString cell = as.values[i];
          QByteArray ba = cell.toLatin1();
          arr[i] = ba.isEmpty() ? '\0' : ba.at(0);
        }
        SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA,
                      arr.data(), dims.data());
      } else {
        if (type == SDDS_LONGDOUBLE) {
          QVector<long double> buffer(elements);
          for (int i = 0; i < elements; ++i)
            if (!parseLongDoubleStrict(as.values[i], &buffer[i]))
              ok = false;
          if (ok)
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
        } else if (type == SDDS_DOUBLE) {
          QVector<double> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            buffer[i] = cell.trimmed().isEmpty() ? 0.0 : cell.toDouble(&ok);
            if (!ok)
              break;
          }
          if (ok)
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
        } else if (type == SDDS_FLOAT) {
          QVector<float> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            buffer[i] = cell.trimmed().isEmpty() ? 0.0f : cell.toFloat(&ok);
            if (!ok)
              break;
          }
          if (ok)
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
        } else if (type == SDDS_LONG64) {
          QVector<int64_t> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            buffer[i] = cell.trimmed().isEmpty() ? 0 : cell.toLongLong(&ok);
            if (!ok)
              break;
          }
          if (ok)
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
        } else if (type == SDDS_ULONG64) {
          QVector<uint64_t> buffer(elements);
          for (int i = 0; i < elements; ++i) {
            const QString cell = as.values[i];
            buffer[i] = cell.trimmed().isEmpty() ? 0 : cell.toULongLong(&ok);
            if (!ok)
              break;
          }
          if (ok)
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
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
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
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
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
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
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
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
            SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer.data(), dims.data());
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

  SDDS_Terminate(&out);

  // Make the UI match what was just written: empty numeric fields are written as 0.
  normalizeEmptyNumericsToZero(dataset.layout, pages);
  populateModels();

  dirty = false;
  updateWindowTitle();
  if (updateSymlink) {
    QFile::remove(path);
    if (!QFile::link(finalPath, path))
      QMessageBox::warning(this, tr("SDDS"), tr("Failed to update symlink"));
  }
  message(tr("Saved %1").arg(finalPath));
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

  QByteArray fname = QFile::encodeName(path);
  hid_t file = H5Fcreate(fname.constData(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  if (file < 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to create HDF file"));
    return false;
  }

  int pcount = dataset.layout.n_parameters;
  int ccount = dataset.layout.n_columns;
  int acount = dataset.layout.n_arrays;

  for (int pg = 0; pg < pages.size(); ++pg) {
    const PageStore &pd = pages[pg];
    QByteArray gname = QString("page%1").arg(pg + 1).toLocal8Bit();
    hid_t page = H5Gcreate1(file, gname.constData(), 0);
    if (page < 0) {
      H5Fclose(file);
      return false;
    }

    if (pcount > 0) {
      hid_t grp = H5Gcreate1(page, "parameters", 0);
      for (int i = 0; i < pcount && i < pd.parameters.size(); ++i) {
        const char *name = dataset.layout.parameter_definition[i].name;
        int32_t type = dataset.layout.parameter_definition[i].type;
        QString val = pd.parameters[i];
        hid_t space = H5Screate(H5S_SCALAR);
        if (type == SDDS_STRING) {
          QByteArray ba = val.toLocal8Bit();
          hid_t dtype = H5Tcopy(H5T_C_S1);
          H5Tset_size(dtype, ba.size() + 1);
          hid_t ds = H5Dcreate1(grp, name, dtype, space, H5P_DEFAULT);
          const char *ptr = ba.constData();
          H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, &ptr);
          H5Dclose(ds);
          H5Tclose(dtype);
        } else if (type == SDDS_CHARACTER) {
          QByteArray ba = val.toLatin1();
          char ch = ba.isEmpty() ? '\0' : ba.at(0);
          hid_t ds = H5Dcreate1(grp, name, H5T_NATIVE_CHAR, space, H5P_DEFAULT);
          H5Dwrite(ds, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, &ch);
          H5Dclose(ds);
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
          hid_t ds = H5Dcreate1(grp, name, dtype, space, H5P_DEFAULT);
          H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
          H5Dclose(ds);
        }
        H5Sclose(space);
      }
      H5Gclose(grp);
    }

    if (ccount > 0) {
      hid_t grp = H5Gcreate1(page, "columns", 0);
      int64_t rows = (ccount > 0 && pd.columns.size() > 0) ? pd.columns[0].size() : 0;
      hsize_t dims[1] = { (hsize_t)rows };
      for (int c = 0; c < ccount && c < pd.columns.size(); ++c) {
        const char *name = dataset.layout.column_definition[c].name;
        int32_t type = dataset.layout.column_definition[c].type;
        hid_t space = H5Screate_simple(1, dims, NULL);
        if (type == SDDS_STRING) {
          QVector<QByteArray> store(rows);
          QVector<char *> ptrs(rows);
          for (int64_t r = 0; r < rows; ++r) {
            QString txt = r < pd.columns[c].size() ? pd.columns[c][r] : QString();
            store[r] = txt.toLocal8Bit();
            ptrs[r] = store[r].data();
          }
          hid_t dtype = H5Tcopy(H5T_C_S1);
          H5Tset_size(dtype, H5T_VARIABLE);
          hid_t ds = H5Dcreate1(grp, name, dtype, space, H5P_DEFAULT);
          H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, ptrs.data());
          H5Dclose(ds);
          H5Tclose(dtype);
        } else if (type == SDDS_CHARACTER) {
          QVector<char> arr(rows);
          for (int64_t r = 0; r < rows; ++r) {
            QByteArray ba = (r < pd.columns[c].size()) ? pd.columns[c][r].toLatin1() : QByteArray();
            arr[r] = ba.isEmpty() ? '\0' : ba.at(0);
          }
          hid_t ds = H5Dcreate1(grp, name, H5T_NATIVE_CHAR, space, H5P_DEFAULT);
          H5Dwrite(ds, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
          H5Dclose(ds);
        } else if (type == SDDS_LONGDOUBLE) {
          QVector<long double> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? strtold(pd.columns[c][r].toLocal8Bit().constData(), nullptr) : 0.0L;
          hid_t ds = H5Dcreate1(grp, name, H5T_NATIVE_LDOUBLE, space, H5P_DEFAULT);
          H5Dwrite(ds, H5T_NATIVE_LDOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
          H5Dclose(ds);
        } else if (type == SDDS_LONG64) {
          QVector<int64_t> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toLongLong() : 0;
          hid_t dtype = hdfTypeForSdds(type);
          hid_t ds = H5Dcreate1(grp, name, dtype, space, H5P_DEFAULT);
          H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
          H5Dclose(ds);
        } else if (type == SDDS_ULONG64) {
          QVector<uint64_t> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toULongLong() : 0;
          hid_t dtype = hdfTypeForSdds(type);
          hid_t ds = H5Dcreate1(grp, name, dtype, space, H5P_DEFAULT);
          H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
          H5Dclose(ds);
        } else {
          QVector<double> arr(rows);
          for (int64_t r = 0; r < rows; ++r)
            arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toDouble() : 0.0;
          hid_t dtype = hdfTypeForSdds(type);
          hid_t ds = H5Dcreate1(grp, name, dtype, space, H5P_DEFAULT);
          H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
          H5Dclose(ds);
        }
        H5Sclose(space);
      }
      H5Gclose(grp);
    }

    if (acount > 0) {
      hid_t grp = H5Gcreate1(page, "arrays", 0);
      for (int a = 0; a < acount && a < pd.arrays.size(); ++a) {
        const char *name = dataset.layout.array_definition[a].name;
        int32_t type = dataset.layout.array_definition[a].type;
        const ArrayStore &as = pd.arrays[a];
        int dimsCount = as.dims.size();
        QVector<hsize_t> dims(dimsCount);
        for (int i = 0; i < dimsCount; ++i)
          dims[i] = as.dims[i];
        hid_t space = H5Screate_simple(dimsCount, dims.data(), NULL);
        int elements = as.values.size();
        if (type == SDDS_STRING) {
          QVector<QByteArray> store(elements);
          QVector<char *> ptrs(elements);
          for (int i = 0; i < elements; ++i) {
            store[i] = as.values[i].toLocal8Bit();
            ptrs[i] = store[i].data();
          }
          hid_t dtype = H5Tcopy(H5T_C_S1);
          H5Tset_size(dtype, H5T_VARIABLE);
          hid_t ds = H5Dcreate1(grp, name, dtype, space, H5P_DEFAULT);
          H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, ptrs.data());
          H5Dclose(ds);
          H5Tclose(dtype);
        } else if (type == SDDS_CHARACTER) {
          QVector<char> arr(elements);
          for (int i = 0; i < elements; ++i) {
            QByteArray ba = as.values[i].toLatin1();
            arr[i] = ba.isEmpty() ? '\0' : ba.at(0);
          }
          hid_t ds = H5Dcreate1(grp, name, H5T_NATIVE_CHAR, space, H5P_DEFAULT);
          H5Dwrite(ds, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, arr.data());
          H5Dclose(ds);
        } else {
          size_t size = SDDS_type_size[type - 1];
          QVector<char> buffer(size * elements);
          for (int i = 0; i < elements; ++i) {
            QString cell = as.values[i];
            switch (type) {
            case SDDS_LONGDOUBLE:
              ((long double *)buffer.data())[i] = strtold(cell.toLocal8Bit().constData(), nullptr);
              break;
            case SDDS_DOUBLE:
              ((double *)buffer.data())[i] = cell.toDouble();
              break;
            case SDDS_FLOAT:
              ((float *)buffer.data())[i] = cell.toFloat();
              break;
            case SDDS_LONG64:
              ((int64_t *)buffer.data())[i] = cell.toLongLong();
              break;
            case SDDS_ULONG64:
              ((uint64_t *)buffer.data())[i] = cell.toULongLong();
              break;
            case SDDS_LONG:
              ((int32_t *)buffer.data())[i] = cell.toInt();
              break;
            case SDDS_ULONG:
              ((uint32_t *)buffer.data())[i] = cell.toUInt();
              break;
            case SDDS_SHORT:
              ((short *)buffer.data())[i] = (short)cell.toInt();
              break;
            case SDDS_USHORT:
              ((unsigned short *)buffer.data())[i] = (unsigned short)cell.toUInt();
              break;
            default:
              ((double *)buffer.data())[i] = cell.toDouble();
              break;
            }
          }
          hid_t dtype = hdfTypeForSdds(type);
          hid_t ds = H5Dcreate1(grp, name, dtype, space, H5P_DEFAULT);
          H5Dwrite(ds, dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer.data());
          H5Dclose(ds);
        }
        H5Sclose(space);
      }
      H5Gclose(grp);
    }

    H5Gclose(page);
  }

  H5Fclose(file);

  // Make the UI match what was exported: empty numeric fields become 0.
  normalizeEmptyNumericsToZero(dataset.layout, pages);
  populateModels();

  return true;
}

bool SDDSEditor::writeCSV(const QString &path) {
  if (!datasetLoaded)
    return false;
  commitModels();

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to open output"));
    return false;
  }

  QTextStream out(&file);

  auto escape = [](const QString &txt) {
    QString t = txt;
    t.replace('"', "\"\"");
    bool need = t.contains(',') || t.contains('"') || t.contains('\n');
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
          QString cell = (r < pd.columns[c].size()) ? pd.columns[c][r] : QString();
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
          QString cell = (a < pd.arrays.size() && r < pd.arrays[a].values.size())
                             ? pd.arrays[a].values[r]
                             : QString();
          const int32_t type = dataset.layout.array_definition[a].type;
          if (SDDS_NUMERIC_TYPE(type) && cell.trimmed().isEmpty())
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

  file.close();

  // Make the UI match what was exported: empty numeric fields become 0.
  normalizeEmptyNumericsToZero(dataset.layout, pages);
  populateModels();

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

void SDDSEditor::saveFileAsHDF() {
  QString path = QFileDialog::getSaveFileName(this, tr("Save HDF"), currentFilename,
                                             tr("HDF Files (*.h5 *.hdf);;All Files (*)"));
  if (path.isEmpty())
    return;
  if (writeHDF(path))
    message(tr("Saved %1").arg(path));
}

void SDDSEditor::exportCSV() {
  QString def = currentFilename;
  if (!def.isEmpty()) {
    QFileInfo fi(def);
    def = fi.path() + '/' + fi.completeBaseName() + ".csv";
  }
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
  loadPage(value + 1);
}

void SDDSEditor::loadPage(int page) {
  currentPage = page - 1;
  populateModels();
}

void SDDSEditor::flushPendingEdits() {
  if (QWidget *fw = QApplication::focusWidget()) {
    fw->clearFocus();
    qApp->processEvents();
  }
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
  const bool hugeTable = (rows > 0 && ccount > 0 && (rows * ccount) > 500000);

  // parameters
  if (progress) {
    progress->setLabelText(tr("Preparing display… (parameters)"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  paramBox->setChecked(pcount > 0);
  paramModel->refresh();
  ++doneUnits;
  if (progress)
    updateProgress(false);

  // columns
  colBox->setChecked(ccount > 0);
  if (progress) {
    progress->setLabelText(tr("Preparing display… (columns)"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  columnModel->refresh();
  ++doneUnits;
  if (progress)
    updateProgress(false);


  // Resize columns to fit their contents first so initial widths are reasonable
  // then allow them to stretch to fill the remaining space and be adjusted by
  // the user.
  if (progress) {
    progress->setLabelText(tr("Preparing display… (sizing columns)"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  // For very large tables, computing size-to-contents can be very expensive.
  // During initial load (when progress dialog exists), skip it to speed up load.
  if (!(progress && hugeTable))
    columnView->resizeColumnsToContents();
  columnView->horizontalHeader()->setStretchLastSection(true);
  columnView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);
  ++doneUnits;
  if (progress)
    updateProgress(false);

  // arrays
  arrayBox->setChecked(acount > 0);
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
  if (!(progress && hugeTable))
    arrayView->resizeColumnsToContents();
  arrayView->horizontalHeader()->setStretchLastSection(true);
  arrayView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);

  ++doneUnits;
  if (progress)
    updateProgress(true);


  updatingModels = false;
}

void SDDSEditor::clearDataset() {

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
      free(def->fixed_value);
      def->fixed_value = val.isEmpty() ? nullptr : strdup(val.toLocal8Bit().constData());
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
  QFormLayout form(&dlg);
  QLineEdit name(def->name ? def->name : "", &dlg);
  QLineEdit symbol(def->symbol ? def->symbol : "", &dlg);
  QLineEdit units(def->units ? def->units : "", &dlg);
  QLineEdit desc(def->description ? def->description : "", &dlg);
  QLineEdit fmt(def->format_string ? def->format_string : "", &dlg);
  QLineEdit fixed(def->fixed_value ? def->fixed_value : "", &dlg);
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
  QByteArray ba;
  ba = name.text().toLocal8Bit();
  SDDS_ChangeParameterInformation(&dataset, (char *)"name", ba.data(),
                                  SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, row);

  ba = symbol.text().toLocal8Bit();
  SDDS_ChangeParameterInformation(&dataset, (char *)"symbol",
                                  symbol.text().isEmpty() ? (char *)"" : ba.data(),
                                  SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, row);

  ba = units.text().toLocal8Bit();
  SDDS_ChangeParameterInformation(&dataset, (char *)"units",
                                  units.text().isEmpty() ? (char *)"" : ba.data(),
                                  SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, row);

  ba = desc.text().toLocal8Bit();
  SDDS_ChangeParameterInformation(&dataset, (char *)"description",
                                  desc.text().isEmpty() ? (char *)"" : ba.data(),
                                  SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, row);

  ba = fmt.text().toLocal8Bit();
  SDDS_ChangeParameterInformation(&dataset, (char *)"format_string",
                                  fmt.text().isEmpty() ? NULL : ba.data(),
                                  SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, row);

  ba = fixed.text().toLocal8Bit();
  SDDS_ChangeParameterInformation(&dataset, (char *)"fixed_value",
                                  fixed.text().isEmpty() ? NULL : ba.data(),
                                  SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, row);
  int32_t tval = typeGroup.checkedId();
  SDDS_ChangeParameterInformation(&dataset, (char *)"type", &tval,
                                  SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row);
  paramModel->refreshRowHeaders(row, row);
  markDirty();
}

void SDDSEditor::editColumnAttributes() {
  if (!datasetLoaded)
    return;
  commitModels();
  QModelIndex idx = columnView->currentIndex();
  if (!idx.isValid())
    return;
  int col = idx.column();
  COLUMN_DEFINITION *def = &dataset.layout.column_definition[col];
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Column Attributes"));
  QFormLayout form(&dlg);
  QLineEdit name(def->name ? def->name : "", &dlg);
  QLineEdit symbol(def->symbol ? def->symbol : "", &dlg);
  QLineEdit units(def->units ? def->units : "", &dlg);
  QLineEdit desc(def->description ? def->description : "", &dlg);
  QLineEdit fmt(def->format_string ? def->format_string : "", &dlg);
  QSpinBox length(&dlg);
  length.setRange(0, 1000000);
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
  QByteArray ba;
  ba = name.text().toLocal8Bit();
  SDDS_ChangeColumnInformation(&dataset, (char *)"name", ba.data(),
                               SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = symbol.text().toLocal8Bit();
  SDDS_ChangeColumnInformation(&dataset, (char *)"symbol",
                               symbol.text().isEmpty() ? (char *)"" : ba.data(),
                               SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = units.text().toLocal8Bit();
  SDDS_ChangeColumnInformation(&dataset, (char *)"units",
                               units.text().isEmpty() ? (char *)"" : ba.data(),
                               SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = desc.text().toLocal8Bit();
  SDDS_ChangeColumnInformation(&dataset, (char *)"description",
                               desc.text().isEmpty() ? (char *)"" : ba.data(),
                               SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = fmt.text().toLocal8Bit();
  SDDS_ChangeColumnInformation(&dataset, (char *)"format_string",
                               fmt.text().isEmpty() ? NULL : ba.data(),
                               SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);
  int32_t len = length.value();
  SDDS_ChangeColumnInformation(&dataset, (char *)"field_length", &len,
                               SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, col);
  int32_t tval = typeGroup.checkedId();
  SDDS_ChangeColumnInformation(&dataset, (char *)"type", &tval,
                               SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, col);
  columnModel->refreshHeaders(col, col);
  markDirty();
}

void SDDSEditor::editArrayAttributes() {
  if (!datasetLoaded)
    return;
  commitModels();
  QModelIndex idx = arrayView->currentIndex();
  if (!idx.isValid())
    return;
  int col = idx.column();
  ARRAY_DEFINITION *def = &dataset.layout.array_definition[col];
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Array Attributes"));
  QFormLayout form(&dlg);
  QLineEdit name(def->name ? def->name : "", &dlg);
  QLineEdit symbol(def->symbol ? def->symbol : "", &dlg);
  QLineEdit units(def->units ? def->units : "", &dlg);
  QLineEdit desc(def->description ? def->description : "", &dlg);
  QLineEdit fmt(def->format_string ? def->format_string : "", &dlg);
  QLineEdit group(def->group_name ? def->group_name : "", &dlg);
  QSpinBox length(&dlg);
  length.setRange(0, 1000000);
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
  QByteArray ba;
  ba = name.text().toLocal8Bit();
  SDDS_ChangeArrayInformation(&dataset, (char *)"name", ba.data(),
                              SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = symbol.text().toLocal8Bit();
  SDDS_ChangeArrayInformation(&dataset, (char *)"symbol",
                              symbol.text().isEmpty() ? (char *)"" : ba.data(),
                              SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = units.text().toLocal8Bit();
  SDDS_ChangeArrayInformation(&dataset, (char *)"units",
                              units.text().isEmpty() ? (char *)"" : ba.data(),
                              SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = desc.text().toLocal8Bit();
  SDDS_ChangeArrayInformation(&dataset, (char *)"description",
                              desc.text().isEmpty() ? (char *)"" : ba.data(),
                              SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = fmt.text().toLocal8Bit();
  SDDS_ChangeArrayInformation(&dataset, (char *)"format_string",
                              fmt.text().isEmpty() ? NULL : ba.data(),
                              SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);

  ba = group.text().toLocal8Bit();
  SDDS_ChangeArrayInformation(&dataset, (char *)"group_name",
                              group.text().isEmpty() ? (char *)"" : ba.data(),
                              SDDS_PASS_BY_STRING | SDDS_SET_BY_INDEX, col);
  int32_t dimCnt = dimsCount.value();
  SDDS_ChangeArrayInformation(&dataset, (char *)"dimensions", &dimCnt,
                              SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, col);
  for (PageStore &pd : pages) {
    if (col >= pd.arrays.size())
      continue;
    ArrayStore &as = pd.arrays[col];
    int old = as.dims.size();
    as.dims.resize(dimCnt);
    for (int i = old; i < dimCnt; ++i)
      as.dims[i] = 1;
    as.values.resize(dimProduct(as.dims));
  }
  int32_t len = length.value();
  SDDS_ChangeArrayInformation(&dataset, (char *)"field_length", &len,
                              SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, col);
  int32_t tval = typeGroup.checkedId();
  SDDS_ChangeArrayInformation(&dataset, (char *)"type", &tval,
                              SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, col);
  arrayModel->refreshHeaders(col, col);
  populateModels();
  markDirty();
}
void SDDSEditor::changeParameterType(int row) {
  if (!datasetLoaded)
    return;
  commitModels();
  QStringList types;
  types << "short" << "ushort" << "long" << "ulong" << "long64"
        << "ulong64" << "float" << "double" << "long double" << "string"
        << "character";
  if (row < 0 || row >= dataset.layout.n_parameters)
    return;
  QString name = dataset.layout.parameter_definition[row].name;
  QString current = SDDS_GetTypeName(dataset.layout.parameter_definition[row].type);
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Parameter Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  SDDS_ChangeParameterInformation(&dataset, (char *)"type", &sddsType,
                                  SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                  const_cast<char *>(name.toLocal8Bit().constData()));
  // Type affects validation/formatting; repaint is sufficient.
  paramView->viewport()->update();
  markDirty();
}

void SDDSEditor::showParameterMenu(QTableView *view, int row,
                                   const QPoint &globalPos) {
  if (row < 0 || row >= dataset.layout.n_parameters)
    return;
  QMenu menu(view);
  QAction *delAct = menu.addAction(tr("Delete"));
  QAction *chosen = menu.exec(globalPos);
  if (chosen == delAct) {
    paramView->setCurrentIndex(paramModel->index(row, 0));
    deleteParameter();
  }
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
  showParameterMenu(paramView, idx.row(), paramView->viewport()->mapToGlobal(pos));
}

void SDDSEditor::changeColumnType(int column) {
  if (!datasetLoaded)
    return;
  commitModels();
  QStringList types;
  types << "short" << "ushort" << "long" << "ulong" << "long64"
        << "ulong64" << "float" << "double" << "long double" << "string"
        << "character";
  if (column < 0 || column >= dataset.layout.n_columns)
    return;
  QString name = dataset.layout.column_definition[column].name;
  QString current = SDDS_GetTypeName(dataset.layout.column_definition[column].type);
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Column Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  SDDS_ChangeColumnInformation(&dataset, (char *)"type", &sddsType,
                               SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                               const_cast<char *>(name.toLocal8Bit().constData()));
  columnModel->refreshHeaders(column, column);
  markDirty();
}

void SDDSEditor::showColumnMenu(QTableView *view, int column,
                                const QPoint &globalPos) {
  if (column < 0 || column >= dataset.layout.n_columns)
    return;
  QMenu menu(view);
  QAction *plotAct = menu.addAction(tr("Plot from file"));
  QAction *ascAct = menu.addAction(tr("Sort ascending"));
  QAction *descAct = menu.addAction(tr("Sort descending"));
  QAction *searchAct = menu.addAction(tr("Search/Replace"));
  QAction *delAct = menu.addAction(tr("Delete"));
  QAction *chosen = menu.exec(globalPos);
  if (chosen == plotAct)
    plotColumn(column);
  else if (chosen == ascAct)
    sortColumn(column, Qt::AscendingOrder);
  else if (chosen == descAct)
    sortColumn(column, Qt::DescendingOrder);
  else if (chosen == searchAct)
    searchColumn(column);
  else if (chosen == delAct) {
    columnView->setCurrentIndex(columnModel->index(0, column));
    deleteColumn();
  }
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
  }
}

void SDDSEditor::showArrayMenu(QTableView *view, int column,
                               const QPoint &globalPos) {
  if (column < 0 || column >= dataset.layout.n_arrays)
    return;
  QMenu menu(view);
  QAction *searchAct = menu.addAction(tr("Search"));
  QAction *resizeAct = menu.addAction(tr("Resize"));
  QAction *delAct = menu.addAction(tr("Delete"));
  QAction *chosen = menu.exec(globalPos);
  if (chosen == searchAct)
    searchArray(column);
  else if (chosen == resizeAct)
    resizeArray(column);
  else if (chosen == delAct) {
    arrayView->setCurrentIndex(arrayModel->index(0, column));
    deleteArray();
  }
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
  showArrayMenu(arrayView, idx.column(),
                arrayView->viewport()->mapToGlobal(pos));
}

void SDDSEditor::plotColumn(int column) {
  if (!datasetLoaded || currentFilename.isEmpty())
    return;

  QString colName = dataset.layout.column_definition[column].name;
  bool hasTime = false;
  for (int c = 0; c < dataset.layout.n_columns; ++c) {
    if (QString(dataset.layout.column_definition[c].name) == QLatin1String("Time")) {
      hasTime = true;
      break;
    }
  }

  QStringList args;
  args << "-split=page" << "-sep=page" << currentFilename;
  if (hasTime) {
    args << QString("-col=Time,%1").arg(colName) << "-tick=xtime";
  } else {
    args << QString("-col=%1").arg(colName);
  }

  QProcess::startDetached("sddsplot", args);
}

void SDDSEditor::sortColumn(int column, Qt::SortOrder order) {
  if (!datasetLoaded || currentPage < 0 || currentPage >= pages.size())
    return;

  commitModels();

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
      long double aval = av.toDouble();
      long double bval = bv.toDouble();
      return order == Qt::AscendingOrder ? aval < bval : aval > bval;
    } else if (type == SDDS_STRING) {
      QByteArray aa = av.toUtf8();
      QByteArray bb = bv.toUtf8();
      int r = strcmp_nh(aa.constData(), bb.constData());
      return order == Qt::AscendingOrder ? r < 0 : r > 0;
    }
    return order == Qt::AscendingOrder ? av < bv : av > bv;
  };

  std::stable_sort(idx.begin(), idx.end(), cmp);

  for (int c = 0; c < pd.columns.size(); ++c) {
    QVector<QString> sorted(rows);
    for (int i = 0; i < rows; ++i)
      sorted[i] = idx[i] < pd.columns[c].size() ? pd.columns[c][idx[i]] : QString();
    pd.columns[c] = sorted;
  }

  populateModels();
  markDirty();
}

void SDDSEditor::searchColumn(int column) {
  if (!datasetLoaded)
    return;

  if (searchColumnDialog)
    searchColumnDialog->close();

  QDialog *dlg = new QDialog(this);
  searchColumnDialog = dlg;
  dlg->setWindowTitle(tr("Search Column"));
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowModality(Qt::NonModal);

  QVBoxLayout *layout = new QVBoxLayout(dlg);
  QFormLayout *form = new QFormLayout();
  layout->addLayout(form);

  QLineEdit *patternEdit = new QLineEdit(dlg);
  patternEdit->setText(lastSearchPattern);
  QLineEdit *replaceEdit = new QLineEdit(dlg);
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
      if (!idx.isValid())
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

  auto replaceCurrent = [this, column, patternEdit, replaceEdit, state, runSearch]() {
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
    val.replace(m.start, patternEdit->text().length(), replaceEdit->text());
    columnModel->setData(idx, val);
    markDirty();
    runSearch(true, true);
  };

  auto replaceAll = [this, column, patternEdit, replaceEdit, state, runSearch]() {
    if (state->matches.isEmpty())
      runSearch(true, true);
    if (state->matches.isEmpty())
      return;
    QString pat = patternEdit->text();
    if (pat.isEmpty())
      return;
    QString repl = replaceEdit->text();
    int replaced = 0;
    for (int r = 0; r < columnModel->rowCount(); ++r) {
      QModelIndex idx = columnModel->index(r, column);
      if (!idx.isValid())
        continue;
      QString val = idx.data(Qt::EditRole).toString();
      int pos = 0;
      bool changed = false;
      while ((pos = val.indexOf(pat, pos, Qt::CaseSensitive)) >= 0) {
        val.replace(pos, pat.length(), repl);
        pos += repl.length();
        ++replaced;
        changed = true;
      }
      if (changed)
        columnModel->setData(idx, val);
    }
    if (replaced > 0)
      markDirty();
    runSearch(replaced == 0, true);
  };

  auto replaceSelected = [this, column, patternEdit, replaceEdit, state, runSearch]() {
    QString pat = patternEdit->text();
    if (pat.isEmpty())
      return;
    QItemSelectionModel *sel = columnView->selectionModel();
    if (!sel)
      return;
    QModelIndexList indexes = sel->selectedIndexes();
    if (indexes.isEmpty())
      return;
    QString repl = replaceEdit->text();
    int replaced = 0;
    for (const QModelIndex &idx : indexes) {
      if (!idx.isValid() || idx.column() != column)
        continue;
      QString val = idx.data(Qt::EditRole).toString();
      int pos = 0;
      bool changed = false;
      while ((pos = val.indexOf(pat, pos, Qt::CaseSensitive)) >= 0) {
        val.replace(pos, pat.length(), repl);
        pos += repl.length();
        ++replaced;
        changed = true;
      }
      if (changed)
        columnModel->setData(idx, val);
    }
    if (replaced > 0)
      markDirty();
    runSearch(replaced == 0, false);
  };

  QObject::connect(searchBtn, &QPushButton::clicked, dlg, [runSearch]() { runSearch(true, true); });
  QObject::connect(replaceBtn, &QPushButton::clicked, dlg, replaceCurrent);
  QObject::connect(replaceSelectedBtn, &QPushButton::clicked, dlg, replaceSelected);
  QObject::connect(replaceAllBtn, &QPushButton::clicked, dlg, replaceAll);
  QObject::connect(nextBtn, &QPushButton::clicked, dlg, [state, focusMatch]() {
    if (state->matches.isEmpty())
      return;
    state->matchIndex = (state->matchIndex + 1) % state->matches.size();
    focusMatch();
  });
  QObject::connect(prevBtn, &QPushButton::clicked, dlg, [state, focusMatch]() {
    if (state->matches.isEmpty())
      return;
    state->matchIndex = (state->matchIndex - 1 + state->matches.size()) % state->matches.size();
    focusMatch();
  });
  QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

  QObject::connect(dlg, &QDialog::destroyed, this, [this, state]() {
    if (state->activeEditor.isValid())
      columnView->closePersistentEditor(state->activeEditor);
    searchColumnDialog = nullptr;
  });

  dlg->adjustSize();
  QRect parentFrame = frameGeometry();
  int x = parentFrame.x() + (parentFrame.width() - dlg->width()) / 2;
  int y = parentFrame.y();
  if (y < 0)
    y = 0;
  dlg->move(x, y);
  dlg->show();
  dlg->raise();
  dlg->activateWindow();
}

void SDDSEditor::resizeArray(int column) {
  if (!datasetLoaded || currentPage < 0 || currentPage >= pages.size())
    return;

  ARRAY_DEFINITION *def = &dataset.layout.array_definition[column];
  PageStore &pd = pages[currentPage];
  if (column < 0 || column >= pd.arrays.size())
    return;

  ArrayStore &as = pd.arrays[column];

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Resize Array"));
  QFormLayout form(&dlg);
  QVector<QSpinBox *> boxes(def->dimensions);
  for (int i = 0; i < def->dimensions; ++i) {
    QSpinBox *sb = new QSpinBox(&dlg);
    sb->setRange(1, 1000000);
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

  as.dims.resize(def->dimensions);
  for (int i = 0; i < def->dimensions; ++i)
    as.dims[i] = boxes[i]->value();
  int newSize = dimProduct(as.dims);
  as.values.resize(newSize);

  populateModels();
  markDirty();
}

void SDDSEditor::searchArray(int column) {
  if (!datasetLoaded)
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Search Array"));
  QVBoxLayout layout(&dlg);
  QFormLayout form;
  QLineEdit patternEdit(&dlg);
  patternEdit.setText(lastSearchPattern);
  QLineEdit replaceEdit(&dlg);
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
    val.replace(m.start, patternEdit.text().length(), replaceEdit.text());
    arrayModel->setData(idx, val);
    markDirty();
    runSearch(true);
  };

  auto replaceAll = [&]() {
    if (matches.isEmpty())
      runSearch(true);
    if (matches.isEmpty())
      return;
    QString pat = patternEdit.text();
    if (pat.isEmpty())
      return;
    QString repl = replaceEdit.text();
    int replaced = 0;
    for (int r = 0; r < arrayModel->rowCount(); ++r) {
      QModelIndex idx = arrayModel->index(r, column);
      if (!idx.isValid())
        continue;
      QString val = idx.data(Qt::EditRole).toString();
      int pos = 0;
      bool changed = false;
      while ((pos = val.indexOf(pat, pos, Qt::CaseSensitive)) >= 0) {
        val.replace(pos, pat.length(), repl);
        pos += repl.length();
        ++replaced;
        changed = true;
      }
      if (changed)
        arrayModel->setData(idx, val);
    }
    if (replaced > 0)
      markDirty();
    runSearch(replaced == 0);
  };

  QObject::connect(&searchBtn, &QPushButton::clicked, [&]() { runSearch(true); });
  QObject::connect(&replaceBtn, &QPushButton::clicked, replaceCurrent);
  QObject::connect(&replaceAllBtn, &QPushButton::clicked, replaceAll);
  QObject::connect(&nextBtn, &QPushButton::clicked, [&]() {
    if (matches.isEmpty())
      return;
    matchIndex = (matchIndex + 1) % matches.size();
    focusMatch();
  });
  QObject::connect(&prevBtn, &QPushButton::clicked, [&]() {
    if (matches.isEmpty())
      return;
    matchIndex = (matchIndex - 1 + matches.size()) % matches.size();
    focusMatch();
  });
  QObject::connect(&closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

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
        << "ulong64" << "float" << "double" << "long double" << "string"
        << "character";
  if (column < 0 || column >= dataset.layout.n_arrays)
    return;
  QString name = dataset.layout.array_definition[column].name;
  QString current = SDDS_GetTypeName(dataset.layout.array_definition[column].type);
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Array Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  SDDS_ChangeArrayInformation(&dataset, (char *)"type", &sddsType,
                              SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              const_cast<char *>(name.toLocal8Bit().constData()));
  arrayModel->refreshHeaders(column, column);
  markDirty();
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
    layout->parameter_definition =
        (PARAMETER_DEFINITION *)realloc(defs, sizeof(PARAMETER_DEFINITION) * (count - 1));
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
    layout->parameter_index =
        (SORTED_INDEX **)realloc(indexes, sizeof(SORTED_INDEX *) * (count - 1));
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
    layout->column_definition =
        (COLUMN_DEFINITION *)realloc(defs, sizeof(COLUMN_DEFINITION) * (count - 1));
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
    layout->column_index =
        (SORTED_INDEX **)realloc(indexes, sizeof(SORTED_INDEX *) * (count - 1));
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
    layout->array_definition =
        (ARRAY_DEFINITION *)realloc(defs, sizeof(ARRAY_DEFINITION) * (count - 1));
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
    layout->array_index =
        (SORTED_INDEX **)realloc(indexes, sizeof(SORTED_INDEX *) * (count - 1));
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

  QDialog dlg(this);
  dlg.setWindowTitle(tr("New Parameter"));
  QFormLayout form(&dlg);
  QLineEdit name(&dlg);
  QLineEdit symbol(&dlg);
  QLineEdit units(&dlg);
  QLineEdit desc(&dlg);
  QLineEdit fmt(&dlg);
  QLineEdit fixed(&dlg);
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

  QByteArray baName = name.text().toLocal8Bit();
  QByteArray baSym = symbol.text().toLocal8Bit();
  QByteArray baUnits = units.text().toLocal8Bit();
  QByteArray baDesc = desc.text().toLocal8Bit();
  QByteArray baFmt = fmt.text().toLocal8Bit();
  QByteArray baFixed = fixed.text().toLocal8Bit();

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

  SDDS_SaveLayout(&dataset);
  for (PageStore &pd : pages)
    pd.parameters.append(QString());

  populateModels();
  markDirty();
}

void SDDSEditor::insertColumn() {
  if (!ensureDataset())
    return;

  commitModels();

  QDialog dlg(this);
  dlg.setWindowTitle(tr("New Column"));
  QFormLayout form(&dlg);
  QLineEdit name(&dlg);
  QLineEdit symbol(&dlg);
  QLineEdit units(&dlg);
  QLineEdit desc(&dlg);
  QLineEdit fmt(&dlg);
  QSpinBox length(&dlg);
  length.setRange(0, 1000000);
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

  SDDS_SaveLayout(&dataset);
  for (PageStore &pd : pages) {
    int rows = pd.columns.size() ? pd.columns[0].size() : 0;
    pd.columns.append(QVector<QString>(rows));
  }

  populateModels();
  markDirty();
}

void SDDSEditor::insertArray() {
  if (!ensureDataset())
    return;

  commitModels();

  QDialog dlg(this);
  dlg.setWindowTitle(tr("New Array"));
  QFormLayout form(&dlg);
  QLineEdit name(&dlg);
  QLineEdit symbol(&dlg);
  QLineEdit units(&dlg);
  QLineEdit desc(&dlg);
  QLineEdit fmt(&dlg);
  QLineEdit group(&dlg);
  QSpinBox length(&dlg);
  length.setRange(0, 1000000);
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

  SDDS_SaveLayout(&dataset);
  for (PageStore &pd : pages) {
    ArrayStore as;
    as.dims = QVector<int>(1, 5);
    as.values.resize(5);
    pd.arrays.append(as);
  }

  populateModels();
  markDirty();
}

void SDDSEditor::deleteParameter() {
  if (!datasetLoaded)
    return;
  commitModels();
  QModelIndex idx = paramView->currentIndex();
  if (!idx.isValid())
    return;
  int row = idx.row();
  SDDS_LAYOUT *layout = &dataset.layout;
  if (row < 0 || row >= layout->n_parameters)
    return;

  removeParameterFromLayout(&dataset.layout, row);
  SDDS_SaveLayout(&dataset);
  for (PageStore &pd : pages)
    if (row < pd.parameters.size())
      pd.parameters.remove(row);

  populateModels();
  markDirty();
}

void SDDSEditor::deleteColumn() {
  if (!datasetLoaded)
    return;
  commitModels();
  QModelIndex idx = columnView->currentIndex();
  if (!idx.isValid())
    return;
  int col = idx.column();
  SDDS_LAYOUT *layout = &dataset.layout;
  if (col < 0 || col >= layout->n_columns)
    return;

  removeColumnFromLayout(&dataset.layout, col);
  SDDS_SaveLayout(&dataset);
  for (PageStore &pd : pages)
    if (col < pd.columns.size())
      pd.columns.remove(col);

  populateModels();
  markDirty();
}

void SDDSEditor::deleteArray() {
  if (!datasetLoaded)
    return;
  commitModels();
  QModelIndex idx = arrayView->currentIndex();
  if (!idx.isValid())
    return;
  int col = idx.column();
  SDDS_LAYOUT *layout = &dataset.layout;
  if (col < 0 || col >= layout->n_arrays)
    return;

  removeArrayFromLayout(&dataset.layout, col);
  SDDS_SaveLayout(&dataset);
  for (PageStore &pd : pages)
    if (col < pd.arrays.size())
      pd.arrays.remove(col);

  populateModels();
  markDirty();
}

void SDDSEditor::insertColumnRows() {
  if (!datasetLoaded)
    return;

  commitModels();

  bool ok = false;
  int rowsToAdd =
      QInputDialog::getInt(this, tr("Insert Rows"), tr("Number of rows"),
                           lastRowAddCount, 1, 1000000, 1, &ok);
  if (!ok || rowsToAdd <= 0)
    return;
  lastRowAddCount = rowsToAdd;

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
}

void SDDSEditor::deleteColumnRows() {
  if (!datasetLoaded)
    return;

  commitModels();

  QModelIndexList selection = columnView->selectionModel()->selectedRows();
  if (selection.isEmpty())
    return;

  QVector<int> rows;
  rows.reserve(selection.size());
  for (const QModelIndex &idx : selection)
    rows.append(idx.row());
  std::sort(rows.begin(), rows.end(), std::greater<int>());

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
}

void SDDSEditor::clonePage() {
  if (!datasetLoaded || currentPage < 0 || currentPage >= pages.size())
    return;

  commitModels();
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
}

void SDDSEditor::insertPage() {
  if (!ensureDataset())
    return;

  commitModels();

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
    pd.arrays[a].values.resize(dimProduct(pd.arrays[a].dims));
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
}

void SDDSEditor::deletePage() {
  if (!datasetLoaded || pages.size() <= 1 || currentPage < 0 ||
      currentPage >= pages.size())
    return;

  commitModels();
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
}

void SDDSEditor::applyTheme(bool dark) {
  darkPalette = dark;
  pageCombo->setStyleSheet(dark
      ? "QComboBox { color: white; background-color: #303030; } "
        "QComboBox QAbstractItemView { color: white; background-color: #303030; }"
      : "");
  const QString headerStyle = dark
      ? "QHeaderView::section { background-color: #404040; color: white; }"
      : "QHeaderView::section { background-color: #f0f0f0; }";
  paramView->horizontalHeader()->setStyleSheet(headerStyle);
  paramView->verticalHeader()->setStyleSheet(headerStyle);
  columnView->horizontalHeader()->setStyleSheet(headerStyle);
  columnView->verticalHeader()->setStyleSheet(headerStyle);
  arrayView->horizontalHeader()->setStyleSheet(headerStyle);
  arrayView->verticalHeader()->setStyleSheet(headerStyle);
  dataSplitter->setStyleSheet(dark
      ? "QSplitter::handle { background-color: #303030; }"
      : "QSplitter::handle { background-color: lightgrey; }");
}

void SDDSEditor::changeEvent(QEvent *event) {
  if (event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::PaletteChange) {
    QPalette pal = qApp->palette();
    bool dark = pal.color(QPalette::Window).lightness() < 128;
    QColor textColor = dark ? Qt::white : Qt::black;
    pal.setColor(QPalette::Active, QPalette::Text, textColor);
    pal.setColor(QPalette::Inactive, QPalette::Text, textColor);
    pal.setColor(QPalette::Active, QPalette::WindowText, textColor);
    pal.setColor(QPalette::Inactive, QPalette::WindowText, textColor);
    qApp->setPalette(pal);
    applyTheme(dark);
  }
  QMainWindow::changeEvent(event);
}

void SDDSEditor::restartApp() {
  if (!maybeSave())
    return;
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
  QVBoxLayout layout(&dlg);
  QPlainTextEdit text(&dlg);
  text.setReadOnly(true);
  text.setPlainText(tr("Open a file using File->Open.\n"
                       "Select a page and edit parameters, columns or arrays in the tables.\n"
                       "Right click headers for more actions such as:\n"
                       " - Plotting a column\n"
                       " - Sorting column or array data\n"
                       " - Searching or replacing values in columns or arrays\n"
                       " - Resizing arrays\n"
                       "Use the Edit menu to insert or delete items, and File->Save to commit changes."));
  text.setMinimumSize(400, 300);
  layout.addWidget(&text);
  QDialogButtonBox box(QDialogButtonBox::Ok, Qt::Horizontal, &dlg);
  connect(&box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  layout.addWidget(&box);
  dlg.exec();
}

#include "SDDSEditor_moc.h"
