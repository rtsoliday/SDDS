/**
 * @file test_sdds_enum_mpi.c
 * @brief Named multi-rank enum interoperability and collective failure regression.
 * @copyright Copyright (c) 2026 The University of Chicago.
 * @license Software License Agreement in LICENSE.
 * @author SDDS maintainers
 */
#include "SDDS.h"
#include "SDDS_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d rank %d: %s\n", __FILE__, __LINE__, rank, #x); SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors); MPI_Abort(MPI_COMM_WORLD, 1); exit(EXIT_FAILURE); } } while (0)
#define STAGE(message) do { fprintf(stderr, "enum-mpi rank %d (%d,%d): %s\n", rank, major, endian, message); fflush(stderr); } while (0)

/** @brief Exercise every public binary writer, including explicit byte swapping. */
static int32_t writeDirectPage(SDDS_DATASET *dataset, int api) {
  if (api == 0) return SDDS_MPI_WriteBinaryPage(dataset);
  if (api == 1) return SDDS_MPI_WriteContinuousBinaryPage(dataset);
  return SDDS_MPI_WriteNonNativeBinaryPage(dataset);
}

/** @brief Require direct page APIs to reject rank-local invalid codes collectively. */
static void testDirectPageAPIs(const char *directory, int rank, int ranks) {
  int major, endian, api, readApi, collective, page;
  char path[2048];
  for (major = 0; major < 2; major++) for (endian = 0; endian < 2; endian++) {
    int nonNative = endian != SDDS_IsBigEndianMachine();
#if defined(_WIN32)
    _putenv_s("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little");
#else
    setenv("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little", 1);
#endif
    for (api = 0; api < 3; api++) {
      SDDS_DATASET writer = {0};
      int32_t code = 10, shape = 1;
      MPI_Offset offset;
      SDDS_ENUM_DEFINITION *different = NULL, *original;
      char *originalText;
      if (api == 2 && !nonNative) continue;
      snprintf(path, sizeof(path), "%s/direct-%d-%d-%d.sdds", directory, major, endian, api);
      SDDS_MPI_Setup(&writer, 1, ranks, rank, MPI_COMM_WORLD, 0);
      CHECK(SDDS_MPI_InitializeOutput(&writer, NULL, NULL, path, SDDS_MPI_WRITE_ONLY, (short)major));
      CHECK(MPI_File_set_size(writer.MPI_dataset->MPI_file, 0) == MPI_SUCCESS);
      CHECK(SDDS_DefineEnumColumn(&writer, "directState", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", 0) == 0);
      CHECK(SDDS_DefineEnumParameter(&writer, "directParameter", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", NULL) == 0);
      CHECK(SDDS_DefineEnumArray(&writer, "directArray", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", 0, 1, NULL) == 0);
      writer.layout.byteOrderDeclared = endian ? SDDS_BIGENDIAN : SDDS_LITTLEENDIAN;
      CHECK(SDDS_MPI_WriteLayout(&writer));
      CHECK(SDDS_StartPage(&writer, 1));
      CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &code, 1, 0));
      if (!rank) CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, code));
      CHECK(SDDS_SetArray(&writer, "directArray", SDDS_CONTIGUOUS_DATA, &code, &shape));
      /* Non-master title data is deliberately invalid and is never serialized. */
      if (rank) ((int32_t *)writer.array[0].data)[0] = 42;
      offset = writer.MPI_dataset->file_offset;
      if (rank == 1) ((int32_t *)writer.data[0])[0] = 42;
      CHECK(!writeDirectPage(&writer, api));
      CHECK(writer.MPI_dataset->file_offset == offset && writer.MPI_dataset->n_page == 0);
      SDDS_ClearErrors();
      if (major) {
        if (rank == 1) writer.row_flag[0] = 0;
        CHECK(!writeDirectPage(&writer, api));
        SDDS_ClearErrors();
        if (rank == 1) writer.row_flag[0] = 1;
      }
      if (rank == 1) ((int32_t *)writer.data[0])[0] = code;
      if (!rank) *(int32_t *)writer.parameter[0] = 42;
      CHECK(!writeDirectPage(&writer, api)); SDDS_ClearErrors();
      if (!rank) {
        *(int32_t *)writer.parameter[0] = code;
        ((int32_t *)writer.array[0].data)[0] = 42;
      }
      CHECK(!writeDirectPage(&writer, api)); SDDS_ClearErrors();
      if (!rank) ((int32_t *)writer.array[0].data)[0] = code;
      original = writer.layout.column_definition[0].enum_definition;
      originalText = writer.layout.column_definition[0].enum_values;
      if (rank == 1) {
        different = SDDS_CreateEnumDefinition("10:FAULT,-1:ON");
        CHECK(different);
        writer.layout.column_definition[0].enum_definition = different;
        writer.layout.column_definition[0].enum_values = (char *)SDDS_EnumValues(different);
      }
      CHECK(!writeDirectPage(&writer, api)); SDDS_ClearErrors();
      writer.layout.column_definition[0].enum_definition = original;
      writer.layout.column_definition[0].enum_values = originalText;
      SDDS_FreeEnumDefinition(different);
      CHECK(writeDirectPage(&writer, api));
      CHECK(((int32_t *)writer.data[0])[0] == code);
      CHECK(SDDS_StartPage(&writer, 0));
      shape = 0;
      CHECK(SDDS_SetArray(&writer, "directArray", SDDS_CONTIGUOUS_DATA, &code, &shape));
      CHECK(writeDirectPage(&writer, api));
      CHECK(SDDS_Terminate(&writer));
      MPI_Barrier(MPI_COMM_WORLD);
      for (readApi = 0; readApi < (nonNative ? 2 : 1); readApi++) for (collective = 0; collective < 2; collective++) {
        SDDS_DATASET reader = {0};
        SDDS_MPI_Setup(&reader, 1, ranks, rank, MPI_COMM_WORLD, 1);
        CHECK(SDDS_MPI_InitializeInput(&reader, path));
        reader.MPI_dataset->collective_io = (short)collective;
        for (page = 0; page < 2; page++) {
          int64_t row;
          CHECK((readApi ? SDDS_MPI_ReadNonNativeBinaryPage(&reader) : SDDS_MPI_ReadBinaryPage(&reader)) == page + 1);
          CHECK(reader.n_rows == (page ? 0 : 1));
          for (row = 0; row < reader.n_rows; row++) CHECK(((int32_t *)reader.data[0])[row] == code);
          CHECK(*(int32_t *)reader.parameter[0] == code);
          CHECK(reader.array[0].elements == (page ? 0 : 1));
        }
        CHECK((readApi ? SDDS_MPI_ReadNonNativeBinaryPage(&reader) : SDDS_MPI_ReadBinaryPage(&reader)) == -1);
        CHECK(SDDS_Terminate(&reader));
      }
    }
    /* The invalid code is assigned only to rank one; every rank must reject it. */
    snprintf(path, sizeof(path), "%s/direct-invalid-%d-%d.sdds", directory, major, endian);
    if (!rank) {
      FILE *file = fopen(path, "wb");
      int32_t value = ranks;
      int row;
      CHECK(file);
      CHECK(fprintf(file, "SDDS6\n&column name=directState,type=enum,enum_values=\"10:ON,-1:FAULT\" &end\n&data mode=binary,endian=%s,column_major_order=%d &end\n", endian ? "big" : "little", major) > 0);
      if (nonNative) SDDS_SwapLong(&value);
      CHECK(fwrite(&value, sizeof(value), 1, file) == 1);
      for (row = 0; row < ranks; row++) {
        value = row == 1 ? 42 : 10;
        if (nonNative) SDDS_SwapLong(&value);
        CHECK(fwrite(&value, sizeof(value), 1, file) == 1);
      }
      CHECK(!fclose(file));
    }
    MPI_Barrier(MPI_COMM_WORLD);
    for (readApi = 0; readApi < (nonNative ? 2 : 1); readApi++) for (collective = 0; collective < 2; collective++) {
      SDDS_DATASET reader = {0};
      SDDS_MPI_Setup(&reader, 1, ranks, rank, MPI_COMM_WORLD, 1);
      CHECK(SDDS_MPI_InitializeInput(&reader, path));
      reader.MPI_dataset->collective_io = (short)collective;
      CHECK(!(readApi ? SDDS_MPI_ReadNonNativeBinaryPage(&reader) : SDDS_MPI_ReadBinaryPage(&reader)));
      CHECK(SDDS_Terminate(&reader)); SDDS_ClearErrors();
    }
  }
}

/** @brief Reject rank-local failures in optional and required header commands. */
static void testHeaderSerializationFailure(const char *directory, int rank, int ranks) {
  uint64_t layoutLimit = SDDS_GetEnumLayoutLimit(), mappingLimit = SDDS_GetEnumMappingLimit();
  char path[2048], description[1025];
  int command, failingRank;
  memset(description, 'x', sizeof(description) - 1);
  description[sizeof(description) - 1] = 0;
  for (command = 0; command < 2; command++) for (failingRank = 0; failingRank < 2; failingRank++) {
    SDDS_DATASET w = {0};
    int64_t offset, minimum, maximum;
    snprintf(path, sizeof(path), "%s/header-limit-%d-%d.sdds", directory, command, failingRank);
    SDDS_MPI_Setup(&w, 1, ranks, rank, MPI_COMM_WORLD, 0);
    CHECK(SDDS_MPI_InitializeOutput(&w, command ? NULL : description, NULL, path, SDDS_MPI_WRITE_ONLY, 0));
    CHECK(MPI_File_set_size(w.MPI_dataset->MPI_file, 0) == MPI_SUCCESS);
    CHECK(SDDS_DefineEnumColumn(&w, "state", NULL, NULL, NULL, NULL, "0:OFF,1:ON", 0) == 0);
    /* At 128 bytes the small column still fits, while the data command does not. */
    if (rank == failingRank) CHECK(SDDS_SetEnumReaderLimits(command ? 128 : 512, 64));
    CHECK(!SDDS_MPI_WriteLayout(&w));
    CHECK(!w.layout.layout_written);
    offset = (int64_t)w.MPI_dataset->file_offset;
    MPI_Allreduce(&offset, &minimum, 1, MPI_INT64_T, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&offset, &maximum, 1, MPI_INT64_T, MPI_MAX, MPI_COMM_WORLD);
    CHECK(minimum == maximum);
    CHECK(SDDS_SetEnumReaderLimits(layoutLimit, mappingLimit));
    SDDS_ClearErrors();
    CHECK(SDDS_Terminate(&w));
  }
}

/** @brief Position every rank using title data populated only on rank zero. */
static void testRootOnlyTitleData(const char *directory, int rank, int ranks) {
  int32_t sizes[] = {2, 0, 3}, values[] = {10, -1, 10};
  char *texts[] = {"short", "a longer title value", ""};
  char path[2048];
  int major, endian, page;
  for (major = 0; major < 2; major++) for (endian = 0; endian < 2; endian++) {
    SDDS_DATASET writer = {0};
#if defined(_WIN32)
    _putenv_s("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little");
#else
    setenv("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little", 1);
#endif
    snprintf(path, sizeof(path), "%s/root-only-title-%d-%d.sdds", directory, major, endian);
    SDDS_MPI_Setup(&writer, 1, ranks, rank, MPI_COMM_WORLD, 0);
    CHECK(SDDS_MPI_InitializeOutput(&writer, NULL, NULL, path, SDDS_MPI_WRITE_ONLY, (short)major));
    CHECK(MPI_File_set_size(writer.MPI_dataset->MPI_file, 0) == MPI_SUCCESS);
    CHECK(SDDS_DefineEnumColumn(&writer, "rootOnlyColumn", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", 0) == 0);
    CHECK(SDDS_DefineEnumParameter(&writer, "rootOnlyParameter", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", NULL) == 0);
    CHECK(SDDS_DefineParameter(&writer, "rootOnlyText", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) == 1);
    CHECK(SDDS_DefineEnumArray(&writer, "rootOnlyArray", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", 0, 1, NULL) == 0);
    CHECK(SDDS_WriteLayout(&writer));
    for (page = 0; page < 3; page++) {
      int32_t code = (rank + page) % 2 ? -1 : 10;
      MPI_Offset minimum, maximum;
      CHECK(SDDS_StartPage(&writer, 1));
      CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &code, 1, 0));
      if (!rank) {
        CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, values[page]));
        CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 1, texts[page]));
        CHECK(SDDS_SetArray(&writer, "rootOnlyArray", SDDS_CONTIGUOUS_DATA, values, sizes + page));
      }
      CHECK(SDDS_WritePage(&writer));
      MPI_Allreduce(&writer.MPI_dataset->file_offset, &minimum, 1, MPI_OFFSET, MPI_MIN, MPI_COMM_WORLD);
      MPI_Allreduce(&writer.MPI_dataset->file_offset, &maximum, 1, MPI_OFFSET, MPI_MAX, MPI_COMM_WORLD);
      CHECK(minimum == maximum);
    }
    CHECK(SDDS_Terminate(&writer));
    MPI_Barrier(MPI_COMM_WORLD);
    if (!rank) {
      SDDS_DATASET reader = {0};
      CHECK(SDDS_InitializeInput(&reader, path));
      for (page = 0; page < 3; page++) {
        int row;
        CHECK(SDDS_ReadPage(&reader) == page + 1);
        CHECK(reader.n_rows == ranks);
        for (row = 0; row < ranks; row++)
          CHECK(((int32_t *)reader.data[0])[row] == ((row + page) % 2 ? -1 : 10));
        CHECK(*(int32_t *)reader.parameter[0] == values[page]);
        CHECK(!strcmp(*(char **)reader.parameter[1], texts[page]));
        CHECK(reader.array[0].dimension[0] == sizes[page] && reader.array[0].elements == sizes[page]);
        if (sizes[page]) CHECK(!memcmp(reader.array[0].data, values, (size_t)sizes[page] * sizeof(*values)));
      }
      CHECK(SDDS_ReadPage(&reader) == -1);
      CHECK(SDDS_Terminate(&reader));
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
}

/** @brief Preserve empty enum and primitive array shapes, including a later zero dimension. */
static void testEmptyMultidimensionalArrays(const char *directory, int rank, int ranks) {
  int32_t shapes[][2] = {{2, 3}, {3, 0}, {2, 3}, {0, 3}};
  int32_t values[] = {0, 1, 0, 1, 0, 1}, row = 1;
  char path[2048];
  int major, endian, page, array;
  for (major = 0; major < 2; major++) for (endian = 0; endian < 2; endian++) {
    SDDS_DATASET reader = {0};
    snprintf(path, sizeof(path), "%s/empty-arrays-%d-%d.sdds", directory, major, endian);
#if defined(_WIN32)
    _putenv_s("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little");
#else
    setenv("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little", 1);
#endif
    if (!rank) {
      SDDS_DATASET writer = {0}, serial = {0};
      CHECK(SDDS_InitializeOutput(&writer, SDDS_BINARY, 1, NULL, NULL, path));
      CHECK(SDDS_DefineColumn(&writer, "emptyArrayRow", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 0);
      CHECK(SDDS_DefineEnumArray(&writer, "emptyStateArray", NULL, NULL, NULL, NULL, "0:OFF,1:ON", 0, 2, NULL) == 0);
      CHECK(SDDS_DefineArray(&writer, "emptyLongArray", NULL, NULL, NULL, NULL, SDDS_LONG, 0, 2, NULL) == 1);
      writer.layout.data_mode.column_major = (short)major;
      CHECK(SDDS_WriteLayout(&writer));
      for (page = 0; page < 4; page++) {
        CHECK(SDDS_StartPage(&writer, 1));
        CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_NAME, &row, 1, "emptyArrayRow"));
        /* The non-NULL source is unused for empty arrays. */
        CHECK(SDDS_SetArray(&writer, "emptyStateArray", SDDS_CONTIGUOUS_DATA, values, shapes[page]));
        CHECK(SDDS_SetArray(&writer, "emptyLongArray", SDDS_CONTIGUOUS_DATA, values, shapes[page]));
        CHECK(SDDS_WritePage(&writer));
      }
      CHECK(SDDS_Terminate(&writer));
      CHECK(SDDS_InitializeInput(&serial, path));
      for (page = 0; page < 4; page++) {
        CHECK(SDDS_ReadPage(&serial) > 0);
        for (array = 0; array < 2; array++) {
          CHECK(!memcmp(serial.array[array].dimension, shapes[page], sizeof(shapes[page])));
          CHECK(serial.array[array].elements == shapes[page][0] * shapes[page][1]);
        }
      }
      CHECK(SDDS_ReadPage(&serial) < 0);
      CHECK(SDDS_Terminate(&serial));
    }
    MPI_Barrier(MPI_COMM_WORLD);
    SDDS_MPI_Setup(&reader, 1, ranks, rank, MPI_COMM_WORLD, 0);
    CHECK(SDDS_MPI_InitializeInput(&reader, path));
    for (page = 0; page < 4; page++) {
      CHECK(SDDS_ReadPage(&reader) > 0);
      for (array = 0; array < 2; array++) {
        CHECK(!memcmp(reader.array[array].dimension, shapes[page], sizeof(shapes[page])));
        CHECK(reader.array[array].elements == shapes[page][0] * shapes[page][1]);
        if (reader.array[array].elements)
          CHECK(!memcmp(reader.array[array].data, values, sizeof(values)));
      }
    }
    CHECK(SDDS_ReadPage(&reader) < 0);
    CHECK(SDDS_Terminate(&reader));
  }
}

/** @brief Read repeated pages after appending enum fields absent from the input stream. */
static void testWriteOnlyDefinitions(const char *directory, int rank, int ranks) {
  int major, endian, collective, masterReader, page;
  int32_t values[] = {10, -1, 10, -1};
  char path[2048];
  for (major = 0; major < 2; major++) for (endian = 0; endian < 2; endian++) {
    snprintf(path, sizeof(path), "%s/write-only-%d-%d.sdds", directory, major, endian);
#if defined(_WIN32)
    _putenv_s("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little");
#else
    setenv("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little", 1);
#endif
    if (!rank) {
      SDDS_DATASET writer = {0};
      CHECK(SDDS_InitializeOutput(&writer, SDDS_BINARY, 1, NULL, NULL, path));
      CHECK(SDDS_DefineEnumColumn(&writer, "writeOnlyInputColumn", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", 0) == 0);
      CHECK(SDDS_DefineEnumParameter(&writer, "writeOnlyInputParameter", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", NULL) == 0);
      writer.layout.data_mode.column_major = (short)major;
      CHECK(SDDS_WriteLayout(&writer));
      for (page = 0; page < 2; page++) {
        CHECK(SDDS_StartPage(&writer, 4));
        CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, values, 4, 0));
        CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, (int32_t)10));
        CHECK(SDDS_WritePage(&writer));
      }
      CHECK(SDDS_Terminate(&writer));
    }
    MPI_Barrier(MPI_COMM_WORLD);
    for (collective = 0; collective < 2; collective++) for (masterReader = 0; masterReader < 2; masterReader++) {
      SDDS_DATASET reader = {0};
      MPI_DATASET mpi = {0};
      if (masterReader) {
        mpi.comm = MPI_COMM_WORLD; mpi.myid = rank; mpi.n_processors = ranks;
        CHECK(SDDS_Master_InitializeInput(&reader, &mpi, path));
      } else {
        SDDS_MPI_Setup(&reader, 1, ranks, rank, MPI_COMM_WORLD, 1);
        CHECK(SDDS_MPI_InitializeInput(&reader, path));
        reader.MPI_dataset->collective_io = (short)collective;
      }
      SDDS_DeferSavingLayout(&reader, 1);
      CHECK(SDDS_ProcessColumnString(&reader, "&column name=writeOnlyOutputColumn,type=enum,enum_values=\"10:ON\" &end", SDDS_WRITEONLY_DEFINITION));
      CHECK(SDDS_ProcessParameterString(&reader, "&parameter name=writeOnlyOutputParameter,type=enum,enum_values=\"10:ON\" &end", SDDS_WRITEONLY_DEFINITION));
      CHECK(SDDS_ProcessParameterString(&reader, "&parameter name=writeOnlyOutputText,type=string &end", SDDS_WRITEONLY_DEFINITION));
      SDDS_DeferSavingLayout(&reader, 0);
      CHECK(SDDS_SaveLayout(&reader));
      for (page = 0; page < 2; page++) {
        int64_t row, start;
        CHECK((masterReader ? SDDS_Master_ReadPage(&reader) : SDDS_ReadPage(&reader)) == page + 1);
        CHECK(*(int32_t *)reader.parameter[0] == 10 && *(int32_t *)reader.parameter[1] == 0);
        CHECK(!*(char **)reader.parameter[2] || !**(char **)reader.parameter[2]);
        start = masterReader ? 0 : reader.MPI_dataset->start_row;
        CHECK(reader.n_rows == (masterReader ? 4 : 4 / ranks + (rank < 4 % ranks)));
        for (row = 0; row < reader.n_rows; row++) {
          CHECK(((int32_t *)reader.data[0])[row] == values[start + row]);
          CHECK(((int32_t *)reader.data[1])[row] == 0);
        }
        CHECK(!SDDS_ValidateEnumPage(&reader)); SDDS_ClearErrors();
      }
      CHECK((masterReader ? SDDS_Master_ReadPage(&reader) : SDDS_ReadPage(&reader)) == -1);
      CHECK(SDDS_Terminate(&reader));
    }
  }
}

/** @brief Preserve complete serial pages through the master-reader MPI API. */
static void testMasterReader(const char *directory, int rank, int ranks) {
  const char *suffixes[] = {"sdds", "sdds.gz", "sdds.xz", "sdds.lzma"};
  int32_t codes[] = {10, -1, 10, -1};
  int32_t rowCounts[] = {3, 1, 0}, shapes[][2] = {{2, 2}, {1, 1}, {3, 0}};
  double numbers[] = {1.5, 2.5, 3.5};
  char longText[4097], path[2048];
  char *strings[] = {longText, "", "master", "array"};
  int enums, compression, mode, page;
  memset(longText, 'x', sizeof(longText) - 1);
  longText[sizeof(longText) - 1] = 0;
  for (enums = 0; enums < 2; enums++) for (compression = 0; compression < 4; compression++)
    for (mode = SDDS_BINARY; mode <= SDDS_ASCII; mode++) {
      SDDS_DATASET reader = {0};
      MPI_DATASET mpi = {0};
      mpi.comm = MPI_COMM_WORLD; mpi.myid = rank; mpi.n_processors = ranks;
      snprintf(path, sizeof(path), "%s/master-%d-%d.%s", directory, enums, mode, suffixes[compression]);
      if (!rank) {
        SDDS_DATASET writer = {0};
        CHECK(SDDS_InitializeOutput(&writer, mode, 1, NULL, NULL, path));
        if (enums) {
          CHECK(SDDS_DefineEnumColumn(&writer, "masterState", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", 0) == 0);
          CHECK(SDDS_DefineEnumParameter(&writer, "masterParameter", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", NULL) == 0);
          CHECK(SDDS_DefineEnumParameter(&writer, "masterFixed", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", "10") == 1);
          CHECK(SDDS_DefineEnumArray(&writer, "masterArray", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", 0, 2, NULL) == 0);
        } else {
          CHECK(SDDS_DefineColumn(&writer, "masterState", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 0);
          CHECK(SDDS_DefineParameter(&writer, "masterParameter", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) == 0);
          CHECK(SDDS_DefineParameter(&writer, "masterFixed", NULL, NULL, NULL, NULL, SDDS_LONG, "10") == 1);
          CHECK(SDDS_DefineArray(&writer, "masterArray", NULL, NULL, NULL, NULL, SDDS_LONG, 0, 2, NULL) == 0);
        }
        CHECK(SDDS_DefineColumn(&writer, "masterText", NULL, NULL, NULL, NULL, SDDS_STRING, 0) == 1);
        CHECK(SDDS_DefineColumn(&writer, "masterNumber", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == 2);
        CHECK(SDDS_DefineParameter(&writer, "masterTitle", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) == 2);
        CHECK(SDDS_DefineArray(&writer, "masterStrings", NULL, NULL, NULL, NULL, SDDS_STRING, 0, 2, NULL) == 1);
        CHECK(SDDS_WriteLayout(&writer));
        for (page = 0; page < 3; page++) {
          CHECK(SDDS_StartPage(&writer, rowCounts[page]));
          CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, (int32_t)-1));
          CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 2, longText));
          CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, codes, rowCounts[page], 0));
          CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, strings, rowCounts[page], 1));
          CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, numbers, rowCounts[page], 2));
          CHECK(SDDS_SetArray(&writer, "masterArray", SDDS_CONTIGUOUS_DATA, codes, shapes[page]));
          CHECK(SDDS_SetArray(&writer, "masterStrings", SDDS_CONTIGUOUS_DATA, strings, shapes[page]));
          CHECK(SDDS_WritePage(&writer));
        }
        CHECK(SDDS_Terminate(&writer));
      }
      MPI_Barrier(MPI_COMM_WORLD);
      if (mode == SDDS_ASCII) CHECK(SDDS_Master_InitializeInputFromSearchPath(&reader, &mpi, path));
      else CHECK(SDDS_Master_InitializeInput(&reader, &mpi, path));
      for (page = 0; page < 3; page++) {
        int64_t row;
        int32_t element;
        CHECK(SDDS_Master_ReadPage(&reader) == page + 1);
        CHECK(reader.n_rows == rowCounts[page]);
        CHECK(*(int32_t *)reader.parameter[0] == -1 && *(int32_t *)reader.parameter[1] == 10);
        CHECK(!strcmp(*(char **)reader.parameter[2], longText));
        for (row = 0; row < reader.n_rows; row++) {
          CHECK(((int32_t *)reader.data[0])[row] == codes[row]);
          CHECK(!strcmp(((char **)reader.data[1])[row], strings[row]));
          CHECK(((double *)reader.data[2])[row] == numbers[row]);
          CHECK(reader.row_flag[row]);
        }
        CHECK(!memcmp(reader.array[0].dimension, shapes[page], sizeof(shapes[page])));
        CHECK(!memcmp(reader.array[1].dimension, shapes[page], sizeof(shapes[page])));
        CHECK(reader.array[0].elements == shapes[page][0] * shapes[page][1]);
        CHECK(reader.array[1].elements == reader.array[0].elements);
        for (element = 0; element < reader.array[0].elements; element++) {
          CHECK(((int32_t *)reader.array[0].data)[element] == codes[element]);
          CHECK(!strcmp(((char **)reader.array[1].data)[element], strings[element]));
        }
        CHECK(SDDS_ValidateEnumPage(&reader));
      }
      CHECK(SDDS_Master_ReadPage(&reader) == -1);
      CHECK(SDDS_Terminate(&reader));
    }
  {
    SDDS_DATASET reader = {0};
    MPI_DATASET mpi = {0};
    mpi.comm = MPI_COMM_WORLD; mpi.myid = rank; mpi.n_processors = ranks;
    CHECK(SDDS_Master_InitializeInput(&reader, &mpi, "testdata/enums/invalid-value-42.sdds"));
    CHECK(!SDDS_Master_ReadPage(&reader));
    CHECK(SDDS_Terminate(&reader)); SDDS_ClearErrors();
    CHECK(!SDDS_Master_InitializeInput(&reader, &mpi, "testdata/enums/invalid-version5.sdds"));
    CHECK(SDDS_Terminate(&reader)); SDDS_ClearErrors();
    CHECK(!SDDS_Master_InitializeInputFromSearchPath(&reader, &mpi, "testdata/enums/no-such-master-input.sdds"));
    CHECK(SDDS_Terminate(&reader)); SDDS_ClearErrors();
  }
}

/** @brief Select the byte order used by later serial and MPI writers. */
static void setOutputEndian(int endian) {
#if defined(_WIN32)
  _putenv_s("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little");
#else
  setenv("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little", 1);
#endif
}

/** @brief Copy all but the last @p drop bytes of @p source (called by rank zero). */
static void copyTruncated(const char *source, const char *target, long drop, int rank) {
  FILE *in, *out;
  char *bytes;
  long size;
  CHECK((in = fopen(source, "rb")));
  CHECK(!fseek(in, 0, SEEK_END) && (size = ftell(in)) > drop && !fseek(in, 0, SEEK_SET));
  CHECK((bytes = malloc((size_t)size)));
  CHECK(fread(bytes, 1, (size_t)size, in) == (size_t)size);
  CHECK(!fclose(in));
  CHECK((out = fopen(target, "wb")));
  CHECK(fwrite(bytes, 1, (size_t)(size - drop), out) == (size_t)(size - drop));
  CHECK(!fclose(out));
  free(bytes);
}

/**
 * @brief Read SDDS1-5 pages in both orders, and fail or recover on every rank together
 * when only the last rank's share of a page is truncated.
 */
static void testLegacyLayoutAndTruncation(const char *directory, int rank, int ranks) {
  char path[2048], truncated[2048];
  int major, endian, collective, recover;
  int64_t total = 4 * (int64_t)ranks, row;
  for (major = 0; major < 2; major++) for (endian = 0; endian < 2; endian++) {
    int32_t previous;
    snprintf(path, sizeof(path), "%s/legacy-%d-%d.sdds", directory, major, endian);
    snprintf(truncated, sizeof(truncated), "%s/legacy-truncated-%d-%d.sdds", directory, major, endian);
    setOutputEndian(endian);
    if (!rank) {
      SDDS_DATASET writer = {0};
      double *values = malloc(sizeof(*values) * (size_t)total);
      CHECK(values);
      for (row = 0; row < total; row++) values[row] = row + 0.5;
      CHECK(SDDS_InitializeOutput(&writer, SDDS_BINARY, 1, NULL, NULL, path));
      CHECK(SDDS_DefineColumn(&writer, "legacyValue", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == 0);
      writer.layout.data_mode.column_major = (short)major;
      CHECK(SDDS_WriteLayout(&writer));
      CHECK(SDDS_StartPage(&writer, total));
      CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, values, total, 0));
      CHECK(SDDS_WritePage(&writer));
      CHECK(SDDS_Terminate(&writer));
      free(values);
      /* Leave the last rank 12 of its 32 bytes: one complete value. */
      copyTruncated(path, truncated, 20, rank);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    for (collective = 0; collective < 2; collective++) {
      SDDS_DATASET reader = {0};
      SDDS_MPI_Setup(&reader, 1, ranks, rank, MPI_COMM_WORLD, 1);
      CHECK(SDDS_MPI_InitializeInput(&reader, path));
      /* Non-root ranks learn the data order from the SDDS1-5 layout broadcast. */
      CHECK(reader.layout.version < 6 && reader.layout.data_mode.column_major == major);
      reader.MPI_dataset->collective_io = (short)collective;
      CHECK(SDDS_MPI_ReadBinaryPage(&reader) == 1);
      CHECK(reader.n_rows == 4);
      for (row = 0; row < reader.n_rows; row++)
        CHECK(((double *)reader.data[0])[row] == reader.MPI_dataset->start_row + row + 0.5);
      CHECK(SDDS_MPI_ReadBinaryPage(&reader) == -1);
      CHECK(SDDS_Terminate(&reader));
    }
    /* An 8-byte buffer makes the collective row reader refill once per value, so the
     * short rank would issue fewer MPI_File_read_all calls if it stopped early. */
    previous = SDDS_SetDefaultReadBufferSize(8);
    for (collective = 0; collective < 2; collective++) for (recover = 0; recover < 2; recover++) {
      SDDS_DATASET reader = {0};
      /* Auto-recovery applies to independent row-by-row reads. */
      if (recover && (major || collective)) continue;
      SDDS_MPI_Setup(&reader, 1, ranks, rank, MPI_COMM_WORLD, 1);
      CHECK(SDDS_MPI_InitializeInput(&reader, truncated));
      reader.MPI_dataset->collective_io = (short)collective;
      if (recover) CHECK(SDDS_SetAutoReadRecovery(&reader, SDDS_AUTOREADRECOVER));
      CHECK(SDDS_MPI_ReadBinaryPage(&reader) == (recover ? 1 : 0));
      if (recover) {
        CHECK(reader.n_rows == (rank == ranks - 1 ? 1 : 4));
        CHECK(SDDS_MPI_ReadBinaryPage(&reader) == -1);
      }
      SDDS_ClearErrors();
      CHECK(SDDS_Terminate(&reader));
    }
    SDDS_SetDefaultReadBufferSize(previous);
  }
}

/** @brief Write only the selected rows in row-major order, independently and collectively. */
static void testSelectedRowMajorRows(const char *directory, int rank, int ranks) {
  char path[2048];
  int endian, collective, page;
  for (endian = 0; endian < 2; endian++) for (collective = 0; collective < 2; collective++) {
    SDDS_DATASET writer = {0};
    snprintf(path, sizeof(path), "%s/selected-rows-%d-%d.sdds", directory, endian, collective);
    setOutputEndian(endian);
    SDDS_MPI_Setup(&writer, 1, ranks, rank, MPI_COMM_WORLD, 0);
    CHECK(SDDS_MPI_InitializeOutput(&writer, NULL, NULL, path, SDDS_MPI_WRITE_ONLY, 0));
    CHECK(MPI_File_set_size(writer.MPI_dataset->MPI_file, 0) == MPI_SUCCESS);
    writer.MPI_dataset->collective_io = (short)collective;
    CHECK(SDDS_DefineColumn(&writer, "selectedValue", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == 0);
    CHECK(SDDS_MPI_WriteLayout(&writer));
    for (page = 0; page < 2; page++) {
      double values[3];
      int row;
      for (row = 0; row < 3; row++) values[row] = 100.0 * page + 10.0 * rank + row;
      CHECK(SDDS_StartPage(&writer, 3));
      CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, values, 3, 0));
      /* Rank one drops its first row, so the ranks write different row counts. */
      if (rank == 1) writer.row_flag[0] = 0;
      CHECK(SDDS_WritePage(&writer));
    }
    CHECK(SDDS_Terminate(&writer));
    MPI_Barrier(MPI_COMM_WORLD);
    if (!rank) {
      SDDS_DATASET serial = {0};
      CHECK(SDDS_InitializeInput(&serial, path));
      for (page = 0; page < 2; page++) {
        int64_t row = 0;
        int source, k;
        CHECK(SDDS_ReadPage(&serial) == page + 1);
        CHECK(serial.n_rows == 3 * ranks - 1);
        for (source = 0; source < ranks; source++)
          for (k = source == 1; k < 3; k++, row++)
            CHECK(((double *)serial.data[0])[row] == 100.0 * page + 10.0 * source + k);
      }
      CHECK(SDDS_ReadPage(&serial) == -1);
      CHECK(SDDS_Terminate(&serial));
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
}

/** @brief Read MPI-written string rows in native and swapped byte order. */
static void testStringRows(const char *directory, int rank, int ranks) {
  char path[2048], expected[64];
  int endian, single, collective;
  for (endian = 0; endian < 2; endian++) for (single = 0; single < 2; single++) {
    /* Either two rows per rank, or one row in total, which the collective reader
     * then reads outside its (empty) collective part. */
    SDDS_DATASET writer = {0};
    int32_t rows = single ? !rank : 2, row;
    snprintf(path, sizeof(path), "%s/string-rows-%d-%d.sdds", directory, endian, single);
    setOutputEndian(endian);
    SDDS_MPI_Setup(&writer, 1, ranks, rank, MPI_COMM_WORLD, 0);
    CHECK(SDDS_MPI_InitializeOutput(&writer, NULL, NULL, path, SDDS_MPI_WRITE_ONLY, 0));
    CHECK(MPI_File_set_size(writer.MPI_dataset->MPI_file, 0) == MPI_SUCCESS);
    CHECK(SDDS_DefineColumn(&writer, "rowText", NULL, NULL, NULL, NULL, SDDS_STRING, 0) == 0);
    CHECK(SDDS_DefineColumn(&writer, "rowNumber", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 1);
    CHECK(SDDS_MPI_WriteLayout(&writer));
    CHECK(SDDS_StartPage(&writer, rows));
    for (row = 0; row < rows; row++) {
      snprintf(expected, sizeof(expected), "rank%d-row%d", rank, (int)row);
      CHECK(SDDS_SetRowValues(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, row, 0, expected, 1, (int32_t)(rank * 10 + row), -1));
    }
    CHECK(SDDS_WritePage(&writer));
    CHECK(SDDS_Terminate(&writer));
    MPI_Barrier(MPI_COMM_WORLD);
    for (collective = 0; collective < 2; collective++) {
      SDDS_DATASET reader = {0};
      SDDS_MPI_Setup(&reader, 1, ranks, rank, MPI_COMM_WORLD, 1);
      CHECK(SDDS_MPI_InitializeInput(&reader, path));
      reader.MPI_dataset->collective_io = (short)collective;
      if (collective && !single) {
        /* Collective row reads reject string columns on every rank. */
        CHECK(!SDDS_MPI_ReadBinaryPage(&reader));
        SDDS_ClearErrors();
      } else {
        int64_t index;
        CHECK(SDDS_MPI_ReadBinaryPage(&reader) == 1);
        CHECK(reader.n_rows == (single ? !rank : 2));
        for (index = 0; index < reader.n_rows; index++) {
          int64_t global = reader.MPI_dataset->start_row + index;
          int source = (int)(global / 2);
          char *text = ((char **)reader.data[0])[index];
          size_t length;
          snprintf(expected, sizeof(expected), "rank%d-row%d", source, (int)(global % 2));
          length = strlen(expected);
          /* The MPI writer pads strings to the default string length. */
          CHECK(!strncmp(text, expected, length) && strspn(text + length, " ") == strlen(text + length));
          CHECK(((int32_t *)reader.data[1])[index] == source * 10 + (int32_t)(global % 2));
        }
        CHECK(SDDS_MPI_ReadBinaryPage(&reader) == -1);
      }
      CHECK(SDDS_Terminate(&reader));
    }
  }
}

/** @brief Primitive layouts still need SDDS6 when their metadata exceeds legacy escaping/buffers. */
static void testPrimitiveMetadata(const char *directory, int rank, int ranks) {
  int major;
  char path[2048], description[4097];
  memset(description, 'x', sizeof(description) - 1);
  memcpy(description, "line one\nstate! ", 16);
  description[sizeof(description) - 1] = 0;
  for (major = 0; major < 2; major++) {
    SDDS_DATASET writer = {0}, reader = {0};
    int32_t value = rank;
    snprintf(path, sizeof(path), "%s/primitive-metadata-%d.sdds", directory, major);
    SDDS_MPI_Setup(&writer, 1, ranks, rank, MPI_COMM_WORLD, 0);
    CHECK(SDDS_MPI_InitializeOutput(&writer, NULL, NULL, path, SDDS_MPI_WRITE_ONLY, (short)major));
    CHECK(MPI_File_set_size(writer.MPI_dataset->MPI_file, 0) == MPI_SUCCESS);
    CHECK(SDDS_DefineColumn(&writer, "value", NULL, NULL, description, NULL, SDDS_LONG, 0) == 0);
    CHECK(SDDS_MPI_WriteLayout(&writer) && writer.layout.version == 6);
    CHECK(SDDS_StartPage(&writer, 1));
    CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &value, 1, 0));
    CHECK(SDDS_WritePage(&writer));
    CHECK(SDDS_Terminate(&writer));
    MPI_Barrier(MPI_COMM_WORLD);
    SDDS_MPI_Setup(&reader, 1, ranks, rank, MPI_COMM_WORLD, 1);
    CHECK(SDDS_MPI_InitializeInput(&reader, path));
    CHECK(reader.layout.version == 6 && !strcmp(reader.layout.column_definition[0].description, description));
    CHECK(SDDS_ReadPage(&reader) == 1 && reader.n_rows == 1);
    CHECK(((int32_t *)reader.data[0])[0] == rank);
    CHECK(SDDS_ReadPage(&reader) == -1);
    CHECK(SDDS_Terminate(&reader));
  }
}

/** @brief Keep accepting *.gz MPI output names, written uncompressed, and reject *.xz. */
static void testCompressedOutputNames(const char *directory, int rank, int ranks) {
  char path[2048];
  SDDS_DATASET writer = {0};
  double value = rank;
  snprintf(path, sizeof(path), "%s/uncompressed.sdds.xz", directory);
  SDDS_MPI_Setup(&writer, 1, ranks, rank, MPI_COMM_WORLD, 0);
  CHECK(!SDDS_MPI_InitializeOutput(&writer, NULL, NULL, path, SDDS_MPI_WRITE_ONLY, 0));
  SDDS_ClearErrors();
  free(writer.MPI_dataset);
  snprintf(path, sizeof(path), "%s/uncompressed.sdds.gz", directory);
  SDDS_MPI_Setup(&writer, 1, ranks, rank, MPI_COMM_WORLD, 0);
  CHECK(SDDS_MPI_InitializeOutput(&writer, NULL, NULL, path, SDDS_MPI_WRITE_ONLY, 0));
  CHECK(MPI_File_set_size(writer.MPI_dataset->MPI_file, 0) == MPI_SUCCESS);
  CHECK(SDDS_DefineColumn(&writer, "gzValue", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == 0);
  CHECK(SDDS_MPI_WriteLayout(&writer));
  CHECK(SDDS_StartPage(&writer, 1));
  CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &value, 1, 0));
  CHECK(SDDS_WritePage(&writer));
  CHECK(SDDS_Terminate(&writer));
  MPI_Barrier(MPI_COMM_WORLD);
  if (!rank) {
    SDDS_DATASET serial = {0};
    int source;
    /* zlib reads the uncompressed file transparently. */
    CHECK(SDDS_InitializeInput(&serial, path));
    CHECK(SDDS_ReadPage(&serial) == 1 && serial.n_rows == ranks);
    for (source = 0; source < ranks; source++) CHECK(((double *)serial.data[0])[source] == source);
    CHECK(SDDS_Terminate(&serial));
  }
  MPI_Barrier(MPI_COMM_WORLD);
}

/** @brief Run serial/MPI round trips and test rejection on a single failing rank. */
#define RUN(test) do { if (!only || !strcmp(only, #test)) test(argv[1], rank, ranks); } while (0)
int main(int argc, char **argv) {
  int rank, ranks, major, endian;
  const char *only;
  char path[2048];
  char mapping[32768] = "0:OFF,10:ON,-1:FAULT", description[4097];
  size_t used = strlen(mapping);
  int state;
  for (state = 1000; state < 2000; state++) used += (size_t)snprintf(mapping + used, sizeof(mapping) - used, ",%d:State%d", state, state);
  memset(description, 'x', sizeof(description) - 1); description[sizeof(description) - 1] = 0;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &ranks);
  /* An optional second argument runs one named test function (or "main") only. */
  CHECK((argc == 2 || argc == 3) && ranks >= 2);
  only = argc == 3 ? argv[2] : NULL;
  for (major = 0; major < 2 && (!only || !strcmp(only, "main")); major++) for (endian = 0; endian < 2; endian++) {
    SDDS_DATASET w = {0}, r = {0};
    int32_t values[] = {0, 10, -1}, dimensions[] = {3};
    SDDS_ENUM_DEFINITION *different = NULL, *old;
    char *oldText;
    snprintf(path, sizeof(path), "%s/enum-mpi-%d-%d.sdds", argv[1], major, endian);
    STAGE("write");
    SDDS_MPI_Setup(&w, 1, ranks, rank, MPI_COMM_WORLD, 0);
    CHECK(SDDS_MPI_InitializeOutput(&w, NULL, NULL, path, SDDS_MPI_WRITE_ONLY, (short)major));
    CHECK(MPI_File_set_size(w.MPI_dataset->MPI_file, 0) == MPI_SUCCESS);
    CHECK(SDDS_DefineEnumColumn(&w, "state", NULL, NULL, description, NULL, mapping, 0) == 0);
    CHECK(SDDS_DefineEnumParameter(&w, "stateParameter", NULL, NULL, NULL, NULL, "10:ON,-1:FAULT", NULL) == 0);
    CHECK(SDDS_DefineEnumArray(&w, "stateArray", NULL, NULL, NULL, NULL, mapping, 0, 1, NULL) == 0);
#if defined(_WIN32)
  _putenv_s("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little");
#else
  setenv("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little", 1);
#endif
    w.layout.byteOrderDeclared = endian ? SDDS_BIGENDIAN : SDDS_LITTLEENDIAN;
    CHECK(SDDS_MPI_WriteLayout(&w));
    CHECK(w.layout.version == 6);
    CHECK(SDDS_StartPage(&w, 3));
    CHECK(SDDS_SetColumn(&w, SDDS_SET_BY_NAME, values, 3, "state"));
    /* Only the writer of title data needs valid output parameters and arrays. */
    if (!rank) CHECK(SDDS_SetParameter(&w, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "stateParameter", (int32_t)-1));
    CHECK(SDDS_SetArray(&w, "stateArray", SDDS_CONTIGUOUS_DATA, values, dimensions));
    if (rank) ((int32_t *)w.array[0].data)[0] = 42;
    if (rank == 1) ((int32_t *)w.data[0])[0] = 42;
    CHECK(!SDDS_WritePage(&w));
    SDDS_ClearErrors();
    /* Row-major output skips deselected rows; column-major output writes them. */
    if (rank == 1) w.row_flag[0] = 0;
    if (major) CHECK(!SDDS_WritePage(&w));
    else CHECK(SDDS_MPI_ValidateEnumPage(&w));
    SDDS_ClearErrors();
    if (rank == 1) w.row_flag[0] = 1;
    if (rank == 1) ((int32_t *)w.data[0])[0] = 0;
    if (!rank) *(int32_t *)w.parameter[0] = 42;
    CHECK(!SDDS_WritePage(&w));
    SDDS_ClearErrors();
    if (!rank) {
      *(int32_t *)w.parameter[0] = -1;
      ((int32_t *)w.array[0].data)[0] = 42;
    }
    CHECK(!SDDS_WritePage(&w));
    SDDS_ClearErrors();
    if (!rank) ((int32_t *)w.array[0].data)[0] = 0;
    old = w.layout.column_definition[0].enum_definition;
    oldText = w.layout.column_definition[0].enum_values;
    if (rank == 1) {
      different = SDDS_CreateEnumDefinition("0:ON,10:OFF,-1:FAULT");
      CHECK(different);
      w.layout.column_definition[0].enum_definition = different;
      w.layout.column_definition[0].enum_values = (char *)SDDS_EnumValues(different);
    }
    CHECK(!SDDS_MPI_CheckEnumLayout(&w));
    SDDS_ClearErrors();
    w.layout.column_definition[0].enum_definition = old;
    w.layout.column_definition[0].enum_values = oldText;
    SDDS_FreeEnumDefinition(different);
    CHECK(SDDS_WritePage(&w));
    CHECK(SDDS_Terminate(&w));
    STAGE("serial read");
    MPI_Barrier(MPI_COMM_WORLD);
    if (!rank) {
      SDDS_DATASET serial = {0};
      CHECK(SDDS_InitializeInput(&serial, path));
      CHECK(SDDS_ReadPage(&serial) > 0);
      CHECK(serial.n_rows == 3 * ranks);
      CHECK(SDDS_ValidateEnumPage(&serial));
      CHECK(*(int32_t *)serial.parameter[0] == -1);
      CHECK(serial.array[0].elements == 3);
      CHECK(!memcmp(serial.array[0].data, values, sizeof(values)));
      CHECK(SDDS_Terminate(&serial));
    }
    MPI_Barrier(MPI_COMM_WORLD);
    SDDS_MPI_Setup(&r, 1, ranks, rank, MPI_COMM_WORLD, 0);
    STAGE("MPI input header");
    CHECK(SDDS_MPI_InitializeInput(&r, path));
    CHECK(!strcmp(SDDS_EnumCodeToLabel(r.layout.column_definition[0].enum_definition, 10), "ON"));
    CHECK(strlen(r.layout.column_definition[0].description) == 4096);
    CHECK(!strcmp(r.layout.column_definition[0].enum_values, mapping));
    STAGE("MPI read page");
    CHECK(SDDS_ReadPage(&r) > 0);
    CHECK(SDDS_ValidateEnumPage(&r));
    CHECK(*(int32_t *)r.parameter[0] == -1);
    CHECK(r.array[0].elements == 3);
    CHECK(!memcmp(r.array[0].data, values, sizeof(values)));
    CHECK(SDDS_Terminate(&r));
    STAGE("complete");
  }
  RUN(testEmptyMultidimensionalArrays);
  RUN(testRootOnlyTitleData);
  RUN(testHeaderSerializationFailure);
  RUN(testWriteOnlyDefinitions);
  RUN(testMasterReader);
  RUN(testDirectPageAPIs);
  RUN(testLegacyLayoutAndTruncation);
  RUN(testSelectedRowMajorRows);
  RUN(testStringRows);
  RUN(testPrimitiveMetadata);
  RUN(testCompressedOutputNames);
  if (!rank) puts(only ? "Selected SDDS MPI test passed" : "SDDS6 MPI enum tests passed");
  MPI_Finalize();
  return 0;
}
