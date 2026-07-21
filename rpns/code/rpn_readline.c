/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

/* file: rpn_readline.c
 * contents: rpn_getline()
 * purpose : interactive line input for the rpn calculator with an editable,
 *           recallable command history.
 *
 *   rpn_getline() is a drop-in replacement for the prompt()+fgets() pair that
 *   the rpn REPL used to read each command.  When standard input is an
 *   interactive terminal it runs a small self-contained line editor (POSIX
 *   termios raw mode) that supports in-line editing and up/down recall of
 *   previous commands, persisted across sessions in a history file.  For any
 *   non-interactive input (pipes, redirected files, pushed command files,
 *   RPN_DEFNS) or on platforms without termios, it falls back to plain fgets()
 *   so behavior is byte-for-byte unchanged.
 *
 *   No external dependency (deliberately not GNU readline, whose GPL is
 *   incompatible with the SDDS license).
 */

#include "mdb.h"
#include "rpn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if !defined(_WIN32)
#  include <unistd.h>
#  include <termios.h>
#  include <errno.h>
#  include <sys/ioctl.h>
#endif

/* ---------------------------------------------------------------------- */
/* Command history (shared by interactive editing and the history file).   */
/* ---------------------------------------------------------------------- */

#define RPN_HISTORY_MAX 1000
#define RPN_LINE_MAX 16384

static char **rpn_history = NULL;
static long rpn_history_count = 0;  /* number of stored entries */
static long rpn_history_loaded = 0; /* history file loaded + save handler set */

static char *rpn_history_filename(void) {
  static char buffer[1024];
  char *path;
  if ((path = getenv("RPN_HISTFILE")))
    return path;
  if ((path = getenv("HOME"))) {
    snprintf(buffer, sizeof(buffer), "%s/.rpn_history", path);
    return buffer;
  }
  return NULL;
}

/* Add a line to the in-memory history, skipping blank lines and exact
 * duplicates of the most recent entry (bash-like). */
static void rpn_history_add(const char *line) {
  const char *p = line;
  char *copy;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (*p == 0)
    return;
  if (rpn_history_count > 0 && strcmp(rpn_history[rpn_history_count - 1], line) == 0)
    return;
  if (!(copy = malloc(strlen(line) + 1)))
    return;
  strcpy(copy, line);
  if (rpn_history_count >= RPN_HISTORY_MAX) {
    /* drop the oldest entry */
    free(rpn_history[0]);
    memmove(rpn_history, rpn_history + 1, sizeof(*rpn_history) * (RPN_HISTORY_MAX - 1));
    rpn_history_count = RPN_HISTORY_MAX - 1;
  }
  {
    char **grown = realloc(rpn_history, sizeof(*rpn_history) * (rpn_history_count + 1));
    if (!grown) {
      free(copy);
      return;
    }
    rpn_history = grown;
  }
  rpn_history[rpn_history_count++] = copy;
}

/* Registered via atexit(): rewrite the history file.  Errors are ignored so a
 * read-only or missing $HOME never disrupts the calculator. */
static void rpn_history_save(void) {
  char *path = rpn_history_filename();
  FILE *fp;
  long i;
  if (!path || !(fp = fopen(path, "w")))
    return;
  for (i = 0; i < rpn_history_count; i++)
    fprintf(fp, "%s\n", rpn_history[i]);
  fclose(fp);
}

/* Load the history file once, then arrange for it to be saved at exit. */
static void rpn_history_init(void) {
  char *path;
  FILE *fp;
  char line[RPN_LINE_MAX];
  if (rpn_history_loaded)
    return;
  rpn_history_loaded = 1;
  if ((path = rpn_history_filename()) && (fp = fopen(path, "r"))) {
    while (fgets(line, sizeof(line), fp)) {
      size_t n = strlen(line);
      while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = 0;
      rpn_history_add(line);
    }
    fclose(fp);
  }
  atexit(rpn_history_save);
}

/* ---------------------------------------------------------------------- */
/* Interactive line editor (termios; not built on Windows).                */
/* ---------------------------------------------------------------------- */

#if !defined(_WIN32)

static struct termios rpn_saved_termios;
static int rpn_raw_active = 0;
static int rpn_atexit_registered = 0;

/* Safety net: if any exit path is taken while the terminal is still in raw
 * mode, put it back the way we found it. */
static void rpn_restore_termios(void) {
  if (rpn_raw_active) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &rpn_saved_termios);
    rpn_raw_active = 0;
  }
}

static int rpn_interactive_tty(FILE *fp) {
  return fp == stdin && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

static int rpn_read_byte(void) {
  unsigned char ch;
  ssize_t n;
  do {
    n = read(STDIN_FILENO, &ch, 1);
  } while (n < 0 && errno == EINTR);
  if (n <= 0)
    return -1;
  return ch;
}

static long rpn_term_width(void) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
    return ws.ws_col;
  return 80;
}

/* Redraw the prompt and the current line, with horizontal scrolling so the
 * cursor is always visible even for lines wider than the terminal. */
static void rpn_refresh(const char *prompt_s, const char *buf, long len, long pos) {
  long width = rpn_term_width();
  long plen = (long)strlen(prompt_s);
  long avail = width - plen - 1;
  long offset, shown, col;
  char seq[32];

  if (avail < 1)
    avail = 1;
  offset = (pos > avail) ? (pos - avail) : 0;
  shown = len - offset;
  if (shown > avail)
    shown = avail;
  if (shown < 0)
    shown = 0;

  fputc('\r', stdout);
  fputs(prompt_s, stdout);
  fwrite(buf + offset, 1, (size_t)shown, stdout);
  fputs("\x1b[K", stdout); /* clear to end of line */
  fputc('\r', stdout);
  col = plen + (pos - offset);
  if (col > 0) {
    snprintf(seq, sizeof(seq), "\x1b[%ldC", col);
    fputs(seq, stdout);
  }
  fflush(stdout);
}

/* Copy src into the edit buffer, updating length and cursor position. */
static void rpn_set_line(char *buf, long buflen, long *len, long *pos, const char *src) {
  long n = (long)strlen(src);
  if (n > buflen - 2)
    n = buflen - 2;
  memcpy(buf, src, (size_t)n);
  buf[n] = 0;
  *len = *pos = n;
}

/* Read and edit one line interactively.  Returns buf (NUL-terminated, with a
 * trailing newline, matching fgets) on Enter, or NULL on end-of-file. */
static char *rpn_edit_line(const char *prompt_s, char *buf, long buflen) {
  struct termios orig, raw;
  long len = 0, pos = 0;
  long histindex;
  char *saved; /* the in-progress line, stashed while browsing history */
  int c, eof = 0;

  rpn_history_init();

  if (tcgetattr(STDIN_FILENO, &orig) < 0) {
    if (prompt_s) {
      fputs(prompt_s, stdout);
      fflush(stdout);
    }
    return fgets(buf, buflen, stdin);
  }
  raw = orig;
  raw.c_lflag &= ~(ICANON | ECHO | ISIG);
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
    if (prompt_s) {
      fputs(prompt_s, stdout);
      fflush(stdout);
    }
    return fgets(buf, buflen, stdin);
  }
  if (!rpn_atexit_registered) {
    atexit(rpn_restore_termios);
    rpn_atexit_registered = 1;
  }
  rpn_saved_termios = orig;
  rpn_raw_active = 1;

  if (!(saved = malloc((size_t)buflen)))
    saved = NULL;
  else
    saved[0] = 0;

  buf[0] = 0;
  len = pos = 0;
  histindex = rpn_history_count;
  rpn_refresh(prompt_s, buf, len, pos);

  for (;;) {
    c = rpn_read_byte();
    if (c < 0) { /* EOF/error on read: treat as Ctrl-D */
      c = 4;
    }

    if (c == '\r' || c == '\n') { /* accept line */
      break;
    } else if (c == 3) { /* Ctrl-C: abandon the current line */
      len = pos = 0;
      buf[0] = 0;
      fputc('\n', stdout);
      fflush(stdout);
      break;
    } else if (c == 4) { /* Ctrl-D: EOF if empty, else delete char under cursor */
      if (len == 0) {
        eof = 1;
        break;
      }
      if (pos < len) {
        memmove(buf + pos, buf + pos + 1, (size_t)(len - pos - 1));
        buf[--len] = 0;
      }
    } else if (c == 1) { /* Ctrl-A: start of line */
      pos = 0;
    } else if (c == 5) { /* Ctrl-E: end of line */
      pos = len;
    } else if (c == 2) { /* Ctrl-B: left */
      if (pos > 0)
        pos--;
    } else if (c == 6) { /* Ctrl-F: right */
      if (pos < len)
        pos++;
    } else if (c == 8 || c == 127) { /* Backspace: delete char to the left */
      if (pos > 0) {
        memmove(buf + pos - 1, buf + pos, (size_t)(len - pos));
        len--;
        pos--;
        buf[len] = 0;
      }
    } else if (c == 21) { /* Ctrl-U: kill to start of line */
      memmove(buf, buf + pos, (size_t)(len - pos));
      len -= pos;
      pos = 0;
      buf[len] = 0;
    } else if (c == 11) { /* Ctrl-K: kill to end of line */
      len = pos;
      buf[len] = 0;
    } else if (c == 23) { /* Ctrl-W: delete previous word */
      long start = pos;
      while (start > 0 && isspace((unsigned char)buf[start - 1]))
        start--;
      while (start > 0 && !isspace((unsigned char)buf[start - 1]))
        start--;
      memmove(buf + start, buf + pos, (size_t)(len - pos));
      len -= (pos - start);
      pos = start;
      buf[len] = 0;
    } else if (c == 12) { /* Ctrl-L: clear screen and redraw */
      fputs("\x1b[H\x1b[2J", stdout);
    } else if (c == 16) { /* Ctrl-P: previous history */
      goto history_prev;
    } else if (c == 14) { /* Ctrl-N: next history */
      goto history_next;
    } else if (c == 27) { /* escape sequence */
      int c1 = rpn_read_byte();
      if (c1 == '[' || c1 == 'O') {
        int c2 = rpn_read_byte();
        if (c2 >= '0' && c2 <= '9') {
          int c3 = rpn_read_byte(); /* extended sequence terminated by '~' */
          if (c2 == '3' && c3 == '~') { /* Delete key */
            if (pos < len) {
              memmove(buf + pos, buf + pos + 1, (size_t)(len - pos - 1));
              buf[--len] = 0;
            }
          } else if ((c2 == '1' || c2 == '7') && c3 == '~') { /* Home */
            pos = 0;
          } else if ((c2 == '4' || c2 == '8') && c3 == '~') { /* End */
            pos = len;
          }
        } else {
          switch (c2) {
          case 'A': /* Up */
          history_prev:
            if (rpn_history_count > 0 && histindex > 0) {
              if (histindex == rpn_history_count && saved) {
                strncpy(saved, buf, (size_t)buflen - 1);
                saved[buflen - 1] = 0;
              }
              histindex--;
              rpn_set_line(buf, buflen, &len, &pos, rpn_history[histindex]);
            }
            break;
          case 'B': /* Down */
          history_next:
            if (histindex < rpn_history_count) {
              histindex++;
              if (histindex == rpn_history_count)
                rpn_set_line(buf, buflen, &len, &pos, saved ? saved : "");
              else
                rpn_set_line(buf, buflen, &len, &pos, rpn_history[histindex]);
            }
            break;
          case 'C': /* Right */
            if (pos < len)
              pos++;
            break;
          case 'D': /* Left */
            if (pos > 0)
              pos--;
            break;
          case 'H': /* Home */
            pos = 0;
            break;
          case 'F': /* End */
            pos = len;
            break;
          default:
            break;
          }
        }
      }
    } else if (c >= 32 && c < 127) { /* printable: insert at cursor */
      if (len < buflen - 2) {
        memmove(buf + pos + 1, buf + pos, (size_t)(len - pos));
        buf[pos] = (char)c;
        len++;
        pos++;
        buf[len] = 0;
      }
    }
    /* ignore any other control byte */

    rpn_refresh(prompt_s, buf, len, pos);
  }

  /* restore the terminal before returning */
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
  rpn_raw_active = 0;
  if (saved)
    free(saved);

  if (eof) {
    fputc('\n', stdout);
    fflush(stdout);
    return NULL;
  }

  /* Newline after the committed line, add it to the history, and hand back the
   * line with a trailing newline so callers see fgets-compatible content. */
  if (c == '\r' || c == '\n') {
    fputc('\n', stdout);
    fflush(stdout);
    rpn_history_add(buf);
  }
  if (len > buflen - 2)
    len = buflen - 2;
  buf[len] = '\n';
  buf[len + 1] = 0;
  return buf;
}

#endif /* !_WIN32 */

/* ---------------------------------------------------------------------- */
/* Public entry point.                                                     */
/* ---------------------------------------------------------------------- */

char *rpn_getline(char *prompt_s, long do_prompt, char *buf, long buflen, FILE *fp) {
#if !defined(_WIN32)
  if (do_prompt && rpn_interactive_tty(fp))
    return rpn_edit_line(prompt_s, buf, buflen);
#endif
  if (do_prompt && prompt_s) {
    fputs(prompt_s, stdout);
    fflush(stdout);
  }
  return fgets(buf, buflen, fp);
}
