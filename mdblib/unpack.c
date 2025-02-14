/**
 * @file unpack.c
 * @brief Provides functions for determining unpacking types and opening unpacked files.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C Saunders, R. Soliday
 */

#include "mdb.h"
#include "scan.h"

#define UNPACK_TYPES 3
static char *unpackSuffix[UNPACK_TYPES] = {
  "gz",
  "F",
  "Z",
};
static char *unpackCommand[UNPACK_TYPES] = {
  "gzip -dcn %s 2> /dev/null ",
  "freeze -dc %s ",
  "uncompress -c %s ",
};

/**
 * @brief Determines the unpacking type based on the file extension.
 *
 * This function checks the file extension of the given filename to determine the type of unpacking required.
 * It can optionally provide the unpacked filename without its extension and verify if the file conforms
 * to the SDDS format if the UNPACK_REQUIRE_SDDS flag is set in the mode.
 *
 * @param filename The name of the file to check.
 * @param unpackedName Pointer to store the unpacked filename without extension (optional).
 * @param mode Flags indicating unpacking requirements and options.
 * @return The index of the unpacking type if the extension matches, or -1 if no matching type is found or an error occurs.
 */
long PackSuffixType(char *filename, char **unpackedName, unsigned long mode) {
  char *extension, buffer[10];
  FILE *fp;
  long i;

  if (!(extension = strrchr(filename, '.')))
    return -1;

  extension++;
  for (i = 0; i < UNPACK_TYPES; i++)
    if (strcmp(extension, unpackSuffix[i]) == 0) {
      if (unpackedName) {
        cp_str(unpackedName, filename);
        extension = strrchr(*unpackedName, '.');
        *extension = 0;
      }
      break;
    }
  if (i == UNPACK_TYPES)
    return -1;

  if (mode & UNPACK_REQUIRE_SDDS) {
    if (!(fp = fopen(filename, FOPEN_READ_MODE)))
      return -1;
    if (fread(buffer, sizeof(*buffer), 4, fp) == 4 && strncmp(buffer, "SDDS", 4) == 0) {
      fclose(fp);
      return -1;
    }
    fclose(fp);
  }
  return i;
}

/**
 * @brief Opens a file, potentially unpacking it based on its extension and mode.
 *
 * This function attempts to open a file, determining whether it needs to be unpacked based on its extension.
 * If unpacking is required and specified by the mode, it either uses a pipe or creates a temporary file
 * to access the unpacked data.
 *
 * @param filename The name of the file to open.
 * @param mode Flags indicating unpacking requirements and options.
 * @param popenUsed Pointer to a short that will be set to 1 if popen is used, otherwise 0 (optional).
 * @param tmpFileUsed Pointer to store the name of the temporary file used for unpacking (optional).
 * @return A FILE pointer to the opened file, or NULL if the file could not be opened or an error occurred.
 */
FILE *UnpackFopen(char *filename, unsigned long mode, short *popenUsed, char **tmpFileUsed) {
  static char *command = NULL;
  long type;
  char *tmpName;

  if (popenUsed)
    *popenUsed = 0;
  if (tmpFileUsed)
    *tmpFileUsed = NULL;
  if (!filename)
    return NULL;
  if ((type = PackSuffixType(filename, NULL, mode)) < 0)
    return fopen(filename, FOPEN_READ_MODE);

  if (!(command = trealloc(command, sizeof(*command) * (strlen(filename) + 100))))
    return NULL;
  if (mode & UNPACK_USE_PIPE) {
    sprintf(command, unpackCommand[type], filename);
    if (popenUsed)
      *popenUsed = 1;
#if defined(vxWorks)
    fprintf(stderr, "popen is not supported in vxWorks\n");
    exit(1);
    return NULL;
#else
    return popen(command, FOPEN_READ_MODE);
#endif
  } else {
    sprintf(command, unpackCommand[type], filename);
    tmpName = tmpname(NULL);
    strcat(command, "> /tmp/");
    strcat(command, tmpName);
    system(command);

    sprintf(command, "/tmp/%s", tmpName);
    if (tmpFileUsed)
      cp_str(tmpFileUsed, command);

    return fopen(command, FOPEN_READ_MODE);
  }
}
