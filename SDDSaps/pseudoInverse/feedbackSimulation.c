/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file i66s distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program feedbackSimulator.c
 * Reads in a orbit correction matrix C for correcting orbit
 * and a response matrix R to calculate the real response from correctors.
 * The usual difference equations are used.
 * BPMs and corrector can be given response filters.
 * code sddscontrollaw.c is used as initial template.
 * Only one plane is used.
 * Source of noise are corrector locations, since we have the
 * response matrix ready-made.
 */


#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "matrixop.h"
#include <stdio.h>

#define CLO_ACOEF 0
#define CLO_BCOEF 1
#define CLO_RESPONSE_MATRIX 2
#define CLO_FEEDBACK_CORRECTION_MATRIX 3
#define CLO_RECONSTRUCT_MATRIX 4
#define CLO_DELTA_T 5
#define CLO_ROOTNAME 6
#define COMMANDLINE_OPTIONS 7


char *commandline_option[COMMANDLINE_OPTIONS] = {
  "acoefficients",
  "bcoefficients",
  "responseMatrix",
  "feedbackCorrectionMatrix",
  "reconstructMatrix",
  "deltaT", "rootname"
  };


char *USAGE1 = "feedbackSimulation <inputDataDirectory> <outputDataDirectory>\n\
     -acoefficients=<list of a coefficients> -bcoefficients=<list of b coefficients> \n\
     -responseMatrix=<response matrix file which contains all bpms and correctors>\n \
     -feedbackCorrectionMatrix=<inverse matrix file for feedback correction> \n\
     -reconstructMatrix=<reponse matrix file which contrains all bpms and feedback correction correctors> \n\
     [-deltaT=<value>] \n\
inputDataDirectory           data directory which has initial bpm data, one file for one bpm.\n\
outputDataDirectory          output data directory for writing bpm and corrector data after feedback correction.\n\
acoefficients                acoefficients of the corrector regulator.\n\
bcoefficients                bcoefficients of the corrector regulator.\n\
responseMatrix               response matrix file that contains all bpms and all correctors.\n\
feedbackCorrectionMatrix     feedback correction matrix which contains the feedback correctors and feedback bpms (not all the bpms).\n\
reconstructMatrix            responseMatrix file in outputDataDirectory which contains the feedback correctors and all the bpms.\n\
deltaT                       time difference between two steps.\n\
outputBPMS                   only write the output files for given bpms.\n\
Program by Hairong Shang, Shifu Xu ANL\n";

char **get_names(char *filename, char *columnanme, int32_t *names);

MAT *read_matrix(char *filename, long m, long n);
double **read_bpm_data(char *inputDir, long bpm_names, char **bpm_name, int32_t *rows);
double apply_regulator(long turn, double *acoef, long acoefs, double *bcoef, long bcoefs, double *corr_driv, double *corr_error);
void write_output_data(char *outputDir, char *rootname, double deltaT, int32_t data_rows, double **bpm_error, int32_t bpm_names, char **bpm_name,
                       int32_t corr_names, char **corr_name, int32_t corr_rows, double **corr_error, double **corr_drive, long bcoefs);

int main(int argc, char **argv)
{
  SCANNED_ARG *s_arg=NULL;
  MAT *Inv, *Recons, *InvT, *ReconsT;
  double *acoef, *bcoef;
  int32_t acoefs, bcoefs, i_arg, i, j, k, bpm_names, feedback_bpm_names, feedback_corr_names, corr_names1, bpm_names1, data_rows, corr_rows;
  char *inputDir, *outputDir, *invFile, *respFile, *reconsFile, **bpm_name, **feedback_bpm_name, **feedback_corr_name, **corr_name1, **bpm_name1, *rootname=NULL;
  char filename[1024];
  long *feedback_bpm_index=NULL;
  double **init_bpm_data=NULL;
  double **feedback_corr_error=NULL, **feedback_corr_drive=NULL, **bpm_error;
  double *feedback_bpm_error=NULL, *orbit_error=NULL;
  double *corr_error=NULL, deltaT;
  double *corr_in, *corr_out;
  long nin, nout;

#if defined(CLAPACK)
  long lda,  incx=1, incy=1; /*incx, incy is the space in bytes between two element of input/output vector */
  double alpha=1.0, beta=1.0; 
#endif
#if defined(LAPACK)
  integer lda,  incx=1, incy=1;
  doublereal alpha=1.0, beta=1.0;  
#endif
  
  acoefs = bcoefs = 0;
  inputDir = outputDir = invFile = respFile = reconsFile = NULL;
  acoef = bcoef = NULL;
  Inv = Recons = NULL;
  
  bpm_name = feedback_bpm_name = feedback_corr_name = corr_name1 = bpm_name1 = NULL;
  bpm_names = feedback_bpm_names = feedback_corr_names = corr_names1 = bpm_names1 = 0;
  argc = scanargs(&s_arg, argc, argv);
  if (argc==1) {
    fprintf(stderr, "%s\n", USAGE1);
    free_scanargs(&s_arg, argc);
    return(1);
  }
  
  for (i_arg = 1;  i_arg<argc;  i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch(match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, UNIQUE_MATCH)) {
      case CLO_ACOEF:
        if (s_arg[i_arg].n_items<2)
          SDDS_Bomb("Invalid -aceofficients syntx/value, no values provided.");
        acoefs = s_arg[i_arg].n_items-1;
        acoef = malloc(sizeof(*acoef)*acoefs);
        for (i=0; i<acoefs; i++) {
          if (!get_double(&acoef[i], s_arg[i_arg].list[i+1]))
            SDDS_Bomb("Invalid -aceofficients  value provided");
        }
        break;
      case CLO_BCOEF:
        if (s_arg[i_arg].n_items<2)
          SDDS_Bomb("Invalid -bceofficients syntx/value, no values provided.");
        bcoefs = s_arg[i_arg].n_items-1;
        bcoef = malloc(sizeof(*bcoef)*bcoefs);
        for (i=0; i<bcoefs; i++)
          if (!get_double(&bcoef[i], s_arg[i_arg].list[i+1]))
            SDDS_Bomb("Invalid -bceofficients  value provided");
        break;
      case CLO_RESPONSE_MATRIX:
        if (s_arg[i_arg].n_items!=2)
          SDDS_Bomb("Invalid -responseMatrix syntx/value");
        respFile =  s_arg[i_arg].list[1];
        break;
      case CLO_FEEDBACK_CORRECTION_MATRIX:
        if (s_arg[i_arg].n_items!=2)
          SDDS_Bomb("Invalid -feedbackCorrectionMatrix syntx/value");
        invFile =  s_arg[i_arg].list[1];
        break;
      case CLO_RECONSTRUCT_MATRIX:
        if (s_arg[i_arg].n_items!=2)
          SDDS_Bomb("Invalid -reconstructMatrix syntx/value");
        reconsFile =  s_arg[i_arg].list[1];
        break;
      case CLO_DELTA_T:
        if (s_arg[i_arg].n_items!=2)
          SDDS_Bomb("Invalid -reconstructMatrix syntx/value");
        if (!get_double(&deltaT, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -deltaT syntax/value");
        break;
      case CLO_ROOTNAME:
        if (s_arg[i_arg].n_items!=2)
          SDDS_Bomb("Invalid -rootname syntx/value");
        rootname = s_arg[i_arg].list[1];
        break;
      default:
	fprintf(stderr, "invalid option provided -- %s\n", s_arg[i_arg].list[0]);
	exit(1);
      }
    } else {
      if (!inputDir)
        inputDir = s_arg[i_arg].list[0];
      else if (!outputDir)
        outputDir = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Two many files provided, the conflict arguments is %s\n", s_arg[i_arg].list[0]);
        exit(1);
      } 
    }
  }
  if (!inputDir)
    SDDS_Bomb("Error, input data directory not provided.");
  if (!outputDir)
    SDDS_Bomb("Error, output data directory not provided.");
  

  /*obtain all bpm names from response matrix file */
  sprintf(filename,"%s/rm1",inputDir);
  bpm_name = get_names(filename, "BPMName", &bpm_names);
  
  /*obtain feedback bpm and corrector names */
  sprintf(filename,"%s/rm2", outputDir);
  feedback_bpm_name = get_names(filename, "BPMName", &feedback_bpm_names);
  sprintf(filename,"%s/irm2", outputDir);
  feedback_corr_name = get_names(filename, "ControlName", &feedback_corr_names);
  /*obtain feedback corrector name and all bpm names from reconstruct matrix */
  sprintf(filename,"%s/rm3", outputDir);
  bpm_name1 = get_names(filename, "BPMName", &bpm_names1);
  sprintf(filename,"%s/irm3", outputDir);
  corr_name1 = get_names(filename, "ControlName", &corr_names1);
  
  
  /*check if bpm_names1 is the same as bpm_names, corr_names1 is the same as feedback_corr_names*/
  if (bpm_names!=bpm_names1) {
    fprintf(stderr, "Error: the number of bpms (rm1 %d) in response matrix is not the same as the bpms in recontruct matrix(rm3 %d).\n", bpm_names, bpm_names1);
    exit(1);
  }
  for (i=0; i<bpm_names; i++) {
    if (strcmp(bpm_name[i], bpm_name1[i])!=0) {
      fprintf(stderr, "Error, the %dth bpm are different in response matrix and reconstruct matrix.\n", i);
      exit(1);
    }
    free(bpm_name1[i]);
  }
  if (feedback_corr_names!=corr_names1)
     SDDS_Bomb("Error: the number of correctors in feedback correction matrix is not the same as the correctors in recontruct matrix.");
  for (i=0; i<feedback_corr_names; i++) {
    if (strcmp(feedback_corr_name[i], corr_name1[i])!=0) {
      fprintf(stderr, "Error, the %dth corrector are different in feedback correction  matrix and reconstruct matrix.\n", i);
      exit(1);
    }
    free(corr_name1[i]);
  }
  free(bpm_name1);
  free(corr_name1);
  /*get the index of feedback correction bpm in the response matrix */
  if (!(feedback_bpm_index = tmalloc(sizeof(*feedback_bpm_index)*feedback_bpm_names)))
    SDDS_Bomb("Error in allocate memory for feedback_bpm_index.");
  for (i=0; i<feedback_bpm_names; i++) {
    feedback_bpm_index[i] = match_string(feedback_bpm_name[i], bpm_name, bpm_names, EXACT_MATCH);
    if (feedback_bpm_index[i]<0) {
      fprintf(stderr, "Error: feedback bpm %s not found in the response matrix.\n", feedback_bpm_name[i]);
      exit(1);
    }
  }
  /*read feedback correction matrix, correctors * bpms */
  sprintf(filename,"%s/irm2.dat", outputDir);
  if (!(Inv = read_matrix(filename, feedback_corr_names, feedback_bpm_names)))
    SDDS_Bomb("Error in reading feedback correction matrix.");
      
  /*read recontruct matrix */
  sprintf(filename,"%s/rm3.dat", outputDir);
  if (!(Recons = read_matrix(filename, bpm_names, feedback_corr_names)))     
    SDDS_Bomb("Error in reading reconstruct matrix.");

  /*read bpm initital data */
  init_bpm_data = read_bpm_data(inputDir, bpm_names, bpm_name, &data_rows);
  
  if (!(feedback_corr_error = tmalloc(sizeof(*feedback_corr_error)*feedback_corr_names)))
    SDDS_Bomb("Error in allocate memory for feedback_corr_error.");
  if (!(feedback_corr_drive = tmalloc(sizeof(*feedback_corr_drive)*feedback_corr_names)))
    SDDS_Bomb("Error in allocate memory for feedback_corr_drive.");
  if (!(bpm_error = tmalloc(sizeof(*bpm_error)*bpm_names)))
    SDDS_Bomb("Error in allocate memory for feedback_error.");

  for (i=0; i<bpm_names; i++) {
    bpm_error[i] =NULL;
    if (!(bpm_error[i] = tmalloc(sizeof(**bpm_error)*data_rows)))
      SDDS_Bomb("Error in allocate memory for feedback_error.");
  }
  if (acoefs>bcoefs)
    corr_rows = data_rows + acoefs;
  else
    corr_rows = data_rows + bcoefs;
  
  for (i=0; i<feedback_corr_names; i++) {
    feedback_corr_error[i] = feedback_corr_drive[i] = NULL;
    if (!(feedback_corr_error[i] = calloc(sizeof(**feedback_corr_error), corr_rows)))
      SDDS_Bomb("Error in allocate memory for feedback_corr_error.");
    if (!(feedback_corr_drive[i] = calloc(sizeof(**feedback_corr_drive), corr_rows)))
      SDDS_Bomb("Error in allocate memory for feedback_corr_drive.");
  }
  if (!(feedback_bpm_error = calloc(sizeof(*feedback_bpm_error), feedback_bpm_names)))
    SDDS_Bomb("Error in allocate memory for feedback_bpm_error.");
  
  if (!(orbit_error = calloc(sizeof(*orbit_error), bpm_names)))
    SDDS_Bomb("Error in allocate memory for orbit_error.");
  /*start feedback simulation loop data_rows is number of turns */
 
  if (!(corr_error = calloc(sizeof(*corr_error), feedback_corr_names)))
    SDDS_Bomb("Error in allocate memory for corr_error.");
 
  /*transpose Inv and Recons to make the matrix vector multiplication in continuouse memory to speed up process */
  InvT = matrix_transpose(Inv);
  ReconsT = matrix_transpose(Recons);
  for (i=0; i<data_rows; i++) {
    if (i==0) {
      for (j=0; j<bpm_names; j++)
        bpm_error[j][i] = init_bpm_data[j][i];
    }
    /*get feedback bpm errors */
    for (j=0; j<feedback_bpm_names; j++) {
      feedback_bpm_error[j] = bpm_error[feedback_bpm_index[j]][i];
    }
    /* for (j=0; j<feedback_corr_names; j++) corr_error[j]=0; */ /*need to set the output to zero before calling dgemv */
    /*apply INV to get corrector error */
    lda = Inv->m;
    /*  dgemv_("N", &Inv->m, &Inv->n, &alpha, Inv->base, &lda, feedback_bpm_error, &incx, &beta, corr_error, &incy); */
    /*apply the -1 sign to get the corr error from bpm error since corr_error = -Inv * bpm_error */
    /* for (j=0; j<feedback_corr_names; j++)
       feedback_corr_error[j][i+bcoefs] = -1.0 * corr_error[j]; */
    for (j=0; j<feedback_corr_names; j++) {
      corr_error[j]=0;
      for (k=0; k<feedback_bpm_names; k++) {
        corr_error[j] += -1* InvT->base[j*InvT->m+k] *feedback_bpm_error[k];
      }
      feedback_corr_error[j][i+bcoefs] = corr_error[j];
    }
    
    
    /*apply regulator to get corrector drive with shifu's code */
    for (j=0; j<feedback_corr_names; j++) {
      feedback_corr_drive[j][i+acoefs] = apply_regulator(i, acoef, acoefs, bcoef, bcoefs, feedback_corr_drive[j], feedback_corr_error[j]);
      /* feedback_corr_drive[j][i+acoefs] = feedback_corr_error[j][i+bcoefs]; */
    }
    
    for (j=0; j<feedback_corr_names; j++) {
      corr_error[j] = feedback_corr_drive[j][i+acoefs];
    }
    /*apply response matrix to get orbit error */
    /*for (j=0; j<bpm_names; j++) orbit_error[j]=0; */ /*need to set the output to zero before calling dgemv */
    lda = Recons->m;
    /* dgemv_("N", &Recons->m, &Recons->n, &alpha, Recons->base, &lda, corr_error, &incx, &beta, orbit_error, &incy);*/
    for (j=0; j<bpm_names; j++) {
      orbit_error[j]=0;
      for (k=0; k<feedback_corr_names; k++) {
        orbit_error[j] += ReconsT->base[j*ReconsT->m+k] * corr_error[k];
      }
    }
    
    /*add orbit error to bpm error next turn */
    if (i<data_rows-1)
      for (j=0; j<bpm_names; j++) 
        bpm_error[j][i+1] = init_bpm_data[j][i+1] + orbit_error[j];
  }
  
  matrix_free(Inv);
  matrix_free(Recons);
  matrix_free(InvT);
  matrix_free(ReconsT);
 
  write_output_data(outputDir, rootname, deltaT, data_rows, bpm_error, bpm_names , bpm_name,
    feedback_corr_names, feedback_corr_name, corr_rows, feedback_corr_error, feedback_corr_drive, bcoefs); 
  
 
  for (i=0; i<bpm_names; i++) {
    free(init_bpm_data[i]);
    free(bpm_error[i]);
    free(bpm_name[i]);
  }
  free(bpm_name);
  free(init_bpm_data);
  free(bpm_error);
  for (i=0; i<feedback_corr_names; i++) {
    free(feedback_corr_error[i]);
    free(feedback_corr_drive[i]);
    free(feedback_bpm_name[i]);
    free(feedback_corr_name[i]);
  }
  free(feedback_corr_error);
  free(feedback_corr_drive);
  free(feedback_bpm_name);
  free(feedback_corr_name);
  
  free(orbit_error);
  free(corr_error);
  free(feedback_bpm_error);
  free(feedback_bpm_index);
  if (acoef) free(acoef);
  if (bcoef) free(bcoef);
  free_scanargs(&s_arg, argc);
  return 0;
}


MAT *read_matrix(char *filename, long m, long n)
{
  MAT *matrix=NULL;
  FILE *fp=NULL;
  long i, j;
  if (!(fp=fopen(filename, "r"))) {
    fprintf(stderr, "Error in opening file %s for reading\n", filename);
    exit(1);
  }
  if (!(matrix = matrix_get(m,n)))
    SDDS_Bomb("Error in allocating memory for matrix.");
  
  /* fread((Real*)matrix->base, sizeof(Real)*m*n, m*n, fp); */
  fseek(fp, 0, 0);
  /*  if (!fBuffer) {
    if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char)*(defaultIOBufferSize+1)))) {
      fprintf(stderr, "Unable to do buffered read--allocation failure");
      exit(1);
    }
    fBuffer->bufferSize = defaultIOBufferSize;
    fBuffer->bytesLeft = defaultIOBufferSize;
  }
  if (!(SDDS_BufferedRead((double*)matrix->base,  m*n*sizeof(double), fp, fBuffer))) {
    fprintf(stderr, "Error in reading matrix file %s\n", filename);
    exit(1);
    } */
  fread((double*)matrix->base, 1, m*n*sizeof(double), fp);
  
  fclose(fp);
 
  return matrix;
}


char **get_names(char *filename, char *columnname, int32_t *names)
{
  SDDS_DATASET matrixData;
  char **name=NULL;
  
  *names = 0;
  if (!SDDS_InitializeInput(&matrixData, filename) || !SDDS_ReadPage(&matrixData))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  
  if (!(*names=SDDS_CountRowsOfInterest(&matrixData))) {
    fprintf(stderr, "No data found in matrix file %s\n", filename);
    exit(1);
  }
  if (!(name = (char**)SDDS_GetColumn(&matrixData, columnname)) || !SDDS_Terminate(&matrixData))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  return name;
}

double **read_bpm_data(char *inputDir, long bpm_names, char **bpm_name, int32_t *rows)
{
  SDDS_DATASET bpmData;
  double **bpm_data;
  char filename[1024];
  FILE *fp=NULL;
  long i;
  
  *rows=0;
  sprintf(filename,"%s/%s_computed.sdds", inputDir, bpm_name[0]);
  if (!SDDS_InitializeInput(&bpmData, filename) || !SDDS_ReadPage(&bpmData))
     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  if (!(*rows=SDDS_CountRowsOfInterest(&bpmData))) {
    fprintf(stderr, "bpm data%s is empty.\n", filename);
    exit(1);
  }
  if (!!SDDS_Terminate(&bpmData))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  
  bpm_data = tmalloc(sizeof(*bpm_data)*bpm_names);
  
  for (i=0; i<bpm_names; i++) {
    bpm_data[i] = tmalloc(sizeof(**bpm_data)*(*rows));
    sprintf(filename, "%s/%s.dat", inputDir, bpm_name[i]);
    if (!(fp = fopen(filename, "r"))) {
      fprintf(stderr, "Error in opening file %s for reading\n", filename);
      exit(1);
    }
    fseek(fp, 0, 0);
    fread((double*)bpm_data[i], 1, (*rows)*sizeof(double), fp);
    fclose(fp);
    fp = NULL;
  }
  return bpm_data;
}


/*for each corrector -- apply regulator */
double apply_regulator(long k, double *acoef, long acoefs, double *bcoef, long bcoefs, double *corr_driv, double *corr_error)
{
  long m, n;
  double suma=0,sumb=0;
  
 
  for (n=0; n<bcoefs; n++) {
    sumb += bcoef[bcoefs-n-1] * corr_error[k+1+n];
  }
  for ( m = 1; m < acoefs; m++ ) {
    suma += acoef[acoefs-m] * corr_driv[k+m];
  }
  return (sumb - suma)/acoef[0];
 
}

void write_output_data(char *outputDir, char *rootname, double deltaT, int32_t data_rows, double **bpm_error, int32_t bpm_names, char **bpm_name,
                       int32_t corr_names, char **corr_name, int32_t corr_rows, double **corr_error, double **corr_drive, long bcoefs) {
  char filename[1024];
  long i, time_rows;
  double *time=NULL;
  SDDS_DATASET bpmData, corrData;
  time_rows = MAX(data_rows, corr_rows);
  if (!(time=tmalloc(sizeof(*time)*time_rows)))
    SDDS_Bomb("Error in allocating memory for time");
  for (i=0; i<time_rows; i++)
    time[i] = i*deltaT;
  
  for (i=0;i<bpm_names; i++) {
    if (!rootname)
      sprintf(filename, "%s/%s.sdds", outputDir, bpm_name[i]);
    else
      sprintf(filename, "%s/%s%s.sdds", outputDir, rootname, bpm_name[i]);
    /* fprintf(stderr, "writing data for %s\n", bpm_name[i]); */
    if (!SDDS_InitializeOutput(&bpmData, SDDS_BINARY, 1, NULL, NULL, filename) ||
        !SDDS_DefineSimpleColumn(&bpmData, "Time", "seconds", SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&bpmData, "Output", "mm", SDDS_DOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    bpmData.layout.data_mode.column_major =1;
    if (!SDDS_WriteLayout(&bpmData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (!SDDS_StartPage(&bpmData, data_rows) || !SDDS_SetColumn(&bpmData, SDDS_SET_BY_NAME, time, data_rows, "Time") ||
        !SDDS_SetColumn(&bpmData, SDDS_SET_BY_NAME, bpm_error[i], data_rows, "Output") ||
        !SDDS_WritePage(&bpmData) || !SDDS_Terminate(&bpmData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  }
  for (i=0; i<corr_names; i++) {
    if (!rootname)
      sprintf(filename, "%s/%s.sdds", outputDir, corr_name[i]);
    else
      sprintf(filename, "%s/%s%s.sdds", outputDir, rootname, corr_name[i]);
    /*   fprintf(stderr, "writing data for %s\n", corr_name[i]); */
    if (!SDDS_InitializeOutput(&corrData, SDDS_BINARY, 1, NULL, NULL, filename) ||
        !SDDS_DefineSimpleColumn(&corrData, "Time", "seconds", SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&corrData, "CorrectorError", "A", SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&corrData, "CorrectorDrive", "A", SDDS_DOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    corrData.layout.data_mode.column_major =1;
    if (!SDDS_WriteLayout(&corrData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (!SDDS_StartPage(&corrData, data_rows) || !SDDS_SetColumn(&corrData, SDDS_SET_BY_NAME, time, data_rows, "Time") ||
        !SDDS_SetColumn(&corrData, SDDS_SET_BY_NAME, corr_error[i], data_rows, "CorrectorError") ||
        !SDDS_SetColumn(&corrData, SDDS_SET_BY_NAME, corr_drive[i], data_rows, "CorrectorDrive") ||
        !SDDS_WritePage(&corrData) || !SDDS_Terminate(&corrData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  }
  free(time);
  return;
}

