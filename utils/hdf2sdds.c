#include <stdio.h>
#include "hdf5.h"
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#define DATASETNAME "YeeElecFieldData" 

herr_t file_info(hid_t loc_id, const char *name,  void *opdata);
herr_t attr_info(hid_t loc_id, const char *name, void *opdata);

typedef struct {
  int query;
  char *filename;
  char *newfilename;
  SDDS_TABLE *SDDS_table;
  int outputmode;
  char *datasetname;
  int datasetfound;
  char *groupname;
  int withindex;
  long reduceFactor, keep;
  char **attribute_name;
  long attributes, *attribute_dim, *attribute_type, spatialDims, write_3d;
  double xorigin, xdelta, yorigin, ydelta, zorigin, zdelta;
  void **attribute_value;
} GLOBAL_VARS;

int DefineAttributeParameters(GLOBAL_VARS *gv);
int SetAttributeParameters(GLOBAL_VARS *gv);
int FreeAttributesMemory(GLOBAL_VARS *gv);


#define SET_PIPE 0
#define SET_QUERY 1
#define SET_ASCII 2
#define SET_BINARY 3
#define SET_DATASET 4
#define SET_GROUP 5
#define SET_WITHINDEX 6
#define SET_REDUCE_FACTOR 7
#define SET_3D_OUTPUT 8
#define N_OPTIONS 9

char *option[N_OPTIONS] = {
  "pipe", "query", "ascii", "binary", "dataset", "group", "withindex", "reduceFactor", "3doutput",
};

char *USAGE = "hdf2sdds <input-file> [<output-file>]\n[-pipe[=out]] [-dataset=<name>] [-group=<name>] \n[-query] [-ascii] [-binary] [-withIndex] [-reduceFactor=<integer>[,keep=<interger>]] [-3doutput]\n\n-group   The format is similar to a directory format, -group=/entry/data\n\nProgram by Robert Soliday. ("__DATE__")\n";

char *YeeElecFieldData[3]={"Ex", "Ey", "Ez"};
char *YeeMagFieldData[3]={"Bx", "By", "Bz"};
char *TotalRhoJData[4]={"Rho", "Jx", "Jy", "Jz"};
char *electronsData3[6]={"x", "y", "z", "Vx", "Vy", "Vz"};
char *electronsData2[5]={"x", "y", "Vx", "Vy", "Vz"};
char *electronsData1[4]={"x", "Vx", "Vy", "Vz"};
char *ionsData3[6]={"x", "y", "z", "Vx", "Vy", "Vz"};
char *ionsData2[5]={"x", "y", "Vx", "Vy", "Vz"};
char *ionsData1[4]={"x", "Vx", "Vy", "Vz"};

char *myComboFieldData[6]={"Ex", "Ey", "Ez", "Bx", "By", "Bz"};
#define DATASET_TYPES 6
char *dataset_type[DATASET_TYPES]={"YeeElecFieldData", "YeeMagFieldData", "TotalRhoJData", "electronsData", "myComboFieldData", "ionsData"};
char *get_column_prefix(int spaticalDims, int data_type, int index);

int main( int argc, char **argv ) {
  hid_t file_id; 
  herr_t status;
  //int idx_f;
  GLOBAL_VARS gv;
  SDDS_TABLE SDDS_table;
  SCANNED_ARG *s_arg;
  unsigned long pipeFlags=0, dummyFlags=0;
  long i_arg;

  gv.filename = gv.newfilename = gv.datasetname = gv.groupname = NULL;
  gv.query=0;
  gv.outputmode=SDDS_BINARY;
  gv.datasetfound=0;
  gv.withindex=0;
  gv.reduceFactor = gv.keep = 1;
  gv.attributes  = 0;
  gv.attribute_name = NULL;
  gv.attribute_value = NULL;
  gv.attribute_dim =gv.attribute_type =  NULL;
  gv.spatialDims = 3;
  gv.write_3d = 0;
  gv.xorigin = gv.yorigin = gv.zorigin = gv.xdelta = gv.ydelta = gv.zdelta = 0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc<3) {
    fprintf(stderr, "%s", USAGE);
    return(1);
  }

  for (i_arg=1; i_arg<argc; i_arg++) {
    if (s_arg[i_arg].arg_type==OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_DATASET:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "invalid -dataset syntax\n");
          return(1);
        }
        gv.datasetname = s_arg[i_arg].list[1];
        break;
      case SET_GROUP:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "invalid -group syntax\n");
          return(1);
        }
        gv.groupname = s_arg[i_arg].list[1];
        break;
      case SET_REDUCE_FACTOR:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "invalid -reduceFactor syntax\n");
          return(1);
        }
        if (!get_long(&gv.reduceFactor,  s_arg[i_arg].list[1])) {
          fprintf(stderr, "invalid -reduceFactor value given\n");
          return(1);
        }
        s_arg[i_arg].n_items -=2;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list+2, &s_arg[i_arg].n_items, 0, 
                          "keep", SDDS_LONG, &gv.keep, 1, 0, NULL) ||
            gv.keep>gv.reduceFactor)
          SDDS_Bomb("invalid -append syntax/value");
        s_arg[i_arg].n_items +=2;
        break;
      case SET_WITHINDEX:
        gv.withindex = 1;
        break;
      case SET_BINARY:
        gv.outputmode = SDDS_BINARY;
        break;
      case SET_ASCII:
        gv.outputmode = SDDS_ASCII;
        break;
      case SET_QUERY:
        gv.query=1; 
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list+1, s_arg[i_arg].n_items-1, &pipeFlags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          return(1);
        }
        break;
      case SET_3D_OUTPUT:
        gv.write_3d=1;
        break;
      default:
	fprintf(stderr, "Error: unknown switch: %s\n", s_arg[i_arg].list[0]);
	return(1);
      }
    } else {
      if (gv.filename == NULL)
	gv.filename = s_arg[i_arg].list[0];
      else if (gv.newfilename == NULL)
	gv.newfilename = s_arg[i_arg].list[0];
      else {
	fprintf(stderr, "Error: too many filenames\n");
	return(1);
      }
    }
  }
  if (!gv.query) {
    /*if (gv.datasetname == NULL) {
      fprintf(stderr, "Error: -dataset option is missing\n");
      return(1);
      } */
    processFilenames("hdf2sdds", &(gv.filename), &(gv.newfilename), pipeFlags, 0, NULL);
  }

  gv.SDDS_table = &SDDS_table;
  
  status = H5Fis_hdf5(gv.filename);
  if (status <= 0) {
    fprintf(stderr, "unable to open %s, it is not an HDF5 file\n", gv.filename);
    return(1);
  }

  file_id = H5Fopen (gv.filename, H5F_ACC_RDONLY, H5P_DEFAULT); 
  if (file_id < 0) {
    fprintf(stderr, "unable to open %s\n", gv.filename);
    return(1);
  }
  if (!(gv.groupname)) {
    H5Giterate(file_id, "/", NULL, file_info, &gv);
  } else {
    H5Giterate(file_id, gv.groupname, NULL, file_info, &gv);
  }
  status = H5Fclose (file_id);
  if (status) {
    fprintf(stderr, "unable to close %s\n", gv.filename);
    return(1);
  }

  if (!gv.query) {
    if (gv.datasetfound == 0) {
      fprintf(stderr, "Error: unable to find dataset %s\n", gv.datasetname);
      return(1);
    }
  }

  return(0);
}

herr_t file_info(hid_t loc_id, const char *name, void *opdata) {
  int rank, i, k, m, n=1, index=0, index2, attrs, dim3;
  long j;
  int64_t rows=0;
  int64_t total_rows=0;
  long II;
  long J;
  long K;
  hid_t dataset, datatype, dataspace, nativetype; 
  herr_t status;
  H5T_class_t t_class;
  //H5T_order_t order;
  size_t size;
  hsize_t *dims_out;
  GLOBAL_VARS *gv;
  char buffer[40], buffer1[256];
  void *dataout;
  int sddstype, data_type, columns=0, Nx, Ny;
  H5G_stat_t statbuf;
  double xpos, ypos, zpos;
  int len;

  gv = (GLOBAL_VARS *)opdata;
  xpos = ypos = zpos = 0;
  Nx = Ny =0;
  buffer1[0] = 0;
  if (gv->query) {
    fprintf(stdout, "Name: %s\n", name);
  } else {
    if (!(gv->datasetname))
      SDDS_CopyString(&gv->datasetname, name);
    else if (strcmp(name, gv->datasetname) != 0) {
      return(0);
    }
    gv->datasetfound=1;
  }
  
  H5Gget_objinfo(loc_id, name, 0, &statbuf);
  if (gv->query) {
    switch (statbuf.type) {
    case H5G_GROUP:
      fprintf(stdout, "Type: group\n");
      break;
    case H5G_DATASET:
      fprintf(stdout, "Type: dataset\n");
      break;
    case H5G_TYPE:
      fprintf(stdout, "Type: type\n");
      break;
    default:
      fprintf(stdout, "Type: unknown\n");
    }
  }

  if (statbuf.type != H5G_DATASET) {
    fprintf(stderr, "HDF buffer type is not dataset, can not convert to SDDS.\n");
    return(0);
  }

  dataset = H5Dopen(loc_id, name);
  if (dataset < 0) {
    fprintf(stderr, "unable to open dataset %s\n", name);
    return(1);
  }

  datatype = H5Dget_type(dataset);
  t_class = H5Tget_class(datatype);
  //order = H5Tget_order(datatype);
  size = H5Tget_size(datatype);
  nativetype = H5Tget_native_type(datatype, H5T_DIR_DESCEND);
  if (gv->query) {
    if (t_class == H5T_INTEGER) 
      fprintf(stdout, "Data set has interger type \n");
    else if (t_class == H5T_FLOAT) 
      fprintf(stdout, "Data set has floating-point type \n");
    else {
      fprintf(stderr, "Data set is an unsupported type and cannot be converted to SDDS\n");
      return(1);
    }
  }
  
  dataspace = H5Dget_space(dataset);
  if  (dataspace < 0) {
    fprintf(stderr, "unable get the dataspace for %s\n", name);
    return(1);
  }
  rank = H5Sget_simple_extent_ndims(dataspace);
  dims_out = malloc(sizeof(hsize_t)*rank);
  H5Sget_simple_extent_dims(dataspace, dims_out, NULL);
  for (i = 0; i < rank - 1; i++) {
    len = strlen(buffer1);
    snprintf(buffer1 + len, sizeof(buffer1) - len, "%d x ", (int)dims_out[i]);
    //sprintf(buffer1, "%s%d x ", buffer1,  (int)dims_out[i]);
  }
  len = strlen(buffer1);
  snprintf(buffer1 + len, sizeof(buffer1) - len, "%d", (int)dims_out[rank - 1]);
  if (gv->query) {
    fprintf(stdout, "No. of dimensions: %d\n", rank);
    fprintf(stdout, "Dimension sizes: %s\n", buffer1);
  }
  
  attrs = H5Aget_num_attrs(dataset);
  if (attrs < 0) {
    fprintf(stderr, "unable to read attributes for %s\n", name);
    return(1);
  }
  if (gv->query) {
    fprintf(stdout, "%d attributes\n", attrs);
  }

  for (i = 0; i < rank; i++) {
    n = n * (int)dims_out[i];
  }

  if ((t_class == H5T_FLOAT) && (size == 8)) {
    nativetype = H5T_NATIVE_DOUBLE;
    dataout = (double*)malloc(sizeof(double)*n);
    status = H5Dread(dataset, nativetype, H5S_ALL, H5S_ALL, H5P_DEFAULT, (double*)dataout);
    sddstype = SDDS_DOUBLE;
  } else if ((t_class == H5T_FLOAT) && (size == 4)) {
    nativetype = H5T_NATIVE_FLOAT;
    dataout = (float*)malloc(sizeof(float)*n);
    status = H5Dread(dataset, nativetype, H5S_ALL, H5S_ALL, H5P_DEFAULT, (float*)dataout);
    sddstype = SDDS_FLOAT;
  } else if ((t_class == H5T_INTEGER) && (size == 4)) {
    nativetype = H5T_NATIVE_INT;
    dataout = (int32_t*)malloc(sizeof(int32_t)*n);
    status = H5Dread(dataset, nativetype, H5S_ALL, H5S_ALL, H5P_DEFAULT, (int32_t*)dataout);
    sddstype = SDDS_LONG;
  } else if ((t_class == H5T_INTEGER) && (size == 2)) {
    dataout = (short*)malloc(sizeof(short)*n);
    if (H5Tequal(nativetype,H5T_NATIVE_USHORT)) {
      nativetype = H5T_NATIVE_USHORT;
      status = H5Dread(dataset, nativetype, H5S_ALL, H5S_ALL, H5P_DEFAULT, (unsigned short*)dataout);
      sddstype = SDDS_USHORT;
    } else {
      nativetype = H5T_NATIVE_SHORT;
      status = H5Dread(dataset, nativetype, H5S_ALL, H5S_ALL, H5P_DEFAULT, (short*)dataout);
      sddstype = SDDS_SHORT;
    }
  } else {
    fprintf(stderr, "Data set is an unsupported type and cannot be converted to SDDS\n");
    return(1);
  }
  if (status < 0) {
    fprintf(stderr, "unable to read dataset %s\n", name);
    return(1);
  }
  /*fprintf(stderr, "rank=%d\n", rank);*/
  if (rank == 4) {
    if (!gv->query) {
      if (SDDS_InitializeOutput(gv->SDDS_table, gv->outputmode, 1, (char*)name, NULL, gv->newfilename) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
      SDDS_DefineParameter(gv->SDDS_table, "NumOfDimension", NULL, NULL, NULL, NULL, SDDS_LONG, NULL);
      SDDS_DefineParameter(gv->SDDS_table, "OtherDims", NULL, NULL, NULL, NULL, SDDS_LONG, NULL);
      SDDS_DefineParameter(gv->SDDS_table, "DimensionSizes", NULL, NULL, NULL, NULL, SDDS_STRING, buffer1);
    }
    
    H5Aiterate(dataset, NULL, attr_info, gv);
    
    if (!gv->query) {
      DefineAttributeParameters(gv);
      if (gv->withindex) {
        if (SDDS_DefineColumn(gv->SDDS_table, "Index", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == -1) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
      }
    
      data_type = match_string(gv->datasetname, dataset_type, DATASET_TYPES, EXACT_MATCH);
      if (wild_match(gv->datasetname, "ions*Data"))
        data_type = 5;
      if (gv->write_3d && data_type!=3 && data_type!=5) {
        /* for electrons and ions data, the data is already 3d output, i.e., x,y,z,vx, vy, vz, no need convert */
        if (SDDS_DefineColumn(gv->SDDS_table, "x", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1 ||
            SDDS_DefineColumn(gv->SDDS_table, "y", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1 ||
            SDDS_DefineColumn(gv->SDDS_table, "z", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        for (j=0; j<(int)dims_out[3]; j++) {
          if (data_type<0)
            sprintf(buffer, "D%ld", j);
          else  
            strcpy(buffer, get_column_prefix(gv->spatialDims, data_type, j));
          if (SDDS_DefineColumn(gv->SDDS_table, buffer, NULL, NULL, NULL, NULL, sddstype, 0) == -1) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
        }
        total_rows = ((int)dims_out[0])*((int)dims_out[1])*((int)dims_out[2]);
      } else {
        for (i=1 ; i<=(int)dims_out[1] ; i++) {
          for (j=1 ; j<=(int)dims_out[3] ; j++) {
            if (data_type<0)
              sprintf(buffer, "D%ld_%d", j, i);
            else
              sprintf(buffer, "%s_%d", get_column_prefix(gv->spatialDims, data_type, j-1), i);
            if (SDDS_DefineColumn(gv->SDDS_table, buffer, NULL, NULL, NULL, NULL, sddstype, 0) == -1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
        }
      }
      if (SDDS_WriteLayout(gv->SDDS_table) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
      if (gv->write_3d && data_type!=3 && data_type!=5) {
        if (SDDS_StartTable(gv->SDDS_table, total_rows) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, "NumOfDimension", rank, 
                                "OtherDims", (int)dims_out[3], NULL)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        SetAttributeParameters(gv);
      }
      rows = 0;
      xpos = ypos = zpos = 0;
      for (i=0 ; i<(int)dims_out[2] ; i++) {
        K = i;
        zpos = K * gv->zdelta + gv->zorigin;
        if (!gv->write_3d || data_type==3 || data_type==5) {
          if (SDDS_StartTable(gv->SDDS_table, (int)dims_out[0]) == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          } 
          if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, "NumOfDimension", rank, 
                                  "OtherDims", (int)dims_out[3], NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
          SetAttributeParameters(gv); 
          rows = 0;
        }
        for (j=0 ; j<(int)dims_out[0] ; j++) {
          II = j;
          xpos = II * gv->xdelta + gv->xorigin;
          if (gv->reduceFactor>1 && (j % (gv->reduceFactor))!=(gv->keep-1))
            continue;
          if (!gv->write_3d || data_type==3 || data_type==5) {
            index = 0;
            if (gv->withindex) {
              SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                rows, index, j, -1);
              index++;
            }
          }
          for (k=0 ; k<(int)dims_out[1]; k++) {
            J = k;
            ypos = J * gv->ydelta + gv->yorigin;
            if (gv->write_3d && data_type!=3 && data_type!=5) {
              index = 0;
              if (gv->withindex) {
                if (!SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                       rows, index, j, -1))
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
                index++;
              }
              if (!SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     rows, index, xpos, -1))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
              index ++;
              if (!SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     rows, index, ypos, -1))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
              index ++;
              if (!SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     rows, index, zpos, -1))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
              index ++;
            }
            for (m=0 ; m<(int)dims_out[3]; m++) {
              index2 = j*(int)dims_out[1]*(int)dims_out[2]*(int)dims_out[3] + k*(int)dims_out[2]*(int)dims_out[3] + i*(int)dims_out[3] + m;
              switch (sddstype) {
              case SDDS_DOUBLE:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((double*)dataout)[index2], -1);
                break;
              case SDDS_FLOAT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((float*)dataout)[index2], -1);
                break;
              case SDDS_LONG:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((int32_t*)dataout)[index2], -1);
                break;
              case SDDS_SHORT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((short*)dataout)[index2], -1);
                break;
              case SDDS_USHORT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((unsigned short*)dataout)[index2], -1);
                break;
              }
              index++;
            }
            if (gv->write_3d && data_type!=3 && data_type!=5)
              rows ++;
          }
          if (!gv->write_3d || data_type==3 || data_type==5)
            rows++;
        }
        if (!gv->write_3d || data_type==3 || data_type==5)
          if (SDDS_WriteTable(gv->SDDS_table) == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
      }
      if (gv->write_3d && data_type!=3 && data_type!=5)
        if (SDDS_WriteTable(gv->SDDS_table) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
      if (SDDS_Terminate(gv->SDDS_table) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
      FreeAttributesMemory(gv);
    }
  } else if (rank == 2 || rank==3) {
    if (!gv->query) {
      if (SDDS_InitializeOutput(gv->SDDS_table, gv->outputmode, 1, (char*)name, NULL, gv->newfilename) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
      SDDS_DefineParameter(gv->SDDS_table, "NumOfDimension", NULL, NULL, NULL, NULL, SDDS_LONG,  NULL);
      SDDS_DefineParameter(gv->SDDS_table, "OtherDims", NULL, NULL, NULL, NULL, SDDS_LONG, NULL);
      SDDS_DefineParameter(gv->SDDS_table, "DimensionSizes", NULL, NULL, NULL, NULL, SDDS_STRING, buffer1);
      if (gv->withindex) {
        if (SDDS_DefineColumn(gv->SDDS_table, "Index", NULL, NULL, NULL, NULL, SDDS_LONG, 0) == -1) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
      }
    }
    
    H5Aiterate(dataset, NULL, attr_info, gv);
    if (!gv->query) {
      
      data_type = match_string(gv->datasetname, dataset_type, DATASET_TYPES, EXACT_MATCH);

      if (wild_match(gv->datasetname, "ions*Data"))
        data_type = 5;
      Nx = (int)dims_out[0];
      if (rank==3) {
        dim3 = (int)dims_out[2];
        columns = (int)dims_out[2];
        Ny = (int)dims_out[1];
      } else {
        dim3 = 1;
        columns = (int)dims_out[1];
        Ny = 1;
      }
      DefineAttributeParameters(gv);
      if (gv->write_3d && data_type!=3 && data_type!=5) {
        /* for electrons and ions data, the data is already 3d output, i.e., x,y,z,vx, vy, vz, no need convert */
        if (SDDS_DefineColumn(gv->SDDS_table, "x", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1 || 
            (rank==3 && SDDS_DefineColumn(gv->SDDS_table, "y", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
		
        for (i=1; i<=columns; i++) {
          if (data_type<0)
            sprintf(buffer, "D%d", i);
          else  
            strcpy(buffer, get_column_prefix(gv->spatialDims, data_type, i-1));
          if (SDDS_DefineColumn(gv->SDDS_table, buffer, NULL, NULL, NULL, NULL, sddstype, 0) == -1) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
        }
      } else {
        for (i=1 ; i<=(int)dims_out[1] ; i++) {
          for (j=1; j<=dim3; j++) {
            if (data_type<0) {
              if (dim3>1)
                sprintf(buffer, "D%ld_%d", j, i);
              else
                sprintf(buffer, "D%d", i);
            } else {
              if (dim3>1) 
                sprintf(buffer, "%s_%d", get_column_prefix(gv->spatialDims, data_type, j-1), i);
              else
                sprintf(buffer, "%s", get_column_prefix(gv->spatialDims, data_type, i-1));
            }
            if (SDDS_DefineColumn(gv->SDDS_table, buffer, NULL, NULL, NULL, NULL, sddstype, 0) == -1) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return(1);
            }
          }
        }
      }
      if (SDDS_WriteLayout(gv->SDDS_table) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
      if (gv->write_3d && data_type!=3 && data_type!=5)
        total_rows = Nx * Ny;
      else
        total_rows = Nx;
	  
      if (SDDS_StartTable(gv->SDDS_table, total_rows) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
      if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, "NumOfDimension", rank, 
                              "OtherDims", dim3, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
      SetAttributeParameters(gv);
      rows = 0;
      xpos = ypos = 0;
      if (!gv->write_3d || data_type==3 || data_type==5) {
        for (j=0 ; j<(int)dims_out[0] ; j++) {
          index = 0;
          if (gv->reduceFactor>1 && (j % (gv->reduceFactor))!=(gv->keep -1))
            continue;
          if (gv->withindex) {
            SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                              rows, index, j, -1);
            index++;
          }
          for (k=0 ; k<(int)dims_out[1]; k++) {
            for (m=0; m<dim3; m++) {
              index2 = j*(int)dims_out[1]*dim3 + k*dim3 + m;
              switch (sddstype) {
              case SDDS_DOUBLE:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((double*)dataout)[index2], -1);
                break;
              case SDDS_FLOAT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((float*)dataout)[index2], -1);
                break;
              case SDDS_LONG:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((int32_t*)dataout)[index2], -1);
                break;
              case SDDS_SHORT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((short*)dataout)[index2], -1);
                break;
              case SDDS_USHORT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((unsigned short*)dataout)[index2], -1);
                break;
              }
              index ++;
            }
          }
          rows ++;
        }
        if (SDDS_WriteTable(gv->SDDS_table) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        if (SDDS_Terminate(gv->SDDS_table) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        FreeAttributesMemory(gv);
      } else {
        rows = 0;
        if (rank==2) {
          for (j=0 ; j<(int)dims_out[0] ; j++) {
            II = j;
            index=0;
            if (gv->reduceFactor>1 && (j % (gv->reduceFactor))!=(gv->keep -1))
              continue;
            if (gv->withindex) {
              SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                rows, index, j, -1);
              index++;
            }
            xpos = II*gv->xdelta + gv->xorigin;
            if (!SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                   rows, index, xpos, -1))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
            index ++;
            for (k=0 ; k<(int)dims_out[1]; k++) {
              index2 = j*(int)dims_out[1] + k;
              switch (sddstype) {
              case SDDS_DOUBLE:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((double*)dataout)[index2], -1);
                break;
              case SDDS_FLOAT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((float*)dataout)[index2], -1);
                break;
              case SDDS_LONG:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((int32_t*)dataout)[index2], -1);
                break;
              case SDDS_SHORT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((short*)dataout)[index2], -1);
                break;
              case SDDS_USHORT:
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, ((unsigned short*)dataout)[index2], -1);
                break;
              }
              index ++;
            }
            rows ++;
          }
        } else {
          /*rank == 3 */
          for (j=0 ; j<(int)dims_out[0] ; j++) {
            II = j;
            if (gv->reduceFactor>1 && (j % (gv->reduceFactor))!=(gv->keep -1))
              continue;
            xpos = II*gv->xdelta + gv->xorigin;
            for (k=0 ; k<(int)dims_out[1]; k++) {
              J = k;
              ypos = J * gv->ydelta + gv->yorigin;
              index = 0;
              if (gv->withindex) {
                SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  rows, index, j, -1);
                index++;
              }
              if (!SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     rows, index, xpos, -1))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
              index ++;
              if (!SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     rows, index, ypos, -1))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
              index ++;
              for (m=0; m<dim3; m++) {
                index2 = j*(int)dims_out[1]*dim3 + k*dim3 + m;
                switch (sddstype) {
                case SDDS_DOUBLE:
                  SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    rows, index, ((double*)dataout)[index2], -1);
                  break;
                case SDDS_FLOAT:
                  SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    rows, index, ((float*)dataout)[index2], -1);
                  break;
                case SDDS_LONG:
                  SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    rows, index, ((int32_t*)dataout)[index2], -1);
                  break;
                case SDDS_SHORT:
                  SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    rows, index, ((short*)dataout)[index2], -1);
                  break;
                case SDDS_USHORT:
                  SDDS_SetRowValues(gv->SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    rows, index, ((unsigned short*)dataout)[index2], -1);
                  break;
                }
                index ++;
              } /* end of dim[2] */
              rows ++;
            } /* end of dim[1] */
          } /* end dim[0] */
        }
        if (SDDS_WriteTable(gv->SDDS_table) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        if (SDDS_Terminate(gv->SDDS_table) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
        FreeAttributesMemory(gv);
      }
    }
  } else {
    if (gv->query) {
      H5Aiterate(dataset, NULL, attr_info, gv);
    }
    fprintf(stderr, "unable to convert HDF file of %d dimensions\n", rank);
    return(1);    
  }
  
  status = H5Dclose(dataset);
  if (status < 0) {
    fprintf(stderr, "unable to close dataset %s\n", name);
    return(1);
  }
  
  return(0);
}

herr_t attr_info(hid_t loc_id, const char *name, void *opdata) {
  hid_t attr, atype, aspace;
  int rank, i;
  hsize_t *dims_out;
  GLOBAL_VARS *gv;
  herr_t status;
  //H5S_class_t class;
  size_t npoints=0, size;
  double *double_array;
  float *float_array;
  //long *long_array;
  int *int_array;
  void *attribute_data=NULL;
  long attribute_type=0;

  gv = (GLOBAL_VARS *)opdata;

  attr = H5Aopen_name(loc_id, name);
  
  if (gv->query) {
    fprintf(stdout, "Attribute name: %s\n", name);
  }
  
  /* Get attribute datatype, dataspace, rank, and dimensions.  */
  atype  = H5Aget_type(attr);
  size = H5Tget_size(atype);
  aspace = H5Aget_space(attr);
  rank = H5Sget_simple_extent_ndims(aspace);
  dims_out = malloc(sizeof(hsize_t)*rank);
  status = H5Sget_simple_extent_dims(aspace, dims_out, NULL);

  /* Get dataspace type */
  //class = H5Sget_simple_extent_type (aspace);
  
  if ((H5T_FLOAT == H5Tget_class(atype)) && (size == 8)) {
    npoints = H5Sget_simple_extent_npoints(aspace);
    double_array = (double *)malloc(sizeof(double)*(int)npoints); 
    status = H5Aread(attr, H5T_NATIVE_DOUBLE, double_array);
    if (gv->query) {
      printf("        Values: ");
      for( i = 0; i < (int)npoints; i++) printf("%le ", double_array[i]); 
      printf("\n");
    }
    if (strcmp((char*)name, "numSpatialDims")==0) 
      gv->spatialDims = (int)(double_array[0]);
    if (!gv->query) {
      attribute_data = (double*)double_array;
      attribute_type = SDDS_DOUBLE;
    }
  } else if ((H5T_FLOAT == H5Tget_class(atype)) && (size == 4)) {
    npoints = H5Sget_simple_extent_npoints(aspace);
    float_array = (float *)malloc(sizeof(float)*(int)npoints); 
    status = H5Aread(attr, H5T_NATIVE_FLOAT, float_array);
    if (gv->query) {
      printf("        Values: ");
      for( i = 0; i < (int)npoints; i++) printf("%le ", float_array[i]); 
      printf("\n");
    }
    if (!gv->query) { 
      attribute_data = (float*)float_array;
      attribute_type = SDDS_FLOAT;
    }
  } else if ((H5T_INTEGER == H5Tget_class(atype)) && (size == 4)) {
    npoints = H5Sget_simple_extent_npoints(aspace);
    int_array = (int *)malloc(sizeof(int)*(int)npoints); 
    status = H5Aread(attr, H5T_NATIVE_INT, int_array);
    if (gv->query) {
      printf("        Values: ");
      for( i = 0; i < (int)npoints; i++) printf("%d ", int_array[i]); 
      printf("\n");
    }
    if (strcmp((char*)name, "numSpatialDims")==0) 
      gv->spatialDims = (int)(int_array[0]);
    if (!gv->query) {
      attribute_data = (int*)int_array;
      attribute_type = SDDS_LONG;
    }   
  } else if (H5T_STRING == H5Tget_class(atype)) {
    fprintf(stderr, "Warning: unable to convert string attribute %s to SDDS (not yet supported)\n", name);
  } else {
    fprintf(stderr, "Unable to convert attribute %s to SDDS\n", name);
    return 1;
  }
  if (gv->query) {
    fprintf(stdout, "\n");
  } else {
    gv->attribute_name = SDDS_Realloc(gv->attribute_name, sizeof(*(gv->attribute_name))*(gv->attributes+1));
    gv->attribute_value = SDDS_Realloc(gv->attribute_value, sizeof(*(gv->attribute_value))*(gv->attributes+1));
    gv->attribute_dim = SDDS_Realloc(gv->attribute_dim, sizeof(*(gv->attribute_dim))*(gv->attributes+1));
    gv->attribute_type =  SDDS_Realloc(gv->attribute_type, sizeof(*(gv->attribute_type))*(gv->attributes+1));
    SDDS_CopyString(&(gv->attribute_name[gv->attributes]), (char*)name);
    gv->attribute_type[gv->attributes] = attribute_type;
    gv->attribute_dim[gv->attributes] = (int)(npoints);
    gv->attribute_value[gv->attributes]= attribute_data;
    gv->attributes ++;
  }
  
  status = H5Aclose(attr);
  if (status < 0) {
    fprintf(stderr, "unable to close attribute %s\n", name);
    return(1);
  }
  return(0);
}

char *get_column_prefix(int spatialDims, int data_type, int index) {
  switch (data_type) {
  case 0:
    return YeeElecFieldData[index];
  case 1:
    return YeeMagFieldData[index];
  case 2:
    return TotalRhoJData[index];
  case 3:
    switch (spatialDims) {
    case 1:
      return electronsData1[index];
    case 2:
      return electronsData2[index];
    case 3:
      return electronsData3[index];
    default:
      fprintf(stderr, "Invalid spatial dimensions!\n");
      exit(1);
    }
    break;
  case 4: 
    return myComboFieldData[index];
  case 5:
    switch (spatialDims) {
    case 1:
      return ionsData1[index];
    case 2:
      return ionsData2[index];
    case 3:
      return ionsData3[index];
    default:
      fprintf(stderr, "Invalid spatial dimensions!\n");
      exit(1);
    }
    break;
  default:
    fprintf(stderr,  "unknown dataset provided.\n");
    exit(1);
    break;
  }
  return NULL;
}

int DefineAttributeParameters(GLOBAL_VARS *gv)
{
  int i, j;
  char buffer[256]; 
  if (SDDS_DefineParameter(gv->SDDS_table, "ReduceFactor", NULL, NULL, NULL, NULL, SDDS_LONG, NULL)<0 ||
      SDDS_DefineParameter(gv->SDDS_table, "ElementhKeeped", NULL, NULL, NULL, NULL, SDDS_LONG, NULL)<0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  for (i=0; i<gv->attributes; i++) {
    for (j=0; j<gv->attribute_dim[i]; j++) {
      if (gv->attribute_dim[i]==1)
	sprintf(buffer,"%s", gv->attribute_name[i]);
      else
	sprintf(buffer,"%s%d", gv->attribute_name[i], j+1);
      if (SDDS_DefineParameter(gv->SDDS_table, buffer, NULL, NULL, NULL, NULL, gv->attribute_type[i], NULL)<0)
	SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    }
  }
  return 0;
}

int SetAttributeParameters(GLOBAL_VARS *gv)
{
  int i, j;
  char buffer[256]; 
  double attribute_value;
  if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, "ReduceFactor", gv->reduceFactor,
                          "ElementhKeeped", gv->keep, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  for (i=0; i<gv->attributes; i++) {
    for (j=0; j<gv->attribute_dim[i]; j++) {
      if (gv->attribute_dim[i]==1)
        sprintf(buffer,"%s", gv->attribute_name[i]);
      else
        sprintf(buffer,"%s%d", gv->attribute_name[i], j+1);
      switch (gv->attribute_type[i]) {
      case SDDS_DOUBLE:
        if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, buffer, ((double*)gv->attribute_value[i])[j], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        attribute_value = ((double*)gv->attribute_value[i])[j];
        break;
      case SDDS_FLOAT:
        if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, buffer, ((float*)gv->attribute_value[i])[j], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        attribute_value = ((float*)gv->attribute_value[i])[j];
        break;
      case SDDS_LONG:
        if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, buffer, ((int32_t*)gv->attribute_value[i])[j], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        attribute_value = ((int32_t*)gv->attribute_value[i])[j];
        break;
      case SDDS_SHORT:
        if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, buffer, ((short*)gv->attribute_value[i])[j], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        attribute_value = ((short*)gv->attribute_value[i])[j];
        break;
      case SDDS_USHORT:
        if (!SDDS_SetParameters(gv->SDDS_table, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, buffer, ((unsigned short*)gv->attribute_value[i])[j], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        attribute_value = ((unsigned short*)gv->attribute_value[i])[j];
        break;
      default:
        fprintf(stderr, "Non enumeric data type for %s\n", gv->attribute_name[i]);
        return 1;
      }
      if (gv->write_3d) {
        if (strcmp(gv->attribute_name[i], "origin")==0) {
          if (j==0) 
            gv->xorigin = attribute_value;
          else if (j==1)
            gv->yorigin = attribute_value;
          else if (j==2)
            gv->zorigin = attribute_value;
        } else if (strcmp(gv->attribute_name[i], "delta")==0) {
          if (j==0)
            gv->xdelta = attribute_value;
          else if (j==1)
            gv->ydelta = attribute_value;
          else if (j==2)
            gv->zdelta = attribute_value;
        }
      }
    }
  }
  return 0;
}

int FreeAttributesMemory(GLOBAL_VARS *gv)
{
  int i;
  if (!gv->attributes)
    return 0;
  for (i=0; i<gv->attributes; i++) {
    free(gv->attribute_name[i]);
    switch (gv->attribute_type[i]) {
    case SDDS_DOUBLE:
      free((double*)(gv->attribute_value[i]));
      break;
    case SDDS_FLOAT:
      free((float*)(gv->attribute_value[i]));
      break;
    case SDDS_LONG:
      free((int32_t*)(gv->attribute_value[i]));
      break;
    case SDDS_SHORT:
      free((short*)(gv->attribute_value[i]));
      break;
    case SDDS_USHORT:
      free((unsigned short*)(gv->attribute_value[i]));
      break;
    }
  }
  free(gv->attribute_name);
  free(gv->attribute_value);
  free(gv->attribute_type);
  free(gv->attribute_dim);
  return 0;
}
