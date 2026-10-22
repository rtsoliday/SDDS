/**
 * @file SDDS_enum.c
 * @brief Immutable SDDS6 categorical dictionaries and checked code access.
 * @copyright Copyright (c) 2026 The University of Chicago.
 * @license Software License Agreement in LICENSE.
 * @author SDDS maintainers
 */
#include "SDDS.h"
#include "SDDS_internal.h"
#include "SDDSenum.h"
#include "mdb_thread.h"
#include <ctype.h>
#include <limits.h>

#if defined(_MSC_VER)
#define ENUM_TLS __declspec(thread)
#else
#define ENUM_TLS _Thread_local
#endif
static ENUM_TLS uint64_t enumLayoutLimit = 16U * 1024U * 1024U;
static ENUM_TLS uint64_t enumMappingLimit = 1024U * 1024U;
static MDB_THREAD_LOCK enumComparisonLock = MDB_THREAD_LOCK_INITIALIZER;
static uint64_t lastEnumIdentity = 0;

/** @brief Set per-thread limits for subsequent SDDS6 reads and dictionary construction. */
int32_t SDDS_SetEnumReaderLimits(uint64_t layoutBytes, uint64_t mappingBytes) {
  if (!layoutBytes || !mappingBytes || layoutBytes >= INT32_MAX || mappingBytes > layoutBytes) {
    SDDS_SetError("Invalid SDDS6 reader limits");
    return 0;
  }
  enumLayoutLimit = layoutBytes;
  enumMappingLimit = mappingBytes;
  return 1;
}
/** @brief Return this thread's SDDS6 header command limit. */
uint64_t SDDS_GetEnumLayoutLimit(void) { return enumLayoutLimit; }
/** @brief Return this thread's decoded enum mapping limit. */
uint64_t SDDS_GetEnumMappingLimit(void) { return enumMappingLimit; }

/** @brief Parse and own a checked enum dictionary. */
SDDS_ENUM_DEFINITION *SDDS_CreateEnumDefinition(const char *values) {
  const char *error;
  SDDS_ENUM_DEFINITION *d = sddsEnumParse(values, (size_t)enumMappingLimit, &error);
  if (!d) SDDS_SetError((char *)error);
  else {
    mdb_thread_lock(&enumComparisonLock);
    /* Leave identity zero (uncached) if the process ever exhausts its IDs. */
    if (lastEnumIdentity != UINT64_MAX) {
      d->identityOwner = &lastEnumIdentity;
      d->identity = ++lastEnumIdentity;
    }
    mdb_thread_unlock(&enumComparisonLock);
  }
  return d;
}
/** @brief Release a dictionary returned by SDDS_CreateEnumDefinition. */
void SDDS_FreeEnumDefinition(SDDS_ENUM_DEFINITION *definition) { sddsEnumDestroy(definition); }
/** @brief Return borrowed canonical mapping text. */
const char *SDDS_EnumValues(const SDDS_ENUM_DEFINITION *definition) { return definition ? definition->text : NULL; }
/** @brief Return the borrowed label for a code, or NULL when unmapped. */
const char *SDDS_EnumCodeToLabel(const SDDS_ENUM_DEFINITION *definition, int32_t code) { return sddsEnumLabel(definition, code); }
/** @brief Look up a code, returning status separately from the signed result. */
int32_t SDDS_EnumLabelToCode(const SDDS_ENUM_DEFINITION *definition, const char *label, int32_t *code) { return sddsEnumCode(definition, label, code); }
/** @brief Cache exact immutable mapping comparisons without retaining peer pointers. */
int32_t SDDS_EnumDefinitionsEqual(const SDDS_ENUM_DEFINITION *a, const SDDS_ENUM_DEFINITION *b) {
  int32_t result;
  if (a == b) return 1;
  if (!a || !b) return 0;
  /* Another loaded library instance has its own IDs and lock; dictionaries
   * created there (or by the shared C++ parser) use the uncached comparison. */
  if (!a->identity || !b->identity || a->identityOwner != &lastEnumIdentity || b->identityOwner != &lastEnumIdentity)
    return sddsEnumEqual(a, b);
  mdb_thread_lock(&enumComparisonLock);
  if (a->comparisonIdentity == b->identity || b->comparisonIdentity == a->identity) {
    result = a->comparisonIdentity == b->identity ? a->comparisonResult : b->comparisonResult;
    mdb_thread_unlock(&enumComparisonLock);
    return result;
  }
  mdb_thread_unlock(&enumComparisonLock);
  result = sddsEnumEqual(a, b);
  mdb_thread_lock(&enumComparisonLock);
  /* Only cache metadata changes; dictionary entries remain immutable. The IDs
   * avoid stale hits even when a freed dictionary's address is reused. */
  ((SDDS_ENUM_DEFINITION *)a)->comparisonIdentity = b->identity;
  ((SDDS_ENUM_DEFINITION *)a)->comparisonResult = result;
  ((SDDS_ENUM_DEFINITION *)b)->comparisonIdentity = a->identity;
  ((SDDS_ENUM_DEFINITION *)b)->comparisonResult = result;
  mdb_thread_unlock(&enumComparisonLock);
  return result;
}

/** @brief Validate all codes, including caller-modified raw data. */
int32_t SDDS_ValidateEnumData(const SDDS_ENUM_DEFINITION *definition, const int32_t *codes, int64_t count) {
  int64_t i;
  if (!definition || count < 0 || (count && !codes)) {
    SDDS_SetError("Missing enum dictionary or data");
    return 0;
  }
  for (i = 0; i < count; i++)
    if (!sddsEnumLabel(definition, codes[i])) {
      char error[128];
      snprintf(error, sizeof(error), "Unmapped enum code %" PRId32 " at value %" PRId64, codes[i], i);
      SDDS_SetError(error);
      return 0;
    }
  return 1;
}

/** @brief Explicitly remap codes by exact labels, validating before modifying output. */
int32_t SDDS_RemapEnumValues(const SDDS_ENUM_DEFINITION *source, const SDDS_ENUM_DEFINITION *target, const int32_t *input, int32_t *output, int64_t count) {
  int64_t i;
  int32_t code;
  if (!source || !target || count < 0 || (count && (!input || !output))) return 0;
  for (i = 0; i < count; i++) {
    const char *label = sddsEnumLabel(source, input[i]);
    if (!label || !sddsEnumCode(target, label, &code)) {
      SDDS_SetError("Enum remapping requires every source value's label in the target mapping");
      return 0;
    }
  }
  for (i = 0; i < count; i++) {
    sddsEnumCode(target, sddsEnumLabel(source, input[i]), &code);
    output[i] = code;
  }
  return 1;
}

/**
 * @brief Parse one complete enum code (fixed values and ASCII data share this grammar).
 * @details Optional surrounding whitespace, an optional '-' and decimal digits; '+' is rejected.
 */
int32_t SDDS_ParseEnumCode(const char *text, int32_t *code) {
  char *end;
  intmax_t value;
  if (!text) return 0;
  while (isspace((unsigned char)*text)) text++;
  if (!((*text >= '0' && *text <= '9') || (*text == '-' && text[1] >= '0' && text[1] <= '9'))) return 0;
  errno = 0;
  value = strtoimax(text, &end, 10);
  while (isspace((unsigned char)*end)) end++;
  if (errno || *end || value < INT32_MIN || value > INT32_MAX) return 0;
  *code = (int32_t)value;
  return 1;
}

/** @brief Validate a complete fixed enum code before changing or writing its definition. */
int32_t SDDS_ValidateFixedEnumValue(const SDDS_ENUM_DEFINITION *definition, const char *text) {
  int32_t code;
  if (!SDDS_ParseEnumCode(text, &code) || !sddsEnumLabel(definition, code)) {
    SDDS_SetError("Invalid fixed enum parameter code");
    return 0;
  }
  return 1;
}

/** @brief Enum fields take no format; a blank format_string counts as none. */
static int32_t hasEnumFormat(const char *format) {
  return format && !SDDS_StringIsBlank((char *)format);
}

/** @brief Define an enum column without changing the primitive definition ABI. */
int32_t SDDS_DefineEnumColumn(SDDS_DATASET *dataset, const char *name, const char *symbol, const char *units, const char *description, const char *format, const char *values, int32_t fieldLength) {
  SDDS_ENUM_DEFINITION *d;
  int32_t index;
  if (!dataset || (dataset->layout.layout_written && dataset->mode && !dataset->deferSavingLayout) || dataset->page_started || dataset->n_rows_allocated || fieldLength || hasEnumFormat(format)) {
    SDDS_SetError("Enum definitions require a fresh layout, zero field_length and no format_string");
    return -1;
  }
  if (!SDDS_IsValidName(name, "column")) return -1;
  if (!(d = SDDS_CreateEnumDefinition(values))) return -1;
  SDDS_CreateRpnMemory(name, 1);
  index = SDDS_DefineColumn(dataset, name, symbol, units, description, NULL, SDDS_LONG, 0);
  if (index < 0) { sddsEnumDestroy(d); return -1; }
  dataset->layout.column_definition[index].type = SDDS_ENUM;
  dataset->layout.column_definition[index].enum_definition = d;
  dataset->layout.column_definition[index].enum_values = d->text;
  SDDS_InvalidateRpnColumnArray(dataset->layout.column_definition + index);
  if (!SDDS_SaveLayout(dataset)) return -1;
  return index;
}

/** @brief Define an enum parameter, including a checked header-resident code. */
int32_t SDDS_DefineEnumParameter(SDDS_DATASET *dataset, const char *name, const char *symbol, const char *units, const char *description, const char *format, const char *values, char *fixedValue) {
  SDDS_ENUM_DEFINITION *d;
  int32_t index;
  if (!dataset || (dataset->layout.layout_written && dataset->mode && !dataset->deferSavingLayout) || dataset->page_started || hasEnumFormat(format)) {
    SDDS_SetError("Enum parameter definitions require a fresh layout and no format_string");
    return -1;
  }
  if (!SDDS_IsValidName(name, "parameter")) return -1;
  if (!(d = SDDS_CreateEnumDefinition(values))) return -1;
  if (fixedValue && !SDDS_ValidateFixedEnumValue(d, fixedValue)) {
    sddsEnumDestroy(d);
    return -1;
  }
  SDDS_CreateRpnMemory(name, 1);
  index = SDDS_DefineParameter(dataset, name, symbol, units, description, NULL, SDDS_LONG, fixedValue);
  if (index < 0) { sddsEnumDestroy(d); return -1; }
  dataset->layout.parameter_definition[index].type = SDDS_ENUM;
  dataset->layout.parameter_definition[index].enum_definition = d;
  dataset->layout.parameter_definition[index].enum_values = d->text;
  if (!SDDS_SaveLayout(dataset)) return -1;
  return index;
}

/** @brief Define an enum array. */
int32_t SDDS_DefineEnumArray(SDDS_DATASET *dataset, const char *name, const char *symbol, const char *units, const char *description, const char *format, const char *values, int32_t fieldLength, int32_t dimensions, const char *groupName) {
  SDDS_ENUM_DEFINITION *d;
  int32_t index;
  if (!dataset || (dataset->layout.layout_written && dataset->mode && !dataset->deferSavingLayout) || dataset->page_started || hasEnumFormat(format) || fieldLength) {
    SDDS_SetError("Enum array definitions require a fresh layout, zero field_length and no format_string");
    return -1;
  }
  if (!SDDS_IsValidName(name, "array")) return -1;
  if (!(d = SDDS_CreateEnumDefinition(values))) return -1;
  index = SDDS_DefineArray(dataset, name, symbol, units, description, NULL, SDDS_LONG, 0, dimensions, groupName);
  if (index < 0) { sddsEnumDestroy(d); return -1; }
  dataset->layout.array_definition[index].type = SDDS_ENUM;
  dataset->layout.array_definition[index].enum_definition = d;
  dataset->layout.array_definition[index].enum_values = d->text;
  if (!SDDS_SaveLayout(dataset)) return -1;
  return index;
}

/** @brief Check header metadata, including attempts to modify borrowed dictionary text. */
int32_t SDDS_ValidateEnumLayout(SDDS_DATASET *dataset) {
  if (!SDDS_CheckDataset(dataset, "SDDS_ValidateEnumLayout")) return 0;
  int32_t kind, i;
  int32_t counts[3] = {dataset->layout.n_columns, dataset->layout.n_parameters, dataset->layout.n_arrays};
  for (kind = 0; kind < 3; kind++) for (i = 0; i < counts[kind]; i++) {
    int32_t type, fieldLength = 0;
    const char *text, *format;
    SDDS_ENUM_DEFINITION *definition, *parsed;
    if (kind == 0) {
      COLUMN_DEFINITION *d = dataset->layout.column_definition + i;
      type = d->type; text = d->enum_values; definition = d->enum_definition; format = d->format_string; fieldLength = d->field_length;
    } else if (kind == 1) {
      PARAMETER_DEFINITION *d = dataset->layout.parameter_definition + i;
      type = d->type; text = d->enum_values; definition = d->enum_definition; format = d->format_string;
    } else {
      ARRAY_DEFINITION *d = dataset->layout.array_definition + i;
      type = d->type; text = d->enum_values; definition = d->enum_definition; format = d->format_string; fieldLength = d->field_length;
    }
    if (type != SDDS_ENUM) {
      if (text || definition) { SDDS_SetError("Enum metadata on a primitive field"); return 0; }
      continue;
    }
    if (!definition || !text || format || fieldLength) { SDDS_SetError("Incomplete or invalid enum definition"); return 0; }
    parsed = SDDS_CreateEnumDefinition(text);
    if (!parsed) return 0;
    if (!SDDS_EnumDefinitionsEqual(definition, parsed)) {
      SDDS_FreeEnumDefinition(parsed); SDDS_SetError("Borrowed enum metadata was modified; construct a new layout"); return 0;
    }
    SDDS_FreeEnumDefinition(parsed);
    if (kind == 1 && dataset->layout.parameter_definition[i].fixed_value &&
        !SDDS_ValidateFixedEnumValue(definition, dataset->layout.parameter_definition[i].fixed_value)) return 0;
  }
  return 1;
}

/** @brief Check all input codes or the output rows selected for serialization. */
static int32_t validateEnumPage(SDDS_DATASET *dataset, int32_t inputPage) {
  if (!SDDS_CheckDataset(dataset, "SDDS_ValidateEnumPage")) return 0;
  int32_t i;
  SDDS_LAYOUT *l = &dataset->layout;
  for (i = 0; i < l->n_columns; i++) {
    const int32_t *codes;
    int64_t row;
    if (l->column_definition[i].type != SDDS_ENUM ||
        (inputPage && (l->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION))) continue;
    codes = dataset->data ? dataset->data[i] : NULL;
    if (inputPage || !dataset->row_flag || dataset->n_rows < 0) {
      if (!SDDS_ValidateEnumData(l->column_definition[i].enum_definition, codes, dataset->n_rows)) goto column;
    } else {
      /* A dictionary is required even when no rows will be written. */
      if (!SDDS_ValidateEnumData(l->column_definition[i].enum_definition, codes, 0)) goto column;
      for (row = 0; row < dataset->n_rows; row++)
        if (dataset->row_flag[row] &&
            !SDDS_ValidateEnumData(l->column_definition[i].enum_definition, codes ? codes + row : NULL, 1)) goto column;
    }
  }
  for (i = 0; i < l->n_parameters; i++) {
    PARAMETER_DEFINITION *definition = l->parameter_definition + i;
    if (definition->type == SDDS_ENUM &&
        !(inputPage && (definition->definition_mode & SDDS_WRITEONLY_DEFINITION))) {
      int32_t fixedCode;
      const int32_t *value = dataset->parameter ? dataset->parameter[i] : NULL;
      if (definition->fixed_value) {
        if (!SDDS_ParseEnumCode(definition->fixed_value, &fixedCode)) {
          SDDS_SetError("Invalid fixed enum parameter code"); return 0;
        }
        value = &fixedCode;
      }
      if (!SDDS_ValidateEnumData(definition->enum_definition, value, 1)) {
        SDDS_SetError("Invalid enum parameter in current page"); return 0;
      }
    }
  }
  for (i = 0; i < l->n_arrays; i++)
    if (l->array_definition[i].type == SDDS_ENUM &&
        !SDDS_ValidateEnumData(l->array_definition[i].enum_definition, dataset->array ? dataset->array[i].data : NULL, dataset->array ? dataset->array[i].elements : 0)) {
      SDDS_SetError("Invalid enum array in current page"); return 0;
    }
  return 1;
column:
  {
    char error[256];
    snprintf(error, sizeof(error), "Invalid enum column %.180s", l->column_definition[i].name);
    SDDS_SetError(error);
  }
  return 0;
}

/** @brief Check enums selected for output, including output-only fields. */
int32_t SDDS_ValidateEnumPage(SDDS_DATASET *dataset) { return validateEnumPage(dataset, 0); }

/** @brief Check only enum fields read from the input stream. */
int32_t SDDS_ValidateEnumReadPage(SDDS_DATASET *dataset) { return validateEnumPage(dataset, 1); }

/** @brief Reject incompatible enum mappings for elements copied by name (0 columns, 1 parameters, 2 arrays). */
int32_t SDDS_CheckEnumCopy(SDDS_DATASET *target, SDDS_DATASET *source, int32_t kind) {
  if (!SDDS_CheckDataset(target, "SDDS_CheckEnumCopy") || !SDDS_CheckDataset(source, "SDDS_CheckEnumCopy") || kind < 0 || kind > 2) return 0;
  int32_t i, j, count = kind == 0 ? source->layout.n_columns : kind == 1 ? source->layout.n_parameters : source->layout.n_arrays;
  for (i = 0; i < count; i++) {
    int32_t a, b;
    const char *name;
    SDDS_ENUM_DEFINITION *da, *db;
    if (kind == 0) {
      name = source->layout.column_definition[i].name;
      if ((j = SDDS_GetColumnIndex(target, (char *)name)) < 0) continue;
      a = source->layout.column_definition[i].type; b = target->layout.column_definition[j].type;
      da = source->layout.column_definition[i].enum_definition; db = target->layout.column_definition[j].enum_definition;
    } else if (kind == 1) {
      name = source->layout.parameter_definition[i].name;
      if ((j = SDDS_GetParameterIndex(target, (char *)name)) < 0) continue;
      a = source->layout.parameter_definition[i].type; b = target->layout.parameter_definition[j].type;
      da = source->layout.parameter_definition[i].enum_definition; db = target->layout.parameter_definition[j].enum_definition;
    } else {
      name = source->layout.array_definition[i].name;
      if ((j = SDDS_GetArrayIndex(target, (char *)name)) < 0) continue;
      a = source->layout.array_definition[i].type; b = target->layout.array_definition[j].type;
      da = source->layout.array_definition[i].enum_definition; db = target->layout.array_definition[j].enum_definition;
    }
    if ((a == SDDS_ENUM || b == SDDS_ENUM) && (a != b || !da || !db || !SDDS_EnumDefinitionsEqual(da, db))) {
      char error[256];
      snprintf(error, sizeof(error), "Incompatible enum mapping for %.180s; explicit conversion/remapping required", name);
      SDDS_SetError(error); return 0;
    }
  }
  return 1;
}

/** @brief Read a byte from one of the supported buffered header streams. */
static int enumHeaderByte(void *stream, int32_t compression) {
  unsigned char c;
  if (compression == 2) return lzma_read((struct lzmafile *)stream, &c, 1) == 1 ? c : EOF;
#if defined(zLib)
  if (compression == 1) return gzgetc((gzFile)stream);
#endif
  return fgetc((FILE *)stream);
}

/** @brief Read a bounded SDDS6 namelist, preserving quoted delimiters and escape parity. */
int32_t SDDS6_GetNamelist(SDDS_DATASET *dataset, void *stream, int32_t compression) {
  SDDS_LAYOUT *layout = &dataset->layout;
  size_t used = 0;
  int c, quoted = 0, escaped = 0, started = 0, comment = 0;
  char commentText[256];
  size_t commentUsed = 0;
  while ((c = enumHeaderByte(stream, compression)) != EOF) {
    if (!c) { SDDS_SetError("NUL in SDDS6 header"); return 0; }
    if (comment) {
      if (c == '\n') {
        commentText[commentUsed] = 0;
        if (commentText[0] == '#') SDDS_ParseSpecialComments(dataset, commentText + 1);
        commentUsed = 0; comment = 0;
        /* Inside a command the newline ending a comment still separates tokens. */
        if (!started) continue;
      } else {
        if (commentUsed + 1 < sizeof(commentText)) commentText[commentUsed++] = (char)c;
        continue;
      }
    }
    if (!quoted && c == '!') { comment = 1; continue; }
    if (!started) {
      if (c != '&') continue;
      started = 1;
    }
    if (used + 1 >= enumLayoutLimit) { SDDS_SetError("SDDS6 layout command exceeds configured byte limit"); return 0; }
    if (used + 2 > (size_t)layout->s_capacity) {
      size_t capacity = (size_t)layout->s_capacity * 2;
      char *buffer;
      if (capacity > enumLayoutLimit) capacity = (size_t)enumLayoutLimit;
      buffer = (char *)realloc(layout->s, capacity);
      if (!buffer) { SDDS_SetError("Unable to grow SDDS6 header buffer"); return 0; }
      layout->s = buffer; layout->s_capacity = (int32_t)capacity;
    }
    layout->s[used++] = (char)((c == '\n' || c == '\r') && !quoted ? ' ' : c);
    layout->s[used] = 0;
    if (c == '"' && !escaped) quoted = !quoted;
    if (!quoted && used >= 4 && !memcmp(layout->s + used - 4, "&end", 4)) {
      /* Consume the rest of the header line without consuming binary page bytes. */
      int trailingComment = 0;
      while ((c = enumHeaderByte(stream, compression)) != EOF && c != '\n') {
        if (!c) { SDDS_SetError("NUL after SDDS6 namelist"); return 0; }
        if (c == '!') trailingComment = 1;
        if (trailingComment) continue;
        if (c != ' ' && c != '\t' && c != '\r' && c != ',') {
          SDDS_SetError("Unexpected text after SDDS6 &end"); return 0;
        }
      }
      layout->s[used - 4] = 0;
      return 1;
    }
    if (c == '\\' && !escaped) escaped = 1; else escaped = 0;
  }
  if (started) SDDS_SetError("Unterminated SDDS6 namelist");
  return 0;
}

/** @brief Escape one logical mapping for the outer namelist string. */
char *SDDS6_QuoteEnumValues(const char *text) {
  size_t n = strlen(text), i, used = 0;
  char *out = (char *)malloc(4 * n + 1);
  if (!out) return NULL;
  for (i = 0; i < n; i++) {
    if (text[i] == '\\' || text[i] == '"' || text[i] == '!') out[used++] = '\\';
    if ((unsigned char)text[i] < 32 || (unsigned char)text[i] == 127) {
      snprintf(out + used, 5, "\\%03o", (unsigned char)text[i]);
      used += 4;
    } else out[used++] = text[i];
  }
  out[used] = 0;
  return out;
}

/** @brief Serialize one SDDS6 command without fixed-size staging buffers. */
char *SDDS6_CreateNamelist(const char *command, const void *data, SDDS_FIELD_INFORMATION *fields, int32_t count) {
  char *pieces[16] = {0}, *result = NULL, *out;
  size_t total = strlen(command) + 10;
  int32_t i;
  if (count > 16) return NULL;
  for (i = 0; i < count; i++) {
    const char *value = NULL;
    char number[32], *escaped = NULL;
    int string = fields[i].type == SDDS_STRING;
    if (string) {
      value = *(char *const *)((const char *)data + fields[i].offset);
      if (!value) continue;
      /* Like the legacy writer, omit a blank format_string; readers reject it as a format. */
      if (!strcmp(fields[i].name, "format_string") && SDDS_StringIsBlank((char *)value)) continue;
      escaped = SDDS6_QuoteEnumValues(value);
      if (!escaped) goto done;
      value = escaped;
    } else {
      int32_t code = fields[i].type == SDDS_SHORT ? *(const short *)((const char *)data + fields[i].offset) :
        *(const int32_t *)((const char *)data + fields[i].offset);
      if (fields[i].enumPair) {
        SDDS_ENUM_PAIR *pair = fields[i].enumPair;
        while (pair->name && pair->value != code) pair++;
        value = pair->name;
        if (!value) {
          if (!strcmp(fields[i].name, "endian") && !code) continue;
          goto done;
        }
      } else { snprintf(number, sizeof(number), "%" PRId32, code); value = number; }
    }
    {
      size_t size = strlen(fields[i].name) + strlen(value) + 8;
      if (size > enumLayoutLimit || total > enumLayoutLimit - size) { free(escaped); goto done; }
      pieces[i] = malloc(size);
      if (!pieces[i]) { free(escaped); goto done; }
      snprintf(pieces[i], size, string ? "%s=\"%s\", " : "%s=%s, ", fields[i].name, value);
      total += strlen(pieces[i]);
    }
    free(escaped);
  }
  result = malloc(total);
  if (!result) goto done;
  out = result;
  out += sprintf(out, "&%s ", command);
  for (i = 0; i < count; i++) if (pieces[i]) { size_t n = strlen(pieces[i]); memcpy(out, pieces[i], n); out += n; }
  strcpy(out, " &end\n");
done:
  for (i = 0; i < count; i++) free(pieces[i]);
  if (!result) SDDS_SetError("Unable to serialize bounded SDDS6 namelist");
  return result;
}

/** @brief Write a shared SDDS6 serialization to any supported stream. */
int32_t SDDS6_WriteNamelist(void *stream, int32_t compression, const char *command, const void *data, SDDS_FIELD_INFORMATION *fields, int32_t count) {
  char *text = SDDS6_CreateNamelist(command, data, fields, count);
  int ok;
  if (!text || !stream) { free(text); return 0; }
  if (compression == 2) ok = lzma_puts(text, (struct lzmafile *)stream) >= 0;
#if defined(zLib)
  else if (compression == 1) ok = gzputs((gzFile)stream, text) >= 0;
#endif
  else ok = fputs(text, (FILE *)stream) >= 0;
  free(text);
  return ok;
}

/** @brief Parse SDDS6 fields with a separate outer escaping layer. */
int32_t SDDS6_ParseNamelist(void *data, SDDS_FIELD_INFORMATION *fields, int32_t count, char *text) {
  char *p = text;
  uint64_t seen = 0;
  while (*p) {
    char *key, *value, *out;
    int32_t index;
    int quoted;
    while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (!*p || !strncmp(p, "&end", 4)) break;
    key = p;
    while (*p && *p != '=' && *p != ' ' && *p != '\t') p++;
    if (!*p) goto invalid;
    if (*p == '=') *p++ = 0;
    else {
      *p++ = 0;
      while (*p == ' ' || *p == '\t') p++;
      if (*p++ != '=') goto invalid;
    }
    while (*p == ' ' || *p == '\t') p++;
    quoted = *p == '"';
    if (quoted) p++;
    value = out = p;
    while (*p) {
      unsigned char c = (unsigned char)*p++;
      if (quoted && c == '"') break;
      if (!quoted && (c == ',' || c == ' ' || c == '\t' || c == '\r' || c == '\n')) break;
      if (c == '\\') {
        c = (unsigned char)*p++;
        if (!c) goto invalid;
        if (c == 'n') c = '\n';
        else if (c == 'r') c = '\r';
        else if (c == 't') c = '\t';
        else if (c >= '0' && c <= '7') {
          unsigned int number = c - '0', digits = 1;
          while (digits < 3 && *p >= '0' && *p <= '7') { number = 8 * number + (unsigned int)(*p++ - '0'); digits++; }
          if (!number || number > 255) goto invalid;
          c = (unsigned char)number;
        } else if (c != '\\' && c != '"' && c != '!') goto invalid;
      }
      *out++ = (char)c;
    }
    if (quoted && p[-1] != '"') goto invalid;
    *out = 0;
    index = SDDS_FindFieldIndex(key, fields, count);
    if (index < 0 || index >= 64 || (seen & (UINT64_C(1) << index))) goto invalid;
    seen |= UINT64_C(1) << index;
    if (fields[index].type == SDDS_STRING) {
      char **slot = (char **)((char *)data + fields[index].offset);
      if (*slot || !SDDS_CopyString(slot, value)) goto invalid;
    } else {
      intmax_t number;
      char *end;
      if (fields[index].enumPair) {
        number = SDDS_MatchEnum(value, fields[index].enumPair);
        if (number < 0) goto invalid;
      } else {
        errno = 0;
        number = strtoimax(value, &end, 10);
        if (errno || end == value || *end) goto invalid;
      }
      if (fields[index].type == SDDS_LONG && number >= INT32_MIN && number <= INT32_MAX)
        *(int32_t *)((char *)data + fields[index].offset) = (int32_t)number;
      else if (fields[index].type == SDDS_SHORT && number >= SHRT_MIN && number <= SHRT_MAX)
        *(short *)((char *)data + fields[index].offset) = (short)number;
      else goto invalid;
    }
  }
  return 1;
invalid:
  SDDS_SetError("Invalid or duplicate SDDS6 namelist field/escape");
  return 0;
}

/** @brief Return dictionary cardinality in declaration order. */
int32_t SDDS_EnumCount(const SDDS_ENUM_DEFINITION *definition) { return definition ? (int32_t)definition->count : 0; }
/** @brief Inspect a declared pair; label lifetime is the dictionary lifetime. */
int32_t SDDS_EnumEntry(const SDDS_ENUM_DEFINITION *definition, int32_t index, int32_t *code, const char **label) {
  if (!definition || index < 0 || (size_t)index >= definition->count || !code || !label) return 0;
  *code = definition->entries[index].code; *label = definition->entries[index].label;
  return 1;
}

/** @brief Format a label (or explicit numeric code) with bounds and categorical checks. */
int32_t SDDS_SprintEnumValue(const SDDS_ENUM_DEFINITION *definition, const int32_t *codes, int64_t index, const char *format, char *buffer, size_t bufferSize, uint32_t mode, double factor, int32_t showCodes) {
  const char *label;
  char number[32];
  int result;
  if (!codes || index < 0 || !buffer || !bufferSize || factor != 1 || !(label = sddsEnumLabel(definition, codes[index]))) {
    SDDS_SetError("Enum formatting requires a mapped code and no numeric scaling"); return 0;
  }
  if (showCodes) { snprintf(number, sizeof(number), "%" PRId32, codes[index]); label = number; }
  if (format && !SDDS_VerifyStringFormat(format)) {
    SDDS_SetError("Enum display format must be a string format"); return 0;
  }
  if (!showCodes && !(mode & SDDS_PRINT_NOQUOTES) &&
      (SDDS_StringIsBlank((char *)label) || strchr(label, '"') || SDDS_HasWhitespace((char *)label))) {
    /* Quote labels exactly as SDDS_SprintTypedValue() quotes string values; codes are never quoted. */
    size_t length = strlen(label);
    const char *s;
    char *quoted, *p;
    if (length > (SIZE_MAX - 3) / 2 || !(quoted = malloc(2 * length + 3))) {
      SDDS_SetError("Unable to allocate enum display buffer"); return 0;
    }
    p = quoted + 1;
    for (s = label; *s; s++) {
      if (*s == '"' && (s == label || s[-1] != '\\')) *p++ = '\\';
      *p++ = *s;
    }
    *p = 0;
    if (SDDS_StringIsBlank((char *)label)) strcpy(quoted, "\"\"");
    else if (SDDS_HasWhitespace(quoted + 1)) { quoted[0] = '"'; *p++ = '"'; *p = 0; }
    else memmove(quoted, quoted + 1, (size_t)(p - quoted));
    result = snprintf(buffer, bufferSize, format ? format : "%s", quoted);
    free(quoted);
  } else result = snprintf(buffer, bufferSize, format ? format : "%s", label);
  if (result < 0 || (size_t)result >= bufferSize) { SDDS_SetError("Enum display buffer too small"); return 0; }
  return 1;
}

/**
 * @brief Escape a label as a legacy string fixed value.
 * @details Only bytes that the legacy header reader or SDDS_InterpretEscapes() would alter are
 * written as octal escapes: backslashes, quotes, comment/namelist delimiters, control bytes,
 * and leading or trailing blanks.  Other bytes, including UTF-8 sequences, are copied unchanged.
 */
static char *escapeLegacyFixedString(const char *label) {
  size_t i, length = strlen(label), first = 0, last = length;
  char *output, *p;
  while (first < length && label[first] == ' ') first++;
  while (last > first && label[last - 1] == ' ') last--;
  if (length > (SIZE_MAX - 1) / 4 || !(output = malloc(4 * length + 1))) return NULL;
  for (i = 0, p = output; i < length; i++) {
    unsigned char c = (unsigned char)label[i];
    /* A backslash is octal too: a trailing "\\" would escape the closing quote for legacy readers. */
    if (c < 0x20 || c == 0x7f || c == '\\' || c == '"' || c == '!' || c == '&' || (c == ' ' && (i < first || i >= last))) {
      snprintf(p, 5, "\\%03o", c); p += 4;
    } else *p++ = (char)c;
  }
  *p = 0;
  return output;
}

/** @brief Length of one field as written by SDDS_PrintNamelistField() (0 when omitted). */
static size_t legacyNamelistFieldLength(const char *name, const char *value) {
  size_t length, quotes = 0;
  const char *s;
  if (!value) return 0;
  if (!*value) return strlen(name) + 5;
  for (s = value; *s; s++) if (*s == '"') quotes++;
  length = strlen(name) + 1 + strlen(value) + quotes + 2;
  return strpbrk(value, " ,*$\t\n\b") ? length + 2 : length;
}

/** @brief Whether a legacy (SDDS1-5) reader can read this string parameter definition. */
static int32_t legacyParameterHeaderFits(const PARAMETER_DEFINITION *d, const char *fixed) {
  static const char *names[3] = {"symbol", "units", "description"};
  const char *values[3];
  size_t length = strlen("&parameter ") + strlen("&end\n");
  int i;
  values[0] = d->symbol; values[1] = d->units; values[2] = d->description;
  length += legacyNamelistFieldLength("name", d->name);
  for (i = 0; i < 3; i++)
    if (values[i] && !SDDS_StringIsBlank((char *)values[i])) length += legacyNamelistFieldLength(names[i], values[i]);
  length += legacyNamelistFieldLength("type", "string");
  length += legacyNamelistFieldLength("fixed_value", fixed);
  /* The legacy reader accepts a definition line of up to SDDS_MAXLINE bytes including its newline. */
  return length <= SDDS_MAXLINE;
}

/** @brief Configure an explicit conversion in a newly constructed output layout only. */
int32_t SDDS_ConfigureEnumConversion(SDDS_DATASET *target, int32_t kind, int32_t index, int32_t targetType, const char *mapping) {
  int32_t *type, sourceType;
  SDDS_ENUM_DEFINITION **definition, *parsed = NULL, *old;
  char **text, **fixed = NULL, *newFixed = NULL, *oldFixed = NULL, *oldFormat, *decodedFixed = NULL;
  if (target && target->deferSavingLayout) {
    SDDS_SetError("Enum conversion requires layout saving enabled; finish deferred layout changes first");
    return 0;
  }
  /* SDDS_InitializeCopy labels stdout destinations as memory-mode datasets. */
  if (!target ||
      (target->mode != SDDS_WRITEMODE && !(target->mode == SDDS_MEMMODE && target->layout.fp == stdout)) ||
      target->layout.layout_written || target->page_started || target->n_rows_allocated || index < 0) goto invalid;
  if (kind == 0 && index < target->layout.n_columns) {
    COLUMN_DEFINITION *d = target->layout.column_definition + index;
    type = &d->type; definition = &d->enum_definition; text = &d->enum_values;
  } else if (kind == 1 && index < target->layout.n_parameters) {
    PARAMETER_DEFINITION *d = target->layout.parameter_definition + index;
    type = &d->type; definition = &d->enum_definition; text = &d->enum_values; fixed = &d->fixed_value;
  } else if (kind == 2 && index < target->layout.n_arrays) {
    ARRAY_DEFINITION *d = target->layout.array_definition + index;
    type = &d->type; definition = &d->enum_definition; text = &d->enum_values;
  } else goto invalid;
  sourceType = *type;
  if (!((sourceType == SDDS_ENUM && (targetType == SDDS_ENUM || targetType == SDDS_LONG || targetType == SDDS_STRING)) ||
        (targetType == SDDS_ENUM && (sourceType == SDDS_LONG || sourceType == SDDS_STRING)))) goto invalid;
  if (targetType == SDDS_ENUM && !(parsed = SDDS_CreateEnumDefinition(mapping))) return 0;
  if (fixed && *fixed) {
    int32_t code = 0;
    char number[32];
    const char *label = *fixed;
    if (sourceType == SDDS_STRING) {
      char *scan = NULL;
      if (!SDDS_CopyString(&scan, *fixed)) goto fixedError;
      if (!SDDS_ScanData(scan, SDDS_STRING, 0, &decodedFixed, 0, 1)) { free(scan); goto fixedError; }
      free(scan);
      label = decodedFixed;
    } else if (sourceType == SDDS_ENUM) {
      if (!SDDS_ParseEnumCode(*fixed, &code) || !(label = sddsEnumLabel(*definition, code))) goto fixedError;
    } else {
      char *end;
      intmax_t value;
      errno = 0; value = strtoimax(*fixed, &end, 10);
      if (errno || end == *fixed || value < INT32_MIN || value > INT32_MAX) goto fixedError;
      while (isspace((unsigned char)*end)) end++;
      if (*end) goto fixedError;
      code = (int32_t)value;
    }
    if (targetType == SDDS_ENUM) {
      if (sourceType != SDDS_LONG && !sddsEnumCode(parsed, label, &code)) goto fixedError;
      if (!sddsEnumLabel(parsed, code)) goto fixedError;
    }
    if (targetType != SDDS_STRING) { snprintf(number, sizeof(number), "%" PRId32, code); label = number; }
    if (targetType == SDDS_STRING) {
      if (!(newFixed = escapeLegacyFixedString(label))) goto fixedError;
      if (!legacyParameterHeaderFits(target->layout.parameter_definition + index, newFixed)) {
        SDDS_FreeEnumDefinition(parsed); free(newFixed); free(decodedFixed);
        SDDS_SetError("Fixed enum label is too long for a string fixed value in an SDDS1-5 header; make the parameter page data first (SDDS_ConfigureEnumConversion)");
        return 0;
      }
    } else if (!SDDS_CopyString(&newFixed, label)) goto fixedError;
    free(decodedFixed); decodedFixed = NULL;
  }
  old = *definition;
  if (fixed) { oldFixed = *fixed; *fixed = newFixed; }
  *type = targetType; *definition = parsed; *text = parsed ? parsed->text : NULL;
  if (kind == 0 && targetType == SDDS_ENUM)
    SDDS_InvalidateRpnColumnArray(target->layout.column_definition + index);
  oldFormat = kind == 0 ? target->layout.column_definition[index].format_string : kind == 1 ? target->layout.parameter_definition[index].format_string : target->layout.array_definition[index].format_string;
  if (kind == 0) { target->layout.column_definition[index].format_string = NULL; target->layout.column_definition[index].field_length = 0; }
  if (kind == 1) target->layout.parameter_definition[index].format_string = NULL;
  if (kind == 2) { target->layout.array_definition[index].format_string = NULL; target->layout.array_definition[index].field_length = 0; }
  if (!SDDS_SaveLayout(target)) return 0;
  SDDS_FreeEnumDefinition(old); free(oldFixed); free(oldFormat);
  return 1;
fixedError:
  SDDS_FreeEnumDefinition(parsed); free(newFixed); free(decodedFixed);
invalid:
  SDDS_SetError("Invalid enum conversion: require a fresh output layout, enum/long/string types and a target dictionary");
  return 0;
}

/** @brief Convert a contiguous field, retaining legacy checked/numeric copy behavior. */
static void *convertEnumField(int32_t sourceType, const SDDS_ENUM_DEFINITION *sourceMap, int32_t targetType, const SDDS_ENUM_DEFINITION *targetMap, const void *input, int64_t count) {
  void *output;
  int64_t i;
  if (count < 0 || (count && !input) || (uint64_t)count > SIZE_MAX / SDDS_type_size[targetType - 1]) return NULL;
  output = calloc(count ? (size_t)count : 1, SDDS_type_size[targetType - 1]);
  if (!output) return NULL;
  for (i = 0; i < count; i++) {
    if (sourceType == SDDS_ENUM || targetType == SDDS_ENUM) {
      int32_t code = 0;
      const char *label = NULL;
      if (sourceType == SDDS_ENUM || sourceType == SDDS_LONG) code = ((const int32_t *)input)[i];
      if (sourceType == SDDS_ENUM) label = sddsEnumLabel(sourceMap, code);
      else if (sourceType == SDDS_STRING) label = ((char *const *)input)[i];
      if (sourceType == SDDS_ENUM && !label) goto failure;
      if (targetType == SDDS_ENUM) {
        if (sourceType != SDDS_LONG && !sddsEnumCode(targetMap, label, &code)) goto failure;
        if (!sddsEnumLabel(targetMap, code)) goto failure;
        ((int32_t *)output)[i] = code;
      } else if (targetType == SDDS_LONG) ((int32_t *)output)[i] = code;
      else if (targetType == SDDS_STRING && label) {
        if (!SDDS_CopyString((char **)output + i, label)) goto failure;
      } else goto failure;
    } else if (sourceType == SDDS_STRING && targetType == SDDS_STRING) {
      if (!SDDS_CopyString((char **)output + i, ((char *const *)input)[i])) goto failure;
    } else if (!SDDS_CastValue((void *)input, i, sourceType, targetType, (char *)output + i * SDDS_type_size[targetType - 1])) goto failure;
  }
  return output;
failure:
  if (targetType == SDDS_STRING) for (i = 0; i < count; i++) free(((char **)output)[i]);
  free(output); SDDS_SetError("Explicit enum conversion failed: incompatible type or unmapped label/code"); return NULL;
}

/** @brief Explicitly copy/convert a page; ordinary copy APIs remain dictionary-strict. */
int32_t SDDS_CopyPageWithEnumConversions(SDDS_DATASET *target, SDDS_DATASET *source) {
  int kind, i;
  if (!SDDS_StartPage(target, target->layout.n_columns ? source->n_rows : 0)) return 0;
  for (kind = 0; kind < 3; kind++) {
    int count = kind == 0 ? target->layout.n_columns : kind == 1 ? target->layout.n_parameters : target->layout.n_arrays;
    for (i = 0; i < count; i++) {
      const void *input;
      SDDS_ENUM_DEFINITION *sourceMap, *targetMap;
      int32_t sourceType, targetType, j;
      int64_t elements;
      char *name;
      void *output;
      int ok;
      if (kind == 0) {
        COLUMN_DEFINITION *d = target->layout.column_definition + i, *s;
        name = d->name; j = SDDS_GetColumnIndex(source, name); if (j < 0) return 0;
        s = source->layout.column_definition + j;
        sourceType = s->type; targetType = d->type; sourceMap = s->enum_definition; targetMap = d->enum_definition;
        elements = source->n_rows; input = source->data[j];
      } else if (kind == 1) {
        PARAMETER_DEFINITION *d = target->layout.parameter_definition + i, *s;
        if (d->fixed_value) continue;
        name = d->name; j = SDDS_GetParameterIndex(source, name); if (j < 0) return 0;
        s = source->layout.parameter_definition + j;
        sourceType = s->type; targetType = d->type; sourceMap = s->enum_definition; targetMap = d->enum_definition;
        elements = 1; input = source->parameter[j];
      } else {
        ARRAY_DEFINITION *d = target->layout.array_definition + i, *s;
        name = d->name; j = SDDS_GetArrayIndex(source, name); if (j < 0) return 0;
        s = source->layout.array_definition + j;
        sourceType = s->type; targetType = d->type; sourceMap = s->enum_definition; targetMap = d->enum_definition;
        elements = source->array[j].elements; input = source->array[j].data;
      }
      output = convertEnumField(sourceType, sourceMap, targetType, targetMap, input, elements);
      if (!output) return 0;
      if (kind == 0) ok = SDDS_SetColumn(target, SDDS_SET_BY_INDEX, output, elements, i);
      else if (kind == 1) ok = SDDS_SetParameter(target, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, i, output);
      else ok = SDDS_SetArray(target, name, SDDS_CONTIGUOUS_DATA, output, source->array[j].dimension);
      if (targetType == SDDS_STRING) { int64_t k; for (k = 0; k < elements; k++) free(((char **)output)[k]); }
      free(output);
      if (!ok) return 0;
    }
  }
  return 1;
}

/** @brief Reject implicit dictionary loss in formats with no native enum representation. */
int32_t SDDS_CheckEnumExport(SDDS_DATASET *dataset) {
  int32_t i;
  if (!SDDS_CheckDataset(dataset, "SDDS_CheckEnumExport")) return 0;
  for (i = 0; i < dataset->layout.n_columns; i++)
    if (dataset->layout.column_definition[i].type == SDDS_ENUM) goto unsupported;
  for (i = 0; i < dataset->layout.n_parameters; i++)
    if (dataset->layout.parameter_definition[i].type == SDDS_ENUM) goto unsupported;
  for (i = 0; i < dataset->layout.n_arrays; i++)
    if (dataset->layout.array_definition[i].type == SDDS_ENUM) goto unsupported;
  return 1;
unsupported:
  SDDS_SetError("This export format cannot preserve enums; use sddscast to convert explicitly to long codes or string labels first");
  return 0;
}

/** @brief Check dictionary compatibility for a selected field, including renamed copies. */
int32_t SDDS_CheckEnumFieldCompatibility(SDDS_DATASET *target, SDDS_DATASET *source, int32_t kind, int32_t targetIndex, int32_t sourceIndex) {
  int32_t a, b;
  const char *targetName, *sourceName;
  char error[256];
  SDDS_ENUM_DEFINITION *ad, *bd;
  if (!target || !source || targetIndex < 0 || sourceIndex < 0) return 0;
  if (kind == 0 && targetIndex < target->layout.n_columns && sourceIndex < source->layout.n_columns) {
    targetName = target->layout.column_definition[targetIndex].name; sourceName = source->layout.column_definition[sourceIndex].name;
    a = target->layout.column_definition[targetIndex].type; ad = target->layout.column_definition[targetIndex].enum_definition;
    b = source->layout.column_definition[sourceIndex].type; bd = source->layout.column_definition[sourceIndex].enum_definition;
  } else if (kind == 1 && targetIndex < target->layout.n_parameters && sourceIndex < source->layout.n_parameters) {
    targetName = target->layout.parameter_definition[targetIndex].name; sourceName = source->layout.parameter_definition[sourceIndex].name;
    a = target->layout.parameter_definition[targetIndex].type; ad = target->layout.parameter_definition[targetIndex].enum_definition;
    b = source->layout.parameter_definition[sourceIndex].type; bd = source->layout.parameter_definition[sourceIndex].enum_definition;
  } else if (kind == 2 && targetIndex < target->layout.n_arrays && sourceIndex < source->layout.n_arrays) {
    targetName = target->layout.array_definition[targetIndex].name; sourceName = source->layout.array_definition[sourceIndex].name;
    a = target->layout.array_definition[targetIndex].type; ad = target->layout.array_definition[targetIndex].enum_definition;
    b = source->layout.array_definition[sourceIndex].type; bd = source->layout.array_definition[sourceIndex].enum_definition;
  } else return 0;
  if (a != SDDS_ENUM && b != SDDS_ENUM) return 1;
  if (a == b && ad && bd && SDDS_EnumDefinitionsEqual(ad, bd)) return 1;
  snprintf(error, sizeof(error), "Incompatible enum mappings for %.80s -> %.80s; use explicit remapping", sourceName, targetName);
  SDDS_SetError(error);
  return 0;
}

/** @brief Scan fixed header values without a legacy fixed-size staging buffer. */
int32_t SDDS_ScanFixedValue(const char *text, int32_t type, void *data) {
  char *copy = NULL;
  int32_t ok;
  if (!text || !SDDS_CopyString(&copy, text)) return 0;
  ok = SDDS_ScanData(copy, type, 0, data, 0, 1);
  free(copy);
  return ok;
}

/** @brief Get a dictionary slot without assuming identical definition structures. */
static SDDS_ENUM_DEFINITION **enumSlot(SDDS_LAYOUT *layout, int kind, int32_t index) {
  if (kind == 0) return &layout->column_definition[index].enum_definition;
  if (kind == 1) return &layout->parameter_definition[index].enum_definition;
  return &layout->array_definition[index].enum_definition;
}

/** @brief Release dictionaries created before a failed header could save its layout. */
void SDDS_FreeUnownedEnums(SDDS_DATASET *dataset) {
  int kind;
  if (!dataset) return;
  for (kind = 0; kind < 3; kind++) {
    int32_t i, j;
    int32_t count = kind == 0 ? dataset->layout.n_columns : kind == 1 ? dataset->layout.n_parameters : dataset->layout.n_arrays;
    int32_t saved = kind == 0 ? dataset->original_layout.n_columns : kind == 1 ? dataset->original_layout.n_parameters : dataset->original_layout.n_arrays;
    for (i = 0; i < count; i++) {
      SDDS_ENUM_DEFINITION **slot = enumSlot(&dataset->layout, kind, i);
      if (!*slot || (i < saved && *slot == *enumSlot(&dataset->original_layout, kind, i))) continue;
      for (j = 0; j < saved && *slot != *enumSlot(&dataset->original_layout, kind, j); j++) {}
      if (j != saved) continue;
      SDDS_FreeEnumDefinition(*slot);
      *slot = NULL;
      if (kind == 0) dataset->layout.column_definition[i].enum_values = NULL;
      else if (kind == 1) dataset->layout.parameter_definition[i].enum_values = NULL;
      else dataset->layout.array_definition[i].enum_values = NULL;
    }
  }
}
