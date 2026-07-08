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
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <process.h>
#else
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

#if defined(_WIN32)
static FILE *csh_input_fp = NULL;

static int rpn_csh_start(void)
{
  if (!csh_input_fp && !(csh_input_fp = popen("csh", "w"))) {
    fprintf(stderr, "error: unable to start csh subprocess\n");
    rpn_set_error();
    stop();
    return 0;
  }
  return 1;
}

static int rpn_csh_send_command(const char *command)
{
  if (!rpn_csh_start())
    return 0;
  if (fprintf(csh_input_fp, "%s\n", command) < 0 || fflush(csh_input_fp) == EOF) {
    fprintf(stderr, "error: unable to write to csh subprocess\n");
    rpn_set_error();
    stop();
    return 0;
  }
  return 1;
}

#else

#if defined(vxWorks)
static int rpn_csh_start(void)
{
  fprintf(stderr, "csh subprocesses are not supported in vxWorks\n");
  exit(1);
}

static int rpn_csh_send_command(const char *command)
{
  (void)command;
  return rpn_csh_start();
}

#else

static FILE *csh_input_fp = NULL;
static int csh_ack_fd = -1;
static int csh_ack_hold_fd = -1;
static char csh_ack_dir[PATH_MAX] = "";
static char csh_ack_path[PATH_MAX] = "";
static int csh_cleanup_registered = 0;

static void rpn_csh_reset(void)
{
  if (csh_input_fp) {
    fclose(csh_input_fp);
    csh_input_fp = NULL;
  }
  if (csh_ack_fd >= 0) {
    close(csh_ack_fd);
    csh_ack_fd = -1;
  }
  if (csh_ack_hold_fd >= 0) {
    close(csh_ack_hold_fd);
    csh_ack_hold_fd = -1;
  }
  if (csh_ack_path[0]) {
    unlink(csh_ack_path);
    csh_ack_path[0] = 0;
  }
  if (csh_ack_dir[0]) {
    rmdir(csh_ack_dir);
    csh_ack_dir[0] = 0;
  }
}

static int rpn_csh_error(const char *operation)
{
  if (errno)
    fprintf(stderr, "error: %s failed for csh subprocess: %s\n", operation, strerror(errno));
  else
    fprintf(stderr, "error: %s failed for csh subprocess\n", operation);
  rpn_csh_reset();
  rpn_set_error();
  stop();
  return 0;
}

static int rpn_csh_create_ack_fifo(void)
{
  int flags;

  if (!csh_cleanup_registered) {
    if (atexit(rpn_csh_reset)) {
      errno = 0;
      return rpn_csh_error("register csh cleanup");
    }
    csh_cleanup_registered = 1;
  }

  if (snprintf(csh_ack_dir, sizeof(csh_ack_dir), "/tmp/rpn_csh_%ld_XXXXXX", (long)getpid()) >= (int)sizeof(csh_ack_dir)) {
    errno = ENAMETOOLONG;
    return rpn_csh_error("temporary directory name");
  }
  if (!mkdtemp(csh_ack_dir))
    return rpn_csh_error("mkdtemp");

  if (snprintf(csh_ack_path, sizeof(csh_ack_path), "%s/ack", csh_ack_dir) >= (int)sizeof(csh_ack_path)) {
    errno = ENAMETOOLONG;
    return rpn_csh_error("temporary fifo name");
  }
  if (mkfifo(csh_ack_path, 0600) < 0)
    return rpn_csh_error("mkfifo");

  csh_ack_fd = open(csh_ack_path, O_RDONLY | O_NONBLOCK);
  if (csh_ack_fd < 0)
    return rpn_csh_error("open acknowledgment fifo");

  csh_ack_hold_fd = open(csh_ack_path, O_WRONLY | O_NONBLOCK);
  if (csh_ack_hold_fd < 0)
    return rpn_csh_error("open acknowledgment fifo");

  if ((flags = fcntl(csh_ack_fd, F_GETFL, 0)) < 0)
    return rpn_csh_error("fcntl");
  if (fcntl(csh_ack_fd, F_SETFL, flags & ~O_NONBLOCK) < 0)
    return rpn_csh_error("fcntl");

  return 1;
}

static int rpn_csh_start(void)
{
  int command_pipe[2];
  pid_t child;

  if (csh_input_fp)
    return 1;

  if (!rpn_csh_create_ack_fifo())
    return 0;

  if (pipe(command_pipe) < 0)
    return rpn_csh_error("pipe");

  child = fork();
  if (child < 0) {
    close(command_pipe[0]);
    close(command_pipe[1]);
    return rpn_csh_error("fork");
  }

  if (child == 0) {
    close(command_pipe[1]);
    close(csh_ack_fd);
    close(csh_ack_hold_fd);
    if (dup2(command_pipe[0], STDIN_FILENO) < 0)
      _exit(127);
    close(command_pipe[0]);
    execlp("csh", "csh", (char *)NULL);
    _exit(127);
  }

  close(command_pipe[0]);

  if (!(csh_input_fp = fdopen(command_pipe[1], "w"))) {
    close(command_pipe[1]);
    return rpn_csh_error("fdopen");
  }
  return 1;
}

static int rpn_csh_wait_for_ack(void)
{
  char ch;
  ssize_t bytes;

  do {
    bytes = read(csh_ack_fd, &ch, 1);
  } while (bytes < 0 && errno == EINTR);
  if (bytes <= 0) {
    if (bytes == 0)
      errno = 0;
    return rpn_csh_error("read acknowledgment");
  }

  while (ch != '\n') {
    do {
      bytes = read(csh_ack_fd, &ch, 1);
    } while (bytes < 0 && errno == EINTR);
    if (bytes <= 0) {
      if (bytes == 0)
        errno = 0;
      return rpn_csh_error("read acknowledgment");
    }
  }
  return 1;
}

static int rpn_csh_send_command(const char *command)
{
  if (!rpn_csh_start())
    return 0;

  if (fprintf(csh_input_fp, "%s\n/bin/echo __rpn_ack__ >! %s\n", command, csh_ack_path) < 0 ||
      fflush(csh_input_fp) == EOF)
    return rpn_csh_error("write");

  return rpn_csh_wait_for_ack();
}

#endif
#endif

void rpn_csh()
{
  char *ptr;
  static RPN_THREAD_LOCAL char s[1024];

  rpn_lock();
  while (fputs("csh> ", stdout), fgets(s, sizeof(s), stdin)) {
    ptr = s;
    while (isspace(*ptr))
      ptr++;
    if (strncmp(ptr, "quit", 4) == 0 || strncmp(ptr, "exit", 4) == 0)
      break;
    if (!rpn_csh_send_command(s))
      break;
  }
  rpn_unlock();
}

void rpn_csh_str()
{
  char *string;

  rpn_lock();
  string = pop_string();
  if (string)
    rpn_csh_send_command(string);
  rpn_unlock();
}

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
