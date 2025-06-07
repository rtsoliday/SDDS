/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* file: rpn_csh.c
 * purpose: create and manage a synchronized csh subprocess
 *
 * M. Borland, 1993
 */
#include "rpn_internal.h"
#include <signal.h>
#include <ctype.h>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#if defined(_WIN32) && !defined(__BORLANDC__)
#define SIGUSR1 16
#endif

/* this gets the code to compile, but the code won't work! */
/*
#if defined(_WIN32) || defined(vxWorks)
void sigpause(int x);

void sigpause(x) 
{
  }
#endif
*/

static FILE *fp = NULL;
static int pid;

/* dummy signal handler for use with sigpause */
void dummy_sigusr1(int signum)
{
    }

#if defined(_WIN32)
void rpn_csh()

{
  char *ptr;
  void dummy_sigusr1(int signum);
  static char s[1024];

  signal(SIGUSR1, dummy_sigusr1);

  if (!fp) {
    /* open a pipe and start csh */
    fp = popen("csh", "w");
    pid = getpid();
  }

  /* loop to print prompt and accept commands */
  while (fputs("csh> ", stdout), fgets(s, 100, stdin)) {
    /* send user's command along with another than causes subprocess
     * to send the SIGUSR1 signal this process 
     */
    ptr = s;
    while (isspace(*ptr))
      ptr++;
    if (strncmp(ptr, "quit", 4)==0 || strncmp(ptr, "exit", 4)==0)
      break;
    fprintf(fp, "%s\nkill -USR1 %d\n", s, pid);
    fflush(fp);
    /* pause until SIGUSR1 is received */
    //sigpause(SIGUSR1);
  }

  /* back to default behavior for sigusr1 */
  signal(SIGUSR1, SIG_DFL);
}

void rpn_csh_str()
{
  char *string;
  void dummy_sigusr1();
  signal(SIGUSR1, dummy_sigusr1);

  if (!fp) {
    /* open a pipe and start csh */
    fp = popen("csh", "w");
    pid = getpid();
  }
  if (!(string = pop_string()))
    return;

  fprintf(fp, "%s\nkill -USR1 %d\n", string, pid);
  fflush(fp);
  /* pause until SIGUSR1 is received */
  //sigpause(SIGUSR1);

  /* back to default behavior for sigusr1 */
  signal(SIGUSR1, SIG_DFL);
}

#else

static sigset_t mask, oldmask;
static volatile sig_atomic_t sigusr1_received = 0;

void rpn_csh() {
  char *ptr;
  static char s[1024];

  struct sigaction sa;
  sa.sa_handler = dummy_sigusr1;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGUSR1, &sa, NULL);

  /* Block SIGUSR1 and save old mask */
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);

  if (!fp) {
    /* open a pipe and start csh */
#if defined(vxWorks)
    fprintf(stderr, "popen is not supported in vxWorks\n");
    exit(1);
#endif
    fp = popen("csh", "w");
    pid = getpid();
  }

  /* loop to print prompt and accept commands */
  while (fputs("csh> ", stdout), fgets(s, sizeof(s), stdin)) {
    /* send user's command along with another that causes subprocess
     * to send the SIGUSR1 signal to this process
     */
    ptr = s;
    while (isspace(*ptr))
      ptr++;
    if (strncmp(ptr, "quit", 4) == 0 || strncmp(ptr, "exit", 4) == 0)
      break;
    fprintf(fp, "%s\nkill -USR1 %d\n", s, pid);
    fflush(fp);

    /* Suspend until SIGUSR1 is received */
    sigsuspend(&oldmask);
  }

  /* Restore old signal mask */
  sigprocmask(SIG_SETMASK, &oldmask, NULL);

  /* Back to default behavior for SIGUSR1 */
  sa.sa_handler = SIG_DFL;
  sigaction(SIGUSR1, &sa, NULL);
}

void rpn_csh_str() {
    char *string;
    struct sigaction sa;
    sigset_t mask, oldmask;

    /* Set up signal handler for SIGUSR1 */
    sa.sa_handler = dummy_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    if (!fp) {
#if defined(vxWorks)
        fprintf(stderr, "popen is not supported in vxWorks\n");
        exit(1);
#else
        fp = popen("csh", "w");
        pid = getpid();
#endif
    }
    if (!(string = pop_string()))
        return;

    fprintf(fp, "%s\nkill -USR1 %d\n", string, pid);
    fflush(fp);

    /* Block SIGUSR1 and suspend execution until it's received */
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while (!sigusr1_received) {
        sigsuspend(&oldmask);
    }

    /* Restore previous signal mask */
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    /* Back to default behavior for SIGUSR1 */
    signal(SIGUSR1, SIG_DFL);
}
#endif

void rpn_execs()
{
    char *string;
#if !defined(vxWorks)
    char buffer[1024];
    FILE *fp;
#endif

    if (!(string = pop_string()))
        return;

#if defined(vxWorks)
    fprintf(stderr, "popen is not supported in vxWorks\n");
    exit(1);
#else
    if (!(fp = popen(string, "r"))) {
      fprintf(stderr, "error: invalid command: %s\n", string);
      stop();
      return;
    }
    if (feof(fp)) {
      fprintf(stderr, "error: command %s returns EOF\n", string);
      stop();
      return;
    }
    if (!fgets(buffer, 1024, fp)) {
      fprintf(stderr, "error: command %s returns NULL\n", string);
      stop();
      return;
    }
    do {
      chop_nl(buffer);
      push_string(buffer);
    } while (fgets(buffer, 1024, fp));
#endif

  }

void rpn_execn()
{
#if !defined(vxWorks)
    char *ptr;
    char buffer[1024];
    FILE *fp;
    double value;
#endif
    char *string;

    if (!(string = pop_string()))
        return;

#if defined(vxWorks)
    fprintf(stderr, "popen is not supported in vxWorks\n");
    exit(1);
#else
    if (!(fp = popen(string, "r"))) {
      fprintf(stderr, "error: invalid command: %s\n", string);
      stop();
      return;
    }
    if (feof(fp)) {
      fprintf(stderr, "error: command %s returns EOF\n", string);
      stop();
      return;
    }
    if (fgets(buffer, 1024, fp) == NULL) {
      fprintf(stderr, "error: command %s returns NULL\n", string);
      stop();
      return;
    }
    do {
      while ((ptr=get_token(buffer))) {
        if (sscanf(ptr, "%le", &value)==1)
          push_num(value);
        else
          push_string(ptr);
      }
    } while (fgets(buffer, 1024, fp));
#endif

  }

