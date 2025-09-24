#include "mpl_qt.h"
#if defined(_WIN32)
 #include <io.h>
 #include <fcntl.h>
#endif
long readdata() {
  bool displayed = false;
  int numread, numvals, step, newnc;
  static int nc_max = 0;
  char command;
  char *bufptr = NULL;
  FILE *input = NULL;

  if (!ifp)
    input = stdin;
  else
    input = ifp;

#if defined(_WIN32)
  _setmode(_fileno(input), _O_BINARY);
#endif

  /* Read lines from stdin */
  while (1) {
    /* Read one character */
    numread = fread(&command, sizeof(char), 1, input);
    if (numread != 1)
      break;
    if (command == 'G') { /* Enter graphics mode */
      /* Trim any old buffer since we are through writing it */
      if (curwrite) {
        curwrite->buffer = (char *)realloc(curwrite->buffer, curwrite->nc);
      }
      /* Make a new plot */
      curwrite = makeplotrec();
      if (domovie || (curwrite->nplot == currentPlot)) {
        cur = curwrite;
      }
      nc_max = 0;

      /* If there is no cur, set it to curwrite, else show old movie frame */
      if (!cur) {
        cur = curwrite;
      }
      bufptr = curwrite->buffer;
    } else if (!curwrite) {
      /* No record, probably deleted */
      continue;
    } else if (command == 'E') {
      /* Leave graphics mode */
      /* Trim the buffer since we are (probably) through writing it */
      curwrite->buffer = (char *)realloc(curwrite->buffer, curwrite->nc);
      nc_max = curwrite->nc;

      /* Display the plot */
      if (domovie || (keep > 0) || (curwrite->nplot == currentPlot)) {
        refreshCanvas();
        displayed = true;
        if (domovie || (keep > 0)) {
          return 0;
        }
      }
      continue;
    } else if (command == 'R') {
      /* Exit */
      quit();
    }

    /* Determine the space required in addition to commands themselves */
    switch (command) {
    case 'V':
    case 'M':
    case 'P':
      numvals = 2;
      break;
    case 'L':
    case 'W':
      numvals = 1;
      break;
    case 'B':
      numvals = 5;
      break;
    case 'U':
      numvals = 4 * sizeof(double) / sizeof(VTYPE);
      break;
    case 'G':
    case 'R':
    case 'E':
      numvals = 0;
      break;
    case 'C':
      numvals = 3;
      break;
    case 'S':
      numvals = 8;
      break;
    default:
      exit(1);
      break;
    }

    /* Allocate more buffer space if necessary */
    step = numvals * sizeof(VTYPE);
    newnc = curwrite->nc + sizeof(char) + step;
    if (newnc >= nc_max) {
      /* (Re)allocate */
      curwrite->buffer = (char *)realloc(curwrite->buffer, (nc_max += DNC));
      bufptr = curwrite->buffer + curwrite->nc;
    }
    /* Store the commands */
    *bufptr = command;
    bufptr += sizeof(char);
    curwrite->nc++;
    if (numvals) {
      if (fread(bufptr, sizeof(VTYPE) * numvals, 1, input) != 1)
        return 1;
      bufptr += step;
      curwrite->nc += step;
    }
  }
  if (ferror(input)) {
    fprintf(stderr, "Error reading input\n");
    return 1;
  }
  if (feof(input)) {
    if (!displayed) {
      refreshCanvas();
    }
    domovie = false;
    return 1;
  }

  return 0;
}
