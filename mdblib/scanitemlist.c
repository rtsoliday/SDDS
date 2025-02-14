/**
 * @file scanitemlist.c
 * @brief This file contains functions for scanning and parsing item lists with various data types.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include <stdarg.h>
#if defined(vxWorks)
#  include "epicsStdlib.h"
#endif

/**
 * @brief Scans a list of items and assigns values based on provided keywords and types.
 *
 * This function processes a list of item strings, each potentially in the format "keyword=value",
 * and assigns the corresponding values to provided data pointers based on specified SDDS types.
 * It supports setting flags and handles various data types as specified in the variable arguments.
 *
 * @param flags Pointer to an unsigned long variable where flags will be set based on matched items.
 * @param item Array of strings containing items to be scanned, each in the format "keyword=value" or "keyword".
 * @param items Pointer to a long variable indicating the number of items in the `item` array.
 *              It will be updated based on the scanning process.
 * @param mode Unsigned long value specifying the scanning mode, used for future expansion and controlling behavior.
 * @param ... Variable arguments specifying pairs of <keyword>, <SDDS-type>, <pointer>, <number-required>, <set-flag>, etc.,
 *             ending with NULL.
 *
 * @return Returns 1 on successful scanning and parsing of items, or 0 on failure.
 */
long scanItemList(unsigned long *flags, char **item, long *items, unsigned long mode, ...)
{
  va_list argptr;
  long i, j, type, flag, retval, length, number, match;
  char *keyword;
  void *data;
  static char **valueptr = NULL;
  static long *keylength = NULL;
  static short *item_matched = NULL, *has_equals = NULL;
  static long maxitems = 0;

  if (!flags)
    return bombre("null flags pointer seen (scanItemList)", NULL, 0);
  if (!item)
    return bombre("null item pointer seen (scanItemList)", NULL, 0);
  if (!items)
    return bombre("null items pointer seen (scanItemList)", NULL, 0);
  if (*items <= 0) {
    *flags = 0;
    return 1;
  }
  if (*items > maxitems) {
    valueptr = trealloc(valueptr, sizeof(*valueptr) * (maxitems = *items));
    keylength = trealloc(keylength, sizeof(*keylength) * maxitems);
    item_matched = trealloc(item_matched, sizeof(*item_matched) * maxitems);
    has_equals = trealloc(has_equals, sizeof(*has_equals) * maxitems);
  }
  *flags = 0;
  for (i = 0; i < *items; i++) {
    item_matched[i] = 0;
#if DEBUG
    fprintf(stderr, "item %ld: %s\n", i, item[i]);
#endif
    if ((valueptr[i] = strchr(item[i], '='))) {
      if ((keylength[i] = valueptr[i] - item[i]) <= 0)
        return bombre("zero-length keyword seen (scanItemList)", NULL, 0);
      *valueptr[i]++ = 0;
      has_equals[i] = 1;
    } else {
      keylength[i] = strlen(item[i]);
      has_equals[i] = 0;
    }

#if DEBUG
    fprintf(stderr, "  keyword: %s   valueptr: %s\n", item[i], valueptr[i]);
#endif
  }

  va_start(argptr, mode);
  retval = 1;
  do {
    if (!(keyword = va_arg(argptr, char *)))
      break;
    type = va_arg(argptr, int32_t);
    data = va_arg(argptr, void *);
    number = va_arg(argptr, int32_t);
    flag = va_arg(argptr, uint32_t);
    length = strlen(keyword);
    match = -1;
    for (i = 0; i < *items; i++)
      if (strncmp_case_insensitive(item[i], keyword, MIN(length, keylength[i])) == 0) {
        if (match != -1) {
          fprintf(stderr, "ambiguous item %s seen\n", keyword);
          retval = 0;
        }
        match = i;
      }
#if defined(DEBUG)
    if (match != -1)
      fprintf(stderr, "keyword %s and item %ld (%s) match\n", keyword, match, item[match]);
    else
      fprintf(stderr, "no match for keyword %s\n", keyword);
#endif
    if (!retval)
      break;
    if (match == -1)
      continue;
    if (!has_equals[match] && number && mode & SCANITEMLIST_IGNORE_VALUELESS)
      continue;
    if (item_matched[match]) {
      fprintf(stderr, "error: ambiguous qualifier %s seen\n", item[match]);
      retval = 0;
      break;
    }
    item_matched[match] = 1;
    *flags |= flag;
    if (!valueptr[match]) {
      if (type == -1)
        continue;
      fprintf(stderr, "error: value not given for %s\n", keyword);
      retval = 0;
      break;
    }
    switch (type) {
    case SDDS_LONGDOUBLE:
#if defined(vxWorks)
      fprintf(stderr, "error: SDDS_LONGDOUBLE is not supported on vxWorks yet\n");
      return 0;
#else
      *((long double *)data) = (long double)strtold(valueptr[match], NULL);
#endif
      break;
    case SDDS_DOUBLE:
      *((double *)data) = atof(valueptr[match]);
      break;
    case SDDS_FLOAT:
      *((float *)data) = (float)atof(valueptr[match]);
      break;
    case SDDS_LONG64:
#if defined(vxWorks)
      epicsParseInt64(valueptr[match], (int64_t *)data, 10, NULL);
#else
      *((int64_t *)data) = (uint64_t)atoll(valueptr[match]);
#endif
      break;
    case SDDS_ULONG64:
      *((uint64_t *)data) = (uint64_t)strtoull(valueptr[match], NULL, 10);
      break;
    case SDDS_LONG:
      *((int32_t *)data) = (int32_t)atol(valueptr[match]);
      break;
    case SDDS_ULONG:
      *((uint32_t *)data) = (uint32_t)strtoul(valueptr[match], NULL, 10);
      break;
    case SDDS_SHORT:
      *((short *)data) = (short)atol(valueptr[match]);
      break;
    case SDDS_USHORT:
      *((unsigned short *)data) = (unsigned short)atol(valueptr[match]);
      break;
    case SDDS_STRING:
      cp_str((char **)data, valueptr[match]);
      break;
    case SDDS_CHARACTER:
      *((char *)data) = valueptr[match][0];
      break;
    default:
      fprintf(stderr, "Error: value not accepted for qualifier %s\n",
              item[match]);
      break;
    }
  } while (retval == 1);
  va_end(argptr);
  for (i = 0; i < *items; i++) {
    if (!item_matched[i]) {
      if (!has_equals[i] && mode & SCANITEMLIST_UNKNOWN_VALUE_OK)
        continue;
      if (has_equals[i] && mode & SCANITEMLIST_UNKNOWN_KEYVALUE_OK)
        continue;
      fprintf(stderr, "unknown keyword/value given: %s\n", item[i]);
      return 0;
    }
  }
  if (mode & SCANITEMLIST_REMOVE_USED_ITEMS) {
    for (i = j = 0; i < *items; i++) {
      if (!item_matched[i]) {
        if (i != j) {
          item_matched[j] = 1;
          item[j] = item[i];
        }
        j++;
      }
    }
    *items = j;
  }
  return retval;
}

/**
 * @brief Scans a list of items with extended flag support and assigns values based on provided keywords and types.
 *
 * This function is similar to `scanItemList` but uses an `unsigned long long` for flags,
 * allowing for a larger set of flags. It processes a list of item strings, assigns values based on SDDS types,
 * and supports various scanning modes.
 *
 * @param flags Pointer to an unsigned long long variable where flags will be set based on matched items.
 * @param item Array of strings containing items to be scanned, each in the format "keyword=value" or "keyword".
 * @param items Pointer to a long variable indicating the number of items in the `item` array.
 *              It will be updated based on the scanning process.
 * @param mode Unsigned long value specifying the scanning mode, used for future expansion and controlling behavior.
 * @param ... Variable arguments specifying pairs of <keyword>, <SDDS-type>, <pointer>, <number-required>, <set-flag>, etc.,
 *             ending with NULL.
 *
 * @return Returns 1 on successful scanning and parsing of items, or 0 on failure.
 */
long scanItemListLong(unsigned long long *flags, char **item, long *items, unsigned long mode, ...)
{
  va_list argptr;
  long i, j, type, retval, length, number, match;
  long long flag;
  char *keyword;
  void *data;
  static char **valueptr = NULL;
  static long *keylength = NULL;
  static short *item_matched = NULL, *has_equals = NULL;
  static long maxitems = 0;

  if (!flags)
    return bombre("null flags pointer seen (scanItemList)", NULL, 0);
  if (!item)
    return bombre("null item pointer seen (scanItemList)", NULL, 0);
  if (!items)
    return bombre("null items pointer seen (scanItemList)", NULL, 0);
  if (*items <= 0) {
    *flags = 0;
    return 1;
  }
  if (*items > maxitems) {
    valueptr = trealloc(valueptr, sizeof(*valueptr) * (maxitems = *items));
    keylength = trealloc(keylength, sizeof(*keylength) * maxitems);
    item_matched = trealloc(item_matched, sizeof(*item_matched) * maxitems);
    has_equals = trealloc(has_equals, sizeof(*has_equals) * maxitems);
  }
  *flags = 0;
  for (i = 0; i < *items; i++) {
    item_matched[i] = 0;
#if DEBUG
    fprintf(stderr, "item %ld: %s\n", i, item[i]);
#endif
    if ((valueptr[i] = strchr(item[i], '='))) {
      if ((keylength[i] = valueptr[i] - item[i]) <= 0)
        return bombre("zero-length keyword seen (scanItemList)", NULL, 0);
      *valueptr[i]++ = 0;
      has_equals[i] = 1;
    } else {
      keylength[i] = strlen(item[i]);
      has_equals[i] = 0;
    }

#if DEBUG
    fprintf(stderr, "  keyword: %s   valueptr: %s\n", item[i], valueptr[i]);
#endif
  }

  va_start(argptr, mode);
  retval = 1;
  do {
    if (!(keyword = va_arg(argptr, char *)))
      break;
    type = va_arg(argptr, int32_t);
    data = va_arg(argptr, void *);
    number = va_arg(argptr, int32_t);
    flag = va_arg(argptr, uint64_t);
    length = strlen(keyword);
    match = -1;
    for (i = 0; i < *items; i++)
      if (strncmp_case_insensitive(item[i], keyword, MIN(length, keylength[i])) == 0) {
        if (match != -1) {
          fprintf(stderr, "ambiguous item %s seen\n", keyword);
          retval = 0;
        }
        match = i;
      }
#if defined(DEBUG)
    if (match != -1)
      fprintf(stderr, "keyword %s and item %ld (%s) match\n", keyword, match, item[match]);
    else
      fprintf(stderr, "no match for keyword %s\n", keyword);
#endif
    if (!retval)
      break;
    if (match == -1)
      continue;
    if (!has_equals[match] && number && mode & SCANITEMLIST_IGNORE_VALUELESS)
      continue;
    if (item_matched[match]) {
      fprintf(stderr, "error: ambiguous qualifier %s seen\n", item[match]);
      retval = 0;
      break;
    }
    item_matched[match] = 1;
    *flags |= flag;
    if (!valueptr[match]) {
      if (type == -1)
        continue;
      fprintf(stderr, "error: value not given for %s\n", keyword);
      retval = 0;
      break;
    }
    switch (type) {
    case SDDS_DOUBLE:
      *((double *)data) = atof(valueptr[match]);
      break;
    case SDDS_FLOAT:
      *((float *)data) = (float)atof(valueptr[match]);
      break;
    case SDDS_LONG:
      *((int32_t *)data) = atol(valueptr[match]);
      break;
    case SDDS_ULONG:
#if defined(_WIN32)
      *((uint32_t *)data) = (uint32_t)_atoi64(valueptr[match]);
#else
#  if defined(vxWorks)
      epicsParseUInt32(valueptr[match], (uint32_t *)data, 10, NULL);
#  else
      *((uint32_t *)data) = (uint32_t)atoll(valueptr[match]);
#  endif
#endif
      break;
    case SDDS_SHORT:
      *((short *)data) = (short)atol(valueptr[match]);
      break;
    case SDDS_USHORT:
      *((unsigned short *)data) = (unsigned short)atol(valueptr[match]);
      break;
    case SDDS_STRING:
      cp_str((char **)data, valueptr[match]);
      break;
    case SDDS_CHARACTER:
      *((char *)data) = valueptr[match][0];
      break;
    default:
      fprintf(stderr, "Error: value not accepted for qualifier %s\n",
              item[match]);
      break;
    }
  } while (retval == 1);
  va_end(argptr);
  for (i = 0; i < *items; i++) {
    if (!item_matched[i]) {
      if (!has_equals[i] && mode & SCANITEMLIST_UNKNOWN_VALUE_OK)
        continue;
      if (has_equals[i] && mode & SCANITEMLIST_UNKNOWN_KEYVALUE_OK)
        continue;
      fprintf(stderr, "unknown keyword/value given: %s\n", item[i]);
      return 0;
    }
  }
  if (mode & SCANITEMLIST_REMOVE_USED_ITEMS) {
    for (i = j = 0; i < *items; i++) {
      if (!item_matched[i]) {
        if (i != j) {
          item_matched[j] = 1;
          item[j] = item[i];
        }
        j++;
      }
    }
    *items = j;
  }
  return retval;
}

/**
 * @brief Scans a list of items without flag extension and assigns values based on provided keywords and types.
 *
 * This version of the scanning function lacks certain functionalities such as flag extension
 * and may not flag entries that don't match any keywords. It is used in various locations within the codebase.
 *
 * @param flags Pointer to an unsigned long variable where flags will be set based on matched items.
 * @param item Array of strings containing items to be scanned, each in the format "keyword=value" or "keyword".
 * @param items Pointer to a long variable indicating the number of items in the `item` array.
 *              It will be updated based on the scanning process.
 * @param ... Variable arguments specifying pairs of <keyword>, <SDDS-type>, <pointer>, <number-required>, <set-flag>, etc.,
 *             ending with NULL.
 *
 * @return Returns 1 on successful scanning and parsing of items, or 0 on failure.
 */
long scan_item_list(unsigned long *flags, char **item, long *items, ...)
{
  va_list argptr;
  long i, j, type, flag, retval, length, match;
  //long number;
  char *keyword;
  void *data;
  static char **valueptr = NULL;
  static long *keylength = NULL, *item_matched = NULL;
  static long maxitems = 0;

  if (!flags)
    return bombre("null flags pointer seen (scan_item_list)", NULL, 0);
  if (!item)
    return bombre("null item pointer seen (scan_item_list)", NULL, 0);
  if (!items)
    return bombre("null items pointer seen (scan_item_list)", NULL, 0);
  if (*items <= 0) {
    *flags = 0;
    return 1;
  }
  if (*items > maxitems) {
    valueptr = trealloc(valueptr, sizeof(*valueptr) * (maxitems = *items));
    keylength = trealloc(keylength, sizeof(*keylength) * maxitems);
    item_matched = trealloc(item_matched, sizeof(*item_matched) * maxitems);
  }
  *flags = 0;
  for (i = 0; i < *items; i++) {
    item_matched[i] = 0;
#if DEBUG
    fprintf(stderr, "item %ld: %s\n", i, item[i]);
#endif
    if ((valueptr[i] = strchr(item[i], '='))) {
      if ((keylength[i] = valueptr[i] - item[i]) <= 0)
        return bombre("zero-length keyword seen (scan_item_list)", NULL, 0);
      *valueptr[i]++ = 0;
    } else
      keylength[i] = strlen(item[i]);
#if DEBUG
    fprintf(stderr, "  keyword: %s   valueptr: %s\n", item[i], valueptr[i]);
#endif
  }

  va_start(argptr, items);
  retval = 1;
  do {
    if (!(keyword = va_arg(argptr, char *)))
      break;
    type = va_arg(argptr, int32_t);
    data = va_arg(argptr, void *);
    //number = va_arg(argptr, int32_t);
    va_arg(argptr, int32_t);
    flag = va_arg(argptr, uint32_t);
    length = strlen(keyword);
    match = -1;
    for (i = 0; i < *items; i++)
      if (strncmp_case_insensitive(item[i], keyword, MIN(length, keylength[i])) == 0) {
        if (match != -1) {
          fprintf(stderr, "ambiguous item %s seen\n", keyword);
          retval = 0;
        }
        match = i;
      }
#if defined(DEBUG)
    if (match != -1)
      fprintf(stderr, "keyword %s and item %ld (%s) match\n", keyword, match, item[match]);
    else
      fprintf(stderr, "no match for keyword %s\n", keyword);
#endif
    if (!retval)
      break;
    if (match == -1)
      continue;
    if (item_matched[match]) {
      fprintf(stderr, "error: ambiguous qualifier %s seen\n", item[match]);
      retval = 0;
      break;
    }
    item_matched[match] = 1;
    *flags |= flag;
    if (!valueptr[match]) {
      if (type == -1)
        continue;
      fprintf(stderr, "error: value not given for %s\n", keyword);
      retval = 0;
      break;
    }
    switch (type) {
    case SDDS_DOUBLE:
      *((double *)data) = atof(valueptr[match]);
      break;
    case SDDS_FLOAT:
      *((float *)data) = (float)atof(valueptr[match]);
      break;
    case SDDS_LONG:
      *((int32_t *)data) = atol(valueptr[match]);
      break;
    case SDDS_ULONG:
#if defined(_WIN32)
      *((uint32_t *)data) = (uint32_t)_atoi64(valueptr[match]);
#else
#  if defined(vxWorks)
      epicsParseUInt32(valueptr[match], (uint32_t *)data, 10, NULL);
#  else
      *((uint32_t *)data) = (uint32_t)atoll(valueptr[match]);
#  endif
#endif
      break;
    case SDDS_SHORT:
      *((short *)data) = (short)atol(valueptr[match]);
      break;
    case SDDS_USHORT:
      *((unsigned short *)data) = (unsigned short)atol(valueptr[match]);
      break;
    case SDDS_STRING:
      cp_str((char **)data, valueptr[match]);
      break;
    case SDDS_CHARACTER:
      *((char *)data) = valueptr[match][0];
      break;
    default:
      fprintf(stderr, "Error: value not accepted for qualifier %s\n", item[match]);
      break;
    }
    free(item[match]);
    (*items)--;
    for (j = match; j < *items; j++) {
      item[j] = item[j + 1];
      valueptr[j] = valueptr[j + 1];
      keylength[j] = keylength[j + 1];
    }
  } while (retval == 1 && *items);
  va_end(argptr);
  return retval;
}

/**
 * @brief Checks if a string contains an unescaped equal sign, indicating a keyword-value phrase.
 *
 * This function scans the input string for the presence of an equal sign ('=') that is not preceded by a backslash ('\\').
 * It returns 1 if such a keyword-value pair is found, otherwise returns 0.
 * Additionally, it handles escaped equal signs by removing the escape character.
 *
 * @param string The input string to be checked for keyword-value phrases.
 *
 * @return Returns 1 if the string contains an unescaped equal sign, otherwise returns 0.
 */
long contains_keyword_phrase(char *string) {
  char *ptr, *ptr0;
  ptr0 = string;
  while ((ptr = strchr(string, '='))) {
    if (ptr != ptr0 && *(ptr - 1) != '\\')
      return 1;
    if (ptr != ptr0 && *(ptr - 1) == '\\')
      strcpy_ss(ptr - 1, ptr);
    string = ptr + 1;
  }
  return 0;
}
