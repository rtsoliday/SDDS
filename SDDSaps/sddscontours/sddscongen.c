/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: congen
 * purpose: generate contour plot output file according to a user-supplied
 *          equation
 *
 * Michael Borland, 1995
 $Log: not supported by cvs2svn $
 Revision 1.10  2003/09/02 19:32:36  soliday
 Cleaned up code for Linux.

 Revision 1.9  2003/06/02 21:37:43  soliday
 Added a function to add parentheses around the string before passing it to if2pf.

 Revision 1.8  2002/08/14 17:35:28  soliday
 Added Open License

 Revision 1.7  2001/04/10 20:42:57  soliday
 Added the ability to convert algebraic equations to rpn equations.

 Revision 1.6  2001/01/10 19:34:34  soliday
 Standardized usage message.

 Revision 1.5  2000/10/04 21:02:51  soliday
 Added definition for outfile that was causing problems with Linux when using
 shared libraries.

 Revision 1.4  2000/04/10 19:01:06  borland
 -shapes option now works again, and uses SDDS files.

 Revision 1.3  1999/06/03 20:42:49  soliday
 Removed compiler warnings under Linux.

 Revision 1.2  1995/09/05 21:08:53  saunders
 First test release of the SDDS1.5 package.

 */
#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "rpn.h"

#define SET_X_RANGE 0
#define SET_Y_RANGE 1
#define SET_Z_EQUATION 2
#define SET_RPN_DEFNS_FILE 3
#define SET_RPN_INIT_COMMAND 4
#define SET_PIPE 5
#define N_OPTIONS 6

FILE* outfile;
char *option[N_OPTIONS] = {
    "xrange", "yrange", "zequation", "rpndefinitions",
    "rpncommand", "pipe",
    } ;

char *USAGE = "sddscongen <outputfile> -pipe[=output] \n\
 -xrange=<lower>,<upper>,<n-points> \n\
 -yrange=<lower>,<upper>,<n-points> \n\
 -zequation=<rpn-expression>[,algebraic] \n\
 -rpncommand=<rpn-expression>[,algebraic] \n\
 -rpndefinitions=<rpn-defns-file> \n\n\
Program by Michael Borland. (This is version 2, May 1995.)";

char *addOuterParentheses(char *arg);

int main(int argc, char **argv)
{
    double x_lo, x_hi, y_lo, y_hi, dx, dy, x, y;
    double **z_data, z_min, z_max, z_val;
    long ix, iy, nx, ny;
    long x_rpn_var, y_rpn_var;
    char *z_equation = NULL;
    char *rpn_defns_file;
    char *rpn_init_command;
    char *z_udf = "Z.UDF";
    char *input, *output;
    long i_arg;
    SCANNED_ARG *s_arg;
    FILE *fp;
    unsigned long pipeFlags;
    char pfix[IFPF_BUF_SIZE], *ptr;

    if (argc<2 || argc>(2+N_OPTIONS)) 
        bomb(NULL, USAGE);

    output = NULL;
    nx = ny = 0;
    rpn_defns_file = NULL;
    rpn_init_command = NULL;
    pipeFlags = 0;

    scanargs(&s_arg, argc, argv);
    for (i_arg=1; i_arg<argc; i_arg++) {
        if (s_arg[i_arg].arg_type==OPTION) {
            delete_chars(s_arg[i_arg].list[0], "_");
            switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
                case SET_RPN_DEFNS_FILE:
                    if (s_arg[i_arg].n_items!=2 ||
                            !(rpn_defns_file=s_arg[i_arg].list[1]))
                        bomb("incorrect -rpndefnsfile syntax", NULL);
                    break;
                case SET_RPN_INIT_COMMAND:
		  /*
                    if (s_arg[i_arg].n_items!=2 ||
                            !(rpn_init_command = s_arg[i_arg].list[1]))
                        bomb("incorrect -rpncommand syntax", NULL);
		  */
		  if ((s_arg[i_arg].n_items<2) || (s_arg[i_arg].n_items>3))
		    SDDS_Bomb("incorrect -rpncommand syntax");
		  if (s_arg[i_arg].n_items==2) {
		    if (!strlen(rpn_init_command=s_arg[i_arg].list[1])) {
		      SDDS_Bomb("incorrect -rpncommand syntax");
		    }
		  } else if (s_arg[i_arg].n_items==3) {
		    if (strncmp(s_arg[i_arg].list[2], "algebraic", strlen(s_arg[i_arg].list[2]))==0) {
		      ptr = addOuterParentheses(s_arg[i_arg].list[1]);
		      if2pf(pfix, ptr, sizeof pfix);
		      free(ptr);
		      if (!SDDS_CopyString(&rpn_init_command, pfix)) {
			fprintf(stderr, "error: problem copying argument string\n");
			return(1);
		      }
		    } else {
		      SDDS_Bomb("incorrect -rpncommand syntax");
		    }
		  }
		  break;
                case SET_Z_EQUATION:
		  /*
                    if (s_arg[i_arg].n_items!=2 ||
			!(z_equation=s_arg[i_arg].list[1]))
                        bomb("incorrect -zequation syntax", NULL);
		  */
		  if ((s_arg[i_arg].n_items<2) || (s_arg[i_arg].n_items>3))
		    SDDS_Bomb("incorrect -zequation syntax");
		  if (s_arg[i_arg].n_items==2) {
		    if (!strlen(z_equation=s_arg[i_arg].list[1])) {
		      SDDS_Bomb("incorrect -zequation syntax");
		    }
		  } else if (s_arg[i_arg].n_items==3) {
		    if (strncmp(s_arg[i_arg].list[2], "algebraic", strlen(s_arg[i_arg].list[2]))==0) {
		      if2pf(pfix, s_arg[i_arg].list[1], sizeof pfix);
		      if (!SDDS_CopyString(&z_equation, pfix)) {
			fprintf(stderr, "error: problem copying argument string\n");
			return(1);
		      }
		    } else {
		      SDDS_Bomb("incorrect -zequation syntax");
		    }
		  }
		  break;
                case SET_X_RANGE:
                    if (s_arg[i_arg].n_items!=4 ||
                            1!=sscanf(s_arg[i_arg].list[1], "%le", &x_lo) ||
                            1!=sscanf(s_arg[i_arg].list[2], "%le", &x_hi) ||
                            x_lo>=x_hi || 
                            1!=sscanf(s_arg[i_arg].list[3], "%ld", &nx) ||
                            nx<=1) 
                        bomb("incorrect -xrange syntax", NULL);
                    break;
                case SET_Y_RANGE:
                    if (s_arg[i_arg].n_items!=4 ||
                            1!=sscanf(s_arg[i_arg].list[1], "%le", &y_lo) ||
                            1!=sscanf(s_arg[i_arg].list[2], "%le", &y_hi) ||
                            y_lo>=y_hi || 
                            1!=sscanf(s_arg[i_arg].list[3], "%ld", &ny) ||
                            ny<=1) 
                        bomb("incorrect -yrange syntax", NULL);
                    break;
                  case SET_PIPE:
                    if (!processPipeOption(s_arg[i_arg].list+1, s_arg[i_arg].n_items-1, &pipeFlags))
                        bomb("invalid -pipe syntax", NULL);
                    break;
                  default:
                    fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
                    bomb(NULL, NULL);
                    break;
                }
            }
        else {
            if (output==NULL) 
                output = s_arg[i_arg].list[0];
            else
                bomb("too many filenames (sddscongen)", NULL);
            }
        }

    pipeFlags |= DEFAULT_STDIN;
    input = NULL;
    processFilenames("sddscongen", &input, &output, pipeFlags, 0, NULL);

    if (nx==0)
        bomb("-xrange must be supplied", NULL);    
    if (ny==0)
        bomb("-yrange must be supplied", NULL);    
    
    if (z_equation==NULL)
        bomb("-zequation must be supplied", NULL);

    rpn(rpn_defns_file?rpn_defns_file:getenv("RPN_DEFNS"));
    if (rpn_init_command)
        rpn(rpn_init_command);
    x_rpn_var = rpn_create_mem("x", 0);
    y_rpn_var = rpn_create_mem("y", 0);
    create_udf(z_udf, z_equation);

    z_data = (double**)array_2d(sizeof(double), 0, nx-1, 0, ny-1);
    dx = (x_hi-x_lo)/(nx-1);
    dy = (y_hi-y_lo)/(ny-1);
    z_min =  FLT_MAX;
    z_max = -FLT_MAX;
    
    for (ix=0,x=x_lo; ix<nx; ix++,x+=dx) {
        for (iy=0,y=y_lo; iy<ny; iy++,y+=dy) {
            rpn_store(x, NULL, x_rpn_var);
            rpn_store(y, NULL, y_rpn_var);
            if ((z_val = z_data[ix][iy] = rpn(z_udf))>z_max)
                z_max = z_val;
            if (z_val<z_min)
                z_min = z_val;
            rpn_clear();
            }
        }

    if (output)
        fp = fopen_e(output, "w", 0);
    else
        fp = stdout;
    fprintf(fp, "SDDS1\n&parameter name=Variable1Name, type=string, fixed_value=x &end\n");
    fprintf(fp, "&parameter name=Variable2Name, type=string, fixed_value=y &end\n");
    fprintf(fp, "&parameter name=xInterval, type=double, fixed_value=%e &end\n", dx);
    fprintf(fp, "&parameter name=xMinimum, type=double, fixed_value=%e &end\n", x_lo);
    fprintf(fp, "&parameter name=xDimension, type=long, fixed_value=%ld &end\n", nx);
    fprintf(fp, "&parameter name=yInterval, type=double, fixed_value=%e &end\n", dy);
    fprintf(fp, "&parameter name=yMinimum, type=double, fixed_value=%e &end\n", y_lo);
    fprintf(fp, "&parameter name=yDimension, type=long, fixed_value=%ld &end\n", ny);
    fprintf(fp, "&column name=z, type=double, description=\"%s\" &end\n", z_equation);
    fprintf(fp, "&data mode=ascii, no_row_counts=1 &end\n");
    for (ix=0; ix<nx; ix++)
         for (iy=0; iy<ny; iy++)
             fprintf(fp, "%e\n", z_data[ix][iy]);
    return(0);
}

char *addOuterParentheses(char *arg) 
{
  char *ptr;
  ptr = tmalloc(sizeof(*ptr)*(strlen(arg)+2));
  sprintf(ptr, "(%s)", arg);
  return ptr;
}

