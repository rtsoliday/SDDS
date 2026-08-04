/**
 * @file SDDS3Legacy.h
 * @brief Deprecated source-compatibility facade for the original SDDS3 C++ API.
 *
 * @details Declares a limited SDDSFile adapter for common migration call sites.
 * It intentionally omits public storage, raw allocation modes, and uncommon
 * historical methods that have no safe modern equivalent.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 */

#ifndef SDDS3_LEGACY_HPP
#define SDDS3_LEGACY_HPP

#include "SDDS.hpp"
#include "SDDStypes.h"

#include <cstdint>
#include <cstdio>
#include <memory>

#ifndef SDDS_BINARY
#  define SDDS_BINARY 1
#endif
#ifndef SDDS_ASCII
#  define SDDS_ASCII 2
#endif

/**
 * @brief Compatibility adapter for code written against the historical SDDSFile class.
 *
 * The adapter preserves the commonly used method interface, but intentionally does not
 * preserve the old object layout or its public data members. New code should use
 * sdds::Reader, sdds::Writer, sdds::Layout, and sdds::Page directly.
 */
class [[deprecated("use the SDDS++ API in SDDS.hpp")]] SDDSPP_API SDDSFile {
 public:
  /** @name Construction */
  /** @{ */
  /** @brief Creates an unconfigured compatibility object. */
  SDDSFile();
  /** @brief Creates an object for a file. @param filename File path. */
  explicit SDDSFile(char *filename);
  /** @brief Creates an object for a file. @param filename File path. */
  explicit SDDSFile(const char *filename);
  /** @brief Creates an object with an initial data mode. @param binary True for binary data. */
  explicit SDDSFile(bool binary);
  /** @brief Creates an object for a file and data mode. @param filename File path. @param binary True for binary data. */
  SDDSFile(char *filename, bool binary);
  /** @brief Creates an object for a file and data mode. @param filename File path. @param binary True for binary data. */
  SDDSFile(const char *filename, bool binary);
  /** @brief Closes any open reader or writer and destroys the object. */
  ~SDDSFile();
  /** @} */

  SDDSFile(const SDDSFile &) = delete;
  SDDSFile &operator=(const SDDSFile &) = delete;

  /**
   * @name File lifecycle
   * Functions returning int32_t use the historical convention of nonzero for
   * success and zero for failure; failure details are retained by the adapter.
   */
  /** @{ */
  int32_t initializeInput(char *filename);
  int32_t initializeInput(const char *filename);
  int32_t initializeOutput(int32_t dataMode, int32_t linesPerRow,
                           char *description, char *contents, char *filename);
  int32_t openInputFile();
  int32_t openOutputFile();
  int32_t closeFile();
  int32_t readLayout();
  int32_t readFile();
  int32_t readPage();
  int32_t readPages();
  int32_t writeLayout();
  int32_t writeFile();
  int32_t writePage(uint32_t page);
  int32_t writePages();
  int32_t writePages(uint32_t startPage, uint32_t endPage);
  /** @} */

  /** @name Error handling */
  /** @{ */
  int32_t checkForErrors();
  void clearErrors();
  void printErrors(FILE *file, int32_t mode);
  void printErrors();
  void setError(char *text);
  int32_t readRecoveryPossible() const;
  /** @} */

  /** @name Layout and encoding options */
  /** @{ */
  void setFileName(char *filename);
  void setFileName(const char *filename);
  uint32_t getDataMode() const;
  void setDataMode(uint32_t mode);
  void setAsciiMode();
  void setBinaryMode();
  void setColumnMajorOrder();
  void setRowMajorOrder();
  void setNativeEndian();
  void setNonNativeEndian();
  void setNoRowCount();
  void setUseRowCount();
  void setReadRecoveryMode(int32_t mode);
  void setLayoutVersion(int32_t version);
  void setDescription(char *text, char *contents);
  int32_t getDescription(char **text, char **contents);
  int32_t readVersion() const;
  /** @} */

  /** @name Page information */
  /** @{ */
  uint32_t pageCount() const;
  uint32_t rowCount(uint32_t page) const;
  void freePage();
  /** @} */

  /**
   * @name Field definitions
   * Overloads accept a numeric or textual SDDS type; detailed overloads also
   * accept the metadata serialized in the corresponding namelist command.
   */
  /** @{ */
  int32_t defineParameter(char *name, int32_t type);
  int32_t defineParameter(char *name, char *type);
  int32_t defineParameter(char *name, char *symbol, char *units, char *description,
                          char *formatString, int32_t type, char *fixedValue);
  int32_t defineColumn(char *name, int32_t type);
  int32_t defineColumn(char *name, char *type);
  int32_t defineColumn(char *name, char *symbol, char *units, char *description,
                       char *formatString, int32_t type, uint32_t fieldLength);
  int32_t defineArray(char *name, int32_t type, uint32_t dimensions);
  int32_t defineArray(char *name, char *type, uint32_t dimensions);
  int32_t defineArray(char *name, char *symbol, char *units, char *description,
                      char *formatString, char *groupName, int32_t type,
                      uint32_t fieldLength, uint32_t dimensions);
  /** @} */

  /** @name Field inspection */
  /** @{ */
  int32_t getParameterCount() const;
  int32_t getColumnCount() const;
  int32_t getArrayCount() const;
  int32_t getParameterIndex(char *name) const;
  int32_t getColumnIndex(char *name) const;
  int32_t getArrayIndex(char *name) const;
  char *getParameterName(int32_t index);
  char *getColumnName(int32_t index);
  char *getArrayName(int32_t index);
  int32_t getParameterType(int32_t index) const;
  int32_t getColumnType(int32_t index) const;
  int32_t getArrayType(int32_t index) const;
  /** @} */

  /**
   * @name Parameter assignment
   * The page argument is one-based, and fields may be selected by index or name.
   */
  /** @{ */
  int32_t setParameter(uint32_t page, int32_t index, int32_t value);
  int32_t setParameter(uint32_t page, int32_t index, uint32_t value);
  int32_t setParameter(uint32_t page, int32_t index, double value);
  int32_t setParameter(uint32_t page, int32_t index, char *value);
  int32_t setParameter(uint32_t page, char *name, int32_t value);
  int32_t setParameter(uint32_t page, char *name, uint32_t value);
  int32_t setParameter(uint32_t page, char *name, double value);
  int32_t setParameter(uint32_t page, char *name, char *value);
  /** @} */

  /**
   * @name Column assignment
   * Values are copied into the named or indexed column beginning at startRow.
   */
  /** @{ */
  int32_t setColumn(int32_t index, uint32_t page, uint32_t startRow,
                    int16_t *values, uint32_t rows);
  int32_t setColumn(int32_t index, uint32_t page, uint32_t startRow,
                    uint16_t *values, uint32_t rows);
  int32_t setColumn(int32_t index, uint32_t page, uint32_t startRow,
                    int32_t *values, uint32_t rows);
  int32_t setColumn(int32_t index, uint32_t page, uint32_t startRow,
                    uint32_t *values, uint32_t rows);
  int32_t setColumn(int32_t index, uint32_t page, uint32_t startRow,
                    float *values, uint32_t rows);
  int32_t setColumn(int32_t index, uint32_t page, uint32_t startRow,
                    double *values, uint32_t rows);
  int32_t setColumn(int32_t index, uint32_t page, uint32_t startRow,
                    char **values, uint32_t rows);
  int32_t setColumn(int32_t index, uint32_t page, uint32_t startRow,
                    char *values, uint32_t rows);

  int32_t setColumn(char *name, uint32_t page, uint32_t startRow,
                    int32_t *values, uint32_t rows);
  int32_t setColumn(char *name, uint32_t page, uint32_t startRow,
                    uint32_t *values, uint32_t rows);
  int32_t setColumn(char *name, uint32_t page, uint32_t startRow,
                    double *values, uint32_t rows);
  int32_t setColumn(char *name, uint32_t page, uint32_t startRow,
                    char **values, uint32_t rows);
  /** @} */

  /**
   * @name Value access
   * The compatibility adapter owns returned strings and vectors; callers must
   * not release them and should treat them as invalidated by later mutations.
   */
  /** @{ */
  int32_t getParameterInInt32(int32_t index, uint32_t page);
  int32_t getParameterInInt32(char *name, uint32_t page);
  uint32_t getParameterInUInt32(int32_t index, uint32_t page);
  uint32_t getParameterInUInt32(char *name, uint32_t page);
  double getParameterInDouble(int32_t index, uint32_t page);
  double getParameterInDouble(char *name, uint32_t page);
  char *getParameterInString(int32_t index, uint32_t page);
  char *getParameterInString(char *name, uint32_t page);
  int32_t *getColumnInInt32(int32_t index, uint32_t page);
  int32_t *getColumnInInt32(char *name, uint32_t page);
  uint32_t *getColumnInUInt32(int32_t index, uint32_t page);
  uint32_t *getColumnInUInt32(char *name, uint32_t page);
  double *getColumnInDouble(int32_t index, uint32_t page);
  double *getColumnInDouble(char *name, uint32_t page);
  char **getColumnInString(int32_t index, uint32_t page);
  char **getColumnInString(char *name, uint32_t page);
  void *getInternalColumn(int32_t index, uint32_t page);
  void *getInternalColumn(char *name, uint32_t page);
  /** @} */

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
