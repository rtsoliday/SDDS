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
#include <QFont>
#include <QHeaderView>
#include <QMenu>
#include <QProcess>
#include <QApplication>
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
#include <functional>
#include <cstdlib>
#include <algorithm>

static bool validateTextForType(const QString &text, int type,
                                bool showMessage = true) {
  if (SDDS_NUMERIC_TYPE(type)) {
    QString trimmed = text.trimmed();
    if (!trimmed.isEmpty()) {
      bool ok = true;
      if (SDDS_FLOATING_TYPE(type))
        trimmed.toDouble(&ok);
      else
        trimmed.toLongLong(&ok);
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
  int prod = 1;
  for (int d : dims)
    prod *= d > 0 ? d : 1;
  return prod;
}

class SDDSItemDelegate : public QStyledItemDelegate {
public:
  using TypeFunc = std::function<int(const QModelIndex &)>;
  explicit SDDSItemDelegate(TypeFunc tf, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), typeFunc(std::move(tf)) {}

  void setModelData(QWidget *editorWidget, QAbstractItemModel *model,
                    const QModelIndex &index) const override {
    QLineEdit *line = qobject_cast<QLineEdit *>(editorWidget);
    if (!line) {
      QStyledItemDelegate::setModelData(editorWidget, model, index);
      return;
    }

    if (validateTextForType(line->text(), typeFunc(index)))
      QStyledItemDelegate::setModelData(editorWidget, model, index);
  }

private:
  TypeFunc typeFunc;
};

SDDSEditor::SDDSEditor(QWidget *parent)
  : QMainWindow(parent), datasetLoaded(false), dirty(false), asciiSave(true), currentPage(0), currentFilename(QString()), lastRowAddCount(1), lastSearchPattern(QString()), lastReplaceText(QString()) {
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
  asciiBtn = new QRadioButton(tr("ascii"), this);
  binaryBtn = new QRadioButton(tr("binary"), this);
  asciiBtn->setChecked(true);
  pageLayout->addWidget(asciiBtn);
  pageLayout->addWidget(binaryBtn);
  mainLayout->addLayout(pageLayout);

  QFont tableFont("Source Code Pro");
  tableFont.setStyleName("Regular");
  tableFont.setPointSize(10);

  const QString headerStyle =
      "QHeaderView::section { background-color: #f0f0f0; }";

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
  paramModel = new QStandardItemModel(this);
  paramModel->setColumnCount(1);
  paramModel->setHorizontalHeaderLabels(QStringList() << tr("Value"));
  paramView = new QTableView(paramBox);
  paramView->setFont(tableFont);
  paramView->setModel(paramModel);
  paramView->setItemDelegate(new SDDSItemDelegate(
      [this](const QModelIndex &idx) {
        return dataset.layout.parameter_definition[idx.row()].type;
      },
      paramView));
  // Let the single value column expand to take up the available space.
  // This keeps the parameter table readable even when the window is wide.
  paramView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  paramView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  paramView->verticalHeader()->setDefaultSectionSize(18);
  paramView->horizontalHeader()->setStyleSheet(headerStyle);
  paramView->verticalHeader()->setStyleSheet(headerStyle);
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
  colLayout->setContentsMargins(0, 0, 0, 0);
  columnModel = new QStandardItemModel(this);
  columnView = new QTableView(colBox);
  columnView->setFont(tableFont);
  columnView->setModel(columnModel);
  connect(columnModel, &QStandardItemModel::itemChanged, this,
          &SDDSEditor::markDirty);
  columnView->setItemDelegate(new SDDSItemDelegate(
      [this](const QModelIndex &idx) {
        return dataset.layout.column_definition[idx.column()].type;
      },
      columnView));
  columnView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  columnView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  columnView->verticalHeader()->setDefaultSectionSize(18);
  columnView->horizontalHeader()->setStyleSheet(headerStyle);
  columnView->verticalHeader()->setStyleSheet(headerStyle);
  columnView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(columnView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeColumnType);
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
  arrayModel = new QStandardItemModel(this);
  arrayView = new QTableView(arrayBox);
  arrayView->setFont(tableFont);
  arrayView->setModel(arrayModel);
  connect(arrayModel, &QStandardItemModel::itemChanged, this,
          &SDDSEditor::markDirty);
  arrayView->setItemDelegate(new SDDSItemDelegate(
      [this](const QModelIndex &idx) {
        return dataset.layout.array_definition[idx.column()].type;
      },
      arrayView));
  arrayView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  arrayView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  arrayView->verticalHeader()->setDefaultSectionSize(18);
  arrayView->horizontalHeader()->setStyleSheet(headerStyle);
  arrayView->verticalHeader()->setStyleSheet(headerStyle);
  arrayView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(arrayView->horizontalHeader(), &QHeaderView::sectionDoubleClicked,
          this, &SDDSEditor::changeArrayType);
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

  setCentralWidget(central);
  resize(1200, 800);

  // menu bar
  QMenu *fileMenu = menuBar()->addMenu(tr("File"));
  QAction *openAct = fileMenu->addAction(tr("Open"));
  QAction *saveAct = fileMenu->addAction(tr("Save"));
  QAction *saveAsAct = fileMenu->addAction(tr("Save as..."));
  QAction *restartAct = fileMenu->addAction(tr("Restart"));
  QAction *quitAct = fileMenu->addAction(tr("Quit"));
  connect(openAct, &QAction::triggered, this, &SDDSEditor::openFile);
  connect(saveAct, &QAction::triggered, this, &SDDSEditor::saveFile);
  connect(saveAsAct, &QAction::triggered, this, &SDDSEditor::saveFileAs);
  connect(restartAct, &QAction::triggered, this, &SDDSEditor::restartApp);
  connect(quitAct, &QAction::triggered, this, &QWidget::close);

  QMenu *editMenu = menuBar()->addMenu(tr("Edit"));
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

  pages.clear();
  while (SDDS_ReadPage(&in) > 0) {
    PageStore pd;
    int32_t pcount;
    char **pnames = SDDS_GetParameterNames(&in, &pcount);
    for (int32_t i = 0; i < pcount; ++i) {
      char *val = SDDS_GetParameterAsString(&in, pnames[i], NULL);
      pd.parameters.append(QString(val ? val : ""));
      free(val);
    }
    SDDS_FreeStringArray(pnames, pcount);
    int32_t ccount;
    char **cnames = SDDS_GetColumnNames(&in, &ccount);
    int64_t rows = SDDS_RowCount(&in);
    pd.columns.resize(ccount);
    for (int32_t c = 0; c < ccount; ++c) {
      char **data = SDDS_GetColumnInString(&in, cnames[c]);
      pd.columns[c].resize(rows);
      for (int64_t r = 0; r < rows; ++r)
        pd.columns[c][r] = QString(data ? data[r] : "");
      SDDS_FreeStringArray(data, rows);
    }
    SDDS_FreeStringArray(cnames, ccount);
    int32_t acount;
    char **anames = SDDS_GetArrayNames(&in, &acount);
    pd.arrays.resize(acount);
    for (int32_t a = 0; a < acount; ++a) {
      ARRAY_DEFINITION *adef = SDDS_GetArrayDefinition(&in, anames[a]);
      int32_t dim;
      char **vals = SDDS_GetArrayInString(&in, anames[a], &dim);
      pd.arrays[a].dims.resize(adef->dimensions);
      for (int d = 0; d < adef->dimensions; ++d)
        pd.arrays[a].dims[d] = in.array[a].dimension[d];
      pd.arrays[a].values.resize(dim);
      for (int i = 0; i < dim; ++i)
        pd.arrays[a].values[i] = QString(vals[i]);
      SDDS_FreeStringArray(vals, dim);
    }
    SDDS_FreeStringArray(anames, acount);
    pages.append(pd);
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
  columnView->horizontalHeader()->setStretchLastSection(true);
  columnView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);

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
  arrayView->horizontalHeader()->setStretchLastSection(true);
  arrayView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);
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
  arrayModel->setHeaderData(col, Qt::Horizontal, name.text());
  populateModels();
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

void SDDSEditor::showColumnMenu(QTableView *view, int column,
                                const QPoint &globalPos) {
  if (column < 0 || column >= dataset.layout.n_columns)
    return;
  QMenu menu(view);
  QAction *plotAct = menu.addAction(tr("Plot from file"));
  QAction *ascAct = menu.addAction(tr("Sort ascending"));
  QAction *descAct = menu.addAction(tr("Sort descending"));
  QAction *searchAct = menu.addAction(tr("Search"));
  QAction *chosen = menu.exec(globalPos);
  if (chosen == plotAct)
    plotColumn(column);
  else if (chosen == ascAct)
    sortColumn(column, Qt::AscendingOrder);
  else if (chosen == descAct)
    sortColumn(column, Qt::DescendingOrder);
  else if (chosen == searchAct)
    searchColumn(column);
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

void SDDSEditor::showArrayMenu(QTableView *view, int column,
                               const QPoint &globalPos) {
  if (column < 0 || column >= dataset.layout.n_arrays)
    return;
  QMenu menu(view);
  QAction *searchAct = menu.addAction(tr("Search"));
  QAction *resizeAct = menu.addAction(tr("Resize"));
  QAction *chosen = menu.exec(globalPos);
  if (chosen == searchAct)
    searchArray(column);
  else if (chosen == resizeAct)
    resizeArray(column);
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

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Search Column"));
  QVBoxLayout layout(&dlg);
  QFormLayout form(&dlg);
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
      columnView->closePersistentEditor(activeEditor);
    QModelIndex idx = columnModel->index(matches[matchIndex].row, column);
    columnView->setCurrentIndex(idx);
    columnView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    columnView->openPersistentEditor(idx);
    if (QWidget *w = columnView->indexWidget(idx)) {
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
      columnView->closePersistentEditor(activeEditor);
      activeEditor = QModelIndex();
    }
    if (pat.isEmpty())
      return;
    lastSearchPattern = pat;
    lastReplaceText = replaceEdit.text();
    for (int r = 0; r < columnModel->rowCount(); ++r) {
      QString val;
      QStandardItem *it = columnModel->item(r, column);
      if (it)
        val = it->text();
      int pos = 0;
      while ((pos = val.indexOf(pat, pos, Qt::CaseInsensitive)) >= 0) {
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
    QStandardItem *it = columnModel->item(m.row, column);
    if (it) {
      QString val = it->text();
      val.replace(m.start, patternEdit.text().length(), replaceEdit.text());
      it->setText(val);
    }
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
    for (int r = 0; r < columnModel->rowCount(); ++r) {
      QStandardItem *it = columnModel->item(r, column);
      if (!it)
        continue;
      QString val = it->text();
      int pos = 0;
      bool changed = false;
      while ((pos = val.indexOf(pat, pos, Qt::CaseInsensitive)) >= 0) {
        val.replace(pos, pat.length(), repl);
        pos += repl.length();
        ++replaced;
        changed = true;
      }
      if (changed)
        it->setText(val);
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
    columnView->closePersistentEditor(activeEditor);
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
  QFormLayout form(&dlg);
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
      QStandardItem *it = arrayModel->item(r, column);
      if (it)
        val = it->text();
      int pos = 0;
      while ((pos = val.indexOf(pat, pos, Qt::CaseInsensitive)) >= 0) {
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
    QStandardItem *it = arrayModel->item(m.row, column);
    if (it) {
      QString val = it->text();
      val.replace(m.start, patternEdit.text().length(), replaceEdit.text());
      it->setText(val);
    }
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
      QStandardItem *it = arrayModel->item(r, column);
      if (!it)
        continue;
      QString val = it->text();
      int pos = 0;
      bool changed = false;
      while ((pos = val.indexOf(pat, pos, Qt::CaseInsensitive)) >= 0) {
        val.replace(pos, pat.length(), repl);
        pos += repl.length();
        ++replaced;
        changed = true;
      }
      if (changed)
        it->setText(val);
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
  if (!datasetLoaded)
    return;

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
  if (!datasetLoaded)
    return;

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
  if (!datasetLoaded)
    return;

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
  markDirty();
}

void SDDSEditor::deleteColumnRows() {
  if (!datasetLoaded)
    return;

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
  if (!datasetLoaded)
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

void SDDSEditor::restartApp() {
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

