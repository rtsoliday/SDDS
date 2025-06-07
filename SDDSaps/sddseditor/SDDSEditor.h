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
  void search();
  void pageChanged(int value);

private:
  void loadPage(int page);
  void populateModels();
  void clearDataset();
  void changeParameterType(int row);
  void changeColumnType(int column);
  void changeArrayType(int column);
  void message(const QString &text);
  void markDirty();

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
  QStandardItemModel *paramModel;
  QStandardItemModel *columnModel;
  QStandardItemModel *arrayModel;
};

#endif // SDDSEDITOR_H
