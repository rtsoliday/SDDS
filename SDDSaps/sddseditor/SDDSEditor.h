/**
 * @file SDDSEditor.h
 * @brief Qt GUI for editing SDDS files.
 *
 * This class provides a simple Qt based editor for parameters,
 * columns and arrays of an SDDS file. It supports multiple pages
 * and allows the user to modify attributes such as the data type.
 */

#ifndef SDDSEDITOR_H
#define SDDSEDITOR_H

#include <QMainWindow>
#include <QTableView>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QStandardItemModel>
#include <QVector>
#include <QString>
#include <QPoint>
#include <QUndoStack>
#include <QPointer>
#include <QCloseEvent>

class QGroupBox;
class QSplitter;
class QDialog;
class QProgressDialog;
class ColumnPageModel;
class ArrayPageModel;

struct ArrayStore {
  QVector<QString> values;
  QVector<int> dims;
};

struct PageStore {
  QVector<QString> parameters;
  QVector<QVector<QString>> columns;
  QVector<ArrayStore> arrays;
};

#ifdef _WIN32
#include <inttypes.h>
#endif

extern "C" {
#include "SDDS.h"
}

class SDDSEditor : public QMainWindow {
  Q_OBJECT

public:
  explicit SDDSEditor(bool darkPalette = false, QWidget *parent = nullptr);
  ~SDDSEditor();
  bool loadFile(const QString &path);

private slots:
  void openFile();
  void saveFile();
  void saveFileAs();
  void saveFileAsHDF();
  void exportCSV();
  void pageChanged(int value);
  void copy();
  void paste();
  void deleteCells();
  void editParameterAttributes();
  void editColumnAttributes();
  void editArrayAttributes();
  void insertParameter();
  void insertColumn();
  void insertArray();
  void deleteParameter();
  void deleteColumn();
  void deleteArray();
  void insertColumnRows();
  void deleteColumnRows();
  void clonePage();
  void insertPage();
  void deletePage();
  void parameterHeaderMenuRequested(const QPoint &pos);
  void parameterCellMenuRequested(const QPoint &pos);
  void columnHeaderMenuRequested(const QPoint &pos);
  void columnCellMenuRequested(const QPoint &pos);
  void columnRowMenuRequested(const QPoint &pos);
  void arrayHeaderMenuRequested(const QPoint &pos);
  void arrayCellMenuRequested(const QPoint &pos);
  void plotColumn(int column);
  void restartApp();
  void showHelp();
  void parameterMoved(int logical, int oldVisual, int newVisual);
  void columnMoved(int logical, int oldVisual, int newVisual);
  void arrayMoved(int logical, int oldVisual, int newVisual);

protected:
  void changeEvent(QEvent *event) override;

private:
  void applyTheme(bool dark);
  void flushPendingEdits();
  void loadPage(int page);
  void populateModels();
  void commitModels();
  void clearDataset();
  /** Ensures a dataset is initialized, creating an empty one if needed. */
  bool ensureDataset();
  bool writeFile(const QString &path);
  bool writeHDF(const QString &path);
  bool writeCSV(const QString &path);
  void changeParameterType(int row);
  void changeColumnType(int column);
  void changeArrayType(int column);
  void showParameterMenu(QTableView *view, int row, const QPoint &globalPos);
  void showColumnMenu(QTableView *view, int column, const QPoint &globalPos);
  void showArrayMenu(QTableView *view, int column, const QPoint &globalPos);
  void resizeArray(int column);
  void sortColumn(int column, Qt::SortOrder order);
  void searchColumn(int column);
  void searchArray(int column);
  void message(const QString &text);
  void markDirty();
  bool maybeSave();
  void updateWindowTitle();
  void closeEvent(QCloseEvent *event) override;
  QTableView *focusedTable() const;

  SDDS_DATASET dataset;
  bool datasetLoaded;
  bool dirty;
  bool asciiSave;

  QPlainTextEdit *consoleEdit;
  QComboBox *pageCombo;
  QRadioButton *asciiBtn;
  QRadioButton *binaryBtn;

  QTableView *paramView;
  QTableView *columnView;
  QTableView *arrayView;
  QGroupBox *paramBox;
  QGroupBox *colBox;
  QGroupBox *arrayBox;
  QSplitter *dataSplitter;
  QStandardItemModel *paramModel;
  ColumnPageModel *columnModel;
  ArrayPageModel *arrayModel;

  QVector<PageStore> pages;
  int currentPage;
  QString currentFilename;
  int lastRowAddCount;
  QString lastSearchPattern;
  QString lastReplaceText;
  QUndoStack *undoStack;
  bool updatingModels;
  bool darkPalette;
  QPointer<QDialog> searchColumnDialog;

  /* Used only during initial load to provide progress through UI model building. */
  QPointer<QProgressDialog> loadProgressDialog;
  int loadProgressMin;
  int loadProgressMax;
};

#endif // SDDSEDITOR_H
