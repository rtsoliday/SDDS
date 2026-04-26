/**
 * @file bely_upload.cc
 * @brief Standalone Qt utility for uploading a file attachment to BELY.
 *
 * @section Usage
 * | Command | Description |
 * |---------|-------------|
 * | `bely_upload -upload <file>` | Opens a BELY upload dialog for the given file. |
 *
 * @details
 * This utility is intended for programs that need to delegate BELY file uploads
 * to a small interactive helper. It shares the same BELY settings and keyring
 * service name used by mpl_qt.
 *
 * @copyright
 * Copyright (c) 2026, UChicago Argonne, LLC.
 * @license
 * EPICS Open License.
 */

#include <QApplication>
#include <QByteArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QVBoxLayout>
#include <cstdio>

static constexpr int kBelyUploadTimeoutMs = 120000;
static const char *kDefaultBelyUrl = "https://bely.aps.anl.gov/bely";
static const char *kDefaultBelyEntryText = "File uploaded to BELY.";
static const char *kBelySecretToolService = "mpl_qt-bely";

struct BelyUploadOptions {
  QString belyUrl;
  QString username;
  QString password;
  QString logDocumentId;
  QString logEntryId;
  QString logEntryText;
  QString attachmentName;
};

static QString findSecretToolForBely() {
#if defined(Q_OS_LINUX)
  return QStandardPaths::findExecutable(QStringLiteral("secret-tool"));
#else
  return QString();
#endif
}

static QString loadBelyPasswordFromKeyring(const QString &belyUrl,
                                           const QString &username) {
  const QString secretTool = findSecretToolForBely();
  if (secretTool.isEmpty() || belyUrl.isEmpty() || username.isEmpty())
    return QString();

  QProcess process;
  process.start(secretTool,
                QStringList() << QStringLiteral("lookup")
                              << QStringLiteral("service")
                              << QString::fromUtf8(kBelySecretToolService)
                              << QStringLiteral("url") << belyUrl
                              << QStringLiteral("username") << username);
  if (!process.waitForStarted(3000))
    return QString();
  if (!process.waitForFinished(5000)) {
    process.kill();
    process.waitForFinished();
    return QString();
  }
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    return QString();
  return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

static void storeBelyPasswordInKeyring(const QString &belyUrl,
                                       const QString &username,
                                       const QString &password) {
  const QString secretTool = findSecretToolForBely();
  if (secretTool.isEmpty() || belyUrl.isEmpty() || username.isEmpty() ||
      password.isEmpty())
    return;

  QProcess process;
  process.start(secretTool,
                QStringList() << QStringLiteral("store")
                              << QStringLiteral("--label=mpl_qt BELY password")
                              << QStringLiteral("service")
                              << QString::fromUtf8(kBelySecretToolService)
                              << QStringLiteral("url") << belyUrl
                              << QStringLiteral("username") << username);
  if (!process.waitForStarted(3000))
    return;
  process.write(password.toUtf8());
  process.closeWriteChannel();
  if (!process.waitForFinished(5000)) {
    process.kill();
    process.waitForFinished();
  }
}

static QString findPythonForBely() {
  const QString oagPython = QStandardPaths::findExecutable(QStringLiteral("oagpython"));
  if (!oagPython.isEmpty())
    return oagPython;
  const QString python3 = QStandardPaths::findExecutable(QStringLiteral("python3"));
  if (!python3.isEmpty())
    return python3;
  return QStandardPaths::findExecutable(QStringLiteral("python"));
}

static QString defaultAttachmentName(const QString &uploadPath) {
  const QFileInfo fileInfo(uploadPath);
  const QString fileName = fileInfo.fileName();
  return fileName.isEmpty() ? QStringLiteral("bely-upload") : fileName;
}

static bool parseBelyJsonResponse(const QByteArray &stdoutData,
                                  QJsonObject *payload) {
  const QList<QByteArray> lines = stdoutData.split('\n');
  for (int i = lines.size() - 1; i >= 0; --i) {
    const QByteArray line = lines[i].trimmed();
    if (line.isEmpty())
      continue;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
      if (payload)
        *payload = doc.object();
      return true;
    }
  }
  return false;
}

static QString belyLookupPythonScript() {
  return QString::fromUtf8(R"PY(import json
import os
import sys


def fail(message, detail=None, code=1):
  payload = {"ok": False, "error": message}
  if detail:
    payload["detail"] = detail
  print(json.dumps(payload))
  sys.exit(code)


def simplify_bely_error(exc):
  text = str(exc)
  lowered = text.lower()
  if "could not verify username or password" in lowered:
    return "Could not verify username or password."
  if "401" in lowered and "unauthorized" in lowered:
    return "Could not verify username or password."
  return text


try:
  from BelyApiFactory import BelyApiFactory
except Exception as exc:
  fail("Unable to import the BELY Python client. Install the bely-api package.", str(exc), 2)


required = ["BELY_URL", "BELY_USERNAME", "BELY_PASSWORD"]
for key in required:
  if not os.environ.get(key):
    fail(f"Missing required environment variable: {key}", code=3)


def get_attr(obj, *names):
  for name in names:
    if hasattr(obj, name):
      value = getattr(obj, name)
      if value is not None:
        return value
    underscored_name = f"_{name}"
    if hasattr(obj, underscored_name):
      value = getattr(obj, underscored_name)
      if value is not None:
        return value
  if isinstance(obj, dict):
    for name in names:
      value = obj.get(name)
      if value is not None:
        return value
      value = obj.get(f"_{name}")
      if value is not None:
        return value
  return None


def get_api(factory, *names):
  for name in names:
    if hasattr(factory, name):
      candidate = getattr(factory, name)
      if callable(candidate):
        try:
          return candidate()
        except TypeError:
          pass
      elif candidate is not None:
        return candidate
  return None


def call_first_method(obj, method_names, **kwargs):
  if obj is None:
    return None
  for name in method_names:
    if not hasattr(obj, name):
      continue
    candidate = getattr(obj, name)
    if not callable(candidate):
      continue
    try:
      return candidate(**kwargs)
    except TypeError:
      continue
  return None


def normalize_text(value):
  return str(value).strip().lower() if value is not None else ""


def object_matches_username(obj, username):
  target = normalize_text(username)
  for name in (
    "username",
    "user_name",
    "created_by",
    "modified_by",
    "entered_by_username",
    "last_modified_by_username",
  ):
    if normalize_text(get_attr(obj, name)) == target:
      return True
  return False


def timestamp_value(obj):
  for name in (
    "last_modified_date",
    "modified_date",
    "created_date",
    "entered_on_date_time",
    "last_modified_on_date_time",
  ):
    value = get_attr(obj, name)
    if value is not None:
      return str(value)
  return ""


def sort_latest(items):
  return sorted(items, key=timestamp_value, reverse=True)


def choose_best_entry(entries, username):
  matching = [entry for entry in entries if object_matches_username(entry, username)]
  candidates = matching or list(entries)
  candidates = sort_latest(candidates)
  return candidates[0] if candidates else None


def get_collection(result, *names):
  value = get_attr(result, *names)
  if value is None:
    return []
  try:
    return list(value)
  except TypeError:
    return []


def collect_entries_for_document(logbook_api, document_id):
  if logbook_api is None or document_id is None:
    return []
  for kwargs in (
    {"log_document_id": int(document_id)},
    {"item_id": int(document_id)},
    {"logDocumentId": int(document_id)},
  ):
    result = call_first_method(
      logbook_api,
      [
        "get_log_entries",
        "getLogEntries",
        "get_log_entries_for_document",
        "getLogEntriesForDocument",
      ],
      **kwargs,
    )
    if result is not None:
      entries = get_collection(result, "log_entries", "log_entry_results", "items")
      if entries:
        return entries
      if isinstance(result, (list, tuple)):
        return list(result)
  return []


try:
  username = os.environ["BELY_USERNAME"]
  api_factory = BelyApiFactory(bely_url=os.environ["BELY_URL"])
  api_factory.authenticate_user(username, os.environ["BELY_PASSWORD"])
  logbook_api = get_api(api_factory, "get_logbook_api", "getLogbookApi", "get_lobook_api", "logbook_api")
  search_api = get_api(api_factory, "get_search_api", "getSearchApi", "search_api")

  entry_results = []
  document_results = []
  if search_api is not None:
    results = call_first_method(
      search_api,
      ["search_logbook", "searchLogbook", "search", "search_logbook_search_post"],
      search_text="*",
      case_insensitive=True,
    )
    if results is not None:
      entry_results = sort_latest(get_collection(results, "log_entry_results", "entries"))
      document_results = sort_latest(get_collection(results, "document_results", "documents"))

  entry_results = [item for item in entry_results if object_matches_username(item, username)]
  document_results = [item for item in document_results if object_matches_username(item, username)]

  chosen_document_id = None
  chosen_log_entry_id = None
  if document_results:
    document_result = document_results[0]
    chosen_document_id = get_attr(document_result, "object_id", "log_document_id", "document_id", "id")
    best_entry = choose_best_entry(
      collect_entries_for_document(logbook_api, chosen_document_id),
      username,
    )
    if best_entry is not None:
      chosen_log_entry_id = get_attr(best_entry, "log_entry_id", "id")
  if chosen_document_id is None or chosen_log_entry_id is None:
    best_entry = choose_best_entry(entry_results, username)
    if best_entry is not None:
      chosen_document_id = get_attr(best_entry, "object_id", "log_document_id", "document_id", "item_id")
      chosen_log_entry_id = get_attr(best_entry, "log_entry_id", "id")

  if chosen_document_id is None or chosen_log_entry_id is None:
    fail("No recent BELY log entry could be found for this user.")

  print(json.dumps({
    "ok": True,
    "log_document_id": str(chosen_document_id),
    "log_entry_id": str(chosen_log_entry_id),
  }))
  try:
    api_factory.logout_user()
  except Exception:
    pass
except Exception as exc:
  fail("Unable to look up the latest BELY log entry for this user.", simplify_bely_error(exc), 4)
)PY");
}

static bool lookupLatestBelyIds(QWidget *parent, const QString &pythonExecutable,
                                const QString &belyUrl,
                                const QString &username,
                                const QString &password,
                                QString *logDocumentId,
                                QString *logEntryId) {
  if (!logDocumentId || !logEntryId)
    return false;

  QProcess process;
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("BELY_URL"), belyUrl);
  environment.insert(QStringLiteral("BELY_USERNAME"), username);
  environment.insert(QStringLiteral("BELY_PASSWORD"), password);
  process.setProcessEnvironment(environment);

  QApplication::setOverrideCursor(Qt::WaitCursor);
  process.start(pythonExecutable,
                QStringList() << QStringLiteral("-c")
                              << belyLookupPythonScript());
  if (!process.waitForStarted()) {
    QApplication::restoreOverrideCursor();
    QMessageBox::warning(parent, QStringLiteral("Lookup Failed"),
                         QStringLiteral("Failed to start the Python lookup helper process."));
    return false;
  }

  const bool finished = process.waitForFinished(kBelyUploadTimeoutMs);
  QApplication::restoreOverrideCursor();

  if (!finished) {
    process.kill();
    process.waitForFinished();
    QMessageBox::warning(parent, QStringLiteral("Lookup Timed Out"),
                         QStringLiteral("The BELY lookup did not finish within %1 seconds.")
                             .arg(kBelyUploadTimeoutMs / 1000));
    return false;
  }

  const QByteArray stdoutData = process.readAllStandardOutput();
  const QByteArray stderrData = process.readAllStandardError();
  QJsonObject payload;
  const bool parsed = parseBelyJsonResponse(stdoutData, &payload);
  const bool ok = parsed && payload.value(QStringLiteral("ok")).toBool(false) &&
                  process.exitStatus() == QProcess::NormalExit &&
                  process.exitCode() == 0;
  if (!ok) {
    QString message;
    if (parsed) {
      message = payload.value(QStringLiteral("error")).toString();
      const QString detail = payload.value(QStringLiteral("detail")).toString();
      if (!detail.isEmpty())
        message += QStringLiteral("\n\n") + detail;
    }
    if (message.isEmpty() && !stderrData.trimmed().isEmpty())
      message = QString::fromUtf8(stderrData.trimmed());
    if (message.isEmpty() && !stdoutData.trimmed().isEmpty())
      message = QString::fromUtf8(stdoutData.trimmed());
    if (message.isEmpty())
      message = QStringLiteral("Unable to determine the latest BELY IDs.");
    QMessageBox::warning(parent, QStringLiteral("Lookup Failed"), message);
    return false;
  }

  *logDocumentId = payload.value(QStringLiteral("log_document_id")).toString();
  *logEntryId = payload.value(QStringLiteral("log_entry_id")).toString();
  storeBelyPasswordInKeyring(belyUrl, username, password);
  return !logDocumentId->isEmpty() && !logEntryId->isEmpty();
}

static bool promptForBelyUploadOptions(QWidget *parent,
                                       const QString &uploadPath,
                                       BelyUploadOptions *options) {
  if (!options)
    return false;

  QSettings settings(QStringLiteral("APS"), QStringLiteral("mpl_qt"));
  settings.beginGroup(QStringLiteral("BELY"));

  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Upload to BELY"));
  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  QLabel *description = new QLabel(
      QStringLiteral("Upload %1 as an attachment to a BELY log document. "
                     "Provide a log entry ID to attach to an existing entry, "
                     "or leave it blank to create a new log entry first. "
                     "This action requires Python with the bely-api package installed.")
          .arg(QFileInfo(uploadPath).fileName()),
      &dialog);
  description->setWordWrap(true);
  layout->addWidget(description);

  QFormLayout *form = new QFormLayout();
  QLineEdit *urlEdit = new QLineEdit(
      settings.value(QStringLiteral("url"), QString::fromUtf8(kDefaultBelyUrl)).toString(),
      &dialog);
  urlEdit->setPlaceholderText(QString::fromUtf8(kDefaultBelyUrl));
  form->addRow(QStringLiteral("BELY URL"), urlEdit);

  QLineEdit *usernameEdit = new QLineEdit(
      settings.value(QStringLiteral("username")).toString(), &dialog);
  form->addRow(QStringLiteral("Username"), usernameEdit);

  QLineEdit *passwordEdit = new QLineEdit(&dialog);
  passwordEdit->setEchoMode(QLineEdit::Password);
  const QString initialKeyringPassword = loadBelyPasswordFromKeyring(
      urlEdit->text().trimmed(), usernameEdit->text().trimmed());
  if (!initialKeyringPassword.isEmpty())
    passwordEdit->setText(initialKeyringPassword);
  form->addRow(QStringLiteral("Password"), passwordEdit);

  QRegularExpression digits(QStringLiteral("[0-9]+"));
  QValidator *idValidator = new QRegularExpressionValidator(digits, &dialog);

  QLineEdit *logDocumentIdEdit = new QLineEdit(
      settings.value(QStringLiteral("logDocumentId")).toString(), &dialog);
  logDocumentIdEdit->setValidator(idValidator);
  form->addRow(QStringLiteral("Log document ID"), logDocumentIdEdit);

  QLineEdit *logEntryIdEdit = new QLineEdit(
      settings.value(QStringLiteral("logEntryId")).toString(), &dialog);
  logEntryIdEdit->setValidator(idValidator);
  logEntryIdEdit->setPlaceholderText(QStringLiteral("Leave blank to create a new entry"));
  form->addRow(QStringLiteral("Log entry ID (optional)"), logEntryIdEdit);

  QString savedEntryText = settings.value(QStringLiteral("logEntryText")).toString();
  QLineEdit *logEntryTextEdit = new QLineEdit(
      savedEntryText.isEmpty() ? QString::fromUtf8(kDefaultBelyEntryText)
                               : savedEntryText,
      &dialog);
  logEntryTextEdit->setPlaceholderText(QString::fromUtf8(kDefaultBelyEntryText));
  form->addRow(QStringLiteral("New entry text"), logEntryTextEdit);

  QPushButton *findLatestButton =
      new QPushButton(QStringLiteral("Find Latest For User"), &dialog);
  QLabel *lookupHint = new QLabel(
      QStringLiteral("Uses the current BELY URL, username, and password to "
                     "populate the latest document and entry IDs for that user."),
      &dialog);
  lookupHint->setWordWrap(true);
  QVBoxLayout *lookupLayout = new QVBoxLayout();
  lookupLayout->addWidget(findLatestButton);
  lookupLayout->addWidget(lookupHint);
  QWidget *lookupWidget = new QWidget(&dialog);
  lookupWidget->setLayout(lookupLayout);
  form->addRow(QString(), lookupWidget);

  QLineEdit *attachmentNameEdit = new QLineEdit(
      defaultAttachmentName(uploadPath), &dialog);
  form->addRow(QStringLiteral("Attachment file name"), attachmentNameEdit);

  layout->addLayout(form);

  QDialogButtonBox *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &dialog);
  QObject::connect(findLatestButton, &QPushButton::clicked, &dialog,
                   [&]() {
                     const QString belyUrl = urlEdit->text().trimmed();
                     const QString username = usernameEdit->text().trimmed();
                     QString password = passwordEdit->text();
                     if (password.isEmpty()) {
                       password = loadBelyPasswordFromKeyring(belyUrl, username);
                       if (!password.isEmpty())
                         passwordEdit->setText(password);
                     }
                     if (belyUrl.isEmpty() || username.isEmpty() || password.isEmpty()) {
                       QMessageBox::warning(
                           &dialog, QStringLiteral("Missing Information"),
                           QStringLiteral("Enter the BELY URL, username, and password "
                                          "before looking up the latest IDs."));
                       return;
                     }
                     const QString pythonExecutable = findPythonForBely();
                     if (pythonExecutable.isEmpty()) {
                       QMessageBox::warning(
                           &dialog, QStringLiteral("Python Not Found"),
                           QStringLiteral("Unable to find python3 or python in PATH. "
                                          "Install Python and the bely-api package to use this feature."));
                       return;
                     }
                     QString foundLogDocumentId;
                     QString foundLogEntryId;
                     if (lookupLatestBelyIds(&dialog, pythonExecutable, belyUrl,
                                             username, password, &foundLogDocumentId,
                                             &foundLogEntryId)) {
                       logDocumentIdEdit->setText(foundLogDocumentId);
                       logEntryIdEdit->setText(foundLogEntryId);
                       QMessageBox::information(
                           &dialog, QStringLiteral("BELY IDs Found"),
                           QStringLiteral("Populated the latest document ID %1 and "
                                          "entry ID %2 for %3.")
                               .arg(foundLogDocumentId, foundLogEntryId, username));
                     }
                   });
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
                   [&]() {
                     if (passwordEdit->text().isEmpty()) {
                       const QString keyringPassword = loadBelyPasswordFromKeyring(
                           urlEdit->text().trimmed(), usernameEdit->text().trimmed());
                       if (!keyringPassword.isEmpty())
                         passwordEdit->setText(keyringPassword);
                     }
                     if (urlEdit->text().trimmed().isEmpty() ||
                         usernameEdit->text().trimmed().isEmpty() ||
                         passwordEdit->text().isEmpty() ||
                         logDocumentIdEdit->text().trimmed().isEmpty() ||
                         logEntryTextEdit->text().trimmed().isEmpty() ||
                         attachmentNameEdit->text().trimmed().isEmpty()) {
                       QMessageBox::warning(
                           &dialog, QStringLiteral("Missing Information"),
                           QStringLiteral("BELY URL, username, password, log document ID, "
                                          "new entry text, and attachment file name are required."));
                       return;
                     }
                     if (!logEntryIdEdit->text().trimmed().isEmpty() &&
                         logDocumentIdEdit->text().trimmed() ==
                         logEntryIdEdit->text().trimmed()) {
                       const QMessageBox::StandardButton choice = QMessageBox::warning(
                           &dialog, QStringLiteral("Verify IDs"),
                           QStringLiteral("The log document ID and log entry ID are both %1. "
                                          "This is unusual for BELY uploads. Continue anyway?")
                               .arg(logDocumentIdEdit->text().trimmed()),
                           QMessageBox::Yes | QMessageBox::No,
                           QMessageBox::No);
                       if (choice != QMessageBox::Yes)
                         return;
                     }
                     dialog.accept();
                   });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted)
    return false;

  options->belyUrl = urlEdit->text().trimmed();
  options->username = usernameEdit->text().trimmed();
  options->password = passwordEdit->text();
  options->logDocumentId = logDocumentIdEdit->text().trimmed();
  options->logEntryId = logEntryIdEdit->text().trimmed();
  options->logEntryText = logEntryTextEdit->text().trimmed();
  options->attachmentName = attachmentNameEdit->text().trimmed();

  settings.setValue(QStringLiteral("url"), options->belyUrl);
  settings.setValue(QStringLiteral("username"), options->username);
  settings.setValue(QStringLiteral("logDocumentId"), options->logDocumentId);
  settings.setValue(QStringLiteral("logEntryId"), options->logEntryId);
  settings.setValue(QStringLiteral("logEntryText"), options->logEntryText);

  return true;
}

static QString belyUploadPythonScript() {
  return QString::fromUtf8(R"PY(import base64
import json
import os
import sys
from urllib import error, parse, request


def fail(message, detail=None, code=1):
  payload = {"ok": False, "error": message}
  if detail:
    payload["detail"] = detail
  print(json.dumps(payload))
  sys.exit(code)


def debug(message):
  print(f"[BELY upload] {message}", flush=True)


def simplify_bely_error(exc):
  text = str(exc)
  lowered = text.lower()
  if "could not verify username or password" in lowered:
    return "Could not verify username or password."
  if "401" in lowered and "unauthorized" in lowered:
    return "Could not verify username or password."
  return text


try:
  from BelyApiFactory import BelyApiFactory
except Exception as exc:
  fail("Unable to import the BELY Python client. Install the bely-api package.", str(exc), 2)


def get_api(factory, *names):
  for name in names:
    if hasattr(factory, name):
      candidate = getattr(factory, name)
      if callable(candidate):
        try:
          return candidate()
        except TypeError:
          pass
      elif candidate is not None:
        return candidate
  return None


def call_first_method(obj, method_names, **kwargs):
  if obj is None:
    return None
  for name in method_names:
    if not hasattr(obj, name):
      continue
    candidate = getattr(obj, name)
    if not callable(candidate):
      continue
    try:
      return candidate(**kwargs)
    except TypeError:
      continue
  return None


def make_multipart_payload(field_name, binary_data, file_name, extra_fields):
  boundary = "----sddsplotbelyboundary"
  chunks = []
  for key, value in extra_fields.items():
    chunks.append(f"--{boundary}\r\n".encode("utf-8"))
    chunks.append(f'Content-Disposition: form-data; name="{key}"\r\n\r\n'.encode("utf-8"))
    chunks.append(f"{value}\r\n".encode("utf-8"))
  chunks.append(f"--{boundary}\r\n".encode("utf-8"))
  chunks.append(
    f'Content-Disposition: form-data; name="{field_name}"; filename="{file_name}"\r\n'.encode("utf-8")
  )
  chunks.append(b"Content-Type: application/octet-stream\r\n\r\n")
  chunks.append(binary_data)
  chunks.append(b"\r\n")
  chunks.append(f"--{boundary}--\r\n".encode("utf-8"))
  return b"".join(chunks), f"multipart/form-data; boundary={boundary}"


def direct_upload_attachment(api_factory, bely_url, log_document_id, log_entry_id,
                             upload_path, file_name, append_reference):
  token = None
  if hasattr(api_factory, "api_client") and hasattr(api_factory.api_client, "default_headers"):
    token = api_factory.api_client.default_headers.get("token")
  if not token:
    fail("BELY upload failed.", "Unable to obtain an authenticated token for direct upload fallback.")

  with open(upload_path, "rb") as handle:
    binary_data = handle.read()

  encoded_data = base64.b64encode(binary_data).decode("ascii")
  quoted_name = parse.quote(file_name, safe="")
  append_values = ["true" if append_reference else "false", "1" if append_reference else "0"]
  multipart_body, multipart_content_type = make_multipart_payload(
    "body",
    binary_data,
    file_name,
    {
      "logDocumentId": log_document_id,
      "logId": log_entry_id,
      "appendReference": "true" if append_reference else "false",
      "fileName": file_name,
    },
  )
  multipart_file_body, multipart_file_content_type = make_multipart_payload(
    "file",
    binary_data,
    file_name,
    {
      "logDocumentId": log_document_id,
      "logId": log_entry_id,
      "appendReference": "true" if append_reference else "false",
      "fileName": file_name,
    },
  )
  body_candidates = [
    ("json-camel", json.dumps({"fileName": file_name, "base64Binary": encoded_data}).encode("utf-8"), "application/json"),
    ("json-snake", json.dumps({"file_name": file_name, "base64_binary": encoded_data}).encode("utf-8"), "application/json"),
    ("raw-binary", binary_data, "application/octet-stream"),
    ("multipart-body", multipart_body, multipart_content_type),
    ("multipart-file", multipart_file_body, multipart_file_content_type),
  ]

  candidate_requests = []
  seen_candidates = set()

  def add_candidate(method, url):
    key = (method, url)
    if key not in seen_candidates:
      seen_candidates.add(key)
      candidate_requests.append(key)

  add_candidate("PUT", f"{bely_url}/api/Logbook/UploadAttachment/{log_document_id}/{log_entry_id}")
  add_candidate("PUT", f"{bely_url}/api/Logbook/UploadAttachment/{log_document_id}/{log_entry_id}?appendReference={'true' if append_reference else 'false'}&fileName={quoted_name}")
  add_candidate("PUT", f"{bely_url}/api/Logbook/UploadAttachment?logDocumentId={log_document_id}&logId={log_entry_id}&appendReference={'true' if append_reference else 'false'}&fileName={quoted_name}")
  add_candidate("POST", f"{bely_url}/api/Logbook/UploadAttachment/{log_document_id}/{log_entry_id}")
  add_candidate("POST", f"{bely_url}/api/Logbook/UploadAttachment/{log_document_id}/{log_entry_id}?appendReference={'true' if append_reference else 'false'}&fileName={quoted_name}")
  add_candidate("POST", f"{bely_url}/api/Logbook/UploadAttachment?logDocumentId={log_document_id}&logId={log_entry_id}&appendReference={'true' if append_reference else 'false'}&fileName={quoted_name}")

  controllers = ["Logbook", "Downloads"]
  actions = ["UploadAttachment", "UploadAttachments", "AddAttachment", "AddLogAttachment"]
  for append_value in append_values:
    for controller in controllers:
      for action in actions:
        for method in ("PUT", "POST"):
          add_candidate(method, f"{bely_url}/api/{controller}/{action}/{log_document_id}/{log_entry_id}/{append_value}/{quoted_name}")
          add_candidate(method, f"{bely_url}/api/{controller}/{action}/{log_document_id}/{log_entry_id}/{quoted_name}/{append_value}")
          add_candidate(method, f"{bely_url}/api/{controller}/{action}/{log_document_id}/{log_entry_id}/{append_value}")
          add_candidate(method, f"{bely_url}/api/{controller}/{action}/{log_document_id}/{log_entry_id}")
          add_candidate(method, f"{bely_url}/api/{controller}/{action}?logDocumentId={log_document_id}&logId={log_entry_id}&appendReference={append_value}&fileName={quoted_name}")
          add_candidate(method, f"{bely_url}/api/{controller}/{action}?itemId={log_document_id}&logId={log_entry_id}&appendReference={append_value}&fileName={quoted_name}")
          add_candidate(method, f"{bely_url}/api/{controller}/{action}")

  priority_errors = []
  other_errors = []
  for method, url in candidate_requests:
    for body_name, payload, content_type in body_candidates:
      headers = {
        "token": token,
        "Accept": "application/json",
        "Content-Type": content_type,
      }
      debug(f"Trying direct upload via {method} {url} [{body_name}]")
      req = request.Request(url, data=payload, headers=headers, method=method)
      try:
        with request.urlopen(req, timeout=120) as response:
          response_body = response.read().decode("utf-8", "replace")
          debug(f"Direct upload succeeded via {method} {url} [{body_name}]")
          if not response_body.strip():
            return {}
          try:
            return json.loads(response_body)
          except Exception:
            return {"raw_response": response_body}
      except error.HTTPError as exc:
        response_body = exc.read().decode("utf-8", "replace")
        allow_header = exc.headers.get("Allow") if exc.headers else None
        last_error = f"{method} {url} [{body_name}] -> HTTP {exc.code}"
        if allow_header:
          last_error += f" Allow={allow_header}"
        if response_body:
          last_error += f": {response_body[:240]}"
        if exc.code != 404:
          priority_errors.append(last_error)
        else:
          other_errors.append(last_error)
        debug(last_error)
      except Exception as exc:
        last_error = f"{method} {url} [{body_name}] -> {exc}"
        priority_errors.append(last_error)
        debug(last_error)

  attempt_errors = priority_errors + other_errors
  if attempt_errors:
    detail = "\n".join(attempt_errors[:20])
    if len(attempt_errors) > 20:
      detail += f"\n... {len(attempt_errors) - 20} more attempts omitted"
    fail("BELY upload failed.", detail)
  fail("BELY upload failed.", "Direct upload fallback could not find a compatible upload endpoint.")


required = [
  "BELY_URL",
  "BELY_USERNAME",
  "BELY_PASSWORD",
  "BELY_LOG_DOCUMENT_ID",
  "BELY_LOG_ENTRY_TEXT",
  "BELY_UPLOAD_PATH",
  "BELY_FILE_NAME",
]
for key in required:
  if not os.environ.get(key):
    fail(f"Missing required environment variable: {key}", code=3)


try:
  from belyApi.models.log_entry import LogEntry
except Exception:
  try:
    from belyApi import LogEntry
  except Exception:
    LogEntry = None


def resolve_log_entry_id(entry):
  for name in ("log_id", "log_entry_id", "id"):
    value = getattr(entry, name, None)
    if value is not None:
      return value
    underscored = getattr(entry, f"_{name}", None)
    if underscored is not None:
      return underscored
  if isinstance(entry, dict):
    for name in ("log_id", "log_entry_id", "id"):
      value = entry.get(name)
      if value is not None:
        return value
      value = entry.get(f"_{name}")
      if value is not None:
        return value
  return None


def create_log_entry(logbook_api, log_document_id, entry_text):
  if logbook_api is None:
    fail("BELY upload failed.", "A compatible logbook API is required to create a new log entry.")
  if LogEntry is None:
    fail("BELY upload failed.", "The installed BELY Python client is missing the LogEntry model needed to create a new log entry.")

  log_entry = LogEntry(item_id=log_document_id, log_entry=entry_text)
  created_entry = call_first_method(
    logbook_api,
    ["add_update_log_entry", "addUpdateLogEntry"],
    log_entry=log_entry,
  )
  if created_entry is None:
    fail("BELY upload failed.", "The installed BELY Python client could not create a new log entry for the selected document.")

  created_entry_id = resolve_log_entry_id(created_entry)
  if created_entry_id is None:
    fail("BELY upload failed.", "A new BELY log entry was created, but its entry ID could not be determined.")
  debug(f"Created log entry {created_entry_id} for document {log_document_id}")
  return int(created_entry_id)


try:
  bely_url = os.environ["BELY_URL"]
  username = os.environ["BELY_USERNAME"]
  password = os.environ["BELY_PASSWORD"]
  log_document_id = int(os.environ["BELY_LOG_DOCUMENT_ID"])
  log_entry_id_text = os.environ.get("BELY_LOG_ENTRY_ID", "").strip()
  upload_path = os.environ["BELY_UPLOAD_PATH"]
  file_name = os.environ["BELY_FILE_NAME"]
  log_entry_text = os.environ["BELY_LOG_ENTRY_TEXT"]
  append_reference = os.environ.get("BELY_APPEND_REFERENCE", "1") == "1"

  api_factory = BelyApiFactory(bely_url=bely_url)
  api_factory.authenticate_user(username, password)
  logbook_api = get_api(api_factory, "get_logbook_api", "getLogbookApi", "get_lobook_api", "logbook_api")
  if log_entry_id_text:
    log_entry_id = int(log_entry_id_text)
  else:
    log_entry_id = create_log_entry(logbook_api, log_document_id, log_entry_text)
  attachment = None
  if logbook_api is not None:
    debug(f"Resolved logbook_api={type(logbook_api).__name__}")
    attachment = call_first_method(
      logbook_api,
      ["upload_attachment", "uploadAttachment"],
      log_document_id=log_document_id,
      log_id=log_entry_id,
      body=upload_path,
      append_reference=append_reference,
      file_name=file_name,
    )
  if attachment is None:
    debug("No compatible upload_attachment wrapper found; using direct REST fallback")
    attachment = direct_upload_attachment(
      api_factory,
      bely_url,
      log_document_id,
      log_entry_id,
      upload_path,
      file_name,
      append_reference,
    )
  try:
    api_factory.logout_user()
  except Exception:
    pass

  payload = {
    "ok": True,
    "log_document_id": str(log_document_id),
    "log_entry_id": str(log_entry_id),
    "download_path": getattr(attachment, "download_path", attachment.get("download_path", "") if isinstance(attachment, dict) else ""),
    "markdown_reference": getattr(attachment, "markdown_reference", attachment.get("markdown_reference", "") if isinstance(attachment, dict) else ""),
    "original_filename": getattr(attachment, "original_filename", attachment.get("original_filename", "") if isinstance(attachment, dict) else ""),
    "stored_filename": getattr(attachment, "stored_filename", attachment.get("stored_filename", "") if isinstance(attachment, dict) else ""),
    "append_reference": append_reference,
  }
  print(json.dumps(payload))
except Exception as exc:
  fail("BELY upload failed.", simplify_bely_error(exc), 4)
)PY");
}

static bool uploadFileToBely(QWidget *parent, const QString &uploadPath,
                             const BelyUploadOptions &options) {
  const QString pythonExecutable = findPythonForBely();
  if (pythonExecutable.isEmpty()) {
    QMessageBox::warning(
        parent, QStringLiteral("Python Not Found"),
        QStringLiteral("Unable to find python3 or python in PATH. "
                       "Install Python and the bely-api package to use this feature."));
    return false;
  }

  QProcess process;
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("BELY_URL"), options.belyUrl);
  environment.insert(QStringLiteral("BELY_USERNAME"), options.username);
  environment.insert(QStringLiteral("BELY_PASSWORD"), options.password);
  environment.insert(QStringLiteral("BELY_LOG_DOCUMENT_ID"), options.logDocumentId);
  if (!options.logEntryId.isEmpty())
    environment.insert(QStringLiteral("BELY_LOG_ENTRY_ID"), options.logEntryId);
  environment.insert(QStringLiteral("BELY_LOG_ENTRY_TEXT"), options.logEntryText);
  environment.insert(QStringLiteral("BELY_UPLOAD_PATH"), uploadPath);
  environment.insert(QStringLiteral("BELY_FILE_NAME"), options.attachmentName);
  environment.insert(QStringLiteral("BELY_APPEND_REFERENCE"), QStringLiteral("1"));
  process.setProcessEnvironment(environment);

  QApplication::setOverrideCursor(Qt::WaitCursor);
  process.start(pythonExecutable,
                QStringList() << QStringLiteral("-c")
                              << belyUploadPythonScript());
  if (!process.waitForStarted()) {
    QApplication::restoreOverrideCursor();
    QMessageBox::warning(parent, QStringLiteral("Upload Failed"),
                         QStringLiteral("Failed to start the Python upload helper process."));
    return false;
  }
  const bool finished = process.waitForFinished(kBelyUploadTimeoutMs);
  QApplication::restoreOverrideCursor();

  const QByteArray stdoutData = process.readAllStandardOutput();
  const QByteArray stderrData = process.readAllStandardError();

  if (!finished) {
    process.kill();
    process.waitForFinished();
    QMessageBox::warning(parent, QStringLiteral("Upload Timed Out"),
                         QStringLiteral("The BELY upload did not finish within %1 seconds.")
                             .arg(kBelyUploadTimeoutMs / 1000));
    return false;
  }

  QJsonObject payload;
  const bool parsed = parseBelyJsonResponse(stdoutData, &payload);
  const bool ok = parsed && payload.value(QStringLiteral("ok")).toBool(false) &&
                  process.exitStatus() == QProcess::NormalExit &&
                  process.exitCode() == 0;
  if (!ok) {
    QString message;
    if (parsed) {
      message = payload.value(QStringLiteral("error")).toString();
      const QString detail = payload.value(QStringLiteral("detail")).toString();
      if (!detail.isEmpty())
        message += QStringLiteral("\n\n") + detail;
    }
    if (message.isEmpty() && !stderrData.trimmed().isEmpty())
      message = QString::fromUtf8(stderrData.trimmed());
    if (message.isEmpty() && !stdoutData.trimmed().isEmpty())
      message = QString::fromUtf8(stdoutData.trimmed());
    if (message.isEmpty())
      message = QStringLiteral("The BELY upload failed for an unknown reason.");
    QMessageBox::warning(parent, QStringLiteral("Upload Failed"), message);
    return false;
  }

  storeBelyPasswordInKeyring(options.belyUrl, options.username,
                             options.password);

  QStringList details;
  const QString finalLogDocumentId =
      payload.value(QStringLiteral("log_document_id")).toString(options.logDocumentId);
  const QString finalLogEntryId =
      payload.value(QStringLiteral("log_entry_id")).toString(options.logEntryId);
  if (options.logEntryId.isEmpty()) {
    details << QStringLiteral("Created log entry %2 in document %1 and uploaded the file.")
                   .arg(finalLogDocumentId, finalLogEntryId);
  } else {
    details << QStringLiteral("File uploaded to log document %1, entry %2.")
                   .arg(finalLogDocumentId, finalLogEntryId);
  }
  const QString originalFilename =
      payload.value(QStringLiteral("original_filename")).toString();
  if (!originalFilename.isEmpty())
    details << QStringLiteral("Attachment: %1").arg(originalFilename);
  const QString downloadPath =
      payload.value(QStringLiteral("download_path")).toString();
  if (!downloadPath.isEmpty())
    details << QStringLiteral("Download path: %1").arg(downloadPath);
  if (payload.value(QStringLiteral("append_reference")).toBool(true)) {
    const QString markdownReference =
        payload.value(QStringLiteral("markdown_reference")).toString();
    if (!markdownReference.isEmpty())
      details << QStringLiteral("Reference appended: %1")
                     .arg(markdownReference);
  }

  QMessageBox::information(parent, QStringLiteral("BELY Upload Complete"),
                           details.join(QStringLiteral("\n\n")));
  return true;
}

static void printUsage(const char *programName, FILE *stream) {
  std::fprintf(stream, "Usage: %s -upload <file>\n", programName);
}

int main(int argc, char **argv) {
  QString uploadPath;
  for (int i = 1; i < argc; ++i) {
    const QString arg = QString::fromLocal8Bit(argv[i]);
    if (arg == QStringLiteral("-h") || arg == QStringLiteral("--help")) {
      printUsage(argv[0], stdout);
      return 0;
    }
    if (arg == QStringLiteral("-upload") && i + 1 < argc) {
      uploadPath = QString::fromLocal8Bit(argv[++i]);
      continue;
    }
    printUsage(argv[0], stderr);
    return 1;
  }

  if (uploadPath.isEmpty()) {
    printUsage(argv[0], stderr);
    return 1;
  }

  QFileInfo uploadInfo(uploadPath);
  if (!uploadInfo.exists() || !uploadInfo.isFile() || !uploadInfo.isReadable()) {
    std::fprintf(stderr, "bely_upload: unable to read %s\n",
                 uploadPath.toLocal8Bit().constData());
    return 1;
  }

  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("bely_upload"));
  QApplication::setOrganizationName(QStringLiteral("APS"));

  const QString absoluteUploadPath = uploadInfo.absoluteFilePath();
  BelyUploadOptions options;
  if (!promptForBelyUploadOptions(nullptr, absoluteUploadPath, &options))
    return 2;
  return uploadFileToBely(nullptr, absoluteUploadPath, options) ? 0 : 1;
}
