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
#include <QLineEdit>
#include <QStandardItemModel>
#include <QVector>
#include <QString>
#include <QPoint>

class QGroupBox;
class QSplitter;

struct ArrayStore {
  QVector<QString> values;
  QVector<int> dims;
};

struct PageStore {
  QVector<QString> parameters;
  QVector<QVector<QString>> columns;
  QVector<ArrayStore> arrays;
};

extern "C" {
#include "SDDS.h"
}

class SDDSEditor : public QMainWindow {
  Q_OBJECT

public:
  explicit SDDSEditor(QWidget *parent = nullptr);
  ~SDDSEditor();
  bool loadFile(const QString &path);

private slots:
  void openFile();
  void saveFile();
  void saveFileAs();
  void pageChanged(int value);
  void copy();
  void paste();
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
  void columnHeaderMenuRequested(const QPoint &pos);
  void columnCellMenuRequested(const QPoint &pos);
  void arrayHeaderMenuRequested(const QPoint &pos);
  void arrayCellMenuRequested(const QPoint &pos);
  void plotColumn(int column);

private:
  void loadPage(int page);
  void populateModels();
  void commitModels();
  void clearDataset();
  bool writeFile(const QString &path);
  void changeParameterType(int row);
  void changeColumnType(int column);
  void changeArrayType(int column);
  void showColumnMenu(QTableView *view, int column, const QPoint &globalPos);
  void showArrayMenu(QTableView *view, int column, const QPoint &globalPos);
  void sortColumn(int column, Qt::SortOrder order);
  void searchColumn(int column);
  void searchArray(int column);
  void message(const QString &text);
  void markDirty();
  QTableView *focusedTable() const;

  SDDS_DATASET dataset;
  bool datasetLoaded;
  bool dirty;
  bool asciiSave;

  QPlainTextEdit *consoleEdit;
  QComboBox *pageCombo;
  QRadioButton *asciiBtn;
  QRadioButton *binaryBtn;
  QLineEdit *searchEdit;

  QTableView *paramView;
  QTableView *columnView;
  QTableView *arrayView;
  QGroupBox *paramBox;
  QGroupBox *colBox;
  QGroupBox *arrayBox;
  QSplitter *dataSplitter;
  QStandardItemModel *paramModel;
  QStandardItemModel *columnModel;
  QStandardItemModel *arrayModel;

  QVector<PageStore> pages;
  int currentPage;
  QString currentFilename;
  int lastRowAddCount;
  QString lastSearchPattern;
};

#endif // SDDSEDITOR_H
