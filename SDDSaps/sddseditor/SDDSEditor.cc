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
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QDockWidget>
#include <QInputDialog>
#include <QHeaderView>
#include <QApplication>
#include <QShortcut>
#include <QClipboard>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QButtonGroup>
#include <QSpinBox>
#include <cstdlib>

SDDSEditor::SDDSEditor(QWidget *parent)
  : QMainWindow(parent), datasetLoaded(false), dirty(false), asciiSave(true), currentPage(0), currentFilename(QString()) {
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

  // container for data panels
  dataSplitter = new QSplitter(Qt::Vertical, this);
  //dataSplitter->setChildrenCollapsible(false);
  dataSplitter->setHandleWidth(4);
  dataSplitter->setStyleSheet("QSplitter::handle { background-color: #CCCCCC; }");
  mainLayout->addWidget(dataSplitter, 1);

  // parameters panel
  paramBox = new QGroupBox(tr("Parameters"), this);
  paramBox->setCheckable(true);
  paramBox->setChecked(true);
  QVBoxLayout *paramLayout = new QVBoxLayout(paramBox);
  paramModel = new QStandardItemModel(this);
  paramModel->setColumnCount(1);
  paramModel->setHorizontalHeaderLabels(QStringList() << tr("Value"));
  paramView = new QTableView(paramBox);
  paramView->setModel(paramModel);
  // Let the single value column expand to take up the available space.
  // This keeps the parameter table readable even when the window is wide.
  paramView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  paramView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  paramView->verticalHeader()->setDefaultSectionSize(18);
  paramView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(paramModel, &QStandardItemModel::itemChanged, this,
          &SDDSEditor::markDirty);
  connect(paramView->verticalHeader(), &QHeaderView::sectionDoubleClicked, this,
          &SDDSEditor::changeParameterType);
  connect(paramBox, &QGroupBox::toggled, paramView, &QWidget::setVisible);
  paramLayout->addWidget(paramView);
  dataSplitter->addWidget(paramBox);

  // columns panel
  colBox = new QGroupBox(tr("Columns"), this);
  colBox->setCheckable(true);
  colBox->setChecked(true);
  QVBoxLayout *colLayout = new QVBoxLayout(colBox);
  columnModel = new QStandardItemModel(this);
  columnView = new QTableView(colBox);
  columnView->setModel(columnModel);
  columnView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  columnView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  columnView->verticalHeader()->setDefaultSectionSize(18);
  columnView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(columnView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeColumnType);
  connect(colBox, &QGroupBox::toggled, columnView, &QWidget::setVisible);
  colLayout->addWidget(columnView);
  dataSplitter->addWidget(colBox);

  // arrays panel
  arrayBox = new QGroupBox(tr("Arrays"), this);
  arrayBox->setCheckable(true);
  arrayBox->setChecked(true);
  QVBoxLayout *arrayLayout = new QVBoxLayout(arrayBox);
  arrayModel = new QStandardItemModel(this);
  arrayView = new QTableView(arrayBox);
  arrayView->setModel(arrayModel);
  arrayView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  arrayView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  arrayView->verticalHeader()->setDefaultSectionSize(18);
  arrayView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(arrayView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeArrayType);
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

  setCentralWidget(central);
  resize(1200, 800);

  // menu bar
  QMenu *fileMenu = menuBar()->addMenu(tr("File"));
  QAction *openAct = fileMenu->addAction(tr("Open"));
  QAction *saveAct = fileMenu->addAction(tr("Save"));
  QAction *saveAsAct = fileMenu->addAction(tr("Save as..."));
  QAction *quitAct = fileMenu->addAction(tr("Quit"));
  connect(openAct, &QAction::triggered, this, &SDDSEditor::openFile);
  connect(saveAct, &QAction::triggered, this, &SDDSEditor::saveFile);
  connect(saveAsAct, &QAction::triggered, this, &SDDSEditor::saveFileAs);
  connect(quitAct, &QAction::triggered, this, &QWidget::close);

  QMenu *editMenu = menuBar()->addMenu(tr("Edit"));
  QMenu *paramMenu = editMenu->addMenu(tr("Parameter"));
  QAction *paramAttr = paramMenu->addAction(tr("Attributes"));
  paramMenu->addAction(tr("Insert"));
  paramMenu->addAction(tr("Delete"));
  connect(paramAttr, &QAction::triggered, this,
          &SDDSEditor::editParameterAttributes);
  QMenu *colMenu = editMenu->addMenu(tr("Column"));
  QAction *colAttr = colMenu->addAction(tr("Attributes"));
  colMenu->addAction(tr("Insert"));
  colMenu->addAction(tr("Delete"));
  connect(colAttr, &QAction::triggered, this,
          &SDDSEditor::editColumnAttributes);
  QMenu *arrayMenu = editMenu->addMenu(tr("Array"));
  QAction *arrayAttr = arrayMenu->addAction(tr("Attributes"));
  arrayMenu->addAction(tr("Insert"));
  arrayMenu->addAction(tr("Delete"));
  connect(arrayAttr, &QAction::triggered, this,
          &SDDSEditor::editArrayAttributes);

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
  for (int r = 0; r < rows.size(); ++r) {
    QStringList cols = rows[r].split('\t');
    for (int c = 0; c < cols.size(); ++c) {
      QModelIndex idx = view->model()->index(start.row() + r, start.column() + c);
      if (idx.isValid())
        view->model()->setData(idx, cols[c]);
    }
  }
  markDirty();
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
  SDDS_SetDefaultIOBufferSize(0);
  memset(&dataset, 0, sizeof(dataset));
  if (!SDDS_InitializeInput(&dataset,
                            const_cast<char *>(path.toLocal8Bit().constData()))) {
    QMessageBox::warning(this, tr("SDDS"), tr("Failed to open file"));
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return false;
  }

  datasetLoaded = true;
  currentFilename = path;
  dirty = false;
  message(tr("Loaded %1").arg(path));

  pages.clear();
  while (SDDS_ReadPage(&dataset) > 0) {
    PageStore pd;
    int32_t pcount;
    char **pnames = SDDS_GetParameterNames(&dataset, &pcount);
    for (int32_t i = 0; i < pcount; ++i) {
      char *val = SDDS_GetParameterAsString(&dataset, pnames[i], NULL);
      pd.parameters.append(QString(val ? val : ""));
      free(val);
    }
    SDDS_FreeStringArray(pnames, pcount);
    int32_t ccount;
    char **cnames = SDDS_GetColumnNames(&dataset, &ccount);
    int64_t rows = SDDS_RowCount(&dataset);
    pd.columns.resize(ccount);
    for (int32_t c = 0; c < ccount; ++c) {
      char **data = SDDS_GetColumnInString(&dataset, cnames[c]);
      pd.columns[c].resize(rows);
      for (int64_t r = 0; r < rows; ++r)
        pd.columns[c][r] = QString(data ? data[r] : "");
      SDDS_FreeStringArray(data, rows);
    }
    SDDS_FreeStringArray(cnames, ccount);
    int32_t acount;
    char **anames = SDDS_GetArrayNames(&dataset, &acount);
    pd.arrays.resize(acount);
    for (int32_t a = 0; a < acount; ++a) {
      ARRAY_DEFINITION *adef = SDDS_GetArrayDefinition(&dataset, anames[a]);
      int32_t dim;
      char **vals = SDDS_GetArrayInString(&dataset, anames[a], &dim);
      pd.arrays[a].dims.resize(adef->dimensions);
      for (int d = 0; d < adef->dimensions; ++d)
        pd.arrays[a].dims[d] = dataset.array[a].dimension[d];
      pd.arrays[a].values.resize(dim);
      for (int i = 0; i < dim; ++i)
        pd.arrays[a].values[i] = QString(vals[i]);
      SDDS_FreeStringArray(vals, dim);
    }
    SDDS_FreeStringArray(anames, acount);
    pages.append(pd);
  }
  if (pages.isEmpty()) {
    QMessageBox::warning(this, tr("SDDS"), tr("File contains no pages"));
    return false;
  }
  pageCombo->blockSignals(true);
  pageCombo->clear();
  for (int i = 0; i < pages.size(); ++i)
    pageCombo->addItem(tr("Page %1").arg(i + 1));
  pageCombo->setCurrentIndex(0);
  pageCombo->blockSignals(false);
  currentPage = 0;
  loadPage(1);
  return true;
}

bool SDDSEditor::writeFile(const QString &path) {
  if (!datasetLoaded)
    return false;
  commitModels();

  SDDS_DATASET out;
  memset(&out, 0, sizeof(out));
  if (!SDDS_InitializeCopy(&out, &dataset,
                           const_cast<char *>(path.toLocal8Bit().constData()), (char *)"w")) {
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

  for (const PageStore &pd : qAsConst(pages)) {
    int64_t rows = 0;
    if (ccount > 0 && pd.columns.size() > 0)
      rows = pd.columns[0].size();
    if (!SDDS_StartPage(&out, rows)) {
      QMessageBox::warning(this, tr("SDDS"), tr("Failed to start page"));
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Terminate(&out);
      return false;
    }

    for (int i = 0; i < pcount && i < pd.parameters.size(); ++i) {
      const char *name = dataset.layout.parameter_definition[i].name;
      int32_t type = dataset.layout.parameter_definition[i].type;
      QString text = pd.parameters[i];
      switch (type) {
      case SDDS_SHORT:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (short)text.toShort(), NULL);
        break;
      case SDDS_USHORT:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, (unsigned short)text.toUShort(), NULL);
        break;
      case SDDS_LONG:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, text.toLong(), NULL);
        break;
      case SDDS_ULONG:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, text.toULong(), NULL);
        break;
      case SDDS_LONG64:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, text.toLongLong(), NULL);
        break;
      case SDDS_ULONG64:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, text.toULongLong(), NULL);
        break;
      case SDDS_FLOAT:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, text.toFloat(), NULL);
        break;
      case SDDS_DOUBLE:
        SDDS_SetParameters(&out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, name, text.toDouble(), NULL);
        break;
      case SDDS_LONGDOUBLE: {
        QByteArray ba = text.toLocal8Bit();
        long double v = 0.0L;
        if (!ba.isEmpty())
          v = strtold(ba.constData(), nullptr);
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
      if (type == SDDS_STRING) {
        QVector<char *> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          QString text = r < pd.columns[c].size() ? pd.columns[c][r] : QString();
          arr[r] = strdup(text.toLocal8Bit().constData());
        }
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
        for (auto p : arr)
          free(p);
      } else if (type == SDDS_CHARACTER) {
        QVector<char> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          QString text = r < pd.columns[c].size() ? pd.columns[c][r] : QString();
          QByteArray ba = text.toLatin1();
          arr[r] = ba.isEmpty() ? '\0' : ba.at(0);
        }
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
      } else if (type == SDDS_LONGDOUBLE) {
        QVector<long double> arr(rows);
        for (int64_t r = 0; r < rows; ++r) {
          QString text = r < pd.columns[c].size() ? pd.columns[c][r] : QString();
          QByteArray ba = text.toLocal8Bit();
          arr[r] = ba.isEmpty() ? 0.0L : strtold(ba.constData(), nullptr);
        }
        SDDS_SetColumn(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
      } else {
        QVector<double> arr(rows);
        for (int64_t r = 0; r < rows; ++r)
          arr[r] = r < pd.columns[c].size() ? pd.columns[c][r].toDouble() : 0.0;
        SDDS_SetColumnFromDoubles(&out, SDDS_SET_BY_NAME, arr.data(), rows, name);
      }
    }

    for (int a = 0; a < acount && a < pd.arrays.size(); ++a) {
      const char *name = dataset.layout.array_definition[a].name;
      int32_t type = dataset.layout.array_definition[a].type;
      const ArrayStore &as = pd.arrays[a];
      int elements = as.values.size();
      QVector<int32_t> dims = QVector<int32_t>::fromList(as.dims.toList());
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
        size_t size = SDDS_type_size[type - 1];
        void *buffer = malloc(size * elements);
        if (!buffer)
          continue;
        for (int i = 0; i < elements; ++i) {
          QString cell = as.values[i];
          switch (type) {
          case SDDS_LONGDOUBLE:
            ((long double *)buffer)[i] = strtold(cell.toLocal8Bit().constData(), nullptr);
            break;
          case SDDS_DOUBLE:
            ((double *)buffer)[i] = cell.toDouble();
            break;
          case SDDS_FLOAT:
            ((float *)buffer)[i] = cell.toFloat();
            break;
          case SDDS_LONG64:
            ((int64_t *)buffer)[i] = cell.toLongLong();
            break;
          case SDDS_ULONG64:
            ((uint64_t *)buffer)[i] = cell.toULongLong();
            break;
          case SDDS_LONG:
            ((int32_t *)buffer)[i] = cell.toInt();
            break;
          case SDDS_ULONG:
            ((uint32_t *)buffer)[i] = cell.toUInt();
            break;
          case SDDS_SHORT:
            ((short *)buffer)[i] = (short)cell.toInt();
            break;
          case SDDS_USHORT:
            ((unsigned short *)buffer)[i] = (unsigned short)cell.toUInt();
            break;
          default:
            ((double *)buffer)[i] = cell.toDouble();
            break;
          }
        }
        SDDS_SetArray(&out, const_cast<char *>(name), SDDS_CONTIGUOUS_DATA, buffer, dims.data());
        free(buffer);
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
  dirty = false;
  message(tr("Saved %1").arg(path));
  return true;
}

void SDDSEditor::saveFile() {
  if (currentFilename.isEmpty())
    return;
  writeFile(currentFilename);
}

void SDDSEditor::saveFileAs() {
  QString path = QFileDialog::getSaveFileName(this, tr("Save SDDS"), currentFilename,
                                             tr("SDDS Files (*.sdds);;All Files (*)"));
  if (path.isEmpty())
    return;
  if (writeFile(path))
    currentFilename = path;
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
  commitModels();
  if (value < 0 || value >= pages.size())
    return;
  loadPage(value + 1);
}

void SDDSEditor::loadPage(int page) {
  currentPage = page - 1;
  populateModels();
}
void SDDSEditor::populateModels() {
  if (!datasetLoaded || pages.isEmpty() || currentPage < 0 || currentPage >= pages.size())
    return;

  const PageStore &pd = pages[currentPage];

  // parameters
  paramModel->removeRows(0, paramModel->rowCount());
  int32_t pcount = dataset.layout.n_parameters;
  paramBox->setChecked(pcount > 0);
  for (int32_t i = 0; i < pcount; ++i) {
    PARAMETER_DEFINITION *def = &dataset.layout.parameter_definition[i];
    paramModel->setRowCount(i + 1);
    paramModel->setVerticalHeaderItem(
        i, new QStandardItem(QString(def->name)));
    QString value = (i < pd.parameters.size()) ? pd.parameters[i] : QString();
    QStandardItem *item = new QStandardItem(value);
    item->setEditable(true);
    item->setData(def->type, Qt::UserRole);
    paramModel->setItem(i, 0, item);
  }

  // columns
  int32_t ccount = dataset.layout.n_columns;
  colBox->setChecked(ccount > 0);
  int64_t rows = (ccount > 0 && pd.columns.size() > 0) ? pd.columns[0].size() : 0;
  columnModel->clear();
  columnModel->setColumnCount(ccount);
  columnModel->setRowCount(rows);
  for (int32_t i = 0; i < ccount; ++i) {
    COLUMN_DEFINITION *def = &dataset.layout.column_definition[i];
    columnModel->setHeaderData(i, Qt::Horizontal,
                               QString(def->name));
    for (int64_t r = 0; r < rows; ++r) {
      QString val = (i < pd.columns.size() && r < pd.columns[i].size()) ? pd.columns[i][r] : QString();
      columnModel->setItem(r, i, new QStandardItem(val));
    }
  }
  for (int64_t r = 0; r < rows; ++r)
    columnModel->setVerticalHeaderItem(r, new QStandardItem(QString::number(r + 1)));

  // Resize columns to fit their contents first so initial widths are reasonable
  // then allow them to stretch to fill the remaining space and be adjusted by
  // the user.
  columnView->resizeColumnsToContents();
  columnView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  // arrays
  int32_t acount = dataset.layout.n_arrays;
  arrayBox->setChecked(acount > 0);
  arrayModel->clear();
  int maxLen = 0;
  for (int a = 0; a < acount && a < pd.arrays.size(); ++a)
    if (pd.arrays[a].values.size() > maxLen)
      maxLen = pd.arrays[a].values.size();
  arrayModel->setColumnCount(acount);
  arrayModel->setRowCount(maxLen);
  for (int a = 0; a < acount; ++a) {
    ARRAY_DEFINITION *def = &dataset.layout.array_definition[a];
    arrayModel->setHeaderData(a, Qt::Horizontal,
                              QString(def->name));
    const QVector<QString> vals = (a < pd.arrays.size()) ? pd.arrays[a].values : QVector<QString>();
    for (int i = 0; i < vals.size(); ++i)
      arrayModel->setItem(i, a, new QStandardItem(vals[i]));
  }
  for (int r = 0; r < maxLen; ++r)
    arrayModel->setVerticalHeaderItem(r, new QStandardItem(QString::number(r + 1)));

  // Similar treatment for arrays table.
  arrayView->resizeColumnsToContents();
  arrayView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void SDDSEditor::clearDataset() {

  if (datasetLoaded) {
    SDDS_Terminate(&dataset);
    datasetLoaded = false;
    paramModel->clear();
    columnModel->clear();
    arrayModel->clear();
    pageCombo->clear();
    pages.clear();
  }
}
void SDDSEditor::commitModels() {
  if (!datasetLoaded || pages.isEmpty() || currentPage < 0 || currentPage >= pages.size())
    return;

  PageStore &pd = pages[currentPage];

  int32_t pcount = dataset.layout.n_parameters;
  pd.parameters.resize(pcount);
  for (int32_t i = 0; i < pcount && i < paramModel->rowCount(); ++i) {
    pd.parameters[i] = paramModel->item(i, 0)->text();
  }

  int32_t ccount = dataset.layout.n_columns;
  int64_t rows = columnModel->rowCount();
  pd.columns.resize(ccount);
  for (int32_t c = 0; c < ccount && c < columnModel->columnCount(); ++c) {
    pd.columns[c].resize(rows);
    for (int64_t r = 0; r < rows; ++r) {
      QStandardItem *it = columnModel->item(r, c);
      pd.columns[c][r] = it ? it->text() : QString();
    }
  }

  int32_t acount = dataset.layout.n_arrays;
  pd.arrays.resize(acount);
  int rowsA = arrayModel->rowCount();
  for (int32_t a = 0; a < acount && a < arrayModel->columnCount(); ++a) {
    ArrayStore &as = pd.arrays[a];
    int elements = as.values.size();
    for (int i = 0; i < elements && i < rowsA; ++i) {
      QStandardItem *it = arrayModel->item(i, a);
      as.values[i] = it ? it->text() : QString();
    }
  }
}

void SDDSEditor::editParameterAttributes() {
  if (!datasetLoaded)
    return;
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
  if (paramModel->verticalHeaderItem(row))
    paramModel->verticalHeaderItem(row)->setText(name.text());
  markDirty();
}

void SDDSEditor::editColumnAttributes() {
  if (!datasetLoaded)
    return;
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
  columnModel->setHeaderData(col, Qt::Horizontal, name.text());
  markDirty();
}

void SDDSEditor::editArrayAttributes() {
  if (!datasetLoaded)
    return;
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
  int32_t len = length.value();
  SDDS_ChangeArrayInformation(&dataset, (char *)"field_length", &len,
                              SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, col);
  int32_t tval = typeGroup.checkedId();
  SDDS_ChangeArrayInformation(&dataset, (char *)"type", &tval,
                              SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, col);
  arrayModel->setHeaderData(col, Qt::Horizontal, name.text());
  markDirty();
}
void SDDSEditor::changeParameterType(int row) {
  if (!datasetLoaded)
    return;
  QStringList types;
  types << "short" << "ushort" << "long" << "ulong" << "long64"
        << "ulong64" << "float" << "double" << "long double" << "string"
        << "character";
  if (row < 0 || row >= dataset.layout.n_parameters)
    return;
  QStandardItem *headerItem = paramModel->verticalHeaderItem(row);
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
  headerItem->setText(name);
  markDirty();
}

void SDDSEditor::changeColumnType(int column) {
  if (!datasetLoaded)
    return;
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
  columnModel->setHeaderData(column, Qt::Horizontal, QString(name));
  markDirty();
}

void SDDSEditor::changeArrayType(int column) {
  if (!datasetLoaded)
    return;
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
  arrayModel->setHeaderData(column, Qt::Horizontal, QString(name));
  markDirty();
}

#include "SDDSEditor_moc.h"

