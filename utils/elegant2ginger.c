#include <stdio.h>
#include "hdf5.h"
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

int writeStringAttribute(const char* name, const char* value, hid_t dataset_id);


#define SET_WAVELENGTH 0
#define SET_PIPE 1
#define N_OPTIONS 2
char *option[N_OPTIONS] = {
  "wavelength", "pipe"
};
char *USAGE = "elegant2ginger [<input-file>] [-pipe=in] <output-file> -wavelength=<lambda>\n\nOnly data from the last page of the SDDS file will be used.\nProgram by Robert Soliday. ("__DATE__")\n";

int main( int argc, char **argv ) {
  SCANNED_ARG *s_arg;
  long i_arg;
  unsigned long pipeFlags=0;
  char *input = NULL;
  char *output = NULL;
  SDDS_TABLE SDDS_table;
  hid_t file_id;
  hid_t dataspace_id;
  hid_t dataset_id;
  hid_t group_id;
  hsize_t dims[3];
  hsize_t fims[3];
  herr_t status;
  int32_t i, page;
  int64_t rows;
  double* dset_data;
  float* fset_data;
  double *x, *y, *xp, *yp, *t, *p;
  double lambda=.001;
  short wavelengthseen=0;

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
      case SET_WAVELENGTH:
	if (s_arg[i_arg].n_items!=2) {
	  fprintf(stderr, "error: invalid -wavelength syntax\n");
	  return(1);
	}
	if (sscanf(s_arg[i_arg].list[1], "%lf", &lambda) != 1 || lambda <= 0) {
	  fprintf(stderr, "error: invalid -wavelength syntax or value\n");
	  return(1);
	}
        wavelengthseen = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list+1, s_arg[i_arg].n_items-1, &pipeFlags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          return(1);
        }
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

  if (!wavelengthseen) {
    fprintf(stderr, "%s", USAGE);
    return(1);
  }
  if (pipeFlags&USE_STDIN) {
    processFilenames("elegant2ginger", &input, &output, USE_STDIN, 1, NULL);
  }

  if (SDDS_InitializeInput(&SDDS_table, input) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  if ((SDDS_CheckColumn(&SDDS_table, "x", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "column x is not in the data file"); 
    return(1); 
  } 
  if ((SDDS_CheckColumn(&SDDS_table, "xp", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "column xp is not in the data file"); 
    return(1); 
  } 
  if ((SDDS_CheckColumn(&SDDS_table, "y", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "column y is not in the data file"); 
    return(1); 
  } 
  if ((SDDS_CheckColumn(&SDDS_table, "yp", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "column yp is not in the data file"); 
    return(1); 
  }
  page = SDDS_ReadTable(&SDDS_table);
  if (page <= 0) {
    fprintf(stderr, "No data in SDDS file."); 
    return(1);
  }
  while (page > 0) {
    x = SDDS_GetColumnInDoubles(&SDDS_table, "x");
    xp = SDDS_GetColumnInDoubles(&SDDS_table, "xp");
    y = SDDS_GetColumnInDoubles(&SDDS_table, "y");
    yp = SDDS_GetColumnInDoubles(&SDDS_table, "yp");
    t = SDDS_GetColumnInDoubles(&SDDS_table, "t");
    p = SDDS_GetColumnInDoubles(&SDDS_table, "p");
    rows = SDDS_RowCount(&SDDS_table);
    
    page = SDDS_ReadTable(&SDDS_table);
  }

  fims[0] = 1; 
  fims[1] = 4; 
  fims[2] = rows;
  fset_data = malloc(sizeof(float)*fims[0]*fims[1]*fims[2]);
    
  for (i = 0; i < rows; i++) {
    fset_data[i] = x[i];
    fset_data[i+rows] = xp[i];
    fset_data[i+rows*2] = y[i];
    fset_data[i+rows*3] = yp[i];
  }

  dims[0] = 1; 
  dims[1] = 2; 
  dims[2] = rows;
  dset_data = malloc(sizeof(double)*dims[0]*dims[1]*dims[2]);
  for (i = 0; i < rows; i++) {
    dset_data[i] = 2*PI*2.99792458e8*t[i]/lambda;
    dset_data[i+rows] = sqrt(p[i]*p[i]+1);
  }

  if (SDDS_Terminate(&SDDS_table) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }



  file_id = H5Fcreate(output, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  group_id = H5Gcreate(file_id, "/particles", 0);

  dataspace_id = H5Screate_simple(3, dims, NULL);
  dataset_id = H5Dcreate(group_id, "gam-theta-data", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
  status = H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_data);
  writeStringAttribute("column_names", "gamma.theta", dataset_id);
  writeStringAttribute("label", "macroparticle gamma-theta data", dataset_id);
  status = H5Dclose(dataset_id);
  status = H5Sclose(dataspace_id);

  dataspace_id = H5Screate_simple(3, fims, NULL);
  dataset_id = H5Dcreate(group_id, "xydata", H5T_NATIVE_FLOAT, dataspace_id, H5P_DEFAULT);
  status = H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, fset_data);
  writeStringAttribute("column_names", "x(m),x'(rad),y(m),y'(rad)", dataset_id);
  writeStringAttribute("label", "4D macroparticle x-x'-y-y' data", dataset_id);
  status = H5Dclose(dataset_id);
  status = H5Sclose(dataspace_id);

  status = H5Gclose(group_id);
  status = H5Fclose(file_id);

  free(fset_data);
  free(dset_data);
  free_scanargs(&s_arg,argc);

  return(0);
}

int writeStringAttribute(const char* name, const char* value, hid_t dataset_id) {
  hid_t attribute_id;
  hid_t atype;
  hid_t a_id;
  herr_t status;

  a_id  = H5Screate(H5S_SCALAR);
  atype = H5Tcopy(H5T_C_S1);
  H5Tset_size(atype, strlen(value) + 1);
  H5Tset_strpad(atype,H5T_STR_NULLTERM);
  attribute_id = H5Acreate(dataset_id, name, atype, a_id, H5P_DEFAULT);
  status = H5Awrite(attribute_id, atype, value);
  status = H5Aclose(attribute_id);
  status = H5Tclose(atype);
  status = H5Sclose(a_id);

  return(0);
}




