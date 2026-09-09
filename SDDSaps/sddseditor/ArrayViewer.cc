/**
 * @file ArrayViewer.cc
 * @brief Grid editing and slice navigation for multidimensional SDDS arrays.
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license See LICENSE in the repository root.
 */
#include "ArrayViewer.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QLinearGradient>
#include <QTimer>
#include <QCloseEvent>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTableView>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
#include <cmath>
#include <sstream>
#include <locale>
#include <iomanip>

static const char *arrayCellsMime = "application/x-sddseditor-array-cells";

/** Parse finite SDDS numbers without narrowing integers or long doubles to double. */
static bool heatmapNumber(const QString &text, long double *value) {
  std::istringstream input(text.toStdString());
  input.imbue(std::locale::classic());
  if (!(input >> *value) || !std::isfinite(*value))
    return false;
  input >> std::ws;
  return input.eof();
}

/** Format limits without Qt's floating formatter narrowing long double values. */
static QString heatmapText(long double value, int precision = std::numeric_limits<long double>::max_digits10) {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(precision) << value;
  return QString::fromStdString(output.str());
}

/** A sequential purple-to-yellow palette, shared by cells and the legend. */
static QColor heatmapColor(long double fraction) {
  static const QColor stops[] = {QColor(68, 1, 84), QColor(59, 82, 139),
                                  QColor(33, 145, 140), QColor(94, 201, 98), QColor(253, 231, 37)};
  const double position = static_cast<double>(std::max(0.0L, std::min(1.0L, fraction))) * 4;
  const int first = std::min(3, static_cast<int>(position));
  const double mix = position - first;
  return QColor(qRound(stops[first].red() * (1 - mix) + stops[first + 1].red() * mix),
                qRound(stops[first].green() * (1 - mix) + stops[first + 1].green() * mix),
                qRound(stops[first].blue() * (1 - mix) + stops[first + 1].blue() * mix));
}

/** Draw the same continuous scale used for numeric cell backgrounds. */
class HeatmapColorBar : public QWidget {
public:
  explicit HeatmapColorBar(QWidget *parent) : QWidget(parent) {
    setMinimumWidth(100);
    setFixedHeight(16);
  }
protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    QLinearGradient gradient(0, 0, width(), 0);
    for (int i = 0; i <= 4; ++i)
      gradient.setColorAt(i / 4.0, heatmapColor(i / 4.0L));
    painter.fillRect(rect(), gradient);
  }
};

/** Commit the delegate's editor even when its top-level window has lost focus. */
class ArraySliceTableView : public QTableView {
public:
  explicit ArraySliceTableView(QWidget *parent) : QTableView(parent) {}
  void finishEditing() {
    const auto editors = viewport()->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly);
    for (QLineEdit *editor : editors) {
      if (!editor->isHidden()) {
        commitData(editor);
        closeEditor(editor, QAbstractItemDelegate::NoHint);
      }
    }
  }
};

/** Connect an array slice to the editor's existing data and validated edit path. */
ArraySliceModel::ArraySliceModel(QAbstractItemModel *source, Edit edit, Validate validate, QObject *parent)
    : QAbstractTableModel(parent), source(source), edit(std::move(edit)), validate(std::move(validate)) {}

/** Reset only the grid mapping; underlying values and undo history are unchanged. */
void ArraySliceModel::configure(const ArrayViewerState &newState, int rows, int columns, const QVector<int> &indices) {
  beginResetModel();
  state = newState;
  rowAxis = rows;
  columnAxis = columns;
  fixed = indices;
  validShape = source && state.column >= 0 && state.column < source->columnCount() &&
               rowAxis >= 0 && rowAxis < state.dimensions.size() &&
               columnAxis >= -1 && columnAxis < state.dimensions.size() && columnAxis != rowAxis &&
               fixed.size() == state.dimensions.size();
  qint64 elements = 1;
  for (int i = 0; i < state.dimensions.size() && validShape; ++i) {
    const int length = state.dimensions[i];
    if (length <= 0 || elements > std::numeric_limits<int>::max() / length ||
        fixed[i] < 0 || fixed[i] >= length) {
      validShape = false;
      break;
    }
    elements *= length;
  }
  validShape = validShape && elements == state.elements && elements <= source->rowCount();
  endResetModel();
}

int ArraySliceModel::rowCount(const QModelIndex &parent) const {
  return !parent.isValid() && validShape ? state.dimensions[rowAxis] : 0;
}

int ArraySliceModel::columnCount(const QModelIndex &parent) const {
  return !parent.isValid() && validShape ? (columnAxis < 0 ? 1 : state.dimensions[columnAxis]) : 0;
}

/** Return the complete zero-based index for a displayed cell. */
QVector<int> ArraySliceModel::coordinates(const QModelIndex &index) const {
  if (!index.isValid() || index.model() != this || index.row() >= rowCount() || index.column() >= columnCount())
    return {};
  QVector<int> result = fixed;
  result[rowAxis] = index.row();
  if (columnAxis >= 0)
    result[columnAxis] = index.column();
  return result;
}

/** SDDS contiguous arrays store the last dimension fastest. */
QModelIndex ArraySliceModel::sourceIndex(const QModelIndex &index) const {
  const QVector<int> indices = coordinates(index);
  if (indices.isEmpty() || !source)
    return {};
  qint64 offset = 0;
  for (int i = 0; i < indices.size(); ++i)
    offset = offset * state.dimensions[i] + indices[i];
  return source->index(static_cast<int>(offset), state.column);
}

QVariant ArraySliceModel::data(const QModelIndex &index, int role) const {
  const QModelIndex original = sourceIndex(index);
  if (!original.isValid())
    return {};
  if (heatmap && state.numeric && (role == Qt::BackgroundRole || role == Qt::ForegroundRole)) {
    QColor color(160, 160, 160);
    long double value;
    if (heatmapRangeValid && heatmapNumber(original.data(Qt::EditRole).toString(), &value)) {
      long double fraction = 0.5L;
      if (value < heatmapMinimum)
        fraction = 0;
      else if (value > heatmapMaximum)
        fraction = 1;
      else if (heatmapMinimum != heatmapMaximum) {
        const long double span = heatmapMaximum - heatmapMinimum;
        fraction = std::isfinite(span) ? (value - heatmapMinimum) / span :
                   (value / 2 - heatmapMinimum / 2) / (heatmapMaximum / 2 - heatmapMinimum / 2);
      }
      color = heatmapColor(fraction);
    }
    return QBrush(role == Qt::BackgroundRole ? color : (qGray(color.rgb()) < 140 ? QColor(Qt::white) : QColor(Qt::black)));
  }
  if (role == Qt::ToolTipRole) {
    QStringList indices;
    for (int value : coordinates(index))
      indices << QString::number(value);
    return QString("%1[%2] = %3").arg(state.name, indices.join(", "), original.data(Qt::EditRole).toString());
  }
  return original.data(role);
}

QVariant ArraySliceModel::headerData(int section, Qt::Orientation, int role) const {
  return role == Qt::DisplayRole ? QVariant(QString::number(section)) : QVariant();
}

Qt::ItemFlags ArraySliceModel::flags(const QModelIndex &index) const {
  return sourceIndex(index).isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable : Qt::NoItemFlags;
}

bool ArraySliceModel::accepts(const QString &value) const {
  return validate && validate(value);
}

bool ArraySliceModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  const QModelIndex original = sourceIndex(index);
  return role == Qt::EditRole && original.isValid() && accepts(value.toString()) && edit && edit(original, value.toString());
}

void ArraySliceModel::refreshValues() {
  if (rowCount() && columnCount())
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::BackgroundRole, Qt::ForegroundRole});
}

/** Scan only the current slice and ignore missing, invalid, NaN and infinite values. */
bool ArraySliceModel::finiteRange(long double *minimum, long double *maximum) const {
  bool found = false;
  for (int r = 0; r < rowCount(); ++r) {
    for (int c = 0; c < columnCount(); ++c) {
      long double value;
      if (!heatmapNumber(data(index(r, c), Qt::EditRole).toString(), &value))
        continue;
      if (!found) {
        *minimum = *maximum = value;
        found = true;
      } else {
        *minimum = std::min(*minimum, value);
        *maximum = std::max(*maximum, value);
      }
    }
  }
  return found;
}

/** Heatmap state affects presentation only; edits and clipboard retain exact text. */
void ArraySliceModel::setHeatmap(bool enabled, bool validRange, long double minimum, long double maximum) {
  heatmap = enabled;
  heatmapRangeValid = validRange;
  heatmapMinimum = minimum;
  heatmapMaximum = maximum;
  if (rowCount() && columnCount())
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::BackgroundRole, Qt::ForegroundRole});
}

/** Build the slice controls and connect updates to the shared source model. */
ArrayViewer::ArrayViewer(QAbstractItemModel *source, QUndoStack *undo, State state,
                         ArraySliceModel::Edit edit, ArraySliceModel::Validate validate, QWidget *parent)
    : QDialog(parent, Qt::Window), getState(std::move(state)), undo(undo) {
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowModality(Qt::NonModal);
  resize(900, 620);
  QVBoxLayout *layout = new QVBoxLayout(this);
  summary = new QLabel(this);
  summary->setTextFormat(Qt::PlainText);
  summary->setWordWrap(true);
  layout->addWidget(summary);
  QToolBar *toolbar = new QToolBar(this);
  layout->addWidget(toolbar);
  QAction *copy = toolbar->addAction(tr("Copy selection"));
  QAction *paste = toolbar->addAction(tr("Paste"));
  QAction *copySlice = toolbar->addAction(tr("Copy slice"));
  connect(copy, &QAction::triggered, this, [this]() { copySelection(); });
  connect(copySlice, &QAction::triggered, this, [this]() { copySelection(true); });
  connect(paste, &QAction::triggered, this, [this]() { finishEditing(); pasteText(QApplication::clipboard()->text()); });
  toolbar->addSeparator();
  QAction *undoAction = new QAction(tr("Undo"), this);
  QAction *redoAction = new QAction(tr("Redo"), this);
  undoAction->setObjectName("arrayViewerUndo");
  redoAction->setObjectName("arrayViewerRedo");
  undoAction->setEnabled(undo->canUndo());
  redoAction->setEnabled(undo->canRedo());
  connect(undo, &QUndoStack::canUndoChanged, undoAction, &QAction::setEnabled);
  connect(undo, &QUndoStack::canRedoChanged, redoAction, &QAction::setEnabled);
  connect(undoAction, &QAction::triggered, this, [this]() { finishEditing(); this->undo->undo(); });
  connect(redoAction, &QAction::triggered, this, [this]() { finishEditing(); this->undo->redo(); });
  undoAction->setShortcut(QKeySequence::Undo);
  redoAction->setShortcut(QKeySequence::Redo);
  toolbar->addAction(undoAction);
  toolbar->addAction(redoAction);

  QHBoxLayout *axes = new QHBoxLayout;
  rowChoice = new QComboBox(this);
  rowChoice->setObjectName("arrayRowDimension");
  columnChoice = new QComboBox(this);
  columnChoice->setObjectName("arrayColumnDimension");
  axes->addWidget(new QLabel(tr("Rows:"), this));
  axes->addWidget(rowChoice);
  axes->addWidget(new QLabel(tr("Columns:"), this));
  axes->addWidget(columnChoice);
  axes->addStretch();
  axes->addWidget(new QLabel(tr("All indices start at 0"), this));
  layout->addLayout(axes);
  QHBoxLayout *colors = new QHBoxLayout;
  heatmapChoice = new QCheckBox(tr("Heatmap"), this);
  heatmapChoice->setObjectName("arrayHeatmap");
  heatmapScale = new QComboBox(this);
  heatmapScale->setObjectName("arrayHeatmapScale");
  heatmapScale->addItems({tr("Current slice"), tr("Fixed range")});
  rangeMinimum = new QLineEdit(this);
  rangeMinimum->setObjectName("arrayHeatmapMinimum");
  rangeMaximum = new QLineEdit(this);
  rangeMaximum->setObjectName("arrayHeatmapMaximum");
  for (QLineEdit *field : {rangeMinimum, rangeMaximum}) {
    field->setMaximumWidth(155);
    field->setToolTip(tr("A finite number; scientific notation is supported."));
  }
  rangeApply = new QPushButton(tr("Apply"), this);
  rangeApply->setObjectName("arrayHeatmapApply");
  rangeApply->setAutoDefault(false);
  colors->addWidget(heatmapChoice);
  colors->addWidget(heatmapScale);
  colors->addWidget(new QLabel(tr("Min:"), this));
  colors->addWidget(rangeMinimum);
  colors->addWidget(new QLabel(tr("Max:"), this));
  colors->addWidget(rangeMaximum);
  colors->addWidget(rangeApply);
  colors->addStretch();
  layout->addLayout(colors);
  heatmapLegend = new QWidget(this);
  QHBoxLayout *legend = new QHBoxLayout(heatmapLegend);
  legend->setContentsMargins(0, 0, 0, 0);
  legendMinimum = new QLabel(heatmapLegend);
  legendMaximum = new QLabel(heatmapLegend);
  legend->addWidget(legendMinimum);
  legend->addWidget(new HeatmapColorBar(heatmapLegend), 1);
  legend->addWidget(legendMaximum);
  legend->addWidget(new QLabel(tr("Gray: missing / nonfinite"), heatmapLegend));
  layout->addWidget(heatmapLegend);
  heatmapStatus = new QLabel(this);
  heatmapStatus->setObjectName("arrayHeatmapStatus");
  heatmapStatus->setWordWrap(true);
  layout->addWidget(heatmapStatus);
  QScrollArea *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setMaximumHeight(155);
  QWidget *slices = new QWidget(scroll);
  sliceControls = new QVBoxLayout(slices);
  scroll->setWidget(slices);
  layout->addWidget(scroll);
  notice = new QLabel(this);
  notice->setTextFormat(Qt::PlainText);
  notice->setWordWrap(true);
  layout->addWidget(notice);
  grid = new ArraySliceTableView(this);
  grid->setObjectName("arraySliceGrid");
  grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
  grid->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  grid->horizontalHeader()->setDefaultSectionSize(110);
  model = new ArraySliceModel(source, std::move(edit), std::move(validate), this);
  grid->setModel(model);
  layout->addWidget(grid, 1);
  selection = new QLabel(this);
  selection->setTextFormat(Qt::PlainText);
  selection->setWordWrap(true);
  layout->addWidget(selection);
  layout->addWidget(new QLabel(tr("Edits are shared with the main editor. Save the SDDS file to keep them."), this));

  auto shortcut = [this](QKeySequence key, std::function<void()> action) {
    QShortcut *s = new QShortcut(key, grid);
    s->setContext(Qt::WidgetShortcut);
    connect(s, &QShortcut::activated, this, action);
  };
  shortcut(QKeySequence::Copy, [this]() { copySelection(); });
  shortcut(QKeySequence::Paste, [this]() { pasteText(QApplication::clipboard()->text()); });
  connect(rowChoice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int axis) { changeAxis(true, axis); });
  connect(columnChoice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int axis) { changeAxis(false, axis); });
  connect(source, &QAbstractItemModel::modelReset, this, &ArrayViewer::refresh);
  connect(source, &QAbstractItemModel::headerDataChanged, this, [this]() { refresh(); });
  heatmapRefresh = new QTimer(this);
  heatmapRefresh->setSingleShot(true);
  heatmapRefresh->setInterval(0);
  connect(heatmapRefresh, &QTimer::timeout, this, &ArrayViewer::updateHeatmap);
  // Coalesce edits in a paste or undo macro into one range scan.
  connect(source, &QAbstractItemModel::dataChanged, this, [this]() {
    model->refreshValues();
    updateSelection();
    if (heatmapChoice->isChecked() && this->state.numeric)
      heatmapRefresh->start();
  });
  connect(heatmapChoice, &QCheckBox::toggled, this, [this]() { finishEditing(); updateHeatmap(); });
  connect(heatmapScale, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
    finishEditing();
    if (heatmapScale->currentIndex() == 1) {
      if (!fixedRangeValid) {
        fixedRangeValid = model->finiteRange(&fixedMinimum, &fixedMaximum);
        if (!fixedRangeValid) {
          fixedMinimum = 0;
          fixedMaximum = 1;
          fixedRangeValid = true;
        }
      }
      rangeMinimum->setText(heatmapText(fixedMinimum));
      rangeMaximum->setText(heatmapText(fixedMaximum));
    }
    updateHeatmap();
  });
  connect(rangeApply, &QPushButton::clicked, this, &ArrayViewer::applyHeatmapRange);
  connect(rangeMinimum, &QLineEdit::returnPressed, this, &ArrayViewer::applyHeatmapRange);
  connect(rangeMaximum, &QLineEdit::returnPressed, this, &ArrayViewer::applyHeatmapRange);
  connect(grid->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { updateSelection(); });
  refresh();
}

/** Refresh page metadata, preserving valid axes and clamping out-of-range slices. */
void ArrayViewer::refresh() {
  const ArrayViewerState next = getState();
  bool adjusted = false;
  state = next;
  const int count = state.dimensions.size();
  const int oldRow = rowAxis, oldColumn = columnAxis;
  if (rowAxis < 0 || rowAxis >= count)
    rowAxis = count > 1 ? count - 2 : (count == 1 ? 0 : -1);
  if (count < 2)
    columnAxis = -1;
  else if (columnAxis < 0 || columnAxis >= count || columnAxis == rowAxis)
    columnAxis = rowAxis == count - 1 ? count - 2 : count - 1;
  adjusted = oldRow >= 0 && (oldRow != rowAxis || oldColumn != columnAxis);
  fixed.resize(count);
  for (int i = 0; i < count; ++i) {
    const int clamped = std::max(0, std::min(fixed[i], state.dimensions[i] - 1));
    adjusted = adjusted || clamped != fixed[i];
    fixed[i] = clamped;
  }
  const QSignalBlocker blockRows(rowChoice), blockColumns(columnChoice);
  rowChoice->clear();
  columnChoice->clear();
  for (int i = 0; i < count; ++i) {
    rowChoice->addItem(tr("Dimension %1").arg(i));
    columnChoice->addItem(tr("Dimension %1").arg(i));
  }
  rowChoice->setCurrentIndex(rowAxis);
  if (count == 1) {
    columnChoice->clear();
    columnChoice->addItem(tr("Single column"));
  } else {
    columnChoice->setCurrentIndex(columnAxis);
  }
  rowChoice->setEnabled(count > 1);
  columnChoice->setEnabled(count > 1);
  QStringList shape;
  for (int value : state.dimensions)
    shape << QString::number(value);
  setWindowTitle(tr("Array Viewer — %1").arg(state.name));
  summary->setText(tr("Array: %1    Page: %2    Shape: %3%4")
                   .arg(state.name).arg(state.page + 1).arg(shape.join(" × "))
                   .arg(state.units.isEmpty() ? QString() : tr("    Units: %1").arg(state.units)));
  notice->setText(adjusted ? tr("Slice selection adjusted to fit this page's array shape.") : QString());
  rebuildSliceControls();
  applySlice();
}

/** Selecting the other axis swaps axes, so two dimensions never overlap. */
void ArrayViewer::changeAxis(bool rows, int axis) {
  if (axis < 0 || state.dimensions.size() < 2)
    return;
  finishEditing();
  if (rows) {
    if (axis == columnAxis)
      columnAxis = rowAxis;
    rowAxis = axis;
  } else {
    if (axis == rowAxis)
      rowAxis = columnAxis;
    columnAxis = axis;
  }
  const QSignalBlocker blockRows(rowChoice), blockColumns(columnChoice);
  rowChoice->setCurrentIndex(rowAxis);
  columnChoice->setCurrentIndex(columnAxis);
  notice->clear();
  rebuildSliceControls();
  applySlice();
}

/** Each undisplayed dimension has a bounded spin box and matching slider. */
void ArrayViewer::rebuildSliceControls() {
  while (QLayoutItem *item = sliceControls->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  for (int i = 0; i < fixed.size(); ++i) {
    if (i == rowAxis || i == columnAxis)
      continue;
    QWidget *row = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("Dimension %1:").arg(i), row));
    QSpinBox *index = new QSpinBox(row);
    index->setObjectName(QString("arraySliceIndex%1").arg(i));
    index->setRange(0, std::max(0, state.dimensions[i] - 1));
    index->setValue(fixed[i]);
    QSlider *slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(index->minimum(), index->maximum());
    slider->setValue(fixed[i]);
    index->setEnabled(state.dimensions[i] > 0);
    slider->setEnabled(state.dimensions[i] > 0);
    layout->addWidget(index);
    layout->addWidget(slider, 1);
    connect(slider, &QSlider::valueChanged, index, &QSpinBox::setValue);
    connect(index, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
    connect(index, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int value) {
      finishEditing();
      fixed[i] = value;
      notice->clear();
      applySlice();
    });
    sliceControls->addWidget(row);
  }
  if (sliceControls->count() == 0)
    sliceControls->addWidget(new QLabel(tr("The complete array is displayed."), this));
}

void ArrayViewer::applySlice() {
  model->configure(state, rowAxis, columnAxis, fixed);
  updateHeatmap();
  if (state.column < 0)
    notice->setText(tr("This array is unavailable. It may have been deleted or renamed."));
  else if (!state.elements)
    notice->setText(tr("This array is empty on the current page."));
  else if (!model->rowCount())
    notice->setText(tr("The array shape does not match its stored elements."));
  updateSelection();
}

/** Recompute automatic bounds only when needed; fixed bounds survive page/slice changes. */
void ArrayViewer::updateHeatmap() {
  heatmapRefresh->stop();
  const bool numeric = state.numeric && state.column >= 0;
  const bool enabled = numeric && heatmapChoice->isChecked();
  const bool fixedScale = heatmapScale->currentIndex() == 1;
  heatmapChoice->setEnabled(numeric);
  heatmapChoice->setToolTip(numeric ? tr("Color numerical cells while keeping their values editable.") : tr("Heatmaps are available for numeric arrays only."));
  heatmapScale->setEnabled(enabled);
  rangeMinimum->setEnabled(enabled);
  rangeMaximum->setEnabled(enabled);
  rangeMinimum->setReadOnly(!fixedScale);
  rangeMaximum->setReadOnly(!fixedScale);
  rangeApply->setEnabled(enabled && fixedScale);
  heatmapLegend->setVisible(enabled);
  heatmapStatus->clear();
  long double minimum = fixedMinimum, maximum = fixedMaximum;
  bool valid = false;
  if (enabled) {
    valid = fixedScale ? fixedRangeValid : model->finiteRange(&minimum, &maximum);
    if (!fixedScale) {
      rangeMinimum->setText(valid ? heatmapText(minimum) : QString());
      rangeMaximum->setText(valid ? heatmapText(maximum) : QString());
    }
    legendMinimum->setText(valid ? heatmapText(minimum, 8) : tr("No range"));
    legendMaximum->setText(valid ? heatmapText(maximum, 8) : QString());
    legendMinimum->setToolTip(valid ? heatmapText(minimum) : QString());
    legendMaximum->setToolTip(valid ? heatmapText(maximum) : QString());
    if (!valid)
      heatmapStatus->setText(tr("This slice has no finite numeric values to scale."));
    else if (fixedScale)
      heatmapStatus->setText(tr("Fixed range: values outside the bounds use the end colors."));
    else if (minimum == maximum)
      heatmapStatus->setText(tr("All finite values are equal; the middle color is used."));
  }
  model->setHeatmap(enabled, valid, minimum, maximum);
}

/** Reject invalid limits without changing the active color range or document. */
void ArrayViewer::applyHeatmapRange() {
  if (!rangeApply->isEnabled())
    return;
  long double minimum, maximum;
  if (!heatmapNumber(rangeMinimum->text(), &minimum) || !heatmapNumber(rangeMaximum->text(), &maximum) || minimum > maximum) {
    heatmapStatus->setText(tr("Enter finite limits with minimum less than or equal to maximum. The previous range is still active."));
    return;
  }
  fixedMinimum = minimum;
  fixedMaximum = maximum;
  fixedRangeValid = true;
  updateHeatmap();
}

void ArrayViewer::updateSelection() {
  const QModelIndex current = grid->currentIndex();
  selection->setText(current.isValid() ? model->data(current, Qt::ToolTipRole).toString() : tr("Select a cell to see its complete array index and value."));
}

void ArrayViewer::finishEditing() {
  static_cast<ArraySliceTableView *>(grid)->finishEditing();
}

/** Commit an active cell before closing the independent viewer window. */
void ArrayViewer::closeEvent(QCloseEvent *event) {
  finishEditing();
  QDialog::closeEvent(event);
}

/** Copy TSV plus a private lossless representation for strings containing tabs/newlines. */
void ArrayViewer::copySelection(bool wholeSlice) {
  finishEditing();
  int top = 0, left = 0, bottom = model->rowCount() - 1, right = model->columnCount() - 1;
  if (!wholeSlice) {
    const QModelIndexList selected = grid->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
      return;
    top = model->rowCount();
    left = model->columnCount();
    bottom = right = -1;
    for (const QModelIndex &index : selected) {
      top = std::min(top, index.row());
      left = std::min(left, index.column());
      bottom = std::max(bottom, index.row());
      right = std::max(right, index.column());
    }
  }
  if (bottom < top || right < left)
    return;
  QStringList lines;
  QJsonArray cells;
  for (int r = top; r <= bottom; ++r) {
    QStringList fields;
    QJsonArray row;
    for (int c = left; c <= right; ++c) {
      const QModelIndex index = model->index(r, c);
      const QString value = wholeSlice || grid->selectionModel()->isSelected(index) ? index.data(Qt::EditRole).toString() : QString();
      fields << value;
      row.append(value);
    }
    lines << fields.join('\t');
    cells.append(row);
  }
  QMimeData *mime = new QMimeData;
  mime->setText(lines.join('\n'));
  mime->setData(arrayCellsMime, QJsonDocument(cells).toJson(QJsonDocument::Compact));
  QApplication::clipboard()->setMimeData(mime);
}

/** Validate the entire rectangle before applying it as one shared undo operation. */
bool ArrayViewer::pasteText(const QString &text) {
  const QModelIndex start = grid->currentIndex();
  if (!start.isValid())
    return false;
  QVector<QStringList> rows;
  const QMimeData *mime = QApplication::clipboard()->mimeData();
  if (mime && mime->hasFormat(arrayCellsMime) && mime->text() == text) {
    const QJsonArray cells = QJsonDocument::fromJson(mime->data(arrayCellsMime)).array();
    for (const QJsonValue &row : cells) {
      QStringList fields;
      for (const QJsonValue &cell : row.toArray())
        fields << cell.toString();
      rows.append(fields);
    }
  } else {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    QStringList lines = normalized.split('\n');
    if (lines.size() > 1 && lines.last().isEmpty())
      lines.removeLast();
    for (const QString &line : lines)
      rows.append(line.split('\t'));
  }
  if (rows.isEmpty())
    return false;
  for (int r = 0; r < rows.size(); ++r) {
    if (rows[r].isEmpty() || rows[r].size() != rows[0].size() ||
        rows[r].size() > model->columnCount() - start.column() || r >= model->rowCount() - start.row()) {
      notice->setText(tr("Paste must be a rectangle that fits within the displayed slice."));
      return false;
    }
    for (const QString &value : rows[r]) {
      if (!model->accepts(value)) {
        notice->setText(tr("Paste contains a value incompatible with the array's data type. No cells were changed."));
        return false;
      }
    }
  }
  undo->beginMacro(tr("Paste array slice"));
  for (int r = 0; r < rows.size(); ++r)
    for (int c = 0; c < rows[r].size(); ++c)
      model->setData(model->index(start.row() + r, start.column() + c), rows[r][c]);
  undo->endMacro();
  notice->clear();
  return true;
}
