/**
 * @file ArrayViewer.h
 * @brief Editable two-dimensional slices of an SDDS array.
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license See LICENSE in the repository root.
 */
#ifndef SDDS_ARRAY_VIEWER_H
#define SDDS_ARRAY_VIEWER_H

#include <QAbstractTableModel>
#include <QDialog>
#include <QPointer>
#include <QVector>
#include <functional>

class QComboBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QTimer;
class QLabel;
class QTableView;
class QVBoxLayout;
class QUndoStack;

/** Metadata for the selected array on the editor's current page. */
struct ArrayViewerState {
  QString name;
  QString units;
  QVector<int> dimensions;
  int column = -1;
  int page = 0;
  int elements = 0;
  bool numeric = false;
};

/** Maps grid coordinates to the original flat array model, without copying data. */
class ArraySliceModel : public QAbstractTableModel {
public:
  using Edit = std::function<bool(const QModelIndex &, const QString &)>;
  using Validate = std::function<bool(const QString &)>;
  ArraySliceModel(QAbstractItemModel *source, Edit edit, Validate validate, QObject *parent);
  void configure(const ArrayViewerState &state, int rowAxis, int columnAxis, const QVector<int> &fixed);
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
  QModelIndex sourceIndex(const QModelIndex &index) const;
  QVector<int> coordinates(const QModelIndex &index) const;
  bool accepts(const QString &value) const;
  void refreshValues();
  bool finiteRange(long double *minimum, long double *maximum) const;
  void setHeatmap(bool enabled, bool validRange, long double minimum, long double maximum);

private:
  QPointer<QAbstractItemModel> source;
  Edit edit;
  Validate validate;
  ArrayViewerState state;
  QVector<int> fixed;
  int rowAxis = 0;
  int columnAxis = -1;
  bool validShape = false;
  bool heatmap = false;
  bool heatmapRangeValid = false;
  long double heatmapMinimum = 0;
  long double heatmapMaximum = 1;
};

/** Nonmodal array window that follows source-model resets and shares its undo stack. */
class ArrayViewer : public QDialog {
public:
  using State = std::function<ArrayViewerState()>;
  ArrayViewer(QAbstractItemModel *source, QUndoStack *undo, State state,
              ArraySliceModel::Edit edit, ArraySliceModel::Validate validate, QWidget *parent);
  void refresh();
  void copySelection(bool wholeSlice = false);
  bool pasteText(const QString &text);
  QTableView *table() const { return grid; }
  ArraySliceModel *sliceModel() const { return model; }

private:
  void closeEvent(QCloseEvent *event) override;
  void rebuildSliceControls();
  void applySlice();
  void updateSelection();
  void changeAxis(bool rows, int axis);
  void finishEditing();
  void updateHeatmap();
  void applyHeatmapRange();
  State getState;
  ArrayViewerState state;
  ArraySliceModel *model;
  QUndoStack *undo;
  QTableView *grid;
  QComboBox *rowChoice;
  QComboBox *columnChoice;
  QLabel *summary;
  QLabel *selection;
  QLabel *notice;
  QVBoxLayout *sliceControls;
  QCheckBox *heatmapChoice;
  QComboBox *heatmapScale;
  QLineEdit *rangeMinimum;
  QLineEdit *rangeMaximum;
  QPushButton *rangeApply;
  QWidget *heatmapLegend;
  QLabel *legendMinimum;
  QLabel *legendMaximum;
  QLabel *heatmapStatus;
  QTimer *heatmapRefresh;
  bool fixedRangeValid = false;
  long double fixedMinimum = 0;
  long double fixedMaximum = 1;
  QVector<int> fixed;
  int rowAxis = -1;
  int columnAxis = -1;
};

#endif
