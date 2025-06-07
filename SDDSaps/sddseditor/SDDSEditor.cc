/**
 * @file SDDSEditor.cc
 * @brief Implementation of the Qt SDDS editor.
 */

#include "SDDSEditor.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QDockWidget>
#include <QInputDialog>
#include <QHeaderView>

SDDSEditor::SDDSEditor(QWidget *parent)
  : QMainWindow(parent), datasetLoaded(false), dirty(false), asciiSave(true) {
  // console dock
  consoleEdit = new QPlainTextEdit(this);
  consoleEdit->setReadOnly(true);
  QDockWidget *dock = new QDockWidget(tr("Console"), this);
  dock->setWidget(consoleEdit);
  addDockWidget(Qt::TopDockWidgetArea, dock);

  QWidget *central = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(central);

  // page selector bar
  QHBoxLayout *pageLayout = new QHBoxLayout();
  pageLayout->addWidget(new QLabel(tr("Page"), this));
  pageCombo = new QComboBox(this);
  connect(pageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SDDSEditor::pageChanged);
  pageLayout->addWidget(pageCombo);

  searchEdit = new QLineEdit(this);
  QPushButton *searchBtn = new QPushButton(tr("Search"), this);
  connect(searchBtn, &QPushButton::clicked, this, &SDDSEditor::search);
  pageLayout->addWidget(searchEdit);
  pageLayout->addWidget(searchBtn);

  pageLayout->addStretch(1);
  asciiBtn = new QRadioButton(tr("ascii"), this);
  binaryBtn = new QRadioButton(tr("binary"), this);
  asciiBtn->setChecked(true);
  pageLayout->addWidget(asciiBtn);
  pageLayout->addWidget(binaryBtn);
  mainLayout->addLayout(pageLayout);

  // parameters panel
  QGroupBox *paramBox = new QGroupBox(tr("Parameters"), this);
  QVBoxLayout *paramLayout = new QVBoxLayout(paramBox);
  paramModel = new QStandardItemModel(this);
  paramModel->setColumnCount(1);
  paramModel->setHorizontalHeaderLabels(QStringList() << tr("Value"));
  paramView = new QTableView(paramBox);
  paramView->setModel(paramModel);
  paramView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  connect(paramModel, &QStandardItemModel::itemChanged, this, &SDDSEditor::markDirty);
  connect(paramView->verticalHeader(), &QHeaderView::sectionDoubleClicked, this,
          &SDDSEditor::changeParameterType);
  paramLayout->addWidget(paramView);
  mainLayout->addWidget(paramBox);

  // columns panel
  QGroupBox *colBox = new QGroupBox(tr("Columns"), this);
  QVBoxLayout *colLayout = new QVBoxLayout(colBox);
  columnModel = new QStandardItemModel(this);
  columnView = new QTableView(colBox);
  columnView->setModel(columnModel);
  columnView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  connect(columnView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeColumnType);
  colLayout->addWidget(columnView);
  mainLayout->addWidget(colBox);

  // arrays panel
  QGroupBox *arrayBox = new QGroupBox(tr("Arrays"), this);
  QVBoxLayout *arrayLayout = new QVBoxLayout(arrayBox);
  arrayModel = new QStandardItemModel(this);
  arrayView = new QTableView(arrayBox);
  arrayView->setModel(arrayModel);
  arrayView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  connect(arrayView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeArrayType);
  arrayLayout->addWidget(arrayView);
  mainLayout->addWidget(arrayBox);

  setCentralWidget(central);

  // menu bar
  QMenu *fileMenu = menuBar()->addMenu(tr("File"));
  QAction *openAct = fileMenu->addAction(tr("Open"));
  QAction *saveAct = fileMenu->addAction(tr("Save"));
  QAction *quitAct = fileMenu->addAction(tr("Quit"));
  connect(openAct, &QAction::triggered, this, &SDDSEditor::openFile);
  connect(saveAct, &QAction::triggered, this, &SDDSEditor::saveFile);
  connect(quitAct, &QAction::triggered, this, &QWidget::close);

  QMenu *infoMenu = menuBar()->addMenu(tr("Info"));
  QAction *aboutAct = infoMenu->addAction(tr("About"));
  connect(aboutAct, &QAction::triggered, []() {
    QMessageBox::about(nullptr, QObject::tr("About"),
                       QObject::tr("SDDS Qt Editor"));
  });
}

SDDSEditor::~SDDSEditor() {
  clearDataset();
}

void SDDSEditor::message(const QString &text) {
  consoleEdit->appendPlainText(text);
}

void SDDSEditor::markDirty() {
  dirty = true;
}

void SDDSEditor::openFile() {
  QString path = QFileDialog::getOpenFileName(this, tr("Open SDDS"), QString(),
                                             tr("SDDS Files (*.sdds);;All Files (*)"));
  if (path.isEmpty())
    return;
  loadFile(path);
}

bool SDDSEditor::loadFile(const QString &path) {
  clearDataset();
  memset(&dataset, 0, sizeof(dataset));
  if (!SDDS_InitializeInput(&dataset,
                            const_cast<char *>(path.toLocal8Bit().constData()))) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to open file"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }

  datasetLoaded = true;
  message(tr("Loaded %1").arg(path));

  int32_t pages = 0;
  while (SDDS_ReadPage(&dataset) > 0)
    pages++;

  if (!pages) {
    QMessageBox::warning(this, tr("SDDS"), tr("File contains no pages"));
    return false;
  }

  if (!SDDS_GotoPage(&dataset, 1) || SDDS_ReadPage(&dataset) <= 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Unable to read first page"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }

  pageCombo->blockSignals(true);
  pageCombo->clear();
  for (int32_t i = 0; i < pages; ++i)
    pageCombo->addItem(tr("Page %1").arg(i + 1));
  pageCombo->setCurrentIndex(0);
  pageCombo->blockSignals(false);

  loadPage(1);
  return true;
}

void SDDSEditor::saveFile() {
  if (!datasetLoaded)
    return;
  QString path = QFileDialog::getSaveFileName(this, tr("Save SDDS"), QString(),
                                             tr("SDDS Files (*.sdds);;All Files (*)"));
  if (path.isEmpty())
    return;
  dataset.layout.data_mode.mode = asciiBtn->isChecked() ? SDDS_ASCII : SDDS_BINARY;
  if (!SDDS_WriteLayout(&dataset) || !SDDS_WritePage(&dataset)) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to write file"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  QFile::copy(dataset.layout.filename, path);
  dirty = false;
  message(tr("Saved %1").arg(path));
}

void SDDSEditor::search() {
  QString text = searchEdit->text();
  if (text.isEmpty())
    return;
  for (int r = 0; r < paramModel->rowCount(); ++r) {
    if (paramModel->verticalHeaderItem(r)->text().contains(text, Qt::CaseInsensitive)) {
      paramView->selectRow(r);
      paramView->scrollTo(paramModel->index(r, 0));
      return;
    }
  }
  for (int c = 0; c < columnModel->columnCount(); ++c) {
    if (columnModel->headerData(c, Qt::Horizontal).toString().contains(text, Qt::CaseInsensitive)) {
      columnView->selectColumn(c);
      columnView->scrollTo(columnModel->index(0, c));
      return;
    }
  }
  for (int c = 0; c < arrayModel->columnCount(); ++c) {
    if (arrayModel->headerData(c, Qt::Horizontal).toString().contains(text, Qt::CaseInsensitive)) {
      arrayView->selectColumn(c);
      arrayView->scrollTo(arrayModel->index(0, c));
      return;
    }
  }
}

void SDDSEditor::pageChanged(int value) {
  if (!datasetLoaded)
    return;
  if (!SDDS_GotoPage(&dataset, value + 1) || SDDS_ReadPage(&dataset) <= 0) {
    QMessageBox::warning(this, tr("SDDS"), tr("Unable to read page"));
    return;
  }
  loadPage(value + 1);
}

void SDDSEditor::loadPage(int page) {
  Q_UNUSED(page);
  populateModels();
}

void SDDSEditor::populateModels() {
  if (!datasetLoaded)
    return;

  // parameters
  paramModel->removeRows(0, paramModel->rowCount());
  int32_t count;
  char **names = SDDS_GetParameterNames(&dataset, &count);
  for (int32_t i = 0; i < count; ++i) {
    char *value = SDDS_GetParameterAsString(&dataset, names[i], NULL);
    PARAMETER_DEFINITION *def = SDDS_GetParameterDefinition(&dataset, names[i]);
    paramModel->setRowCount(i + 1);
    paramModel->setVerticalHeaderItem(i, new QStandardItem(QString("%1 (%2)").arg(names[i]).arg(SDDS_GetTypeName(def->type))));
    QStandardItem *item = new QStandardItem(QString(value ? value : ""));
    item->setEditable(false);
    item->setData(def->type, Qt::UserRole);
    paramModel->setItem(i, 0, item);
    free(value);
  }
  SDDS_FreeStringArray(names, count);

  // columns
  names = SDDS_GetColumnNames(&dataset, &count);
  int64_t rows = SDDS_RowCount(&dataset);
  columnModel->clear();
  columnModel->setColumnCount(count);
  columnModel->setRowCount(rows);
  for (int32_t i = 0; i < count; ++i) {
    COLUMN_DEFINITION *def = SDDS_GetColumnDefinition(&dataset, names[i]);
    columnModel->setHeaderData(i, Qt::Horizontal,
                              QString("%1 (%2)").arg(names[i]).arg(SDDS_GetTypeName(def->type)));
    char **data = SDDS_GetColumnInString(&dataset, names[i]);
    for (int64_t r = 0; r < rows; ++r)
      columnModel->setItem(r, i, new QStandardItem(QString(data ? data[r] : "")));
    SDDS_FreeStringArray(data, rows);
  }
  for (int64_t r = 0; r < rows; ++r)
    columnModel->setVerticalHeaderItem(r, new QStandardItem(QString::number(r + 1)));
  SDDS_FreeStringArray(names, count);

  // arrays
  names = SDDS_GetArrayNames(&dataset, &count);
  arrayModel->clear();
  int maxLen = 0;
  QVector<char **> arrayData(count);
  QVector<int32_t> sizes(count);
  for (int32_t i = 0; i < count; ++i) {
    ARRAY_DEFINITION *def = SDDS_GetArrayDefinition(&dataset, names[i]);
    int32_t dim;
    char **vals = SDDS_GetArrayInString(&dataset, names[i], &dim);
    arrayModel->setColumnCount(i + 1);
    arrayModel->setHeaderData(i, Qt::Horizontal,
                              QString("%1 (%2)").arg(names[i]).arg(SDDS_GetTypeName(def->type)));
    arrayData[i] = vals;
    sizes[i] = dim;
    if (dim > maxLen)
      maxLen = dim;
  }
  arrayModel->setRowCount(maxLen);
  for (int32_t c = 0; c < count; ++c) {
    for (int32_t r = 0; r < sizes[c]; ++r) {
      arrayModel->setItem(r, c, new QStandardItem(QString(arrayData[c][r])));
    }
    SDDS_FreeStringArray(arrayData[c], sizes[c]);
  }
  for (int r = 0; r < maxLen; ++r)
    arrayModel->setVerticalHeaderItem(r, new QStandardItem(QString::number(r + 1)));
  SDDS_FreeStringArray(names, count);
}

void SDDSEditor::clearDataset() {
  if (datasetLoaded) {
    SDDS_Terminate(&dataset);
    datasetLoaded = false;
    paramModel->clear();
    columnModel->clear();
    arrayModel->clear();
    pageCombo->clear();
  }
}

void SDDSEditor::changeParameterType(int row) {
  if (!datasetLoaded)
    return;
  QStringList types;
  types << "long" << "double" << "string";
  QStandardItem *headerItem = paramModel->verticalHeaderItem(row);
  QString label = headerItem->text();
  QString current = label.section(' ', -1).remove('(').remove(')');
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Parameter Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  QString name = label.section(' ', 0, 0);
  SDDS_ChangeParameterInformation(&dataset, (char *)"type", &sddsType,
                                  SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                  const_cast<char *>(name.toLocal8Bit().constData()));
  headerItem->setText(QString("%1 (%2)").arg(name).arg(newType));
  markDirty();
}

void SDDSEditor::changeColumnType(int column) {
  if (!datasetLoaded)
    return;
  QStringList types;
  types << "long" << "double" << "string";
  QString label = columnModel->headerData(column, Qt::Horizontal).toString();
  QString name = label.section(' ', 0, 0);
  QString current = label.section(' ', -1).remove('(').remove(')');
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Column Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  SDDS_ChangeColumnInformation(&dataset, (char *)"type", &sddsType,
                               SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                               const_cast<char *>(name.toLocal8Bit().constData()));
  columnModel->setHeaderData(column, Qt::Horizontal,
                             QString("%1 (%2)").arg(name).arg(newType));
  markDirty();
}

void SDDSEditor::changeArrayType(int column) {
  if (!datasetLoaded)
    return;
  QStringList types;
  types << "long" << "double" << "string";
  QString label = arrayModel->headerData(column, Qt::Horizontal).toString();
  QString name = label.section(' ', 0, 0);
  QString current = label.section(' ', -1).remove('(').remove(')');
  bool ok = false;
  QString newType = QInputDialog::getItem(this, tr("Array Type"), tr("Type"), types,
                                         types.indexOf(current), false, &ok);
  if (!ok || newType == current)
    return;
  int32_t sddsType = SDDS_IdentifyType(const_cast<char *>(newType.toLocal8Bit().constData()));
  SDDS_ChangeArrayInformation(&dataset, (char *)"type", &sddsType,
                              SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              const_cast<char *>(name.toLocal8Bit().constData()));
  arrayModel->setHeaderData(column, Qt::Horizontal,
                            QString("%1 (%2)").arg(name).arg(newType));
  markDirty();
}

#include "SDDSEditor_moc.h"

