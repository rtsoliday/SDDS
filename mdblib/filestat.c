/**
 * @file filestat.c
 * @brief Utility functions for handling file links and retrieving file information.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author H. Shang, M. Borland, R. Soliday
 */

#if !defined(_WIN32)
#  include <unistd.h>
#endif
#include "mdb.h"

 /**
  * @brief Retrieves the leading directories from a given path.
  *
  * Allocates memory for the leading directories part of the provided PATH.
  * If memory allocation fails, returns NULL. Assumes that any trailing
  * slashes in PATH have already been removed. This function avoids using
  * the `dirname` builtin to maintain consistent behavior across different
  * environments.
  *
  * @param path The file path from which to extract the leading directories.
  * @return A newly allocated string containing the leading directories, or NULL if out of memory.
  */
char *dir_name(const char *path) {
  char *newpath = NULL;
  const char *slash = NULL;
  int length; /* Length of result, not including NUL.  */

  slash = strrchr(path, '/');
  if (slash == 0) {
    /* File is in the current directory.  */
    path = ".";
    length = 1;
  } else {
    /* Remove any trailing slashes from the result.  */
    while (slash > path && *slash == '/')
      --slash;
    length = slash - path + 1;
  }
  newpath = (char *)malloc(length + 1);
  if (newpath == 0)
    return 0;
  strncpy(newpath, path, length);
  newpath[length] = 0;
  return newpath;
}

 /**
  * @brief Reads the first link of a file.
  *
  * Returns the first link of the specified file. If the file is not a link
  * or if an error occurs (including unsupported platforms), returns NULL.
  * On successful retrieval, the returned string is dynamically allocated
  * and should be freed by the caller.
  *
  * @param filename The name of the file to read the link from.
  * @return A newly allocated string containing the link target, or NULL if not a link or on error.
  */
char *read_file_link(const char *filename) {
#if defined(_WIN32) || defined(vxWorks)
  return (NULL);
#else
  int size = 100, nchars = -1;
  char *tmpbuf = NULL, *dir = NULL, *tempname = NULL;
  tmpbuf = (char *)calloc(size, sizeof(char));
  while (1) {
    nchars = readlink(filename, tmpbuf, size);
    if (nchars < 0) {
      free(tmpbuf);
      return NULL;
    }
    if (nchars < size) {
      if (tmpbuf[0] == '/')
        return tmpbuf;
      else {
        dir = dir_name(filename);
        tempname = (char *)malloc(sizeof(char) * (strlen(filename) + strlen(tmpbuf) + 2));
        tempname[0] = 0;
        strcat(tempname, dir);
        strcat(tempname, "/");
        strcat(tempname, tmpbuf);
        free(tmpbuf);
        free(dir);
        return tempname;
      }
    }
    size *= 2;
    tmpbuf = (char *)realloc(tmpbuf, sizeof(char) * size);
  }
#endif
}

 /**
  * @brief Retrieves the last link in a chain of symbolic links.
  *
  * Traverses the chain of symbolic links starting from the given filename
  * and returns the name of the last link that directly points to the final
  * target file. If the file is not a link, returns the original filename.
  *
  * @param filename The starting file name to resolve the last link from.
  * @return The name of the last link in the chain, or the original filename if not a link.
  */
const char *read_file_lastlink(const char *filename) {
  char *linkname = NULL;
  const char *lastlink = NULL;
  lastlink = filename;
  while ((linkname = read_file_link(filename)) != NULL) {
    lastlink = filename;
    filename = linkname;
  }
  return lastlink;
}

 /**
  * @brief Resolves the final target file that a symbolic link points to.
  *
  * Follows the chain of symbolic links starting from the given filename and
  * returns the name of the final target file. The returned string is dynamically
  * allocated and should be freed by the caller. If the filename is not a link,
  * returns NULL.
  *
  * @param filename The starting file name to resolve to the final target.
  * @return A newly allocated string containing the final target file name, or NULL if not a link.
  */
char *read_last_link_to_file(const char *filename) {
  char *linkname = NULL;
  char *tmpname = NULL;

  if ((linkname = read_file_link(filename)) == NULL)
    return NULL;
  tmpname = (char *)calloc(1024, sizeof(char));
  do {
    strcpy_ss(tmpname, linkname);
    free(linkname);
    linkname = read_file_link(tmpname);
  } while (linkname != NULL);
  return tmpname;
}

 /**
  * @brief Retrieves the file status of a given file or its final link target.
  *
  * Obtains the file status information for the specified filename. If the file
  * is a symbolic link and a final_file is provided, it retrieves the status of
  * the final target file. If the file does not exist or an error occurs during
  * the retrieval, an error message is printed to stderr.
  *
  * @param filename The name of the file to retrieve the status for.
  * @param final_file Optional parameter specifying the final target file if filename is a link.
  * @param filestat Pointer to a struct stat where the file status will be stored.
  * @return 0 on success, 1 on failure.
  */
long get_file_stat(const char *filename, const char *final_file, struct stat *filestat) {
  const char *input = NULL;
  if (!fexists(filename)) {
    fprintf(stderr, "%s file does not exist, unable to get the state of it!\n", filename);
    return (1);
  }
  input = filename;
  if (final_file)
    input = final_file;
  if (stat(input, filestat) != 0) {
    fprintf(stderr, "Problem getting state of file %s\n", input);
    return (1);
  }
  return 0;
}

 /**
  * @brief Checks if a file has been modified.
  *
  * Determines whether the specified input file has been modified by comparing
  * its current state against a previously recorded state. If the file is a symbolic
  * link, it checks the final target file for modifications. The function updates
  * the final_file pointer if the link target has changed.
  *
  * @param inputfile The path to the input file to check for modifications.
  * @param final_file Pointer to a string containing the final target file name. May be updated.
  * @param input_stat Pointer to a struct stat containing the previous state of the file.
  * @return 1 if the file has been modified or the link target has changed, 0 otherwise.
  */
long file_is_modified(const char *inputfile, char **final_file, struct stat *input_stat) {
  struct stat filestat;
  char *tempfile = NULL;
  const char *tmpinput = NULL;

  if (!fexists(inputfile)) {
    fprintf(stderr, "%s file does not exist!\n", inputfile);
    return (1);
  }
  if (*final_file && !fexists(*final_file)) {
    fprintf(stderr, "linked file %s of inputfile %s does not exist!\n", *final_file, inputfile);
    return (1);
  }
  if (!input_stat) {
    fprintf(stderr, "The previous state of file %s is not known.\n", inputfile);
    return (1);
  }

  tempfile = read_last_link_to_file(inputfile);

  /*the final link name changed */
  if (tempfile && *final_file && strcmp(tempfile, *final_file)) {
    if (*final_file)
      free(*final_file);
    *final_file = tempfile;
    return 1;
  }
  if ((tempfile && !(*final_file)) || (!tempfile && *final_file)) {
    if (*final_file)
      free(*final_file);
    *final_file = tempfile;
    return 1;
  }
  /* the final link name did not change, check if the file state changed */
  if (tempfile)
    free(tempfile);
  if (*final_file)
    tmpinput = *final_file;
  else
    tmpinput = inputfile;
  filestat = *input_stat;
  if (stat(tmpinput, input_stat) != 0) {
    fprintf(stderr, "Problem getting modification time for %s\n", tmpinput);
    exit(1);
  }
  if (input_stat->st_ctime != filestat.st_ctime) {
    /* file state is changed */
    return 1;
  }
  return 0;
}
