/**
 * @file SDDSenum.h
 * @brief Private, dependency-free dictionary implementation shared by C and SDDS++.
 * @copyright Copyright (c) 2026 The University of Chicago.
 * @license Software License Agreement in LICENSE.
 * @author SDDS maintainers
 */
#ifndef SDDS_ENUM_CORE_H
#define SDDS_ENUM_CORE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>

typedef struct {
  int32_t code;
  char *label;
} SDDS_ENUM_ENTRY;

struct SDDS_ENUM_DEFINITION {
  size_t count;
  SDDS_ENUM_ENTRY *entries;
  SDDS_ENUM_ENTRY **byCode, **byLabel;
  char *text;
  /* C-library comparison cache; identities are never reused after destruction. */
  const void *identityOwner;
  uint64_t identity, comparisonIdentity;
  int comparisonResult;
};

/** @brief Check strict UTF-8, excluding NUL and invalid scalar values. */
static inline int sddsEnumUtf8(const unsigned char *s) {
  while (*s) {
    uint32_t value;
    unsigned int n, i;
    if (*s < 128) { s++; continue; }
    if (*s >= 0xc2 && *s <= 0xdf) { n = 1; value = *s & 31; }
    else if (*s >= 0xe0 && *s <= 0xef) { n = 2; value = *s & 15; }
    else if (*s >= 0xf0 && *s <= 0xf4) { n = 3; value = *s & 7; }
    else return 0;
    s++;
    for (i = 0; i < n; i++) {
      if ((*s & 0xc0) != 0x80) return 0;
      value = (value << 6) | (*s++ & 63);
    }
    if ((n == 1 && value < 128) || (n == 2 && value < 2048) ||
        (n == 3 && value < 65536) || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff)) return 0;
  }
  return 1;
}

/** @brief Release an independently owned dictionary. */
static inline void sddsEnumDestroy(struct SDDS_ENUM_DEFINITION *d) {
  size_t i;
  if (!d) return;
  for (i = 0; i < d->count; i++) free(d->entries[i].label);
  free(d->entries); free(d->byCode); free(d->byLabel); free(d->text); free(d);
}

/** @brief Order dictionary index entries by code. */
static inline int sddsEnumCodeCompare(const void *a, const void *b) {
  int32_t x = (*(SDDS_ENUM_ENTRY *const *)a)->code;
  int32_t y = (*(SDDS_ENUM_ENTRY *const *)b)->code;
  return (x > y) - (x < y);
}

/** @brief Order dictionary index entries by exact UTF-8 bytes. */
static inline int sddsEnumLabelCompare(const void *a, const void *b) {
  return strcmp((*(SDDS_ENUM_ENTRY *const *)a)->label,
                (*(SDDS_ENUM_ENTRY *const *)b)->label);
}

/** @brief Parse, validate, index and canonically serialize a bounded mapping. */
static inline struct SDDS_ENUM_DEFINITION *sddsEnumParse(const char *text, size_t limit, const char **error) {
  struct SDDS_ENUM_DEFINITION *d = NULL;
  size_t length, capacity = 0, i, total = 1;
  const char *p = text;
  char *out;
  *error = "Invalid enum mapping";
  if (!text || !*text) return NULL;
  for (length = 0; length <= limit && text[length]; length++) {}
  if (length > (SIZE_MAX - 32) / 32) { *error = "Enum mapping allocation size overflow"; return NULL; }
  if (length > limit) { *error = "Enum mapping exceeds configured byte limit"; return NULL; }
  d = (struct SDDS_ENUM_DEFINITION *)calloc(1, sizeof(*d));
  if (!d) goto memory;
  while (*p) {
    intmax_t code;
    char *end, *label;
    size_t used = 0;
    if (!((*p >= '0' && *p <= '9') || (*p == '-' && p[1] >= '0' && p[1] <= '9'))) goto invalid;
    errno = 0;
    code = strtoimax(p, &end, 10);
    if (errno || code < INT32_MIN || code > INT32_MAX || *end != ':') goto invalid;
    p = end + 1;
    {
      const char *q = p;
      while (*q && *q != ',') {
        if (*q == '\\' && q[1]) q++;
        q++;
      }
      label = (char *)malloc((size_t)(q - p) + 1);
    }
    if (!label) goto memory;
    while (*p && *p != ',') {
      unsigned char c = (unsigned char)*p++;
      if (c == '\\') {
        c = (unsigned char)*p++;
        switch (c) {
        case ',': case ':': case '\\': case '"': break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        default: free(label); goto invalid;
        }
      } else if (c == ':' || c == '"' || c < 32 || c == 127) {
        free(label); goto invalid;
      }
      label[used++] = (char)c;
    }
    label[used] = 0;
    if (!used || !sddsEnumUtf8((const unsigned char *)label)) { free(label); goto invalid; }
    if (d->count == capacity) {
      SDDS_ENUM_ENTRY *entries;
      capacity = capacity ? capacity * 2 : 8;
      entries = (SDDS_ENUM_ENTRY *)realloc(d->entries, capacity * sizeof(*entries));
      if (!entries) { free(label); goto memory; }
      d->entries = entries;
    }
    d->entries[d->count].code = (int32_t)code;
    d->entries[d->count++].label = label;
    total += 14 + 2 * used;
    if (*p == ',' && !*++p) goto invalid;
  }
  d->byCode = (SDDS_ENUM_ENTRY **)malloc(d->count * sizeof(*d->byCode));
  d->byLabel = (SDDS_ENUM_ENTRY **)malloc(d->count * sizeof(*d->byLabel));
  d->text = (char *)malloc(total);
  if (!d->byCode || !d->byLabel || !d->text) goto memory;
  for (i = 0; i < d->count; i++) d->byCode[i] = d->byLabel[i] = d->entries + i;
  qsort(d->byCode, d->count, sizeof(*d->byCode), sddsEnumCodeCompare);
  qsort(d->byLabel, d->count, sizeof(*d->byLabel), sddsEnumLabelCompare);
  for (i = 1; i < d->count; i++) {
    if (d->byCode[i - 1]->code == d->byCode[i]->code ||
        !strcmp(d->byLabel[i - 1]->label, d->byLabel[i]->label)) {
      *error = "Duplicate enum code or label"; goto invalid;
    }
  }
  out = d->text;
  for (i = 0; i < d->count; i++) {
    const unsigned char *s = (const unsigned char *)d->entries[i].label;
    if (i) *out++ = ',';
    out += sprintf(out, "%" PRId32 ":", d->entries[i].code);
    while (*s) {
      unsigned char c = *s++;
      switch (c) {
      case ',': case ':': case '\\': case '"': *out++ = '\\'; break;
      case '\n': *out++ = '\\'; c = 'n'; break;
      case '\r': *out++ = '\\'; c = 'r'; break;
      case '\t': *out++ = '\\'; c = 't'; break;
      default: break;
      }
      *out++ = (char)c;
    }
  }
  *out = 0;
  if ((size_t)(out - d->text) > limit) { *error = "Canonical enum mapping exceeds configured byte limit"; goto invalid; }
  *error = NULL;
  return d;
memory:
  *error = "Unable to allocate enum dictionary";
invalid:
  sddsEnumDestroy(d);
  return NULL;
}

/** @brief Resolve a code using the sorted dictionary index. */
static inline const char *sddsEnumLabel(const struct SDDS_ENUM_DEFINITION *d, int32_t code) {
  size_t lo = 0, hi;
  if (!d) return NULL;
  hi = d->count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (d->byCode[mid]->code == code) return d->byCode[mid]->label;
    if (d->byCode[mid]->code < code) lo = mid + 1; else hi = mid;
  }
  return NULL;
}

/** @brief Resolve a label without reserving any integer error sentinel. */
static inline int sddsEnumCode(const struct SDDS_ENUM_DEFINITION *d, const char *label, int32_t *code) {
  size_t lo = 0, hi;
  if (!d || !label || !code) return 0;
  hi = d->count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    int cmp = strcmp(d->byLabel[mid]->label, label);
    if (!cmp) { *code = d->byLabel[mid]->code; return 1; }
    if (cmp < 0) lo = mid + 1; else hi = mid;
  }
  return 0;
}

/** @brief Compare mappings independently of declaration order. */
static inline int sddsEnumEqual(const struct SDDS_ENUM_DEFINITION *a, const struct SDDS_ENUM_DEFINITION *b) {
  size_t i;
  if (a == b) return 1;
  if (!a || !b) return a == b;
  if (a->count != b->count) return 0;
  for (i = 0; i < a->count; i++)
    if (a->byCode[i]->code != b->byCode[i]->code || strcmp(a->byCode[i]->label, b->byCode[i]->label)) return 0;
  return 1;
}
#endif
