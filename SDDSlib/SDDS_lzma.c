/**
 * @file SDDS_lzma.c
 * @brief Implementation of LZMA-compressed file handling functions.
 *
 * This file provides a set of functions to work with files compressed using the LZMA
 * compression algorithm. It abstracts the complexities of LZMA stream handling, offering
 * a simple file-like interface for reading from and writing to compressed files.
 *
 * Features:
 * - Open and close LZMA-compressed files.
 * - Read and write data with automatic compression/decompression.
 * - Support for reading lines and formatted output.
 * - Error handling with descriptive messages.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 *  R. Soliday
 */



#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
//#typedef long_ptr ssize_t
#else
#  include <syslog.h>
#  include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <lzma.h>

#if LZMA_VERSION <= UINT32_C(49990030)
#  define LZMA_EASY_ENCODER(a, b) lzma_easy_encoder_single(a, b)
#elif LZMA_VERSION <= UINT32_C(49990050)
#  define LZMA_EASY_ENCODER(a, b) lzma_easy_encoder(a, b)
#else
#  define LZMA_EASY_ENCODER(a, b) lzma_easy_encoder(a, b, LZMA_CHECK_CRC32)
#endif

#define BUF_SIZE 40960

static const lzma_stream lzma_stream_init = LZMA_STREAM_INIT;

struct lzmafile {
  lzma_stream str;               /* codec stream descriptor */
  FILE *fp;                      /* backing file descriptor */
  char mode;                     /* access mode ('r' or 'w') */
  unsigned char rdbuf[BUF_SIZE]; /* read buffer used by lzmaRead */
};

static int lzmaCompressionLevel = 2;

void SDDS_SetLZMACompressionLevel(int32_t level) {
  if (level >= 0 && level <= 9)
    lzmaCompressionLevel = level;
}

int32_t SDDS_GetLZMACompressionLevel(void) {
  return lzmaCompressionLevel;
}

/* lzma_open opens the file whose name is the string pointed to
   by 'path' and associates a stream with it. The 'mode' argument
   is expected to be 'r' or 'w'. Upon successful completion, a 
   lzmafile pointer will be returned. Upon error, NULL will be returned.*/
void *lzma_open(const char *path, const char *mode) {
  int ret;

  /* initialize LZMA stream */
  struct lzmafile *lf = malloc(sizeof(struct lzmafile));
  lf->fp = fopen(path, mode);
  lf->str = lzma_stream_init;
  lf->mode = mode[0];
  if (mode[0] == 'r') {
#if LZMA_VERSION <= UINT32_C(49990030)
    ret = lzma_auto_decoder(&lf->str, NULL, NULL);
#else
    ret = lzma_auto_decoder(&lf->str, -1, 0);
#endif
    lf->str.avail_in = 0;
  } else {
    ret = LZMA_EASY_ENCODER(&lf->str, lzmaCompressionLevel);
  }
  if (ret != LZMA_OK) {
    fprintf(stderr, "lzma_open error: %d\n", ret);
    return NULL;
  }
  return (void *)lf;
}

/* lzma_close flushes the stream pointed to by the lzmafile pointer
   and closes the underlying file descriptor. Upon successful 
   completion 0 is returned. On error, EOF is returned. */
int lzma_close(struct lzmafile *file) {
  int ret, outsize;
  unsigned char buf[BUF_SIZE]; /* buffer used when flushing remaining
                                  output data in write mode */
  if (!file)
    return -1;
  if (file->mode == 'w') {
    /* flush LZMA output buffer */
    for (;;) {
      file->str.next_out = buf;
      file->str.avail_out = BUF_SIZE;
      ret = lzma_code(&file->str, LZMA_FINISH);
      if (ret != LZMA_STREAM_END && ret != LZMA_OK) {
        fprintf(stderr, "lzma_close error: encoding failed: %d\n", ret);
        lzma_end(&file->str);
        fclose(file->fp);
        free(file);
        return EOF;
      }
      outsize = BUF_SIZE - file->str.avail_out;
      if (fwrite(buf, 1, outsize, file->fp) != outsize) {
        lzma_end(&file->str);
        fclose(file->fp);
        free(file);
        return EOF;
      }
      if (ret == LZMA_STREAM_END)
        break;
    }
  }
  lzma_end(&file->str);
  ret = fclose(file->fp);
  free(file);
  return ret;
}

/* lzma_read attempts to read up to 'count' bytes from the 
   lzmafile pointer into the buffer 'buf'. On success, the 
   number of bytes is returned. On error, -1 is returned. 
   The 'buf' variable is not terminated by '\0'. */
long lzma_read(struct lzmafile *file, void *buf, size_t count) {
  int ret;
  lzma_stream *lstr;
  if (file->mode != 'r')
    return -1;

  lstr = &file->str;
  lstr->next_out = buf;
  lstr->avail_out = count;

  /* decompress until EOF or output buffer is full */
  while (lstr->avail_out) {
    if (lstr->avail_in == 0) {
      /* refill input buffer */
      ret = fread(file->rdbuf, 1, BUF_SIZE, file->fp);
      if (ret == 0) {
        break; /* EOF */
      }
      lstr->next_in = file->rdbuf; /* buffer containing lzma data just read */
      lstr->avail_in = ret;        /* number of bytes read */
    }
    ret = lzma_code(lstr, LZMA_RUN);
    /* this fills up lstr->next_out and decreases lstr->avail_out */
    /* it also emptys lstr->next_in and decreases lstr->avail_in */
    if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
      fprintf(stderr, "lzma_read error: decoding failed: %d\n", ret);
      return -1;
    }
    if (ret == LZMA_STREAM_END) {
      break; /* EOF */
    }
  }
  return count - lstr->avail_out; /* length of buf that has valid data */
}

/* lzma_gets reads in at most one less than 'size' characters from 
   the the lzmafile pointer into the buffer pointed to by 's'. 
   Reading stops after an EOF or a newline. If a newline is read
   it is stored into the buffer. A '\0' is stored after the 
   last character in the buffer. Returns 's' on success and NULL on
   error. */
char *lzma_gets(char *s, int size, struct lzmafile *file) {
  int ret;
  int i = 0;
  lzma_stream *lstr;
  if (file->mode != 'r')
    return NULL;
  if (s == NULL || size < 1)
    return NULL;
  s[0] = '\0';
  lstr = &file->str;
  lstr->next_out = (void *)s;

  /* decompress until newline or EOF or output buffer is full */
  while (1) {
    if (lstr->avail_in == 0) {
      /* refill input buffer */
      ret = fread(file->rdbuf, 1, BUF_SIZE, file->fp);
      if (ret == 0) {
        break; /* EOF */
      }
      lstr->next_in = file->rdbuf; /* buffer containing lzma data just read */
      lstr->avail_in = ret;        /* number of bytes read */
    }
    if (i + 1 == size) {
      s[i] = '\0';
      break;
    }
    lstr->avail_out = 1;
    ret = lzma_code(lstr, LZMA_RUN);
    /* this fills up lstr->next_out and decreases lstr->avail_out */
    /* it also emptys lstr->next_in and decreases lstr->avail_in */
    if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
      fprintf(stderr, "lzma_gets error: decoding failed: %d\n", ret);
      return NULL;
    }
    if (ret == LZMA_STREAM_END) { /* EOF */
      s[i + 1] = '\0';
      break;
    }
    if (s[i] == 10) { /* 10 is the value for \n */
      if (i > 0) {    /* we sometimes get \10\10 */
        if ((i == 1) && (s[0] == 32)) {
          /* when uncompressing the lzma stream we some times end 
		     up with \10\32\10 instead of a simple \10 */
        } else {
          s[i + 1] = '\0';
          break;
        }
      }
    }
    i++;
  }
  return s;
}

/* lzma_write writes up to 'count' bytes from the buffer 'buf' 
   to the file referred to by the lzmafile pointer. On success, 
   the number of bytes written is returned. On error, -1 is returned. */
long lzma_write(struct lzmafile *file, const void *buf, size_t count) {
  int ret;
  lzma_stream *lstr = &file->str;
  unsigned char *bufout; /* compressed output buffer */
  bufout = malloc(sizeof(char) * count);

  if (file->mode != 'w') {
    fprintf(stderr, "lzma_write error: file was not opened for writting\n");
    free(bufout);
    return -1;
  }
  lstr->next_in = buf;
  lstr->avail_in = count;
  while (lstr->avail_in) {
    lstr->next_out = bufout;
    lstr->avail_out = count;
    ret = lzma_code(lstr, LZMA_RUN);
    if (ret != LZMA_OK) {
      fprintf(stderr, "lzma_write error: encoding failed: %d\n", ret);
      free(bufout);
      return -1;
    }
    ret = fwrite(bufout, 1, count - lstr->avail_out, file->fp);
    if (ret != count - lstr->avail_out) {
      fprintf(stderr, "lzma_write error\n");
      free(bufout);
      return -1;
    }
  }
  free(bufout);
  return count;
}

/* lzma_puts writes the string 's' to the lzmafile file pointer,
   without its trailing '\0'. Returns a non-negative number on
   success, or EOF on error. */
int lzma_puts(const char *s, struct lzmafile *file) {
  int ret;
  lzma_stream *lstr = &file->str;
  int count;
  unsigned char *bufout; /* compressed output buffer */
  char *buf;

  if (file->mode != 'w') {
    fprintf(stderr, "lzma_puts error: file was not opened for writting\n");
    return EOF;
  }
  count = strlen(s);
  bufout = malloc(sizeof(unsigned char) * count);
  buf = malloc(sizeof(char) * count);
  //strncpy(buf, s, count);
  memcpy(buf, s, count);

  lstr->next_in = (void *)buf;
  lstr->avail_in = count;
  while (lstr->avail_in) {
    lstr->next_out = bufout;
    lstr->avail_out = count;
    ret = lzma_code(lstr, LZMA_RUN);
    if (ret != LZMA_OK) {
      fprintf(stderr, "lzma_puts error: encoding failed: %d\n", ret);
      free(bufout);
      free(buf);
      return EOF;
    }
    ret = fwrite(bufout, 1, count - lstr->avail_out, file->fp);
    if (ret != count - lstr->avail_out) {
      fprintf(stderr, "lzma_puts error\n");
      free(bufout);
      free(buf);
      return EOF;
    }
  }
  free(bufout);
  free(buf);
  return count;
}

/* lzma_putc writes the character 'c', cast to an unsigned char, 
   to the lzmafile file pointer. Returns the character written as
   an unsigned char cast to an int or EOF on error. */
int lzma_putc(int c, struct lzmafile *file) {
  int ret;
  lzma_stream *lstr = &file->str;

  unsigned char bufout[1]; /* compressed output buffer */
  char buf[1];

  if (file->mode != 'w') {
    fprintf(stderr, "lzma_putc error: file was not opened for writting\n");
    return EOF;
  }
  buf[0] = c;

  lstr->next_in = (void *)buf;
  lstr->avail_in = 1;
  while (lstr->avail_in) {
    lstr->next_out = bufout;
    lstr->avail_out = 1;
    ret = lzma_code(lstr, LZMA_RUN);
    if (ret != LZMA_OK) {
      fprintf(stderr, "lzma_putc error: encoding failed: %d\n", ret);
      return EOF;
    }
    ret = fwrite(bufout, 1, 1 - lstr->avail_out, file->fp);
    if (ret != 1 - lstr->avail_out) {
      fprintf(stderr, "lzma_putc error\n");
      return EOF;
    }
  }
  return (unsigned char)c;
}

/* lzma_printf writes the output to the given lzmafile file pointer.
   Upon success, it returns the number of characters printed (not
   including the trailing '\0'). If an output error is encountered,
   a negative value is returned. */
int lzma_printf(struct lzmafile *file, const char *format, ...) {
  size_t size = 32768;
  int len;
  unsigned char in[32768];
  va_list va;
  va_start(va, format);

  in[size - 1] = 0;
  (void)vsnprintf((char *)in, size, format, va);
  va_end(va);
  len = strlen((char *)in);

  /* check that printf() results fit in buffer */
  if (len <= 0 || len >= (int)size || in[size - 1] != 0) {
    fprintf(stderr, "lzma_printf error: the printf results do not fit in the buffer\n");
    return -1;
  }
  in[len] = '\0';
  len = lzma_write(file, in, len);

  return len;
}

int lzma_eof(struct lzmafile *file) {
  lzma_stream *lstr;
  lstr = &file->str;
  if (lstr->avail_in == 0) {
    return feof(file->fp);
  } else {
    return 0;
  }
}

/* lzma_tell and lzma_seek will probably have to be 
   changed if they are seriously going to be used.
   As far as I know they will only be called for updating
   an existing file or when using fixed_row_count. 
   Both of which are not allowed when using lzma compression */
long lzma_tell(struct lzmafile *file) {
  return ftell(file->fp);
}

int lzma_seek(struct lzmafile *file, long offset, int whence) {
  return fseek(file->fp, offset, whence);
}

void *UnpackLZMAOpen(char *filename) {
  if (!filename)
    return NULL;
  return lzma_open(filename, "rb");
}
