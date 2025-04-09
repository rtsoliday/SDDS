/* TCOMP.C *** Program to compare two text files */

#include <stdio.h>
#include <stdlib.h>
#ifdef SUNOS4
#  include <malloc.h>
#endif
#include <string.h>

#define BUFSIZE 1048576
#define LINESIZE 256
#define OK 0
#define ENDOFFILE 1
#define ENDFULLBUF 2

#define SEEK_SET 0 /* Beginning of file */
#define SEEK_END 2 /* End of file */

/* Structures */

/* Function prototypes */

int main(int argc, char **argv);
short checkp(void);
short checkq(void);
void dumpp(void);
void dumpq(void);
void finishp(void);
void finishq(void);
short getlinep(void);
short getlineq(void);
void usage(void);
void quit(int code, char **argv);

/* Global variables */

FILE *pfile, *qfile;
long bufsize = BUFSIZE;
long psize, qsize, pbufsize, qbufsize;
long pstart, pstartnext, pproc0, pproc0next, pproc, pprocnext, p, pnext, pend, pmax;
long qstart, qstartnext, qproc0, qproc0next, qproc, qprocnext, q, qnext, qend, qmax;
long peof = -1, qeof = -1;
long outlinep = 0, outlineq = 0;
long pline = 0, qline = 0, pproc0line, qproc0line, pprocline, qprocline;
int block = 1;
short pstatus, qstatus, pcheck, qcheck, unmatch = 0, identical = 1;
short pold = 0, qold = 0, pfull = 0, qfull = 0, noblank = 0;
char *pbuf, *qbuf;

/**************************** main ****************************************/
int main(int argc, char **argv) {
  long psize2, qsize2;
  int i;

  /* Print command line */
  strcpy(argv[0], "TCOMP");
  for (i = 0; i < argc; i++)
    printf("%s ", argv[i]);
  printf("\n\n");
  /* Check arguments */
  if (argc < 3)
    usage();
  /* Check for options */
  for (i = 3; i < argc; i++) {
    if (argv[i][0] == '-' || argv[i][0] == '/') {
      switch (argv[i][1]) {
      case 'b':
      case 'B':
        noblank = 1;
        break;
      case 'a':
      case 'A':
        bufsize = atol(argv[i] + 2);
        bufsize = (bufsize < (2 * LINESIZE)) ? (2 * LINESIZE) : bufsize;
        break;
      default:
        fprintf(stderr, "\n*** Invalid option %s\n", argv[i]);
        exit(8);
      }
    }
  }
  /* Open files */
  pfile = fopen(argv[1], "rt");
  if (pfile == NULL) {
    fprintf(stderr, "\n*** Unable to open %s\n", argv[1]);
    exit(4);
  }
  qfile = fopen(argv[2], "rt");
  if (qfile == NULL) {
    fprintf(stderr, "\n*** Unable to open %s\n", argv[2]);
    exit(5);
  }
  /* Determine file length */
  fseek(pfile, 0l, SEEK_END);
  psize = ftell(pfile);
  fseek(pfile, 0l, SEEK_SET);
  printf("%s (Length is %ld bytes)\n", argv[1], psize);
  fseek(qfile, 0l, SEEK_END);
  qsize = ftell(qfile);
  fseek(qfile, 0l, SEEK_SET);
  printf("%s (Length is %ld bytes)\n", argv[2], qsize);
  /* Allocate buffers */
  psize2 = psize * 2 + LINESIZE;
  /* 	pbufsize=(psize2>bufsize)?bufsize:psize2; (not nessary in Unix) */
  pbufsize = psize2;
  pbuf = (char *)malloc((size_t)pbufsize);
  if (pbuf == NULL) {
    fprintf(stderr, "\n*** Unable to allocate %ld byte buffer\n", pbufsize);
    printf("\n*** Unable to allocate %ld byte buffer\n", pbufsize);
    exit(6);
  }
  pstart = pstartnext = pproc = p = pnext = pend = 0;
  pmax = pbufsize - LINESIZE - 1;
  qsize2 = qsize * 2 + LINESIZE;
  qbufsize = (qsize2 > bufsize) ? bufsize : qsize2;
  qbuf = (char *)malloc((size_t)qbufsize);
  if (qbuf == NULL) {
    fprintf(stderr, "\n*** Unable to allocate %ld byte buffer\n", qbufsize);
    printf("\n*** Unable to allocate %ld byte buffer\n", qbufsize);
    exit(7);
  }
  qstart = qstartnext = qproc = q = qnext = qend = 0;
  qmax = qbufsize - LINESIZE - 1;
  /* Process lines */
  for (;;) {
    /* Get next two lines */
    pstatus = getlinep();
    qstatus = getlineq();
    /* Check for end of file */
    if (pstatus == ENDOFFILE && qstatus == ENDOFFILE) {
      if (identical == 1) {
        printf("\nFiles are identical\n");
        exit(0);
      }
      quit(1, argv);
    }
    if (pstatus == ENDOFFILE) {
      printf("\n[%d] These lines inserted in %s"
             " starting at line %ld:\n",
             block++, argv[2], qline);
      finishq();
      quit(1, argv);
    }
    if (qstatus == ENDOFFILE) {
      printf("\n[%d] These lines inserted in %s"
             " starting at line %ld:\n",
             block++, argv[1], pline);
      finishp();
      quit(1, argv);
    }
    /* Check for end of full buffer */
    if (pstatus == ENDFULLBUF || qstatus == ENDFULLBUF) {
      fprintf(stderr, "\n*** End of buffer reached unexpectedly\n\a\a");
      quit(20, argv);
    }
    /* Check if lines compare */
    if (strcmp(pbuf + p, qbuf + q) == 0) {
      pold = qold = 0;
      pstart = pnext;
      qstart = qnext;
      continue;
    }
    /* Look for a comparison further down */
    identical = 0;
    pstart = pproc0 = p;
    pstartnext = pproc0next = pnext;
    pproc0line = pline;
    qstart = qproc0 = q;
    qstartnext = qproc0next = qnext;
    qproc0line = qline;
    for (;;) {
      /* Look in first file */
      pproc = p;
      pprocnext = pnext;
      pprocline = pline;
      pcheck = checkp();
      if (pcheck == OK) {
        if (unmatch) {
          printf("\n[%d] These lines from %s"
                 " starting at line %ld:\n",
                 block, argv[1], pproc0line);
          dumpp();
          printf("\n[%d] Replace these lines from %s"
                 " starting at line %ld:\n",
                 block++, argv[2], qproc0line);
          dumpq();
          break;
        } else {
          printf("\n[%d] These lines inserted in %s"
                 " starting at line %ld:\n",
                 block++, argv[1], pproc0line);
          dumpp();
          break;
        }
      }
      /* Look in second file */
      p = pproc;
      pnext = pprocnext;
      pline = pprocline;
      qproc = q;
      qprocnext = qnext;
      qprocline = qline;
      qcheck = checkq();
      if (qcheck == OK) {
        if (!unmatch) {
          printf("\n[%d] These lines inserted in %s"
                 " starting at line %ld:\n",
                 block++, argv[2], qproc0line);
          dumpq();
          break;
        } else {
          printf("\n[%d] These lines from %s"
                 " starting at line %ld:\n",
                 block, argv[1], pproc0line);
          dumpp();
          printf("\n[%d] Replace these lines from %s"
                 " starting at line %ld:\n",
                 block++, argv[2], qproc0line);
          dumpq();
          break;
        }
      }
      /* Not in either file, try next two lines */
      unmatch = 1;
      q = qproc;
      qnext = qprocnext;
      qline = qprocline;
      pstatus = getlinep();
      qstatus = getlineq();
      /* Check for end of file */
      if (pstatus == ENDOFFILE && qstatus == ENDOFFILE) {
        printf("\n[%d] These lines from %s"
               " starting at line %ld:\n",
               block, argv[1], pproc0line);
        dumpp();
        printf("\n[%d] Replace these lines from %s"
               " starting at line %ld:\n",
               block++, argv[2], qproc0line);
        dumpq();
        quit(1, argv);
      }
      if (pstatus == ENDOFFILE) {
        printf("\n[%d] These lines from %s"
               " starting at line %ld:\n",
               block, argv[1], pproc0line);
        dumpp();
        printf("\n[%d] Replace these lines from %s"
               " starting at line %ld:\n",
               block++, argv[2], qproc0line);
        dumpq();
        finishq();
        quit(1, argv);
      }
      if (qstatus == ENDOFFILE) {
        printf("\n[%d] These lines from %s"
               " starting at line %ld:\n",
               block, argv[1], pproc0line);
        dumpp();
        finishp();
        printf("\n[%d] Replace these lines from %s"
               " starting at line %ld:\n",
               block++, argv[2], qproc0line);
        dumpq();
        quit(1, argv);
      }
      /* Check for end of buffer */
      if (pstatus == ENDFULLBUF || qstatus == ENDFULLBUF) {
        printf("\n[%d] These lines from %s"
               " starting at line %ld:\n",
               block, argv[1], pproc0line);
        dumpp();
        printf("\n[%d] Replace these lines from %s"
               " starting at line %ld:\n",
               block++, argv[2], qproc0line);
        dumpq();
        fprintf(stderr, "\n*** Number of unmatched lines exceeds "
                "buffer capacity\n\a\a");
        printf("\n*** Files too different to continue\n");
        exit(20);
      }
      /* Check if lines compare */
      if (strcmp(pbuf + p, qbuf + q) == 0) {
        printf("\n[%d] These lines from %s"
               " starting at line %ld:\n",
               block, argv[1], pproc0line);
        dumpp();
        printf("\n[%d] Replace these lines from %s"
               " starting at line %ld:\n",
               block++, argv[2], qproc0line);
        dumpq();
        break;
      }
    }
  }
  return (OK);
}
/**************************** checkp **************************************/
short checkp(void) {
  for (;;) {
    /* Get next line */
    pstatus = getlinep();
    /* Check for end of file or buffer full */
    if (pstatus == ENDOFFILE || pstatus == ENDFULLBUF) {
      return (pstatus);
    }
    /* Check if lines compare */
    if (strcmp(pbuf + p, qbuf + q) == 0) {
      return (OK);
    }
  }
}
/**************************** checkq **************************************/
short checkq(void) {
  for (;;) {
    /* Get next line */
    qstatus = getlineq();
    /* Check for end of file or buffer full */
    if (qstatus == ENDOFFILE || qstatus == ENDFULLBUF) {
      return (qstatus);
    }
    /* Check if lines compare */
    if (strcmp(pbuf + p, qbuf + q) == 0) {
      return (OK);
    }
  }
}
/**************************** dumpp ***************************************/
void dumpp(void) {
  long psave, pnextsave, plinesave;
  short noblanksave;

  noblanksave = noblank;
  noblank = 0;
  printf("%s", pbuf + pproc0);
  outlinep++;
  psave = p;
  plinesave = pline;
  pnextsave = pnext;
  for (p = pproc0, pnext = pproc0next, pline = pproc0line;; p++) {
    pstatus = getlinep();
    if (p == psave)
      break;
    printf("%s", pbuf + p);
    outlinep++;
  }
  pstart = pnext = pnextsave;
  pline = plinesave;
  pold = unmatch = 0;
  noblank = noblanksave;
}
/**************************** dumpq ***************************************/
void dumpq(void) {
  long qsave, qnextsave, qlinesave;
  short noblanksave;

  noblanksave = noblank;
  noblank = 0;
  printf("%s", qbuf + qproc0);
  outlineq++;
  qsave = q;
  qlinesave = qline;
  qnextsave = qnext;
  for (q = qproc0, qnext = qproc0next;; q++) {
    qstatus = getlineq();
    if (q == qsave)
      break;
    printf("%s", qbuf + q);
    outlineq++;
  }
  qstart = qnext = qnextsave;
  qline = qlinesave;
  qold = unmatch = 0;
  noblank = noblanksave;
}
/**************************** finishp *************************************/
void finishp(void) {
  short noblanksave;

  noblanksave = noblank;
  noblank = 0;
  printf("%s", pbuf + p);
  outlinep++;
  for (;;) {
    pstatus = getlinep();
    if (pstatus == ENDOFFILE)
      return;
    printf("%s", pbuf + p);
    outlinep++;
  }
  noblank = noblanksave;
}
/**************************** finishq *************************************/
void finishq(void) {
  short noblanksave;

  noblanksave = noblank;
  noblank = 0;
  printf("%s", qbuf + q);
  outlineq++;
  for (;;) {
    qstatus = getlineq();
    if (qstatus == ENDOFFILE)
      return;
    printf("%s", qbuf + q);
    outlineq++;
  }
  noblank = noblanksave;
}
/**************************** getlinep ************************************/
short getlinep(void) {
  char *err;
  size_t len;
  long del = 0;
  short read = 0;

 REDO:
  p = pnext;
  /* Check if at end of file */
  if (p == peof)
    return (ENDOFFILE);
  /* Read from file if at end of buffer and buffer is not full */
  if (p == pend) {
    if (pfull == 1 && pstart == 0 && pend == 0)
      return (ENDFULLBUF);
    else
      pfull = 0;
    del = pend - pstart;
    if (del > pmax)
      return (ENDFULLBUF);
    if (del < 0 && (-del) < LINESIZE)
      return (ENDFULLBUF);
    read = 1;
    err = fgets(pbuf + p, LINESIZE, pfile);
    if (err == NULL) {
      pend = peof = p;
      return (ENDOFFILE);
    }
  }
  pline++;
  /* Find next p */
  len = strlen(pbuf + p);
  pnext = p + len + 1;
  if (pnext > pmax) {
    pnext = 0;
    if (pstart == 0)
      pfull = 1;
  }
  if (read)
    pend = pnext;
  /* Determine next p after start if this is the start */
  if (!pold) {
    pstartnext = pnext;
    pold = 1;
  }
  /* Check for blank line */
  if (noblank && *(pbuf + p) == '\n')
    goto REDO;
  return (OK);
}
/**************************** getlineq ************************************/
short getlineq(void) {
  char *err;
  size_t len;
  long del = 0;
  short read = 0;

 REDO:
  q = qnext;
  /* Check if at end of file */
  if (q == qeof)
    return (ENDOFFILE);
  /* Read from file if at end of buffer and buffer is not full */
  if (q == qend) {
    if (qfull == 1 && qstart == 0 && qend == 0)
      return (ENDFULLBUF);
    else
      qfull = 0;
    del = qend - qstart;
    if (del > qmax)
      return (ENDFULLBUF);
    if (del < 0 && (-del) < LINESIZE)
      return (ENDFULLBUF);
    read = 1;
    err = fgets(qbuf + q, LINESIZE, qfile);
    if (err == NULL) {
      qend = qeof = q;
      return (ENDOFFILE);
    }
  }
  qline++;
  /* Find next q */
  len = strlen(qbuf + q);
  qnext = q + len + 1;
  if (qnext > qmax) {
    qnext = 0;
    if (qstart == 0)
      qfull = 1;
  }
  if (read)
    qend = qnext;
  /* Determine next q after start if this is the start */
  if (!qold) {
    qstartnext = qnext;
    qold = 1;
  }
  /* Check for blank line */
  if (noblank && *(qbuf + q) == '\n')
    goto REDO;
  return (OK);
}
/**************************** quit ****************************************/
void quit(int code, char **argv) {
  printf("\nTotal of %ld different line(s) found in %d group(s)\n",
         outlinep + outlineq, --block);
  printf("  %ld line(s) in %s\n", outlinep, argv[1]);
  printf("  %ld line(s) in %s\n", outlineq, argv[2]);
  exit(code);
}
/**************************** usage ***************************************/
void usage(void) {
  printf("Usage is: TCOMP filename1 filename2 [-an] [-b]\n");
  printf("  Options:\n");
  printf("    -an  Set look ahead buffer size to n bytes [%d]\n", BUFSIZE);
  printf("    -b   Ignore blank lines\n");
  exit(3);
}
