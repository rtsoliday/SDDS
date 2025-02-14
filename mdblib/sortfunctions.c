/**
 * @file sortfunctions.c
 * @brief Useful routines for sorting, compatible with qsort()
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
#include "SDDStypes.h"

/**
 * @brief Compare two doubles in ascending order.
 *
 * This function compares two double values pointed to by `a` and `b`.
 *
 * @param a Pointer to the first double.
 * @param b Pointer to the second double.
 * @return int Returns -1 if *a < *b, 1 if *a > *b, and 0 if equal.
 */
int double_cmpasc(const void *a, const void *b) {
  double diff;

  diff = *((double *)b) - *((double *)a);
  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

/**
 * @brief Compare the absolute values of two doubles in ascending order.
 *
 * This function compares the absolute values of two double values pointed to by `a` and `b`.
 *
 * @param a Pointer to the first double.
 * @param b Pointer to the second double.
 * @return int Returns -1 if |*a| < |*b|, 1 if |*a| > |*b|, and 0 if equal.
 */
int double_abs_cmpasc(const void *a, const void *b) {
  double diff;

  diff = fabs(*((double *)b)) - fabs(*((double *)a));
  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

/**
 * @brief Compare two doubles in descending order.
 *
 * This function compares two double values pointed to by `a` and `b` in descending order.
 *
 * @param a Pointer to the first double.
 * @param b Pointer to the second double.
 * @return int Returns 1 if *a < *b, -1 if *a > *b, and 0 if equal.
 */
int double_cmpdes(const void *a, const void *b) {
  double diff;

  diff = *((double *)b) - *((double *)a);
  return (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
}

/**
 * @brief Compare the absolute values of two doubles in descending order.
 *
 * This function compares the absolute values of two double values pointed to by `a` and `b` in descending order.
 *
 * @param a Pointer to the first double.
 * @param b Pointer to the second double.
 * @return int Returns 1 if |*a| < |*b|, -1 if |*a| > |*b|, and 0 if equal.
 */
int double_abs_cmpdes(const void *a, const void *b) {
  double diff;

  diff = fabs(*((double *)b)) - fabs(*((double *)a));
  return (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
}

void double_copy(void *a, void *b) {

  *((double *)a) = *((double *)b);
}

int float_cmpasc(const void *a, const void *b) {
  float diff;

  diff = *((float *)b) - *((float *)a);
  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

int float_abs_cmpasc(const void *a, const void *b) {
  float diff;

  diff = fabsf(*((float *)b)) - fabsf(*((float *)a));
  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

int float_cmpdes(const void *a, const void *b) {
  float diff;

  diff = *((float *)b) - *((float *)a);
  return (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
}

int float_abs_cmpdes(const void *a, const void *b) {
  float diff;

  diff = fabsf(*((float *)b)) - fabsf(*((float *)a));
  return (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
}

void float_copy(void *a, void *b) {
  *((float *)a) = *((float *)b);
}

/**
 * @brief Compare two long integers in ascending order.
 *
 * This function compares two `int32_t` values pointed to by `a` and `b`.
 *
 * @param a Pointer to the first long integer.
 * @param b Pointer to the second long integer.
 * @return int Returns -1 if *a < *b, 1 if *a > *b, and 0 if equal.
 */
int long_cmpasc(const void *a, const void *b) {
  int32_t diff;
  diff = *((int32_t *)b) - *((int32_t *)a);
  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

/**
 * @brief Compare the absolute values of two long integers in ascending order.
 *
 * This function compares the absolute values of two `int32_t` values pointed to by `a` and `b`.
 *
 * @param a Pointer to the first long integer.
 * @param b Pointer to the second long integer.
 * @return int Returns -1 if |*a| < |*b|, 1 if |*a| > |*b|, and 0 if equal.
 */
int long_abs_cmpasc(const void *a, const void *b) {
  int32_t diff;
  diff = labs(*((int32_t *)b)) - labs(*((int32_t *)a));
  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

int long_cmpdes(const void *a, const void *b) {
  long diff;
  diff = *((long *)a) - *((long *)b);
  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

int long_abs_cmpdes(const void *a, const void *b) {
  long diff;
  diff = labs(*((long *)a)) - labs(*((long *)b));
  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

void long_copy(void *a, void *b) {
  *((long *)a) = *((long *)b);
}

/**
 * @brief Compare two strings in ascending order.
 *
 * This function compares two null-terminated strings pointed to by `a` and `b` using `strcmp`.
 *
 * @param a Pointer to the first string.
 * @param b Pointer to the second string.
 * @return int Returns a negative value if *a < *b, a positive value if *a > *b, and 0 if equal.
 */
int string_cmpasc(const void *a, const void *b) {
  return (strcmp(*((char **)a), *((char **)b)));
}

int string_cmpdes(const void *a, const void *b) {
  return (strcmp(*((char **)b), *((char **)a)));
}

/**
 * @brief Copy a string value.
 *
 * This function copies the string from the source pointed to by `b` to the destination pointed to by `a`.
 * If the destination buffer is large enough, it uses `strcpy_ss`; otherwise, it allocates memory using `cp_str`.
 *
 * @param a Destination pointer where the string will be copied.
 * @param b Source pointer from where the string will be copied.
 */
void string_copy(void *a, void *b) {
  if ((long)strlen(*((char **)a)) >= (long)strlen(*((char **)b)))
    strcpy_ss(*((char **)a), *((char **)b));
  else
    cp_str(((char **)a), *((char **)b));
}

/**
 * @brief Remove duplicate elements from a sorted array.
 *
 * This function iterates through a sorted array and removes duplicate items based on the provided comparison function.
 *
 * @param base Pointer to the first element of the array.
 * @param n_items Number of items in the array.
 * @param size Size of each element in the array.
 * @param compare Function pointer to the comparison function.
 * @param copy Function pointer to the copy function.
 * @return int Returns the new number of unique items in the array.
 */
int unique(void *base, size_t n_items, size_t size,
           int (*compare)(const void *a, const void *b),
           void (*copy)(void *a, void *b)) {
  long i, j;

  for (i = 0; i < n_items - 1; i++) {
    if ((*compare)((char *)base + i * size, (char *)base + (i + 1) * size) == 0) {
      for (j = i + 1; j < n_items - 1; j++)
        (*copy)((char *)base + j * size, (char *)base + (j + 1) * size);
      n_items--;
      i--;
    }
  }
  return (n_items);
}

static int (*item_compare)(const void *a, const void *b);
static int column_to_compare;
static int size_of_element;
static int number_of_columns;

/**
 * @brief Set up parameters for row-based sorting.
 *
 * This function initializes the sorting parameters for sorting 2D data by rows based on a specified column.
 *
 * @param sort_by_column The column index to sort by.
 * @param n_columns Total number of columns.
 * @param element_size Size of each element in a row.
 * @param compare Function pointer to the comparison function.
 */
void set_up_row_sort(
  int sort_by_column,
  size_t n_columns,
  size_t element_size,
  int (*compare)(const void *a, const void *b)) {
  if ((column_to_compare = sort_by_column) >= (number_of_columns = n_columns))
    bomb("column out of range in set_up_row_sort()", NULL);
  size_of_element = element_size;
  if (!(item_compare = compare))
    bomb("null function pointer in set_up_row_sort()", NULL);
}

/**
 * @brief Compare two rows based on the previously set sorting parameters.
 *
 * This static function is used internally to compare two rows during sorting.
 *
 * @param av Pointer to the first row.
 * @param bv Pointer to the second row.
 * @return int Result of the comparison.
 */
int row_compare(const void *av, const void *bv) {
  char **a, **b;
  a = (char **)av;
  b = (char **)bv;
  return ((*item_compare)(*a + size_of_element * column_to_compare,
                          *b + size_of_element * column_to_compare));
}

void row_copy(void *av, void *bv) {
  void **a, **b;
  void *ptr;
  a = (void **)av;
  b = (void **)bv;
  ptr = *a;
  *a = *b;
  *b = ptr;
}

static long orderIndices; /* compare source indices if keys are identical? */

/**
 * @brief Compare two KEYED_INDEX structures based on string keys.
 *
 * This function compares two `KEYED_INDEX` structures using their `stringKey` fields.
 * If the string keys are identical and `orderIndices` is set, it compares based on `rowIndex`.
 *
 * @param ki1 Pointer to the first KEYED_INDEX.
 * @param ki2 Pointer to the second KEYED_INDEX.
 * @return int Returns a negative value if ki1 < ki2, positive if ki1 > ki2, or based on `rowIndex` if keys are equal.
 */
int CompareStringKeyedIndex(const void *ki1, const void *ki2) {
  int value;
  if ((value = strcmp((*(const KEYED_INDEX *)ki1).stringKey, (*(const KEYED_INDEX *)ki2).stringKey)))
    return value;
  if (orderIndices)
    return (*(const KEYED_INDEX *)ki1).rowIndex - (*(const KEYED_INDEX *)ki2).rowIndex;
  return value;
}

/**
 * @brief Compare two KEYED_INDEX structures based on double keys.
 *
 * This function compares two `KEYED_INDEX` structures using their `doubleKey` fields.
 * If the double keys are identical and `orderIndices` is set, it compares based on `rowIndex`.
 *
 * @param ki1 Pointer to the first KEYED_INDEX.
 * @param ki2 Pointer to the second KEYED_INDEX.
 * @return int Returns -1 if ki1 < ki2, 1 if ki1 > ki2, or based on `rowIndex` if keys are equal.
 */
int CompareDoubleKeyedIndex(const void *ki1, const void *ki2) {
  double diff;
  if ((diff = (*(const KEYED_INDEX *)ki1).doubleKey - (*(const KEYED_INDEX *)ki2).doubleKey)) {
    if (diff < 0)
      return -1;
    return 1;
  }
  if (orderIndices)
    return (*(const KEYED_INDEX *)ki1).rowIndex - (*(const KEYED_INDEX *)ki2).rowIndex;
  return 0;
}

/**
 * @brief Compare two KEYED_EQUIVALENT groups based on string keys.
 *
 * This function compares the first string key of two `KEYED_EQUIVALENT` groups.
 *
 * @param kg1 Pointer to the first KEYED_EQUIVALENT group.
 * @param kg2 Pointer to the second KEYED_EQUIVALENT group.
 * @return int Returns the result of `strcmp` on the first string keys.
 */
int CompareStringKeyedGroup(const void *kg1, const void *kg2) {
  return strcmp((*(KEYED_EQUIVALENT *)kg1).equivalent[0]->stringKey, (*(KEYED_EQUIVALENT *)kg2).equivalent[0]->stringKey);
}

/**
 * @brief Compare two KEYED_EQUIVALENT groups based on double keys.
 *
 * This function compares the first double key of two `KEYED_EQUIVALENT` groups.
 *
 * @param kg1 Pointer to the first KEYED_EQUIVALENT group.
 * @param kg2 Pointer to the second KEYED_EQUIVALENT group.
 * @return int Returns -1 if kg1 < kg2, 1 if kg1 > kg2, or 0 if equal.
 */
int CompareDoubleKeyedGroup(const void *kg1, const void *kg2) {
  double diff;
  if ((diff = (*(KEYED_EQUIVALENT *)kg1).equivalent[0]->doubleKey - (*(KEYED_EQUIVALENT *)kg2).equivalent[0]->doubleKey)) {
    if (diff < 0)
      return -1;
    return 1;
  }
  return 0;
}

/**
 * @brief Create sorted key groups from data.
 *
 * This function generates sorted groups of keys from the provided data based on the specified key type.
 *
 * @param keyGroups Pointer to store the number of key groups created.
 * @param keyType The type of key (e.g., SDDS_STRING or SDDS_DOUBLE).
 * @param data Pointer to the data to be grouped.
 * @param points Number of data points.
 * @return KEYED_EQUIVALENT** Returns an array of pointers to `KEYED_EQUIVALENT` structures.
 */
KEYED_EQUIVALENT **MakeSortedKeyGroups(long *keyGroups, long keyType, void *data, long points) {
  KEYED_EQUIVALENT **keyedEquiv = NULL;
  static KEYED_INDEX *keyedIndex = NULL;
  long iEquiv, i2, j;
  long i1;

  if (!points)
    return 0;
  if (keyedIndex)
    free(keyedIndex);
  if (!(keyedIndex = (KEYED_INDEX *)malloc(sizeof(*keyedIndex) * points)) ||
      !(keyedEquiv = (KEYED_EQUIVALENT **)malloc(sizeof(*keyedEquiv) * points))) {
    fprintf(stderr, "memory allocation failure");
    exit(1);
  }
  if (keyType == SDDS_STRING) {
    char **string;
    string = data;
    for (i1 = 0; i1 < points; i1++) {
      keyedIndex[i1].stringKey = string[i1];
      keyedIndex[i1].rowIndex = i1;
    }
    orderIndices = 1; /* subsort by source row index */
    qsort((void *)keyedIndex, points, sizeof(*keyedIndex), CompareStringKeyedIndex);
    orderIndices = 0; /* ignore index in comparisons */
    for (iEquiv = i1 = 0; i1 < points; iEquiv++) {
      for (i2 = i1 + 1; i2 < points; i2++) {
        if (CompareStringKeyedIndex(keyedIndex + i1, keyedIndex + i2))
          break;
      }
      if (!(keyedEquiv[iEquiv] = (KEYED_EQUIVALENT *)malloc(sizeof(KEYED_EQUIVALENT))) ||
          !(keyedEquiv[iEquiv]->equivalent = (KEYED_INDEX **)malloc(sizeof(KEYED_INDEX *) * (i2 - i1)))) {
        fprintf(stderr, "memory allocation failure");
        exit(1);
      }
      keyedEquiv[iEquiv]->equivalents = i2 - i1;
      keyedEquiv[iEquiv]->nextIndex = 0;
      for (j = 0; i1 < i2; i1++, j++)
        keyedEquiv[iEquiv]->equivalent[j] = keyedIndex + i1;
    }
  } else {
    double *value;
    value = data;
    for (i1 = 0; i1 < points; i1++) {
      keyedIndex[i1].doubleKey = value[i1];
      keyedIndex[i1].rowIndex = i1;
    }
    orderIndices = 1; /* subsort by source row index */
    qsort((void *)keyedIndex, points, sizeof(*keyedIndex), CompareDoubleKeyedIndex);
    orderIndices = 0; /* ignore index in comparisons */
    for (iEquiv = i1 = 0; i1 < points; iEquiv++) {
      for (i2 = i1 + 1; i2 < points; i2++) {
        if (CompareDoubleKeyedIndex(keyedIndex + i1, keyedIndex + i2))
          break;
      }
      if (!(keyedEquiv[iEquiv] = (KEYED_EQUIVALENT *)malloc(sizeof(KEYED_EQUIVALENT))) ||
          !(keyedEquiv[iEquiv]->equivalent = (KEYED_INDEX **)malloc(sizeof(KEYED_INDEX *) * (i2 - i1)))) {
        fprintf(stderr, "memory allocation failure");
        exit(1);
      }
      keyedEquiv[iEquiv]->equivalents = i2 - i1;
      keyedEquiv[iEquiv]->nextIndex = 0;
      for (j = 0; i1 < i2; i1++, j++)
        keyedEquiv[iEquiv]->equivalent[j] = keyedIndex + i1;
    }
  }
  *keyGroups = iEquiv;
  return keyedEquiv;
}

/**
 * @brief Find a matching key group for a search key.
 *
 * This function searches for a key group that matches the provided search key.
 *
 * @param keyGroup Array of key groups.
 * @param keyGroups Number of key groups.
 * @param keyType The type of key (e.g., SDDS_STRING or SDDS_DOUBLE).
 * @param searchKeyData Pointer to the search key data.
 * @param reuse Flag indicating whether to allow reuse of key groups.
 * @return long Returns the row index of the matching key group or -1 if not found.
 */
long FindMatchingKeyGroup(KEYED_EQUIVALENT **keyGroup, long keyGroups, long keyType,
                          void *searchKeyData, long reuse) {
  static KEYED_EQUIVALENT *searchKey = NULL;
  static KEYED_INDEX keyedIndex;
  long rowIndex, i;

  if (!searchKey) {
    searchKey = (KEYED_EQUIVALENT *)malloc(sizeof(*searchKey));
    searchKey->equivalent = (KEYED_INDEX **)malloc(sizeof(*(searchKey->equivalent)));
    searchKey->equivalent[0] = &keyedIndex;
    searchKey->equivalents = 1;
  }
  if (keyType == SDDS_STRING) {
    keyedIndex.stringKey = *(char **)searchKeyData;
    i = binaryIndexSearch((void **)keyGroup, keyGroups, (void *)searchKey, CompareStringKeyedGroup, 0);
  } else {
    keyedIndex.doubleKey = *(double *)searchKeyData;
    i = binaryIndexSearch((void **)keyGroup, keyGroups, (void *)searchKey, CompareDoubleKeyedGroup, 0);
  }
  if (i < 0 || keyGroup[i]->nextIndex >= keyGroup[i]->equivalents)
    return -1;
  rowIndex = keyGroup[i]->equivalent[keyGroup[i]->nextIndex]->rowIndex;
  if (!reuse)
    keyGroup[i]->nextIndex += 1;
  return rowIndex;
}

/**
 * @brief Sort data and return the sorted index.
 *
 * This function sorts the provided data based on the specified type and order, and returns an array of indices representing the sorted order.
 *
 * @param data Pointer to the data to be sorted.
 * @param type The data type (e.g., SDDS_STRING, SDDS_DOUBLE).
 * @param rows Number of rows in the data.
 * @param increaseOrder If non-zero, sort in increasing order; otherwise, sort in decreasing order.
 * @return long* Returns an array of indices representing the sorted order, or NULL on failure.
 */
long *sort_and_return_index(void *data, long type, long rows, long increaseOrder) {
  long *index = NULL;
  long i, keyGroups, i1, j, istart, jstart, i2, j2;
  KEYED_EQUIVALENT **keyGroup;
  char **tmpstring = NULL;
  double *tmpdata = NULL;

  if (!rows || !data)
    return 0;
  index = (long *)malloc(sizeof(*index) * rows);
  switch (type) {
  case SDDS_STRING:
    tmpstring = (char **)data;
    keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_STRING, tmpstring, rows);
    break;
  default:
    if (type == SDDS_DOUBLE)
      tmpdata = (double *)data;
    else {
      tmpdata = calloc(sizeof(*tmpdata), rows);
      for (i = 0; i < rows; i++) {
        switch (type) {
        case SDDS_SHORT:
          tmpdata[i] = *((short *)data + i);
          break;
        case SDDS_USHORT:
          tmpdata[i] = *((unsigned short *)data + i);
          break;
        case SDDS_LONG:
          tmpdata[i] = *((int32_t *)data + i);
          break;
        case SDDS_ULONG:
          tmpdata[i] = *((uint32_t *)data + i);
          break;
        case SDDS_CHARACTER:
          tmpdata[i] = *((unsigned char *)data + i);
          break;
        case SDDS_FLOAT:
          tmpdata[i] = *((float *)data + i);
          break;
        default:
          fprintf(stderr, "Invalid data type given!\n");
          exit(1);
          break;
        }
      }
    }
    keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_DOUBLE, tmpdata, rows);
    if (type != SDDS_DOUBLE)
      free(tmpdata);
    break;
  }
  i1 = 0;
  if (increaseOrder) {
    istart = 0;
  } else {
    istart = keyGroups - 1;
  }
  for (i = istart, i2 = 0; i2 < keyGroups; i2++) {
    if (increaseOrder) {
      jstart = 0;
    } else {
      jstart = keyGroup[i]->equivalents - 1;
    }
    for (j = jstart, j2 = 0; j2 < keyGroup[i]->equivalents; j2++) {
      switch (type) {
      case SDDS_STRING:
        ((char **)data)[i1] = keyGroup[i]->equivalent[j]->stringKey;
        break;
      case SDDS_DOUBLE:
        ((double *)data)[i1] = keyGroup[i]->equivalent[j]->doubleKey;
        break;
      case SDDS_FLOAT:
        ((float *)data)[i1] = (float)keyGroup[i]->equivalent[j]->doubleKey;
        break;
      case SDDS_LONG:
        ((int32_t *)data)[i1] = (int32_t)keyGroup[i]->equivalent[j]->doubleKey;
        break;
      case SDDS_ULONG:
        ((uint32_t *)data)[i1] = (uint32_t)keyGroup[i]->equivalent[j]->doubleKey;
        break;
      case SDDS_SHORT:
        ((short *)data)[i1] = (short)keyGroup[i]->equivalent[j]->doubleKey;
        break;
      case SDDS_USHORT:
        ((unsigned short *)data)[i1] = (unsigned short)keyGroup[i]->equivalent[j]->doubleKey;
        break;
      case SDDS_CHARACTER:
        ((char *)data)[i1] = (unsigned char)keyGroup[i]->equivalent[j]->doubleKey;
        break;
      default:
        fprintf(stderr, "Invalid data type given!\n");
        exit(1);
        break;
      }
      index[i1] = keyGroup[i]->equivalent[j]->rowIndex;
      i1++;
      if (increaseOrder)
        j++;
      else
        j--;
    }
    if (increaseOrder)
      i++;
    else
      i--;
  }
  for (i = 0; i < keyGroups; i++) {
    free(keyGroup[i]->equivalent);
    free(keyGroup[i]);
  }
  free(keyGroup);
  return index;
}

/**
 * @brief Compare two strings while skipping specified characters.
 *
 * This function compares two null-terminated strings `s1` and `s2`, ignoring any characters found in the `skip` string.
 *
 * @param s1 Pointer to the first string.
 * @param s2 Pointer to the second string.
 * @param skip String containing characters to be skipped during comparison.
 * @return int Returns a negative value if `s1` < `s2`, a positive value if `s1` > `s2`, and 0 if equal.
 */
int strcmp_skip(const char *s1, const char *s2, const char *skip) {
  do {
    if (*s1 != *s2) {
      while (*s1 && strchr(skip, *s1))
        s1++;
      while (*s2 && strchr(skip, *s2))
        s2++;
      if (*s1 != *s2)
        return *s1 - *s2;
    }
    s1++;
    s2++;
  } while (*s1 && *s2);
  return *s1 - *s2;
}
