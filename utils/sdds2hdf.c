#include <stdio.h>
#include "hdf5.h"
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#ifndef min
#define min(a,b) ((a) <= (b) ? (a) : (b))
#endif

int writeStringAttribute(const char* name, const char* value, hid_t dataset_id);
int writeIntAttribute(const char* name, long value, hid_t dataset_id);
int writeDoubleAttribute(const char* name, double value, hid_t dataset_id);

#define SET_PIPE 0
#define SET_VIZSCHEMA 1
#define SET_SPATIALCOLUMNS 2
#define SET_VSSTEP 3
#define SET_VSTIME 4
#define N_OPTIONS 5
char *option[N_OPTIONS] = {
  "pipe", "vizschema", "spatialcolumns", "vsstep", "vstime"
};
char *USAGE = "sdds2hdf [<input-file>] [-pipe=in] <output-file>\n\
-vizschema=vsType=variableWithMesh\n\
-spatialColumns=<x>,<y>[,<z|time>]\n\
-vsTime=<parameter>\n\
-vsStep=<parameter>\n\n\
Program by Robert Soliday. ("__DATE__")\n";

#define VIZ_TYPE 0x00000001U



int main( int argc, char **argv ) {
  SCANNED_ARG *s_arg;
  long i_arg;
  unsigned long pipeFlags=0;
  char *input = NULL;
  char *output = NULL;
  SDDS_TABLE SDDS_table;
  char **columnNames, **parameterNames, **arrayNames;
  int32_t ncolumns, nparameters, narrays;
  void **columnData=NULL, **parameterData=NULL;
  SDDS_ARRAY **arrayData=NULL;
  long *columnType=NULL, *parameterType=NULL, *arrayType=NULL;
  hid_t *columnhdfType=NULL, *parameterhdfType=NULL, *arrayhdfType=NULL;
  char **columnUnits=NULL, **parameterUnits=NULL, **arrayUnits=NULL;
  hid_t file_id;
  hid_t column_dataspace_id;
  hid_t column_dataset_id;
  hid_t column_dataspace_buffer_id;
  hid_t array_dataspace_id;
  hid_t array_dataset_id;
  hid_t parameter_dataspace_id;
  hid_t parameter_dataset_id;
  hid_t page_group_id=0, column_group_id=0, parameter_group_id=0, array_group_id=0, limit_group_id=0, time_group_id=0, runinfo_group_id=0;
  hid_t stringType;
  hsize_t column_dims[2];
  hsize_t parameter_dims[1];
  hsize_t **array_dims=NULL;
  int32_t i, j, page;
  int64_t rows;
  char buffer[1024];
  char *description=NULL, *contents=NULL;
  int vizschema=0;
  int vsType=0;
  char *vsSpatialColumns[3];
  long vsSC[3], vsSCDefined=0;
  hsize_t  offset_file[2];
  hsize_t  count_file[2];
  char *vsMeshType=NULL;
  unsigned long vsFlag;
  char *unitBuffer;
  char unitsBuffer[1024];
  int64_t indexMin[3], indexMax[3];
  char *vsStepParameter=NULL, *vsTimeParameter=NULL;
  long vsTimeIndex[2];
  int32_t vsStep;
  double vsTime;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&s_arg, argc, argv);
  if (argc<3) {
    fprintf(stderr, "%s", USAGE);
    return(1);
  }
  for (i_arg=1; i_arg<argc; i_arg++) {
    if (s_arg[i_arg].arg_type==OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list+1, s_arg[i_arg].n_items-1, &pipeFlags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          return(1);
        }
        break;
      case SET_VIZSCHEMA:
        vizschema=1;
        if (s_arg[i_arg].n_items < 2) {
           fprintf(stderr, "invalid -vizschema syntax\n");
           return(1);
        }
        vsFlag = 0;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&vsFlag, s_arg[i_arg].list+1, &s_arg[i_arg].n_items, 0,
                          "vstype", SDDS_STRING, &vsMeshType, 1, VIZ_TYPE,
                          NULL)) {
          fprintf(stderr, "invalid -vizschema syntax\n");
          return(1);
        }
        if (strlen(vsMeshType) <= 4) {
          if (strncasecmp("mesh",vsMeshType,min(4,strlen(vsMeshType))) == 0) {
            fprintf(stderr, "Error: -vizschema=vsType=mesh not implemented yet\n");
            return(1);
          }
        }
        if (strlen(vsMeshType) <= 8) {
          if (strncasecmp("variable",vsMeshType,min(8,strlen(vsMeshType))) == 0) {
            fprintf(stderr, "Error: -vizschema=vsType=variable not implemented yet\n");
            return(1);
          }
        }
        if (strlen(vsMeshType) <= 16) {
          if (strncasecmp("variableWithMesh",vsMeshType,min(16,strlen(vsMeshType))) == 0) {
            vsType=1;
          }
          
        } 
        if (vsType == 0) {
          fprintf(stderr, "invalid -vizschema syntax\n");
          return(1);
        }
        break;
      case SET_SPATIALCOLUMNS:
        if (s_arg[i_arg].n_items!=4) {
          fprintf(stderr, "invalid -spatialColumns syntax\n");
          return(1);
        }
        for (i=1; i<s_arg[i_arg].n_items; i++) {
          vsSpatialColumns[i-1]=s_arg[i_arg].list[i];
        }
        vsSCDefined=1;
        break;
      case SET_VSSTEP:
        if (s_arg[i_arg].n_items != 2) {
           fprintf(stderr, "invalid -vsStep syntax\n");
           return(1);
        }
        vsStepParameter = s_arg[i_arg].list[1];
        break;
      case SET_VSTIME:
        if (s_arg[i_arg].n_items != 2) {
           fprintf(stderr, "invalid -vsTime syntax\n");
           return(1);
        }
        vsTimeParameter = s_arg[i_arg].list[1];
        break;
      default:
	fprintf(stderr, "Error: unknown switch: %s\n", s_arg[i_arg].list[0]);
	return(1);
      }
    } else {
      if (input == NULL)
        SDDS_CopyString(&input,s_arg[i_arg].list[0]);
      else if (output == NULL)
        SDDS_CopyString(&output,s_arg[i_arg].list[0]);
      else {
	fprintf(stderr, "Error: too many filenames\n");
	return(1);
      }
    }
  }
  if (vizschema && vsType==1) {
    if (vsSCDefined==0) {
      fprintf(stderr, "Error: no spatialColumns defined\n"); 
      return(1);
    }
  }
  if (pipeFlags&USE_STDIN) {
    processFilenames("sdds2hdf", &input, &output, USE_STDIN, 1, NULL);
  }

  if (SDDS_InitializeInput(&SDDS_table, input) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  file_id = H5Fcreate(output, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  if (!SDDS_GetDescription(&SDDS_table, &description, &contents)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (description) {
    writeStringAttribute("description", description, file_id);
    free(description);
  }
  if (contents) {
    writeStringAttribute("contents", contents, file_id);
    free(contents);
  }

  parameterNames = SDDS_GetParameterNames(&SDDS_table, &nparameters);
  if (nparameters > 0) {
    parameterType = tmalloc(sizeof(*parameterType)*nparameters);
    parameterhdfType = tmalloc(sizeof(*parameterhdfType)*nparameters);
    parameterData = tmalloc(sizeof(*parameterData)*nparameters);
    parameterUnits = tmalloc(sizeof(*parameterUnits)*nparameters);
    parameter_dims[0] = 1;
    for (i=0; i<nparameters; i++) {
      if ((parameterType[i]=SDDS_GetParameterType(&SDDS_table, i))<=0 ||
          SDDS_GetParameterInformation(&SDDS_table, "units", &parameterUnits[i], SDDS_GET_BY_INDEX, i)!=SDDS_STRING) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      if (parameterType[i] == SDDS_LONGDOUBLE) {
        parameterhdfType[i] = H5T_NATIVE_LDOUBLE;
      } else if (parameterType[i] == SDDS_DOUBLE) {
        parameterhdfType[i] = H5T_NATIVE_DOUBLE;
      } else if (parameterType[i] == SDDS_FLOAT) {
        parameterhdfType[i] = H5T_NATIVE_FLOAT;
      } else if (parameterType[i] == SDDS_ULONG) {
        parameterhdfType[i] = H5T_NATIVE_UINT;
      } else if (parameterType[i] == SDDS_LONG) {
        parameterhdfType[i] = H5T_NATIVE_INT;
      } else if (parameterType[i] == SDDS_USHORT) {
        parameterhdfType[i] = H5T_NATIVE_USHORT;
      } else if (parameterType[i] == SDDS_SHORT) {
        parameterhdfType[i] = H5T_NATIVE_SHORT;
      } else if (parameterType[i] == SDDS_CHARACTER) {
        parameterhdfType[i] = H5T_NATIVE_CHAR;
      } else if (parameterType[i] == SDDS_STRING) {
        parameterhdfType[i] = H5T_C_S1;
      }
    }
  }

  arrayNames = SDDS_GetArrayNames(&SDDS_table, &narrays);
  if (narrays == 0) {
    if (arrayNames) free(arrayNames);
  }
  if (narrays > 0) {
    arrayType = tmalloc(sizeof(*arrayType)*narrays);
    arrayhdfType = tmalloc(sizeof(*arrayhdfType)*narrays);
    arrayData = tmalloc(sizeof(**arrayData)*narrays);
    arrayUnits = tmalloc(sizeof(*arrayUnits)*narrays);
    array_dims = tmalloc(sizeof(*array_dims)*narrays);

    for (i=0; i<narrays; i++) {
      array_dims[i] = tmalloc(sizeof(*(array_dims[i])) * SDDS_table.layout.array_definition[i].dimensions);
      if ((arrayType[i]=SDDS_GetArrayType(&SDDS_table, i))<=0 ||
          SDDS_GetArrayInformation(&SDDS_table, "units", &arrayUnits[i], SDDS_GET_BY_INDEX, i)!=SDDS_STRING) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      if (arrayType[i] == SDDS_LONGDOUBLE) {
        arrayhdfType[i] = H5T_NATIVE_LDOUBLE;
      } else if (arrayType[i] == SDDS_DOUBLE) {
        arrayhdfType[i] = H5T_NATIVE_DOUBLE;
      } else if (arrayType[i] == SDDS_FLOAT) {
        arrayhdfType[i] = H5T_NATIVE_FLOAT;
      } else if (arrayType[i] == SDDS_ULONG) {
        arrayhdfType[i] = H5T_NATIVE_UINT;
      } else if (arrayType[i] == SDDS_LONG) {
        arrayhdfType[i] = H5T_NATIVE_INT;
      } else if (arrayType[i] == SDDS_USHORT) {
        arrayhdfType[i] = H5T_NATIVE_USHORT;
      } else if (arrayType[i] == SDDS_SHORT) {
        arrayhdfType[i] = H5T_NATIVE_SHORT;
      } else if (arrayType[i] == SDDS_CHARACTER) {
        arrayhdfType[i] = H5T_NATIVE_CHAR;
      } else if (arrayType[i] == SDDS_STRING) {
        arrayhdfType[i] = H5T_C_S1;
      }
    }
  }


  columnNames = SDDS_GetColumnNames(&SDDS_table, &ncolumns);
  if (ncolumns > 0) {
    columnType = tmalloc(sizeof(*columnType)*ncolumns);
    columnhdfType = tmalloc(sizeof(*columnhdfType)*ncolumns);
    columnData = tmalloc(sizeof(*columnData)*ncolumns);
    columnUnits = tmalloc(sizeof(*columnUnits)*ncolumns);

    for (i=0; i<ncolumns; i++) {
      if ((columnType[i]=SDDS_GetColumnType(&SDDS_table, i))<=0 ||
          SDDS_GetColumnInformation(&SDDS_table, "units", &columnUnits[i], SDDS_GET_BY_INDEX, i)!=SDDS_STRING) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      if (columnType[i] == SDDS_LONGDOUBLE) {
        columnhdfType[i] = H5T_NATIVE_LDOUBLE;
      } else if (columnType[i] == SDDS_DOUBLE) {
        columnhdfType[i] = H5T_NATIVE_DOUBLE;
      } else if (columnType[i] == SDDS_FLOAT) {
        columnhdfType[i] = H5T_NATIVE_FLOAT;
      } else if (columnType[i] == SDDS_ULONG) {
        columnhdfType[i] = H5T_NATIVE_UINT;
      } else if (columnType[i] == SDDS_LONG) {
        columnhdfType[i] = H5T_NATIVE_INT;
      } else if (columnType[i] == SDDS_USHORT) {
        columnhdfType[i] = H5T_NATIVE_USHORT;
      } else if (columnType[i] == SDDS_SHORT) {
        columnhdfType[i] = H5T_NATIVE_SHORT;
      } else if (columnType[i] == SDDS_CHARACTER) {
        columnhdfType[i] = H5T_NATIVE_CHAR;
      } else if (columnType[i] == SDDS_STRING) {
        columnhdfType[i] = H5T_C_S1;
      }
    }
  }

  page = SDDS_ReadTable(&SDDS_table);
  if (page <= 0) {
    fprintf(stderr, "No data in SDDS file.\n"); 
    H5Fclose(file_id);
    return(1);
  }

  if (vizschema && vsType==1) {
    vsSC[0] = SDDS_GetColumnIndex(&SDDS_table,vsSpatialColumns[0]);
    vsSC[1] = SDDS_GetColumnIndex(&SDDS_table,vsSpatialColumns[1]);
    vsSC[2] = SDDS_GetColumnIndex(&SDDS_table,vsSpatialColumns[2]);
    for (i=0;i<3;i++) {
      if (vsSC[i] == -1) {
        fprintf(stderr, "Error: spatialColumn %s does not exist\n", vsSpatialColumns[i]); 
        H5Fclose(file_id);
        return(1);
      }
    }
    if (vsStepParameter != NULL) {
      vsTimeIndex[0] = SDDS_GetParameterIndex(&SDDS_table,vsStepParameter);
      if (vsTimeIndex[0] == -1) {
        fprintf(stderr, "Error: vsStep %s parameter does not exist\n", vsStepParameter); 
        H5Fclose(file_id);
        return(1);
      }
    } else {
      vsTimeIndex[0] = -1;
    }
    if (vsTimeParameter != NULL) {
      vsTimeIndex[1] = SDDS_GetParameterIndex(&SDDS_table,vsTimeParameter);
      if (vsTimeIndex[1] == -1) {
        fprintf(stderr, "Error: vsTime %s parameter does not exist\n", vsTimeParameter); 
        H5Fclose(file_id);
        return(1);
      }
    } else {
      vsTimeIndex[1] = -1;
    }
    while (page > 0) {
      sprintf(buffer, "/page%" PRId32, page);
      page_group_id = H5Gcreate(file_id, buffer, 0);


      if (nparameters > 0) {
        for (i=0; i<nparameters; i++) {
          if (SDDS_FLOATING_TYPE(parameterType[i])) {
            parameterData[i]=SDDS_GetParameterAsDouble(&SDDS_table, SDDS_table.layout.parameter_definition[i].name, NULL);
          } else if (SDDS_INTEGER_TYPE(parameterType[i])) {
            parameterData[i]=SDDS_GetParameterAsLong(&SDDS_table, SDDS_table.layout.parameter_definition[i].name, NULL);
          } else {
            parameterData[i]=SDDS_GetParameterAsString(&SDDS_table, SDDS_table.layout.parameter_definition[i].name, NULL);
          }
          if (!(parameterData[i])) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        }
        for (i=0; i<nparameters; i++) {
          if (SDDS_FLOATING_TYPE(parameterType[i])) {
            writeDoubleAttribute(SDDS_table.layout.parameter_definition[i].name, *((double*)(parameterData[i])), page_group_id);
          } else if (SDDS_INTEGER_TYPE(parameterType[i])) {
            writeIntAttribute(SDDS_table.layout.parameter_definition[i].name, *((int32_t*)(parameterData[i])), page_group_id);
          } else {
            writeStringAttribute(SDDS_table.layout.parameter_definition[i].name, ((char*)(parameterData[i])), page_group_id);
          }
          if ((parameterUnits[i] != NULL) && (strlen(parameterUnits[i]) > 0)) {
            sprintf(buffer, "%s_units", SDDS_table.layout.parameter_definition[i].name);
            writeStringAttribute(buffer, parameterUnits[i], page_group_id);
          }
        }
      }

      if (ncolumns > 0) {
        rows = SDDS_RowCount(&SDDS_table);
        column_dims[0] = rows; 
        column_dims[1] = 0; 
        for (i=0; i<ncolumns; i++) {
          if (SDDS_NUMERIC_TYPE(columnType[i])) {
            column_dims[1]++;
            columnData[i]=SDDS_GetColumnInDoubles(&SDDS_table, SDDS_table.layout.column_definition[i].name);
            if (!(columnData[i])) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(1);
            }
          }
        }
        if (column_dims[1] == 0) {
          fprintf(stderr, "No numeric columns in SDDS file.\n"); 
          H5Fclose(file_id);
          return(1);
        }
        column_dataspace_id = H5Screate_simple(2, column_dims, NULL);
        sprintf(buffer, "mesh%" PRId32, page);
        column_dataset_id = H5Dcreate(page_group_id, buffer, H5T_NATIVE_DOUBLE, column_dataspace_id, H5P_DEFAULT);

        offset_file[0] = 0;
        offset_file[1] = 0;
        count_file[0] = rows;
        count_file[1] = 1;
        for (i=0;i<3;i++) {
          if (SDDS_NUMERIC_TYPE(columnType[vsSC[i]])) {
            SDDS_GetColumnInformation(&SDDS_table, "units", &unitBuffer, SDDS_GET_BY_INDEX, vsSC[i]);
            if (offset_file[1] == 0) {
              sprintf(buffer, "%s", SDDS_table.layout.column_definition[vsSC[i]].name);
              if (unitBuffer == NULL) {
                sprintf(unitsBuffer, " ");
              } else {
                sprintf(unitsBuffer, "%s", unitBuffer);
                free(unitBuffer);
              }
            } else {
              strncat(buffer, ", ", sizeof(buffer) - strlen(buffer) - 1);
              strncat(buffer, SDDS_table.layout.column_definition[vsSC[i]].name, sizeof(buffer) - strlen(buffer) - 1);
              //sprintf(buffer, "%s, %s", buffer, SDDS_table.layout.column_definition[vsSC[i]].name);
              if (unitBuffer == NULL) {
                strncat(unitsBuffer, ", ", sizeof(unitsBuffer) - strlen(unitsBuffer) - 1);
                //sprintf(unitsBuffer, "%s, ", unitsBuffer);
              } else {
                strncat(unitsBuffer, ", ", sizeof(unitsBuffer) - strlen(unitsBuffer) - 1);
                strncat(unitsBuffer, unitBuffer, sizeof(unitsBuffer) - strlen(unitsBuffer) - 1);
                //sprintf(unitsBuffer, "%s, %s", unitsBuffer, unitBuffer);
                free(unitBuffer);
              }
            }
            H5Sselect_hyperslab (column_dataspace_id, H5S_SELECT_SET, offset_file, NULL, count_file, NULL);
            column_dataspace_buffer_id = H5Screate_simple(1, column_dims, NULL);
            H5Dwrite(column_dataset_id, H5T_NATIVE_DOUBLE, column_dataspace_buffer_id, column_dataspace_id, H5P_DEFAULT, columnData[vsSC[i]]);
            H5Sclose(column_dataspace_buffer_id);
            offset_file[1]++;
          }

        }


        for (i=0; i<ncolumns; i++) {
          if ((i==vsSC[0]) || (i==vsSC[1]) || (i==vsSC[2])) {
            continue;
          }
          if (SDDS_NUMERIC_TYPE(columnType[i])) {
            SDDS_GetColumnInformation(&SDDS_table, "units", &unitBuffer, SDDS_GET_BY_INDEX, i);
            if (offset_file[1] == 0) {
              sprintf(buffer, "%s", SDDS_table.layout.column_definition[i].name);
              if (unitBuffer == NULL) {
                sprintf(unitsBuffer, " ");
              } else {
                sprintf(unitsBuffer, "%s", unitBuffer);
                free(unitBuffer);
              }
            } else {
              strncat(buffer, ", ", sizeof(buffer) - strlen(buffer) - 1);
              strncat(buffer, SDDS_table.layout.column_definition[i].name, sizeof(buffer) - strlen(buffer) - 1);
              //sprintf(buffer, "%s, %s", buffer, SDDS_table.layout.column_definition[i].name);
              if (unitBuffer == NULL) {
                strncat(unitsBuffer, ", ", sizeof(unitsBuffer) - strlen(unitsBuffer) - 1);
                //sprintf(unitsBuffer, "%s, ", unitsBuffer);
              } else {
                strncat(unitsBuffer, ", ", sizeof(unitsBuffer) - strlen(unitsBuffer) - 1);
                strncat(unitsBuffer, unitBuffer, sizeof(unitsBuffer) - strlen(unitsBuffer) - 1);
                //sprintf(unitsBuffer, "%s, %s", unitsBuffer, unitBuffer);
                free(unitBuffer);
              }
            }
            H5Sselect_hyperslab (column_dataspace_id, H5S_SELECT_SET, offset_file, NULL, count_file, NULL);
            column_dataspace_buffer_id = H5Screate_simple(1, column_dims, NULL);
            H5Dwrite(column_dataset_id, H5T_NATIVE_DOUBLE, column_dataspace_buffer_id, column_dataspace_id, H5P_DEFAULT, columnData[i]);
            H5Sclose(column_dataspace_buffer_id);
            offset_file[1]++;
          }
        }

        writeStringAttribute("vsType", "variableWithMesh", column_dataset_id);
        writeStringAttribute("vsLabels", buffer, column_dataset_id);
        writeIntAttribute("vsNumSpatialDims", 3, column_dataset_id);
        sprintf(buffer, "meshLimits%" PRId32, page);
        writeStringAttribute("vsLimits", buffer, column_dataset_id);
        sprintf(buffer, "meshTime%" PRId32, page);
        writeStringAttribute("vsTimeGroup", buffer, column_dataset_id);
        writeStringAttribute("vsUnits", unitsBuffer, column_dataset_id);

        H5Dclose(column_dataset_id);
        H5Sclose(column_dataspace_id);
        sprintf(buffer, "meshLimits%" PRId32, page);
        limit_group_id = H5Gcreate(page_group_id, buffer, 0);
        writeStringAttribute("vsType", "limits", limit_group_id);
        writeStringAttribute("vsKind", "Cartesian", limit_group_id);
        index_min_max(&indexMin[0], &indexMax[0], columnData[vsSC[0]], rows);
        index_min_max(&indexMin[1], &indexMax[1], columnData[vsSC[1]], rows);
        index_min_max(&indexMin[2], &indexMax[2], columnData[vsSC[2]], rows);
        sprintf(buffer, "%22.15e, %22.15e, %22.15e",  ((double**)columnData)[vsSC[0]][indexMin[0]], ((double**)columnData)[vsSC[1]][indexMin[1]], ((double**)columnData)[vsSC[2]][indexMin[2]]);
        writeStringAttribute("vsLowerBounds", buffer, limit_group_id);
        sprintf(buffer, "%22.15e, %22.15e, %22.15e",  ((double**)columnData)[vsSC[0]][indexMax[0]], ((double**)columnData)[vsSC[1]][indexMax[1]], ((double**)columnData)[vsSC[2]][indexMax[2]]);
        writeStringAttribute("vsUpperBounds", buffer, limit_group_id);
        H5Gclose(limit_group_id);
        for (i=0; i<ncolumns; i++) {
          if (SDDS_NUMERIC_TYPE(columnType[i])) {
            free(columnData[i]);
          }
        }
        
        for (i=0; i<nparameters; i++) {
            free(parameterData[i]);
        }
        
        sprintf(buffer, "meshTime%" PRId32, page);
        time_group_id = H5Gcreate(page_group_id, buffer, 0);
        writeStringAttribute("vsType", "time", time_group_id);
        if (vsTimeIndex[0] != -1) {
          if (SDDS_GetParameterAsLong(&SDDS_table, vsStepParameter, &vsStep)) {
            writeIntAttribute("vsStep", vsStep, time_group_id);
          }
        }
        if (vsTimeIndex[1] != -1) {
          if (!SDDS_GetParameterAsDouble(&SDDS_table, vsTimeParameter, &vsTime)) {
            vsTime=0;
          }
        } else {
          vsTime=0;
        }
        writeDoubleAttribute("vsTime", vsTime, time_group_id);

        H5Gclose(time_group_id);
      }

      H5Gclose(page_group_id);
      page = SDDS_ReadTable(&SDDS_table);
    }
    runinfo_group_id = H5Gcreate(file_id, "/runInfo", 0);
    writeStringAttribute("vsType", "runInfo", runinfo_group_id);
    writeStringAttribute("vsSoftware", "sdds2hdf", runinfo_group_id);
    writeStringAttribute("vsSwVersion", __DATE__, runinfo_group_id);
    writeStringAttribute("vsVsVersion", "2.1.0", runinfo_group_id);
    H5Gclose(runinfo_group_id);

    H5Fclose(file_id);
    if (nparameters > 0) {
      for (i=0; i<nparameters; i++) {
        free(parameterNames[i]);
        if (parameterUnits[i]) free(parameterUnits[i]);
      }
      free(parameterData);
      free(parameterNames);
      free(parameterhdfType);
      free(parameterType);
      free(parameterUnits);
    }
    if (narrays > 0) {
      for (i=0; i<narrays; i++) {
        free(array_dims[i]);
        free(arrayNames[i]);
        if (arrayUnits[i]) free(arrayUnits[i]);
      }
      free(array_dims);
      free(arrayData);
      free(arrayNames);
      free(arrayhdfType);
      free(arrayType);
      free(arrayUnits);
    }
    if (ncolumns > 0) {
      for (i=0; i<ncolumns; i++) {
        free(columnNames[i]);
        if (columnUnits[i]) free(columnUnits[i]);
      }
      free(columnData);
      free(columnNames);
      free(columnhdfType);
      free(columnType);
      free(columnUnits);
    }
    if (vsMeshType) {
      free(vsMeshType);
    }
  } else {
    while (page > 0) {
      sprintf(buffer, "/page%" PRId32, page);
      page_group_id = H5Gcreate(file_id, buffer, 0);
      if (nparameters > 0) {
        for (i=0; i<nparameters; i++) {
          if (parameterType[i] == SDDS_STRING) {
            parameterData[i]=SDDS_GetParameterAsString(&SDDS_table, SDDS_table.layout.parameter_definition[i].name, NULL);
          } else {
            parameterData[i]=SDDS_GetParameter(&SDDS_table, SDDS_table.layout.parameter_definition[i].name, NULL);
          }
          if (!(parameterData[i])) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        }
        parameter_group_id = H5Gcreate(page_group_id, "parameters", 0);
        for (i=0; i<nparameters; i++) {
          parameter_dataspace_id = H5Screate_simple(1, parameter_dims, NULL);
          if (parameterType[i] == SDDS_STRING) {
            stringType = H5Tcopy(H5T_C_S1);
            H5Tset_size(stringType, strlen(parameterData[i]) + 1);
            parameter_dataset_id = H5Dcreate(parameter_group_id, SDDS_table.layout.parameter_definition[i].name, stringType, parameter_dataspace_id, H5P_DEFAULT);
            H5Dwrite(parameter_dataset_id, stringType, H5S_ALL, H5S_ALL, H5P_DEFAULT, parameterData[i]);
          
          } else {
            parameter_dataset_id = H5Dcreate(parameter_group_id, SDDS_table.layout.parameter_definition[i].name, parameterhdfType[i], parameter_dataspace_id, H5P_DEFAULT);
            H5Dwrite(parameter_dataset_id, parameterhdfType[i], H5S_ALL, H5S_ALL, H5P_DEFAULT, parameterData[i]);
          }
          writeStringAttribute("units", parameterUnits[i], parameter_dataset_id);
          H5Dclose(parameter_dataset_id);
          H5Sclose(parameter_dataspace_id);
          free(parameterData[i]);
        }
        H5Gclose(parameter_group_id);
      }
      if (narrays > 0) {
        for (i=0; i<narrays; i++) {
          arrayData[i]=SDDS_GetArray(&SDDS_table, SDDS_table.layout.array_definition[i].name, NULL);
          if (!(arrayData[i])) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        }
        array_group_id = H5Gcreate(page_group_id, "arrays", 0);
        for (i=0; i<narrays; i++) {
          for (j=0; j<SDDS_table.layout.array_definition[i].dimensions; j++) {
            array_dims[i][j] = arrayData[i]->dimension[j];
          }
          array_dataspace_id = H5Screate_simple(SDDS_table.layout.array_definition[i].dimensions, array_dims[i], NULL);
          if (arrayType[i] == SDDS_STRING) {
            stringType = H5Tcopy(H5T_C_S1);
            H5Tset_size(stringType, H5T_VARIABLE);
            array_dataset_id = H5Dcreate(array_group_id, SDDS_table.layout.array_definition[i].name, stringType, array_dataspace_id, H5P_DEFAULT);
            H5Dwrite(array_dataset_id, stringType, H5S_ALL, H5S_ALL, H5P_DEFAULT, arrayData[i]->data);
          } else {
            array_dataset_id = H5Dcreate(array_group_id, SDDS_table.layout.array_definition[i].name, arrayhdfType[i], array_dataspace_id, H5P_DEFAULT);
            H5Dwrite(array_dataset_id, arrayhdfType[i], H5S_ALL, H5S_ALL, H5P_DEFAULT, arrayData[i]->data);
          }
          writeStringAttribute("units", arrayUnits[i], array_dataset_id);
          H5Dclose(array_dataset_id);
          H5Sclose(array_dataspace_id);
          SDDS_FreeArray(arrayData[i]);
        }
        H5Gclose(array_group_id);
      }
      if (ncolumns > 0) {
        rows = SDDS_RowCount(&SDDS_table);
        for (i=0; i<ncolumns; i++) {
          columnData[i]=SDDS_GetColumn(&SDDS_table, SDDS_table.layout.column_definition[i].name);
          if (!(columnData[i])) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        }
        column_group_id = H5Gcreate(page_group_id, "columns", 0);
        for (i=0; i<ncolumns; i++) {
          column_dims[0] = rows; 
          column_dataspace_id = H5Screate_simple(1, column_dims, NULL);
          if (columnType[i] == SDDS_STRING) {
            stringType = H5Tcopy(H5T_C_S1);
            H5Tset_size(stringType, H5T_VARIABLE);
            column_dataset_id = H5Dcreate(column_group_id, SDDS_table.layout.column_definition[i].name, stringType, column_dataspace_id, H5P_DEFAULT);
            H5Dwrite(column_dataset_id, stringType, H5S_ALL, H5S_ALL, H5P_DEFAULT, columnData[i]);
          } else {
            column_dataset_id = H5Dcreate(column_group_id, SDDS_table.layout.column_definition[i].name, columnhdfType[i], column_dataspace_id, H5P_DEFAULT);
            H5Dwrite(column_dataset_id, columnhdfType[i], H5S_ALL, H5S_ALL, H5P_DEFAULT, columnData[i]);
          }
          writeStringAttribute("units", columnUnits[i], column_dataset_id);
          H5Dclose(column_dataset_id);
          H5Sclose(column_dataspace_id);
          if (columnType[i] == SDDS_STRING) {
            SDDS_FreeStringArray(columnData[i], rows);
            free(columnData[i]);
          } else {
            free(columnData[i]);
          }
        }
        H5Gclose(column_group_id);
      }
      H5Gclose(page_group_id);
      page = SDDS_ReadTable(&SDDS_table);
    }
    H5Fclose(file_id);
    if (nparameters > 0) {
      for (i=0; i<nparameters; i++) {
        free(parameterNames[i]);
        if (parameterUnits[i]) free(parameterUnits[i]);
      }
      free(parameterData);
      free(parameterNames);
      free(parameterhdfType);
      free(parameterType);
      free(parameterUnits);
    }
    if (narrays > 0) {
      for (i=0; i<narrays; i++) {
        free(array_dims[i]);
        free(arrayNames[i]);
        if (arrayUnits[i]) free(arrayUnits[i]);
      }
      free(array_dims);
      free(arrayData);
      free(arrayNames);
      free(arrayhdfType);
      free(arrayType);
      free(arrayUnits);
    }
    if (ncolumns > 0) {
      for (i=0; i<ncolumns; i++) {
        free(columnNames[i]);
        if (columnUnits[i]) free(columnUnits[i]);
      }
      free(columnData);
      free(columnNames);
      free(columnhdfType);
      free(columnType);
      free(columnUnits);
    }
  }
  if (SDDS_Terminate(&SDDS_table) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  
  free_scanargs(&s_arg,argc);
  free(output);
  free(input);
  return(0);
}

int writeStringAttribute(const char* name, const char* value, hid_t dataset_id) {
  hid_t attribute_id;
  hid_t atype;
  hid_t a_id;

  a_id  = H5Screate(H5S_SCALAR);
  atype = H5Tcopy(H5T_C_S1);
  if (value == NULL)
    H5Tset_size(atype, 1);
  else
    H5Tset_size(atype, strlen(value) + 1);
  H5Tset_strpad(atype,H5T_STR_NULLTERM);
  attribute_id = H5Acreate(dataset_id, name, atype, a_id, H5P_DEFAULT);
  if (value == NULL)
    H5Awrite(attribute_id, atype, "");
  else
    H5Awrite(attribute_id, atype, value);
  H5Aclose(attribute_id);
  H5Tclose(atype);
  H5Sclose(a_id);

  return(0);
}
int writeIntAttribute(const char* name, long value, hid_t dataset_id) {
  hid_t attribute_id, a_id;

  a_id  = H5Screate(H5S_SCALAR);
  attribute_id = H5Acreate(dataset_id, name, H5T_NATIVE_INT, a_id, H5P_DEFAULT);
  H5Awrite(attribute_id, H5T_NATIVE_INT, &value);
  H5Aclose(attribute_id);
  H5Sclose(a_id);

  return(0);
}

int writeDoubleAttribute(const char* name, double value, hid_t dataset_id) {
  hid_t attribute_id, a_id;

  a_id  = H5Screate(H5S_SCALAR);
  attribute_id = H5Acreate(dataset_id, name, H5T_NATIVE_DOUBLE, a_id, H5P_DEFAULT);
  H5Awrite(attribute_id, H5T_NATIVE_DOUBLE, &value);
  H5Aclose(attribute_id);
  H5Sclose(a_id);

  return(0);
}



