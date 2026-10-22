/**
 * @file test_sdds_enum.c
 * @brief SDDS6 dictionary, ownership, encoding and validation regression tests.
 * @copyright Copyright (c) 2026 The University of Chicago.
 * @license Software License Agreement in LICENSE.
 * @author SDDS maintainers
 */
#include "SDDS.h"
#include "SDDS_internal.h"
#if defined(RPN_SUPPORT)
#include "rpn.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors); abort(); } } while (0)
static const char *mapping = "0:OFF,10:ON,-1:FAULT,20:space \\, colon\\: quote\\\" slash\\\\ newline\\n tab\\t snow☃";

/** @brief Change one test environment variable portably, including removal. */
static void setTestEnvironment(const char *name, const char *value) {
#if defined(_WIN32)
  CHECK(_putenv_s(name, value ? value : "") == 0);
#else
  if (value) CHECK(setenv(name, value, 1) == 0);
  else CHECK(unsetenv(name) == 0);
#endif
}

/** @brief Finalize both pages in the byte order pinned by the SDDS6 header. */
static void fixedRowCountByteOrderTests(const char *directory) {
  char *savedEndian = NULL;
  int useEnum, major, bigEndian, environment, page;
  CHECK(SDDS_CopyString(&savedEndian, getenv("SDDS_OUTPUT_ENDIANESS")));
  for (useEnum = 0; useEnum < 2; useEnum++)
    for (major = 0; major < 2; major++)
      for (bigEndian = 0; bigEndian < 2; bigEndian++)
        for (environment = 0; environment < (useEnum ? 3 : 1); environment++) {
          SDDS_DATASET writer = {0}, reader = {0};
          int32_t codes[][3] = {{10, -1, 0}, {-1, 10, 0}};
          int64_t offsets[2];
          char path[2048];
          FILE *file;
          setTestEnvironment("SDDS_OUTPUT_ENDIANESS", bigEndian ? "big" : "little");
          snprintf(path, sizeof(path), "%s/api/fixed-row-order-%d-%d-%d-%d.sdds",
                   directory, useEnum, major, bigEndian, environment);
          CHECK(SDDS_InitializeOutput(&writer, SDDS_BINARY, 1, NULL, NULL, path));
          if (useEnum)
            CHECK(SDDS_DefineEnumColumn(&writer, "fixedRowState", NULL, NULL, NULL, NULL, mapping, 0) == 0);
          else
            CHECK(SDDS_DefineColumn(&writer, "fixedRowValue", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 0);
          CHECK(SDDS_SetRowCountMode(&writer, SDDS_FIXEDROWCOUNT));
          writer.layout.data_mode.column_major = (short)major;
          CHECK(SDDS_WriteLayout(&writer));
          if (useEnum) {
            CHECK(writer.layout.version == 6);
            setTestEnvironment("SDDS_OUTPUT_ENDIANESS", environment == 0 ? NULL : environment == 1 ? "little" : "big");
          }
          for (page = 0; page < 2; page++) {
            CHECK(SDDS_StartPage(&writer, page + 2));
            CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, codes[page], page + 2, 0));
            CHECK(SDDS_WritePage(&writer));
            offsets[page] = writer.rowcount_offset;
          }
          CHECK(SDDS_Terminate(&writer));
          CHECK((file = fopen(path, "rb")) != NULL);
          for (page = 0; page < 2; page++) {
            unsigned char bytes[4];
            int byte;
            CHECK(fseek(file, (long)offsets[page], SEEK_SET) == 0);
            CHECK(fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
            for (byte = 0; byte < 4; byte++)
              CHECK(bytes[byte] == (unsigned char)((uint32_t)(page + 2) >> (8 * (bigEndian ? 3 - byte : byte))));
          }
          CHECK(fclose(file) == 0);
          CHECK(SDDS_InitializeInput(&reader, path));
          for (page = 0; page < 2; page++) {
            CHECK(SDDS_ReadPage(&reader) == page + 1);
            CHECK(reader.n_rows == page + 2);
            CHECK(!memcmp(reader.data[0], codes[page], (size_t)(page + 2) * sizeof(int32_t)));
          }
          CHECK(SDDS_ReadPage(&reader) == -1);
          CHECK(SDDS_Terminate(&reader));
        }
  setTestEnvironment("SDDS_OUTPUT_ENDIANESS", savedEndian);
  free(savedEndian);
}

#if defined(RPN_SUPPORT)
/** @brief Reject categorical calculation targets before evaluating any expression. */
static void enumRpnTargetTests(void) {
  SDDS_DATASET dataset = {0};
  int32_t codes[] = {10, 0};
  double numbers[] = {2.5, 3.5};
  char characters[] = {'A', 'B'};
  int64_t sideEffect;
  int rows;
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumColumn(&dataset, "enumRpnTargetColumn", NULL, NULL, NULL, NULL, mapping, 0) == 0);
  CHECK(SDDS_DefineColumn(&dataset, "enumRpnNumberColumn", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == 1);
  CHECK(SDDS_DefineColumn(&dataset, "enumRpnCharacterColumn", NULL, NULL, NULL, NULL, SDDS_CHARACTER, 0) == 2);
  CHECK(SDDS_DefineEnumParameter(&dataset, "enumRpnTargetParameter", NULL, NULL, NULL, NULL, mapping, NULL) == 0);
  CHECK(SDDS_DefineParameter(&dataset, "enumRpnNumberParameter", NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL) == 1);
  CHECK(SDDS_DefineParameter(&dataset, "enumRpnCharacterParameter", NULL, NULL, NULL, NULL, SDDS_CHARACTER, NULL) == 2);
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 2));
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, codes, 2, 0));
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, numbers, 2, 1));
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, characters, 2, 2));
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, (int32_t)10));
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 1, 3.5));
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 2, (int)'A'));
  CHECK((sideEffect = SDDS_CreateRpnMemory("enumRpnSideEffect", 0)) >= 0);
  rpn_store(0, NULL, sideEffect);
  for (rows = 0; rows <= 2; rows += 2) {
    dataset.n_rows = rows;
    CHECK(!SDDS_ComputeColumn(&dataset, 0, "1 sto enumRpnSideEffect 0"));
    CHECK(SDDS_NumberOfErrors() > 0); SDDS_ClearErrors();
    CHECK(!SDDS_ComputeParameter(&dataset, 0, "1 sto enumRpnSideEffect 0"));
    CHECK(SDDS_NumberOfErrors() > 0); SDDS_ClearErrors();
    CHECK(rpn_recall(sideEffect) == 0);
    CHECK(!memcmp(dataset.data[0], codes, sizeof(codes)) && *(int32_t *)dataset.parameter[0] == 10);
  }
  CHECK(SDDS_ComputeParameter(&dataset, 1, "7") && *(double *)dataset.parameter[1] == 7);
  CHECK(SDDS_ComputeParameter(&dataset, 2, "67") && *(char *)dataset.parameter[2] == 'C');
  CHECK(SDDS_ComputeColumn(&dataset, 1, "enumRpnNumberParameter i_row +"));
  CHECK(((double *)dataset.data[1])[0] == 7 && ((double *)dataset.data[1])[1] == 8);
  CHECK(SDDS_ComputeColumn(&dataset, 2, "68 i_row +"));
  CHECK(((char *)dataset.data[2])[0] == 'D' && ((char *)dataset.data[2])[1] == 'E');
  CHECK(SDDS_StoreParametersInRpnMemories(&dataset) && SDDS_StoreRowInRpnMemories(&dataset, 0));
  CHECK(SDDS_Terminate(&dataset));
}
/** @brief Reject enum array references, preserving numeric arrays and aliases across layouts. */
static void enumRpnArrayTests(void) {
  SDDS_DATASET categorical = {0}, numeric = {0}, converted = {0}, restored = {0};
  int32_t codes[] = {10, 0}, length;
  double values[] = {12.5, 3.5}, *array;
  int64_t memory, alias;
  CHECK((memory = SDDS_CreateRpnArray("&enumRpnArrayState")) >= 0);
  CHECK((array = rpn_getarraypointer(memory, &length)) != NULL && length == 1);
  array[0] = 123;
  CHECK((alias = SDDS_CreateRpnMemory("enumRpnArrayAlias", 0)) >= 0);
  CHECK(rpn_store(rpn_recall(memory), NULL, alias));
  CHECK(SDDS_InitializeOutput(&categorical, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumColumn(&categorical, "enumRpnArrayState", NULL, NULL, NULL, NULL, mapping, 0) == 0);
  CHECK(rpn_recall(memory) == -1);
  CHECK(SDDS_DefineColumn(&categorical, "enumRpnArrayNumber", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == 1);
  CHECK(SDDS_SaveLayout(&categorical) && SDDS_StartPage(&categorical, 2));
  CHECK(SDDS_SetColumn(&categorical, SDDS_SET_BY_INDEX, codes, 2, 0));
  CHECK(SDDS_SetColumn(&categorical, SDDS_SET_BY_INDEX, values, 2, 1));
  CHECK(!SDDS_ComputeColumn(&categorical, 1, "0 &enumRpnArrayState ["));
  CHECK(SDDS_NumberOfErrors() > 0 && rpn_check_error());
  SDDS_ClearErrors(); rpn_clear_error(); rpn_clear();
  CHECK(SDDS_ComputeColumn(&categorical, 1, "0 &enumRpnArrayNumber ["));
  CHECK(((double *)categorical.data[1])[0] == 12.5 && ((double *)categorical.data[1])[1] == 12.5);
  CHECK((array = rpn_getarraypointer(alias, &length)) != NULL && length == 1 && array[0] == 123);

  /* The shared pointer name can be rebound for numeric use, then invalidated
   * again by each categorical preparation path without touching its old aliases. */
  CHECK(SDDS_InitializeOutput(&numeric, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineColumn(&numeric, "enumRpnArrayState", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == 0);
  CHECK(SDDS_SaveLayout(&numeric) && SDDS_StartPage(&numeric, 2));
  CHECK(SDDS_SetColumn(&numeric, SDDS_SET_BY_INDEX, values, 2, 0));
  for (int preparation = 0; preparation < 5; preparation++) {
    CHECK(SDDS_StoreColumnsInRpnArrays(&numeric));
    CHECK(rpn_recall(memory) == rpn_recall(alias));
    CHECK((array = rpn_getarraypointer(memory, &length)) != NULL && length == 2 && array[0] == 12.5);
    if (preparation == 0) CHECK(SDDS_StoreColumnsInRpnArrays(&categorical));
    else if (preparation == 1) CHECK(SDDS_StoreRowInRpnMemories(&categorical, 0));
    else if (preparation == 2) CHECK(SDDS_StoreParametersInRpnMemories(&categorical));
    else {
      if (preparation == 4) categorical.n_rows = 0;
      CHECK(SDDS_FilterRowsWithRpnTest(&categorical, "1 1 =="));
      categorical.n_rows = 2;
    }
    CHECK(rpn_recall(memory) == -1);
    CHECK((array = rpn_getarraypointer(alias, &length)) != NULL && length == 2 && array[0] == 12.5 && array[1] == 3.5);
  }

  CHECK(SDDS_InitializeOutput(&converted, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_CopyLayout(&converted, &categorical));
  CHECK(SDDS_ConfigureEnumConversion(&converted, 0, 0, SDDS_LONG, NULL));
  CHECK(SDDS_CopyPageWithEnumConversions(&converted, &categorical));
  CHECK(SDDS_StoreColumnsInRpnArrays(&converted));
  CHECK((array = rpn_getarraypointer(memory, &length)) != NULL && length == 2 && array[0] == 10 && array[1] == 0);
  CHECK(SDDS_InitializeOutput(&restored, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_CopyLayout(&restored, &converted));
  CHECK(SDDS_ConfigureEnumConversion(&restored, 0, 0, SDDS_ENUM, mapping));
  CHECK(rpn_recall(memory) == -1);
  CHECK(SDDS_CopyPageWithEnumConversions(&restored, &converted));
  CHECK(SDDS_StoreColumnsInRpnArrays(&restored));
  CHECK(rpn_recall(memory) == -1 && !memcmp(restored.data[0], codes, sizeof(codes)));
  CHECK((array = rpn_getarraypointer(alias, &length)) != NULL && length == 2 && array[0] == 10 && array[1] == 0);
  CHECK(SDDS_Terminate(&restored) && SDDS_Terminate(&converted) && SDDS_Terminate(&numeric) && SDDS_Terminate(&categorical));
}
#endif

/** @brief Exercise dictionary equality, valid negative codes and checked remapping. */
static void dictionaryTests(void) {
  SDDS_ENUM_DEFINITION *a = SDDS_CreateEnumDefinition(mapping);
  SDDS_ENUM_DEFINITION *b = SDDS_CreateEnumDefinition("-1:FAULT,10:ON,0:OFF");
  SDDS_ENUM_DEFINITION *c = SDDS_CreateEnumDefinition("7:OFF,8:ON,9:FAULT");
  SDDS_ENUM_DEFINITION *d = SDDS_CreateEnumDefinition("0:OFF,10:ON,-1:FAULT");
  int32_t code, input[] = {0, 10, -1}, output[3] = {0};
  const char *invalid[] = {"", "0:", "0:A,", "0:A,0:B", "0:A,1:A", "2147483648:A", "-2147483649:A", "0:A\\q", "0:A\\", "0:A:B", "0:\300\200", NULL};
  int i;
  CHECK(a && b && c && d);
  CHECK(SDDS_EnumLabelToCode(a, "FAULT", &code) && code == -1);
  CHECK(!SDDS_EnumLabelToCode(a, "fault", &code));
  CHECK(SDDS_EnumDefinitionsEqual(b, d));
  CHECK(!SDDS_EnumDefinitionsEqual(b, c));
  CHECK(SDDS_EnumDefinitionsEqual(b, b));
  CHECK(SDDS_EnumDefinitionsEqual(NULL, NULL));
  CHECK(!SDDS_EnumDefinitionsEqual(b, NULL));
  /* Repeated allocations exercise cache lifetime safety when addresses recycle. */
  for (i = 0; i < 256; i++) {
    SDDS_ENUM_DEFINITION *peer = SDDS_CreateEnumDefinition(i % 2 ? "0:OFF,10:ON,-1:FAULT" : "0:ON,10:OFF,-1:FAULT");
    CHECK(peer);
    CHECK(SDDS_EnumDefinitionsEqual(b, peer) == i % 2);
    CHECK(SDDS_EnumDefinitionsEqual(peer, b) == i % 2);
    SDDS_FreeEnumDefinition(peer);
  }
  CHECK(SDDS_RemapEnumValues(b, c, input, output, 3));
  CHECK(output[0] == 7 && output[1] == 8 && output[2] == 9);
  for (i = 0; invalid[i]; i++) { CHECK(!SDDS_CreateEnumDefinition(invalid[i])); SDDS_ClearErrors(); }
  CHECK(SDDS_SetEnumReaderLimits(4096, 8));
  CHECK(!SDDS_CreateEnumDefinition(mapping));
  SDDS_ClearErrors();
  CHECK(SDDS_SetEnumReaderLimits(16U * 1024U * 1024U, 1024U * 1024U));
  SDDS_FreeEnumDefinition(a); SDDS_FreeEnumDefinition(b); SDDS_FreeEnumDefinition(c); SDDS_FreeEnumDefinition(d);
  CHECK(!SDDS_ValidateEnumPage(NULL)); SDDS_ClearErrors();
  CHECK(!SDDS_CopyColumns(NULL, NULL)); SDDS_ClearErrors();
  CHECK(!SDDS_NUMERIC_TYPE(SDDS_ENUM) && !SDDS_INTEGER_TYPE(SDDS_ENUM));
  CHECK(SDDS_ANY_NUMERIC_TYPE == 0x10001 && SDDS_ENUM == 12);
}

/** @brief Round trip every element kind, byte order, major order and compression wrapper. */
static void roundTrip(char *path, int mode, int major, int endian) {
  SDDS_DATASET w = {0}, r = {0}, copy = {0};
  int32_t codes[] = {0, 10, -1, 20}, shape[] = {4}, invalid[] = {0, 10, 42, 20};
  int32_t parameter = 10;
  char *values = NULL, longFixed[8193];
  int page, i;
  for (i = 0; i < 2048; i++) memcpy(longFixed + 4 * i, "\\170", 4);
  longFixed[8192] = 0;
  CHECK(SDDS_InitializeOutput(&w, mode, 1, "SDDS6 enum regression", NULL, path));
  CHECK(SDDS_DefineEnumColumn(&w, NULL, NULL, NULL, NULL, NULL, mapping, 0) < 0); SDDS_ClearErrors();
  CHECK(SDDS_DefineEnumColumn(&w, "state", NULL, NULL, NULL, NULL, mapping, 0) == 0);
  CHECK(SDDS_DefineEnumParameter(&w, "stateParameter", NULL, NULL, NULL, NULL, mapping, NULL) == 0);
  CHECK(SDDS_DefineEnumParameter(&w, "fixedState", NULL, NULL, NULL, NULL, mapping, "-1") == 1);
  CHECK(SDDS_DefineEnumArray(&w, "stateArray", NULL, NULL, NULL, NULL, mapping, 0, 1, NULL) == 0);
  CHECK(SDDS_DefineParameter(&w, "fixedText", NULL, NULL, NULL, NULL, SDDS_STRING, "\\141\\134\\142") == 2);
  CHECK(SDDS_DefineParameter(&w, "longFixedText", NULL, NULL, NULL, NULL, SDDS_STRING, longFixed) == 3);
  CHECK(SDDS_DefineColumn(&w, "bad", NULL, NULL, NULL, NULL, SDDS_ENUM, 0) < 0);
  SDDS_ClearErrors();
  w.layout.data_mode.column_major = (short)major;
#if defined(_WIN32)
  _putenv_s("SDDS_OUTPUT_ENDIANESS", endian == SDDS_BIGENDIAN ? "big" : "little");
#else
  setenv("SDDS_OUTPUT_ENDIANESS", endian == SDDS_BIGENDIAN ? "big" : "little", 1);
#endif
  w.layout.byteOrderDeclared = (uint32_t)endian;
  CHECK(SDDS_WriteLayout(&w) && w.layout.version == 6);
  for (page = 0; page < 2; page++) {
    CHECK(SDDS_StartPage(&w, 4));
    CHECK(!SDDS_SetColumn(&w, SDDS_SET_BY_NAME, invalid, 4, "state")); SDDS_ClearErrors();
    CHECK(SDDS_SetColumn(&w, SDDS_SET_BY_NAME, codes, 4, "state"));
    CHECK(SDDS_SetParameter(&w, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "stateParameter", parameter));
    CHECK(SDDS_SetArray(&w, "stateArray", SDDS_CONTIGUOUS_DATA, codes, shape));
    ((int32_t *)w.data[0])[0] = 42;
    CHECK(!SDDS_WritePage(&w)); SDDS_ClearErrors();
    ((int32_t *)w.data[0])[0] = 0;
    CHECK(SDDS_WritePage(&w));
  }
  CHECK(SDDS_Terminate(&w));
  CHECK(SDDS_InitializeInput(&r, path) && r.layout.version == 6);
  CHECK(SDDS_GetColumnInformation(&r, "enum_values", &values, SDDS_GET_BY_INDEX, 0) == SDDS_STRING);
  CHECK(!strcmp(values, mapping)); free(values);
  CHECK(SDDS_InitializeCopy(&copy, &r, NULL, "m"));
  CHECK(SDDS_EnumDefinitionsEqual(copy.layout.column_definition[0].enum_definition, r.layout.column_definition[0].enum_definition));
  CHECK(copy.layout.column_definition[0].enum_definition != r.layout.column_definition[0].enum_definition);
  CHECK(SDDS_TransferAllColumnDefinitions(&copy, &r, SDDS_TRANSFER_KEEPOLD));
  CHECK(SDDS_TransferAllParameterDefinitions(&copy, &r, SDDS_TRANSFER_KEEPOLD));
  CHECK(!SDDS_ChangeColumnInformation(&copy, "enum_values", (void *)mapping, SDDS_BY_INDEX, 0));
  SDDS_ClearErrors();
  for (page = 0; page < 2; page++) {
    CHECK(SDDS_ReadPage(&r) > 0);
    CHECK(r.n_rows == 4 && !memcmp(r.data[0], codes, sizeof(codes)));
    CHECK(*(int32_t *)r.parameter[0] == 10 && *(int32_t *)r.parameter[1] == -1);
    CHECK(!strcmp(*(char **)r.parameter[2], "a\\b"));
    CHECK(strlen(*(char **)r.parameter[3]) == 2048);
    CHECK(r.array[0].elements == 4 && !memcmp(r.array[0].data, codes, sizeof(codes)));
    CHECK(!SDDS_GetColumnInDoubles(&r, "state")); SDDS_ClearErrors();
    CHECK(SDDS_StartPage(&copy, 4) && SDDS_CopyPage(&copy, &r));
  }
  CHECK(SDDS_ReadPage(&r) < 0);
  CHECK(SDDS_Terminate(&copy) && SDDS_Terminate(&r));
  CHECK(SDDS_InitializeInput(&r, path));
  CHECK(SDDS_ReadPageSparse(&r, 0, 2, 0, 0) > 0);
  CHECK(r.n_rows == 2 && ((int32_t *)r.data[0])[0] == 0 && ((int32_t *)r.data[0])[1] == -1);
  CHECK(SDDS_Terminate(&r));
  CHECK(SDDS_InitializeInput(&r, path));
  CHECK(SDDS_ReadPageLastRows(&r, 1) > 0);
  CHECK(r.n_rows == 1 && ((int32_t *)r.data[0])[0] == 20);
  CHECK(SDDS_Terminate(&r));
}

/** @brief Validate only serialized output rows while retaining complete input validation. */
static void rowSelectionTests(char *path, int mode, int major, int endian) {
  SDDS_DATASET writer = {0}, reader = {0};
  CHECK(SDDS_InitializeOutput(&writer, mode, 1, NULL, NULL, path));
  CHECK(SDDS_DefineEnumColumn(&writer, "selectedOutputState", NULL, NULL, NULL, NULL, "1:ONE", 0) == 0);
  writer.layout.data_mode.column_major = (short)major;
  writer.layout.byteOrderDeclared = (uint32_t)endian;
  CHECK(SDDS_WriteLayout(&writer) && SDDS_StartPage(&writer, 3));
  CHECK(SDDS_SetRowValues(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, (int64_t)2, 0, (int32_t)1, -1));
  CHECK(SDDS_SetRowFlags(&writer, 0));
  CHECK(SDDS_AssertRowFlags(&writer, SDDS_INDEX_LIMITS, (int64_t)2, (int64_t)2, (int32_t)1));
  CHECK(writer.n_rows == 3 && SDDS_CountRowsOfInterest(&writer) == 1);
  CHECK(((int32_t *)writer.data[0])[0] == 0 && ((int32_t *)writer.data[0])[1] == 0);
  CHECK(SDDS_ValidateEnumPage(&writer));
  ((int32_t *)writer.data[0])[2] = 0;
  CHECK(!SDDS_WritePage(&writer)); SDDS_ClearErrors();
  CHECK(SDDS_SetRowValues(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, (int64_t)2, 0, (int32_t)1, -1));
  CHECK(SDDS_WritePage(&writer));
  CHECK(SDDS_StartPage(&writer, 3));
  CHECK(SDDS_SetRowValues(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, (int64_t)2, 0, (int32_t)1, -1));
  CHECK(SDDS_SetRowFlags(&writer, 0));
  CHECK(SDDS_WritePage(&writer) && SDDS_Terminate(&writer));
  CHECK(SDDS_InitializeInput(&reader, path) && SDDS_ReadPage(&reader) == 1);
  CHECK(reader.n_rows == 1 && ((int32_t *)reader.data[0])[0] == 1);
  CHECK(SDDS_SetRowFlags(&reader, 0));
  ((int32_t *)reader.data[0])[0] = 0;
  CHECK(!SDDS_ValidateEnumReadPage(&reader)); SDDS_ClearErrors();
  CHECK(SDDS_ReadPage(&reader) == 2 && reader.n_rows == 0);
  CHECK(SDDS_ReadPage(&reader) == -1 && SDDS_Terminate(&reader));
}

/** @brief Use the same valid/malformed files as C++ and toolkit tests. */
static void sharedFixtures(void) {
  FILE *manifest = fopen("testdata/enums/manifest.txt", "r");
  char name[256], path[512];
  int valid, selection;
  CHECK(manifest);
  while (fscanf(manifest, "%d %255s", &valid, name) == 2) {
    for (selection = 0; selection < 2; selection++) {
      SDDS_DATASET dataset = {0};
      int accepted;
      snprintf(path, sizeof(path), "testdata/enums/%s", name);
      accepted = SDDS_InitializeInput(&dataset, path);
      if (accepted) accepted = (selection ? SDDS_ReadPageSparse(&dataset, 0, 2, 100, 0) : SDDS_ReadPage(&dataset)) > 0;
      if (accepted != valid) fprintf(stderr, "Unexpected fixture result: %s selection=%d\n", path, selection);
      CHECK(accepted == valid);
      SDDS_Terminate(&dataset);
      SDDS_ClearErrors();
    }
  }
  fclose(manifest);
}

/** @brief Inspect live allocator storage where available, without runtime interposition. */
static size_t allocatedBytes(void) {
#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2, 33)
  struct mallinfo2 info = mallinfo2();
#else
  struct mallinfo info = mallinfo();
#endif
  return (size_t)info.uordblks + (size_t)info.hblkhd;
#else
  return 0;
#endif
}

#if defined(RPN_SUPPORT)
/** @brief Repeated enum RPN stores must retain labels without retaining lookup copies. */
static void enumRpnStorageTests(void) {
  SDDS_DATASET dataset = {0};
  const char *values = "10:This label is long enough to reveal every leaked allocation clearly";
  size_t before = 0, after;
  char *label;
  int iteration;
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumColumn(&dataset, "enumRpnStorageColumn", NULL, NULL, NULL, NULL, values, 0) == 0);
  CHECK(SDDS_DefineEnumParameter(&dataset, "enumRpnStorageParameter", NULL, NULL, NULL, NULL, values, NULL) == 0);
  CHECK(SDDS_StartPage(&dataset, 1));
  CHECK(SDDS_SetRowValues(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, 0, (int32_t)10, -1));
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, (int32_t)10));
  for (iteration = 0; iteration < 10032; iteration++) {
    CHECK(SDDS_StoreRowInRpnMemories(&dataset, 0));
    CHECK(SDDS_StoreParametersInRpnMemories(&dataset));
    CHECK(SDDS_FilterRowsWithRpnTest(&dataset, "1 1 =="));
    if (iteration == 31) before = allocatedBytes();
  }
  after = allocatedBytes();
  printf("SDDS6 enum RPN stores: retained allocator growth=%zu bytes\n", after > before ? after - before : 0);
  CHECK(after <= before || after - before < 256U * 1024U);
  CHECK(dataset.row_flag[0]);
  CHECK((label = rpn_str_recall(dataset.layout.column_definition[0].memory_number)) != NULL);
  CHECK(!strcmp(label, SDDS_EnumCodeToLabel(dataset.layout.column_definition[0].enum_definition, 10)));
  free(label);
  CHECK((label = rpn_str_recall(dataset.layout.parameter_definition[0].memory_number)) != NULL);
  CHECK(!strcmp(label, SDDS_EnumCodeToLabel(dataset.layout.parameter_definition[0].enum_definition, 10)));
  free(label);
  CHECK(SDDS_Terminate(&dataset));
}
#endif

/** @brief Bound retained metadata after repeated parsing failures and successful fixed headers. */
static void headerCleanupTests(void) {
  int32_t (*process[])(SDDS_DATASET *, char *) = {
    SDDS_ProcessColumnDefinition, SDDS_ProcessParameterDefinition, SDDS_ProcessArrayDefinition
  };
  const char *suffixes[] = {
    "type=long,enum_values=\"0:OFF\"",
    "type=enum,enum_values=\"0:OFF\",enum_values=\"10:ON\"",
    "type=enum,enum_values=\"0:OFF\",units=\"bad\\q\"",
    "type=enum,enum_values=\"0:OFF,0:ON\"",
    "type=enum,enum_values=\"0:OFF,10:ON\"",
    "type=enum,enum_values=\"0:OFF,10:ON\"",
    "type=string"
  };
  const size_t length = 65536;
  char *large = malloc(length + 1), *text = malloc(3 * length + 1024);
  size_t before = 0, after;
  int iteration, kind, test;
  CHECK(large && text);
  memset(large, 'a', length); large[length] = 0;
  for (iteration = 0; iteration < 33; iteration++) {
    for (kind = 0; kind < 3; kind++) for (test = 0; test < 7; test++) {
      SDDS_DATASET dataset = {0};
      char *p = text;
      if (test == 6 && kind != 1) continue;
      dataset.layout.version = test == 4 ? 5 : 6;
      p += sprintf(p, "name=headerCleanupProbe,symbol=s,description=\"%s\",", large);
      if (kind == 1) p += sprintf(p, "fixed_value=\"%s\",", test == 5 ? "10" : large);
      if (kind == 2) p += sprintf(p, "group_name=\"%s\",", large);
      strcpy(p, suffixes[test]);
      CHECK(process[kind](&dataset, text) == (test >= 5));
      if (test >= 5) {
        CHECK(SDDS_SaveLayout(&dataset));
        if (kind == 1)
          CHECK(!strcmp(dataset.layout.parameter_definition[0].fixed_value, test == 5 ? "10" : large));
      }
      CHECK(SDDS_Terminate(&dataset));
      SDDS_ClearErrors();
    }
    {
      SDDS_DATASET dataset = {0};
      dataset.layout.version = 6;
      sprintf(text, "text=\"%s\",contents=content,unknown=bad", large);
      CHECK(!SDDS_ProcessDescription(&dataset, text));
      CHECK(!dataset.layout.description && !dataset.layout.contents);
      sprintf(text, "name=headerCleanupAssociate,filename=file,description=\"%s\",unknown=bad", large);
      CHECK(!SDDS_ProcessAssociateDefinition(&dataset, text));
      sprintf(text, "filename=\"%s\",unknown=bad", large);
      CHECK(!SDDS_ProcessIncludeCommand(&dataset, text));
      sprintf(text, "filename=\"%s\"", large);
      CHECK(!SDDS_ProcessIncludeCommand(&dataset, text));
      CHECK(SDDS_Terminate(&dataset));
      SDDS_ClearErrors();
    }
    /* Warm up error storage and the process-wide RPN namespace first. */
    if (!iteration) before = allocatedBytes();
  }
  after = allocatedBytes();
  printf("SDDS6 header cleanup: retained allocator growth=%zu bytes\n", after > before ? after - before : 0);
  CHECK(after <= before || after - before < 1024U * 1024U);
  free(large); free(text);
}

/** @brief Check indexed lookup scaling with increasing dictionary cardinality. */
static void lookupScaling(void) {
  int n;
  for (n = 1000; n <= 10000; n *= 10) {
    size_t used = 0;
    int i;
    char *text = malloc((size_t)n * 32), label[32];
    SDDS_ENUM_DEFINITION *dictionary;
    clock_t start;
    CHECK(text);
    for (i = 0; i < n; i++) used += sprintf(text + used, "%s%d:label%d", i ? "," : "", i, i);
    dictionary = SDDS_CreateEnumDefinition(text);
    CHECK(dictionary);
    start = clock();
    for (i = 0; i < 100000; i++) {
      int32_t code;
      snprintf(label, sizeof(label), "label%d", i % n);
      CHECK(SDDS_EnumLabelToCode(dictionary, label, &code) && code == i % n);
      CHECK(!strcmp(SDDS_EnumCodeToLabel(dictionary, code), label));
    }
    printf("enum lookup cardinality=%d lookups=200000 CPU_seconds=%.6f\n", n,
           (double)(clock() - start) / CLOCKS_PER_SEC);
    SDDS_FreeEnumDefinition(dictionary);
    free(text);
  }
}

/** @brief Validate sparse/last-row allocation and malformed discarded binary codes. */
static void sparseEdgeCases(const char *directory) {
  int major, endian, i;
  for (major = 0; major < 2; major++) for (endian = 0; endian < 2; endian++) {
    SDDS_DATASET writer = {0}, reader = {0};
    int32_t values[128], pattern[] = {0, 10, -1, 20};
    char path[2048];
    FILE *file;
    unsigned char invalid[4] = {0, 0, 0, 0};
#if defined(_WIN32)
    _putenv_s("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little");
#else
    setenv("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little", 1);
#endif
    for (i = 0; i < 128; i++) values[i] = pattern[i % 4];
    snprintf(path, sizeof(path), "%s/edgecases/discarded-%d-%d.sdds", directory, major, endian);
    CHECK(SDDS_InitializeOutput(&writer, SDDS_BINARY, 1, NULL, NULL, path));
    CHECK(SDDS_DefineEnumColumn(&writer, "state", NULL, NULL, NULL, NULL, mapping, 0) == 0);
    writer.layout.data_mode.column_major = (short)major;
    CHECK(SDDS_WriteLayout(&writer) && SDDS_StartPage(&writer, 128));
    CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_NAME, values, 128, "state"));
    CHECK(SDDS_WritePage(&writer) && SDDS_Terminate(&writer));
    CHECK(SDDS_InitializeInput(&reader, path));
    CHECK(SDDS_ReadPageSparse(&reader, 0, 17, 3, 0) > 0 && reader.n_rows == 8);
    for (i = 0; i < 8; i++) CHECK(((int32_t *)reader.data[0])[i] == pattern[(3 + 17 * i) % 4]);
    CHECK(SDDS_Terminate(&reader));
    CHECK(SDDS_InitializeInput(&reader, path));
    CHECK(SDDS_ReadPageLastRows(&reader, 3) > 0 && reader.n_rows == 3);
    for (i = 0; i < 3; i++) CHECK(((int32_t *)reader.data[0])[i] == pattern[(125 + i) % 4]);
    CHECK(SDDS_Terminate(&reader));
    CHECK(SDDS_InitializeInput(&reader, path));
    CHECK(SDDS_ReadPageSparse(&reader, 0, 2, 200, 0) > 0 && reader.n_rows == 0);
    CHECK(SDDS_Terminate(&reader));
    file = fopen(path, "r+b");
    CHECK(file && !fseek(file, -(long)sizeof(values), SEEK_END));
    invalid[endian ? 3 : 0] = 42;
    CHECK(fwrite(invalid, 1, sizeof(invalid), file) == sizeof(invalid));
    CHECK(!fclose(file));
    CHECK(SDDS_InitializeInput(&reader, path));
    CHECK(SDDS_ReadPageSparse(&reader, 0, 2, 1, 0) == 0);
    CHECK(SDDS_Terminate(&reader)); SDDS_ClearErrors();
  }
}

/** @brief Keep independent metadata when legacy within-dataset copies involve enums. */
static void copyOwnership(void) {
  SDDS_DATASET input = {0}, copy = {0};
  int32_t source[] = {10, -1}, target[] = {0, 0}, other[] = {7, 7};
  CHECK(SDDS_InitializeInput(&input, "testdata/enums/valid-basic.sdds"));
  CHECK(SDDS_InitializeCopy(&copy, &input, NULL, "m"));
  CHECK(SDDS_DefineEnumColumn(&copy, "same", NULL, NULL, NULL, NULL, "-1:FAULT,10:ON,0:OFF", 0) == 1);
  CHECK(SDDS_DefineEnumColumn(&copy, "different", NULL, NULL, NULL, NULL, "7:OFF", 0) == 2);
#if defined(RPN_SUPPORT)
  CHECK(SDDS_CreateRpnMemory("numericBeforeEnum", 0) >= 0);
  CHECK(SDDS_DefineEnumColumn(&copy, "numericBeforeEnum", NULL, NULL, NULL, NULL, "0:OFF", 0) == 3);
#endif
  CHECK(SDDS_DefineEnumParameter(&copy, "p", NULL, NULL, NULL, NULL, "0:OFF,10:ON,-1:FAULT", NULL) == 0);
  CHECK(SDDS_DefineEnumParameter(&copy, "q", NULL, NULL, NULL, NULL, "-1:FAULT,0:OFF,10:ON", NULL) == 1);
  CHECK(SDDS_SaveLayout(&copy) && SDDS_StartPage(&copy, 2));
  CHECK(((int32_t *)copy.data[2])[0] == 0);
  CHECK(!SDDS_ValidateEnumData(copy.layout.column_definition[2].enum_definition, copy.data[2], 1)); SDDS_ClearErrors();
  CHECK(SDDS_SetColumn(&copy, SDDS_SET_BY_INDEX, source, 2, 0));
  CHECK(SDDS_SetColumn(&copy, SDDS_SET_BY_INDEX, target, 2, 1));
  CHECK(SDDS_SetColumn(&copy, SDDS_SET_BY_INDEX, other, 2, 2));
  CHECK(SDDS_SetParameter(&copy, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, (int32_t)-1));
  CHECK(SDDS_SetParameter(&copy, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 1, (int32_t)0));
#if defined(RPN_SUPPORT)
  CHECK(!SDDS_StoreRowInRpnMemories(&copy, 0)); SDDS_ClearErrors();
#endif
  CHECK(SDDS_CopyColumn(&copy, 1, 0));
  CHECK(!strcmp(copy.layout.column_definition[1].name, "same"));
  CHECK(copy.layout.column_definition[0].enum_definition != copy.layout.column_definition[1].enum_definition);
  CHECK(!memcmp(copy.data[1], source, sizeof(source)));
  CHECK(!SDDS_CopyColumn(&copy, 2, 0)); SDDS_ClearErrors();
  CHECK(SDDS_CopyParameter(&copy, 1, 0));
  CHECK(!strcmp(copy.layout.parameter_definition[1].name, "q") && *(int32_t *)copy.parameter[1] == -1);
  CHECK(!SDDS_ChangeColumnInformation(&copy, "format_string", "%d", SDDS_BY_INDEX, 0)); SDDS_ClearErrors();
  CHECK(SDDS_SaveLayout(&copy));
  CHECK(SDDS_Terminate(&copy) && SDDS_Terminate(&input));
}

/** @brief Ignore excluded enum columns while preflighting every selected row-copy field. */
static void selectedRowCopyTests(void) {
  int direct;
  for (direct = 0; direct < 2; direct++) {
    SDDS_DATASET source = {0}, target = {0};
    int32_t sourceCodes[] = {1, 1}, targetCodes[] = {2, 2};
    int32_t sourceNumbers[] = {42, 84}, targetNumbers[] = {-1, -1};
    CHECK(SDDS_InitializeOutput(&source, SDDS_BINARY, 1, NULL, NULL, NULL));
    CHECK(SDDS_InitializeOutput(&target, SDDS_BINARY, 1, NULL, NULL, NULL));
    CHECK(SDDS_DefineColumn(&source, "selectedRowNumber", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 0);
    CHECK(SDDS_DefineColumn(&target, "selectedRowNumber", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 0);
    CHECK(SDDS_DefineEnumColumn(&source, "excludedRowState", NULL, NULL, NULL, NULL, "1:SOURCE", 0) == 1);
    CHECK(SDDS_DefineEnumColumn(&target, "excludedRowState", NULL, NULL, NULL, NULL, "2:TARGET", 0) == 1);
    CHECK(SDDS_StartPage(&source, 2) && SDDS_StartPage(&target, 2));
    CHECK(SDDS_SetColumn(&source, SDDS_SET_BY_INDEX, sourceNumbers, 2, 0));
    CHECK(SDDS_SetColumn(&target, SDDS_SET_BY_INDEX, targetNumbers, 2, 0));
    CHECK(SDDS_SetColumn(&source, SDDS_SET_BY_INDEX, sourceCodes, 2, 1));
    CHECK(SDDS_SetColumn(&target, SDDS_SET_BY_INDEX, targetCodes, 2, 1));
    source.row_flag[0] = 0;
    CHECK(!(direct ? SDDS_CopyRowDirect(&target, 0, &source, 1) : SDDS_CopyRow(&target, 0, &source, 0)));
    CHECK(!memcmp(target.data[0], targetNumbers, sizeof(targetNumbers)));
    CHECK(!memcmp(target.data[1], targetCodes, sizeof(targetCodes)));
    SDDS_ClearErrors();
    source.column_flag[1] = 0;
    CHECK(direct ? SDDS_CopyRowDirect(&target, 0, &source, 1) : SDDS_CopyRow(&target, 0, &source, 0));
    CHECK(((int32_t *)target.data[0])[0] == 84 && ((int32_t *)target.data[0])[1] == -1);
    CHECK(!memcmp(target.data[1], targetCodes, sizeof(targetCodes)));
    CHECK(SDDS_Terminate(&source) && SDDS_Terminate(&target));
  }
}

/** @brief Generic type edits must reject enum names and all accepted numeric spellings. */
static void enumTypeMutationTests(void) {
  const char *requests[] = {"enum", "12", " 12 ", "+12", "012"};
  SDDS_DATASET dataset = {0};
  int kind, request;
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineColumn(&dataset, "enumMutationColumn", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == 0);
  CHECK(SDDS_DefineParameter(&dataset, "enumMutationParameter", NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL) == 0);
  CHECK(SDDS_DefineArray(&dataset, "enumMutationArray", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0, 1, NULL) == 0);
  for (kind = 0; kind < 3; kind++) {
    int32_t *type = kind == 0 ? &dataset.layout.column_definition[0].type :
      kind == 1 ? &dataset.layout.parameter_definition[0].type : &dataset.layout.array_definition[0].type;
    for (request = 0; request < (int)(sizeof(requests) / sizeof(*requests)); request++) {
      char text[16];
      int32_t result;
      strcpy(text, requests[request]);
      if (kind == 0) result = SDDS_ChangeColumnInformation(&dataset, "type", text, SDDS_BY_INDEX | SDDS_PASS_BY_STRING, 0);
      else if (kind == 1) result = SDDS_ChangeParameterInformation(&dataset, "type", text, SDDS_BY_INDEX | SDDS_PASS_BY_STRING, 0);
      else result = SDDS_ChangeArrayInformation(&dataset, "type", text, SDDS_BY_INDEX | SDDS_PASS_BY_STRING, 0);
      CHECK(!result && *type == SDDS_DOUBLE);
      CHECK(!dataset.layout.column_definition[0].enum_definition && !dataset.layout.column_definition[0].enum_values);
      CHECK(!dataset.layout.parameter_definition[0].enum_definition && !dataset.layout.parameter_definition[0].enum_values);
      CHECK(!dataset.layout.array_definition[0].enum_definition && !dataset.layout.array_definition[0].enum_values);
      SDDS_ClearErrors();
    }
    {
      char text[] = "6";
      int32_t result;
      if (kind == 0) result = SDDS_ChangeColumnInformation(&dataset, "type", text, SDDS_BY_INDEX | SDDS_PASS_BY_STRING, 0);
      else if (kind == 1) result = SDDS_ChangeParameterInformation(&dataset, "type", text, SDDS_BY_INDEX | SDDS_PASS_BY_STRING, 0);
      else result = SDDS_ChangeArrayInformation(&dataset, "type", text, SDDS_BY_INDEX | SDDS_PASS_BY_STRING, 0);
      CHECK(result == SDDS_LONG && *type == SDDS_LONG);
    }
  }
  CHECK(SDDS_ValidateEnumLayout(&dataset) && SDDS_SaveLayout(&dataset) && SDDS_Terminate(&dataset));
}

/** @brief Keep conflicting existing definitions without blocking unrelated transfers. */
static void keepOldDefinitionTests(void) {
  int targetEnum;
  for (targetEnum = 0; targetEnum < 2; targetEnum++) {
    SDDS_DATASET source = {0}, target = {0};
    CHECK(SDDS_InitializeOutput(&source, SDDS_BINARY, 1, NULL, NULL, NULL));
    CHECK(SDDS_InitializeOutput(&target, SDDS_BINARY, 1, NULL, NULL, NULL));
    CHECK(SDDS_DefineEnumColumn(&source, "keepOldColumn", NULL, NULL, NULL, NULL, "1:SOURCE", 0) == 0);
    CHECK(SDDS_DefineColumn(&source, "newColumn", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 1);
    CHECK(SDDS_DefineEnumParameter(&source, "keepOldParameter", NULL, NULL, NULL, NULL, "1:SOURCE", NULL) == 0);
    CHECK(SDDS_DefineParameter(&source, "newParameter", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) == 1);
    if (targetEnum) {
      CHECK(SDDS_DefineEnumColumn(&target, "keepOldColumn", NULL, NULL, NULL, NULL, "2:TARGET", 0) == 0);
      CHECK(SDDS_DefineEnumParameter(&target, "keepOldParameter", NULL, NULL, NULL, NULL, "2:TARGET", NULL) == 0);
    } else {
      CHECK(SDDS_DefineColumn(&target, "keepOldColumn", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 0);
      CHECK(SDDS_DefineParameter(&target, "keepOldParameter", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) == 0);
    }
    CHECK(SDDS_TransferAllColumnDefinitions(&target, &source, SDDS_TRANSFER_KEEPOLD));
    CHECK(SDDS_TransferAllParameterDefinitions(&target, &source, SDDS_TRANSFER_KEEPOLD));
    CHECK(SDDS_GetColumnIndex(&target, "newColumn") == 1 && SDDS_GetParameterIndex(&target, "newParameter") == 1);
    CHECK(target.layout.column_definition[0].type == (targetEnum ? SDDS_ENUM : SDDS_LONG));
    CHECK(target.layout.parameter_definition[0].type == (targetEnum ? SDDS_ENUM : SDDS_LONG));
    if (targetEnum) {
      CHECK(!strcmp(SDDS_EnumValues(target.layout.column_definition[0].enum_definition), "2:TARGET"));
      CHECK(!strcmp(SDDS_EnumValues(target.layout.parameter_definition[0].enum_definition), "2:TARGET"));
    }
    CHECK(!SDDS_TransferAllColumnDefinitions(&target, &source, SDDS_TRANSFER_OVERWRITE)); SDDS_ClearErrors();
    CHECK(!SDDS_TransferAllParameterDefinitions(&target, &source, SDDS_TRANSFER_OVERWRITE)); SDDS_ClearErrors();
    CHECK(SDDS_Terminate(&source) && SDDS_Terminate(&target));
  }
}

/** @brief Ignore output-only enums while reading, but require values before writing. */
static void writeOnlyReadTests(const char *directory) {
  int mode, major, endian, api;
  for (mode = SDDS_BINARY; mode <= SDDS_ASCII; mode++)
    for (major = 0; major < 2; major++)
      for (endian = 0; endian < 2; endian++) {
        SDDS_DATASET writer = {0};
        char path[2048];
        int32_t codes[] = {10, -1, 0, 10};
        snprintf(path, sizeof(path), "%s/api/write-only-read-%d-%d-%d.sdds", directory, mode, major, endian);
        CHECK(SDDS_InitializeOutput(&writer, mode, 1, NULL, NULL, path));
        CHECK(SDDS_DefineEnumColumn(&writer, "inputColumn", NULL, NULL, NULL, NULL, "0:OFF,10:ON,-1:FAULT", 0) == 0);
        CHECK(SDDS_DefineEnumParameter(&writer, "inputParameter", NULL, NULL, NULL, NULL, "0:OFF,10:ON", NULL) == 0);
        writer.layout.data_mode.column_major = (short)major;
#if defined(_WIN32)
        _putenv_s("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little");
#else
        setenv("SDDS_OUTPUT_ENDIANESS", endian ? "big" : "little", 1);
#endif
        CHECK(SDDS_WriteLayout(&writer) && SDDS_StartPage(&writer, 4));
        CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, codes, 4, 0));
        CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, (int32_t)10));
        CHECK(SDDS_WritePage(&writer) && SDDS_Terminate(&writer));
        for (api = 0; api < 3; api++) {
          SDDS_DATASET reader = {0}, output = {0};
          int64_t row;
          int32_t result;
          CHECK(SDDS_InitializeInput(&reader, path));
          SDDS_DeferSavingLayout(&reader, 1);
          CHECK(SDDS_ProcessColumnString(&reader, "&column name=outputColumn,type=enum,enum_values=\"10:ON\" &end", SDDS_WRITEONLY_DEFINITION));
          CHECK(SDDS_ProcessParameterString(&reader, "&parameter name=outputParameter,type=enum,enum_values=\"10:ON\" &end", SDDS_WRITEONLY_DEFINITION));
          SDDS_DeferSavingLayout(&reader, 0);
          CHECK(SDDS_SaveLayout(&reader));
          result = api == 0 ? SDDS_ReadPage(&reader) : api == 1 ? SDDS_ReadPageSparse(&reader, 0, 2, 0, 0) : SDDS_ReadPageLastRows(&reader, 1);
          CHECK(result > 0 && reader.n_rows == (api == 0 ? 4 : api == 1 ? 2 : 1));
          CHECK(*(int32_t *)reader.parameter[0] == 10);
          CHECK(((int32_t *)reader.data[0])[0] == (api == 2 ? 10 : codes[0]));
          if (api == 1) CHECK(((int32_t *)reader.data[0])[1] == codes[2]);
          CHECK(!SDDS_ValidateEnumPage(&reader)); SDDS_ClearErrors();
          for (row = 0; row < reader.n_rows; row++) ((int32_t *)reader.data[1])[row] = 10;
          CHECK(!SDDS_ValidateEnumPage(&reader)); SDDS_ClearErrors();
          CHECK(SDDS_SetParameter(&reader, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 1, (int32_t)10));
          CHECK(SDDS_ValidateEnumPage(&reader));
          snprintf(path, sizeof(path), "%s/api/write-only-output-%d-%d-%d-%d.sdds", directory, mode, major, endian, api);
          CHECK(SDDS_InitializeCopy(&output, &reader, path, "w"));
          output.layout.column_definition[1].definition_mode = SDDS_WRITEONLY_DEFINITION;
          output.layout.parameter_definition[1].definition_mode = SDDS_WRITEONLY_DEFINITION;
          CHECK(SDDS_SaveLayout(&output) && SDDS_WriteLayout(&output));
          CHECK(SDDS_CopyPage(&output, &reader));
          ((int32_t *)output.data[1])[0] = 0;
          CHECK(!SDDS_WritePage(&output)); SDDS_ClearErrors();
          ((int32_t *)output.data[1])[0] = 10;
          *(int32_t *)output.parameter[1] = 0;
          CHECK(!SDDS_WritePage(&output)); SDDS_ClearErrors();
          *(int32_t *)output.parameter[1] = 10;
          CHECK(SDDS_WritePage(&output));
          CHECK(SDDS_Terminate(&output) && SDDS_Terminate(&reader));
          snprintf(path, sizeof(path), "%s/api/write-only-read-%d-%d-%d.sdds", directory, mode, major, endian);
        }
      }
}

/** @brief Numeric parameter access must reject enum codes without changing outputs. */
static void numericParameterTests(void) {
  SDDS_DATASET dataset = {0};
  double floating = -99;
  long double extended = -99;
  int32_t integer = -99;
  int64_t wide = -99;
  int i;
  const char *names[] = {"enumNumericAccess", "enumFixedNumericAccess"};
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumParameter(&dataset, names[0], NULL, NULL, NULL, NULL, mapping, NULL) == 0);
  CHECK(SDDS_DefineEnumParameter(&dataset, names[1], NULL, NULL, NULL, NULL, mapping, "10") == 1);
  CHECK(SDDS_DefineParameter(&dataset, "numericAccess", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) == 2);
  CHECK(SDDS_DefineParameter(&dataset, "characterAccess", NULL, NULL, NULL, NULL, SDDS_CHARACTER, NULL) == 3);
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 0));
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, (int32_t)10));
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 2, (int32_t)10));
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 3, 'A'));
  for (i = 0; i < 2; i++) {
    CHECK(!SDDS_GetParameterAsDouble(&dataset, (char *)names[i], &floating) && floating == -99); SDDS_ClearErrors();
    CHECK(!SDDS_GetParameterAsLongDouble(&dataset, (char *)names[i], &extended) && extended == -99); SDDS_ClearErrors();
    CHECK(!SDDS_GetParameterAsLong(&dataset, (char *)names[i], &integer) && integer == -99); SDDS_ClearErrors();
    CHECK(!SDDS_GetParameterAsLong64(&dataset, (char *)names[i], &wide) && wide == -99); SDDS_ClearErrors();
    CHECK(!SDDS_GetParameterAsDouble(&dataset, (char *)names[i], NULL)); SDDS_ClearErrors();
    CHECK(!SDDS_GetParameterAsLongDouble(&dataset, (char *)names[i], NULL)); SDDS_ClearErrors();
    CHECK(!SDDS_GetParameterAsLong(&dataset, (char *)names[i], NULL)); SDDS_ClearErrors();
    CHECK(!SDDS_GetParameterAsLong64(&dataset, (char *)names[i], NULL)); SDDS_ClearErrors();
  }
  CHECK(SDDS_GetParameterAsDouble(&dataset, "numericAccess", &floating) && floating == 10);
  CHECK(SDDS_GetParameterAsLongDouble(&dataset, "numericAccess", &extended) && extended == 10);
  CHECK(SDDS_GetParameterAsLong(&dataset, "numericAccess", &integer) && integer == 10);
  CHECK(SDDS_GetParameterAsLong64(&dataset, "numericAccess", &wide) && wide == 10);
  CHECK(SDDS_GetParameterAsDouble(&dataset, "characterAccess", &floating) && floating == 'A');
  CHECK(SDDS_Terminate(&dataset));
}

/** @brief Scalar column getters distinguish rejected enums from valid numeric zero. */
static void numericColumnTests(void) {
  SDDS_DATASET dataset = {0};
  int32_t codes[] = {10, 0, -1}, numbers[] = {10, 0, 3};
  int32_t type, row;
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumColumn(&dataset, "enumScalarAccess", NULL, NULL, NULL, NULL, mapping, 0) == 0);
  for (type = 1; type <= SDDS_USHORT; type++)
    CHECK(SDDS_DefineColumn(&dataset, SDDS_type_name[type - 1], NULL, NULL, NULL, NULL, type, 0) == type);
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 3));
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, codes, 3, 0));
  for (type = 1; type <= SDDS_USHORT; type++) {
    void *values = calloc(3, SDDS_type_size[type - 1]);
    CHECK(values);
    for (row = 0; row < 3; row++)
      CHECK(SDDS_CastValue(numbers, row, SDDS_LONG, type, (char *)values + row * SDDS_type_size[type - 1]));
    CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, values, 3, type));
    free(values);
  }
  for (row = 0; row < 3; row++) {
    SDDS_ClearErrors();
    CHECK(SDDS_GetValueAsDouble(&dataset, "enumScalarAccess", row) == 0 && SDDS_NumberOfErrors() > 0);
    SDDS_ClearErrors();
    CHECK(SDDS_GetValueByIndexAsDouble(&dataset, 0, row) == 0 && SDDS_NumberOfErrors() > 0);
    SDDS_ClearErrors();
    for (type = 1; type <= SDDS_USHORT; type++) {
      CHECK(SDDS_GetValueAsDouble(&dataset, SDDS_type_name[type - 1], row) == numbers[row]);
      CHECK(SDDS_GetValueByIndexAsDouble(&dataset, type, row) == numbers[row]);
      CHECK(SDDS_NumberOfErrors() == 0);
    }
  }
  CHECK(!memcmp(dataset.data[0], codes, sizeof(codes)));
  CHECK(SDDS_Terminate(&dataset));
}

/** @brief Unit conversion rejects enums before changing metadata or page data. */
static void enumUnitsConversionTests(const char *directory) {
  SDDS_DATASET writer = {0};
  char path[2048];
  char *names[3][2] = {{"enumColumnUnits", "enumColumnNoUnits"},
                       {"enumParameterUnits", "enumParameterNoUnits"},
                       {"enumArrayUnits", "enumArrayNoUnits"}};
  int32_t code = 10, dimension = 1;
  double number = 2;
  int kind, field, readPage;
  snprintf(path, sizeof(path), "%s/api/units-conversion.sdds", directory);
  CHECK(SDDS_InitializeOutput(&writer, SDDS_BINARY, 1, NULL, NULL, path));
  for (field = 0; field < 2; field++) {
    const char *units = field ? NULL : "old";
    CHECK(SDDS_DefineEnumColumn(&writer, names[0][field], NULL, units, NULL, NULL, mapping, 0) == field);
    CHECK(SDDS_DefineEnumParameter(&writer, names[1][field], NULL, units, NULL, NULL, mapping, NULL) == field);
    CHECK(SDDS_DefineEnumArray(&writer, names[2][field], NULL, units, NULL, NULL, mapping, 0, 1, NULL) == field);
  }
  CHECK(SDDS_DefineColumn(&writer, "numericColumnUnits", NULL, "old", NULL, NULL, SDDS_DOUBLE, 0) == 2);
  CHECK(SDDS_DefineParameter(&writer, "numericParameterUnits", NULL, "old", NULL, NULL, SDDS_DOUBLE, NULL) == 2);
  CHECK(SDDS_DefineArray(&writer, "numericArrayUnits", NULL, "old", NULL, NULL, SDDS_DOUBLE, 0, 1, NULL) == 2);
  CHECK(SDDS_WriteLayout(&writer) && SDDS_StartPage(&writer, 1));
  for (field = 0; field < 2; field++) {
    CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &code, 1, field));
    CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, field, code));
    CHECK(SDDS_SetArray(&writer, names[2][field], SDDS_CONTIGUOUS_DATA, &code, &dimension));
  }
  CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &number, 1, 2));
  CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 2, number));
  CHECK(SDDS_SetArray(&writer, "numericArrayUnits", SDDS_CONTIGUOUS_DATA, &number, &dimension));
  CHECK(SDDS_WritePage(&writer) && SDDS_Terminate(&writer));
  for (readPage = 0; readPage < 2; readPage++) {
    SDDS_DATASET reader = {0};
    CHECK(SDDS_InitializeInput(&reader, path));
    if (readPage) CHECK(SDDS_ReadPage(&reader) > 0);
    for (kind = 0; kind < 3; kind++) for (field = 0; field < 2; field++) {
      char *units = kind == 0 ? reader.layout.column_definition[field].units :
        kind == 1 ? reader.layout.parameter_definition[field].units : reader.layout.array_definition[field].units;
      int32_t result;
      SDDS_ClearErrors();
      if (kind == 0) result = SDDS_SetColumnUnitsConversion(&reader, names[kind][field], "new", "old", 2);
      else if (kind == 1) result = SDDS_SetParameterUnitsConversion(&reader, names[kind][field], "new", "old", 2);
      else result = SDDS_SetArrayUnitsConversion(&reader, names[kind][field], "new", "old", 2);
      CHECK(!result && SDDS_NumberOfErrors() > 0);
      SDDS_ClearErrors();
      if (kind == 0) {
        CHECK(reader.layout.column_definition[field].units == units && reader.original_layout.column_definition[field].units == units);
        if (readPage) CHECK(*(int32_t *)reader.data[field] == code);
      } else if (kind == 1) {
        CHECK(reader.layout.parameter_definition[field].units == units && reader.original_layout.parameter_definition[field].units == units);
        if (readPage) CHECK(*(int32_t *)reader.parameter[field] == code);
      } else {
        CHECK(reader.layout.array_definition[field].units == units && reader.original_layout.array_definition[field].units == units);
        if (readPage) CHECK(*(int32_t *)reader.array[field].data == code);
      }
      CHECK(field ? units == NULL : units && !strcmp(units, "old"));
    }
    if (readPage) {
      CHECK(SDDS_SetColumnUnitsConversion(&reader, "numericColumnUnits", "new", "old", 2));
      CHECK(SDDS_SetParameterUnitsConversion(&reader, "numericParameterUnits", "new", "old", 2));
      CHECK(SDDS_SetArrayUnitsConversion(&reader, "numericArrayUnits", "new", "old", 2));
      CHECK(*(double *)reader.data[2] == 4 && *(double *)reader.parameter[2] == 4 && *(double *)reader.array[2].data == 4);
      CHECK(!strcmp(reader.layout.column_definition[2].units, "new"));
      CHECK(!strcmp(reader.layout.parameter_definition[2].units, "new"));
      CHECK(!strcmp(reader.layout.array_definition[2].units, "new"));
      CHECK(SDDS_NumberOfErrors() == 0);
    }
    CHECK(SDDS_Terminate(&reader));
  }
}

/** @brief Reject invalid fixed-value edits and raw metadata before writing any header bytes. */
static void fixedValueMutationTests(const char *directory) {
  const char *invalid[] = {"42", "10x", "+10", "2147483648", "", NULL};
  SDDS_DATASET writer = {0}, reader = {0};
  char path[2048], *fixed;
  int i;
  snprintf(path, sizeof(path), "%s/api/fixed-value-mutation.sdds", directory);
  CHECK(SDDS_InitializeOutput(&writer, SDDS_ASCII, 1, NULL, NULL, path));
  CHECK(SDDS_DefineEnumParameter(&writer, "fixedMutation", NULL, NULL, NULL, NULL, mapping, "10") == 0);
  CHECK(SDDS_ChangeParameterInformation(&writer, "fixed_value", NULL, SDDS_BY_INDEX, 0) == SDDS_STRING);
  for (i = 0; invalid[i]; i++) {
    CHECK(!SDDS_ChangeParameterInformation(&writer, "fixed_value", (void *)invalid[i], SDDS_BY_INDEX, 0));
    CHECK(!strcmp(writer.layout.parameter_definition[0].fixed_value, "10"));
    SDDS_ClearErrors();
  }
  CHECK(SDDS_ChangeParameterInformation(&writer, "fixed_value", "-1", SDDS_BY_NAME, "fixedMutation") == SDDS_STRING);
  CHECK(SDDS_SaveLayout(&writer));
  fixed = writer.layout.parameter_definition[0].fixed_value;
  memcpy(fixed, "42", 3);
  CHECK(!SDDS_WriteLayout(&writer) && !writer.layout.layout_written);
  CHECK(ftell(writer.layout.fp) == 0);
  SDDS_ClearErrors();
  memcpy(fixed, "-1", 3);
  CHECK(SDDS_WriteLayout(&writer) && SDDS_StartPage(&writer, 0) && SDDS_WritePage(&writer));
  CHECK(SDDS_Terminate(&writer));
  CHECK(SDDS_InitializeInput(&reader, path) && SDDS_ReadPage(&reader) > 0);
  CHECK(*(int32_t *)reader.parameter[0] == -1);
  CHECK(SDDS_Terminate(&reader));
}

/** @brief Convert complete fixed long values with whitespace, retaining strict suffix checks. */
static void fixedLongConversionTests(const char *directory) {
  char valid[][16] = {"10 ", "\t10\r\n", "-1\t", "+10 "};
  char invalid[][16] = {"10x", "10 x", "10\t42", " \t"};
  int32_t expected[] = {10, 10, -1, 10};
  int mode, test;
  for (mode = SDDS_BINARY; mode <= SDDS_ASCII; mode++) {
    for (test = 0; test < (int)(sizeof(valid) / sizeof(*valid)); test++) {
      SDDS_DATASET writer = {0}, reader = {0};
      int32_t value = 0;
      char path[2048];
      snprintf(path, sizeof(path), "%s/api/fixed-long-conversion-%d-%d.sdds", directory, mode, test);
      CHECK(SDDS_InitializeOutput(&writer, mode, 1, NULL, NULL, path));
      CHECK(SDDS_DefineParameter(&writer, "fixedLongConversion", NULL, NULL, NULL, NULL, SDDS_LONG, valid[test]) == 0);
      CHECK(SDDS_GetFixedValueParameter(&writer, "fixedLongConversion", &value) && value == expected[test]);
      CHECK(SDDS_ConfigureEnumConversion(&writer, 1, 0, SDDS_ENUM, "10:ON,-1:FAULT"));
      CHECK(SDDS_WriteLayout(&writer) && SDDS_StartPage(&writer, 0) && SDDS_WritePage(&writer));
      CHECK(SDDS_Terminate(&writer));
      CHECK(SDDS_InitializeInput(&reader, path) && SDDS_ReadPage(&reader) > 0);
      CHECK(reader.layout.parameter_definition[0].type == SDDS_ENUM);
      CHECK(*(int32_t *)reader.parameter[0] == expected[test]);
      CHECK(SDDS_Terminate(&reader));
    }
  }
  for (test = 0; test < (int)(sizeof(invalid) / sizeof(*invalid)); test++) {
    SDDS_DATASET writer = {0};
    CHECK(SDDS_InitializeOutput(&writer, SDDS_BINARY, 1, NULL, NULL, NULL));
    CHECK(SDDS_DefineParameter(&writer, "invalidFixedLongConversion", NULL, NULL, NULL, NULL, SDDS_LONG, invalid[test]) == 0);
    CHECK(!SDDS_ConfigureEnumConversion(&writer, 1, 0, SDDS_ENUM, "10:ON,-1:FAULT"));
    CHECK(writer.layout.parameter_definition[0].type == SDDS_LONG);
    CHECK(!strcmp(writer.layout.parameter_definition[0].fixed_value, invalid[test]));
    SDDS_ClearErrors();
    CHECK(SDDS_SaveLayout(&writer) && SDDS_Terminate(&writer));
  }
}

/** @brief Reject deferred conversions without releasing saved-layout metadata. */
static void deferredConversionTests(const char *directory) {
  int kind, convert;
  for (kind = 0; kind < 3; kind++) for (convert = 0; convert < 2; convert++) {
    SDDS_DATASET writer = {0};
    SDDS_ENUM_DEFINITION *definition;
    char path[2048];
    snprintf(path, sizeof(path), "%s/api/deferred-conversion-%d-%d.sdds", directory, kind, convert);
    CHECK(SDDS_InitializeOutput(&writer, SDDS_BINARY, 1, NULL, NULL, path));
    if (kind == 0) {
      CHECK(SDDS_DefineEnumColumn(&writer, "deferredState", NULL, NULL, NULL, NULL, mapping, 0) == 0);
      definition = writer.layout.column_definition[0].enum_definition;
    } else if (kind == 1) {
      CHECK(SDDS_DefineEnumParameter(&writer, "deferredState", NULL, NULL, NULL, NULL, mapping, "-1") == 0);
      definition = writer.layout.parameter_definition[0].enum_definition;
    } else {
      CHECK(SDDS_DefineEnumArray(&writer, "deferredState", NULL, NULL, NULL, NULL, mapping, 0, 1, NULL) == 0);
      definition = writer.layout.array_definition[0].enum_definition;
    }
    SDDS_DeferSavingLayout(&writer, 1);
    CHECK(!SDDS_ConfigureEnumConversion(&writer, kind, 0, SDDS_LONG, NULL));
    SDDS_ClearErrors();
    CHECK(!SDDS_ConfigureEnumConversion(&writer, kind, 0, SDDS_ENUM, "7:OFF,8:ON,9:FAULT"));
    SDDS_ClearErrors();
    CHECK(!strcmp(SDDS_EnumCodeToLabel(definition, -1), "FAULT"));
    CHECK(SDDS_ValidateEnumLayout(&writer));
    if (kind == 0) CHECK(writer.original_layout.column_definition[0].enum_definition == definition);
    else if (kind == 1) CHECK(writer.original_layout.parameter_definition[0].enum_definition == definition);
    else CHECK(writer.original_layout.array_definition[0].enum_definition == definition);
    SDDS_DeferSavingLayout(&writer, 0);
    if (convert) CHECK(SDDS_ConfigureEnumConversion(&writer, kind, 0, SDDS_LONG, NULL));
    /* Error cleanup must also be safe without another SDDS_SaveLayout call. */
    CHECK(SDDS_Terminate(&writer));
  }
}

/** @brief Encode a test wire integer without depending on native byte order. */
static void appendTestInteger(unsigned char *buffer, size_t *used, int32_t value, int bigEndian) {
  int byte;
  for (byte = 0; byte < 4; byte++)
    buffer[(*used)++] = (unsigned char)((uint32_t)value >> (8 * (bigEndian ? 3 - byte : byte)));
}

/** @brief Exercise direct and ordinary page reads with valid and malformed title data. */
static void directReadValidationTests(const char *directory) {
  const char *suffixes[] = {"sdds", "sdds.gz", "sdds.xz", "sdds.lzma"};
  int mode, bigEndian, compression, invalidField, api;
  for (mode = SDDS_BINARY; mode <= SDDS_ASCII; mode++)
    for (bigEndian = 0; bigEndian < 2; bigEndian++)
      for (compression = 0; compression < 4; compression++)
        for (invalidField = 0; invalidField < 4; invalidField++) {
          unsigned char buffer[1024];
          char path[2048];
          size_t used;
          int32_t parameter = invalidField == 1 ? 42 : -1;
          int32_t arrayValue = invalidField == 2 ? 42 : 10;
          int32_t column = invalidField == 3 ? 42 : 10;
          snprintf(path, sizeof(path), "%s/api/direct-read-%d-%d-%d.%s", directory,
                   mode, bigEndian, invalidField, suffixes[compression]);
          used = (size_t)snprintf((char *)buffer, sizeof(buffer),
              "SDDS6\n"
              "&parameter name=readParameter, type=enum, enum_values=\"0:OFF,10:ON,-1:FAULT\" &end\n"
              "&array name=readArray, type=enum, enum_values=\"0:OFF,10:ON,-1:FAULT\" &end\n"
              "&column name=readColumn, type=enum, enum_values=\"0:OFF,10:ON,-1:FAULT\" &end\n"
              "&data mode=%s, endian=%s &end\n", mode == SDDS_ASCII ? "ascii" : "binary", bigEndian ? "big" : "little");
          if (mode == SDDS_ASCII) {
            used += (size_t)snprintf((char *)buffer + used, sizeof(buffer) - used,
                                    "%" PRId32 "\n2\n0 %" PRId32 "\n1\n%" PRId32 "\n", parameter, arrayValue, column);
          } else {
            appendTestInteger(buffer, &used, 1, bigEndian);
            appendTestInteger(buffer, &used, parameter, bigEndian);
            appendTestInteger(buffer, &used, 2, bigEndian);
            appendTestInteger(buffer, &used, 0, bigEndian);
            appendTestInteger(buffer, &used, arrayValue, bigEndian);
            appendTestInteger(buffer, &used, column, bigEndian);
          }
          if (compression == 1) {
            gzFile file = gzopen(path, "wb");
            CHECK(file && gzwrite(file, buffer, (unsigned int)used) == (int)used);
            CHECK(gzclose(file) == Z_OK);
          } else if (compression > 1) {
            struct lzmafile *file = lzma_open(path, "wb");
            CHECK(file && lzma_write(file, buffer, used) == (long)used);
            lzma_close(file);
          } else {
            FILE *file = fopen(path, "wb");
            CHECK(file && fwrite(buffer, 1, used, file) == used);
            CHECK(!fclose(file));
          }
          for (api = 0; api < 5; api++) {
            SDDS_DATASET reader = {0};
            int32_t result;
            CHECK(SDDS_InitializeInput(&reader, path));
            if (api == 0) result = SDDS_ReadPage(&reader);
            else if (api == 1)
              result = mode == SDDS_ASCII ? SDDS_ReadAsciiPage(&reader, 1, 0, 0) : SDDS_ReadBinaryPage(&reader, 1, 0, 0);
            else if (api == 2)
              result = mode == SDDS_ASCII ? SDDS_ReadAsciiPageLastRows(&reader, 1) :
                reader.swapByteOrder ? SDDS_ReadNonNativeBinaryPage(&reader, 1, 0) : SDDS_ReadBinaryPageLastRows(&reader, 1);
            else if (mode == SDDS_ASCII || reader.swapByteOrder)
              result = api == 3 ? SDDS_ReadNonNativePage(&reader) : SDDS_ReadNonNativePageSparse(&reader, 0, 1, 0);
            else result = SDDS_ReadPageLastRows(&reader, 1);
            CHECK(invalidField ? result == 0 : result > 0);
            if (!invalidField) {
              CHECK(*(int32_t *)reader.parameter[0] == -1 && reader.array[0].elements == 2);
              CHECK(((int32_t *)reader.array[0].data)[1] == 10 && *(int32_t *)reader.data[0] == 10);
            }
            CHECK(SDDS_Terminate(&reader));
            SDDS_ClearErrors();
          }
        }
}

/** @brief Time repeated row copies after the immutable layouts have been checked. */
static double rowCopyTime(int cardinality) {
  SDDS_DATASET source = {0}, target = {0};
  char *values = malloc((size_t)cardinality * 40), *out = values;
  int32_t code = 0;
  int i;
  clock_t begin;
  double elapsed;
  CHECK(values);
  for (i = 0; i < cardinality; i++)
    out += sprintf(out, "%s%d:copyState%05d", i ? "," : "", i, i);
  CHECK(SDDS_InitializeOutput(&source, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumColumn(&source, "enumCopyScaling", NULL, NULL, NULL, NULL, values, 0) == 0);
  CHECK(SDDS_InitializeCopy(&target, &source, NULL, "m"));
  CHECK(SDDS_StartPage(&source, 1) && SDDS_StartPage(&target, 1));
  CHECK(SDDS_SetColumn(&source, SDDS_SET_BY_INDEX, &code, 1, 0));
  CHECK(SDDS_CopyRowDirect(&target, 0, &source, 0));
  begin = clock();
  for (i = 0; i < 20000; i++) CHECK(SDDS_CopyRowDirect(&target, 0, &source, 0));
  elapsed = (double)(clock() - begin) / CLOCKS_PER_SEC;
  CHECK(*(int32_t *)target.data[0] == 0 && target.n_rows == 1);
  printf("enum row-copy cardinality=%d copies=20000 CPU_seconds=%g\n", cardinality, elapsed);
  CHECK(SDDS_Terminate(&target) && SDDS_Terminate(&source));
  free(values);
  return elapsed;
}

/** @brief Dictionary size must not multiply the cost of every copied row. */
static void rowCopyScaling(void) {
  double small = rowCopyTime(1000), large = rowCopyTime(10000);
  /* Allow clock granularity and instrumentation overhead, while catching a
   * full dictionary comparison for every row in the larger dictionary. */
  CHECK(large <= 3.5 * small + 0.02);
}

/** @brief Extract categorical arrays and reject invalid numeric access/assignments. */
static void arrayAndSelectionTests(void) {
  SDDS_DATASET input = {0}, copy = {0};
  int32_t codes[] = {0, 10}, shape[] = {2}, invalid = 42, appended = -1, count;
  char *text[] = {"123", "word"}, **labels;
  CHECK(SDDS_InitializeInput(&input, "testdata/enums/valid-basic.sdds"));
  CHECK(SDDS_InitializeCopy(&copy, &input, NULL, "m"));
  CHECK(SDDS_DefineEnumArray(&copy, "states", NULL, NULL, NULL, NULL, mapping, 0, 1, NULL) == 0);
  CHECK(SDDS_DefineColumn(&copy, "scanText", NULL, NULL, NULL, NULL, SDDS_STRING, 0) == 1);
  CHECK(SDDS_SaveLayout(&copy) && SDDS_StartPage(&copy, 2));
  CHECK(SDDS_SetColumn(&copy, SDDS_SET_BY_NAME, codes, 2, "state"));
  CHECK(SDDS_SetColumn(&copy, SDDS_SET_BY_NAME, text, 2, "scanText"));
  CHECK(SDDS_SetArray(&copy, "states", SDDS_CONTIGUOUS_DATA, codes, shape));
  labels = SDDS_GetArrayInString(&copy, "states", &count);
  CHECK(labels && count == 2 && !strcmp(labels[0], "OFF") && !strcmp(labels[1], "ON"));
  CHECK(SDDS_FreeStringArray(labels, count)); free(labels);
  CHECK(!SDDS_GetArrayInDoubles(&copy, "states", &count) && count == 0); SDDS_ClearErrors();
  CHECK(!SDDS_GetArrayInLong(&copy, "states", &count) && count == 0); SDDS_ClearErrors();
  CHECK(!SDDS_AppendToArrayVararg(&copy, "states", SDDS_CONTIGUOUS_DATA, &invalid, 1, 3));
  SDDS_ClearErrors();
  CHECK(copy.array[0].elements == 2 && copy.array[0].dimension[0] == 2);
  CHECK(!memcmp(copy.array[0].data, codes, sizeof(codes)));
  CHECK(SDDS_AppendToArrayVararg(&copy, "states", SDDS_CONTIGUOUS_DATA, &appended, 1, 3));
  CHECK(copy.array[0].elements == 3 && ((int32_t *)copy.array[0].data)[2] == -1);
  CHECK(SDDS_ValidateEnumPage(&copy));
  ((int32_t *)copy.array[0].data)[0] = invalid;
  CHECK(!SDDS_GetArrayInString(&copy, "states", &count) && count == 0); SDDS_ClearErrors();
  ((int32_t *)copy.array[0].data)[0] = 0;
  CHECK(SDDS_FilterRowsByNumScan(&copy, "state", 0) == -1); SDDS_ClearErrors();
  CHECK(SDDS_CountRowsOfInterest(&copy) == 2);
  CHECK(SDDS_FilterRowsByNumScan(&copy, "scanText", 0) == 1 && copy.row_flag[0] && !copy.row_flag[1]);
  CHECK(SDDS_FilterRowsByNumScan(&copy, "scanText", NUMSCANFILTER_INVERT) == 1 && !copy.row_flag[0] && copy.row_flag[1]);
  CHECK(SDDS_Terminate(&copy) && SDDS_Terminate(&input));
}

/** @brief Exercise each name-list and wildcard row-selection calling convention. */
static int64_t selectEnumTestRows(SDDS_DATASET *dataset, int32_t mode) {
  char *label = mode >= SDDS_CI_NAME_ARRAY ? "on" : "ON";
  char *names[] = {label};
  switch (mode) {
  case SDDS_NAME_ARRAY:
  case SDDS_CI_NAME_ARRAY:
    return SDDS_SetRowsOfInterest(dataset, "matchValidationState", mode, (int32_t)1, names);
  case SDDS_NAMES_STRING:
  case SDDS_CI_NAMES_STRING:
    return SDDS_SetRowsOfInterest(dataset, "matchValidationState", mode, label);
  case SDDS_NAME_STRINGS:
  case SDDS_CI_NAME_STRINGS:
    return SDDS_SetRowsOfInterest(dataset, "matchValidationState", mode, label, (char *)NULL);
  default:
    return SDDS_SetRowsOfInterest(dataset, "matchValidationState", mode,
                                  mode == SDDS_CI_MATCH_STRING ? "o?" : "O?", (int32_t)SDDS_OR);
  }
}

/** @brief Unset and raw unmapped codes must fail matching before row flags change. */
static void enumRowMatchValidationTests(void) {
  SDDS_DATASET dataset = {0};
  int32_t modes[] = {SDDS_NAME_ARRAY, SDDS_NAMES_STRING, SDDS_NAME_STRINGS, SDDS_MATCH_STRING,
                     SDDS_CI_NAME_ARRAY, SDDS_CI_NAMES_STRING, SDDS_CI_NAME_STRINGS, SDDS_CI_MATCH_STRING};
  int32_t numbers[] = {1, 2}, states[] = {10, -1}, peers[] = {10, 10};
  int invalid, mode, noCase;
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumColumn(&dataset, "matchValidationState", NULL, NULL, NULL, NULL, "10:ON,-1:OFF", 0) == 0);
  CHECK(SDDS_DefineEnumColumn(&dataset, "matchValidationPeer", NULL, NULL, NULL, NULL, "-1:OFF,10:ON", 0) == 1);
  CHECK(SDDS_DefineEnumColumn(&dataset, "matchValidationDifferent", NULL, NULL, NULL, NULL, "10:OTHER,-1:OFF", 0) == 2);
  CHECK(SDDS_DefineColumn(&dataset, "matchValidationNumber", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == 3);
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 2));
  /* Setting an unrelated column creates rows while categorical codes remain zero. */
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, numbers, 2, 3));
  CHECK(((int32_t *)dataset.data[0])[0] == 0 && ((int32_t *)dataset.data[1])[0] == 0);
  for (invalid = 0; invalid < 2; invalid++) {
    if (invalid) {
      CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, states, 2, 0));
      ((int32_t *)dataset.data[0])[1] = 12345;
    }
    for (mode = 0; mode < (int)(sizeof(modes) / sizeof(*modes)); mode++) {
      dataset.row_flag[0] = 0; dataset.row_flag[1] = 1;
      CHECK(selectEnumTestRows(&dataset, modes[mode]) == -1);
      CHECK(SDDS_NumberOfErrors() > 0 && !dataset.row_flag[0] && dataset.row_flag[1]);
      SDDS_ClearErrors();
    }
    for (noCase = 0; noCase < 2; noCase++) {
      dataset.row_flag[0] = 0; dataset.row_flag[1] = 1;
      CHECK(SDDS_MatchRowsOfInterest(&dataset, "matchValidationState", noCase ? "o?" : "O?",
                                     SDDS_OR | (noCase ? SDDS_NOCASE_COMPARE : 0)) == -1);
      CHECK(SDDS_NumberOfErrors() > 0 && !dataset.row_flag[0] && dataset.row_flag[1]);
      SDDS_ClearErrors();
    }
  }
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, states, 2, 0));
  /* The primary column is now valid; an unset indirect column must still fail. */
  CHECK(SDDS_MatchRowsOfInterest(&dataset, "matchValidationState", "matchValidationPeer", SDDS_OR | SDDS_INDIRECT_MATCH) == -1);
  CHECK(SDDS_NumberOfErrors() > 0 && !dataset.row_flag[0] && dataset.row_flag[1]);
  SDDS_ClearErrors();
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, peers, 2, 1));
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, peers, 2, 2));
  for (mode = 0; mode < (int)(sizeof(modes) / sizeof(*modes)); mode++) {
    dataset.row_flag[0] = dataset.row_flag[1] = 0;
    CHECK(selectEnumTestRows(&dataset, modes[mode]) == 1);
    CHECK(dataset.row_flag[0] && !dataset.row_flag[1]);
  }
  for (noCase = 0; noCase < 2; noCase++) {
    int32_t logic = SDDS_OR | (noCase ? SDDS_NOCASE_COMPARE : 0);
    dataset.row_flag[0] = dataset.row_flag[1] = 0;
    CHECK(SDDS_MatchRowsOfInterest(&dataset, "matchValidationState", noCase ? "o?" : "O?", logic) == 1);
    CHECK(dataset.row_flag[0] && !dataset.row_flag[1]);
    dataset.row_flag[0] = dataset.row_flag[1] = 0;
    CHECK(SDDS_MatchRowsOfInterest(&dataset, "matchValidationState", "matchValidationPeer", logic | SDDS_INDIRECT_MATCH) == 1);
    CHECK(dataset.row_flag[0] && !dataset.row_flag[1]);
    ((int32_t *)dataset.data[1])[1] = 12345;
    dataset.row_flag[0] = 0; dataset.row_flag[1] = 1;
    CHECK(SDDS_MatchRowsOfInterest(&dataset, "matchValidationState", "matchValidationPeer", logic | SDDS_INDIRECT_MATCH) == -1);
    CHECK(SDDS_NumberOfErrors() > 0 && !dataset.row_flag[0] && dataset.row_flag[1]);
    SDDS_ClearErrors();
    CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, peers, 2, 1));
  }
  CHECK(SDDS_MatchRowsOfInterest(&dataset, "matchValidationState", "matchValidationDifferent", SDDS_OR | SDDS_INDIRECT_MATCH) == -1);
  CHECK(SDDS_NumberOfErrors() > 0 && !dataset.row_flag[0] && dataset.row_flag[1]);
  SDDS_ClearErrors();
  CHECK(SDDS_Terminate(&dataset));
}

/** @brief Format enum parameter labels without numeric casts or fixed-size buffers. */
static void formattedEnumParameterTests(void) {
  SDDS_DATASET input = {0}, dataset = {0};
  int32_t codes[] = {0, 10, -1, 20};
  const char *invalidFormats[] = {"", "%d", "%c", "%f"};
  char longMapping[5003], *result, *memory = NULL;
  size_t i;
  memcpy(longMapping, "1:", 2);
  memset(longMapping + 2, 'x', 5000);
  longMapping[5002] = 0;
  CHECK(SDDS_InitializeInput(&input, "testdata/enums/valid-basic.sdds"));
  CHECK(SDDS_InitializeCopy(&dataset, &input, NULL, "m"));
  CHECK(SDDS_DefineEnumParameter(&dataset, "formattedState", NULL, NULL, NULL, NULL, mapping, NULL) == 0);
  CHECK(SDDS_DefineEnumParameter(&dataset, "longFormattedState", NULL, NULL, NULL, NULL, longMapping, NULL) == 1);
  CHECK(SDDS_StartPage(&dataset, 1));
  for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
    const char *label = SDDS_EnumCodeToLabel(dataset.layout.parameter_definition[0].enum_definition, codes[i]);
    CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, codes[i]));
    result = SDDS_GetParameterAsFormattedString(&dataset, "formattedState", NULL, NULL);
    CHECK(result && !strcmp(result, label));
    free(result);
  }
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, (int32_t)10));
  result = SDDS_GetParameterAsFormattedString(&dataset, "formattedState", &memory, "[%8s]");
  CHECK(result && result == memory && !strcmp(result, "[      ON]"));
  free(result);
  memory = NULL;
  result = SDDS_GetParameterAsFormattedString(&dataset, "formattedState", NULL, "[%-8s]");
  CHECK(result && !strcmp(result, "[ON      ]"));
  free(result);
  for (i = 0; i < sizeof(invalidFormats) / sizeof(invalidFormats[0]); i++) {
    CHECK(!SDDS_GetParameterAsFormattedString(&dataset, "formattedState", &memory, (char *)invalidFormats[i]));
    CHECK(!memory && SDDS_NumberOfErrors() > 0);
    SDDS_ClearErrors();
  }
  ((int32_t *)dataset.parameter[0])[0] = 42;
  CHECK(!SDDS_GetParameterAsFormattedString(&dataset, "formattedState", &memory, NULL));
  CHECK(!memory && SDDS_NumberOfErrors() > 0);
  SDDS_ClearErrors();
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 1, (int32_t)1));
  result = SDDS_GetParameterAsFormattedString(&dataset, "longFormattedState", NULL, NULL);
  CHECK(result && !strcmp(result, longMapping + 2));
  free(result);
  result = SDDS_GetParameterAsFormattedString(&dataset, "longFormattedState", &memory, "state=%s");
  CHECK(result && result == memory && strlen(result) == 5006);
  CHECK(!strncmp(result, "state=", 6) && !strcmp(result + 6, longMapping + 2));
  free(result);
  CHECK(SDDS_Terminate(&dataset) && SDDS_Terminate(&input));
}

/** @brief Exercise long SDDS6 names and diagnostics in every header compression. */
static void longHeaderTests(const char *directory) {
  const char *suffixes[] = {"sdds", "sdds.gz", "sdds.xz", "sdds.lzma"};
  char *name = malloc(20001), path[2048];
  int compression;
  CHECK(name);
  memset(name, 'A', 20000); name[20000] = 0;
  for (compression = 0; compression < 4; compression++) {
    SDDS_DATASET writer = {0}, reader = {0}, copy = {0};
    int32_t code = 10, shape[] = {1};
    FILE *file = NULL;
    gzFile gzip = NULL;
    struct lzmafile *lzma = NULL;
    snprintf(path, sizeof(path), "%s/edgecases/long-name.%s", directory, suffixes[compression]);
    CHECK(SDDS_InitializeOutput(&writer, SDDS_ASCII, 1, NULL, NULL, path));
    CHECK(SDDS_DefineEnumColumn(&writer, name, NULL, NULL, NULL, NULL, mapping, 0) == 0);
    CHECK(SDDS_DefineEnumParameter(&writer, name, NULL, NULL, NULL, NULL, mapping, "10") == 0);
    CHECK(SDDS_DefineEnumArray(&writer, name, NULL, NULL, NULL, NULL, mapping, 0, 1, NULL) == 0);
    CHECK(SDDS_DefineEnumColumn(&writer, name, NULL, NULL, NULL, NULL, mapping, 0) < 0); SDDS_ClearErrors();
    CHECK(SDDS_DefineEnumParameter(&writer, name, NULL, NULL, NULL, NULL, mapping, "10") < 0); SDDS_ClearErrors();
    CHECK(SDDS_DefineEnumArray(&writer, name, NULL, NULL, NULL, NULL, mapping, 0, 1, NULL) < 0); SDDS_ClearErrors();
    CHECK(SDDS_WriteLayout(&writer) && SDDS_StartPage(&writer, 1));
    CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &code, 1, 0));
    CHECK(SDDS_SetArray(&writer, name, SDDS_CONTIGUOUS_DATA, &code, shape));
    CHECK(SDDS_WritePage(&writer) && SDDS_Terminate(&writer));
    CHECK(SDDS_InitializeInput(&reader, path) && SDDS_ReadPage(&reader) > 0);
    CHECK(!strcmp(reader.layout.column_definition[0].name, name));
    CHECK(*(int32_t *)reader.data[0] == 10 && *(int32_t *)reader.parameter[0] == 10);
    CHECK(*(int32_t *)reader.array[0].data == 10);
    CHECK(SDDS_InitializeCopy(&copy, &reader, NULL, "m"));
    CHECK(!SDDS_TransferAllColumnDefinitions(&copy, &reader, 0)); SDDS_ClearErrors();
    CHECK(!SDDS_TransferAllParameterDefinitions(&copy, &reader, 0)); SDDS_ClearErrors();
    CHECK(SDDS_Terminate(&copy) && SDDS_Terminate(&reader));
    snprintf(path, sizeof(path), "%s/edgecases/unknown-command.%s", directory, suffixes[compression]);
    if (compression == 0) {
      CHECK((file = fopen(path, "wb")) != NULL);
      CHECK(SDDS_WriteVersion(6, file));
      CHECK(SDDS6_WriteNamelist(file, 0, name, NULL, NULL, 0));
      CHECK(fclose(file) == 0);
    } else if (compression == 1) {
      CHECK((gzip = gzopen(path, "wb")) != NULL);
      CHECK(SDDS_GZipWriteVersion(6, gzip));
      CHECK(SDDS6_WriteNamelist(gzip, 1, name, NULL, NULL, 0));
      CHECK(gzclose(gzip) == Z_OK);
    } else {
      CHECK((lzma = lzma_open(path, "wb")) != NULL);
      CHECK(SDDS_LZMAWriteVersion(6, lzma));
      CHECK(SDDS6_WriteNamelist(lzma, 2, name, NULL, NULL, 0));
      lzma_close(lzma);
    }
    CHECK(!SDDS_InitializeInput(&reader, path));
    CHECK(SDDS_Terminate(&reader)); SDDS_ClearErrors();
  }
  free(name);
}

/** @brief Compare two optional metadata strings. */
static int sameText(const char *a, const char *b) {
  return (!a && !b) || (a && b && !strcmp(a, b));
}

/**
 * @brief Application definition strings keep literal backslashes in every output version.
 * @details Only text read from an SDDS6 file header uses the SDDS6 escape grammar.
 */
static void applicationDefinitionStringTests(const char *directory) {
  static const char *symbol = "y\\_1";
  static const char *description = "C:\\temp\\new ! x &end y \\\\ z \\q";
  static const char *units = "m\\s";
  char path[2048], text[1024];
  int withEnum, mode;
  for (withEnum = 0; withEnum < 2; withEnum++) for (mode = SDDS_BINARY; mode <= SDDS_ASCII; mode++) {
    SDDS_DATASET writer = {0}, reader = {0};
    int32_t code = 10, dimension = 1, writtenVersion;
    double number = 2;
    snprintf(path, sizeof(path), "%s/api/application-strings-%d-%d.sdds", directory, withEnum, mode);
    CHECK(SDDS_InitializeOutput(&writer, mode, 1, NULL, NULL, path));
    CHECK(writer.layout.version < 6);
    snprintf(text, sizeof(text), "&column name=y1, symbol=%s, units=%s, description=\"%s\", type=double &end", symbol, units, description);
    CHECK(SDDS_ProcessColumnString(&writer, text, 0));
    snprintf(text, sizeof(text), "&parameter name=p1, symbol=%s, units=%s, description=\"%s\", type=double &end", symbol, units, description);
    CHECK(SDDS_ProcessParameterString(&writer, text, 0));
    snprintf(text, sizeof(text), "&array name=a1, symbol=%s, units=%s, description=\"%s\", type=double &end", symbol, units, description);
    CHECK(SDDS_ProcessArrayString(&writer, text));
    /* Unknown SDDS6 escapes are not errors for application strings. */
    CHECK(SDDS_ProcessColumnString(&writer, "&column name=y2, symbol=y\\_2, type=double &end", 0));
    if (withEnum) {
      CHECK(SDDS_ProcessColumnString(&writer, "&column name=state, type=enum, enum_values=\"0:OFF,10:ON\" &end", 0));
      CHECK(SDDS_ProcessParameterString(&writer, "&parameter name=fixedState, type=enum, enum_values=\"0:OFF,10:ON\", fixed_value=10 &end", 0));
      CHECK(SDDS_ProcessArrayString(&writer, "&array name=states, type=enum, enum_values=\"0:OFF,10:ON\" &end"));
    }
    CHECK(!strcmp(writer.layout.column_definition[0].symbol, symbol) && !strcmp(writer.layout.column_definition[0].units, units));
    CHECK(!strcmp(writer.layout.column_definition[0].description, description));
    CHECK(!strcmp(writer.layout.parameter_definition[0].description, description));
    CHECK(!strcmp(writer.layout.array_definition[0].description, description));
    CHECK(SDDS_WriteLayout(&writer));
    writtenVersion = writer.layout.version;
    CHECK(withEnum ? writtenVersion == 6 : writtenVersion < 6);
    CHECK(writer.original_layout.version == writtenVersion);
    CHECK(SDDS_StartPage(&writer, 1) && writer.layout.version == writtenVersion);
    CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_NAME, &number, 1, "y1"));
    CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_NAME, &number, 1, "y2"));
    CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "p1", number));
    CHECK(SDDS_SetArray(&writer, "a1", SDDS_CONTIGUOUS_DATA, &number, &dimension));
    if (withEnum) {
      CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_NAME, &code, 1, "state"));
      CHECK(SDDS_SetArray(&writer, "states", SDDS_CONTIGUOUS_DATA, &code, &dimension));
    }
    CHECK(SDDS_WritePage(&writer) && SDDS_Terminate(&writer));
    CHECK(SDDS_InitializeInput(&reader, path) && reader.layout.version == writtenVersion);
    /* Strings added to a reader of an SDDS6 file also use the application grammar. */
    SDDS_DeferSavingLayout(&reader, 1);
    CHECK(SDDS_ProcessColumnString(&reader, "&column name=y3, symbol=y\\_3, type=double &end", SDDS_WRITEONLY_DEFINITION));
    SDDS_DeferSavingLayout(&reader, 0);
    CHECK(SDDS_SaveLayout(&reader));
    CHECK(sameText(reader.layout.column_definition[reader.layout.n_columns - 1].symbol, "y\\_3"));
    CHECK(SDDS_ReadPage(&reader) == 1);
    CHECK(sameText(reader.layout.column_definition[0].symbol, symbol) && sameText(reader.layout.column_definition[0].units, units));
    CHECK(sameText(reader.layout.column_definition[0].description, description));
    CHECK(sameText(reader.layout.parameter_definition[0].symbol, symbol) && sameText(reader.layout.parameter_definition[0].description, description));
    CHECK(sameText(reader.layout.array_definition[0].units, units) && sameText(reader.layout.array_definition[0].description, description));
    CHECK(sameText(reader.layout.column_definition[1].symbol, "y\\_2"));
    if (withEnum) {
      CHECK(reader.layout.column_definition[2].type == SDDS_ENUM && *(int32_t *)reader.data[2] == 10);
      CHECK(!strcmp(reader.layout.parameter_definition[1].fixed_value, "10"));
    }
    CHECK(SDDS_Terminate(&reader));
  }
}

/** @brief Blank format strings mean "no format" and are omitted from SDDS6 headers. */
static void blankFormatStringTests(const char *directory) {
  SDDS_DATASET writer = {0}, reader = {0};
  char path[2048];
  int32_t code = 0, dimension = 1;
  double number = 1;
  snprintf(path, sizeof(path), "%s/api/blank-format.sdds", directory);
  CHECK(SDDS_InitializeOutput(&writer, SDDS_ASCII, 1, NULL, NULL, path));
  CHECK(SDDS_DefineColumn(&writer, "x", NULL, NULL, NULL, "", SDDS_DOUBLE, 0) == 0);
  CHECK(SDDS_DefineParameter(&writer, "p", NULL, NULL, NULL, " ", SDDS_DOUBLE, NULL) == 0);
  CHECK(SDDS_DefineArray(&writer, "a", NULL, NULL, NULL, "", SDDS_DOUBLE, 0, 1, NULL) == 0);
  CHECK(SDDS_DefineEnumColumn(&writer, "state", NULL, NULL, NULL, "", mapping, 0) == 1);
  CHECK(!writer.layout.column_definition[0].format_string && !writer.layout.parameter_definition[0].format_string);
  /* Even a blank format stored directly must not reach the SDDS6 header. */
  CHECK(SDDS_CopyString(&writer.layout.column_definition[0].format_string, ""));
  CHECK(SDDS_WriteLayout(&writer) && writer.layout.version == 6 && SDDS_StartPage(&writer, 1));
  CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_NAME, &number, 1, "x"));
  CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_NAME, &code, 1, "state"));
  CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "p", number));
  CHECK(SDDS_SetArray(&writer, "a", SDDS_CONTIGUOUS_DATA, &number, &dimension));
  CHECK(SDDS_WritePage(&writer) && SDDS_Terminate(&writer));
  CHECK(SDDS_InitializeInput(&reader, path) && SDDS_ReadPage(&reader) == 1);
  CHECK(!reader.layout.column_definition[0].format_string);
  CHECK(SDDS_Terminate(&reader));
}

/** @brief Units of string/character elements are relabelled; enums are rejected. */
static void stringUnitsConversionTests(const char *directory) {
  SDDS_DATASET writer = {0}, reader = {0};
  char path[2048], *text = "label", character = 'q';
  int32_t dimension = 1;
  snprintf(path, sizeof(path), "%s/api/string-units-conversion.sdds", directory);
  CHECK(SDDS_InitializeOutput(&writer, SDDS_ASCII, 1, NULL, NULL, path));
  CHECK(SDDS_DefineColumn(&writer, "text", NULL, "old", NULL, NULL, SDDS_STRING, 0) == 0);
  CHECK(SDDS_DefineColumn(&writer, "character", NULL, "old", NULL, NULL, SDDS_CHARACTER, 0) == 1);
  CHECK(SDDS_DefineParameter(&writer, "textParameter", NULL, "old", NULL, NULL, SDDS_STRING, NULL) == 0);
  CHECK(SDDS_DefineArray(&writer, "textArray", NULL, "old", NULL, NULL, SDDS_STRING, 0, 1, NULL) == 0);
  CHECK(SDDS_WriteLayout(&writer) && SDDS_StartPage(&writer, 1));
  CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &text, 1, 0));
  CHECK(SDDS_SetColumn(&writer, SDDS_SET_BY_INDEX, &character, 1, 1));
  CHECK(SDDS_SetParameter(&writer, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, text));
  CHECK(SDDS_SetArray(&writer, "textArray", SDDS_CONTIGUOUS_DATA, &text, &dimension));
  CHECK(SDDS_WritePage(&writer) && SDDS_Terminate(&writer));
  CHECK(SDDS_InitializeInput(&reader, path) && SDDS_ReadPage(&reader) == 1);
  CHECK(SDDS_SetColumnUnitsConversion(&reader, "text", "new", "old", 2));
  CHECK(SDDS_SetColumnUnitsConversion(&reader, "character", "new", NULL, 2));
  CHECK(SDDS_SetParameterUnitsConversion(&reader, "textParameter", "new", "old", 2));
  CHECK(SDDS_SetArrayUnitsConversion(&reader, "textArray", "new", "old", 2));
  CHECK(!strcmp(reader.layout.column_definition[0].units, "new") && !strcmp(reader.layout.column_definition[1].units, "new"));
  CHECK(!strcmp(reader.layout.parameter_definition[0].units, "new") && !strcmp(reader.layout.array_definition[0].units, "new"));
  CHECK(!strcmp(((char **)reader.data[0])[0], "label") && ((char *)reader.data[1])[0] == 'q');
  CHECK(!strcmp(*(char **)reader.parameter[0], "label") && !strcmp(((char **)reader.array[0].data)[0], "label"));
  CHECK(SDDS_Terminate(&reader));
}

/** @brief Header string formats keep legacy acceptance; formatting a string requires one string conversion. */
static void stringFormatTests(void) {
  SDDS_DATASET dataset = {0};
  char *value = "x", buffer[SDDS_PRINT_BUFLEN];
  CHECK(SDDS_VerifyPrintfFormat("%s (%s)", SDDS_STRING) && SDDS_VerifyPrintfFormat("%s %d", SDDS_STRING));
  CHECK(SDDS_VerifyPrintfFormat("%-10.4s|", SDDS_STRING) && !SDDS_VerifyPrintfFormat("%d", SDDS_STRING));
  CHECK(!SDDS_VerifyStringFormat("%s (%s)") && !SDDS_VerifyStringFormat("%s %d") && SDDS_VerifyStringFormat("%%%-10.4s%%"));
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_ASCII, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineColumn(&dataset, "legacyFormat", NULL, NULL, NULL, "%s (%s)", SDDS_STRING, 0) == 0);
  CHECK(SDDS_Terminate(&dataset));
  CHECK(SDDS_SprintTypedValue(&value, 0, SDDS_STRING, "[%-3s]", buffer, SDDS_PRINT_NOQUOTES) && !strcmp(buffer, "[x  ]"));
  CHECK(!SDDS_SprintTypedValue(&value, 0, SDDS_STRING, "%s (%s)", buffer, SDDS_PRINT_NOQUOTES));
  CHECK(SDDS_NumberOfErrors() > 0); SDDS_ClearErrors();
}

/** @brief Enum labels are quoted like strings only when needed; codes are never quoted. */
static void enumPresentationQuotingTests(void) {
  SDDS_ENUM_DEFINITION *definition;
  int32_t codes[] = {0, 10, -1, 20};
  char buffer[256];
  CHECK((definition = SDDS_CreateEnumDefinition("0:OFF,10:two words,-1:say \\\"hi\\\",20:a\\\"b")));
  CHECK(SDDS_SprintEnumValue(definition, codes, 0, NULL, buffer, sizeof(buffer), 0, 1, 0) && !strcmp(buffer, "OFF"));
  CHECK(SDDS_SprintEnumValue(definition, codes, 1, NULL, buffer, sizeof(buffer), 0, 1, 0) && !strcmp(buffer, "\"two words\""));
  CHECK(SDDS_SprintEnumValue(definition, codes, 2, NULL, buffer, sizeof(buffer), 0, 1, 0) && !strcmp(buffer, "\"say \\\"hi\\\"\""));
  CHECK(SDDS_SprintEnumValue(definition, codes, 3, NULL, buffer, sizeof(buffer), 0, 1, 0) && !strcmp(buffer, "a\\\"b"));
  CHECK(SDDS_SprintEnumValue(definition, codes, 1, NULL, buffer, sizeof(buffer), SDDS_PRINT_NOQUOTES, 1, 0) && !strcmp(buffer, "two words"));
  CHECK(SDDS_SprintEnumValue(definition, codes, 1, "[%s]", buffer, sizeof(buffer), 0, 1, 0) && !strcmp(buffer, "[\"two words\"]"));
  CHECK(SDDS_SprintEnumValue(definition, codes, 2, NULL, buffer, sizeof(buffer), 0, 1, 1) && !strcmp(buffer, "-1"));
  CHECK(!SDDS_SprintEnumValue(definition, codes, 0, "%s (%s)", buffer, sizeof(buffer), 0, 1, 0)); SDDS_ClearErrors();
  SDDS_FreeEnumDefinition(definition);
}

/** @brief Rejected enum array assignments leave the previous dimensions and data intact. */
static void rejectedArrayAssignmentTests(void) {
  SDDS_DATASET dataset = {0};
  int32_t good[] = {0, 10, -1, 0}, bad[] = {10, 42, 0, 0}, row0[] = {10, 0}, row1[] = {0, 42};
  int32_t *rows[] = {row0, row1}, shape[] = {2, 2}, small[] = {2};
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumArray(&dataset, "flat", NULL, NULL, NULL, NULL, mapping, 0, 1, NULL) == 0);
  CHECK(SDDS_DefineEnumArray(&dataset, "grid", NULL, NULL, NULL, NULL, mapping, 0, 2, NULL) == 1);
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 1));
  CHECK(SDDS_SetArray(&dataset, "flat", SDDS_CONTIGUOUS_DATA, good, small));
  CHECK(!SDDS_SetArray(&dataset, "flat", SDDS_CONTIGUOUS_DATA, bad, shape)); SDDS_ClearErrors();
  CHECK(!SDDS_SetArrayVararg(&dataset, "flat", SDDS_CONTIGUOUS_DATA, bad, 3)); SDDS_ClearErrors();
  CHECK(dataset.array[0].elements == 2 && dataset.array[0].dimension[0] == 2 && !memcmp(dataset.array[0].data, good, 2 * sizeof(*good)));
  CHECK(SDDS_SetArray(&dataset, "grid", SDDS_CONTIGUOUS_DATA, good, shape));
  CHECK(!SDDS_SetArray(&dataset, "grid", SDDS_POINTER_ARRAY, rows, shape)); SDDS_ClearErrors();
  CHECK(!SDDS_SetArrayVararg(&dataset, "grid", SDDS_POINTER_ARRAY, rows, 2, 2)); SDDS_ClearErrors();
  CHECK(!SDDS_SetArrayVararg(&dataset, "grid", SDDS_CONTIGUOUS_DATA, bad, 1, 4)); SDDS_ClearErrors();
  CHECK(dataset.array[1].elements == 4 && dataset.array[1].dimension[0] == 2 && dataset.array[1].dimension[1] == 2);
  CHECK(!memcmp(dataset.array[1].data, good, sizeof(good)));
  row1[1] = -1;
  CHECK(SDDS_SetArrayVararg(&dataset, "grid", SDDS_POINTER_ARRAY, rows, 2, 2));
  CHECK(((int32_t *)dataset.array[1].data)[0] == 10 && ((int32_t *)dataset.array[1].data)[3] == -1);
  {
    int32_t negative[] = {2, -1}, oversized[] = {INT32_MAX, INT32_MAX};
    CHECK(!SDDS_SetArray(&dataset, "grid", SDDS_CONTIGUOUS_DATA, good, negative)); SDDS_ClearErrors();
    CHECK(dataset.array[1].elements == 4 && dataset.array[1].dimension[0] == 2 && dataset.array[1].dimension[1] == 2);
    CHECK(!SDDS_SetArrayVararg(&dataset, "grid", SDDS_CONTIGUOUS_DATA, good, 2, -1)); SDDS_ClearErrors();
    CHECK(dataset.array[1].elements == 4 && dataset.array[1].dimension[1] == 2);
    CHECK(!SDDS_SetArray(&dataset, "grid", SDDS_CONTIGUOUS_DATA, NULL, shape)); SDDS_ClearErrors();
    CHECK(dataset.array[1].elements == 4 && dataset.array[1].dimension[1] == 2);
    CHECK(!SDDS_SetArrayVararg(&dataset, "grid", SDDS_CONTIGUOUS_DATA, NULL, 2, 2)); SDDS_ClearErrors();
    CHECK(!SDDS_SetArray(&dataset, "grid", SDDS_CONTIGUOUS_DATA, good, oversized)); SDDS_ClearErrors();
    CHECK(!SDDS_SetArrayVararg(&dataset, "grid", SDDS_CONTIGUOUS_DATA, good, INT32_MAX, INT32_MAX)); SDDS_ClearErrors();
    CHECK(dataset.array[1].elements == 4 && dataset.array[1].dimension[0] == 2 && dataset.array[1].dimension[1] == 2);
    CHECK(((int32_t *)dataset.array[1].data)[0] == 10 && ((int32_t *)dataset.array[1].data)[3] == -1);
  }
  CHECK(SDDS_Terminate(&dataset));
}

/** @brief Pointer-array counters depend on rank, including shapes with only one element. */
static void singletonPointerArrayTests(void) {
  SDDS_DATASET dataset = {0};
  int32_t code = 10, *row[] = {&code}, **plane[] = {row}, ***cube[] = {plane};
  int32_t shape[] = {1, 1, 1, 1}, empty[] = {1, 0, 1, 1};
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineEnumArray(&dataset, "singleton", NULL, NULL, NULL, NULL, mapping, 0, 4, NULL) == 0);
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 1));
  CHECK(SDDS_SetArray(&dataset, "singleton", SDDS_POINTER_ARRAY, cube, shape));
  CHECK(dataset.array[0].elements == 1 && *(int32_t *)dataset.array[0].data == 10);
  CHECK(SDDS_SetArrayVararg(&dataset, "singleton", SDDS_POINTER_ARRAY, cube, 1, 1, 1, 1));
  CHECK(dataset.array[0].elements == 1 && *(int32_t *)dataset.array[0].data == 10);
  plane[0] = NULL;
  CHECK(!SDDS_SetArray(&dataset, "singleton", SDDS_POINTER_ARRAY, cube, shape)); SDDS_ClearErrors();
  CHECK(!SDDS_SetArrayVararg(&dataset, "singleton", SDDS_POINTER_ARRAY, cube, 1, 1, 1, 1)); SDDS_ClearErrors();
  CHECK(dataset.array[0].elements == 1 && *(int32_t *)dataset.array[0].data == 10);
  CHECK(SDDS_SetArray(&dataset, "singleton", SDDS_CONTIGUOUS_DATA, NULL, empty));
  CHECK(dataset.array[0].elements == 0);
  CHECK(SDDS_SetArrayVararg(&dataset, "singleton", SDDS_CONTIGUOUS_DATA, NULL, 1, 0, 1, 1));
  CHECK(dataset.array[0].elements == 0);
  CHECK(SDDS_Terminate(&dataset));
}

/** @brief String RPN targets keep the legacy no-store success; enum targets stay rejected. */
static void stringRpnTargetTests(void) {
#if defined(RPN_SUPPORT)
  SDDS_DATASET dataset = {0};
  char *text[] = {"a", "b"};
  SDDS_ClearErrors();
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_BINARY, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineColumn(&dataset, "stringRpnTargetColumn", NULL, NULL, NULL, NULL, SDDS_STRING, 0) == 0);
  CHECK(SDDS_DefineParameter(&dataset, "stringRpnTargetParameter", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) == 0);
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 2));
  CHECK(SDDS_SetColumn(&dataset, SDDS_SET_BY_INDEX, text, 2, 0));
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, "p"));
  CHECK(SDDS_ComputeColumn(&dataset, 0, "0") && SDDS_NumberOfErrors() == 0);
  CHECK(SDDS_ComputeParameter(&dataset, 0, "0") && SDDS_NumberOfErrors() == 0);
  CHECK(!strcmp(((char **)dataset.data[0])[1], "b") && !strcmp(*(char **)dataset.parameter[0], "p"));
  CHECK(SDDS_Terminate(&dataset));
#endif
}

/** @brief Columns defined like arrays keep the array field length (enum columns use zero). */
static void columnLikeArrayTests(void) {
  SDDS_DATASET source = {0}, target = {0};
  CHECK(SDDS_InitializeOutput(&source, SDDS_ASCII, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineArray(&source, "fixedWidth", NULL, NULL, NULL, NULL, SDDS_STRING, 5, 1, NULL) == 0);
  CHECK(SDDS_DefineEnumArray(&source, "states", NULL, NULL, NULL, NULL, mapping, 0, 1, NULL) == 1);
  CHECK(SDDS_InitializeOutput(&target, SDDS_ASCII, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineColumnLikeArray(&target, &source, "fixedWidth", NULL));
  CHECK(SDDS_DefineColumnLikeArray(&target, &source, "states", "stateColumn"));
  CHECK(target.layout.column_definition[0].field_length == 5 && target.layout.column_definition[0].type == SDDS_STRING);
  CHECK(target.layout.column_definition[1].field_length == 0 && target.layout.column_definition[1].type == SDDS_ENUM);
  CHECK(SDDS_Terminate(&target) && SDDS_Terminate(&source));
}

/** @brief Parameters defined after page storage exists receive their own value storage. */
static void lateParameterDefinitionTests(void) {
  SDDS_DATASET dataset = {0};
  char *text;
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_ASCII, 1, NULL, NULL, NULL));
  CHECK(SDDS_DefineParameter(&dataset, "early", NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL) == 0);
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 1));
  CHECK(SDDS_ProcessParameterString(&dataset, "&parameter name=late, type=double &end", SDDS_WRITEONLY_DEFINITION));
  CHECK(SDDS_DefineParameter(&dataset, "lateText", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) == 2);
  CHECK(SDDS_SaveLayout(&dataset));
  CHECK(dataset.parameter[1] && *(double *)dataset.parameter[1] == 0 && *(char **)dataset.parameter[2] == NULL);
  CHECK(SDDS_SetParameters(&dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "late", 2.5, "lateText", "value", NULL));
  CHECK(*(double *)dataset.parameter[1] == 2.5 && !strcmp(*(char **)dataset.parameter[2], "value"));
  CHECK((text = SDDS_GetParameterAsFormattedString(&dataset, "late", NULL, NULL)) != NULL);
  free(text);
  CHECK(SDDS_Terminate(&dataset));
  memset(&dataset, 0, sizeof(dataset));
  CHECK(SDDS_InitializeOutput(&dataset, SDDS_ASCII, 1, NULL, NULL, NULL));
  CHECK(SDDS_SaveLayout(&dataset) && SDDS_StartPage(&dataset, 1));
  CHECK(SDDS_ProcessParameterString(&dataset, "&parameter name=firstLate, type=double &end", SDDS_WRITEONLY_DEFINITION));
  CHECK(dataset.parameter && dataset.parameter[0] && *(double *)dataset.parameter[0] == 0);
  CHECK(SDDS_SetParameter(&dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, 0, 2.5));
  CHECK(*(double *)dataset.parameter[0] == 2.5);
  CHECK(SDDS_Terminate(&dataset));
}

/** @brief Unscaled 64-bit integers print exactly. */
static void integerPrintingTests(void) {
  uint64_t unsignedValues[] = {UINT64_MAX, 9007199254740993ULL};
  int64_t signedValues[] = {INT64_MAX, INT64_MIN};
  char buffer[SDDS_PRINT_BUFLEN];
  CHECK(SDDS_SprintTypedValue(unsignedValues, 0, SDDS_ULONG64, NULL, buffer, 0) && !strcmp(buffer, "18446744073709551615"));
  CHECK(SDDS_SprintTypedValue(unsignedValues, 1, SDDS_ULONG64, NULL, buffer, 0) && !strcmp(buffer, "9007199254740993"));
  CHECK(SDDS_SprintTypedValue(signedValues, 0, SDDS_LONG64, NULL, buffer, 0) && !strcmp(buffer, "9223372036854775807"));
  CHECK(SDDS_SprintTypedValueFactor(signedValues, 1, SDDS_LONG64, NULL, buffer, 0, 1.0) && !strcmp(buffer, "-9223372036854775808"));
  CHECK(SDDS_SprintTypedValueFactor(unsignedValues, 1, SDDS_ULONG64, NULL, buffer, 0, 2.0) && !strcmp(buffer, "18014398509481984"));
}

/** @brief Run the checked-in enum regression matrix in the supplied artifact directory. */
int main(int argc, char **argv) {
  const char *suffixes[] = {"sdds", "sdds.gz", "sdds.xz", "sdds.lzma"};
  int mode, major, endian, compression;
  char path[2048];
  CHECK(argc == 2);
#if defined(RPN_SUPPORT)
  {
    char *savedDefinitions = NULL;
    CHECK(SDDS_CopyString(&savedDefinitions, getenv("RPN_DEFNS")));
    setTestEnvironment("RPN_DEFNS", "");
    rpn(NULL);
    setTestEnvironment("RPN_DEFNS", savedDefinitions);
    free(savedDefinitions);
  }
  enumRpnTargetTests();
  enumRpnArrayTests();
  enumRpnStorageTests();
#endif
  fixedRowCountByteOrderTests(argv[1]);
  dictionaryTests();
  sharedFixtures();
  headerCleanupTests();
  lookupScaling();
  copyOwnership();
  selectedRowCopyTests();
  enumTypeMutationTests();
  keepOldDefinitionTests();
  writeOnlyReadTests(argv[1]);
  numericParameterTests();
  numericColumnTests();
  enumUnitsConversionTests(argv[1]);
  fixedValueMutationTests(argv[1]);
  fixedLongConversionTests(argv[1]);
  deferredConversionTests(argv[1]);
  directReadValidationTests(argv[1]);
  rowCopyScaling();
  arrayAndSelectionTests();
  enumRowMatchValidationTests();
  formattedEnumParameterTests();
  longHeaderTests(argv[1]);
  applicationDefinitionStringTests(argv[1]);
  blankFormatStringTests(argv[1]);
  stringUnitsConversionTests(argv[1]);
  stringFormatTests();
  enumPresentationQuotingTests();
  rejectedArrayAssignmentTests();
  singletonPointerArrayTests();
  stringRpnTargetTests();
  integerPrintingTests();
  columnLikeArrayTests();
  lateParameterDefinitionTests();
  for (mode = SDDS_BINARY; mode <= SDDS_ASCII; mode++)
    for (major = 0; major < 2; major++)
      for (endian = 0; endian < 2; endian++)
        for (compression = 0; compression < 4; compression++) {
          snprintf(path, sizeof(path), "%s/enum-%d-%d-%d.%s", argv[1], mode, major, endian, suffixes[compression]);
          roundTrip(path, mode, major, endian ? SDDS_BIGENDIAN : SDDS_LITTLEENDIAN);
          snprintf(path, sizeof(path), "%s/api/selected-output-%d-%d-%d.%s", argv[1], mode, major, endian, suffixes[compression]);
          rowSelectionTests(path, mode, major, endian ? SDDS_BIGENDIAN : SDDS_LITTLEENDIAN);
        }
  sparseEdgeCases(argv[1]);
  puts("SDDS6 enum tests passed");
  return 0;
}
