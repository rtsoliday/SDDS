/**
 * @file array.c
 * @brief Implementation of dynamic 2D arrays and memory management functions.
 *
 * This file contains functions for allocating, resizing, and freeing 2D arrays,
 * as well as custom memory allocation functions with tracking capabilities.
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

static FILE *fp_tmalloc = NULL;
static FILE *fp_trealloc = NULL;
static FILE *fp_tfree = NULL;

/**
 * @brief Keeps a record of memory allocations by opening tracking files.
 *
 * Opens tracking files for memory allocation, reallocation, and freeing based on the provided filename.
 * If tracking files are already open, they are closed before reopening.
 *
 * @param filename The base name for the tracking files.
 */
void keep_alloc_record(char *filename) {
  char s[100];

  if (fp_tmalloc)
    free(fp_tmalloc);
  if (fp_trealloc)
    free(fp_trealloc);
  if (fp_tfree)
    free(fp_tfree);
  sprintf(s, "%s.tmalloc", filename);
  fp_tmalloc = fopen_e(s, "w", 0);
  sprintf(s, "%s.trealloc", filename);
  fp_trealloc = fopen_e(s, "w", 0);
  sprintf(s, "%s.tfree", filename);
  fp_tfree = fopen_e(s, "w", 0);
}

/**
 * @brief Allocates a memory block of the specified size with zero initialization.
 *
 * Uses `calloc` to allocate memory and initializes it to zero. Tracks the allocation if tracking is enabled.
 * If the allocation fails, the function prints an error message and aborts the program.
 *
 * @param size_of_block The size of the memory block to allocate in bytes.
 * @return Pointer to the allocated memory block.
 */
void *tmalloc(uint64_t size_of_block) {
  void *ptr;
  static uint64_t total_bytes = 0;

  if (size_of_block <= 0)
    size_of_block = 4;

  /* even though the function is tMalloc, I use calloc to get memory filled
   * with zeros
   */
  if (!(ptr = calloc(size_of_block, 1))) {
    printf("error: memory allocation failure--%"PRIu64" Bytes requested.\n",
           size_of_block);
    printf("tmalloc() has allocated %"PRIu64" bytes previously\n", total_bytes);
    abort();
  }
  if (fp_tmalloc) {
    fprintf(fp_tmalloc, "%"PRIx64"  %"PRIu64"\n", (uint64_t)ptr, size_of_block);
    fflush(fp_tmalloc);
  }
  total_bytes += size_of_block;
  return (ptr);
}

/**
 * @brief Allocates a 2D array with specified dimensions.
 *
 * Allocates memory for a 2D array where each row is a contiguous block of memory. Initializes each row to zero.
 *
 * @param size The size of each element in the array.
 * @param n1 The number of rows.
 * @param n2 The number of columns.
 * @return Pointer to the allocated 2D array.
 */
void **zarray_2d(uint64_t size, uint64_t n1, uint64_t n2) {
  void **ptr1, **ptr0;

  ptr0 = ptr1 = (void **)tmalloc((uint64_t)(sizeof(*ptr0) * n1));
  while (n1--)
    *ptr1++ = (void *)tmalloc((uint64_t)(size * n2));
  return (ptr0);
}

/**
 * @brief Resizes an existing 2D array to new dimensions.
 *
 * Resizes the array of pointers if the number of rows (`n1`) increases.
 * Additionally, resizes each row to accommodate more columns (`n2`) if needed.
 * If resizing fails, the function aborts the program.
 *
 * @param size The size of each element in the array.
 * @param old_n1 The original number of rows.
 * @param old_n2 The original number of columns.
 * @param array Pointer to the original 2D array.
 * @param n1 The new number of rows.
 * @param n2 The new number of columns.
 * @return Pointer to the resized 2D array.
 */
void **resize_zarray_2d(uint64_t size, uint64_t old_n1, uint64_t old_n2,
                        void **array, uint64_t n1, uint64_t n2) {
  void **ptr;

  if (n1 > old_n1) {
    /* increase length of array of pointers */
    if (!(array = (void **)trealloc((void *)array,
                                    (uint64_t)(sizeof(*array) * n1))))
      bomb("memory allocation failuire in resize_zarray_2d()", NULL);
    /* allocate memory for new pointed-to objects */
    ptr = array + n1;
    while (n1-- != old_n1)
      *--ptr = (void *)tmalloc(size * n2);
  }

  if (n2 > old_n2) {
    /* increase size of old pointed-to objects */
    ptr = array;
    while (old_n1--) {
      if (!(*ptr = (void *)trealloc((void *)*ptr, (uint64_t)(size * n2))))
        bomb("memory allocation failure in resize_zarray_2d()", NULL);
      ptr++;
    }
  }

  return (array);
}

/**
 * @brief Frees a 2D array and its associated memory.
 *
 * Frees each row of the 2D array and then frees the array of pointers itself.
 *
 * @param array Pointer to the 2D array to free.
 * @param n1 The number of rows in the array.
 * @param n2 The number of columns in the array.
 * @return Status of the free operation (1 if successful, 0 otherwise).
 */
int free_zarray_2d(void **array, uint64_t n1, uint64_t n2) {
  void *ptr0;

  if (!(ptr0 = array))
    return (0);
  while (n1--) {
    if (*array) {
      tfree(*array);
      *array = NULL;
    } else
      return (0);
    array++;
  }
  return (tfree(ptr0));
}

/**
 * @brief Reallocates a memory block to a new size.
 *
 * Uses `realloc` to resize the memory block. Tracks the reallocation if tracking is enabled.
 * If the reallocation fails, the function prints an error message and aborts the program.
 *
 * @param old_ptr Pointer to the original memory block.
 * @param size_of_block The new size for the memory block in bytes.
 * @return Pointer to the reallocated memory block.
 */
void *trealloc(void *old_ptr, uint64_t size_of_block) {
  void *ptr;
  static uint64_t total_bytes = 0;

  if (size_of_block <= 0)
    size_of_block = 4;

  if (!old_ptr)
    return (tmalloc(size_of_block));
  if (!(ptr = realloc((void *)old_ptr, (uint64_t)(size_of_block)))) {
    printf("error: memory reallocation failure--%"PRIu64" bytes requested.\n",
           size_of_block);
    printf("trealloc() has reallocated %"PRIu64" bytes previously\n", total_bytes);
    abort();
  }
  if (fp_trealloc) {
    fprintf(fp_trealloc, "d:%"PRIx64"\na:%"PRIx64"  %"PRIu64"\n", (uint64_t)old_ptr,
            (uint64_t)ptr, size_of_block);
    fflush(fp_trealloc);
  }
  total_bytes += size_of_block;
  return (ptr);
}

/**
 * @brief Sets a block of memory to zero.
 *
 * Iterates through the specified memory block and sets each byte to zero.
 *
 * @param mem Pointer to the memory block.
 * @param n_bytes The number of bytes to set to zero.
 */
void zero_memory(void *mem, uint64_t n_bytes) {
  char *cmem;

  if (!(cmem = mem))
    return;
  while (n_bytes--)
    *cmem++ = 0;
}

/**
 * @brief Frees a memory block and records the deallocation if tracking is enabled.
 *
 * Frees the specified memory block and logs the deallocation if tracking is active.
 *
 * @param ptr Pointer to the memory block to free.
 * @return Status of the free operation (1 if successful, 0 otherwise).
 */
int tfree(void *ptr) {
  if (fp_tfree) {
    fprintf(fp_tfree, "%"PRIx64"\n", (uint64_t)ptr);
    fflush(fp_tfree);
  }
  if (ptr) {
    free(ptr);
    return (1);
  }
  return (0);
}

/**
 * @brief Allocates a 1D array with specified lower and upper indices.
 *
 * Allocates memory for a 1D array and adjusts the pointer based on the lower index to allow
 * negative indexing if necessary.
 *
 * @param size The size of each element in the array.
 * @param lower_index The lower index of the array.
 * @param upper_index The upper index of the array.
 * @return Pointer to the allocated 1D array.
 */
void *array_1d(uint64_t size, uint64_t lower_index, uint64_t upper_index) {
  char *ptr;

  if (!(ptr = tmalloc((uint64_t)size * (upper_index - lower_index + 1))))
    bomb("unable to allocate array (array_1d)", NULL);
  ptr -= lower_index * size;
  return ((void *)ptr);
}

/**
 * @brief Allocates a 2D array with specified lower and upper indices for both dimensions.
 *
 * Allocates memory for a 2D array of pointers, where each row is a 1D array.
 * Adjusts pointers based on lower indices to allow for flexible indexing ranges.
 *
 * @param size The size of each element in the array.
 * @param lower1 The lower index for the first dimension (rows).
 * @param upper1 The upper index for the first dimension (rows).
 * @param lower2 The lower index for the second dimension (columns).
 * @param upper2 The upper index for the second dimension (columns).
 * @return Pointer to the allocated 2D array.
 */
void **array_2d(uint64_t size, uint64_t lower1, uint64_t upper1,
                uint64_t lower2, uint64_t upper2)
    /* array is [upper1-lower1+1]x[upper2-lower2+1] */
{
  register uint64_t i, n1, n2;
  char **ptr;

  if (!(ptr = tmalloc((uint64_t)sizeof(*ptr) *(n1 = upper1 - lower1 + 1))))
    bomb("unable to allocate array (array_2d)", NULL);

  n2 = upper2 - lower2 + 1;
  for (i = 0; i < n1; i++) {
    if (!(ptr[i] = tmalloc((uint64_t)size * n2)))
      bomb("unable to allocate array (array_2d)", NULL);
    ptr[i] -= lower2 * size;
  }

  return ((void **)(ptr - lower1));
}

/**
 * @brief Frees a 1D array that was previously allocated.
 *
 * Adjusts the pointer based on the lower index and frees the allocated memory.
 *
 * @param array Pointer to the 1D array to free.
 * @param size The size of each element in the array.
 * @param lower_index The lower index of the array.
 * @param upper_index The upper index of the array.
 * @return Status of the free operation (1 if successful, 0 otherwise).
 */
int free_array_1d(void *array, uint64_t size, uint64_t lower_index,
                  uint64_t upper_index) {
  if (!array)
    return (0);
  free((char *)array + size * lower_index);
  return (1);
}

/**
 * @brief Frees a 2D array and its associated memory.
 *
 * Adjusts the pointer based on the lower indices and frees each row followed by the array of pointers.
 *
 * @param array Pointer to the 2D array to free.
 * @param size The size of each element in the array.
 * @param lower1 The lower index for the first dimension (rows).
 * @param upper1 The upper index for the first dimension (rows).
 * @param lower2 The lower index for the second dimension (columns).
 * @param upper2 The upper index for the second dimension (columns).
 * @return Status of the free operation (1 if successful, 0 otherwise).
 */
int free_array_2d(void **array, uint64_t size, uint64_t lower1, uint64_t upper1,
                  uint64_t lower2, uint64_t upper2)
    /* array is [upper1-lower1]x[upper2-lower2] */
{
  uint64_t i, n1;
  char *ptr;

  if (!array)
    return (0);

  n1 = upper1 - lower1 + 1;
  array += lower1;
  for (i = 0; i < n1; i++) {
    if ((ptr = (char *)array[i] + size * lower2))
      free(ptr);
  }

  free(array);
  return (1);
}

/**
 * @brief Allocates a contiguous 2D array with zero-based indexing.
 *
 * Allocates a single contiguous block of memory for a 2D array and sets up row pointers accordingly.
 *
 * @param size The size of each element in the array.
 * @param n1 The number of rows.
 * @param n2 The number of columns.
 * @return Pointer to the allocated contiguous 2D array.
 */
void **czarray_2d(const uint64_t size, const uint64_t n1, const uint64_t n2) {
  char **ptr0;
  char *buffer;
  uint64_t i;

  ptr0 = (char **)tmalloc((uint64_t)(sizeof(*ptr0) * n1));
  buffer = (char *)tmalloc((uint64_t)(sizeof(*buffer) * size * n1 * n2));
  for (i = 0; i < n1; i++)
    ptr0[i] = buffer + i * size * n2;
  return ((void **)ptr0);
}

/**
 * @brief Resizes a contiguous 2D array to new dimensions.
 *
 * Resizes both the array of row pointers and the contiguous memory block holding the array elements.
 *
 * @param data Pointer to the original contiguous 2D array.
 * @param size The size of each element in the array.
 * @param n1 The new number of rows.
 * @param n2 The new number of columns.
 * @return Pointer to the resized contiguous 2D array.
 */
void **resize_czarray_2d(void **data, uint64_t size, uint64_t n1, uint64_t n2) {
  char **ptr0;
  char *buffer;
  uint64_t i;

  if (!data)
    return czarray_2d(size, n1, n2);
  buffer = (char *)trealloc(*data, (uint64_t)(sizeof(char) * size * n1 * n2));
  ptr0 = (char **)trealloc(data, (uint64_t)(sizeof(char *) * n1));
  for (i = 0; i < n1; i++)
    ptr0[i] = buffer + i * size * n2;
  return ((void **)ptr0);
}

/**
 * @brief Frees a contiguous 2D array and its associated memory.
 *
 * Frees the contiguous memory block holding the array elements and the array of row pointers.
 *
 * @param array Pointer to the contiguous 2D array to free.
 * @param n1 The number of rows in the array.
 * @param n2 The number of columns in the array.
 * @return Status of the free operation (always returns 0).
 */
int free_czarray_2d(void **array, uint64_t n1, uint64_t n2) {
  free(*array);
  free(array);
  return 0;
}
