#include "mpl_qt.h"
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QFile>
#include <QHBoxLayout>
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QPageLayout>
#include <QPageSize>
#include <QImageWriter>
#include <QTextStream>

static constexpr int PNG_XMAX = 656;
static constexpr int PNG_YMAX = 506;
static constexpr int MPNG_XMAX = PNG_XMAX * 100 / 80;
static constexpr int MPNG_YMAX = PNG_YMAX * 100 / 80;
static constexpr int LPNG_XMAX = MPNG_XMAX * 100 / 75;
static constexpr int LPNG_YMAX = MPNG_YMAX * 100 / 75;
static constexpr int HPNG_XMAX = LPNG_XMAX * 2;
static constexpr int HPNG_YMAX = LPNG_YMAX * 2;
static constexpr int GPNG_XMAX = HPNG_XMAX * 2;
static constexpr int GPNG_YMAX = HPNG_YMAX * 2;
static constexpr int SPNG_XMAX = PNG_XMAX * 80 / 100;
static constexpr int SPNG_YMAX = PNG_YMAX * 80 / 100;
static constexpr int kMaxMetadataLength = 1012;
static constexpr int kEpsResolutionDpi = 600;
static constexpr int kBelyUploadTimeoutMs = 120000;
static const char *kDefaultBelyUrl = "https://bely.aps.anl.gov/bely";
static const char *kDefaultBelyEntryText = "Plot uploaded from sddsplot.";
static const char *kBelySecretToolService = "mpl_qt-bely";

struct BelyUploadSize {
  const char *key;
  const char *label;
  int width;
  int height;
};

static constexpr BelyUploadSize kBelyUploadSizes[] = {
  {"spng", "Small", SPNG_XMAX, SPNG_YMAX},
  {"mpng", "Medium", MPNG_XMAX, MPNG_YMAX},
  {"lpng", "Large", LPNG_XMAX, LPNG_YMAX},
  {"hpng", "Huge", HPNG_XMAX, HPNG_YMAX},
  {"gpng", "Gigantic", GPNG_XMAX, GPNG_YMAX},
};

struct BelyUploadOptions {
  QString belyUrl;
  QString username;
  QString password;
  QString logDocumentId;
  QString logEntryId;
  QString logEntryText;
  QString attachmentName;
  QString uploadSizeKey;
  QSize uploadSize;
};

static QString findPythonForBely();

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

static QString commandLineMetadata() {
  if (sddsplotCommandline2 && sddsplotCommandline2[0]) {
    QString command = QString::fromUtf8(sddsplotCommandline2);
    if (command.size() > kMaxMetadataLength)
      command.truncate(kMaxMetadataLength);
    return command;
  }
  return QStringLiteral("mpl_qt");
}

static QString cwdMetadata() {
  return QDir::currentPath();
}

static QString defaultBelyAttachmentName() {
  return QStringLiteral("sddsplot-%1.png")
      .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
}

static int belyUploadSizeCount() {
  return static_cast<int>(sizeof(kBelyUploadSizes) / sizeof(kBelyUploadSizes[0]));
}

static const BelyUploadSize &defaultBelyUploadSize() {
  return kBelyUploadSizes[2];
}

static const BelyUploadSize &belyUploadSizeForKey(const QString &key) {
  for (int i = 0; i < belyUploadSizeCount(); ++i) {
    if (key == QString::fromUtf8(kBelyUploadSizes[i].key))
      return kBelyUploadSizes[i];
  }
  return defaultBelyUploadSize();
}

static QString belyUploadSizeText(const BelyUploadSize &uploadSize) {
  return QStringLiteral("%1 (%2 x %3)")
      .arg(QString::fromUtf8(uploadSize.label))
      .arg(uploadSize.width)
      .arg(uploadSize.height);
}

static QString belyLookupPythonScript() {
  return QString::fromUtf8(R"PY(import datetime
import json
import os
import sys


def fail(message, detail=None, code=1):
  payload = {"ok": False, "error": message}
  if detail:
    payload["detail"] = detail
  print(json.dumps(payload))
  sys.exit(code)


def debug(message):
  return


def simplify_bely_error(exc):
  text = str(exc)
  lowered = text.lower()
  if "could not verify username or password" in lowered:
    return "Could not verify username or password."
  if "401" in lowered and "unauthorized" in lowered:
    return "Could not verify username or password."
  return text


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


def describe_object(obj):
  if obj is None:
    return {"type": "None"}
  if isinstance(obj, dict):
    return {
      "type": "dict",
      "keys": sorted(list(obj.keys()))[:30],
    }
  data = {"type": type(obj).__name__}
  object_dict = getattr(obj, "__dict__", None)
  if isinstance(object_dict, dict):
    data["keys"] = sorted(list(object_dict.keys()))[:30]
  else:
    data["keys"] = sorted([name for name in dir(obj) if not name.startswith("_")])[:30]
  for name in (
    "id",
    "object_id",
    "log_document_id",
    "document_id",
    "section_id",
    "log_entry_id",
    "username",
    "user_name",
    "created_by",
    "modified_by",
    "created_date",
    "modified_date",
    "last_modified_date",
    "entered_on_date_time",
    "last_modified_on_date_time",
    "entered_by_username",
    "last_modified_by_username",
    "name",
    "object_name",
  ):
    value = get_attr(obj, name)
    if value is not None:
      data[name] = str(value)
  return data


def debug_collection(label, items, limit=3):
  debug(f"{label}: count={len(items)}")
  for index, item in enumerate(items[:limit]):
    debug(f"{label}[{index}]={json.dumps(describe_object(item), default=str, sort_keys=True)}")


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


def call_first_method(obj, method_names, *args, **kwargs):
  if obj is None:
    return None
  for name in method_names:
    if not hasattr(obj, name):
      continue
    candidate = getattr(obj, name)
    if not callable(candidate):
      continue
    try:
      return candidate(*args, **kwargs)
    except TypeError:
      continue
  return None


def parse_datetime(value):
  if value is None:
    return None
  if isinstance(value, datetime.datetime):
    return value
  if isinstance(value, datetime.date):
    return datetime.datetime.combine(value, datetime.time.min)
  if isinstance(value, (int, float)):
    try:
      return datetime.datetime.fromtimestamp(value, tz=datetime.timezone.utc)
    except Exception:
      return None
  text = str(value).strip()
  if not text:
    return None
  normalized = text.replace("Z", "+00:00")
  try:
    return datetime.datetime.fromisoformat(normalized)
  except Exception:
    for fmt in (
      "%Y-%m-%d %H:%M:%S",
      "%Y-%m-%dT%H:%M:%S",
      "%Y-%m-%d",
    ):
      try:
        return datetime.datetime.strptime(text, fmt)
      except Exception:
        pass
  return None


def numeric_key(value):
  if value is None:
    return float("-inf")
  try:
    return int(value)
  except Exception:
    try:
      text = str(value).strip()
      return int(text)
    except Exception:
      return float("-inf")


def entry_sort_key(obj):
  created_timestamp_names = (
    "entered_on_date_time",
    "created_date",
    "create_date",
    "created",
    "created_at",
  )
  modified_timestamp_names = (
    "last_modified_on_date_time",
    "last_modified_date",
    "modified_date",
    "last_modified",
    "modified",
    "updated_at",
    "timestamp",
  )
  candidates = [obj, get_attr(obj, "entry")]
  best_created_timestamp = float("-inf")
  best_modified_timestamp = float("-inf")
  for candidate in candidates:
    if candidate is None:
      continue
    for name in created_timestamp_names:
      parsed = parse_datetime(get_attr(candidate, name))
      if parsed is not None:
        best_created_timestamp = max(best_created_timestamp, parsed.timestamp())
    for name in modified_timestamp_names:
      parsed = parse_datetime(get_attr(candidate, name))
      if parsed is not None:
        best_modified_timestamp = max(best_modified_timestamp, parsed.timestamp())

  best_numeric_id = float("-inf")
  for candidate in candidates:
    if candidate is None:
      continue
    for name in ("log_entry_id", "log_id", "entry_id", "id", "object_id"):
      best_numeric_id = max(best_numeric_id, numeric_key(get_attr(candidate, name)))

  return (best_created_timestamp, best_numeric_id, best_modified_timestamp)


def time_key(obj):
  return entry_sort_key(obj)


def sort_latest(items):
  return sorted(items, key=time_key, reverse=True)


def choose_best_entry(entries, label=None):
  if not entries:
    return None
  scored_entries = []
  for entry in entries:
    score = entry_sort_key(entry)
    scored_entries.append((score, entry))

  scored_entries.sort(key=lambda item: item[0], reverse=True)
  if label:
    debug(f"{label}: count={len(scored_entries)}")
    for index, (score, entry) in enumerate(scored_entries[:5]):
      debug(
        f"{label}[{index}] id={get_attr(entry, 'log_entry_id', 'id')} "
        f"entered={get_attr(entry, 'entered_on_date_time', 'created_date')} "
        f"modified={get_attr(entry, 'last_modified_on_date_time', 'last_modified_date')} "
        f"score={score}"
      )
  return scored_entries[0][1]


def normalize_username(value):
  if value is None:
    return ""
  return str(value).strip().lower()


def iter_nested_values(obj, depth=0, max_depth=3, seen=None):
  if seen is None:
    seen = set()
  if obj is None or depth > max_depth:
    return
  object_id = id(obj)
  if object_id in seen:
    return
  seen.add(object_id)

  if isinstance(obj, (str, int, float, bool)):
    yield obj
    return

  if isinstance(obj, dict):
    for key, value in obj.items():
      yield key
      yield from iter_nested_values(value, depth + 1, max_depth, seen)
    return

  if isinstance(obj, (list, tuple, set)):
    for value in obj:
      yield from iter_nested_values(value, depth + 1, max_depth, seen)
    return

  object_dict = getattr(obj, "__dict__", None)
  if isinstance(object_dict, dict):
    for key, value in object_dict.items():
      yield key
      yield from iter_nested_values(value, depth + 1, max_depth, seen)
    return

  for name in dir(obj):
    if name.startswith("__"):
      continue
    try:
      value = getattr(obj, name)
    except Exception:
      continue
    yield name
    yield from iter_nested_values(value, depth + 1, max_depth, seen)


def iter_named_candidates(obj, depth=0, max_depth=3, seen=None):
  if seen is None:
    seen = set()
  if obj is None or depth > max_depth:
    return
  object_id = id(obj)
  if object_id in seen:
    return
  seen.add(object_id)

  if isinstance(obj, dict):
    for key, value in obj.items():
      yield key, value, depth
      if not isinstance(value, (str, int, float, bool)):
        yield from iter_named_candidates(value, depth + 1, max_depth, seen)
    return

  if isinstance(obj, (list, tuple, set)):
    for value in obj:
      if not isinstance(value, (str, int, float, bool)):
        yield from iter_named_candidates(value, depth + 1, max_depth, seen)
    return

  object_dict = getattr(obj, "__dict__", None)
  if isinstance(object_dict, dict):
    for key, value in object_dict.items():
      clean_key = key[1:] if key.startswith("_") else key
      yield clean_key, value, depth
      if not isinstance(value, (str, int, float, bool)):
        yield from iter_named_candidates(value, depth + 1, max_depth, seen)
    return

  for name in dir(obj):
    if name.startswith("__"):
      continue
    try:
      value = getattr(obj, name)
    except Exception:
      continue
    yield name, value, depth
    if not isinstance(value, (str, int, float, bool)):
      yield from iter_named_candidates(value, depth + 1, max_depth, seen)


def object_matches_username(obj, username):
  target = normalize_username(username)
  if not target:
    return False
  for name in (
    "username",
    "user_name",
    "created_by",
    "modified_by",
    "author",
    "owner",
    "user",
    "creator",
  ):
    value = get_attr(obj, name)
    if value is None:
      continue
    if normalize_username(value) == target:
      return True
    nested_value = get_attr(value, "username", "user_name", "name")
    if nested_value is not None and normalize_username(nested_value) == target:
      return True

  for value in iter_nested_values(obj):
    if normalize_username(value) == target:
      return True
  return False


def resolve_entry_id(entry, document_id=None, section_id=None):
  invalid_ids = set()
  for invalid_id in (document_id, section_id):
    if invalid_id is not None:
      invalid_ids.add(str(invalid_id))

  def valid(candidate):
    return candidate is not None and str(candidate) not in invalid_ids

  entry_id = get_attr(entry, "log_entry_id", "entry_id", "log_id")
  if valid(entry_id):
    return entry_id

  for nested_name in ("log_entry", "entry", "item", "log", "log_item"):
    nested = get_attr(entry, nested_name)
    if nested is None:
      continue
    nested_id = get_attr(
      nested,
      "log_entry_id",
      "entry_id",
      "log_id",
      "id",
      "object_id",
      "item_id",
    )
    if valid(nested_id):
      return nested_id

  for name, value, depth in iter_named_candidates(entry, max_depth=4):
    clean_name = str(name).lstrip("_").lower()
    if clean_name in ("log_entry_id", "entry_id", "log_id") and valid(value):
      return value

  for name, value, depth in iter_named_candidates(entry, max_depth=4):
    clean_name = str(name).lstrip("_").lower()
    if depth == 0:
      continue
    if clean_name in ("id", "object_id", "item_id") and valid(value):
      return value

  return None


def make_entry_reference(entry, document_id, section_id=None):
  resolved_entry_id = resolve_entry_id(entry, document_id, section_id)
  return {
    "entry": entry,
    "id": resolved_entry_id,
    "log_entry_id": resolved_entry_id,
    "log_document_id": document_id,
    "document_id": document_id,
    "object_id": document_id,
    "section_id": section_id,
    "last_modified_on_date_time": get_attr(entry, "last_modified_on_date_time", "last_modified_date", "modified_date", "last_modified", "modified"),
    "last_modified_date": get_attr(entry, "last_modified_date", "modified_date", "last_modified", "modified"),
    "entered_on_date_time": get_attr(entry, "entered_on_date_time", "created_date", "create_date", "created", "created_at"),
    "created_date": get_attr(entry, "created_date", "create_date", "created", "created_at"),
    "username": get_attr(entry, "username", "user_name", "entered_by_username"),
    "entered_by_username": get_attr(entry, "entered_by_username", "username", "user_name"),
    "created_by": get_attr(entry, "created_by", "entered_by_username", "author", "owner", "user", "creator"),
    "modified_by": get_attr(entry, "modified_by", "last_modified_by_username"),
    "last_modified_by_username": get_attr(entry, "last_modified_by_username", "modified_by"),
  }


def collect_entries_for_document(logbook_api, document_id, username=None, require_username_match=False):
  entry_results = []
  seen_entry_ids = set()

  entries = call_first_method(
    logbook_api,
    ["get_log_entries", "getLogEntries"],
    int(document_id),
  ) or []
  debug(f"document_id={document_id} top_level_entries={len(entries)}")
  for entry in entries:
    entry_id = resolve_entry_id(entry, document_id)
    if entry_id is None:
      debug(f"document_id={document_id} unresolved_top_level_entry={json.dumps(describe_object(entry), default=str, sort_keys=True)}")
      continue
    if entry_id in seen_entry_ids:
      continue
    if require_username_match and not object_matches_username(entry, username):
      continue
    entry_results.append(make_entry_reference(entry, document_id))
    seen_entry_ids.add(entry_id)

  sections = call_first_method(
    logbook_api,
    ["get_logbook_sections", "getLogbookSections", "get_sections", "getSections"],
    int(document_id),
  ) or []
  if sections:
    debug(f"document_id={document_id} sections={len(sections)}")
  for section in sections:
    section_id = get_attr(section, "id", "object_id", "section_id")
    if section_id is None:
      continue
    section_entries = call_first_method(
      logbook_api,
      ["get_log_entries", "getLogEntries"],
      int(section_id),
    ) or []
    debug(f"document_id={document_id} section_id={section_id} entries={len(section_entries)}")
    for entry in section_entries:
      entry_id = resolve_entry_id(entry, document_id, section_id)
      if entry_id is None:
        debug(f"document_id={document_id} section_id={section_id} unresolved_section_entry={json.dumps(describe_object(entry), default=str, sort_keys=True)}")
        continue
      if entry_id in seen_entry_ids:
        continue
      if require_username_match and not object_matches_username(entry, username):
        continue
      entry_results.append(make_entry_reference(entry, document_id, section_id))
      seen_entry_ids.add(entry_id)

  return sort_latest(entry_results)


def collect_latest_via_logbook_api(logbook_api, username):
  if logbook_api is None:
    return [], []

  debug("Falling back to logbook API traversal")

  logbooks = call_first_method(
    logbook_api,
    ["get_logbook_types", "getLogbookTypes", "get_logbooks", "getLogbooks"],
  ) or []
  debug_collection("logbooks", list(logbooks), limit=5)

  document_results = []
  entry_results = []
  seen_document_ids = set()
  seen_entry_ids = set()

  for logbook in logbooks:
    logbook_id = get_attr(logbook, "id", "logbook_type_id")
    if logbook_id is None:
      continue
    debug(f"Inspecting logbook_id={logbook_id}")
    documents = call_first_method(
      logbook_api,
      ["get_log_documents", "getLogDocuments", "get_logbook_documents", "getLogbookDocuments"],
      int(logbook_id),
      100,
    ) or []
    debug(f"logbook_id={logbook_id} documents={len(documents)}")
    for document in documents:
      document_id = get_attr(document, "id", "object_id", "log_document_id", "document_id")
      if document_id is None:
        continue
      if object_matches_username(document, username) and document_id not in seen_document_ids:
        document_results.append(document)
        seen_document_ids.add(document_id)

      matching_entries = collect_entries_for_document(
        logbook_api,
        document_id,
        username=username,
        require_username_match=True,
      )
      for entry in matching_entries:
        entry_id = get_attr(entry, "id", "log_entry_id")
        if entry_id is None or entry_id in seen_entry_ids:
          continue
        entry_results.append(entry)
        seen_entry_ids.add(entry_id)
        if document_id not in seen_document_ids:
          document_results.append(document)
          seen_document_ids.add(document_id)

  debug_collection("fallback_document_results", document_results)
  debug_collection("fallback_entry_results", entry_results)

  return sort_latest(document_results), sort_latest(entry_results)


try:
  from BelyApiFactory import BelyApiFactory
except Exception as exc:
  fail("Unable to import the BELY Python client. Install the bely-api package.", str(exc), 2)


required = ["BELY_URL", "BELY_USERNAME", "BELY_PASSWORD"]
for key in required:
  if not os.environ.get(key):
    fail(f"Missing required environment variable: {key}", code=3)


try:
  username = os.environ["BELY_USERNAME"]
  debug(f"Starting lookup for username={username} url={os.environ['BELY_URL']}")
  api_factory = BelyApiFactory(bely_url=os.environ["BELY_URL"])
  debug(f"Factory type={type(api_factory).__name__}")
  api_factory.authenticate_user(username, os.environ["BELY_PASSWORD"])
  debug("Authentication succeeded")
  users_api = get_api(api_factory, "get_users_api", "getUsersApi", "users_api")
  search_api = get_api(api_factory, "get_search_api", "getSearchApi", "search_api")
  logbook_api = get_api(api_factory, "get_logbook_api", "getLogbookApi", "get_lobook_api", "logbook_api")
  debug(f"Resolved APIs: users_api={type(users_api).__name__ if users_api is not None else 'None'} search_api={type(search_api).__name__ if search_api is not None else 'None'} logbook_api={type(logbook_api).__name__ if logbook_api is not None else 'None'}")
  debug(f"Search API methods={[name for name in dir(search_api) if 'search' in name.lower()][:20] if search_api is not None else []}")
  debug(f"Logbook API methods={[name for name in dir(logbook_api) if 'log' in name.lower() or 'section' in name.lower()][:30] if logbook_api is not None else []}")

  if logbook_api is None:
    fail("The installed BELY Python client is missing the required logbook API.")

  user_id = None
  if users_api is not None and hasattr(users_api, "get_user_by_username"):
    try:
      user = users_api.get_user_by_username(username)
      user_id = get_attr(user, "id")
      if user_id is not None:
        user_id = int(user_id)
      debug(f"Resolved user_id={user_id}")
    except Exception:
      debug("users_api.get_user_by_username failed; continuing without user_id filter")
      user_id = None
  else:
    debug("No compatible users API found; will match results by username fields")

  entry_results = []
  document_results = []

  if search_api is not None:
    search_kwargs = {
      "search_text": "*",
      "case_insensitive": True,
    }
    if user_id is not None:
      search_kwargs["user_id"] = [user_id]

    results = call_first_method(
      search_api,
      ["search_logbook", "searchLogbook", "search", "search_logbook_search_post"],
      **search_kwargs,
    )

    debug(f"Search attempted with kwargs={json.dumps(search_kwargs, default=str, sort_keys=True)} result_type={type(results).__name__ if results is not None else 'None'}")

    if results is not None:
      entry_results = sort_latest(list(get_attr(results, "log_entry_results") or []))
      document_results = sort_latest(list(get_attr(results, "document_results") or []))
      debug_collection("search_document_results", document_results)
      debug_collection("search_entry_results", entry_results)
  else:
    debug("No compatible search API found")

  if user_id is None:
    entry_results = [item for item in entry_results if object_matches_username(item, username)]
    document_results = [item for item in document_results if object_matches_username(item, username)]
    debug_collection("filtered_document_results", document_results)
    debug_collection("filtered_entry_results", entry_results)

  if not entry_results and not document_results:
    document_results, entry_results = collect_latest_via_logbook_api(logbook_api, username)

  chosen_document_id = None
  chosen_log_entry_id = None

  if document_results:
    document_result = document_results[0]
    chosen_document_id = get_attr(document_result, "object_id", "log_document_id", "document_id", "id")
    if chosen_document_id is not None:
      log_entries = collect_entries_for_document(
        logbook_api,
        chosen_document_id,
        username=username,
        require_username_match=(user_id is None),
      )
      debug_collection("chosen_document_matching_entries", log_entries)
      if not log_entries:
        debug("No username-matched entries found for chosen document; falling back to latest entry regardless of author")
        log_entries = collect_entries_for_document(
          logbook_api,
          chosen_document_id,
          username=username,
          require_username_match=False,
        )
        debug_collection("chosen_document_all_entries", log_entries)
      best_entry = choose_best_entry(log_entries, "chosen_document_ranked_entries")
      if best_entry is not None:
        chosen_log_entry_id = get_attr(best_entry, "log_entry_id", "id")
  elif entry_results:
    entry_result = choose_best_entry(entry_results, "global_ranked_entries")
    chosen_document_id = get_attr(entry_result, "object_id", "log_document_id", "document_id", "item_id")
    chosen_log_entry_id = get_attr(entry_result, "log_entry_id", "id")

  if chosen_document_id is None or chosen_log_entry_id is None:
    debug(f"No IDs chosen. document_results={len(document_results)} entry_results={len(entry_results)}")
    fail("No recent BELY log entry could be found for this user.")

  debug(f"Chosen IDs: log_document_id={chosen_document_id} log_entry_id={chosen_log_entry_id}")

  payload = {
    "ok": True,
    "log_document_id": str(chosen_document_id),
    "log_entry_id": str(chosen_log_entry_id),
  }
  print(json.dumps(payload))
  try:
    api_factory.logout_user()
  except Exception:
    pass
except Exception as exc:
  fail("Unable to look up the latest BELY log entry for this user.", simplify_bely_error(exc), 4)
)PY");
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

static bool lookupLatestBelyIds(QWidget *parent, const QString &pythonExecutable,
                const QString &belyUrl, const QString &username,
                const QString &password, QString *logDocumentId,
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
             QStringLiteral("Failed to start the Python lookup "
                    "helper process."));
  return false;
  }

  const bool finished = process.waitForFinished(kBelyUploadTimeoutMs);
  QApplication::restoreOverrideCursor();

  if (!finished) {
  process.kill();
  process.waitForFinished();
  QMessageBox::warning(parent, QStringLiteral("Lookup Timed Out"),
             QStringLiteral("The BELY lookup did not finish within "
                    "%1 seconds.")
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
                                       BelyUploadOptions *options) {
  if (!options)
    return false;

  QSettings settings(QStringLiteral("APS"), QStringLiteral("mpl_qt"));
  settings.beginGroup(QStringLiteral("BELY"));

  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Upload Plot to BELY"));
  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  QLabel *description = new QLabel(
      QStringLiteral("Upload the current plot as a PNG attachment to a "
             "BELY log document. Provide a log entry ID to attach "
             "to an existing entry, or leave it blank to create a "
             "new log entry first. This action requires Python "
             "with the bely-api package installed."),
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

  const QString savedUploadSizeKey =
      settings.value(QStringLiteral("uploadSize"),
                     QString::fromUtf8(defaultBelyUploadSize().key)).toString();
  const BelyUploadSize &savedUploadSize =
      belyUploadSizeForKey(savedUploadSizeKey);
  QComboBox *uploadSizeCombo = new QComboBox(&dialog);
  int savedUploadSizeIndex = 0;
  for (int i = 0; i < belyUploadSizeCount(); ++i) {
    const BelyUploadSize &uploadSize = kBelyUploadSizes[i];
    uploadSizeCombo->addItem(belyUploadSizeText(uploadSize),
                             QString::fromUtf8(uploadSize.key));
    if (QString::fromUtf8(uploadSize.key) ==
        QString::fromUtf8(savedUploadSize.key))
      savedUploadSizeIndex = i;
  }
  uploadSizeCombo->setCurrentIndex(savedUploadSizeIndex);
  form->addRow(QStringLiteral("Image size"), uploadSizeCombo);

  QString savedAttachmentName =
      settings.value(QStringLiteral("attachmentName")).toString();
  QLineEdit *attachmentNameEdit = new QLineEdit(
      savedAttachmentName.isEmpty() ? defaultBelyAttachmentName()
                                    : savedAttachmentName,
      &dialog);
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
                                          "Install Python and the bely-api package to use "
                                          "this feature."));
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
  options->uploadSizeKey = uploadSizeCombo->currentData().toString();
  const BelyUploadSize &uploadSize =
      belyUploadSizeForKey(options->uploadSizeKey);
  options->uploadSize = QSize(uploadSize.width, uploadSize.height);

  settings.setValue(QStringLiteral("url"), options->belyUrl);
  settings.setValue(QStringLiteral("username"), options->username);
  settings.setValue(QStringLiteral("logDocumentId"), options->logDocumentId);
  settings.setValue(QStringLiteral("logEntryId"), options->logEntryId);
  settings.setValue(QStringLiteral("logEntryText"), options->logEntryText);
  settings.setValue(QStringLiteral("attachmentName"), options->attachmentName);
  settings.setValue(QStringLiteral("uploadSize"), options->uploadSizeKey);

  return true;
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
               image_path, file_name, append_reference):
  token = None
  if hasattr(api_factory, "api_client") and hasattr(api_factory.api_client, "default_headers"):
    token = api_factory.api_client.default_headers.get("token")
  if not token:
    fail("BELY upload failed.", "Unable to obtain an authenticated token for direct upload fallback.")

  with open(image_path, "rb") as handle:
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

  # Prioritize the route shape that returned 405, which means the path exists.
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
  "BELY_IMAGE_PATH",
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
  image_path = os.environ["BELY_IMAGE_PATH"]
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
    debug(f"Logbook API methods={[name for name in dir(logbook_api) if 'attach' in name.lower() or 'upload' in name.lower()][:30]}")
    attachment = call_first_method(
      logbook_api,
      ["upload_attachment", "uploadAttachment"],
      log_document_id=log_document_id,
      log_id=log_entry_id,
      body=image_path,
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
      image_path,
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

static QImage buildExportImage(const QSize &exportSize) {
  if (!canvas)
    return QImage();
  QImage plotImage = exportCurrentPlotImage(exportSize);
  if (plotImage.isNull()) {
    QCoreApplication::processEvents();
    QPixmap pixmap = canvas->grab();
    plotImage = pixmap.toImage().scaled(exportSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                      .convertToFormat(QImage::Format_ARGB32);
  }
  if (surfaceGraph && surfaceContainer && surfaceContainer->isVisible()) {
    QRect containerGeom = surfaceContainer->geometry();
    double scaleX = (exportSize.width() > 0 && canvas->width() > 0)
                        ? static_cast<double>(exportSize.width()) / canvas->width()
                        : 1.0;
    double scaleY = (exportSize.height() > 0 && canvas->height() > 0)
                        ? static_cast<double>(exportSize.height()) / canvas->height()
                        : 1.0;
    QSize scaledSize(std::lround(containerGeom.width() * scaleX),
                     std::lround(containerGeom.height() * scaleY));
    QImage graphImage = surfaceGraph->renderToImage(0, scaledSize);
    if (!graphImage.isNull()) {
      QPainter painter(&plotImage);
      QPoint scaledTopLeft(std::lround(containerGeom.x() * scaleX), std::lround(containerGeom.y() * scaleY));
      painter.drawImage(QRect(scaledTopLeft, scaledSize), graphImage);
      painter.end();
    }
  }
  return plotImage;
}

static bool rewritePsHeaders(const QString &filePath, const QString &creator,
                             const QString &cwd) {
  QFile handle(filePath);
  if (!handle.open(QIODevice::ReadWrite | QIODevice::Text))
    return true;

  QString content = QString::fromUtf8(handle.readAll());
  QStringList lines = content.split('\n');
  bool creatorSet = false;
  bool cwdSet = false;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines[i].startsWith(QStringLiteral("%%Creator:"))) {
      lines[i] = QStringLiteral("%%Creator: ") + creator;
      creatorSet = true;
    } else if (lines[i].startsWith(QStringLiteral("%%CWD:"))) {
      lines[i] = QStringLiteral("%%CWD: ") + cwd;
      cwdSet = true;
    }
  }
  if (!creatorSet)
    lines.insert(1, QStringLiteral("%%Creator: ") + creator);
  if (!cwdSet)
    lines.insert(creatorSet ? 2 : 2, QStringLiteral("%%CWD: ") + cwd);

  content = lines.join(QStringLiteral("\n"));
  handle.resize(0);
  handle.seek(0);
  handle.write(content.toUtf8());
  handle.close();
  return true;
}

static bool renderPlotWithPrinter(const QString &fileName,
                                  const QString &creator,
                                  const QString &docName,
                                  QPrinter::OutputFormat format) {
  QPrinter printer;
  printer.setOutputFormat(format);
  printer.setOutputFileName(fileName);
  printer.setCreator(creator);
  printer.setDocName(docName);
  printer.setResolution(kEpsResolutionDpi);
  if (canvas) {
    QSizeF ptsSize(canvas->width(), canvas->height());
    QPageSize pageSize(ptsSize, QPageSize::Point);
    printer.setPageSize(pageSize);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Point);
  }
  printer.setFullPage(true);

  onWhite();
  QPainter painter(&printer);
  QSize target = printer.pageRect(QPrinter::DevicePixel).size().toSize();
  if (!target.isValid())
    target = QSize(LPNG_XMAX, LPNG_YMAX);
  painter.fillRect(QRect(QPoint(0, 0), target), Qt::white);
  bool painted = exportCurrentPlotToPainter(painter, target);
  if (!painted && canvas) {
    double scaleX = (target.width() > 0 && canvas->width() > 0)
                        ? static_cast<double>(target.width()) / canvas->width()
                        : 1.0;
    double scaleY = (target.height() > 0 && canvas->height() > 0)
                        ? static_cast<double>(target.height()) / canvas->height()
                        : 1.0;
    painter.save();
    painter.scale(scaleX, scaleY);
    QCoreApplication::processEvents();
    canvas->render(&painter);
    painter.restore();
  }
  if (surfaceGraph && surfaceContainer && surfaceContainer->isVisible()) {
    QRect containerGeom = surfaceContainer->geometry();
    double scaleX = (target.width() > 0 && canvas->width() > 0)
                        ? static_cast<double>(target.width()) / canvas->width()
                        : 1.0;
    double scaleY = (target.height() > 0 && canvas->height() > 0)
                        ? static_cast<double>(target.height()) / canvas->height()
                        : 1.0;
    QSize scaledSize(std::lround(containerGeom.width() * scaleX),
                     std::lround(containerGeom.height() * scaleY));
    QImage graphImage = surfaceGraph->renderToImage(0, scaledSize);
    QPoint scaledTopLeft(std::lround(containerGeom.x() * scaleX), std::lround(containerGeom.y() * scaleY));
    painter.drawImage(QRect(scaledTopLeft, scaledSize), graphImage);
  }
  painter.end();
  onBlack();
  return true;
}

static bool convertPdfToEpsWithGs(const QString &pdfFile,
                                  const QString &epsFile,
                                  const QString &creator,
                                  const QString &cwd) {
  QStringList args;
  args << "-dBATCH" << "-dNOPAUSE" << "-dSAFER"
       << "-sDEVICE=eps2write"
       << QString("-r%1").arg(kEpsResolutionDpi)
       << QString("-sOutputFile=%1").arg(epsFile)
       << pdfFile;

  QProcess gs;
  gs.start(QStringLiteral("gs"), args);
  if (!gs.waitForStarted() || !gs.waitForFinished()) {
    fprintf(stderr, "mpl_qt: ghostscript did not finish EPS conversion.\n");
    return false;
  }
  if (gs.exitStatus() != QProcess::NormalExit || gs.exitCode() != 0) {
    const QByteArray err = gs.readAllStandardError();
    if (!err.isEmpty())
      fwrite(err.constData(), 1, err.size(), stderr);
    fprintf(stderr, "mpl_qt: ghostscript EPS conversion failed.\n");
    return false;
  }

  return rewritePsHeaders(epsFile, creator, cwd);
}

static bool convertPdfToPsWithGs(const QString &pdfFile,
                                 const QString &psFile,
                                 const QString &creator,
                                 const QString &cwd) {
  QStringList args;
  args << "-dBATCH" << "-dNOPAUSE" << "-dSAFER"
       << "-sDEVICE=ps2write"
       << QString("-r%1").arg(kEpsResolutionDpi)
       << "-dLanguageLevel=2"
       << QString("-sOutputFile=%1").arg(psFile)
       << pdfFile;

  QProcess gs;
  gs.start(QStringLiteral("gs"), args);
  if (!gs.waitForStarted() || !gs.waitForFinished()) {
    fprintf(stderr, "mpl_qt: ghostscript did not finish PS conversion.\n");
    return false;
  }
  if (gs.exitStatus() != QProcess::NormalExit || gs.exitCode() != 0) {
    const QByteArray err = gs.readAllStandardError();
    if (!err.isEmpty())
      fwrite(err.constData(), 1, err.size(), stderr);
    fprintf(stderr, "mpl_qt: ghostscript PS conversion failed.\n");
    return false;
  }

  return rewritePsHeaders(psFile, creator, cwd);
}

static bool writeEps(const QString &fileName, const QImage &image,
                     const QString &creator, const QString &cwd) {
  if (image.isNull())
    return false;
  QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
  QFile epsFile(fileName);
  if (!epsFile.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  QTextStream out(&epsFile);
  const int width = rgbImage.width();
  const int height = rgbImage.height();
  out << "%!PS-Adobe-3.0 EPSF-3.0\n";
  if (!creator.isEmpty())
    out << "%%Creator: " << creator << "\n";
  if (!cwd.isEmpty())
    out << "%%CWD: " << cwd << "\n";
  out << "%%Title: " << QFileInfo(fileName).fileName() << "\n";
  out << "%%CreationDate: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
  out << "%%BoundingBox: 0 0 " << width << " " << height << "\n";
  out << "%%LanguageLevel: 2\n";
  out << "%%EndComments\n";
  out << "/pix " << (width * 3) << " string def\n";
  out << width << " " << height << " 8\n";
  out << "[" << width << " 0 0 -" << height << " 0 " << height << "]\n";
  out << "{ currentfile pix readhexstring pop } false 3 colorimage\n";

  int columnCount = 0;
  for (int y = 0; y < height; ++y) {
    const uchar *scan = rgbImage.constScanLine(y);
    for (int x = 0; x < width; ++x) {
      for (int channel = 0; channel < 3; ++channel) {
        unsigned char value = scan[x * 3 + channel];
        out << QString("%1").arg(value, 2, 16, QLatin1Char('0'));
        columnCount += 2;
        if (columnCount >= 64) {
          out << "\n";
          columnCount = 0;
        }
      }
    }
    if (columnCount) {
      out << "\n";
      columnCount = 0;
    }
  }
  out << "showpage\n%%EOF\n";
  return true;
}

void print() {
  QPrinter printer;
  QPrintDialog dialog(&printer, canvas);
  if (dialog.exec() == QDialog::Accepted) {
    onWhite();
    QPainter painter(&printer);
    QSize target = printer.pageRect(QPrinter::DevicePixel).size().toSize();
    if (!target.isValid() && canvas)
      target = canvas->size();
    if (canvas) {
      double scaleX = (target.width() > 0 && canvas->width() > 0)
                          ? static_cast<double>(target.width()) / canvas->width()
                          : 1.0;
      double scaleY = (target.height() > 0 && canvas->height() > 0)
                          ? static_cast<double>(target.height()) / canvas->height()
                          : 1.0;
      painter.save();
      painter.scale(scaleX, scaleY);
      QCoreApplication::processEvents();
      canvas->render(&painter);
      painter.restore();
      if (surfaceGraph && surfaceContainer && surfaceContainer->isVisible()) {
        QRect containerGeom = surfaceContainer->geometry();
        QSize scaledSize(std::lround(containerGeom.width() * scaleX),
                         std::lround(containerGeom.height() * scaleY));
        QImage graphImage = surfaceGraph->renderToImage(0, scaledSize);
        QPoint scaledTopLeft(std::lround(containerGeom.x() * scaleX), std::lround(containerGeom.y() * scaleY));
        painter.drawImage(QRect(scaledTopLeft, scaledSize), graphImage);
      }
    }
    painter.end();
    onBlack();
  }
}

void save() {
  QString filters = QObject::tr("PNG Files (*.png);;JPEG Files (*.jpg *.jpeg)");
  QString fileName = QFileDialog::getSaveFileName(canvas->window(),
                                                  QObject::tr("Save Image"),
                                                  "",
                                                  filters,
                                                  nullptr,
                                                  QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  QFileInfo fi(fileName);
  QString ext = fi.suffix().toLower();
  if (ext.isEmpty())
    ext = QStringLiteral("png");

  onWhite();
  QImage plotImage = buildExportImage(QSize(LPNG_XMAX, LPNG_YMAX));
  onBlack();

  if (plotImage.isNull()) {
    fprintf(stderr, "mpl_qt: failed to capture plot for saving.\n");
    return;
  }

  QString commentText = QStringLiteral("Command: %1\nCWD: %2")
                            .arg(commandLineMetadata(), cwdMetadata());
  QString format = (ext == "jpg" || ext == "jpeg") ? "JPG" : "PNG";
  QImageWriter writer(fileName, format.toUtf8());
  writer.setText(QStringLiteral("Description"), commandLineMetadata());
  writer.setText(QStringLiteral("Comment"), commentText);
  if (!writer.write(plotImage)) {
    fprintf(stderr, "mpl_qt: failed to save image %s (%s)\n",
            fileName.toLocal8Bit().constData(),
            writer.errorString().toLocal8Bit().constData());
  }
}

void savePdfOrEps() {
  QString filters = QObject::tr("PDF Files (*.pdf);;PS Files (*.ps);;EPS Files (*.eps)");
  QString fileName = QFileDialog::getSaveFileName(canvas->window(),
                                                  QObject::tr("Save as PDF, PS or EPS"),
                                                  "",
                                                  filters,
                                                  nullptr,
                                                  QFileDialog::DontUseNativeDialog);
  if (fileName.isEmpty())
    return;

  QFileInfo fi(fileName);
  QString ext = fi.suffix().toLower();
  if (ext.isEmpty())
    ext = QStringLiteral("pdf");

  QString creator = commandLineMetadata();
  if (ext == "eps") {
    QTemporaryFile pdfTemp(QStringLiteral("mpl_qt_eps_XXXXXX.pdf"));
    pdfTemp.setAutoRemove(true);
    if (!pdfTemp.open()) {
      fprintf(stderr, "mpl_qt: unable to create temporary PDF for EPS export.\n");
      return;
    }
    QString tempPdfName = pdfTemp.fileName();
    pdfTemp.close();

    if (!renderPlotWithPrinter(tempPdfName, creator, fi.fileName(), QPrinter::PdfFormat)) {
      fprintf(stderr, "mpl_qt: failed to render plot for EPS export.\n");
      return;
    }

    if (!convertPdfToEpsWithGs(tempPdfName, fileName, creator, cwdMetadata()) &&
        [&]() {
          onWhite();
          QSize base = canvas ? canvas->size() : QSize(LPNG_XMAX, LPNG_YMAX);
          QSize highRes(base.width() * 3, base.height() * 3);
          QImage fallbackImage = buildExportImage(highRes);
          onBlack();
          return writeEps(fileName, fallbackImage, creator, cwdMetadata());
        }()) {
      fprintf(stderr, "mpl_qt: unable to write EPS file %s\n",
              fileName.toLocal8Bit().constData());
    }
    return;
  }

  if (ext == "ps") {
    QTemporaryFile pdfTemp(QStringLiteral("mpl_qt_ps_XXXXXX.pdf"));
    pdfTemp.setAutoRemove(true);
    if (!pdfTemp.open()) {
      fprintf(stderr, "mpl_qt: unable to create temporary PDF for PS export.\n");
      return;
    }
    QString tempPdfName = pdfTemp.fileName();
    pdfTemp.close();

    if (!renderPlotWithPrinter(tempPdfName, creator, fi.fileName(), QPrinter::PdfFormat)) {
      fprintf(stderr, "mpl_qt: failed to render plot for PS export.\n");
      return;
    }

    if (!convertPdfToPsWithGs(tempPdfName, fileName, creator, cwdMetadata()))
      fprintf(stderr, "mpl_qt: unable to write PS file %s\n",
              fileName.toLocal8Bit().constData());
    return;
  }

  renderPlotWithPrinter(fileName, creator, fi.fileName(), QPrinter::PdfFormat);
}

void uploadToBely() {
  QWidget *parent = canvas ? canvas->window() : nullptr;
  BelyUploadOptions options;
  if (!promptForBelyUploadOptions(parent, &options))
    return;

  const QString pythonExecutable = findPythonForBely();
  if (pythonExecutable.isEmpty()) {
    QMessageBox::warning(
        parent, QStringLiteral("Python Not Found"),
        QStringLiteral("Unable to find python3 or python in PATH. "
                       "Install Python and the bely-api package to use this "
                       "feature."));
    return;
  }

  QTemporaryFile imageFile(QDir::tempPath() + QStringLiteral("/sddsplot_bely_XXXXXX.png"));
  imageFile.setAutoRemove(false);
  if (!imageFile.open()) {
    QMessageBox::warning(parent, QStringLiteral("Export Failed"),
                         QStringLiteral("Unable to create a temporary file "
                                        "for the plot image."));
    return;
  }
  const QString imagePath = imageFile.fileName();
  imageFile.close();

  onWhite();
  QImage plotImage = buildExportImage(options.uploadSize.isValid()
                                          ? options.uploadSize
                                          : QSize(LPNG_XMAX, LPNG_YMAX));
  onBlack();

  if (plotImage.isNull()) {
    QFile::remove(imagePath);
    QMessageBox::warning(parent, QStringLiteral("Export Failed"),
                         QStringLiteral("Unable to capture the current plot "
                                        "for upload."));
    return;
  }

  QImageWriter writer(imagePath, "PNG");
  writer.setText(QStringLiteral("Description"), commandLineMetadata());
  writer.setText(QStringLiteral("Comment"),
                 QStringLiteral("Command: %1\nCWD: %2")
                     .arg(commandLineMetadata(), cwdMetadata()));
  if (!writer.write(plotImage)) {
    QFile::remove(imagePath);
    QMessageBox::warning(
        parent, QStringLiteral("Export Failed"),
        QStringLiteral("Failed to write the temporary plot image: %1")
            .arg(writer.errorString()));
    return;
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
  environment.insert(QStringLiteral("BELY_IMAGE_PATH"), imagePath);
  environment.insert(QStringLiteral("BELY_FILE_NAME"), options.attachmentName);
  environment.insert(QStringLiteral("BELY_APPEND_REFERENCE"),
                     QStringLiteral("1"));
  process.setProcessEnvironment(environment);

  QApplication::setOverrideCursor(Qt::WaitCursor);
  process.start(pythonExecutable,
                QStringList() << QStringLiteral("-c")
                              << belyUploadPythonScript());
  if (!process.waitForStarted()) {
    QApplication::restoreOverrideCursor();
    QFile::remove(imagePath);
    QMessageBox::warning(parent, QStringLiteral("Upload Failed"),
                         QStringLiteral("Failed to start the Python upload "
                                        "helper process."));
    return;
  }
  const bool finished = process.waitForFinished(kBelyUploadTimeoutMs);
  QApplication::restoreOverrideCursor();

  const QByteArray stdoutData = process.readAllStandardOutput();
  const QByteArray stderrData = process.readAllStandardError();
  QFile::remove(imagePath);

  if (!finished) {
    process.kill();
    process.waitForFinished();
    QMessageBox::warning(parent, QStringLiteral("Upload Timed Out"),
                         QStringLiteral("The BELY upload did not finish within "
                                        "%1 seconds.")
                             .arg(kBelyUploadTimeoutMs / 1000));
    return;
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
    return;
  }

  storeBelyPasswordInKeyring(options.belyUrl, options.username,
                             options.password);

  QStringList details;
  const QString finalLogDocumentId =
      payload.value(QStringLiteral("log_document_id")).toString(options.logDocumentId);
  const QString finalLogEntryId =
      payload.value(QStringLiteral("log_entry_id")).toString(options.logEntryId);
  if (options.logEntryId.isEmpty()) {
    details << QStringLiteral("Created log entry %2 in document %1 and uploaded the plot.")
                   .arg(finalLogDocumentId, finalLogEntryId);
  } else {
    details << QStringLiteral("Plot uploaded to log document %1, entry %2.")
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
}
