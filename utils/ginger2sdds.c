#include <stdio.h>
#include "hdf5.h"
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

int extractHDFtoSDDS(char *inputfile, char *outputfile, char *groupname, char *datasetname, int withindex);

#define SET_WITHINDEX 0
#define N_OPTIONS 1
char *option[N_OPTIONS] = {
  "withindex"
};
char *USAGE = "ginger2sdds <input-file> [<output-file-prefix>] [-withIndex]\n\nProgram by Robert Soliday. ("__DATE__")\n";

int main( int argc, char **argv ) {
  SCANNED_ARG *s_arg;
  long i_arg;
  int withindex = 0;
  char *input = NULL;
  char *output = NULL;
  char buffer[1024];

  argc = scanargs(&s_arg, argc, argv);
  if (argc<3) {
    fprintf(stderr, "%s", USAGE);
    return(1);
  }
  for (i_arg=1; i_arg<argc; i_arg++) {
    if (s_arg[i_arg].arg_type==OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_WITHINDEX:
        withindex = 1;
        break;
      default:
	fprintf(stderr, "Error: unknown switch: %s\n", s_arg[i_arg].list[0]);
	return(1);
      }
    } else {
      if (input == NULL)
	input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else {
	fprintf(stderr, "Error: too many filenames\n");
	return(1);
      }
    }
  }

  sprintf(buffer, "%s.base_param.int_param_buf.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "base_param", "int_param_buf", withindex) == 1) {
    return(1);
  }
  sprintf(buffer, "%s.base_param.real_param_buf.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "base_param", "real_param_buf", withindex) == 1) {
    return(1);
  }
  sprintf(buffer, "%s.grids.3Dfld_zgrid.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "grids", "3Dfld_zgrid", withindex) == 1) {
    return(1);
  }
  sprintf(buffer, "%s.grids.rgrid.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "grids", "rgrid", withindex) == 1) {
    return(1);
  }
  sprintf(buffer, "%s.grids.scalar_zgrid.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "grids", "scalar_zgrid", withindex) == 1) {
    return(1);
  }
  sprintf(buffer, "%s.particles.env_data.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "particles", "env_data", withindex) == 1) {
    return(1);
  }
  sprintf(buffer, "%s.particles.scalar_data.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "particles", "scalar_data", withindex) == 1) {
    return(1);
  }
  sprintf(buffer, "%s.radiation.fund_r-z-t_data.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "radiation", "fund_r-z-t_data", withindex) == 1) {
    return(1);
  }
  sprintf(buffer, "%s.radiation.scalar_data.sdds", output);
  if (extractHDFtoSDDS(input, buffer, "radiation", "scalar_data", withindex) == 1) {
    return(1);
  }
  return(0);
}

int extractHDFtoSDDS(char *inputfile, char *outputfile, char *groupname, char *datasetname, int withindex) {
  hid_t file_id; 
  herr_t status;
  hid_t group;
  hid_t dataset;
  hid_t datatype;
  hid_t dataspace;
  H5T_class_t t_class;
  size_t size;
  void *dataout;
  int i, page;
  int64_t n=1, row;
  int sddstype;
  SDDS_TABLE SDDS_table;
  int rank;
  hsize_t *dims_out=NULL;
  int status_n;

  status = H5Fis_hdf5(inputfile);
  if (status <= 0) {
    fprintf(stderr, "unable to open %s, it is not an HDF5 file\n", inputfile);
    return(1);
  }

  file_id = H5Fopen (inputfile, H5F_ACC_RDONLY, H5P_DEFAULT); 
  if (file_id < 0) {
    fprintf(stderr, "unable to open %s\n", inputfile);
    return(1);
  }

  group = H5Gopen(file_id, groupname);
  if (group < 0) {
    fprintf(stderr, "unable to open group %s\n", groupname);
    return(1);
  }
  dataset = H5Dopen(group, datasetname);
  if (dataset < 0) {
    fprintf(stderr, "unable to open dataset %s\n", datasetname);
    return(1);
  }

  datatype = H5Dget_type(dataset);
  t_class = H5Tget_class(datatype);
  size = H5Tget_size(datatype);

  if ((t_class != H5T_INTEGER) && (t_class != H5T_FLOAT)) {
    fprintf(stderr, "Data set is an unsupported type and cannot be converted to SDDS\n");
    return(1);
  }
  dataspace = H5Dget_space(dataset);
  n = H5Sget_simple_extent_npoints(dataspace);

  rank = H5Sget_simple_extent_ndims(dataspace);
  if (rank > 1) {
    dims_out = malloc(sizeof(hsize_t)*rank);
    status_n = H5Sget_simple_extent_dims(dataspace, dims_out, NULL);
  }


  if ((t_class == H5T_FLOAT) && (size == 8)) {
    dataout = (double*)malloc(sizeof(double)*n);
    status = H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, (double*)dataout);
    sddstype = SDDS_DOUBLE;
  } else if ((t_class == H5T_FLOAT) && (size == 4)) {
    dataout = (float*)malloc(sizeof(float)*n);
    status = H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, (float*)dataout);
    sddstype = SDDS_FLOAT;
  } else if ((t_class == H5T_INTEGER) && (size == 8)) {
    dataout = (long*)malloc(sizeof(long)*n);
    status = H5Dread(dataset, H5T_NATIVE_LONG, H5S_ALL, H5S_ALL, H5P_DEFAULT, (long*)dataout);
    sddstype = SDDS_LONG;
  } else if ((t_class == H5T_INTEGER) && (size == 4)) {
    dataout = (int*)malloc(sizeof(int)*n);
    status = H5Dread(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, (int*)dataout);
    sddstype = SDDS_LONG;
  } else {
    fprintf(stderr, "Data set is an unsupported type and cannot be converted to SDDS\n");
    return(1);
  }
  if (status < 0) {
    fprintf(stderr, "unable to read dataset %s\n", datasetname);
    return(1);
  }

  if (SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, 1, NULL, NULL, outputfile) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  if (SDDS_DefineParameter(&SDDS_table, "HDF5Group", NULL, NULL, NULL, NULL, SDDS_STRING, groupname) == -1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  if (SDDS_DefineParameter(&SDDS_table, "HDF5Dataset", NULL, NULL, NULL, NULL, SDDS_STRING, datasetname) == -1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  if (withindex) {
    if (SDDS_DefineSimpleColumn(&SDDS_table, "Index", NULL, SDDS_LONG) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return(1);
    }
  }
  if (SDDS_DefineSimpleColumn(&SDDS_table, "Values", NULL, sddstype) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  if (SDDS_WriteLayout(&SDDS_table) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }

  if (rank == 1) {
    if (SDDS_StartTable(&SDDS_table, n) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return(1);
    }
    if (withindex) {
      for (row=0; row<n; row++) {
        if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row, "Index", row, NULL)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return(1);
        }
      }
    }
    if (SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, dataout, n, "Values") == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return(1);
    }
    if (SDDS_WriteTable(&SDDS_table) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return(1);
    }
  } else {
    i = 0;
    for (page=1; page<=dims_out[2]; page++) {
      if (SDDS_StartTable(&SDDS_table, dims_out[1]) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
      for (row = 0; row < dims_out[1]; row++) {
        if (withindex) {
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row, "Index", row, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
        }
        switch (sddstype) {
        case SDDS_DOUBLE:
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row, "Values", ((double*)dataout)[i], NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
          break;
        case SDDS_FLOAT:
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row, "Values", ((float*)dataout)[i], NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
          break;
        case SDDS_LONG:
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row, "Values", ((long*)dataout)[i], NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
          break;
        case SDDS_SHORT:
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row, "Values", ((int*)dataout)[i], NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return(1);
          }
          break;
        }
        i += dims_out[2];
        if (i >= n) {
          i = page;
        }
      }
      if (SDDS_WriteTable(&SDDS_table) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return(1);
      }
    }
  }
  if (SDDS_Terminate(&SDDS_table) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }

  status = H5Dclose(dataset);
  if (status < 0) {
    fprintf(stderr, "unable to close dataset %s\n", datasetname);
    return(1);
  }
  status = H5Gclose(group);
  if (status < 0) {
    fprintf(stderr, "unable to close group %s\n", groupname);
    return(1);
  }
  status = H5Fclose (file_id);
  if (status) {
    fprintf(stderr, "unable to close %s\n", inputfile);
    return(1);
  }
  return(0);

}

