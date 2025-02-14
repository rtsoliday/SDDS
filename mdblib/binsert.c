/**
 * @file binsert.c
 * @brief Binary search and insertion functions.
 * @details This file provides implementations for binary search and insertion
 *          operations on arrays of pointers or data values.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday
 */


#include "mdb.h"

/* The data for binaryInsert and binaryIndexSearch is an array of pointers to the
 * values.  To search a simple array of values, use binaryArraySearch.
 */

/**
 * @brief Inserts a new member into a sorted array using binary search.
 *
 * This function performs a binary search to find the appropriate position
 * for the new member in the sorted array. If a duplicate is found, it sets
 * the duplicate flag and does not insert the new member.
 *
 * @param array Pointer to the array of pointers to members.
 * @param members The number of members currently in the array.
 * @param newMember Pointer to the new member to be inserted.
 * @param compare Comparison function that defines the order of the elements.
 * @param duplicate Pointer to an integer that is set to 1 if a duplicate is found.
 * @return The index at which the new member was inserted, or the index of a duplicate.
 */
long binaryInsert(void **array, long members, void *newMember,
                  int (*compare)(const void *c1, const void *c2), int32_t *duplicate) {
  long ih, il, im, comparison;

  *duplicate = 0;
  if (members == 0) {
    *array = newMember;
    return 0;
  }

  il = 0;
  ih = members - 1;
  if ((comparison = (*compare)(array[il], newMember)) >= 0) {
    if (comparison == 0) {
      *duplicate = 1;
      return 0;
    }
    im = 0;
  } else if ((comparison = (*compare)(array[ih], newMember)) <= 0) {
    if (comparison == 0) {
      *duplicate = 1;
      return ih;
    }
    im = members;
  } else {
    while ((ih - il) > 1) {
      im = (il + ih) / 2;
      if ((comparison = (*compare)(array[im], newMember)) == 0) {
        *duplicate = 1;
        return im;
      }
      if (comparison > 0)
        ih = im;
      else
        il = im;
    }
    im = ih;
  }
  for (il = members; il > im; il--)
    array[il] = array[il - 1];
  array[im] = newMember;
  return im;
}

/**
 * @brief Searches for a key in a sorted array of pointers using binary search.
 *
 * This function performs a binary search to find the index of the key in the array.
 * If the key is not found and the bracket flag is set, it returns the index of the
 * nearest element less than or equal to the key.
 *
 * @param array Pointer to the array of pointers to members.
 * @param members The number of members in the array.
 * @param key Pointer to the key to search for.
 * @param compare Comparison function that defines the order of the elements.
 * @param bracket If non-zero, returns the nearest index if the key is not found.
 * @return The index of the key if found, the nearest index if bracket is set,
 *         or -1 if the key is not found and bracket is not set.
 */
long binaryIndexSearch(void **array, long members, void *key,
                       int (*compare)(const void *c1, const void *c2), long bracket) {
  long ih, il, im, comparison;

  if (members == 0)
    return -1;

  il = 0;
  ih = members - 1;
  if ((comparison = (*compare)(array[il], key)) >= 0) {
    if (comparison == 0)
      return il;
    im = 0;
  } else if ((comparison = (*compare)(array[ih], key)) <= 0) {
    if (comparison == 0)
      return ih;
    im = members;
  } else {
    while ((ih - il) > 1) {
      im = (il + ih) / 2;
      if ((comparison = (*compare)(array[im], key)) == 0)
        return im;
      if (comparison > 0)
        ih = im;
      else
        il = im;
    }
    im = ih;
  }
  if (!bracket)
    return -1;

  /* return index of nearest point less than or equal to the key */
  comparison = (*compare)(array[im], key);
  if (comparison <= 0)
    return im;
  comparison = (*compare)(array[il], key);
  if (comparison <= 0)
    return il;
  return -1;
}

/**
 * @brief Searches for a key in a sorted array of data values using binary search.
 *
 * This function performs a binary search on an array of data values (not pointers)
 * to find the index of the key. If the key is not found and the bracket flag is set,
 * it returns the index of the nearest element less than or equal to the key.
 *
 * @param array Pointer to the array of data values.
 * @param elemSize The size of each element in the array.
 * @param members The number of members in the array.
 * @param key Pointer to the key to search for.
 * @param compare Comparison function that defines the order of the elements.
 * @param bracket If non-zero, returns the nearest index if the key is not found.
 * @return The index of the key if found, the nearest index if bracket is set,
 *         or -1 if the key is not found and bracket is not set.
 */
long binaryArraySearch(void *array, size_t elemSize, long members, void *key,
                       int (*compare)(const void *c1, const void *c2), long bracket)
/* Same as binaryIndexSearch, but the array is an array of data values,
 * rather than an array of pointers 
 */
{
  long ih, il, im, comparison;

  if (members == 0)
    return -1;

  il = 0;
  ih = members - 1;
  if ((comparison = (*compare)((void *)((char *)array + il * elemSize), key)) >= 0) {
    if (comparison == 0)
      return il;
    im = 0;
  } else if ((comparison = (*compare)((void *)((char *)array + ih * elemSize), key)) <= 0) {
    if (comparison == 0)
      return ih;
    im = ih;
  } else {
    while ((ih - il) > 1) {
      im = (il + ih) / 2;
      if ((comparison = (*compare)((void *)((char *)array + im * elemSize), key)) == 0)
        return im;
      if (comparison > 0)
        ih = im;
      else
        il = im;
    }
    im = ih;
  }
  if (!bracket)
    return -1;

  /* return index of nearest point less than or equal to the key */
  comparison = (*compare)((void *)((char *)array + im * elemSize), key);
  if (comparison <= 0)
    return im;
  comparison = (*compare)((void *)(((char *)array) + il * elemSize), key);
  if (comparison <= 0)
    return il;
  return -1;
}
