/**
 * @file SDDSmpi_enum.c
 * @brief Checked, length-prefixed enum dictionary collectives.
 * @copyright Copyright (c) 2026 The University of Chicago.
 * @license Software License Agreement in LICENSE.
 * @author SDDS maintainers
 */
#include "SDDS.h"
#include "SDDS_internal.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/** @brief Return the type and enum fields of column (0), parameter (1) or array (2) @p i. */
static int32_t *enumFields(SDDS_DATASET *dataset, int kind, int32_t i, SDDS_ENUM_DEFINITION ***definition, char ***text) {
  if (kind == 0) {
    *definition = &dataset->layout.column_definition[i].enum_definition;
    *text = &dataset->layout.column_definition[i].enum_values;
    return &dataset->layout.column_definition[i].type;
  }
  if (kind == 1) {
    *definition = &dataset->layout.parameter_definition[i].enum_definition;
    *text = &dataset->layout.parameter_definition[i].enum_values;
    return &dataset->layout.parameter_definition[i].type;
  }
  *definition = &dataset->layout.array_definition[i].enum_definition;
  *text = &dataset->layout.array_definition[i].enum_values;
  return &dataset->layout.array_definition[i].type;
}

/**
 * @brief Synchronize enum mappings without fixed-size metadata buffers.
 * @details The element counts and then root's type vector are broadcast in one call
 * each, so a layout without enums costs four collectives whatever its size. Only the
 * enum fields of root's layout (the same on every rank) need a dictionary exchange.
 * In check mode every rank's types must equal root's; in install mode non-root ranks
 * receive the dictionaries of the fields that they defined as SDDS_LONG.
 */
static int32_t synchronizeEnums(SDDS_DATASET *dataset, int install) {
  MPI_DATASET *mpi = dataset->MPI_dataset;
  int local = install ? 1 : SDDS_ValidateEnumLayout(dataset), all, kind;
  int32_t counts[3] = {dataset->layout.n_columns, dataset->layout.n_parameters, dataset->layout.n_arrays};
  int32_t rootCounts[3], *rootTypes = NULL, i;
  int64_t total = (int64_t)counts[0] + counts[1] + counts[2], n, enums = 0;
  SDDS_ENUM_DEFINITION **definition;
  char **text;
  memcpy(rootCounts, counts, sizeof(counts));
  MPI_Bcast(rootCounts, 3, MPI_INT32_T, 0, mpi->comm);
  if (memcmp(rootCounts, counts, sizeof(counts)) || total > INT_MAX) local = 0;
  /* Every rank allocates the agreed vector size, so the allocation joins the count agreement. */
  if (local && total && !(rootTypes = malloc(sizeof(*rootTypes) * (size_t)total))) local = 0;
  MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, mpi->comm);
  if (!all) { free(rootTypes); SDDS_SetError("MPI enum layout element counts differ between ranks"); return 0; }
  for (kind = n = 0; kind < 3; kind++)
    for (i = 0; i < counts[kind]; i++, n++)
      rootTypes[n] = *enumFields(dataset, kind, i, &definition, &text);
  if (total) MPI_Bcast(rootTypes, (int)total, MPI_INT32_T, 0, mpi->comm);
  for (kind = n = 0; kind < 3; kind++) {
    for (i = 0; i < counts[kind]; i++, n++) {
      if (!install && *enumFields(dataset, kind, i, &definition, &text) != rootTypes[n]) local = 0;
      if (rootTypes[n] == SDDS_ENUM) enums++;
    }
  }
  /* enums counts root's enum fields, so every rank either runs or skips this pass. */
  for (kind = n = 0; enums && kind < 3; kind++) {
    for (i = 0; i < counts[kind]; i++, n++) {
      int32_t *type;
      SDDS_ENUM_DEFINITION *parsed;
      char *payload = NULL;
      uint64_t bytes = 0;
      if (rootTypes[n] != SDDS_ENUM) continue;
      type = enumFields(dataset, kind, i, &definition, &text);
      if (!mpi->myid && *text) bytes = strlen(*text);
      MPI_Bcast(&bytes, 1, MPI_UINT64_T, 0, mpi->comm);
      local = local && bytes && bytes <= SDDS_GetEnumMappingLimit() && bytes < INT_MAX;
      if (local && !(payload = malloc((size_t)bytes + 1))) local = 0;
      MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, mpi->comm);
      if (!all) {
        free(payload); free(rootTypes);
        SDDS_SetError("MPI enum mapping exceeds a rank's reader limit, types differ or allocation failed");
        return 0;
      }
      if (!mpi->myid) memcpy(payload, *text, (size_t)bytes + 1);
      MPI_Bcast(payload, (int)bytes + 1, MPI_CHAR, 0, mpi->comm);
      parsed = SDDS_CreateEnumDefinition(payload);
      free(payload);
      local = parsed != NULL;
      if (local && (!install || !mpi->myid)) local = SDDS_EnumDefinitionsEqual(*definition, parsed);
      MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, mpi->comm);
      if (!all) { SDDS_FreeEnumDefinition(parsed); free(rootTypes); SDDS_SetError("MPI enum dictionaries differ between ranks or are invalid"); return 0; }
      if (install && mpi->myid) {
        SDDS_FreeEnumDefinition(*definition);
        *definition = parsed;
        *text = (char *)SDDS_EnumValues(parsed);
        *type = SDDS_ENUM;
      } else SDDS_FreeEnumDefinition(parsed);
    }
  }
  free(rootTypes);
  MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, mpi->comm);
  if (!all) SDDS_SetError("MPI layout types differ between ranks");
  return all;
}

/** @brief Install dictionaries after broadcasting the ordinary layout. */
int32_t SDDS_MPI_BroadcastEnums(SDDS_DATASET *dataset) { return synchronizeEnums(dataset, 1); }
/** @brief Require compatible dictionaries on every output rank. */
int32_t SDDS_MPI_CheckEnumLayout(SDDS_DATASET *dataset) { return synchronizeEnums(dataset, 0); }
/** @brief Validate local input or output enums and the title data used by this rank. */
static int32_t validateMPIEnumPage(SDDS_DATASET *dataset, int32_t inputPage) {
  SDDS_DATASET page = *dataset;
  int local, all;
  /* Output title data comes only from rank zero.  Readers receive the title
   * on every rank, so they must still validate all of their local values. */
  if (!inputPage && dataset->MPI_dataset->myid) {
    page.layout.n_parameters = 0;
    page.layout.n_arrays = 0;
  }
  /* Column-major MPI writes every row, regardless of its selection flag. Row-major
   * writers (independent and collective) write only the selected rows, which is
   * exactly what SDDS_ValidateEnumPage checks when row_flag is kept. */
  if (!inputPage && dataset->layout.data_mode.column_major) page.row_flag = NULL;
  local = inputPage ? SDDS_ValidateEnumReadPage(&page) : SDDS_ValidateEnumPage(&page);
  MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, dataset->MPI_dataset->comm);
  if (!all) SDDS_SetError("Invalid enum page on an MPI rank");
  return all;
}

/** @brief Validate every enum used by the MPI page writer. */
int32_t SDDS_MPI_ValidateEnumPage(SDDS_DATASET *dataset) { return validateMPIEnumPage(dataset, 0); }

/** @brief Validate input enums without requiring values for output-only definitions. */
int32_t SDDS_MPI_ValidateEnumReadPage(SDDS_DATASET *dataset) { return validateMPIEnumPage(dataset, 1); }

/** @brief Broadcast a bounded nullable string or complete SDDS6 command. */
static int32_t broadcastPayload(SDDS_DATASET *dataset, char **payload, int nullable) {
  MPI_DATASET *mpi = dataset->MPI_dataset;
  uint64_t bytes = !mpi->myid && *payload ? strlen(*payload) + 1 : 0;
  int local, all;
  MPI_Bcast(&bytes, 1, MPI_UINT64_T, 0, mpi->comm);
  local = (nullable || bytes) && bytes <= SDDS_GetEnumLayoutLimit() && bytes <= INT_MAX;
  if (mpi->myid) *payload = local && bytes ? malloc((size_t)bytes) : NULL;
  if (bytes && !*payload) local = 0;
  MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, mpi->comm);
  if (!all) { SDDS_SetError("MPI SDDS6 metadata payload exceeds a reader limit or allocation failed"); return 0; }
  if (bytes) MPI_Bcast(*payload, (int)bytes, MPI_CHAR, 0, mpi->comm);
  return 1;
}

/**
 * @brief Broadcast the entire SDDS6 layout without legacy fixed-size metadata strings.
 * @details All ranks parse the same serialized commands through the C SDDS6 parser.
 */
int32_t SDDS_MPI_BroadcastLayout6(SDDS_DATASET *dataset) {
  MPI_DATASET *mpi = dataset->MPI_dataset;
  SDDS_LAYOUT *layout = &dataset->layout;
  int32_t counts[6] = {1, layout->n_columns, layout->n_parameters, layout->n_arrays, layout->n_associates, 1};
  int32_t metadata[12] = {layout->version, dataset->swapByteOrder, (int32_t)layout->byteOrderDeclared,
    layout->layout_written, dataset->autoRecover, layout->data_mode.column_memory_mode,
    layout->data_mode.fsync_data, layout->disconnected, layout->popenUsed,
    (int32_t)layout->commentFlags, layout->depth, layout->data_command_seen};
  /* Serial master readers also accept compressed streams, which have no
   * physical page offsets. Only parallel file I/O needs this offset. */
  int64_t offset = !mpi->myid && dataset->pagecount_offset ? dataset->pagecount_offset[0] : 0;
  char *payload = NULL;
  int group, i, local = 1, all;
  static const char *commands[] = {"description", "column", "parameter", "array", "associate", "data"};
  MPI_Bcast(counts, 6, MPI_INT32_T, 0, mpi->comm);
  MPI_Bcast(metadata, 12, MPI_INT32_T, 0, mpi->comm);
  MPI_Bcast(&offset, 1, MPI_INT64_T, 0, mpi->comm);
  if (!mpi->myid) local = SDDS_CopyString(&payload, layout->filename);
  MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, mpi->comm);
  if (!all || !broadcastPayload(dataset, &payload, 1)) { free(payload); return 0; }
  if (mpi->myid) { layout->filename = payload; payload = NULL; layout->version = 6; }
  free(payload); payload = NULL;
  SDDS_DeferSavingLayout(dataset, 1);
  for (group = 0; group < 6; group++) for (i = 0; i < counts[group]; i++) {
    if (!mpi->myid) {
      const void *data;
      SDDS_FIELD_INFORMATION *fields;
      int32_t fieldCount;
      if (group == 0) { data = layout; fields = SDDS_DescriptionFieldInformation; fieldCount = SDDS_DESCRIPTION_FIELDS; }
      else if (group == 1) { data = layout->column_definition + i; fields = SDDS_ColumnFieldInformation; fieldCount = SDDS_COLUMN_FIELDS; }
      else if (group == 2) { data = layout->parameter_definition + i; fields = SDDS_ParameterFieldInformation; fieldCount = SDDS_PARAMETER_FIELDS; }
      else if (group == 3) { data = layout->array_definition + i; fields = SDDS_ArrayFieldInformation; fieldCount = SDDS_ARRAY_FIELDS; }
      else if (group == 4) { data = layout->associate_definition + i; fields = SDDS_AssociateFieldInformation; fieldCount = SDDS_ASSOCIATE_FIELDS; }
      else { data = &layout->data_mode; fields = SDDS_DataFieldInformation; fieldCount = SDDS_DATA_FIELDS; }
      payload = SDDS6_CreateNamelist(commands[group], data, fields, fieldCount);
    }
    if (!broadcastPayload(dataset, &payload, 0)) goto failure;
    local = 1;
    if (mpi->myid) {
      char *body = strchr(payload, ' ');
      if (!body) local = 0;
      else if (group == 0) local = SDDS_ProcessDescription(dataset, body + 1);
      else if (group == 1) local = SDDS_ProcessColumnDefinition(dataset, body + 1);
      else if (group == 2) local = SDDS_ProcessParameterDefinition(dataset, body + 1);
      else if (group == 3) local = SDDS_ProcessArrayDefinition(dataset, body + 1);
      else if (group == 4) local = SDDS_ProcessAssociateDefinition(dataset, body + 1);
      else local = SDDS_ProcessDataMode(dataset, body + 1);
    }
    free(payload); payload = NULL;
    MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, mpi->comm);
    if (!all) { SDDS_SetError("MPI SDDS6 definition parsing failed on a rank"); goto failure; }
  }
  dataset->swapByteOrder = (short)metadata[1];
  layout->byteOrderDeclared = (uint32_t)metadata[2];
  layout->layout_written = metadata[3]; dataset->autoRecover = metadata[4];
  layout->data_mode.column_memory_mode = metadata[5]; layout->data_mode.fsync_data = metadata[6];
  layout->disconnected = metadata[7]; layout->popenUsed = metadata[8];
  layout->commentFlags = (uint32_t)metadata[9]; layout->depth = metadata[10]; layout->data_command_seen = metadata[11];
  dataset->mode = SDDS_READMODE;
  mpi->file_offset = (MPI_Offset)offset;
  SDDS_DeferSavingLayout(dataset, 0);
  local = SDDS_SaveLayout(dataset);
  MPI_Allreduce(&local, &all, 1, MPI_INT, MPI_MIN, mpi->comm);
  return all;
failure:
  free(payload);
  SDDS_DeferSavingLayout(dataset, 0);
  SDDS_SaveLayout(dataset);
  return 0;
}
