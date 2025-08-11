/**
 * @file SDDS_binary.c
 * @brief SDDS binary data input and output routines
 *
 * This file contains the implementation of binary file handling functions
 * for the SDDS (Self-Describing Data Sets) library. It includes functions
 * for reading and writing binary data files in the SDDS format.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "SDDS.h"
#include "SDDS_internal.h"
#include "mdb.h"
#include <string.h>
#include <errno.h>

#undef DEBUG

#if defined(_WIN32)
#  include <windows.h>
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif
#if defined(vxWorks)
#  include <time.h>
#endif

#if SDDS_VERSION != 5
#  error "SDDS_VERSION does not match the version number of this file"
#endif

double makeFloat64FromFloat80(unsigned char x[16], int32_t byteOrder);

static int32_t defaultIOBufferSize = SDDS_FILEBUFFER_SIZE;

/**
 * @brief Obsolete routine retained for backward compatibility.
 *
 * This function no longer performs any operations. Use
 * SDDS_SetDefaultIOBufferSize(0) to disable buffering instead.
 *
 * @param dummy Unused parameter kept for API compatibility.
 * @return Always returns 0.
 */
int32_t SDDS_SetBufferedRead(int32_t dummy) {
  return 0;
}

/**
 * Sets the default I/O buffer size used for file operations.
 *
 * This function updates the global `defaultIOBufferSize` variable, which determines the size of the I/O buffer
 * used for file read/write operations. The initial default is `SDDS_FILEBUFFER_SIZE`, which is 262144 bytes.
 *
 * @param newValue The new default I/O buffer size in bytes. If `newValue` is negative, the function returns
 *                 the current buffer size without changing it. If `newValue` is between 0 and 128 (inclusive),
 *                 it is treated as 0, effectively disabling buffering.
 *
 * @return The previous default I/O buffer size if `newValue` is greater than or equal to 0; otherwise,
 *         returns the current default buffer size without changing it.
 */
int32_t SDDS_SetDefaultIOBufferSize(int32_t newValue) {
  int32_t previous;
  if (newValue < 0)
    return defaultIOBufferSize;
  if (newValue < 128) /* arbitrary limit */
    newValue = 0;
  previous = defaultIOBufferSize;
  defaultIOBufferSize = newValue;
  return previous;
}

/**
 * Reads data from a file into a buffer, optimizing performance with buffering.
 *
 * This function reads `targetSize` bytes from the file `fp` into the memory pointed to by `target`.
 * It uses the provided `fBuffer` to buffer file data, improving read performance. If the data type
 * is `SDDS_LONGDOUBLE` and the `long double` precision is not 18 digits, it handles conversion to
 * double precision if the environment variable `SDDS_LONGDOUBLE_64BITS` is not set.
 *
 * If `target` is NULL, the function skips over `targetSize` bytes in the file.
 *
 * @param target Pointer to the memory location where the data will be stored. If NULL, the data is skipped.
 * @param targetSize The number of bytes to read from the file.
 * @param fp The file pointer from which data is read.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure used for buffering file data.
 * @param type The SDDS data type of the data being read (e.g., `SDDS_LONGDOUBLE`).
 * @param byteOrder The byte order of the data (`SDDS_LITTLEENDIAN` or `SDDS_BIGENDIAN`).
 *
 * @return Returns 1 on success; returns 0 on error.
 */
int32_t SDDS_BufferedRead(void *target, int64_t targetSize, FILE *fp, SDDS_FILEBUFFER *fBuffer, int32_t type, int32_t byteOrder) {
  int float80tofloat64 = 0;
  if ((LDBL_DIG != 18) && (type == SDDS_LONGDOUBLE)) {
    if (getenv("SDDS_LONGDOUBLE_64BITS") == NULL) {
      targetSize *= 2;
      float80tofloat64 = 1;
    }
  }
  if (!fBuffer->bufferSize) {
    /* just read into users buffer or seek if no buffer given */
    if (!target)
      return !fseek(fp, (long)targetSize, SEEK_CUR);
    else {
      if (float80tofloat64) {
        unsigned char x[16];
        double d;
        int64_t shift = 0;
        while (shift < targetSize) {
          if (fread(&x, (size_t)1, 16, fp) != 16)
            return 0;
          d = makeFloat64FromFloat80(x, byteOrder);
          memcpy((char *)target + shift, &d, 8);
          shift += 16;
        }
        return 1;
      } else {
        return fread(target, (size_t)1, (size_t)targetSize, fp) == targetSize;
      }
    }
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    /* sufficient data is already in the buffer */
    if (target) {
      if (float80tofloat64) {
        unsigned char x[16];
        double d;
        int64_t shift = 0;
        while (shift < targetSize) {
          memcpy(x, (char *)fBuffer->data + shift, 16);
          d = makeFloat64FromFloat80(x, byteOrder);
          memcpy((char *)target + shift, &d, 8);
          shift += 16;
        }
      } else {
        memcpy((char *)target, (char *)fBuffer->data, targetSize);
      }
    }
    fBuffer->data += targetSize;
    return 1;
  } else {
    /* need to read additional data into buffer */
    int64_t bytesNeeded, offset;
    fBuffer->bytesLeft += targetSize; /* adds back amount subtracted above */

    /* first, use the data that is already available. this cleans out the buffer */
    if ((offset = fBuffer->bytesLeft)) {
      /* some data is available in the buffer */
      if (target) {
        if (float80tofloat64) {
          unsigned char x[16];
          double d;
          int64_t shift = 0;
          while (shift < offset) {
            memcpy(x, (char *)fBuffer->data + shift, 16);
            d = makeFloat64FromFloat80(x, byteOrder);
            memcpy((char *)target + shift, &d, 8);
            shift += 16;
          }
        } else {
          memcpy((char *)target, (char *)fBuffer->data, offset);
        }
      }
      bytesNeeded = targetSize - offset;
      fBuffer->bytesLeft = 0;
    } else {
      bytesNeeded = targetSize;
    }
    fBuffer->data = fBuffer->buffer;

    if (fBuffer->bufferSize < bytesNeeded) {
      /* just read what is needed directly into user's memory or seek */
      if (!target)
        return !fseek(fp, (long)bytesNeeded, SEEK_CUR);
      else {
        if (float80tofloat64) {
          unsigned char x[16];
          double d;
          int64_t shift = 0;
          while (shift < bytesNeeded) {
            if (fread(&x, (size_t)1, 16, fp) != 16)
              return 0;
            d = makeFloat64FromFloat80(x, byteOrder);
            memcpy((char *)target + offset + shift, &d, 8);
            shift += 16;
          }
          return 1;
        } else {
          return fread((char *)target + offset, (size_t)1, (size_t)bytesNeeded, fp) == bytesNeeded;
        }
      }
    }

    /* fill the buffer */
    if ((fBuffer->bytesLeft = fread(fBuffer->data, (size_t)1, (size_t)fBuffer->bufferSize, fp)) < bytesNeeded)
      return 0;
    if (target) {
      if (float80tofloat64) {
        unsigned char x[16];
        double d;
        int64_t shift = 0;
        while (shift < bytesNeeded) {
          memcpy(x, (char *)fBuffer->data + shift, 16);
          d = makeFloat64FromFloat80(x, byteOrder);
          memcpy((char *)target + offset + shift, &d, 8);
          shift += 16;
        }
      } else {
        memcpy((char *)target + offset, (char *)fBuffer->data, bytesNeeded);
      }
    }
    fBuffer->data += bytesNeeded;
    fBuffer->bytesLeft -= bytesNeeded;
    return 1;
  }
}

/**
 * Reads data from an LZMA-compressed file into a buffer, optimizing performance with buffering.
 *
 * This function reads `targetSize` bytes from the LZMA-compressed file `lzmafp` into the memory
 * pointed to by `target`. It uses the provided `fBuffer` to buffer file data, improving read performance.
 * If the data type is `SDDS_LONGDOUBLE` and the `long double` precision is not 18 digits, it handles
 * conversion to double precision if the environment variable `SDDS_LONGDOUBLE_64BITS` is not set.
 *
 * If `target` is NULL, the function skips over `targetSize` bytes in the file.
 *
 * @param target Pointer to the memory location where the data will be stored. If NULL, the data is skipped.
 * @param targetSize The number of bytes to read from the file.
 * @param lzmafp The LZMA file pointer from which data is read.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure used for buffering file data.
 * @param type The SDDS data type of the data being read (e.g., `SDDS_LONGDOUBLE`).
 * @param byteOrder The byte order of the data (`SDDS_LITTLEENDIAN` or `SDDS_BIGENDIAN`).
 *
 * @return Returns 1 on success; returns 0 on error.
 *
 * @note This function requires that `fBuffer->bufferSize` is non-zero. If it is zero, an error is set.
 */
int32_t SDDS_LZMABufferedRead(void *target, int64_t targetSize, struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer, int32_t type, int32_t byteOrder) {
  int float80tofloat64 = 0;
  if (!fBuffer->bufferSize) {
    SDDS_SetError("You must presently have a nonzero file buffer to use LZMA (reading/writing .lzma or .xz files)");
    return 0;
  }
  if ((LDBL_DIG != 18) && (type == SDDS_LONGDOUBLE)) {
    if (getenv("SDDS_LONGDOUBLE_64BITS") == NULL) {
      targetSize *= 2;
      float80tofloat64 = 1;
    }
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    if (target) {
      if (float80tofloat64) {
        unsigned char x[16];
        double d;
        int64_t shift = 0;
        while (shift < targetSize) {
          memcpy(x, (char *)fBuffer->data + shift, 16);
          d = makeFloat64FromFloat80(x, byteOrder);
          memcpy((char *)target + shift, &d, 8);
          shift += 16;
        }
      } else {
        memcpy((char *)target, (char *)fBuffer->data, targetSize);
      }
    }
    fBuffer->data += targetSize;
    return 1;
  } else {
    int64_t bytesNeeded, offset;
    fBuffer->bytesLeft += targetSize;
    if ((offset = fBuffer->bytesLeft)) {
      if (target) {
        if (float80tofloat64) {
          unsigned char x[16];
          double d;
          int64_t shift = 0;
          while (shift < offset) {
            memcpy(x, (char *)fBuffer->data + shift, 16);
            d = makeFloat64FromFloat80(x, byteOrder);
            memcpy((char *)target + shift, &d, 8);
            shift += 16;
          }
        } else {
          memcpy((char *)target, (char *)fBuffer->data, offset);
        }
      }
      bytesNeeded = targetSize - offset;
      fBuffer->bytesLeft = 0;
    } else {
      bytesNeeded = targetSize;
    }
    fBuffer->data = fBuffer->buffer;

    if (fBuffer->bufferSize < bytesNeeded) {
      /* just read what is needed directly into user's memory or seek */
      if (!target)
        return !lzma_seek(lzmafp, (long)bytesNeeded, SEEK_CUR);
      else {
        if (float80tofloat64) {
          unsigned char x[16];
          double d;
          int64_t shift = 0;
          while (shift < bytesNeeded) {
            if (lzma_read(lzmafp, &x, 16) != 16)
              return 0;
            d = makeFloat64FromFloat80(x, byteOrder);
            memcpy((char *)target + offset + shift, &d, 8);
            shift += 16;
          }
          return 1;
        } else {
          return lzma_read(lzmafp, (char *)target + offset, (size_t)bytesNeeded) == bytesNeeded;
        }
      }
    }

    if ((fBuffer->bytesLeft = lzma_read(lzmafp, fBuffer->data, (size_t)fBuffer->bufferSize)) < bytesNeeded)
      return 0;
    if (target) {
      if (float80tofloat64) {
        unsigned char x[16];
        double d;
        int64_t shift = 0;
        while (shift < bytesNeeded) {
          memcpy(x, (char *)fBuffer->data + shift, 16);
          d = makeFloat64FromFloat80(x, byteOrder);
          memcpy((char *)target + offset + shift, &d, 8);
          shift += 16;
        }
      } else {
        memcpy((char *)target + offset, (char *)fBuffer->data, bytesNeeded);
      }
    }
    fBuffer->data += bytesNeeded;
    fBuffer->bytesLeft -= bytesNeeded;
    return 1;
  }
}

#if defined(zLib)
/**
 * Reads data from a GZIP-compressed file into a buffer, optimizing performance with buffering.
 *
 * This function reads `targetSize` bytes from the GZIP-compressed file `gzfp` into the memory
 * pointed to by `target`. It uses the provided `fBuffer` to buffer file data, improving read performance.
 * If the data type is `SDDS_LONGDOUBLE` and the `long double` precision is not 18 digits, it handles
 * conversion to double precision if the environment variable `SDDS_LONGDOUBLE_64BITS` is not set.
 *
 * If `target` is NULL, the function skips over `targetSize` bytes in the file.
 *
 * @param target Pointer to the memory location where the data will be stored. If NULL, the data is skipped.
 * @param targetSize The number of bytes to read from the file.
 * @param gzfp The GZIP file pointer from which data is read.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure used for buffering file data.
 * @param type The SDDS data type of the data being read (e.g., `SDDS_LONGDOUBLE`).
 * @param byteOrder The byte order of the data (`SDDS_LITTLEENDIAN` or `SDDS_BIGENDIAN`).
 *
 * @return Returns 1 on success; returns 0 on error.
 *
 * @note This function requires that `fBuffer->bufferSize` is non-zero. If it is zero, an error is set.
 */
int32_t SDDS_GZipBufferedRead(void *target, int64_t targetSize, gzFile gzfp, SDDS_FILEBUFFER *fBuffer, int32_t type, int32_t byteOrder) {
  int float80tofloat64 = 0;
  if (!fBuffer->bufferSize) {
    SDDS_SetError("You must presently have a nonzero file buffer to use zLib (reading/writing .gz files)");
    return 0;
  }
  if ((LDBL_DIG != 18) && (type == SDDS_LONGDOUBLE)) {
    if (getenv("SDDS_LONGDOUBLE_64BITS") == NULL) {
      targetSize *= 2;
      float80tofloat64 = 1;
    }
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    if (target) {
      if (float80tofloat64) {
        unsigned char x[16];
        double d;
        int64_t shift = 0;
        while (shift < targetSize) {
          memcpy(x, (char *)fBuffer->data + shift, 16);
          d = makeFloat64FromFloat80(x, byteOrder);
          memcpy((char *)target + shift, &d, 8);
          shift += 16;
        }
      } else {
        memcpy((char *)target, (char *)fBuffer->data, targetSize);
      }
    }
    fBuffer->data += targetSize;
    return 1;
  } else {
    int64_t bytesNeeded, offset;
    fBuffer->bytesLeft += targetSize;
    if ((offset = fBuffer->bytesLeft)) {
      if (target) {
        if (float80tofloat64) {
          unsigned char x[16];
          double d;
          int64_t shift = 0;
          while (shift < offset) {
            memcpy(x, (char *)fBuffer->data + shift, 16);
            d = makeFloat64FromFloat80(x, byteOrder);
            memcpy((char *)target + shift, &d, 8);
            shift += 16;
          }
        } else {
          memcpy((char *)target, (char *)fBuffer->data, offset);
        }
      }
      bytesNeeded = targetSize - offset;
      fBuffer->bytesLeft = 0;
    } else {
      bytesNeeded = targetSize;
    }
    fBuffer->data = fBuffer->buffer;

    if (fBuffer->bufferSize < bytesNeeded) {
      /* just read what is needed directly into user's memory or seek */
      if (!target)
        return !gzseek(gzfp, bytesNeeded, SEEK_CUR);
      else {
        if (float80tofloat64) {
          unsigned char x[16];
          double d;
          int64_t shift = 0;
          while (shift < bytesNeeded) {
            if (gzread(gzfp, &x, 16) != 16)
              return 0;
            d = makeFloat64FromFloat80(x, byteOrder);
            memcpy((char *)target + offset + shift, &d, 8);
            shift += 16;
          }
          return 1;
        } else {
          return gzread(gzfp, (char *)target + offset, bytesNeeded) == bytesNeeded;
        }
      }
    }

    if ((fBuffer->bytesLeft = gzread(gzfp, fBuffer->data, fBuffer->bufferSize)) < bytesNeeded)
      return 0;
    if (target) {
      if (float80tofloat64) {
        unsigned char x[16];
        double d;
        int64_t shift = 0;
        while (shift < bytesNeeded) {
          memcpy(x, (char *)fBuffer->data + shift, 16);
          d = makeFloat64FromFloat80(x, byteOrder);
          memcpy((char *)target + offset + shift, &d, 8);
          shift += 16;
        }
      } else {
        memcpy((char *)target + offset, (char *)fBuffer->data, bytesNeeded);
      }
    }
    fBuffer->data += bytesNeeded;
    fBuffer->bytesLeft -= bytesNeeded;
    return 1;
  }
}
#endif

/**
 * Writes data to a file using a buffer to optimize performance.
 *
 * This function writes `targetSize` bytes from the memory pointed to by `target` to the file `fp`.
 * It uses the provided `fBuffer` to buffer file data, improving write performance. If the buffer
 * is full, it flushes the buffer to the file before writing more data.
 *
 * @param target Pointer to the memory location of the data to write.
 * @param targetSize The number of bytes to write to the file.
 * @param fp The file pointer to which data is written.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure used for buffering file data.
 *
 * @return Returns 1 on success; returns 0 on error.
 */
int32_t SDDS_BufferedWrite(void *target, int64_t targetSize, FILE *fp, SDDS_FILEBUFFER *fBuffer) {
  if (!fBuffer->bufferSize) {
    return fwrite(target, (size_t)1, (size_t)targetSize, fp) == targetSize;
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    memcpy((char *)fBuffer->data, (char *)target, targetSize);
    fBuffer->data += targetSize;
#ifdef DEBUG
    fprintf(stderr, "SDDS_BufferedWrite of %" PRId64 " bytes done in-memory, %" PRId64 " bytes left\n", targetSize, fBuffer->bytesLeft);
#endif
    return 1;
  } else {
    int64_t lastLeft;
    /* add back what was subtracted in test above.
     * lastLeft is the number of bytes left in the buffer before doing anything
     * and also the number of bytes from the users data that get copied into the buffer.
     */
    lastLeft = (fBuffer->bytesLeft += targetSize);
    /* copy part of the data into the buffer and write the buffer out */
    memcpy((char *)fBuffer->data, (char *)target, (size_t)fBuffer->bytesLeft);
    if (fwrite(fBuffer->buffer, (size_t)1, (size_t)fBuffer->bufferSize, fp) != fBuffer->bufferSize)
      return 0;
    if (fflush(fp)) {
      SDDS_SetError("Problem flushing file (SDDS_BufferedWrite)");
      SDDS_SetError(strerror(errno));
      return 0;
    }
    /* reset the data pointer and the bytesLeft value.
     * also, determine if the remaining data is too large for the buffer.
     * if so, just write it out.
     */
    fBuffer->data = fBuffer->buffer;
    if ((targetSize -= lastLeft) > (fBuffer->bytesLeft = fBuffer->bufferSize)) {
      return fwrite((char *)target + lastLeft, (size_t)1, (size_t)targetSize, fp) == targetSize;
    }
    /* copy remaining data into the buffer.
     * could do this with a recursive call, but this is more efficient.
     */
    memcpy((char *)fBuffer->data, (char *)target + lastLeft, targetSize);
    fBuffer->data += targetSize;
    fBuffer->bytesLeft -= targetSize;
    return 1;
  }
}

/**
 * Writes data to an LZMA-compressed file using a buffer to optimize performance.
 *
 * This function writes `targetSize` bytes from the memory pointed to by `target` to the LZMA-compressed
 * file referenced by `lzmafp`. It uses the provided `fBuffer` to buffer data before writing to the file,
 * which can improve write performance by reducing the number of write operations.
 *
 * If there is enough space in the buffer (`fBuffer`), the data is copied into the buffer. If the buffer
 * does not have enough space to hold the data, the buffer is flushed to the file, and the function
 * recursively calls itself to handle the remaining data.
 *
 * @param target Pointer to the memory location of the data to write.
 * @param targetSize The number of bytes to write to the file.
 * @param lzmafp The LZMA file pointer to which data is written.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure used for buffering data.
 *
 * @return Returns 1 on success; returns 0 on error.
 *
 * @note This function requires that `fBuffer->bufferSize` is non-zero. If it is zero, the function
 *       sets an error message and returns 0.
 */
int32_t SDDS_LZMABufferedWrite(void *target, int64_t targetSize, struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer) {
  if (!fBuffer->bufferSize) {
    SDDS_SetError("You must presently have a nonzero file buffer to use lzma (reading/writing .xz files)");
    return 0;
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    memcpy((char *)fBuffer->data, (char *)target, targetSize);
    fBuffer->data += targetSize;
    return 1;
  } else {
    int64_t lastLeft;
    lastLeft = (fBuffer->bytesLeft += targetSize);
    memcpy((char *)fBuffer->data, (char *)target, (size_t)fBuffer->bytesLeft);
    if (lzma_write(lzmafp, fBuffer->buffer, (size_t)fBuffer->bufferSize) != fBuffer->bufferSize)
      return 0;
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
    return SDDS_LZMABufferedWrite((char *)target + lastLeft, targetSize - lastLeft, lzmafp, fBuffer);
  }
}

#if defined(zLib)
/**
 * Writes data to a GZIP-compressed file using a buffer to optimize performance.
 *
 * This function writes `targetSize` bytes from the memory pointed to by `target` to the GZIP-compressed
 * file referenced by `gzfp`. It uses the provided `fBuffer` to buffer data before writing to the file,
 * which can improve write performance by reducing the number of write operations.
 *
 * If there is enough space in the buffer (`fBuffer`), the data is copied into the buffer. If the buffer
 * does not have enough space to hold the data, the buffer is flushed to the file, and the function
 * recursively calls itself to handle the remaining data.
 *
 * @param target Pointer to the memory location of the data to write.
 * @param targetSize The number of bytes to write to the file.
 * @param gzfp The GZIP file pointer to which data is written.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure used for buffering data.
 *
 * @return Returns 1 on success; returns 0 on error.
 *
 * @note This function requires that `fBuffer->bufferSize` is non-zero. If it is zero, the function
 *       sets an error message and returns 0.
 */
int32_t SDDS_GZipBufferedWrite(void *target, int64_t targetSize, gzFile gzfp, SDDS_FILEBUFFER *fBuffer) {
  if (!fBuffer->bufferSize) {
    SDDS_SetError("You must presently have a nonzero file buffer to use zLib (reading/writing .gz files}");
    return 0;
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    memcpy((char *)fBuffer->data, (char *)target, targetSize);
    fBuffer->data += targetSize;
    return 1;
  } else {
    int64_t lastLeft;
    lastLeft = (fBuffer->bytesLeft + targetSize);
    memcpy((char *)fBuffer->data, (char *)target, lastLeft);
    if (gzwrite(gzfp, fBuffer->buffer, fBuffer->bufferSize) != fBuffer->bufferSize)
      return 0;
    /*gzflush(gzfp, Z_FULL_FLUSH); */
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
    return SDDS_GZipBufferedWrite((char *)target + lastLeft, targetSize - lastLeft, gzfp, fBuffer);
  }
}
#endif

/**
 * Flushes the buffered data to a file to ensure all data is written.
 *
 * This function writes any remaining data in the buffer (`fBuffer`) to the file pointed to by `fp`. If the
 * buffer contains data, it writes the data to the file, resets the buffer, and flushes the file's output
 * buffer using `fflush`. This ensures that all buffered data is physically written to the file.
 *
 * @param fp The file pointer to which buffered data will be written.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure containing the buffered data.
 *
 * @return Returns 1 on success; returns 0 on error.
 *
 * @note If `fBuffer->bufferSize` is zero, the function will only call `fflush(fp)`.
 *
 * @warning If `fp` or `fBuffer` is `NULL`, the function sets an error message and returns 0.
 */
int32_t SDDS_FlushBuffer(FILE *fp, SDDS_FILEBUFFER *fBuffer) {
  int64_t writeBytes;
  if (!fp) {
    SDDS_SetError("Unable to flush buffer: file pointer is NULL. (SDDS_FlushBuffer)");
    return 0;
  }
  if (!fBuffer->bufferSize) {
    if (fflush(fp)) {
      SDDS_SetError("Problem flushing file (SDDS_FlushBuffer.1)");
      SDDS_SetError(strerror(errno));
      return 0;
    }
    return 1;
  }
  if (!fBuffer) {
    SDDS_SetError("Unable to flush buffer: buffer pointer is NULL. (SDDS_FlushBuffer)");
    return 0;
  }
  if ((writeBytes = fBuffer->bufferSize - fBuffer->bytesLeft)) {
    if (writeBytes < 0) {
      SDDS_SetError("Unable to flush buffer: negative byte count (SDDS_FlushBuffer).");
      return 0;
    }
#ifdef DEBUG
    fprintf(stderr, "Writing %" PRId64 " bytes to disk\n", writeBytes);
#endif
    if (fwrite(fBuffer->buffer, 1, writeBytes, fp) != writeBytes) {
      SDDS_SetError("Unable to flush buffer: write operation failed (SDDS_FlushBuffer).");
      return 0;
    }
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
  }
  if (fflush(fp)) {
    SDDS_SetError("Problem flushing file (SDDS_FlushBuffer.2)");
    SDDS_SetError(strerror(errno));
    return 0;
  }
  return 1;
}

/**
 * Flushes the buffered data to an LZMA-compressed file to ensure all data is written.
 *
 * This function writes any remaining data in the buffer (`fBuffer`) to the LZMA-compressed file pointed
 * to by `lzmafp`. If the buffer contains data, it writes the data to the file, resets the buffer,
 * ensuring that all buffered data is physically written to the file.
 *
 * @param lzmafp The LZMA file pointer to which buffered data will be written.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure containing the buffered data.
 *
 * @return Returns 1 on success; returns 0 on error.
 *
 * @note This function assumes that `fBuffer->bufferSize` is non-zero and `fBuffer` is properly initialized.
 */
int32_t SDDS_LZMAFlushBuffer(struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer) {
  int32_t writeBytes;
  if ((writeBytes = fBuffer->bufferSize - fBuffer->bytesLeft)) {
    if (lzma_write(lzmafp, fBuffer->buffer, writeBytes) != writeBytes)
      return 0;
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
  }
  return 1;
}

#if defined(zLib)
/**
 * Flushes the buffered data to a GZIP-compressed file to ensure all data is written.
 *
 * This function writes any remaining data in the buffer (`fBuffer`) to the GZIP-compressed file pointed
 * to by `gzfp`. If the buffer contains data, it writes the data to the file, resets the buffer,
 * ensuring that all buffered data is physically written to the file.
 *
 * @param gzfp The GZIP file pointer to which buffered data will be written.
 * @param fBuffer Pointer to an `SDDS_FILEBUFFER` structure containing the buffered data.
 *
 * @return Returns 1 on success; returns 0 on error.
 *
 * @note This function assumes that `fBuffer->bufferSize` is non-zero and `fBuffer` is properly initialized.
 */
int32_t SDDS_GZipFlushBuffer(gzFile gzfp, SDDS_FILEBUFFER *fBuffer) {
  int32_t writeBytes;
  if ((writeBytes = fBuffer->bufferSize - fBuffer->bytesLeft)) {
    if (gzwrite(gzfp, fBuffer->buffer, writeBytes) != writeBytes)
      return 0;
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
  }
  /*gzflush(gzfp, Z_FULL_FLUSH); */
  return 1;
}
#endif

/**
 * Writes a binary page of data to an SDDS dataset, handling compression and buffering.
 *
 * This function writes a binary page (including parameters, arrays, and row data) to the SDDS dataset
 * pointed to by `SDDS_dataset`. It handles different file types, including regular files, LZMA-compressed
 * files, and GZIP-compressed files, and uses buffering to improve write performance.
 *
 * The function performs the following steps:
 * - Checks for output endianess and writes a non-native binary page if needed.
 * - Determines the number of rows to write and calculates any fixed row counts.
 * - Writes the number of rows to the file.
 * - Writes parameters, arrays, and column data using the appropriate write functions.
 * - Flushes the buffer to ensure all data is written.
 *
 * It uses the appropriate buffered write functions (`SDDS_BufferedWrite`, `SDDS_LZMABufferedWrite`, or
 * `SDDS_GZipBufferedWrite`) depending on the file type.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the SDDS dataset to write to.
 *
 * @return Returns 1 on success; returns 0 on error.
 *
 * @note The function sets error messages using `SDDS_SetError` if any step fails.
 */
int32_t SDDS_WriteBinaryPage(SDDS_DATASET *SDDS_dataset) {
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  int64_t i, rows, fixed_rows;
  int32_t min32 = INT32_MIN, rows32;
  /*  static char buffer[SDDS_MAXLINE]; */
  SDDS_FILEBUFFER *fBuffer;
  char *outputEndianess = NULL;

  if ((outputEndianess = getenv("SDDS_OUTPUT_ENDIANESS"))) {
    if (((strncmp(outputEndianess, "big", 3) == 0) && (SDDS_IsBigEndianMachine() == 0)) || ((strncmp(outputEndianess, "little", 6) == 0) && (SDDS_IsBigEndianMachine() == 1)))
      return SDDS_WriteNonNativeBinaryPage(SDDS_dataset);
  }

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteBinaryPage"))
    return (0);

#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!(gzfp = SDDS_dataset->layout.gzfp)) {
      SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteBinaryPage)");
      return (0);
    }
    fBuffer = &SDDS_dataset->fBuffer;

    if (!fBuffer->buffer) {
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (defaultIOBufferSize + 1)))) {
        SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_WriteBinaryPage)");
        return 0;
      }
      fBuffer->bufferSize = defaultIOBufferSize;
      fBuffer->bytesLeft = defaultIOBufferSize;
    }

    rows = SDDS_CountRowsOfInterest(SDDS_dataset);
    SDDS_dataset->rowcount_offset = gztell(gzfp);
    if (SDDS_dataset->layout.data_mode.fixed_row_count) {
      fixed_rows = ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment;
      if (fixed_rows > INT32_MAX) {
        if (!SDDS_GZipBufferedWrite(&min32, sizeof(min32), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
          return (0);
        }
        if (!SDDS_GZipBufferedWrite(&fixed_rows, sizeof(fixed_rows), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
          return (0);
        }
      } else {
        rows32 = (int32_t)fixed_rows;
        if (!SDDS_GZipBufferedWrite(&rows32, sizeof(rows32), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
          return (0);
        }
      }
    } else {
      if (rows > INT32_MAX) {
        if (!SDDS_GZipBufferedWrite(&min32, sizeof(min32), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
          return (0);
        }
        if (!SDDS_GZipBufferedWrite(&rows, sizeof(rows), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
          return (0);
        }
      } else {
        rows32 = (int32_t)rows;
        if (!SDDS_GZipBufferedWrite(&rows32, sizeof(rows32), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
          return (0);
        }
      }
    }
    if (!SDDS_WriteBinaryParameters(SDDS_dataset)) {
      SDDS_SetError("Unable to write page--parameter writing problem (SDDS_WriteBinaryPage)");
      return 0;
    }
    if (!SDDS_WriteBinaryArrays(SDDS_dataset)) {
      SDDS_SetError("Unable to write page--array writing problem (SDDS_WriteBinaryPage)");
      return 0;
    }
    if (SDDS_dataset->layout.n_columns) {
      if (SDDS_dataset->layout.data_mode.column_major) {
        if (!SDDS_WriteBinaryColumns(SDDS_dataset)) {
          SDDS_SetError("Unable to write page--column writing problem (SDDS_WriteBinaryPage)");
          return 0;
        }
      } else {
        for (i = 0; i < SDDS_dataset->n_rows; i++) {
          if (SDDS_dataset->row_flag[i] && !SDDS_WriteBinaryRow(SDDS_dataset, i)) {
            SDDS_SetError("Unable to write page--row writing problem (SDDS_WriteBinaryPage)");
            return 0;
          }
        }
      }
    }
    if (!SDDS_GZipFlushBuffer(gzfp, fBuffer)) {
      SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteBinaryPage)");
      return 0;
    }
    SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
    SDDS_dataset->n_rows_written = rows;
    SDDS_dataset->writing_page = 1;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!(lzmafp = SDDS_dataset->layout.lzmafp)) {
        SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteBinaryPage)");
        return (0);
      }
      fBuffer = &SDDS_dataset->fBuffer;

      if (!fBuffer->buffer) {
        if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (defaultIOBufferSize + 1)))) {
          SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_WriteBinaryPage)");
          return 0;
        }
        fBuffer->bufferSize = defaultIOBufferSize;
        fBuffer->bytesLeft = defaultIOBufferSize;
      }
      rows = SDDS_CountRowsOfInterest(SDDS_dataset);
      SDDS_dataset->rowcount_offset = lzma_tell(lzmafp);
      if (SDDS_dataset->layout.data_mode.fixed_row_count) {
        fixed_rows = ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment;
        if (fixed_rows > INT32_MAX) {
          if (!SDDS_LZMABufferedWrite(&min32, sizeof(min32), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
          if (!SDDS_LZMABufferedWrite(&fixed_rows, sizeof(fixed_rows), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
        } else {
          rows32 = (int32_t)fixed_rows;
          if (!SDDS_LZMABufferedWrite(&rows32, sizeof(rows32), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
        }
      } else {
        if (rows > INT32_MAX) {
          if (!SDDS_LZMABufferedWrite(&min32, sizeof(min32), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
          if (!SDDS_LZMABufferedWrite(&rows, sizeof(rows), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
        } else {
          rows32 = (int32_t)rows;
          if (!SDDS_LZMABufferedWrite(&rows32, sizeof(rows32), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
        }
      }
      if (!SDDS_WriteBinaryParameters(SDDS_dataset)) {
        SDDS_SetError("Unable to write page--parameter writing problem (SDDS_WriteBinaryPage)");
        return 0;
      }
      if (!SDDS_WriteBinaryArrays(SDDS_dataset)) {
        SDDS_SetError("Unable to write page--array writing problem (SDDS_WriteBinaryPage)");
        return 0;
      }
      if (SDDS_dataset->layout.n_columns) {
        if (SDDS_dataset->layout.data_mode.column_major) {
          if (!SDDS_WriteBinaryColumns(SDDS_dataset)) {
            SDDS_SetError("Unable to write page--column writing problem (SDDS_WriteBinaryPage)");
            return 0;
          }
        } else {
          for (i = 0; i < SDDS_dataset->n_rows; i++) {
            if (SDDS_dataset->row_flag[i] && !SDDS_WriteBinaryRow(SDDS_dataset, i)) {
              SDDS_SetError("Unable to write page--row writing problem (SDDS_WriteBinaryPage)");
              return 0;
            }
          }
        }
      }
      if (!SDDS_LZMAFlushBuffer(lzmafp, fBuffer)) {
        SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteBinaryPage)");
        return 0;
      }
      SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
      SDDS_dataset->n_rows_written = rows;
      SDDS_dataset->writing_page = 1;
    } else {
      if (!(fp = SDDS_dataset->layout.fp)) {
        SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteBinaryPage)");
        return (0);
      }
      fBuffer = &SDDS_dataset->fBuffer;

      if (!fBuffer->buffer) {
        if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (defaultIOBufferSize + 1)))) {
          SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_WriteBinaryPage)");
          return 0;
        }
        fBuffer->bufferSize = defaultIOBufferSize;
        fBuffer->bytesLeft = defaultIOBufferSize;
      }

      /* Flush any existing data in the output buffer so we can determine the
       * row count offset for the file.  This is probably unnecessary.
       */
      if (!SDDS_FlushBuffer(fp, fBuffer)) {
        SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteBinaryPage)");
        return 0;
      }

      /* output the row count and determine its byte offset in the file */
      rows = SDDS_CountRowsOfInterest(SDDS_dataset);
      SDDS_dataset->rowcount_offset = ftell(fp);
      if (SDDS_dataset->layout.data_mode.fixed_row_count) {
        fixed_rows = ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment;
#if defined(DEBUG)
        fprintf(stderr, "setting %" PRId64 " fixed rows\n", fixed_rows);
#endif
        if (fixed_rows > INT32_MAX) {
          if (!SDDS_BufferedWrite(&min32, sizeof(min32), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
          if (!SDDS_BufferedWrite(&fixed_rows, sizeof(fixed_rows), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
        } else {
          rows32 = (int32_t)fixed_rows;
          if (!SDDS_BufferedWrite(&rows32, sizeof(rows32), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
        }
      } else {
#if defined(DEBUG)
        fprintf(stderr, "setting %" PRId64 " rows\n", rows);
#endif
        if (rows > INT32_MAX) {
          if (!SDDS_BufferedWrite(&min32, sizeof(min32), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
          if (!SDDS_BufferedWrite(&rows, sizeof(rows), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
        } else {
          rows32 = (int32_t)rows;
          if (!SDDS_BufferedWrite(&rows32, sizeof(rows32), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteBinaryPage)");
            return (0);
          }
        }
      }

      /* write the data, using buffered I/O */
      if (!SDDS_WriteBinaryParameters(SDDS_dataset)) {
        SDDS_SetError("Unable to write page--parameter writing problem (SDDS_WriteBinaryPage)");
        return 0;
      }
      if (!SDDS_WriteBinaryArrays(SDDS_dataset)) {
        SDDS_SetError("Unable to write page--array writing problem (SDDS_WriteBinaryPage)");
        return 0;
      }
      if (SDDS_dataset->layout.n_columns) {
        if (SDDS_dataset->layout.data_mode.column_major) {
          if (!SDDS_WriteBinaryColumns(SDDS_dataset)) {
            SDDS_SetError("Unable to write page--column writing problem (SDDS_WriteBinaryPage)");
            return 0;
          }
        } else {
          for (i = 0; i < SDDS_dataset->n_rows; i++) {
            if (SDDS_dataset->row_flag[i] && !SDDS_WriteBinaryRow(SDDS_dataset, i)) {
              SDDS_SetError("Unable to write page--row writing problem (SDDS_WriteBinaryPage)");
              return 0;
            }
          }
        }
      }
      /* flush the page */
      if (!SDDS_FlushBuffer(fp, fBuffer)) {
        SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteBinaryPage)");
        return 0;
      }
      SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
      SDDS_dataset->n_rows_written = rows;
      SDDS_dataset->writing_page = 1;
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Updates the binary page of an SDDS dataset.
 *
 * This function updates the binary page of the specified SDDS dataset based on the provided mode.
 * It handles writing the dataset's binary data to the associated file, managing buffering, and
 * handling different file formats such as gzip and LZMA if applicable.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset to update.
 * @param mode Bitmask indicating the update mode. It can be:
 *             - `0` for a standard update.
 *             - `FLUSH_TABLE` to flush the table after updating.
 *
 * @return 
 *   - Returns `1` on successful update.
 *   - Returns `0` if an error occurs during the update process.
 *
 * @details
 * The function performs several checks before updating:
 * - Checks the environment variable `SDDS_OUTPUT_ENDIANESS` to determine if a non-native
 *   binary update is required.
 * - Validates the dataset structure.
 * - Ensures that the dataset is not using gzip or LZMA compression, or is not in column-major
 *   data mode.
 * - Handles writing the binary page, updating row counts, and managing buffer flushing.
 *
 * @note 
 * - The function is not thread-safe and should be called in a synchronized context.
 * - Requires that the dataset has been properly initialized and populated with data.
 */
int32_t SDDS_UpdateBinaryPage(SDDS_DATASET *SDDS_dataset, uint32_t mode) {
  FILE *fp;
  int64_t i, rows, offset, code, fixed_rows;
  int32_t min32 = INT32_MIN, rows32;
  SDDS_FILEBUFFER *fBuffer;
  char *outputEndianess = NULL;

  if ((outputEndianess = getenv("SDDS_OUTPUT_ENDIANESS"))) {
    if (((strncmp(outputEndianess, "big", 3) == 0) && (SDDS_IsBigEndianMachine() == 0)) || ((strncmp(outputEndianess, "little", 6) == 0) && (SDDS_IsBigEndianMachine() == 1)))
      return SDDS_UpdateNonNativeBinaryPage(SDDS_dataset, mode);
  }

#ifdef DEBUG
  fprintf(stderr, "%" PRId64 " virtual rows present, first=%" PRId64 "\n", SDDS_CountRowsOfInterest(SDDS_dataset), SDDS_dataset->first_row_in_mem);
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_UpdateBinaryPage"))
    return (0);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    SDDS_SetError("Unable to perform page updates on a gzip file (SDDS_UpdateBinaryPage)");
    return 0;
  }
#endif
  if (SDDS_dataset->layout.lzmaFile) {
    SDDS_SetError("Unable to perform page updates on an .lzma or .xz file (SDDS_UpdateBinaryPage)");
    return 0;
  }
  if (SDDS_dataset->layout.data_mode.column_major) {
    SDDS_SetError("Unable to perform page updates on column major order file. (SDDS_UpdateBinaryPage)");
    return 0;
  }
  if (!SDDS_dataset->writing_page) {
#ifdef DEBUG
    fprintf(stderr, "Page not being written---calling SDDS_UpdateBinaryPage\n");
#endif
    if (!(code = SDDS_WriteBinaryPage(SDDS_dataset)))
      return 0;
    if (mode & FLUSH_TABLE) {
      SDDS_FreeTableStrings(SDDS_dataset);
      SDDS_dataset->first_row_in_mem = SDDS_CountRowsOfInterest(SDDS_dataset);
      SDDS_dataset->last_row_written = -1;
      SDDS_dataset->n_rows = 0;
    }
    return code;
  }

  if (!(fp = SDDS_dataset->layout.fp)) {
    SDDS_SetError("Unable to update page--file pointer is NULL (SDDS_UpdateBinaryPage)");
    return (0);
  }
  fBuffer = &SDDS_dataset->fBuffer;
  if (!SDDS_FlushBuffer(fp, fBuffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_UpdateBinaryPage)");
    return 0;
  }
  offset = ftell(fp);

  rows = SDDS_CountRowsOfInterest(SDDS_dataset) + SDDS_dataset->first_row_in_mem;
#ifdef DEBUG
  fprintf(stderr, "%" PRId64 " rows stored in table, %" PRId64 " already written\n", rows, SDDS_dataset->n_rows_written);
#endif
  if (rows == SDDS_dataset->n_rows_written)
    return (1);
  if (rows < SDDS_dataset->n_rows_written) {
    SDDS_SetError("Unable to update page--new number of rows less than previous number (SDDS_UpdateBinaryPage)");
    return (0);
  }
  if ((!SDDS_dataset->layout.data_mode.fixed_row_count) || (((rows + rows - SDDS_dataset->n_rows_written) / SDDS_dataset->layout.data_mode.fixed_row_increment) != (rows / SDDS_dataset->layout.data_mode.fixed_row_increment))) {
    if (SDDS_fseek(fp, SDDS_dataset->rowcount_offset, 0) == -1) {
      SDDS_SetError("Unable to update page--failure doing fseek (SDDS_UpdateBinaryPage)");
      return (0);
    }
    if (SDDS_dataset->layout.data_mode.fixed_row_count) {
      if ((rows - SDDS_dataset->n_rows_written) + 1 > SDDS_dataset->layout.data_mode.fixed_row_increment) {
        SDDS_dataset->layout.data_mode.fixed_row_increment = (rows - SDDS_dataset->n_rows_written) + 1;
      }
      fixed_rows = ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment;
#if defined(DEBUG)
      fprintf(stderr, "Setting %" PRId64 " fixed rows\n", fixed_rows);
#endif
      if ((fixed_rows > INT32_MAX) && (SDDS_dataset->n_rows_written <= INT32_MAX)) {
        SDDS_SetError("Unable to update page--crossed the INT32_MAX row boundary (SDDS_UpdateBinaryPage)");
        return (0);
      }
      if (fixed_rows > INT32_MAX) {
        if (fwrite(&min32, sizeof(min32), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateBinaryPage)");
          return (0);
        }
        if (fwrite(&fixed_rows, sizeof(fixed_rows), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateBinaryPage)");
          return (0);
        }
      } else {
        rows32 = (int32_t)fixed_rows;
        if (fwrite(&fixed_rows, sizeof(rows32), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateBinaryPage)");
          return (0);
        }
      }
    } else {
#if defined(DEBUG)
      fprintf(stderr, "Setting %" PRId64 " rows\n", rows);
#endif
      if ((rows > INT32_MAX) && (SDDS_dataset->n_rows_written <= INT32_MAX)) {
        SDDS_SetError("Unable to update page--crossed the INT32_MAX row boundary (SDDS_UpdateBinaryPage)");
        return (0);
      }
      if (rows > INT32_MAX) {
        if (fwrite(&min32, sizeof(min32), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateBinaryPage)");
          return (0);
        }
        if (fwrite(&rows, sizeof(rows), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateBinaryPage)");
          return (0);
        }
      } else {
        rows32 = (int32_t)rows;
        if (fwrite(&rows32, sizeof(rows32), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateBinaryPage)");
          return (0);
        }
      }
    }
    if (SDDS_fseek(fp, offset, 0) == -1) {
      SDDS_SetError("Unable to update page--failure doing fseek to end of page (SDDS_UpdateBinaryPage)");
      return (0);
    }
  }
  for (i = SDDS_dataset->last_row_written + 1; i < SDDS_dataset->n_rows; i++)
    if (SDDS_dataset->row_flag[i] && !SDDS_WriteBinaryRow(SDDS_dataset, i)) {
      SDDS_SetError("Unable to update page--failure writing row (SDDS_UpdateBinaryPage)");
      return (0);
    }
#ifdef DEBUG
  fprintf(stderr, "Flushing buffer\n");
#endif
  if (!SDDS_FlushBuffer(fp, fBuffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_UpdateBinaryPage)");
    return 0;
  }
  SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
  SDDS_dataset->n_rows_written = rows;
  if (mode & FLUSH_TABLE) {
    SDDS_FreeTableStrings(SDDS_dataset);
    SDDS_dataset->first_row_in_mem = rows;
    SDDS_dataset->last_row_written = -1;
    SDDS_dataset->n_rows = 0;
  }
  return (1);
}

#define FSEEK_TRIES 10
/**
 * @brief Sets the file position indicator for a given file stream with retry logic.
 *
 * Attempts to set the file position indicator for the specified file stream (`fp`) to a new position
 * defined by `offset` and `dir`. The function retries the `fseek` operation up to `FSEEK_TRIES`
 * times in case of transient failures, implementing a delay between attempts.
 *
 * @param fp     Pointer to the `FILE` stream whose position indicator is to be set.
 * @param offset Number of bytes to offset from the position specified by `dir`.
 * @param dir    Positioning directive, which can be one of:
 *               - `SEEK_SET` to set the position relative to the beginning of the file,
 *               - `SEEK_CUR` to set the position relative to the current position,
 *               - `SEEK_END` to set the position relative to the end of the file.
 *
 * @return 
 *   - Returns `0` if the operation is successful.
 *   - Returns `-1` if all retry attempts fail to set the file position.
 *
 * @details
 * The function attempts to set the file position using `fseek`. If `fseek` fails, it sleeps for 1 second
 * (or 1 second using `nanosleep` on vxWorks systems) before retrying. After `FSEEK_TRIES` unsuccessful
 * attempts, it reports a warning and returns `-1`.
 *
 * @note 
 * - The function is designed to handle temporary file access issues by retrying the `fseek` operation.
 * - It is not suitable for non-recoverable `fseek` errors, which will cause it to fail after retries.
 */
int32_t SDDS_fseek(FILE *fp, int64_t offset, int32_t dir) {
  int32_t try;
#if defined(vxWorks)
  struct timespec rqtp;
  rqtp.tv_sec = 1;
  rqtp.tv_nsec = 0;
#endif
  for (try = 0; try < FSEEK_TRIES; try++) {
    if (fseek(fp, offset, dir) == -1) {
#if defined(vxWorks)
      nanosleep(&rqtp, NULL);
#else
      sleep(1);
#endif
    } else
      break;
  }
  if (try == 0)
    return 0;
  if (try == FSEEK_TRIES) {
    fputs("warning: fseek problems--unable to recover\n", stderr);
    return -1;
  }
  fputs("warning: fseek problems--recovered\n", stderr);
  return 0;
}

/**
 * @brief Sets the file position indicator for a given LZMA file stream with retry logic.
 *
 * Attempts to set the file position indicator for the specified LZMA file stream (`lzmafp`) to a new position
 * defined by `offset` and `dir`. The function retries the `lzma_seek` operation up to `FSEEK_TRIES`
 * times in case of transient failures, implementing a delay between attempts.
 *
 * @param lzmafp Pointer to the `lzmafile` stream whose position indicator is to be set.
 * @param offset Number of bytes to offset from the position specified by `dir`.
 * @param dir    Positioning directive, which can be one of:
 *               - `SEEK_SET` to set the position relative to the beginning of the file,
 *               - `SEEK_CUR` to set the position relative to the current position,
 *               - `SEEK_END` to set the position relative to the end of the file.
 *
 * @return 
 *   - Returns `0` if the operation is successful.
 *   - Returns `-1` if all retry attempts fail to set the file position.
 *
 * @details
 * The function attempts to set the file position using `lzma_seek`. If `lzma_seek` fails, it sleeps for 1 second
 * (or 1 second using `nanosleep` on vxWorks systems) before retrying. After `FSEEK_TRIES` unsuccessful
 * attempts, it reports a warning and returns `-1`.
 *
 * @note 
 * - The function is designed to handle temporary file access issues by retrying the `lzma_seek` operation.
 * - It is not suitable for non-recoverable `lzma_seek` errors, which will cause it to fail after retries.
 */
int32_t SDDS_lzmaseek(struct lzmafile *lzmafp, int64_t offset, int32_t dir) {
  int32_t try;
#if defined(vxWorks)
  struct timespec rqtp;
  rqtp.tv_sec = 1;
  rqtp.tv_nsec = 0;
#endif
  for (try = 0; try < FSEEK_TRIES; try++) {
    if (lzma_seek(lzmafp, offset, dir) == -1) {
#if defined(vxWorks)
      nanosleep(&rqtp, NULL);
#else
      sleep(1);
#endif
    } else
      break;
  }
  if (try == 0)
    return 0;
  if (try == FSEEK_TRIES) {
    fputs("warning: lzma_seek problems--unable to recover\n", stderr);
    return -1;
  }
  fputs("warning: lzma_seek problems--recovered\n", stderr);
  return 0;
}

#if defined(zLib)
/**
 * @brief Sets the file position indicator for a given GZIP file stream with retry logic.
 *
 * Attempts to set the file position indicator for the specified GZIP file stream (`gzfp`) to a new position
 * defined by `offset` and `dir`. The function retries the `gzseek` operation up to `FSEEK_TRIES`
 * times in case of transient failures, implementing a delay between attempts.
 *
 * @param gzfp   Pointer to the `gzFile` stream whose position indicator is to be set.
 * @param offset Number of bytes to offset from the position specified by `dir`.
 * @param dir    Positioning directive, which can be one of:
 *               - `SEEK_SET` to set the position relative to the beginning of the file,
 *               - `SEEK_CUR` to set the position relative to the current position,
 *               - `SEEK_END` to set the position relative to the end of the file.
 *
 * @return 
 *   - Returns `0` if the operation is successful.
 *   - Returns `-1` if all retry attempts fail to set the file position.
 *
 * @details
 * The function attempts to set the file position using `gzseek`. If `gzseek` fails, it sleeps for 1 second
 * (or 1 second using `nanosleep` on vxWorks systems) before retrying. After `FSEEK_TRIES` unsuccessful
 * attempts, it reports a warning and returns `-1`.
 *
 * @note 
 * - The function is designed to handle temporary file access issues by retrying the `gzseek` operation.
 * - It is not suitable for non-recoverable `gzseek` errors, which will cause it to fail after retries.
 */
int32_t SDDS_gzseek(gzFile gzfp, int64_t offset, int32_t dir) {
  int32_t try;
#  if defined(vxWorks)
  struct timespec rqtp;
  rqtp.tv_sec = 1;
  rqtp.tv_nsec = 0;
#  endif
  for (try = 0; try < FSEEK_TRIES; try++) {
    if (gzseek(gzfp, offset, dir) == -1) {
#  if defined(vxWorks)
      nanosleep(&rqtp, NULL);
#  else
      sleep(1);
#  endif
    } else
      break;
  }
  if (try == 0)
    return 0;
  if (try == FSEEK_TRIES) {
    fputs("warning: gzseek problems--unable to recover\n", stderr);
    return -1;
  }
  fputs("warning: gzseek problems--recovered\n", stderr);
  return 0;
}
#endif

/**
 * @brief Writes the binary parameters of the SDDS dataset.
 *
 * This function writes all non-fixed parameters of the SDDS dataset to the associated binary file.
 * It handles different compression formats such as gzip and LZMA if enabled.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure containing the dataset to write.
 *
 * @return 
 *   - Returns `1` on successful write of all parameters.
 *   - Returns `0` if an error occurs during the writing process.
 *
 * @details
 * The function performs the following steps:
 * - Validates the dataset structure.
 * - Iterates over all parameters defined in the dataset layout.
 * - For each parameter:
 *   - If it is a fixed value, it is skipped.
 *   - If the parameter is of type `SDDS_STRING`, it writes the string using the appropriate
 *     compression method.
 *   - Otherwise, it writes the parameter's value using buffered write functions.
 *
 * The function handles different file formats:
 * - For gzip files, it uses `SDDS_GZipWriteBinaryString` and `SDDS_GZipBufferedWrite`.
 * - For LZMA files, it uses `SDDS_LZMAWriteBinaryString` and `SDDS_LZMABufferedWrite`.
 * - For standard binary files, it uses `SDDS_WriteBinaryString` and `SDDS_BufferedWrite`.
 *
 * @note 
 * - The function assumes that the dataset has been properly initialized and that all parameters are correctly allocated.
 * - Compression support (`zLib` or LZMA) must be enabled during compilation for handling compressed files.
 */
int32_t SDDS_WriteBinaryParameters(SDDS_DATASET *SDDS_dataset) {
  int32_t i;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  /*  static char buffer[SDDS_MAXLINE]; */
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteBinaryParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    fBuffer = &SDDS_dataset->fBuffer;
    for (i = 0; i < layout->n_parameters; i++) {
      if (layout->parameter_definition[i].fixed_value)
        continue;
      if (layout->parameter_definition[i].type == SDDS_STRING) {
        if (!SDDS_GZipWriteBinaryString(*((char **)SDDS_dataset->parameter[i]), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write parameters--failure writing string (SDDS_WriteBinaryParameters)");
          return (0);
        }
      } else if (!SDDS_GZipBufferedWrite(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], gzfp, fBuffer)) {
        SDDS_SetError("Unable to write parameters--failure writing value (SDDS_WriteBinaryParameters)");
        return (0);
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      fBuffer = &SDDS_dataset->fBuffer;
      for (i = 0; i < layout->n_parameters; i++) {
        if (layout->parameter_definition[i].fixed_value)
          continue;
        if (layout->parameter_definition[i].type == SDDS_STRING) {
          if (!SDDS_LZMAWriteBinaryString(*((char **)SDDS_dataset->parameter[i]), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write parameters--failure writing string (SDDS_WriteBinaryParameters)");
            return (0);
          }
        } else if (!SDDS_LZMABufferedWrite(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], lzmafp, fBuffer)) {
          SDDS_SetError("Unable to write parameters--failure writing value (SDDS_WriteBinaryParameters)");
          return (0);
        }
      }
    } else {
      fp = layout->fp;
      fBuffer = &SDDS_dataset->fBuffer;
      for (i = 0; i < layout->n_parameters; i++) {
        if (layout->parameter_definition[i].fixed_value)
          continue;
        if (layout->parameter_definition[i].type == SDDS_STRING) {
          if (!SDDS_WriteBinaryString(*((char **)SDDS_dataset->parameter[i]), fp, fBuffer)) {
            SDDS_SetError("Unable to write parameters--failure writing string (SDDS_WriteBinaryParameters)");
            return (0);
          }
        } else if (!SDDS_BufferedWrite(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], fp, fBuffer)) {
          SDDS_SetError("Unable to write parameters--failure writing value (SDDS_WriteBinaryParameters)");
          return (0);
        }
      }
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Writes the binary arrays of the SDDS dataset to a file.
 *
 * This function writes all arrays defined in the SDDS dataset to the associated binary file.
 * It handles arrays with and without dimensions, and manages different compression formats
 * such as gzip and LZMA if enabled.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure containing the dataset to write.
 *
 * @return 
 *   - Returns `1` on successful write of all arrays.
 *   - Returns `0` if an error occurs during the writing process.
 *
 * @details
 * The function performs the following steps:
 * - Validates the dataset structure.
 * - Iterates over all arrays defined in the dataset layout.
 * - For each array:
 *   - If the array has no dimensions, it writes zeroes for each defined dimension.
 *   - If the array has dimensions, it writes the dimension sizes using the appropriate
 *     compression method.
 *   - If the array type is `SDDS_STRING`, it writes each string element using the appropriate
 *     compression method.
 *   - Otherwise, it writes the array's data using buffered write functions.
 *
 * The function handles different file formats:
 * - For gzip files, it uses `SDDS_GZipWriteBinaryString` and `SDDS_GZipBufferedWrite`.
 * - For LZMA files, it uses `SDDS_LZMAWriteBinaryString` and `SDDS_LZMABufferedWrite`.
 * - For standard binary files, it uses `SDDS_WriteBinaryString` and `SDDS_BufferedWrite`.
 *
 * @note 
 * - The function assumes that the dataset has been properly initialized and that all arrays are correctly allocated.
 * - Compression support (`zLib` or LZMA) must be enabled during compilation for handling compressed files.
 */
int32_t SDDS_WriteBinaryArrays(SDDS_DATASET *SDDS_dataset) {
  int32_t i, j, zero = 0;
  SDDS_LAYOUT *layout;
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteBinaryArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    fBuffer = &SDDS_dataset->fBuffer;
    for (i = 0; i < layout->n_arrays; i++) {
      if (!SDDS_dataset->array[i].dimension) {
        for (j = 0; j < layout->array_definition[i].dimensions; j++)
          if (!SDDS_GZipBufferedWrite(&zero, sizeof(zero), gzfp, fBuffer)) {
            SDDS_SetError("Unable to write null array--failure writing dimensions (SDDS_WriteBinaryArrays)");
            return 0;
          }
        continue;
      }
      if (!SDDS_GZipBufferedWrite(SDDS_dataset->array[i].dimension, sizeof(*(SDDS_dataset->array)[i].dimension) * layout->array_definition[i].dimensions, gzfp, fBuffer)) {
        SDDS_SetError("Unable to write arrays--failure writing dimensions (SDDS_WriteBinaryArrays)");
        return (0);
      }
      if (layout->array_definition[i].type == SDDS_STRING) {
        for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
          if (!SDDS_GZipWriteBinaryString(((char **)SDDS_dataset->array[i].data)[j], gzfp, fBuffer)) {
            SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteBinaryArrays)");
            return (0);
          }
        }
      } else if (!SDDS_GZipBufferedWrite(SDDS_dataset->array[i].data, SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements, gzfp, fBuffer)) {
        SDDS_SetError("Unable to write arrays--failure writing values (SDDS_WriteBinaryArrays)");
        return (0);
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.gzipFile) {
      lzmafp = layout->lzmafp;
      fBuffer = &SDDS_dataset->fBuffer;
      for (i = 0; i < layout->n_arrays; i++) {
        if (!SDDS_dataset->array[i].dimension) {
          for (j = 0; j < layout->array_definition[i].dimensions; j++)
            if (!SDDS_LZMABufferedWrite(&zero, sizeof(zero), lzmafp, fBuffer)) {
              SDDS_SetError("Unable to write null array--failure writing dimensions (SDDS_WriteBinaryArrays)");
              return 0;
            }
          continue;
        }
        if (!SDDS_LZMABufferedWrite(SDDS_dataset->array[i].dimension, sizeof(*(SDDS_dataset->array)[i].dimension) * layout->array_definition[i].dimensions, lzmafp, fBuffer)) {
          SDDS_SetError("Unable to write arrays--failure writing dimensions (SDDS_WriteBinaryArrays)");
          return (0);
        }
        if (layout->array_definition[i].type == SDDS_STRING) {
          for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
            if (!SDDS_LZMAWriteBinaryString(((char **)SDDS_dataset->array[i].data)[j], lzmafp, fBuffer)) {
              SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteBinaryArrays)");
              return (0);
            }
          }
        } else if (!SDDS_LZMABufferedWrite(SDDS_dataset->array[i].data, SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements, lzmafp, fBuffer)) {
          SDDS_SetError("Unable to write arrays--failure writing values (SDDS_WriteBinaryArrays)");
          return (0);
        }
      }
    } else {
      fp = layout->fp;
      fBuffer = &SDDS_dataset->fBuffer;
      for (i = 0; i < layout->n_arrays; i++) {
        if (!SDDS_dataset->array[i].dimension) {
          for (j = 0; j < layout->array_definition[i].dimensions; j++)
            if (!SDDS_BufferedWrite(&zero, sizeof(zero), fp, fBuffer)) {
              SDDS_SetError("Unable to write null array--failure writing dimensions (SDDS_WriteBinaryArrays)");
              return 0;
            }
          continue;
        }
        if (!SDDS_BufferedWrite(SDDS_dataset->array[i].dimension, sizeof(*(SDDS_dataset->array)[i].dimension) * layout->array_definition[i].dimensions, fp, fBuffer)) {
          SDDS_SetError("Unable to write arrays--failure writing dimensions (SDDS_WriteBinaryArrays)");
          return (0);
        }
        if (layout->array_definition[i].type == SDDS_STRING) {
          for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
            if (!SDDS_WriteBinaryString(((char **)SDDS_dataset->array[i].data)[j], fp, fBuffer)) {
              SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteBinaryArrays)");
              return (0);
            }
          }
        } else if (!SDDS_BufferedWrite(SDDS_dataset->array[i].data, SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements, fp, fBuffer)) {
          SDDS_SetError("Unable to write arrays--failure writing values (SDDS_WriteBinaryArrays)");
          return (0);
        }
      }
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Writes the binary columns of an SDDS dataset to the associated file.
 *
 * This function iterates over each column defined in the SDDS dataset layout and writes its data
 * to the binary file. It handles different data types, including strings and numeric types, and
 * supports various compression formats such as gzip and LZMA if enabled.
 *
 * Depending on the dataset's configuration, the function writes directly to a standard binary
 * file, a gzip-compressed file, or an LZMA-compressed file. It also handles sparse data by
 * only writing rows flagged for inclusion.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset to write.
 *
 * @return 
 *   - Returns `1` on successful writing of all columns.
 *   - Returns `0` if an error occurs during the writing process.
 *
 * @details
 * The function performs the following steps:
 * - Validates the dataset structure using `SDDS_CheckDataset`.
 * - Determines the file format (standard, gzip, LZMA) and initializes the corresponding file pointer.
 * - Iterates through each column in the dataset:
 *   - For string columns, writes each string entry individually using the appropriate write function.
 *   - For numeric columns, writes the entire column data in a buffered manner if all rows are flagged;
 *     otherwise, writes individual row entries.
 * - Handles errors by setting appropriate error messages and aborting the write operation.
 *
 * @note
 * - The function assumes that the dataset has been properly initialized and populated with data.
 * - Compression support (`zLib` for gzip, LZMA libraries) must be enabled during compilation
 *   for handling compressed files.
 * - The function is not thread-safe and should be called in a synchronized context.
 */
int32_t SDDS_WriteBinaryColumns(SDDS_DATASET *SDDS_dataset) {
  int64_t i, row, rows, type, size;
  SDDS_LAYOUT *layout;
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteBinaryColumns"))
    return (0);
  layout = &SDDS_dataset->layout;
  fBuffer = &SDDS_dataset->fBuffer;
  rows = SDDS_CountRowsOfInterest(SDDS_dataset);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    for (i = 0; i < layout->n_columns; i++) {
      type = layout->column_definition[i].type;
      size = SDDS_type_size[type - 1];
      if (type == SDDS_STRING) {
        for (row = 0; row < SDDS_dataset->n_rows; row++) {
          if (SDDS_dataset->row_flag[row] && !SDDS_GZipWriteBinaryString(*((char **)SDDS_dataset->data[i] + row), gzfp, fBuffer)) {
            SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteBinaryColumns)");
            return (0);
          }
        }
      } else {
        if (rows == SDDS_dataset->n_rows) {
          if (!SDDS_GZipBufferedWrite(SDDS_dataset->data[i], size * rows, gzfp, fBuffer)) {
            SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteBinaryColumns)");
            return (0);
          }
        } else {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (SDDS_dataset->row_flag[row] && !SDDS_GZipBufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, gzfp, fBuffer)) {
              SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteBinaryColumns)");
              return (0);
            }
          }
        }
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      for (i = 0; i < layout->n_columns; i++) {
        type = layout->column_definition[i].type;
        size = SDDS_type_size[type - 1];
        if (layout->column_definition[i].type == SDDS_STRING) {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (SDDS_dataset->row_flag[row] && !SDDS_LZMAWriteBinaryString(*((char **)SDDS_dataset->data[i] + row), lzmafp, fBuffer)) {
              SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteBinaryColumns)");
              return (0);
            }
          }
        } else {
          if (rows == SDDS_dataset->n_rows) {
            if (!SDDS_LZMABufferedWrite(SDDS_dataset->data[i], size * rows, lzmafp, fBuffer)) {
              SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteBinaryColumns)");
              return (0);
            }
          } else {
            for (row = 0; row < SDDS_dataset->n_rows; row++) {
              if (SDDS_dataset->row_flag[row] && !SDDS_LZMABufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, lzmafp, fBuffer)) {
                SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteBinaryColumns)");
                return (0);
              }
            }
          }
        }
      }
    } else {
      fp = layout->fp;
      for (i = 0; i < layout->n_columns; i++) {
        type = layout->column_definition[i].type;
        size = SDDS_type_size[type - 1];
        if (layout->column_definition[i].type == SDDS_STRING) {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (SDDS_dataset->row_flag[row] && !SDDS_WriteBinaryString(*((char **)SDDS_dataset->data[i] + row), fp, fBuffer)) {
              SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteBinaryColumns)");
              return (0);
            }
          }
        } else {
          if (rows == SDDS_dataset->n_rows) {
            if (!SDDS_BufferedWrite(SDDS_dataset->data[i], size * rows, fp, fBuffer)) {
              SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteBinaryColumns)");
              return (0);
            }
          } else {
            for (row = 0; row < SDDS_dataset->n_rows; row++) {
              if (SDDS_dataset->row_flag[row] && !SDDS_BufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, fp, fBuffer)) {
                SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteBinaryColumns)");
                return (0);
              }
            }
          }
        }
      }
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Writes non-native endian binary columns of an SDDS dataset to the associated file.
 *
 * This function iterates over each column defined in the SDDS dataset layout and writes its data
 * to the binary file using a non-native byte order. It handles different data types, including
 * strings and numeric types, and supports various compression formats such as gzip and LZMA if
 * enabled.
 *
 * Depending on the dataset's configuration, the function writes directly to a standard binary
 * file, a gzip-compressed file, or an LZMA-compressed file. It also handles sparse data by
 * only writing rows flagged for inclusion.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset to write.
 *
 * @return 
 *   - Returns `1` on successful writing of all columns.
 *   - Returns `0` if an error occurs during the writing process.
 *
 * @details
 * The function performs the following steps:
 * - Validates the dataset structure using `SDDS_CheckDataset`.
 * - Determines the file format (standard, gzip, LZMA) and initializes the corresponding file pointer.
 * - Iterates through each column in the dataset:
 *   - For string columns, writes each string entry individually using the appropriate non-native write function.
 *   - For numeric columns, writes the entire column data in a buffered manner if all rows are flagged;
 *     otherwise, writes individual row entries.
 * - Handles errors by setting appropriate error messages and aborting the write operation.
 *
 * @note
 * - The function assumes that the dataset has been properly initialized and populated with data.
 * - Compression support (`zLib` for gzip, LZMA libraries) must be enabled during compilation
 *   for handling compressed files.
 * - The function is not thread-safe and should be called in a synchronized context.
 */
int32_t SDDS_WriteNonNativeBinaryColumns(SDDS_DATASET *SDDS_dataset) {
  int64_t i, row, rows, size, type;
  SDDS_LAYOUT *layout;
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteNonNativeBinaryColumns"))
    return (0);
  layout = &SDDS_dataset->layout;
  rows = SDDS_CountRowsOfInterest(SDDS_dataset);
  fBuffer = &SDDS_dataset->fBuffer;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    for (i = 0; i < layout->n_columns; i++) {
      type = layout->column_definition[i].type;
      size = SDDS_type_size[type - 1];
      if (type == SDDS_STRING) {
        for (row = 0; row < SDDS_dataset->n_rows; row++) {
          if (SDDS_dataset->row_flag[row] && !SDDS_GZipWriteNonNativeBinaryString(*((char **)SDDS_dataset->data[i] + row), gzfp, fBuffer)) {
            SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteNonNativeBinaryColumns)");
            return (0);
          }
        }
      } else {
        if (rows == SDDS_dataset->n_rows) {
          if (!SDDS_GZipBufferedWrite((char *)SDDS_dataset->data[i], size * rows, gzfp, fBuffer)) {
            SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteNonNativeBinaryColumns)");
            return (0);
          }
        } else {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (SDDS_dataset->row_flag[row] && !SDDS_GZipBufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, gzfp, fBuffer)) {
              SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteNonNativeBinaryColumns)");
              return (0);
            }
          }
        }
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      for (i = 0; i < layout->n_columns; i++) {
        type = layout->column_definition[i].type;
        size = SDDS_type_size[type - 1];
        if (type == SDDS_STRING) {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (SDDS_dataset->row_flag[row] && !SDDS_LZMAWriteNonNativeBinaryString(*((char **)SDDS_dataset->data[i] + row), lzmafp, fBuffer)) {
              SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteNonNativeBinaryColumns)");
              return (0);
            }
          }
        } else {
          if (rows == SDDS_dataset->n_rows) {
            if (!SDDS_LZMABufferedWrite(SDDS_dataset->data[i], size * rows, lzmafp, fBuffer)) {
              SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteNonNativeBinaryColumns)");
              return (0);
            }
          } else {
            for (row = 0; row < SDDS_dataset->n_rows; row++) {
              if (SDDS_dataset->row_flag[row] && !SDDS_LZMABufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, lzmafp, fBuffer)) {
                SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteNonNativeBinaryColumns)");
                return (0);
              }
            }
          }
        }
      }
    } else {
      fp = layout->fp;
      for (i = 0; i < layout->n_columns; i++) {
        type = layout->column_definition[i].type;
        size = SDDS_type_size[type - 1];
        if (type == SDDS_STRING) {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (SDDS_dataset->row_flag[row] && !SDDS_WriteNonNativeBinaryString(*((char **)SDDS_dataset->data[i] + row), fp, fBuffer)) {
              SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteNonNativeBinaryColumns)");
              return (0);
            }
          }
        } else {
          if (rows == SDDS_dataset->n_rows) {
            if (!SDDS_BufferedWrite(SDDS_dataset->data[i], size * rows, fp, fBuffer)) {
              SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteNonNativeBinaryColumns)");
              return (0);
            }
          } else {
            for (row = 0; row < SDDS_dataset->n_rows; row++) {
              if (SDDS_dataset->row_flag[row] && !SDDS_BufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, fp, fBuffer)) {
                SDDS_SetError("Unable to write columns--failure writing values (SDDS_WriteNonNativeBinaryColumns)");
                return (0);
              }
            }
          }
        }
      }
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Writes a single binary row of an SDDS dataset to the associated file.
 *
 * This function writes the data of a specified row within the SDDS dataset to the binary file.
 * It handles different data types, including strings and numeric types, and supports various
 * compression formats such as gzip and LZMA if enabled.
 *
 * Depending on the dataset's configuration, the function writes directly to a standard binary
 * file, a gzip-compressed file, or an LZMA-compressed file.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param row          The zero-based index of the row to write.
 *
 * @return 
 *   - Returns `1` on successful writing of the row.
 *   - Returns `0` if an error occurs during the writing process.
 *
 * @details
 * The function performs the following steps:
 * - Validates the dataset structure using `SDDS_CheckDataset`.
 * - Determines the file format (standard, gzip, LZMA) and initializes the corresponding file pointer.
 * - Iterates through each column in the dataset:
 *   - For string columns, writes the string entry of the specified row using the appropriate write function.
 *   - For numeric columns, writes the data of the specified row using buffered write functions.
 * - Handles errors by setting appropriate error messages and aborting the write operation.
 *
 * @note
 * - The function assumes that the dataset has been properly initialized and that the specified row exists.
 * - Compression support (`zLib` for gzip, LZMA libraries) must be enabled during compilation
 *   for handling compressed files.
 * - The function is not thread-safe and should be called in a synchronized context.
 */
int32_t SDDS_WriteBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row) {
  int64_t i, type, size;
  SDDS_LAYOUT *layout;
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteBinaryRow"))
    return (0);
  layout = &SDDS_dataset->layout;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    fBuffer = &SDDS_dataset->fBuffer;
    for (i = 0; i < layout->n_columns; i++) {
      if ((type = layout->column_definition[i].type) == SDDS_STRING) {
        if (!SDDS_GZipWriteBinaryString(*((char **)SDDS_dataset->data[i] + row), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write rows--failure writing string (SDDS_WriteBinaryRows)");
          return (0);
        }
      } else {
        size = SDDS_type_size[type - 1];
        if (!SDDS_GZipBufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, gzfp, fBuffer)) {
          SDDS_SetError("Unable to write row--failure writing value (SDDS_WriteBinaryRow)");
          return (0);
        }
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      fBuffer = &SDDS_dataset->fBuffer;
      for (i = 0; i < layout->n_columns; i++) {
        if ((type = layout->column_definition[i].type) == SDDS_STRING) {
          if (!SDDS_LZMAWriteBinaryString(*((char **)SDDS_dataset->data[i] + row), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write rows--failure writing string (SDDS_WriteBinaryRows)");
            return (0);
          }
        } else {
          size = SDDS_type_size[type - 1];
          if (!SDDS_LZMABufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write row--failure writing value (SDDS_WriteBinaryRow)");
            return (0);
          }
        }
      }
    } else {
      fp = layout->fp;
      fBuffer = &SDDS_dataset->fBuffer;
      for (i = 0; i < layout->n_columns; i++) {
        if ((type = layout->column_definition[i].type) == SDDS_STRING) {
          if (!SDDS_WriteBinaryString(*((char **)SDDS_dataset->data[i] + row), fp, fBuffer)) {
            SDDS_SetError("Unable to write rows--failure writing string (SDDS_WriteBinaryRows)");
            return (0);
          }
        } else {
          size = SDDS_type_size[type - 1];
          if (!SDDS_BufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, fp, fBuffer)) {
            SDDS_SetError("Unable to write row--failure writing value (SDDS_WriteBinaryRow)");
            return (0);
          }
        }
      }
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Checks if any data in an SDDS page was recovered after an error was detected.
 *
 * This function inspects the SDDS dataset to determine if any data recovery was possible
 * following an error during data reading. It resets the recovery flag after checking.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 *
 * @return 
 *   - Returns `1` if recovery was possible.
 *   - Returns `0` if no recovery was performed or if recovery was not possible.
 *
 * @details
 * The function performs the following steps:
 * - Retrieves the current state of the `readRecoveryPossible` flag from the dataset.
 * - Resets the `readRecoveryPossible` flag to `0`.
 * - Returns the original state of the `readRecoveryPossible` flag.
 *
 * @note
 * - This function is typically used after attempting to recover from a read error to verify
 *   if any partial data was successfully recovered.
 * - The recovery flag is automatically managed by other functions within the SDDS library.
 */
int32_t SDDS_ReadRecoveryPossible(SDDS_DATASET *SDDS_dataset) {
  int32_t returnValue;

  returnValue = SDDS_dataset->readRecoveryPossible;
  SDDS_dataset->readRecoveryPossible = 0;
  return returnValue;
}

/**
 * @brief Sets the read recovery mode for an SDDS dataset.
 *
 * This function configures whether read recovery is possible for the specified SDDS dataset.
 * Enabling recovery allows the dataset to attempt to recover partial data in case of read errors.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param mode          Integer flag indicating the recovery mode:
 *                      - `0` to disable read recovery.
 *                      - `1` to enable read recovery.
 *
 * @details
 * The function updates the `readRecoveryPossible` flag within the dataset structure based on the
 * provided `mode` parameter. This flag is later checked by other functions to determine whether
 * to attempt data recovery after encountering read errors.
 *
 * @note
 * - Enabling read recovery does not guarantee that all data can be recovered after an error.
 * - It is recommended to enable recovery only if partial data recovery is acceptable in your application.
 */
void SDDS_SetReadRecoveryMode(SDDS_DATASET *SDDS_dataset, int32_t mode) {
  SDDS_dataset->readRecoveryPossible = mode;
}

/**
 * @brief Reads a binary page from an SDDS dataset.
 *
 * This function reads a binary page from the specified SDDS dataset. It allows for sparse reading
 * by specifying the `sparse_interval` and `sparse_offset` parameters, enabling the reading of
 * data at specified intervals or starting from a specific offset.
 *
 * @param SDDS_dataset      Pointer to the `SDDS_DATASET` structure representing the dataset to read from.
 * @param sparse_interval   Interval at which to read rows. A value greater than `1` enables sparse reading.
 * @param sparse_offset     Number of initial rows to skip before starting to read data.
 * @param sparse_statistics Flag indicating whether to compute statistics during sparse reading:
 *                          - `0`: No statistics.
 *                          - `1`: Compute average.
 *                          - `2`: Compute median.
 *                          - `3`: Compute minimum.
 *                          - `4`: Compute maximum.
 *
 * @return 
 *   - Returns the page number on successful read.
 *   - Returns `-1` if the end-of-file is reached.
 *   - Returns `0` on error.
 *
 * @details
 * The function internally calls `SDDS_ReadBinaryPageDetailed` with the provided parameters to perform
 * the actual reading. It handles various scenarios, including non-native byte orders and different
 * data layouts (row-major or column-major).
 *
 * @note
 * - This function is typically called to read data pages in bulk, allowing for efficient data access
 *   by skipping unnecessary rows.
 * - Sparse statistics can be used to reduce the amount of data by computing aggregated values.
 * - The function assumes that the dataset has been properly initialized and that the file pointers
 *   are correctly set up.
 */
int32_t SDDS_ReadBinaryPage(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int32_t sparse_statistics) {
  return SDDS_ReadBinaryPageDetailed(SDDS_dataset, sparse_interval, sparse_offset, 0, sparse_statistics);
}

/**
 * @brief Reads the last specified number of rows from a binary page of an SDDS dataset.
 *
 * This function reads the last `last_rows` rows from the binary page of the specified SDDS dataset.
 * It is useful for retrieving recent data entries without processing the entire dataset.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset to read from.
 * @param last_rows    The number of rows to read from the end of the dataset.
 *
 * @return 
 *   - Returns the page number on successful read.
 *   - Returns `-1` if the end-of-file is reached.
 *   - Returns `0` on error.
 *
 * @details
 * The function internally calls `SDDS_ReadBinaryPageDetailed` with `sparse_interval` set to `1`,
 * `sparse_offset` set to `0`, and `last_rows` as specified. This configuration ensures that only
 * the last `last_rows` rows are read from the dataset.
 *
 * @note
 * - This function is particularly useful for applications that need to display or process the most
 *   recent data entries.
 * - Ensure that `last_rows` does not exceed the total number of rows in the dataset to avoid errors.
 * - The function assumes that the dataset has been properly initialized and that the file pointers
 *   are correctly set up.
 */
int32_t SDDS_ReadBinaryPageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows) {
  return SDDS_ReadBinaryPageDetailed(SDDS_dataset, 1, 0, last_rows, 0);
}

/**
 * @brief Reads a binary page from an SDDS dataset with detailed options.
 *
 * This function reads a binary page from the specified SDDS dataset, providing detailed control
 * over the reading process. It supports sparse reading, reading a specific number of rows from
 * the end, and computing statistics on the data.
 *
 * Typically, this function is not called directly. Instead, it is invoked through higher-level
 * functions such as `SDDS_ReadBinaryPage` or `SDDS_ReadBinaryPageLastRows`, which provide
 * simplified interfaces for common reading scenarios.
 *
 * @param SDDS_dataset       Pointer to the `SDDS_DATASET` structure representing the dataset to read from.
 * @param sparse_interval    Interval at which to read rows. A value greater than `1` enables sparse reading.
 * @param sparse_offset      Number of initial rows to skip before starting to read data.
 * @param last_rows          The number of rows to read from the end of the dataset. If `0`, all rows are read.
 * @param sparse_statistics  Flag indicating whether to compute statistics during sparse reading:
 *                           - `0`: No statistics.
 *                           - `1`: Compute average.
 *                           - `2`: Compute median.
 *                           - `3`: Compute minimum.
 *                           - `4`: Compute maximum.
 *
 * @return 
 *   - Returns the page number on successful read.
 *   - Returns `-1` if the end-of-file is reached.
 *   - Returns `0` on error.
 *
 * @details
 * The function performs the following steps:
 * - Checks if the dataset has been auto-recovered; if so, it returns `-1`.
 * - Determines if the dataset uses native or non-native byte order and delegates to `SDDS_ReadNonNativePageDetailed` if necessary.
 * - Initializes file pointers based on the compression format (standard, gzip, LZMA).
 * - Allocates and initializes the buffer for reading if not already allocated.
 * - Reads the number of rows from the binary file, handling both 32-bit and 64-bit row counts.
 * - Validates the row count and ensures it does not exceed predefined limits.
 * - Adjusts for column-major layouts by calling `SDDS_ReadBinaryColumns` if necessary.
 * - Handles sparse reading by skipping rows based on `sparse_interval` and `sparse_offset`.
 * - If `sparse_statistics` is enabled, computes the specified statistics (average, median, min, max) on floating-point data.
 * - Handles errors by setting appropriate error messages and managing recovery modes.
 *
 * @note
 * - This function provides extensive control over the reading process, allowing for optimized data access.
 * - Ensure that all parameters are set correctly to avoid unintended data skips or miscomputations.
 * - The function assumes that the dataset has been properly initialized and that the file pointers
 *   are correctly set up.
 * - Compression support (`zLib` for gzip, LZMA libraries) must be enabled during compilation
 *   for handling compressed files.
 */
int32_t SDDS_ReadBinaryPageDetailed(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int64_t last_rows, int32_t sparse_statistics) {
  int32_t n_rows32;
  int64_t n_rows, i, j, k, alloc_rows, rows_to_store, mod;

  /*  int32_t page_number, i; */
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;
  SDDS_FILEBUFFER *fBuffer;
  void **statData=NULL;
  double statResult;

  if (SDDS_dataset->autoRecovered)
    return -1;
  if (SDDS_dataset->swapByteOrder) {
    return SDDS_ReadNonNativePageDetailed(SDDS_dataset, 0, sparse_interval, sparse_offset, last_rows);
  }

  /*  static char s[SDDS_MAXLINE]; */
  n_rows = 0;
  SDDS_SetReadRecoveryMode(SDDS_dataset, 0);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = SDDS_dataset->layout.gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = SDDS_dataset->layout.lzmafp;
    } else {
      fp = SDDS_dataset->layout.fp;
    }
#if defined(zLib)
  }
#endif
  fBuffer = &SDDS_dataset->fBuffer;
  if (!fBuffer->buffer) {
    if (defaultIOBufferSize == 0 && (SDDS_dataset->layout.popenUsed || !SDDS_dataset->layout.filename) && (sparse_interval > 1 || sparse_offset > 0 || last_rows > 0)) {
      SDDS_SetError("The IO buffer size is 0 for data being read from a pipe with sparsing.  This is not supported.");
      return 0;
    }
    if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (defaultIOBufferSize + 1)))) {
      SDDS_SetError("Unable to do buffered read--allocation failure");
      return 0;
    }
    fBuffer->bufferSize = defaultIOBufferSize;
    fBuffer->bytesLeft = 0;
  }
  SDDS_dataset->rowcount_offset = -1;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_GZipBufferedRead(&n_rows32, sizeof(n_rows32), gzfp, &SDDS_dataset->fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
      if (gzeof(gzfp))
        return (SDDS_dataset->page_number = -1);
      SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadBinaryPageDetailed)");
      return (0);
    }
    if (n_rows32 == INT32_MIN) {
      if (!SDDS_GZipBufferedRead(&n_rows, sizeof(n_rows), gzfp, &SDDS_dataset->fBuffer, SDDS_LONG64, SDDS_dataset->layout.byteOrderDeclared)) {
        if (gzeof(gzfp))
          return (SDDS_dataset->page_number = -1);
        SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadBinaryPageDetailed)");
        return (0);
      }
    } else {
      n_rows = n_rows32;
    }
  } else {
#endif
    /* This value will only be valid if read buffering is turned off, which is done for
     * certain append operations!   Should really modify SDDS_BufferedRead and SDDS_BufferedWrite
     * to provide ftell capability.
     */
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_LZMABufferedRead(&n_rows32, sizeof(n_rows32), lzmafp, &SDDS_dataset->fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
        if (lzma_eof(lzmafp))
          return (SDDS_dataset->page_number = -1);
        SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadBinaryPageDetailed)");
        return (0);
      }
      if (n_rows32 == INT32_MIN) {
        if (!SDDS_LZMABufferedRead(&n_rows, sizeof(n_rows), lzmafp, &SDDS_dataset->fBuffer, SDDS_LONG64, SDDS_dataset->layout.byteOrderDeclared)) {
          if (lzma_eof(lzmafp))
            return (SDDS_dataset->page_number = -1);
          SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadBinaryPageDetailed)");
          return (0);
        }
      } else {
        n_rows = n_rows32;
      }
    } else {
      SDDS_dataset->rowcount_offset = ftell(fp);
      if (!SDDS_BufferedRead(&n_rows32, sizeof(n_rows32), fp, &SDDS_dataset->fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
        if (feof(fp))
          return (SDDS_dataset->page_number = -1);
        SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadBinaryPageDetailed)");
        return (0);
      }
      if (n_rows32 == INT32_MIN) {
        if (!SDDS_BufferedRead(&n_rows, sizeof(n_rows), fp, &SDDS_dataset->fBuffer, SDDS_LONG64, SDDS_dataset->layout.byteOrderDeclared)) {
          if (feof(fp))
            return (SDDS_dataset->page_number = -1);
          SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadBinaryPageDetailed)");
          return (0);
        }
      } else {
        n_rows = n_rows32;
      }
    }
#if defined(zLib)
  }
#endif

#if defined(DEBUG)
  fprintf(stderr, "Expect %" PRId64 " rows of data\n", n_rows);
#endif
  if (n_rows < 0) {
    SDDS_SetError("Unable to read page--negative number of rows (SDDS_ReadBinaryPageDetailed)");
    return (0);
  }
  if (SDDS_dataset->layout.byteOrderDeclared == 0) {
    if (n_rows > 10000000) {
      SDDS_SetError("Unable to read page--endian byte order not declared and suspected to be non-native. (SDDS_ReadBinaryPageDetailed)");
      return (0);
    }
  }
  if (n_rows > SDDS_GetRowLimit()) {
    /* the number of rows is "unreasonably" large---treat like end-of-file */
    return (SDDS_dataset->page_number = -1);
  }
  if (last_rows < 0)
    last_rows = 0;
  /* Fix this limitation later */

  if (last_rows) {
    sparse_interval = 1;
    sparse_offset = n_rows - last_rows;
  }
  if (sparse_interval <= 0)
    sparse_interval = 1;
  if (sparse_offset < 0)
    sparse_offset = 0;

  rows_to_store = (n_rows - sparse_offset) / sparse_interval + 2;
  alloc_rows = rows_to_store - SDDS_dataset->n_rows_allocated;

  if (!SDDS_StartPage(SDDS_dataset, 0) || !SDDS_LengthenTable(SDDS_dataset, alloc_rows)) {
    SDDS_SetError("Unable to read page--couldn't start page (SDDS_ReadBinaryPageDetailed)");
    return (0);
  }

  /* read the parameter values */
  if (!SDDS_ReadBinaryParameters(SDDS_dataset)) {
    SDDS_SetError("Unable to read page--parameter reading error (SDDS_ReadBinaryPageDetailed)");
    return (0);
  }

  /* read the array values */
  if (!SDDS_ReadBinaryArrays(SDDS_dataset)) {
    SDDS_SetError("Unable to read page--array reading error (SDDS_ReadBinaryPageDetailed)");
    return (0);
  }
  if (SDDS_dataset->layout.data_mode.column_major) {
    SDDS_dataset->n_rows = n_rows;
    if (sparse_statistics == 0) {
      if (!SDDS_ReadBinaryColumns(SDDS_dataset, sparse_interval, sparse_offset)) {
        SDDS_SetError("Unable to read page--column reading error (SDDS_ReadBinaryPageDetailed)");
        return (0);
      }
      return (SDDS_dataset->page_number);
    } else {
      /* allocate space for full columns if needed */
      if (SDDS_dataset->n_rows_allocated < n_rows) {
        if (!SDDS_LengthenTable(SDDS_dataset, n_rows - SDDS_dataset->n_rows_allocated)) {
          SDDS_SetError("Unable to read page--couldn't start page (SDDS_ReadBinaryPageDetailed)");
          return (0);
        }
      }
      if (!SDDS_ReadBinaryColumns(SDDS_dataset, 1, 0)) {
        SDDS_SetError("Unable to read page--column reading error (SDDS_ReadBinaryPageDetailed)");
        return (0);
      }
      statData = (void**)malloc(SDDS_dataset->layout.n_columns * sizeof(void*));
      for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
        statData[i] = NULL;
        if (SDDS_FLOATING_TYPE(SDDS_dataset->layout.column_definition[i].type))
          statData[i] = (double*)calloc(sparse_interval, sizeof(double));
      }
      n_rows -= sparse_offset;
      for (j = k = 0; j < n_rows; j++) {
        for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
          switch (SDDS_dataset->layout.column_definition[i].type) {
          case SDDS_FLOAT:
            if (statData[i])
              ((double*)statData[i])[j % sparse_interval] = (double)(((float*)SDDS_dataset->data[i])[j + sparse_offset]);
            break;
          case SDDS_DOUBLE:
            if (statData[i])
              ((double*)statData[i])[j % sparse_interval] = ((double*)SDDS_dataset->data[i])[j + sparse_offset];
            break;
          case SDDS_LONGDOUBLE:
            if (statData[i])
              ((double*)statData[i])[j % sparse_interval] = (double)(((long double*)SDDS_dataset->data[i])[j + sparse_offset]);
            break;
          case SDDS_STRING:
            if (((char ***)SDDS_dataset->data)[i][k])
              free(((char ***)SDDS_dataset->data)[i][k]);
            ((char ***)SDDS_dataset->data)[i][k] = ((char ***)SDDS_dataset->data)[i][j + sparse_offset];
            ((char ***)SDDS_dataset->data)[i][j + sparse_offset] = NULL;
            break;
          case SDDS_SHORT:
            ((short*)SDDS_dataset->data[i])[k] = ((short*)SDDS_dataset->data[i])[j + sparse_offset];
            break;
          case SDDS_USHORT:
            ((unsigned short*)SDDS_dataset->data[i])[k] = ((unsigned short*)SDDS_dataset->data[i])[j + sparse_offset];
            break;
          case SDDS_LONG:
            ((int32_t*)SDDS_dataset->data[i])[k] = ((int32_t*)SDDS_dataset->data[i])[j + sparse_offset];
            break;
          case SDDS_ULONG:
            ((uint32_t*)SDDS_dataset->data[i])[k] = ((uint32_t*)SDDS_dataset->data[i])[j + sparse_offset];
            break;
          case SDDS_LONG64:
            ((int64_t*)SDDS_dataset->data[i])[k] = ((int64_t*)SDDS_dataset->data[i])[j + sparse_offset];
            break;
          case SDDS_ULONG64:
            ((uint64_t*)SDDS_dataset->data[i])[k] = ((uint64_t*)SDDS_dataset->data[i])[j + sparse_offset];
            break;
          case SDDS_CHARACTER:
            ((char*)SDDS_dataset->data[i])[k] = ((char*)SDDS_dataset->data[i])[j + sparse_offset];
            break;
          default:
            break;
          }
          if (statData[i]) {
            if (sparse_statistics == 1)
              compute_average(&statResult, (double*)statData[i], (j % sparse_interval) + 1);
            else if (sparse_statistics == 2)
              compute_median(&statResult, (double*)statData[i], (j % sparse_interval) + 1);
            else if (sparse_statistics == 3)
              statResult = min_in_array((double*)statData[i], (j % sparse_interval) + 1);
            else if (sparse_statistics == 4)
              statResult = max_in_array((double*)statData[i], (j % sparse_interval) + 1);
            switch (SDDS_dataset->layout.column_definition[i].type) {
            case SDDS_FLOAT:
              ((float*)SDDS_dataset->data[i])[k] = statResult;
              break;
            case SDDS_DOUBLE:
              ((double*)SDDS_dataset->data[i])[k] = statResult;
              break;
            case SDDS_LONGDOUBLE:
              ((long double*)SDDS_dataset->data[i])[k] = statResult;
              break;
            }
          }
        }
        if (j % sparse_interval == sparse_interval - 1)
          k++;
      }
      for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
        if (SDDS_dataset->layout.column_definition[i].type == SDDS_STRING) {
          int64_t r;
          for (r = k; r < SDDS_dataset->n_rows; r++) {
            if (((char ***)SDDS_dataset->data)[i][r]) {
              free(((char ***)SDDS_dataset->data)[i][r]);
              ((char ***)SDDS_dataset->data)[i][r] = NULL;
            }
          }
        }
        if (statData[i])
          free(statData[i]);
      }
      free(statData);
      SDDS_dataset->n_rows = k;
      return (SDDS_dataset->page_number);
    }
  }
  if ((sparse_interval <= 1) && (sparse_offset == 0)) {
    for (j = 0; j < n_rows; j++) {
      if (!SDDS_ReadBinaryRow(SDDS_dataset, j, 0)) {
        SDDS_dataset->n_rows = j;
        if (SDDS_dataset->autoRecover) {
#if defined(DEBUG)
          fprintf(stderr, "Doing auto-read recovery\n");
#endif
          SDDS_dataset->autoRecovered = 1;
          SDDS_ClearErrors();
          return (SDDS_dataset->page_number);
        }
        SDDS_SetError("Unable to read page--error reading data row (SDDS_ReadBinaryPageDetailed)");
        SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
        return (0);
      }
    }
    SDDS_dataset->n_rows = j;
    return (SDDS_dataset->page_number);
  } else {
    for (j = 0; j < sparse_offset; j++) {
      if (!SDDS_ReadBinaryRow(SDDS_dataset, 0, 1)) {
        SDDS_dataset->n_rows = 0;
        if (SDDS_dataset->autoRecover) {
          SDDS_dataset->autoRecovered = 1;
          SDDS_ClearErrors();
          return (SDDS_dataset->page_number);
        }
        SDDS_SetError("Unable to read page--error reading data row (SDDS_ReadBinaryPageDetailed)");
        SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
        return (0);
      }
    }
    n_rows -= sparse_offset;
    if (sparse_statistics != 0) {
      // Allocate buffer space for statistical sparsing
      statData = (void**)malloc(SDDS_dataset->layout.n_columns * sizeof(void*));
      for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
        if (SDDS_FLOATING_TYPE(SDDS_dataset->layout.column_definition[i].type)) {
          // Not ideal for SDDS_LONGDOUBLE but we may never run across this error
          statData[i] = (double*)calloc(sparse_interval, sizeof(double));
        }
      }
      for (j = k = 0; j < n_rows; j++) {
        if (!SDDS_ReadBinaryRow(SDDS_dataset, k, 0)) {
          SDDS_dataset->n_rows = k;
          if (SDDS_dataset->autoRecover) {
            SDDS_dataset->autoRecovered = 1;
            SDDS_ClearErrors();
            return (SDDS_dataset->page_number);
          }
          SDDS_SetError("Unable to read page--error reading data row (SDDS_ReadBinaryPageDetailed)");
          SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
          return (0);
        }
        for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
          switch (SDDS_dataset->layout.column_definition[i].type) {
          case SDDS_FLOAT:
            ((double*)statData[i])[j % sparse_interval] = (double)(((float*)SDDS_dataset->data[i])[k]);
            break;
          case SDDS_DOUBLE:
            ((double*)statData[i])[j % sparse_interval] = ((double*)SDDS_dataset->data[i])[k];
            break;
          case SDDS_LONGDOUBLE:
            ((double*)statData[i])[j % sparse_interval] = (double)(((long double*)SDDS_dataset->data[i])[k]);
            break;
          }
          if (SDDS_FLOATING_TYPE(SDDS_dataset->layout.column_definition[i].type)) {
            if (sparse_statistics == 1) {
              // Sparse and get average statistics
              compute_average(&statResult, (double*)statData[i], (j % sparse_interval) + 1);
            } else if (sparse_statistics == 2) {
              // Sparse and get median statistics
              compute_median(&statResult, (double*)statData[i], (j % sparse_interval) + 1);
            } else if (sparse_statistics == 3) {
              // Sparse and get minimum statistics
              statResult = min_in_array((double*)statData[i], (j % sparse_interval) + 1);
            } else if (sparse_statistics == 4) {
              // Sparse and get maximum statistics
              statResult = max_in_array((double*)statData[i], (j % sparse_interval) + 1);
            }
          }
          switch (SDDS_dataset->layout.column_definition[i].type) {
          case SDDS_FLOAT:
            ((float*)SDDS_dataset->data[i])[k] = statResult;
            break;
          case SDDS_DOUBLE:
            ((double*)SDDS_dataset->data[i])[k] = statResult;
            break;
          case SDDS_LONGDOUBLE:
            ((long double*)SDDS_dataset->data[i])[k] = statResult;
            break;
          }
        }
        if (j % sparse_interval == sparse_interval - 1) {
          k++;
        }
      }
      for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
        if (SDDS_FLOATING_TYPE(SDDS_dataset->layout.column_definition[i].type)) {
          free(statData[i]);
        }
      }
      free(statData);
    } else {
      for (j = k = 0; j < n_rows; j++) {
        if (!SDDS_ReadBinaryRow(SDDS_dataset, k, mod = j % sparse_interval)) {
          SDDS_dataset->n_rows = k;
          if (SDDS_dataset->autoRecover) {
            SDDS_dataset->autoRecovered = 1;
            SDDS_ClearErrors();
            return (SDDS_dataset->page_number);
          }
          SDDS_SetError("Unable to read page--error reading data row (SDDS_ReadBinaryPageDetailed)");
          SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
          return (0);
        }
        k += mod ? 0 : 1;
      }
    }
    SDDS_dataset->n_rows = k;
    return (SDDS_dataset->page_number);
  }
}

/**
 * @brief Writes a binary string to a file with buffering.
 *
 * This function writes a binary string to the specified file by first writing the length of the string
 * followed by the string's content to ensure proper binary formatting. If the input string is NULL,
 * an empty string is written instead. The writing operation utilizes a buffered approach to enhance performance.
 *
 * @param[in] string The null-terminated string to be written. If NULL, an empty string is written.
 * @param[in] fp The file pointer to write to. Must be an open file in binary write mode.
 * @param[in,out] fBuffer Pointer to the file buffer used for buffered writing operations.
 *
 * @return int32_t Returns 1 on success, 0 on failure.
 * @retval 1 Operation was successful.
 * @retval 0 An error occurred during writing.
 */
int32_t SDDS_WriteBinaryString(char *string, FILE *fp, SDDS_FILEBUFFER *fBuffer) {
  int32_t length;
  static char *dummy_string = "";
  if (!string)
    string = dummy_string;
  length = strlen(string);
  if (!SDDS_BufferedWrite(&length, sizeof(length), fp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing length");
    return (0);
  }
  if (length && !SDDS_BufferedWrite(string, sizeof(*string) * length, fp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing contents");
    return (0);
  }
  return (1);
}

/**
 * @brief Writes a binary string to a file with LZMA compression.
 *
 * This function writes a binary string to the specified LZMA-compressed file by first writing the length
 * of the string followed by the string's content. If the input string is NULL, an empty string is written instead.
 * The writing operation utilizes LZMA buffered write functions to ensure data is compressed appropriately.
 *
 * @param[in] string The null-terminated string to be written. If NULL, an empty string is written.
 * @param[in] lzmafp The LZMA file pointer to write to. Must be a valid, open LZMA-compressed file in write mode.
 * @param[in,out] fBuffer Pointer to the file buffer used for buffered writing operations.
 *
 * @return int32_t Returns 1 on success, 0 on failure.
 * @retval 1 Operation was successful.
 * @retval 0 An error occurred during writing.
 */
int32_t SDDS_LZMAWriteBinaryString(char *string, struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer) {
  int32_t length;
  static char *dummy_string = "";
  if (!string)
    string = dummy_string;
  length = strlen(string);
  if (!SDDS_LZMABufferedWrite(&length, sizeof(length), lzmafp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing length");
    return (0);
  }
  if (length && !SDDS_LZMABufferedWrite(string, sizeof(*string) * length, lzmafp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing contents");
    return (0);
  }
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes a binary string to a GZIP-compressed file with buffering.
 *
 * This function writes a binary string to the specified GZIP-compressed file by first writing the length
 * of the string followed by the string's content. If the input string is NULL, an empty string is written instead.
 * The writing operation uses GZIP buffered write functions to compress the data.
 *
 * @param[in] string The null-terminated string to be written. If NULL, an empty string is written.
 * @param[in] gzfp The GZIP file pointer to write to. Must be a valid, open GZIP-compressed file in write mode.
 * @param[in,out] fBuffer Pointer to the file buffer used for buffered writing operations.
 *
 * @return int32_t Returns 1 on success, 0 on failure.
 * @retval 1 Operation was successful.
 * @retval 0 An error occurred during writing.
 */
int32_t SDDS_GZipWriteBinaryString(char *string, gzFile gzfp, SDDS_FILEBUFFER *fBuffer) {
  int32_t length;
  static char *dummy_string = "";
  if (!string)
    string = dummy_string;
  length = strlen(string);
  if (!SDDS_GZipBufferedWrite(&length, sizeof(length), gzfp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing length");
    return (0);
  }
  if (length && !SDDS_GZipBufferedWrite(string, sizeof(*string) * length, gzfp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing contents");
    return (0);
  }
  return (1);
}
#endif

/**
 * @brief Reads a binary string from a file with buffering.
 *
 * This function reads a binary string from the specified file by first reading the length of the string
 * and then reading the string content based on the length. If the 'skip' parameter is set, the string data
 * is skipped over instead of being stored. The function allocates memory for the string, which should be
 * freed by the caller when no longer needed.
 *
 * @param[in] fp The file pointer to read from. Must be an open file in binary read mode.
 * @param[in,out] fBuffer Pointer to the file buffer used for buffered reading operations.
 * @param[in] skip If non-zero, the string data is skipped without being stored.
 *
 * @return char* Returns a pointer to the read null-terminated string on success, or NULL if an error occurred.
 * @retval NULL An error occurred during reading or memory allocation.
 * @retval Non-NULL Pointer to the read string.
 */
char *SDDS_ReadBinaryString(FILE *fp, SDDS_FILEBUFFER *fBuffer, int32_t skip) {
  int32_t length;
  char *string;

  if (!SDDS_BufferedRead(&length, sizeof(length), fp, fBuffer, SDDS_LONG, 0) || length < 0)
    return (0);
  if (!(string = SDDS_Malloc(sizeof(*string) * (length + 1))))
    return (NULL);
  if (length && !SDDS_BufferedRead(skip ? NULL : string, sizeof(*string) * length, fp, fBuffer, SDDS_STRING, 0))
    return (NULL);
  string[length] = 0;
  return (string);
}

/**
 * @brief Reads a binary string from an LZMA-compressed file with buffering.
 *
 * This function reads a binary string from the specified LZMA-compressed file by first reading the length
 * of the string and then reading the string content based on the length. If the 'skip' parameter is set,
 * the string data is skipped over instead of being stored. The function allocates memory for the string,
 * which should be freed by the caller when no longer needed.
 *
 * @param[in] lzmafp The LZMA file pointer to read from. Must be an open LZMA-compressed file in read mode.
 * @param[in,out] fBuffer Pointer to the file buffer used for buffered reading operations.
 * @param[in] skip If non-zero, the string data is skipped without being stored.
 *
 * @return char* Returns a pointer to the read null-terminated string on success, or NULL if an error occurred.
 * @retval NULL An error occurred during reading or memory allocation.
 * @retval Non-NULL Pointer to the read string.
 */
char *SDDS_ReadLZMABinaryString(struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer, int32_t skip) {
  int32_t length;
  char *string;

  if (!SDDS_LZMABufferedRead(&length, sizeof(length), lzmafp, fBuffer, SDDS_LONG, 0) || length < 0)
    return (0);
  if (!(string = SDDS_Malloc(sizeof(*string) * (length + 1))))
    return (NULL);
  if (length && !SDDS_LZMABufferedRead(skip ? NULL : string, sizeof(*string) * length, lzmafp, fBuffer, SDDS_STRING, 0))
    return (NULL);
  string[length] = 0;
  return (string);
}

#if defined(zLib)
/**
 * @brief Reads a binary string from a GZIP-compressed file with buffering.
 *
 * This function reads a binary string from the specified GZIP-compressed file by first reading the length
 * of the string and then reading the string content based on the length. If the 'skip' parameter is set,
 * the string data is skipped over instead of being stored. The function allocates memory for the string,
 * which should be freed by the caller when no longer needed.
 *
 * @param[in] gzfp The GZIP file pointer to read from. Must be an open GZIP-compressed file in read mode.
 * @param[in,out] fBuffer Pointer to the file buffer used for buffered reading operations.
 * @param[in] skip If non-zero, the string data is skipped without being stored.
 *
 * @return char* Returns a pointer to the read null-terminated string on success, or NULL if an error occurred.
 * @retval NULL An error occurred during reading or memory allocation.
 * @retval Non-NULL Pointer to the read string.
 */
char *SDDS_ReadGZipBinaryString(gzFile gzfp, SDDS_FILEBUFFER *fBuffer, int32_t skip) {
  int32_t length;
  char *string;

  if (!SDDS_GZipBufferedRead(&length, sizeof(length), gzfp, fBuffer, SDDS_LONG, 0) || length < 0)
    return (0);
  if (!(string = SDDS_Malloc(sizeof(*string) * (length + 1))))
    return (NULL);
  if (length && !SDDS_GZipBufferedRead(skip ? NULL : string, sizeof(*string) * length, gzfp, fBuffer, SDDS_STRING, 0))
    return (NULL);
  string[length] = 0;
  return (string);
}
#endif

/**
 * @brief Reads a binary row from the specified SDDS dataset.
 *
 * This function reads a single row of data from the given SDDS dataset. Depending on the dataset's configuration,
 * it handles uncompressed, LZMA-compressed, or GZIP-compressed files. For each column in the dataset, the function
 * reads the appropriate data type. If a column is of type string, it reads the string using the corresponding
 * string reading function. If the 'skip' parameter is set, the function skips reading the data without storing it.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 * @param[in] row The row number to read. Must be within the allocated range of rows in the dataset.
 * @param[in] skip If non-zero, the function skips reading the data for each column without storing it.
 *
 * @return int32_t Returns 1 on successful reading of the row, or 0 if an error occurred.
 * @retval 1 The row was successfully read and stored (or skipped).
 * @retval 0 An error occurred during reading, such as I/O errors or memory allocation failures.
 *
 * @note This function may modify the dataset's data structures by allocating memory for string columns.
 *       Ensure that the dataset is properly initialized and that memory is managed appropriately.
 */
int32_t SDDS_ReadBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row, int32_t skip) {
  int64_t i, type, size;
  SDDS_LAYOUT *layout;
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadBinaryRow"))
    return (0);
  layout = &SDDS_dataset->layout;
  fBuffer = &SDDS_dataset->fBuffer;

#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    for (i = 0; i < layout->n_columns; i++) {
      if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
        continue;
      if ((type = layout->column_definition[i].type) == SDDS_STRING) {
        if (!skip) {
          if (((char ***)SDDS_dataset->data)[i][row])
            free((((char ***)SDDS_dataset->data)[i][row]));
          if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadGZipBinaryString(gzfp, fBuffer, 0))) {
            SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadBinaryRows)");
            return (0);
          }
        } else {
          if (!SDDS_ReadGZipBinaryString(gzfp, fBuffer, 1)) {
            SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadBinaryRows)");
            return 0;
          }
        }
      } else {
        size = SDDS_type_size[type - 1];
        if (!SDDS_GZipBufferedRead(skip ? NULL : (char *)SDDS_dataset->data[i] + row * size, size, gzfp, fBuffer, type, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read row--failure reading value (SDDS_ReadBinaryRow)");
          return (0);
        }
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      for (i = 0; i < layout->n_columns; i++) {
        if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
          continue;
        if ((type = layout->column_definition[i].type) == SDDS_STRING) {
          if (!skip) {
            if (((char ***)SDDS_dataset->data)[i][row])
              free((((char ***)SDDS_dataset->data)[i][row]));
            if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadLZMABinaryString(lzmafp, fBuffer, 0))) {
              SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadBinaryRows)");
              return (0);
            }
          } else {
            if (!SDDS_ReadLZMABinaryString(lzmafp, fBuffer, 1)) {
              SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadBinaryRows)");
              return 0;
            }
          }
        } else {
          size = SDDS_type_size[type - 1];
          if (!SDDS_LZMABufferedRead(skip ? NULL : (char *)SDDS_dataset->data[i] + row * size, size, lzmafp, fBuffer, type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read row--failure reading value (SDDS_ReadBinaryRow)");
            return (0);
          }
        }
      }
    } else {
      fp = layout->fp;
      for (i = 0; i < layout->n_columns; i++) {
        if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
          continue;
        if ((type = layout->column_definition[i].type) == SDDS_STRING) {
          if (!skip) {
            if (((char ***)SDDS_dataset->data)[i][row])
              free((((char ***)SDDS_dataset->data)[i][row]));
            if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadBinaryString(fp, fBuffer, 0))) {
              SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadBinaryRows)");
              return (0);
            }
          } else {
            if (!SDDS_ReadBinaryString(fp, fBuffer, 1)) {
              SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadBinaryRows)");
              return 0;
            }
          }
        } else {
          size = SDDS_type_size[type - 1];
          if (!SDDS_BufferedRead(skip ? NULL : (char *)SDDS_dataset->data[i] + row * size, size, fp, fBuffer, type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read row--failure reading value (SDDS_ReadBinaryRow)");
            return (0);
          }
        }
      }
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Reads new binary rows from the SDDS dataset.
 *
 * This function updates the SDDS dataset by reading any new rows that have been added to the underlying file
 * since the last read operation. It verifies that the dataset is in a compatible binary format and ensures
 * that byte order and compression settings are supported. If the number of rows in the file exceeds the
 * currently allocated rows in memory, the function expands the dataset's internal storage to accommodate
 * the new rows.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 *
 * @return int32_t Returns the number of new rows successfully read on success, or -1 if an error occurred.
 * @retval >0 The number of new rows read and added to the dataset.
 * @retval -1 An error occurred during the read operation, such as unsupported file format,
 *             I/O errors, or memory allocation failures.
 *
 * @note This function does not support MPI parallel I/O, ASCII files, column-major order binary files,
 *       non-native byte orders, or compressed files (gzip or lzma). Attempts to use these features will
 *       result in an error.
 */
int32_t SDDS_ReadNewBinaryRows(SDDS_DATASET *SDDS_dataset) {
  int64_t row, offset, newRows = 0;
  int32_t rowsPresent32;
  int64_t rowsPresent;

#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io) {
    SDDS_SetError("Error: MPI mode not supported yet in SDDS_ReadNewBinaryRows");
    return -1;
  }
#endif
  if (SDDS_dataset->original_layout.data_mode.mode == SDDS_ASCII) {
    SDDS_SetError("Error: ASCII files not supported in SDDS_ReadNewBinaryRows");
    return -1;
  }
  if (SDDS_dataset->layout.data_mode.column_major) {
    SDDS_SetError("Error: column-major order binary files not supported in SDDS_ReadNewBinaryRows");
    return -1;
  }
  if (SDDS_dataset->swapByteOrder) {
    SDDS_SetError("Error: Non-native endian not supported yet in SDDS_ReadNewBinaryRows");
    return -1;
  }
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    SDDS_SetError("Error: gzip compressed files not supported yet in SDDS_ReadNewBinaryRows");
    return -1;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      SDDS_SetError("Error: lzma compressed files not supported yet in SDDS_ReadNewBinaryRows");
      return -1;
    }
#if defined(zLib)
  }
#endif

  // Read how many rows we have now
  offset = ftell(SDDS_dataset->layout.fp);
  fseek(SDDS_dataset->layout.fp, SDDS_dataset->rowcount_offset, 0);
  if (SDDS_dataset->layout.data_mode.mode == SDDS_BINARY) {
    if (fread(&rowsPresent32, sizeof(rowsPresent32), 1, SDDS_dataset->layout.fp) == 0) {
      SDDS_SetError("Error: row count not present or not correct length");
      return -1;
    }
    if (SDDS_dataset->swapByteOrder) {
      SDDS_SwapLong(&rowsPresent32);
    }
    if (rowsPresent32 == INT32_MIN) {
      if (fread(&rowsPresent, sizeof(rowsPresent), 1, SDDS_dataset->layout.fp) == 0) {
        SDDS_SetError("Error: row count not present or not correct length");
        return -1;
      }
      if (SDDS_dataset->swapByteOrder) {
        SDDS_SwapLong64(&rowsPresent);
      }
    } else {
      rowsPresent = rowsPresent32;
    }
  } else {
    char buffer[30];
    if (!fgets(buffer, 30, SDDS_dataset->layout.fp) || strlen(buffer) != 21 || sscanf(buffer, "%" SCNd64, &rowsPresent) != 1) {
      SDDS_SetError("Error: row count not present or not correct length");
      return -1;
    }
  }
  fseek(SDDS_dataset->layout.fp, offset, 0);

  // If the row count listed in the file is greather than the allocated rows, then lengthen the table in memory
  if (rowsPresent > SDDS_dataset->n_rows_allocated) {
    if (!SDDS_LengthenTable(SDDS_dataset, rowsPresent + 3)) {
      return -1;
    }
  }

  for (row = SDDS_dataset->n_rows; row < rowsPresent; row++) {
    if (!SDDS_ReadBinaryRow(SDDS_dataset, row, 0)) {
      if (SDDS_dataset->autoRecover) {
        row--;
        SDDS_dataset->autoRecovered = 1;
        SDDS_ClearErrors();
        break;
      }
      SDDS_SetError("Unable to read page--error reading data row");
      return -1;
    }
  }
  newRows = row + 1 - SDDS_dataset->n_rows;
  SDDS_dataset->n_rows = row + 1;
  return newRows;
}

/**
 * @brief Reads binary parameters from the specified SDDS dataset.
 *
 * This function iterates through all the parameters defined in the SDDS dataset layout and reads their values
 * from the underlying file. It handles different data types, including strings, and manages memory allocation
 * for string parameters. Depending on the dataset's compression settings (uncompressed, LZMA, or GZIP),
 * it uses the appropriate reading functions to retrieve the parameter values.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 *
 * @return int32_t Returns 1 on successfully reading all binary parameters, or 0 if an error occurred.
 * @retval 1 All parameters were successfully read and stored.
 * @retval 0 An error occurred during the read operation, such as I/O errors, data type mismatches, or memory allocation failures.
 *
 * @note Parameters with the 'fixed_value' attribute are handled by scanning the fixed value string
 *       instead of reading from the file. String parameters are dynamically allocated and should be
 *       freed by the caller when no longer needed.
 */
int32_t SDDS_ReadBinaryParameters(SDDS_DATASET *SDDS_dataset) {
  int32_t i;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  char buffer[SDDS_MAXLINE];
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadBinaryParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_parameters)
    return (1);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
    } else {
      fp = layout->fp;
    }
#if defined(zLib)
  }
#endif
  fBuffer = &SDDS_dataset->fBuffer;
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
      continue;
    if (layout->parameter_definition[i].fixed_value) {
      strcpy(buffer, layout->parameter_definition[i].fixed_value);
      if (!SDDS_ScanData(buffer, layout->parameter_definition[i].type, 0, SDDS_dataset->parameter[i], 0, 1)) {
        SDDS_SetError("Unable to read page--parameter scanning error (SDDS_ReadBinaryParameters)");
        return (0);
      }
    } else if (layout->parameter_definition[i].type == SDDS_STRING) {
      if (*(char **)SDDS_dataset->parameter[i])
        free(*(char **)SDDS_dataset->parameter[i]);
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!(*((char **)SDDS_dataset->parameter[i]) = SDDS_ReadGZipBinaryString(gzfp, fBuffer, 0))) {
          SDDS_SetError("Unable to read parameters--failure reading string (SDDS_ReadBinaryParameters)");
          return (0);
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!(*((char **)SDDS_dataset->parameter[i]) = SDDS_ReadLZMABinaryString(lzmafp, fBuffer, 0))) {
            SDDS_SetError("Unable to read parameters--failure reading string (SDDS_ReadBinaryParameters)");
            return (0);
          }
        } else {
          if (!(*((char **)SDDS_dataset->parameter[i]) = SDDS_ReadBinaryString(fp, fBuffer, 0))) {
            SDDS_SetError("Unable to read parameters--failure reading string (SDDS_ReadBinaryParameters)");
            return (0);
          }
        }
#if defined(zLib)
      }
#endif
    } else {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!SDDS_GZipBufferedRead(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], gzfp, fBuffer, layout->parameter_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read parameters--failure reading value (SDDS_ReadBinaryParameters)");
          return (0);
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!SDDS_LZMABufferedRead(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], lzmafp, fBuffer, layout->parameter_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read parameters--failure reading value (SDDS_ReadBinaryParameters)");
            return (0);
          }
        } else {
          if (!SDDS_BufferedRead(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], fp, fBuffer, layout->parameter_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read parameters--failure reading value (SDDS_ReadBinaryParameters)");
            return (0);
          }
        }
#if defined(zLib)
      }
#endif
    }
  }
  return (1);
}

/**
 * @brief Reads binary arrays from an SDDS dataset.
 *
 * This function iterates through all array definitions within the specified SDDS dataset and reads their
 * binary data from the underlying file. It handles various compression formats, including uncompressed,
 * LZMA-compressed, and GZIP-compressed files. For each array, the function reads its definition, dimensions,
 * and data elements, allocating and managing memory as necessary. String arrays are handled by reading
 * each string individually, while other data types are read in bulk.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 *
 * @return int32_t Returns 1 on successful reading of all arrays, or 0 if an error occurred.
 * @retval 1 All arrays were successfully read and stored.
 * @retval 0 An error occurred during the read operation, such as I/O failures, memory allocation issues,
 *             or corrupted array definitions.
 *
 * @note The caller is responsible for ensuring that the SDDS_dataset structure is properly initialized
 *       and that memory allocations for arrays are managed appropriately to prevent memory leaks.
 */
int32_t SDDS_ReadBinaryArrays(SDDS_DATASET *SDDS_dataset) {
  int32_t i, j;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  /*  static char buffer[SDDS_MAXLINE]; */
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;
  SDDS_ARRAY *array;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadBinaryArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_arrays)
    return (1);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
    } else {
      fp = layout->fp;
    }
#if defined(zLib)
  }
#endif
  fBuffer = &SDDS_dataset->fBuffer;
  if (!SDDS_dataset->array) {
    SDDS_SetError("Unable to read array--pointer to structure storage area is NULL (SDDS_ReadBinaryArrays)");
    return (0);
  }
  for (i = 0; i < layout->n_arrays; i++) {
    array = SDDS_dataset->array + i;
    if (array->definition && !SDDS_FreeArrayDefinition(array->definition)) {
      SDDS_SetError("Unable to get array--array definition corrupted (SDDS_ReadBinaryArrays)");
      return (0);
    }
    if (!SDDS_CopyArrayDefinition(&array->definition, layout->array_definition + i)) {
      SDDS_SetError("Unable to read array--definition copy failed (SDDS_ReadBinaryArrays)");
      return (0);
    }
    /*if (array->dimension) free(array->dimension); */
    if (!(array->dimension = SDDS_Realloc(array->dimension, sizeof(*array->dimension) * array->definition->dimensions))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_ReadBinaryArrays)");
      return (0);
    }
#if defined(zLib)
    if (SDDS_dataset->layout.gzipFile) {
      if (!SDDS_GZipBufferedRead(array->dimension, sizeof(*array->dimension) * array->definition->dimensions, gzfp, fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
        SDDS_SetError("Unable to read arrays--failure reading dimensions (SDDS_ReadBinaryArrays)");
        return (0);
      }
    } else {
#endif
      if (SDDS_dataset->layout.lzmaFile) {
        if (!SDDS_LZMABufferedRead(array->dimension, sizeof(*array->dimension) * array->definition->dimensions, lzmafp, fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read arrays--failure reading dimensions (SDDS_ReadBinaryArrays)");
          return (0);
        }
      } else {
        if (!SDDS_BufferedRead(array->dimension, sizeof(*array->dimension) * array->definition->dimensions, fp, fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read arrays--failure reading dimensions (SDDS_ReadBinaryArrays)");
          return (0);
        }
      }
#if defined(zLib)
    }
#endif
    array->elements = 1;
    for (j = 0; j < array->definition->dimensions; j++)
      array->elements *= array->dimension[j];
    if (array->data)
      free(array->data);
    array->data = array->pointer = NULL;
    if (array->elements == 0)
      continue;
    if (array->elements < 0) {
      SDDS_SetError("Unable to read array--number of elements is negative (SDDS_ReadBinaryArrays)");
      return (0);
    }
    if (!(array->data = SDDS_Realloc(array->data, array->elements * SDDS_type_size[array->definition->type - 1]))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_ReadBinaryArrays)");
      return (0);
    }
    if (array->definition->type == SDDS_STRING) {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        for (j = 0; j < array->elements; j++) {
          if (!(((char **)(array->data))[j] = SDDS_ReadGZipBinaryString(gzfp, fBuffer, 0))) {
            SDDS_SetError("Unable to read arrays--failure reading string (SDDS_ReadBinaryArrays)");
            return (0);
          }
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          for (j = 0; j < array->elements; j++) {
            if (!(((char **)(array->data))[j] = SDDS_ReadLZMABinaryString(lzmafp, fBuffer, 0))) {
              SDDS_SetError("Unable to read arrays--failure reading string (SDDS_ReadBinaryArrays)");
              return (0);
            }
          }
        } else {
          for (j = 0; j < array->elements; j++) {
            if (!(((char **)(array->data))[j] = SDDS_ReadBinaryString(fp, fBuffer, 0))) {
              SDDS_SetError("Unable to read arrays--failure reading string (SDDS_ReadBinaryArrays)");
              return (0);
            }
          }
        }
#if defined(zLib)
      }
#endif
    } else {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!SDDS_GZipBufferedRead(array->data, SDDS_type_size[array->definition->type - 1] * array->elements, gzfp, fBuffer, array->definition->type, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read arrays--failure reading values (SDDS_ReadBinaryArrays)");
          return (0);
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!SDDS_LZMABufferedRead(array->data, SDDS_type_size[array->definition->type - 1] * array->elements, lzmafp, fBuffer, array->definition->type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read arrays--failure reading values (SDDS_ReadBinaryArrays)");
            return (0);
          }
        } else {
          if (!SDDS_BufferedRead(array->data, SDDS_type_size[array->definition->type - 1] * array->elements, fp, fBuffer, array->definition->type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read arrays--failure reading values (SDDS_ReadBinaryArrays)");
            return (0);
          }
        }
#if defined(zLib)
      }
#endif
    }
  }
  return (1);
}

/**
 * @brief Reads the binary columns from an SDDS dataset.
 *
 * This function iterates through all column definitions within the specified SDDS dataset and reads their
 * binary data from the underlying file. It handles various compression formats, including uncompressed,
 * LZMA-compressed, and GZIP-compressed files. For each column, the function reads data for each row,
 * managing memory allocation for string columns as necessary. Non-string data types are read in bulk for
 * each column.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 *
 * @return int32_t Returns 1 on successful reading of all columns, or 0 if an error occurred.
 * @retval 1 All columns were successfully read and stored.
 * @retval 0 An error occurred during the read operation, such as I/O failures, memory allocation issues,
 *             or corrupted column definitions.
 *
 * @note The caller is responsible for ensuring that the SDDS_dataset structure is properly initialized
 *       and that memory allocations for columns are managed appropriately to prevent memory leaks.
 */
int32_t SDDS_ReadBinaryColumns(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset) {
  int64_t i, j, k, row;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  /*  static char buffer[SDDS_MAXLINE]; */
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadBinaryColumns"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_columns || !SDDS_dataset->n_rows)
    return (1);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
    } else {
      fp = layout->fp;
    }
#if defined(zLib)
  }
#endif
  fBuffer = &SDDS_dataset->fBuffer;

  for (i = 0; i < layout->n_columns; i++) {
    if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
      continue;
    if (layout->column_definition[i].type == SDDS_STRING) {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        for (row = 0; row < SDDS_dataset->n_rows; row++) {
          if (((char ***)SDDS_dataset->data)[i][row])
            free((((char ***)SDDS_dataset->data)[i][row]));
          if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadGZipBinaryString(gzfp, fBuffer, 0))) {
            SDDS_SetError("Unable to read columns--failure reading string (SDDS_ReadBinaryColumns)");
            return (0);
          }
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (((char ***)SDDS_dataset->data)[i][row])
              free((((char ***)SDDS_dataset->data)[i][row]));
            if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadLZMABinaryString(lzmafp, fBuffer, 0))) {
              SDDS_SetError("Unable to read columns--failure reading string (SDDS_ReadBinaryColumms)");
              return (0);
            }
          }
        } else {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (((char ***)SDDS_dataset->data)[i][row])
              free((((char ***)SDDS_dataset->data)[i][row]));
            if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadBinaryString(fp, fBuffer, 0))) {
              SDDS_SetError("Unable to read columns--failure reading string (SDDS_ReadBinaryColumms)");
              return (0);
            }
          }
        }
#if defined(zLib)
      }
#endif
    } else {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!SDDS_GZipBufferedRead(SDDS_dataset->data[i], SDDS_type_size[layout->column_definition[i].type - 1] * SDDS_dataset->n_rows, gzfp, fBuffer, layout->column_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read columns--failure reading values (SDDS_ReadBinaryColumns)");
          return (0);
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!SDDS_LZMABufferedRead(SDDS_dataset->data[i], SDDS_type_size[layout->column_definition[i].type - 1] * SDDS_dataset->n_rows, lzmafp, fBuffer, layout->column_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read columns--failure reading values (SDDS_ReadBinaryColumns)");
            return (0);
          }
        } else {
          if (!SDDS_BufferedRead(SDDS_dataset->data[i], SDDS_type_size[layout->column_definition[i].type - 1] * SDDS_dataset->n_rows, fp, fBuffer, layout->column_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read columns--failure reading values (SDDS_ReadBinaryColumns)");
            return (0);
          }
        }
#if defined(zLib)
      }
#endif
    }
  }

  if (sparse_interval == 1 && sparse_offset == 0) {
    return(1);
  }

  j = SDDS_dataset->n_rows;
  for (i = 0; i < layout->n_columns; i++) {
    j = k = 0;
    switch (layout->column_definition[i].type) {
    case SDDS_SHORT:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((short*)SDDS_dataset->data[i])[j] = ((short*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_USHORT:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((unsigned short*)SDDS_dataset->data[i])[j] = ((unsigned short*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_LONG:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((int32_t*)SDDS_dataset->data[i])[j] = ((int32_t*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_ULONG:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((uint32_t*)SDDS_dataset->data[i])[j] = ((uint32_t*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_LONG64:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((int64_t*)SDDS_dataset->data[i])[j] = ((int64_t*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_ULONG64:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((uint64_t*)SDDS_dataset->data[i])[j] = ((uint64_t*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_FLOAT:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((float*)SDDS_dataset->data[i])[j] = ((float*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_DOUBLE:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((double*)SDDS_dataset->data[i])[j] = ((double*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_LONGDOUBLE:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((long double*)SDDS_dataset->data[i])[j] = ((long double*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    case SDDS_STRING:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((char**)SDDS_dataset->data[i])[j] = ((char**)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      for (k=j; k<SDDS_dataset->n_rows; k++) {
        if (((char ***)SDDS_dataset->data)[i][k]) {
          free((((char ***)SDDS_dataset->data)[i][k]));
          ((char ***)SDDS_dataset->data)[i][k] = NULL;
        }
      }

      break; 
    case SDDS_CHARACTER:
      for (row = sparse_offset; row < SDDS_dataset->n_rows; row++) {
        if (k % sparse_interval == 0) {
          ((char*)SDDS_dataset->data[i])[j] = ((char*)SDDS_dataset->data[i])[row];
          j++;
        }
        k++;
      }
      break; 
    default:
      break;
    }
  }
    
  SDDS_dataset->n_rows = j;

  return (1);
}

/**
 * @brief Reads the non-native endian binary columns from an SDDS dataset.
 *
 * This function is similar to SDDS_ReadBinaryColumns but specifically handles columns with non-native
 * endianness. It iterates through all column definitions within the specified SDDS dataset and reads
 * their binary data from the underlying file, ensuring that the byte order is correctly swapped
 * to match the system's native endianness. The function supports various compression formats,
 * including uncompressed, LZMA-compressed, and GZIP-compressed files.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 *
 * @return int32_t Returns 1 on successful reading and byte-swapping of all columns, or 0 if an error occurred.
 * @retval 1 All non-native endian columns were successfully read and byte-swapped.
 * @retval 0 An error occurred during the read or byte-swapping operation, such as I/O failures,
 *             memory allocation issues, or corrupted column definitions.
 *
 * @note This function assumes that the dataset's byte order has been declared and that the
 *       underlying file's byte order differs from the system's native byte order. Proper
 *       initialization and configuration of the SDDS_dataset structure are required before
 *       calling this function.
 */
int32_t SDDS_ReadNonNativeBinaryColumns(SDDS_DATASET *SDDS_dataset) {
  int64_t i, row;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  /*  static char buffer[SDDS_MAXLINE]; */
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadNonNativeBinaryColumns"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_columns || !SDDS_dataset->n_rows)
    return (1);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
    } else {
      fp = layout->fp;
    }
#if defined(zLib)
  }
#endif
  fBuffer = &SDDS_dataset->fBuffer;

  for (i = 0; i < layout->n_columns; i++) {
    if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
      continue;
    if (layout->column_definition[i].type == SDDS_STRING) {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        for (row = 0; row < SDDS_dataset->n_rows; row++) {
          if (((char ***)SDDS_dataset->data)[i][row])
            free((((char ***)SDDS_dataset->data)[i][row]));
          if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadNonNativeGZipBinaryString(gzfp, fBuffer, 0))) {
            SDDS_SetError("Unable to read columns--failure reading string (SDDS_ReadNonNativeBinaryColumns)");
            return (0);
          }
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (((char ***)SDDS_dataset->data)[i][row])
              free((((char ***)SDDS_dataset->data)[i][row]));
            if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadNonNativeLZMABinaryString(lzmafp, fBuffer, 0))) {
              SDDS_SetError("Unable to read columns--failure reading string (SDDS_ReadNonNativeBinaryColumms)");
              return (0);
            }
          }
        } else {
          for (row = 0; row < SDDS_dataset->n_rows; row++) {
            if (((char ***)SDDS_dataset->data)[i][row])
              free((((char ***)SDDS_dataset->data)[i][row]));
            if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadNonNativeBinaryString(fp, fBuffer, 0))) {
              SDDS_SetError("Unable to read columns--failure reading string (SDDS_ReadNonNativeBinaryColumms)");
              return (0);
            }
          }
        }
#if defined(zLib)
      }
#endif
    } else {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!SDDS_GZipBufferedRead(SDDS_dataset->data[i], SDDS_type_size[layout->column_definition[i].type - 1] * SDDS_dataset->n_rows, gzfp, fBuffer, layout->column_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read columns--failure reading values (SDDS_ReadNonNativeBinaryColumns)");
          return (0);
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!SDDS_LZMABufferedRead(SDDS_dataset->data[i], SDDS_type_size[layout->column_definition[i].type - 1] * SDDS_dataset->n_rows, lzmafp, fBuffer, layout->column_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read columns--failure reading values (SDDS_ReadNonNativeBinaryColumns)");
            return (0);
          }
        } else {
          if (!SDDS_BufferedRead(SDDS_dataset->data[i], SDDS_type_size[layout->column_definition[i].type - 1] * SDDS_dataset->n_rows, fp, fBuffer, layout->column_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read columns--failure reading values (SDDS_ReadNonNativeBinaryColumns)");
            return (0);
          }
        }
#if defined(zLib)
      }
#endif
    }
  }
  return (1);
}

/**
 * @brief Swaps the endianness of the column data in an SDDS dataset.
 *
 * This function iterates through all columns in the specified SDDS dataset and swaps the byte order
 * of each data element to match the system's native endianness. It supports various data types,
 * including short, unsigned short, long, unsigned long, long long, unsigned long long, float,
 * double, and long double. The function ensures that binary data is correctly interpreted on systems
 * with different byte orders.
 *
 * @param[in,out] SDDSin Pointer to the SDDS_DATASET structure representing the dataset whose
 *                     column data endianness is to be swapped.
 *
 * @return int32_t Always returns 1.
 * @retval 1 The endianness of all applicable column data elements was successfully swapped.
 *
 * @note This function modifies the dataset's column data in place. It should be called only when
 *       the dataset's byte order is known to differ from the system's native byte order.
 *       String data types are not affected by this function.
 */
int32_t SDDS_SwapEndsColumnData(SDDS_DATASET *SDDSin) {
  int32_t i, row;
  SDDS_LAYOUT *layout;
  short *sData;
  unsigned short *suData;
  int32_t *lData;
  uint32_t *luData;
  int64_t *lData64;
  uint64_t *luData64;
  float *fData;
  double *dData;
  long double *ldData;

  layout = &SDDSin->layout;
  for (i = 0; i < layout->n_columns; i++) {
    switch (layout->column_definition[i].type) {
    case SDDS_SHORT:
      sData = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapShort(sData + row);
      break;
    case SDDS_USHORT:
      suData = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapUShort(suData + row);
      break;
    case SDDS_LONG:
      lData = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapLong(lData + row);
      break;
    case SDDS_ULONG:
      luData = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapULong(luData + row);
      break;
    case SDDS_LONG64:
      lData64 = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapLong64(lData64 + row);
      break;
    case SDDS_ULONG64:
      luData64 = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapULong64(luData64 + row);
      break;
    case SDDS_LONGDOUBLE:
      ldData = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapLongDouble(ldData + row);
      break;
    case SDDS_DOUBLE:
      dData = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapDouble(dData + row);
      break;
    case SDDS_FLOAT:
      fData = SDDSin->data[i];
      for (row = 0; row < SDDSin->n_rows; row++)
        SDDS_SwapFloat(fData + row);
      break;
    default:
      break;
    }
  }
  return (1);
}

/**
 * @brief Swaps the endianness of the parameter data in an SDDS dataset.
 *
 * This function iterates through all parameters in the specified SDDS dataset and swaps the byte order
 * of each data element to match the system's native endianness. It handles various data types, including
 * short, unsigned short, long, unsigned long, long long, unsigned long long, float, double, and
 * long double. Parameters with fixed values are skipped as their byte order is already consistent.
 *
 * @param[in,out] SDDSin Pointer to the SDDS_DATASET structure representing the dataset whose
 *                     parameter data endianness is to be swapped.
 *
 * @return int32_t Always returns 1.
 * @retval 1 The endianness of all applicable parameter data elements was successfully swapped.
 *
 * @note This function modifies the dataset's parameter data in place. It should be called only when
 *       the dataset's byte order is known to differ from the system's native byte order.
 *       String data types and parameters with fixed values are not affected by this function.
 */
int32_t SDDS_SwapEndsParameterData(SDDS_DATASET *SDDSin) {
  int32_t i;
  SDDS_LAYOUT *layout;
  short *sData;
  unsigned short *suData;
  int32_t *lData;
  uint32_t *luData;
  int64_t *lData64;
  uint64_t *luData64;
  float *fData;
  double *dData;
  long double *ldData;

  layout = &SDDSin->layout;
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].fixed_value) {
      continue;
    }
    switch (layout->parameter_definition[i].type) {
    case SDDS_SHORT:
      sData = SDDSin->parameter[i];
      SDDS_SwapShort(sData);
      break;
    case SDDS_USHORT:
      suData = SDDSin->parameter[i];
      SDDS_SwapUShort(suData);
      break;
    case SDDS_LONG:
      lData = SDDSin->parameter[i];
      SDDS_SwapLong(lData);
      break;
    case SDDS_ULONG:
      luData = SDDSin->parameter[i];
      SDDS_SwapULong(luData);
      break;
    case SDDS_LONG64:
      lData64 = SDDSin->parameter[i];
      SDDS_SwapLong64(lData64);
      break;
    case SDDS_ULONG64:
      luData64 = SDDSin->parameter[i];
      SDDS_SwapULong64(luData64);
      break;
    case SDDS_LONGDOUBLE:
      ldData = SDDSin->parameter[i];
      SDDS_SwapLongDouble(ldData);
      break;
    case SDDS_DOUBLE:
      dData = SDDSin->parameter[i];
      SDDS_SwapDouble(dData);
      break;
    case SDDS_FLOAT:
      fData = SDDSin->parameter[i];
      SDDS_SwapFloat(fData);
      break;
    default:
      break;
    }
  }
  return (1);
}

/**
 * @brief Swaps the endianness of the array data in an SDDS dataset.
 *
 * This function iterates through all arrays defined in the specified SDDS dataset and swaps the byte order
 * of each element to match the system's native endianness. It supports various data types including
 * short, unsigned short, long, unsigned long, long long, unsigned long long, float, double, and long double.
 * The function ensures that binary data is correctly interpreted on systems with different byte orders.
 *
 * @param[in,out] SDDSin Pointer to the SDDS_DATASET structure representing the dataset whose
 *                      array data endianness is to be swapped.
 *
 * @return int32_t Always returns 1.
 * @retval 1 The endianness of all applicable array data elements was successfully swapped.
 *
 * @note This function modifies the dataset's array data in place. It should be called only when
 *       the dataset's byte order is known to differ from the system's native byte order.
 */
int32_t SDDS_SwapEndsArrayData(SDDS_DATASET *SDDSin) {
  int32_t i, j;
  SDDS_LAYOUT *layout;
  short *sData;
  unsigned short *suData;
  int32_t *lData;
  uint32_t *luData;
  int64_t *lData64;
  uint64_t *luData64;
  float *fData;
  double *dData;
  long double *ldData;

  layout = &SDDSin->layout;

  for (i = 0; i < layout->n_arrays; i++) {
    switch (layout->array_definition[i].type) {
    case SDDS_SHORT:
      sData = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapShort(sData + j);
      break;
    case SDDS_USHORT:
      suData = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapUShort(suData + j);
      break;
    case SDDS_LONG:
      lData = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapLong(lData + j);
      break;
    case SDDS_ULONG:
      luData = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapULong(luData + j);
      break;
    case SDDS_LONG64:
      lData64 = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapLong64(lData64 + j);
      break;
    case SDDS_ULONG64:
      luData64 = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapULong64(luData64 + j);
      break;
    case SDDS_LONGDOUBLE:
      ldData = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapLongDouble(ldData + j);
      break;
    case SDDS_DOUBLE:
      dData = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapDouble(dData + j);
      break;
    case SDDS_FLOAT:
      fData = SDDSin->array[i].data;
      for (j = 0; j < SDDSin->array[i].elements; j++)
        SDDS_SwapFloat(fData + j);
      break;
    default:
      break;
    }
  }
  return (1);
}

/**
 * @brief Swaps the endianness of a short integer.
 *
 * This function swaps the byte order of a 16-bit short integer pointed to by the provided data pointer.
 * It effectively converts the data between little-endian and big-endian formats.
 *
 * @param[in,out] data Pointer to the short integer whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned 16-bit short integer.
 */
void SDDS_SwapShort(short *data) {
  unsigned char c1;
  c1 = *((char *)data);
  *((char *)data) = *(((char *)data) + 1);
  *(((char *)data) + 1) = c1;
}

/**
 * @brief Swaps the endianness of an unsigned short integer.
 *
 * This function swaps the byte order of a 16-bit unsigned short integer pointed to by the provided data pointer.
 * It effectively converts the data between little-endian and big-endian formats.
 *
 * @param[in,out] data Pointer to the unsigned short integer whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned 16-bit unsigned short integer.
 */
void SDDS_SwapUShort(unsigned short *data) {
  unsigned char c1;
  c1 = *((char *)data);
  *((char *)data) = *(((char *)data) + 1);
  *(((char *)data) + 1) = c1;
}

/**
 * @brief Swaps the endianness of a 32-bit integer.
 *
 * This function swaps the byte order of a 32-bit integer pointed to by the provided data pointer.
 * It effectively converts the data between little-endian and big-endian formats.
 *
 * @param[in,out] data Pointer to the 32-bit integer whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned 32-bit integer.
 */
void SDDS_SwapLong(int32_t *data) {
  int32_t copy;
  short i, j;
  copy = *data;
  for (i = 0, j = 3; i < 4; i++, j--)
    *(((char *)data) + i) = *(((char *)&copy) + j);
}

/**
 * @brief Swaps the endianness of a 32-bit unsigned integer.
 *
 * This function swaps the byte order of a 32-bit unsigned integer pointed to by the provided data pointer.
 * It effectively converts the data between little-endian and big-endian formats.
 *
 * @param[in,out] data Pointer to the 32-bit unsigned integer whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned 32-bit unsigned integer.
 */
void SDDS_SwapULong(uint32_t *data) {
  uint32_t copy;
  short i, j;
  copy = *data;
  for (i = 0, j = 3; i < 4; i++, j--)
    *(((char *)data) + i) = *(((char *)&copy) + j);
}

/**
 * @brief Swaps the endianness of a 64-bit integer.
 *
 * This function swaps the byte order of a 64-bit integer pointed to by the provided data pointer.
 * It effectively converts the data between little-endian and big-endian formats.
 *
 * @param[in,out] data Pointer to the 64-bit integer whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned 64-bit integer.
 */
void SDDS_SwapLong64(int64_t *data) {
  int64_t copy;
  short i, j;
  copy = *data;
  for (i = 0, j = 7; i < 8; i++, j--)
    *(((char *)data) + i) = *(((char *)&copy) + j);
}

/**
 * @brief Swaps the endianness of a 64-bit unsigned integer.
 *
 * This function swaps the byte order of a 64-bit unsigned integer pointed to by the provided data pointer.
 * It effectively converts the data between little-endian and big-endian formats.
 *
 * @param[in,out] data Pointer to the 64-bit unsigned integer whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned 64-bit unsigned integer.
 */
void SDDS_SwapULong64(uint64_t *data) {
  uint64_t copy;
  short i, j;
  copy = *data;
  for (i = 0, j = 7; i < 8; i++, j--)
    *(((char *)data) + i) = *(((char *)&copy) + j);
}

/**
 * @brief Swaps the endianness of a float.
 *
 * This function swaps the byte order of a 32-bit floating-point number pointed to by the provided data pointer.
 * It effectively converts the data between little-endian and big-endian formats.
 *
 * @param[in,out] data Pointer to the float whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned float.
 */
void SDDS_SwapFloat(float *data) {
  float copy;
  short i, j;
  copy = *data;
  for (i = 0, j = 3; i < 4; i++, j--)
    *(((char *)data) + i) = *(((char *)&copy) + j);
}

/**
 * @brief Swaps the endianness of a double.
 *
 * This function swaps the byte order of a 64-bit double-precision floating-point number pointed to by the
 * provided data pointer. It effectively converts the data between little-endian and big-endian formats.
 *
 * @param[in,out] data Pointer to the double whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned double.
 */
void SDDS_SwapDouble(double *data) {
  double copy;
  short i, j;
  copy = *data;
  for (i = 0, j = 7; i < 8; i++, j--)
    *(((char *)data) + i) = *(((char *)&copy) + j);
}

/**
 * @brief Swaps the endianness of a long double.
 *
 * This function swaps the byte order of a long double floating-point number pointed to by the provided data pointer.
 * It effectively converts the data between little-endian and big-endian formats. The function accounts for
 * different sizes of long double based on the system's architecture.
 *
 * @param[in,out] data Pointer to the long double whose byte order is to be swapped.
 *
 * @note The function modifies the data in place. Ensure that the pointer is valid and points to a
 *       properly aligned long double. The size of long double may vary between different systems.
 */
void SDDS_SwapLongDouble(long double *data) {
  long double copy;
  short i, j;
  copy = *data;
  if (LDBL_DIG == 18) {
    for (i = 0, j = 11; i < 12; i++, j--)
      *(((char *)data) + i) = *(((char *)&copy) + j);
  } else {
    for (i = 0, j = 7; i < 8; i++, j--)
      *(((char *)data) + i) = *(((char *)&copy) + j);
  }
}

/**
 * @brief Reads a non-native endian page from an SDDS dataset.
 *
 * This function reads a page of data from the specified SDDS dataset, handling data with non-native
 * endianness. It supports both ASCII and binary data modes, performing necessary byte order
 * conversions to ensure correct data interpretation on the host system.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 *
 * @return int32_t Returns the number of rows read on success, or 0 on failure.
 * @retval >0 Number of rows successfully read.
 * @retval 0 An error occurred during the read operation, such as I/O failures, data corruption,
 *            or unsupported data modes.
 *
 * @note This function is a wrapper for SDDS_ReadNonNativePageDetailed with default parameters.
 *       It should be used when no specific mode, sparse interval, or offset is required.
 */
int32_t SDDS_ReadNonNativePage(SDDS_DATASET *SDDS_dataset) {
  return SDDS_ReadNonNativePageDetailed(SDDS_dataset, 0, 1, 0, 0);
}
/**
 * @brief Reads a sparse non-native endian page from an SDDS dataset.
 *
 * This function reads a sparse page of data from the specified SDDS dataset, handling data with non-native
 * endianness. Sparse reading allows for selective row retrieval based on the provided interval and offset.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 * @param[in] mode Mode flag to support future expansion.
 * @param[in] sparse_interval Interval between rows to be read for sparsity.
 * @param[in] sparse_offset Offset to start reading rows for sparsity.
 *
 * @return int32_t Returns the number of rows read on success, or 0 on failure.
 * @retval >0 Number of rows successfully read.
 * @retval 0 An error occurred during the read operation, such as I/O failures, data corruption,
 *            or unsupported data modes.
 *
 * @note This function is a wrapper for SDDS_ReadNonNativePageDetailed with specific parameters
 *       to enable sparse reading. It should be used when selective row retrieval is desired.
 */
int32_t SDDS_ReadNonNativePageSparse(SDDS_DATASET *SDDS_dataset, uint32_t mode, int64_t sparse_interval, int64_t sparse_offset) {
  return SDDS_ReadNonNativePageDetailed(SDDS_dataset, mode, sparse_interval, sparse_offset, 0);
}

/**
 * @brief Reads a detailed non-native endian page from an SDDS dataset.
 *
 * This function reads a page of data from the specified SDDS dataset, handling data with non-native
 * endianness. It supports both ASCII and binary data modes, performing necessary byte order
 * conversions to ensure correct data interpretation on the host system. Additionally, it allows
 * for sparse reading and reading of the last few rows based on the provided parameters.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 * @param[in] mode Mode flag to support future expansion.
 * @param[in] sparse_interval Interval between rows to be read for sparsity.
 * @param[in] sparse_offset Offset to start reading rows for sparsity.
 * @param[in] last_rows Number of last rows to read from the dataset.
 *
 * @return int32_t Returns the number of rows read on success, or 0 on failure.
 * @retval >0 Number of rows successfully read.
 * @retval 0 An error occurred during the read operation, such as I/O failures, data corruption,
 *            or unsupported data modes.
 *
 * @note This function handles various compression formats, including uncompressed, LZMA-compressed,
 *       and GZIP-compressed files. It manages memory allocation for parameters, arrays, and columns,
 *       ensuring that data is correctly stored and byte-swapped as necessary.
 */
int32_t SDDS_ReadNonNativePageDetailed(SDDS_DATASET *SDDS_dataset, uint32_t mode, int64_t sparse_interval, int64_t sparse_offset, int64_t last_rows)
/* the mode argument is to support future expansion */
{
  int32_t retval;
  /*  SDDS_LAYOUT layout_copy; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadNonNativePageDetailed"))
    return (0);
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't read page--file is disconnected (SDDS_ReadNonNativePageDetailed)");
    return 0;
  }
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_dataset->layout.gzfp) {
      SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadNonNativePageDetailed)");
      return (0);
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_dataset->layout.lzmafp) {
        SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadNonNativePageDetailed)");
        return (0);
      }
    } else {
      if (!SDDS_dataset->layout.fp) {
        SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadNonNativePageDetailed)");
        return (0);
      }
    }
#if defined(zLib)
  }
#endif
  if (SDDS_dataset->original_layout.data_mode.mode == SDDS_ASCII) {
    if ((retval = SDDS_ReadAsciiPage(SDDS_dataset, sparse_interval, sparse_offset, 0)) < 1) {
      return (retval);
    }
  } else if (SDDS_dataset->original_layout.data_mode.mode == SDDS_BINARY) {
    if ((retval = SDDS_ReadNonNativeBinaryPage(SDDS_dataset, sparse_interval, sparse_offset)) < 1) {
      return (retval);
    }
  } else {
    SDDS_SetError("Unable to read page--unrecognized data mode (SDDS_ReadNonNativePageDetailed)");
    return (0);
  }
  return (retval);
}

/**
 * @brief Reads the last few rows from a non-native endian page in an SDDS dataset.
 *
 * This function reads the specified number of last rows from the non-native endian page of the
 * given SDDS dataset. It handles data with non-native endianness, performing necessary byte order
 * conversions to ensure correct data interpretation on the host system.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 * @param[in] last_rows Number of last rows to read from the dataset.
 *
 * @return int32_t Returns the number of rows read on success, or 0 on failure.
 * @retval >0 Number of rows successfully read.
 * @retval 0 An error occurred during the read operation, such as I/O failures, data corruption,
 *            or unsupported data modes.
 *
 * @note This function is a wrapper for SDDS_ReadNonNativePageDetailed with specific parameters
 *       to read the last few rows. It should be used when only the most recent rows are needed.
 */
int32_t SDDS_ReadNonNativePageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows) {
  int32_t retval;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadNonNativePageLastRows"))
    return (0);
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't read page--file is disconnected (SDDS_ReadNonNativePageLastRows)");
    return 0;
  }
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_dataset->layout.gzfp) {
      SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadNonNativePageLastRows)");
      return (0);
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_dataset->layout.lzmafp) {
        SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadNonNativePageLastRows)");
        return (0);
      }
    } else {
      if (!SDDS_dataset->layout.fp) {
        SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadNonNativePageLastRows)");
        return (0);
      }
    }
#if defined(zLib)
  }
#endif
  if (SDDS_dataset->original_layout.data_mode.mode == SDDS_ASCII) {
    if ((retval = SDDS_ReadAsciiPageLastRows(SDDS_dataset, last_rows)) < 1) {
      return (retval);
    }
  } else if (SDDS_dataset->original_layout.data_mode.mode == SDDS_BINARY) {
    if ((retval = SDDS_ReadNonNativeBinaryPageLastRows(SDDS_dataset, last_rows)) < 1) {
      return (retval);
    }
  } else {
    SDDS_SetError("Unable to read page--unrecognized data mode (SDDS_ReadNonNativePageLastRows)");
    return (0);
  }
  return (retval);
}

/**
 * @brief Reads a non-native endian binary page from an SDDS dataset.
 *
 * This function reads a binary page from the specified SDDS dataset, handling data with non-native
 * endianness. It performs necessary byte order conversions to ensure correct data interpretation on
 * the host system. The function supports sparse reading based on the provided interval and offset.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 * @param[in] sparse_interval Interval between rows to be read for sparsity.
 * @param[in] sparse_offset Offset to start reading rows for sparsity.
 *
 * @return int32_t Returns the number of rows read on success, or 0 on failure.
 * @retval >0 Number of rows successfully read.
 * @retval 0 An error occurred during the read operation, such as I/O failures, data corruption,
 *            or unsupported data modes.
 *
 * @note This function is a wrapper for SDDS_ReadNonNativeBinaryPageDetailed with specific parameters.
 */
int32_t SDDS_ReadNonNativeBinaryPage(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset) {
  return SDDS_ReadNonNativeBinaryPageDetailed(SDDS_dataset, sparse_interval, sparse_offset, 0);
}

/**
 * @brief Reads the last few rows from a non-native endian binary page in an SDDS dataset.
 *
 * This function reads the specified number of last rows from a binary page in the given SDDS dataset,
 * handling data with non-native endianness. It performs necessary byte order conversions to ensure
 * correct data interpretation on the host system.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 * @param[in] last_rows Number of last rows to read from the dataset.
 *
 * @return int32_t Returns the number of rows read on success, or 0 on failure.
 * @retval >0 Number of rows successfully read.
 * @retval 0 An error occurred during the read operation, such as I/O failures, data corruption,
 *            or unsupported data modes.
 *
 * @note This function is a wrapper for SDDS_ReadNonNativeBinaryPageDetailed with specific parameters
 *       to read the last few rows. It should be used when only the most recent rows are needed.
 */
int32_t SDDS_ReadNonNativeBinaryPageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows) {
  return SDDS_ReadNonNativeBinaryPageDetailed(SDDS_dataset, 1, 0, last_rows);
}

/**
 * @brief Reads a detailed non-native endian binary page from an SDDS dataset.
 *
 * This function reads a binary page from the specified SDDS dataset, handling data with non-native
 * endianness. It supports both sparse reading and reading of the last few rows based on the provided
 * parameters. The function performs necessary byte order conversions to ensure correct data interpretation
 * on the host system.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 * @param[in] sparse_interval Interval between rows to be read for sparsity.
 * @param[in] sparse_offset Offset to start reading rows for sparsity.
 * @param[in] last_rows Number of last rows to read from the dataset.
 *
 * @return int32_t Returns the page number on success, or 0 on failure.
 * @retval >0 Page number successfully read.
 * @retval 0 An error occurred during the read operation, such as I/O failures, data corruption,
 *            or unsupported data modes.
 *
 * @note This function handles various compression formats, including uncompressed, LZMA-compressed,
 *       and GZIP-compressed files. It manages memory allocation for parameters, arrays, and columns,
 *       ensuring that data is correctly stored and byte-swapped as necessary.
 *       The function also updates the dataset's row count and handles auto-recovery in case of errors.
 */
int32_t SDDS_ReadNonNativeBinaryPageDetailed(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int64_t last_rows) {
  int32_t n_rows32 = 0;
  int64_t n_rows, j, k, alloc_rows, rows_to_store, mod;
  /*  int32_t page_number, i; */
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;
  SDDS_FILEBUFFER *fBuffer;

  /*  static char s[SDDS_MAXLINE]; */
  n_rows = 0;
  SDDS_SetReadRecoveryMode(SDDS_dataset, 0);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = SDDS_dataset->layout.gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = SDDS_dataset->layout.lzmafp;
    } else {
      fp = SDDS_dataset->layout.fp;
    }
#if defined(zLib)
  }
#endif
  fBuffer = &SDDS_dataset->fBuffer;
  if (!fBuffer->buffer) {
    if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * defaultIOBufferSize))) {
      SDDS_SetError("Unable to do buffered read--allocation failure");
      return 0;
    }
    fBuffer->bufferSize = defaultIOBufferSize;
    fBuffer->bytesLeft = 0;
  }
  SDDS_dataset->rowcount_offset = -1;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_GZipBufferedRead(&n_rows32, sizeof(n_rows32), gzfp, &SDDS_dataset->fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
      if (gzeof(gzfp))
        return (SDDS_dataset->page_number = -1);
      SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadNonNativeBinaryPage)");
      return (0);
    }
    SDDS_SwapLong(&n_rows32);
    if (n_rows32 == INT32_MIN) {
      if (!SDDS_GZipBufferedRead(&n_rows, sizeof(n_rows), gzfp, &SDDS_dataset->fBuffer, SDDS_LONG64, SDDS_dataset->layout.byteOrderDeclared)) {
        if (gzeof(gzfp))
          return (SDDS_dataset->page_number = -1);
        SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadNonNativeBinaryPage)");
        return (0);
      }
      SDDS_SwapLong64(&n_rows);
    } else {
      n_rows = n_rows32;
    }
  } else {
#endif
    /* This value will only be valid if read buffering is turned off, which is done for
     * certain append operations!   Should really modify SDDS_BufferedRead and SDDS_BufferedWrite
     * to provide ftell capability.
     */
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_LZMABufferedRead(&n_rows32, sizeof(n_rows32), lzmafp, &SDDS_dataset->fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
        if (lzma_eof(lzmafp))
          return (SDDS_dataset->page_number = -1);
        SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadNonNativeBinaryPage)");
        return (0);
      }
      SDDS_SwapLong(&n_rows32);
      if (n_rows32 == INT32_MIN) {
        if (!SDDS_LZMABufferedRead(&n_rows, sizeof(n_rows), lzmafp, &SDDS_dataset->fBuffer, SDDS_LONG64, SDDS_dataset->layout.byteOrderDeclared)) {
          if (lzma_eof(lzmafp))
            return (SDDS_dataset->page_number = -1);
          SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadNonNativeBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&n_rows);
      } else {
        n_rows = n_rows32;
      }
    } else {
      SDDS_dataset->rowcount_offset = ftell(fp);
      if (!SDDS_BufferedRead(&n_rows32, sizeof(n_rows32), fp, &SDDS_dataset->fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
        if (feof(fp))
          return (SDDS_dataset->page_number = -1);
        SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadNonNativeBinaryPage)");
        return (0);
      }
      SDDS_SwapLong(&n_rows32);
      if (n_rows32 == INT32_MIN) {
        if (!SDDS_BufferedRead(&n_rows, sizeof(n_rows), fp, &SDDS_dataset->fBuffer, SDDS_LONG64, SDDS_dataset->layout.byteOrderDeclared)) {
          if (feof(fp))
            return (SDDS_dataset->page_number = -1);
          SDDS_SetError("Unable to read page--failure reading number of rows (SDDS_ReadNonNativeBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&n_rows);
      } else {
        n_rows = n_rows32;
      }
    }
#if defined(zLib)
  }
#endif
  if (n_rows < 0) {
    SDDS_SetError("Unable to read page--negative number of rows (SDDS_ReadNonNativeBinaryPage)");
    return (0);
  }
  if (last_rows < 0)
    last_rows = 0;
  /* Fix this limitation later */
  if (SDDS_dataset->layout.data_mode.column_major) {
    sparse_interval = 1;
    sparse_offset = 0;
    last_rows = 0;
  }
  if (last_rows) {
    sparse_interval = 1;
    sparse_offset = n_rows - last_rows;
    rows_to_store = last_rows + 2;
    alloc_rows = rows_to_store - SDDS_dataset->n_rows_allocated;
  }
  if (sparse_interval <= 0)
    sparse_interval = 1;
  if (sparse_offset < 0)
    sparse_offset = 0;

  rows_to_store = (n_rows - sparse_offset) / sparse_interval + 2;
  alloc_rows = rows_to_store - SDDS_dataset->n_rows_allocated;
  if (!SDDS_StartPage(SDDS_dataset, 0) || !SDDS_LengthenTable(SDDS_dataset, alloc_rows)) {
    SDDS_SetError("Unable to read page--couldn't start page (SDDS_ReadNonNativeBinaryPage)");
    return (0);
  }

  /* read the parameter values */
  if (!SDDS_ReadNonNativeBinaryParameters(SDDS_dataset)) {
    SDDS_SetError("Unable to read page--parameter reading error (SDDS_ReadNonNativeBinaryPage)");
    return (0);
  }

  /* read the array values */
  if (!SDDS_ReadNonNativeBinaryArrays(SDDS_dataset)) {
    SDDS_SetError("Unable to read page--array reading error (SDDS_ReadNonNativeBinaryPage)");
    return (0);
  }
  if (SDDS_dataset->layout.data_mode.column_major) {
    SDDS_dataset->n_rows = n_rows;
    if (!SDDS_ReadNonNativeBinaryColumns(SDDS_dataset)) {
      SDDS_SetError("Unable to read page--column reading error (SDDS_ReadNonNativeBinaryPage)");
      return (0);
    }
    SDDS_SwapEndsColumnData(SDDS_dataset);
    return (SDDS_dataset->page_number);
  }
  if ((sparse_interval <= 1) && (sparse_offset == 0)) {
    for (j = 0; j < n_rows; j++) {
      if (!SDDS_ReadNonNativeBinaryRow(SDDS_dataset, j, 0)) {
        SDDS_dataset->n_rows = j - 1;
        if (SDDS_dataset->autoRecover) {
          SDDS_ClearErrors();
          SDDS_SwapEndsColumnData(SDDS_dataset);
          return (SDDS_dataset->page_number);
        }
        SDDS_SetError("Unable to read page--error reading data row (SDDS_ReadNonNativeBinaryPage)");
        SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
        return (0);
      }
    }
    SDDS_dataset->n_rows = j;
    SDDS_SwapEndsColumnData(SDDS_dataset);
    return (SDDS_dataset->page_number);
  } else {
    for (j = 0; j < sparse_offset; j++) {
      if (!SDDS_ReadNonNativeBinaryRow(SDDS_dataset, 0, 1)) {
        SDDS_dataset->n_rows = 0;
        if (SDDS_dataset->autoRecover) {
          SDDS_ClearErrors();
          SDDS_SwapEndsColumnData(SDDS_dataset);
          return (SDDS_dataset->page_number);
        }
        SDDS_SetError("Unable to read page--error reading data row (SDDS_ReadNonNativeBinaryPage)");
        SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
        return (0);
      }
    }
    n_rows -= sparse_offset;
    for (j = k = 0; j < n_rows; j++) {
      if (!SDDS_ReadNonNativeBinaryRow(SDDS_dataset, k, mod = j % sparse_interval)) {
        SDDS_dataset->n_rows = k - 1;
        if (SDDS_dataset->autoRecover) {
          SDDS_ClearErrors();
          SDDS_SwapEndsColumnData(SDDS_dataset);
          return (SDDS_dataset->page_number);
        }
        SDDS_SetError("Unable to read page--error reading data row (SDDS_ReadNonNativeBinaryPage)");
        SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
        return (0);
      }
      k += mod ? 0 : 1;
    }
    SDDS_dataset->n_rows = k;
    SDDS_SwapEndsColumnData(SDDS_dataset);
    return (SDDS_dataset->page_number);
  }
}

/**
 * @brief Reads non-native endian binary parameters from an SDDS dataset.
 *
 * This function iterates through all parameter definitions in the specified SDDS dataset and reads their
 * binary data from the underlying file. It handles various data types, including short, unsigned short,
 * long, unsigned long, long long, unsigned long long, float, double, and long double. For string
 * parameters, it reads each string individually, ensuring proper memory allocation and byte order
 * conversion. Parameters with fixed values are processed by scanning the fixed value strings into
 * the appropriate data types. The function supports different compression formats, including uncompressed,
 * LZMA-compressed, and GZIP-compressed files.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 *
 * @return int32_t Returns 1 on successful reading and byte-swapping of all parameters, or 0 if an error occurred.
 * @retval 1 All non-native endian parameters were successfully read and byte-swapped.
 * @retval 0 An error occurred during the read or byte-swapping process, such as I/O failures, memory allocation issues,
 *             or corrupted parameter definitions.
 *
 * @note This function modifies the dataset's parameter data in place. It should be called after successfully opening
 *       and preparing the dataset for reading. Ensure that the dataset structure is properly initialized to prevent
 *       undefined behavior.
 */
int32_t SDDS_ReadNonNativeBinaryParameters(SDDS_DATASET *SDDS_dataset) {
  int32_t i;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  char buffer[SDDS_MAXLINE];
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadNonNativeBinaryParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_parameters)
    return (1);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
    } else {
      fp = layout->fp;
    }
#if defined(zLib)
  }
#endif
  fBuffer = &SDDS_dataset->fBuffer;
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
      continue;
    if (layout->parameter_definition[i].fixed_value) {
      strcpy(buffer, layout->parameter_definition[i].fixed_value);
      if (!SDDS_ScanData(buffer, layout->parameter_definition[i].type, 0, SDDS_dataset->parameter[i], 0, 1)) {
        SDDS_SetError("Unable to read page--parameter scanning error (SDDS_ReadNonNativeBinaryParameters)");
        return (0);
      }
    } else if (layout->parameter_definition[i].type == SDDS_STRING) {
      if (*(char **)SDDS_dataset->parameter[i])
        free(*(char **)SDDS_dataset->parameter[i]);
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!(*((char **)SDDS_dataset->parameter[i]) = SDDS_ReadNonNativeGZipBinaryString(gzfp, fBuffer, 0))) {
          SDDS_SetError("Unable to read parameters--failure reading string (SDDS_ReadNonNativeBinaryParameters)");
          return (0);
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!(*((char **)SDDS_dataset->parameter[i]) = SDDS_ReadNonNativeLZMABinaryString(lzmafp, fBuffer, 0))) {
            SDDS_SetError("Unable to read parameters--failure reading string (SDDS_ReadNonNativeBinaryParameters)");
            return (0);
          }
        } else {
          if (!(*((char **)SDDS_dataset->parameter[i]) = SDDS_ReadNonNativeBinaryString(fp, fBuffer, 0))) {
            SDDS_SetError("Unable to read parameters--failure reading string (SDDS_ReadNonNativeBinaryParameters)");
            return (0);
          }
        }
#if defined(zLib)
      }
#endif
    } else {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!SDDS_GZipBufferedRead(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], gzfp, fBuffer, layout->parameter_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read parameters--failure reading value (SDDS_ReadNonNativeBinaryParameters)");
          return (0);
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!SDDS_LZMABufferedRead(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], lzmafp, fBuffer, layout->parameter_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read parameters--failure reading value (SDDS_ReadNonNativeBinaryParameters)");
            return (0);
          }
        } else {
          if (!SDDS_BufferedRead(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], fp, fBuffer, layout->parameter_definition[i].type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read parameters--failure reading value (SDDS_ReadNonNativeBinaryParameters)");
            return (0);
          }
        }
#if defined(zLib)
      }
#endif
    }
  }
  SDDS_SwapEndsParameterData(SDDS_dataset);
  return (1);
}

/**
 * @brief Reads non-native endian binary arrays from an SDDS dataset.
 *
 * This function iterates through all array definitions in the specified SDDS dataset and reads their
 * binary data from the underlying file. It handles various data types, including short, unsigned short,
 * long, unsigned long, long long, unsigned long long, float, double, and long double. For string
 * arrays, it reads each string individually, ensuring proper memory allocation and byte order conversion.
 * The function supports different compression formats, including uncompressed, LZMA-compressed, and
 * GZIP-compressed files. After reading, it swaps the endianness of the array data to match the system's
 * native byte order.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 *
 * @return int32_t Returns 1 on successful reading and byte-swapping of all arrays, or 0 if an error occurred.
 * @retval 1 All non-native endian arrays were successfully read and byte-swapped.
 * @retval 0 An error occurred during the read or byte-swapping process, such as I/O failures, memory allocation issues,
 *             or corrupted array definitions.
 *
 * @note This function modifies the dataset's array data in place. It should be called after successfully opening
 *       and preparing the dataset for reading. Ensure that the dataset structure is properly initialized to prevent
 *       undefined behavior.
 */
int32_t SDDS_ReadNonNativeBinaryArrays(SDDS_DATASET *SDDS_dataset) {
  int32_t i, j;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  /*  static char buffer[SDDS_MAXLINE]; */
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;
  SDDS_ARRAY *array;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadNonNativeBinaryArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_arrays)
    return (1);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
    } else {
      fp = layout->fp;
    }
#if defined(zLib)
  }
#endif
  fBuffer = &SDDS_dataset->fBuffer;
  if (!SDDS_dataset->array) {
    SDDS_SetError("Unable to read array--pointer to structure storage area is NULL (SDDS_ReadNonNativeBinaryArrays)");
    return (0);
  }
  for (i = 0; i < layout->n_arrays; i++) {
    array = SDDS_dataset->array + i;
    if (array->definition && !SDDS_FreeArrayDefinition(array->definition)) {
      SDDS_SetError("Unable to get array--array definition corrupted (SDDS_ReadNonNativeBinaryArrays)");
      return (0);
    }
    if (!SDDS_CopyArrayDefinition(&array->definition, layout->array_definition + i)) {
      SDDS_SetError("Unable to read array--definition copy failed (SDDS_ReadNonNativeBinaryArrays)");
      return (0);
    }
    /*if (array->dimension) free(array->dimension); */
    if (!(array->dimension = SDDS_Realloc(array->dimension, sizeof(*array->dimension) * array->definition->dimensions))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_ReadNonNativeBinaryArrays)");
      return (0);
    }
#if defined(zLib)
    if (SDDS_dataset->layout.gzipFile) {
      if (!SDDS_GZipBufferedRead(array->dimension, sizeof(*array->dimension) * array->definition->dimensions, gzfp, fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
        SDDS_SetError("Unable to read arrays--failure reading dimensions (SDDS_ReadNonNativeBinaryArrays)");
        return (0);
      }
    } else {
#endif
      if (SDDS_dataset->layout.lzmaFile) {
        if (!SDDS_LZMABufferedRead(array->dimension, sizeof(*array->dimension) * array->definition->dimensions, lzmafp, fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read arrays--failure reading dimensions (SDDS_ReadNonNativeBinaryArrays)");
          return (0);
        }
      } else {
        if (!SDDS_BufferedRead(array->dimension, sizeof(*array->dimension) * array->definition->dimensions, fp, fBuffer, SDDS_LONG, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read arrays--failure reading dimensions (SDDS_ReadNonNativeBinaryArrays)");
          return (0);
        }
      }
#if defined(zLib)
    }
#endif
    array->elements = 1;
    for (j = 0; j < array->definition->dimensions; j++) {
      SDDS_SwapLong(&(array->dimension[j]));
      array->elements *= array->dimension[j];
    }
    if (array->data)
      free(array->data);
    array->data = array->pointer = NULL;
    if (array->elements == 0)
      continue;
    if (array->elements < 0) {
      SDDS_SetError("Unable to read array--number of elements is negative (SDDS_ReadNonNativeBinaryArrays)");
      return (0);
    }
    if (!(array->data = SDDS_Realloc(array->data, array->elements * SDDS_type_size[array->definition->type - 1]))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_ReadNonNativeBinaryArrays)");
      return (0);
    }
    if (array->definition->type == SDDS_STRING) {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        for (j = 0; j < array->elements; j++) {
          if (!(((char **)(array->data))[j] = SDDS_ReadNonNativeGZipBinaryString(gzfp, fBuffer, 0))) {
            SDDS_SetError("Unable to read arrays--failure reading string (SDDS_ReadNonNativeBinaryArrays)");
            return (0);
          }
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          for (j = 0; j < array->elements; j++) {
            if (!(((char **)(array->data))[j] = SDDS_ReadNonNativeLZMABinaryString(lzmafp, fBuffer, 0))) {
              SDDS_SetError("Unable to read arrays--failure reading string (SDDS_ReadNonNativeBinaryArrays)");
              return (0);
            }
          }
        } else {
          for (j = 0; j < array->elements; j++) {
            if (!(((char **)(array->data))[j] = SDDS_ReadNonNativeBinaryString(fp, fBuffer, 0))) {
              SDDS_SetError("Unable to read arrays--failure reading string (SDDS_ReadNonNativeBinaryArrays)");
              return (0);
            }
          }
        }
#if defined(zLib)
      }
#endif
    } else {
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!SDDS_GZipBufferedRead(array->data, SDDS_type_size[array->definition->type - 1] * array->elements, gzfp, fBuffer, array->definition->type, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read arrays--failure reading values (SDDS_ReadNonNativeBinaryArrays)");
          return (0);
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!SDDS_LZMABufferedRead(array->data, SDDS_type_size[array->definition->type - 1] * array->elements, lzmafp, fBuffer, array->definition->type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read arrays--failure reading values (SDDS_ReadNonNativeBinaryArrays)");
            return (0);
          }
        } else {
          if (!SDDS_BufferedRead(array->data, SDDS_type_size[array->definition->type - 1] * array->elements, fp, fBuffer, array->definition->type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read arrays--failure reading values (SDDS_ReadNonNativeBinaryArrays)");
            return (0);
          }
        }
#if defined(zLib)
      }
#endif
    }
  }
  SDDS_SwapEndsArrayData(SDDS_dataset);
  return (1);
}

/**
 * @brief Reads a non-native endian binary row from an SDDS dataset.
 *
 * This function reads a single row of data from the specified SDDS dataset, handling data with
 * non-native endianness. It iterates through all column definitions and reads each column's data
 * for the given row. For string columns, it ensures proper memory allocation and byte order conversion.
 * For other data types, it reads the binary data and performs necessary byte swapping. The function
 * supports different compression formats, including uncompressed, LZMA-compressed, and GZIP-compressed files.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to read from.
 * @param[in] row The index of the row to read.
 * @param[in] skip If non-zero, the function will skip reading the row data, useful for sparse reading.
 *
 * @return int32_t Returns 1 on successful reading of the row, or 0 if an error occurred.
 * @retval 1 The row was successfully read and byte-swapped.
 * @retval 0 An error occurred during the read or byte-swapping process, such as I/O failures or corrupted data.
 *
 * @note This function modifies the dataset's data in place. It should be called after successfully
 *       opening and preparing the dataset for reading. Ensure that the dataset structure is properly initialized
 *       to prevent undefined behavior.
 */
int32_t SDDS_ReadNonNativeBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row, int32_t skip) {
  int64_t i, type, size;
  SDDS_LAYOUT *layout;
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadNonNativeBinaryRow"))
    return (0);
  layout = &SDDS_dataset->layout;
  fBuffer = &SDDS_dataset->fBuffer;

#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    for (i = 0; i < layout->n_columns; i++) {
      if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
        continue;
      if ((type = layout->column_definition[i].type) == SDDS_STRING) {
        if (!skip) {
          if (((char ***)SDDS_dataset->data)[i][row])
            free((((char ***)SDDS_dataset->data)[i][row]));
          if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadNonNativeGZipBinaryString(gzfp, fBuffer, 0))) {
            SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadNonNativeBinaryRow)");
            return (0);
          }
        } else {
          if (!SDDS_ReadNonNativeGZipBinaryString(gzfp, fBuffer, 1)) {
            SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadNonNativeBinaryRow)");
            return 0;
          }
        }
      } else {
        size = SDDS_type_size[type - 1];
        if (!SDDS_GZipBufferedRead(skip ? NULL : (char *)SDDS_dataset->data[i] + row * size, size, gzfp, fBuffer, type, SDDS_dataset->layout.byteOrderDeclared)) {
          SDDS_SetError("Unable to read row--failure reading value (SDDS_ReadNonNativeBinaryRow)");
          return (0);
        }
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      for (i = 0; i < layout->n_columns; i++) {
        if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
          continue;
        if ((type = layout->column_definition[i].type) == SDDS_STRING) {
          if (!skip) {
            if (((char ***)SDDS_dataset->data)[i][row])
              free((((char ***)SDDS_dataset->data)[i][row]));
            if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadNonNativeLZMABinaryString(lzmafp, fBuffer, 0))) {
              SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadNonNativeBinaryRow)");
              return (0);
            }
          } else {
            if (!SDDS_ReadNonNativeLZMABinaryString(lzmafp, fBuffer, 1)) {
              SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadNonNativeBinaryRow)");
              return 0;
            }
          }
        } else {
          size = SDDS_type_size[type - 1];
          if (!SDDS_LZMABufferedRead(skip ? NULL : (char *)SDDS_dataset->data[i] + row * size, size, lzmafp, fBuffer, type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read row--failure reading value (SDDS_ReadNonNativeBinaryRow)");
            return (0);
          }
        }
      }
    } else {
      fp = layout->fp;
      for (i = 0; i < layout->n_columns; i++) {
        if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
          continue;
        if ((type = layout->column_definition[i].type) == SDDS_STRING) {
          if (!skip) {
            if (((char ***)SDDS_dataset->data)[i][row])
              free((((char ***)SDDS_dataset->data)[i][row]));
            if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_ReadNonNativeBinaryString(fp, fBuffer, 0))) {
              SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadNonNativeBinaryRow)");
              return (0);
            }
          } else {
            if (!SDDS_ReadNonNativeBinaryString(fp, fBuffer, 1)) {
              SDDS_SetError("Unable to read rows--failure reading string (SDDS_ReadNonNativeBinaryRow)");
              return 0;
            }
          }
        } else {
          size = SDDS_type_size[type - 1];
          if (!SDDS_BufferedRead(skip ? NULL : (char *)SDDS_dataset->data[i] + row * size, size, fp, fBuffer, type, SDDS_dataset->layout.byteOrderDeclared)) {
            SDDS_SetError("Unable to read row--failure reading value (SDDS_ReadNonNativeBinaryRow)");
            return (0);
          }
        }
      }
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Reads a non-native endian binary string from a file.
 *
 * This function reads a binary string from the specified file pointer, handling non-native endianness.
 * It first reads the length of the string, swaps its byte order if necessary, allocates memory for the
 * string, reads the string data, and null-terminates it.
 *
 * @param[in] fp Pointer to the FILE from which to read the string.
 * @param[in,out] fBuffer Pointer to the SDDS_FILEBUFFER structure used for buffered reading.
 * @param[in] skip If non-zero, the function will skip reading the string data, useful for sparse reading.
 *
 * @return char* Returns a pointer to the read string on success, or NULL if an error occurred.
 * @retval Non-NULL Pointer to the newly allocated string.
 * @retval NULL An error occurred during reading or memory allocation.
 *
 * @note The caller is responsible for freeing the returned string to prevent memory leaks.
 */
char *SDDS_ReadNonNativeBinaryString(FILE *fp, SDDS_FILEBUFFER *fBuffer, int32_t skip) {
  int32_t length;
  char *string;

  if (!SDDS_BufferedRead(&length, sizeof(length), fp, fBuffer, SDDS_LONG, 0))
    return (0);
  SDDS_SwapLong(&length);
  if (length < 0)
    return (0);
  if (!(string = SDDS_Malloc(sizeof(*string) * (length + 1))))
    return (NULL);
  if (length && !SDDS_BufferedRead(skip ? NULL : string, sizeof(*string) * length, fp, fBuffer, SDDS_STRING, 0))
    return (NULL);
  string[length] = 0;
  return (string);
}

/**
 * @brief Reads a non-native endian binary string from an LZMA-compressed file.
 *
 * This function reads a binary string from the specified LZMA-compressed file pointer, handling
 * non-native endianness. It first reads the length of the string, swaps its byte order if necessary,
 * allocates memory for the string, reads the string data, and null-terminates it.
 *
 * @param[in] lzmafp Pointer to the LZMAFILE from which to read the string.
 * @param[in,out] fBuffer Pointer to the SDDS_FILEBUFFER structure used for buffered reading.
 * @param[in] skip If non-zero, the function will skip reading the string data, useful for sparse reading.
 *
 * @return char* Returns a pointer to the read string on success, or NULL if an error occurred.
 * @retval Non-NULL Pointer to the newly allocated string.
 * @retval NULL An error occurred during reading or memory allocation.
 *
 * @note The caller is responsible for freeing the returned string to prevent memory leaks.
 */
char *SDDS_ReadNonNativeLZMABinaryString(struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer, int32_t skip) {
  int32_t length;
  char *string;

  if (!SDDS_LZMABufferedRead(&length, sizeof(length), lzmafp, fBuffer, SDDS_LONG, 0))
    return (0);
  SDDS_SwapLong(&length);
  if (length < 0)
    return (0);
  if (!(string = SDDS_Malloc(sizeof(*string) * (length + 1))))
    return (NULL);
  if (length && !SDDS_LZMABufferedRead(skip ? NULL : string, sizeof(*string) * length, lzmafp, fBuffer, SDDS_STRING, 0))
    return (NULL);
  string[length] = 0;
  return (string);
}

#if defined(zLib)
/**
 * @brief Reads a non-native endian binary string from a GZIP-compressed file.
 *
 * This function reads a binary string from the specified GZIP-compressed file pointer, handling
 * non-native endianness. It first reads the length of the string, swaps its byte order if necessary,
 * allocates memory for the string, reads the string data, and null-terminates it.
 *
 * @param[in] gzfp Pointer to the gzFile from which to read the string.
 * @param[in,out] fBuffer Pointer to the SDDS_FILEBUFFER structure used for buffered reading.
 * @param[in] skip If non-zero, the function will skip reading the string data, useful for sparse reading.
 *
 * @return char* Returns a pointer to the read string on success, or NULL if an error occurred.
 * @retval Non-NULL Pointer to the newly allocated string.
 * @retval NULL An error occurred during reading or memory allocation.
 *
 * @note The caller is responsible for freeing the returned string to prevent memory leaks.
 */
char *SDDS_ReadNonNativeGZipBinaryString(gzFile gzfp, SDDS_FILEBUFFER *fBuffer, int32_t skip) {
  int32_t length;
  char *string;

  if (!SDDS_GZipBufferedRead(&length, sizeof(length), gzfp, fBuffer, SDDS_LONG, 0))
    return (0);
  SDDS_SwapLong(&length);
  if (length < 0)
    return (0);
  if (!(string = SDDS_Malloc(sizeof(*string) * (length + 1))))
    return (NULL);
  if (length && !SDDS_GZipBufferedRead(skip ? NULL : string, sizeof(*string) * length, gzfp, fBuffer, SDDS_STRING, 0))
    return (NULL);
  string[length] = 0;
  return (string);
}
#endif

/**
 * @brief Writes a non-native endian binary page to an SDDS dataset.
 *
 * This function writes a binary page to the specified SDDS dataset, handling byte order reversal
 * to convert between little-endian and big-endian formats. It manages various compression formats,
 * including uncompressed, GZIP-compressed, and LZMA-compressed files. The function performs the
 * following operations:
 * - Counts the number of rows to write.
 * - Writes the row count with appropriate byte order handling.
 * - Writes non-native endian parameters and arrays.
 * - Writes column data in either column-major or row-major format based on the dataset's configuration.
 * - Flushes the buffer to ensure all data is written to the file.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to write to.
 *
 * @return int32_t Returns 1 on successful writing of the binary page, or 0 if an error occurred.
 * @retval 1 The binary page was successfully written and byte-swapped.
 * @retval 0 An error occurred during the write operation, such as I/O failures, memory allocation issues,
 *             or corrupted dataset definitions.
 *
 * @note This function modifies the dataset's internal structures during the write process. Ensure that
 *       the dataset is properly initialized and opened for writing before invoking this function. After
 *       writing, the dataset's state is updated to reflect the newly written page.
 */
int32_t SDDS_WriteNonNativeBinaryPage(SDDS_DATASET *SDDS_dataset)
/* Write binary page with byte order reversed.  Used for little-to-big
 * and big-to-little endian conversion
 */
{
  FILE *fp;
  struct lzmafile *lzmafp = NULL;
  int64_t i, rows, fixed_rows;
  int32_t min32 = INT32_MIN, rows32;
  SDDS_FILEBUFFER *fBuffer;
#if defined(zLib)
  gzFile gzfp = NULL;
#endif

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteNonNativeBinaryPage"))
    return (0);
  if (!(fp = SDDS_dataset->layout.fp)) {
    SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteNonNativeBinaryPage)");
    return (0);
  }
  fBuffer = &SDDS_dataset->fBuffer;

  if (!fBuffer->buffer) {
    if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * defaultIOBufferSize))) {
      SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_WriteNonNativeBinaryPage)");
      return 0;
    }
    fBuffer->bufferSize = defaultIOBufferSize;
    fBuffer->bytesLeft = defaultIOBufferSize;
  }
  SDDS_SwapLong(&min32);

  rows = SDDS_CountRowsOfInterest(SDDS_dataset);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!(gzfp = SDDS_dataset->layout.gzfp)) {
      SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteNonNativeBinaryPage)");
      return (0);
    }
    SDDS_dataset->rowcount_offset = gztell(gzfp);
    if (SDDS_dataset->layout.data_mode.fixed_row_count) {
      fixed_rows = ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment;
      if (fixed_rows > INT32_MAX) {
        if (!SDDS_GZipBufferedWrite(&min32, sizeof(min32), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&fixed_rows);
        if (!SDDS_GZipBufferedWrite(&fixed_rows, sizeof(fixed_rows), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&fixed_rows);
      } else {
        rows32 = (int32_t)fixed_rows;
        SDDS_SwapLong(&rows32);
        if (!SDDS_GZipBufferedWrite(&rows32, sizeof(rows32), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
          return (0);
        }
      }
    } else {
      if (rows > INT32_MAX) {
        if (!SDDS_GZipBufferedWrite(&min32, sizeof(min32), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&rows);
        if (!SDDS_GZipBufferedWrite(&rows, sizeof(rows), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&rows);
      } else {
        rows32 = (int32_t)rows;
        SDDS_SwapLong(&rows32);
        if (!SDDS_GZipBufferedWrite(&rows32, sizeof(rows32), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
          return (0);
        }
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!(lzmafp = SDDS_dataset->layout.lzmafp)) {
        SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteNonNativeBinaryPage)");
        return (0);
      }
      SDDS_dataset->rowcount_offset = lzma_tell(lzmafp);
      if (SDDS_dataset->layout.data_mode.fixed_row_count) {
        fixed_rows = ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment;
        if (fixed_rows > INT32_MAX) {
          if (!SDDS_LZMABufferedWrite(&min32, sizeof(min32), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
          SDDS_SwapLong64(&fixed_rows);
          if (!SDDS_LZMABufferedWrite(&fixed_rows, sizeof(fixed_rows), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
          SDDS_SwapLong64(&fixed_rows);
        } else {
          rows32 = (int32_t)fixed_rows;
          SDDS_SwapLong(&rows32);
          if (!SDDS_LZMABufferedWrite(&rows32, sizeof(rows32), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
        }
      } else {
        if (rows > INT32_MAX) {
          if (!SDDS_LZMABufferedWrite(&min32, sizeof(min32), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
          SDDS_SwapLong64(&rows);
          if (!SDDS_LZMABufferedWrite(&rows, sizeof(rows), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
          SDDS_SwapLong64(&rows);
        } else {
          rows32 = (int32_t)rows;
          SDDS_SwapLong(&rows32);
          if (!SDDS_LZMABufferedWrite(&rows32, sizeof(rows32), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
        }
      }
    } else {
      SDDS_dataset->rowcount_offset = ftell(fp);
      if (SDDS_dataset->layout.data_mode.fixed_row_count) {
        fixed_rows = ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment;
        if (fixed_rows > INT32_MAX) {
          if (!SDDS_BufferedWrite(&min32, sizeof(min32), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
          SDDS_SwapLong64(&fixed_rows);
          if (!SDDS_BufferedWrite(&fixed_rows, sizeof(fixed_rows), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
          SDDS_SwapLong64(&fixed_rows);
        } else {
          rows32 = (int32_t)fixed_rows;
          SDDS_SwapLong(&rows32);
          if (!SDDS_BufferedWrite(&rows32, sizeof(rows32), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
        }
      } else {
        if (rows > INT32_MAX) {
          if (!SDDS_BufferedWrite(&min32, sizeof(min32), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
          SDDS_SwapLong64(&rows);
          if (!SDDS_BufferedWrite(&rows, sizeof(rows), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
          SDDS_SwapLong64(&rows);
        } else {
          rows32 = (int32_t)rows;
          SDDS_SwapLong(&rows32);
          if (!SDDS_BufferedWrite(&rows32, sizeof(rows32), fp, fBuffer)) {
            SDDS_SetError("Unable to write page--failure writing number of rows (SDDS_WriteNonNativeBinaryPage)");
            return (0);
          }
        }
      }
    }
#if defined(zLib)
  }
#endif
  if (!SDDS_WriteNonNativeBinaryParameters(SDDS_dataset)) {
    SDDS_SetError("Unable to write page--parameter writing problem (SDDS_WriteNonNativeBinaryPage)");
    return 0;
  }
  if (!SDDS_WriteNonNativeBinaryArrays(SDDS_dataset)) {
    SDDS_SetError("Unable to write page--array writing problem (SDDS_WriteNonNativeBinaryPage)");
    return 0;
  }
  SDDS_SwapEndsColumnData(SDDS_dataset);
  if (SDDS_dataset->layout.n_columns) {
    if (SDDS_dataset->layout.data_mode.column_major) {
      if (!SDDS_WriteNonNativeBinaryColumns(SDDS_dataset)) {
        SDDS_SetError("Unable to write page--column writing problem (SDDS_WriteNonNativeBinaryPage)");
        return 0;
      }
    } else {
      for (i = 0; i < SDDS_dataset->n_rows; i++) {
        if (SDDS_dataset->row_flag[i]) {
          if (!SDDS_WriteNonNativeBinaryRow(SDDS_dataset, i)) {
            SDDS_SetError("Unable to write page--row writing problem (SDDS_WriteNonNativeBinaryPage)");
            return 0;
          }
        }
      }
    }
  }
  SDDS_SwapEndsColumnData(SDDS_dataset);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_GZipFlushBuffer(gzfp, fBuffer)) {
      SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteNonNativeBinaryPage)");
      return 0;
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_LZMAFlushBuffer(lzmafp, fBuffer)) {
        SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteNonNativeBinaryPage)");
        return 0;
      }
    } else {
      if (!SDDS_FlushBuffer(fp, fBuffer)) {
        SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteNonNativeBinaryPage)");
        return 0;
      }
    }
#if defined(zLib)
  }
#endif
  SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
  SDDS_dataset->n_rows_written = rows;
  SDDS_dataset->writing_page = 1;
  return (1);
}

/**
 * @brief Writes non-native endian binary parameters to an SDDS dataset.
 *
 * This function iterates through all parameter definitions in the specified SDDS dataset and writes their
 * binary data to the underlying file. It handles various data types, including short, unsigned short,
 * long, unsigned long, long long, unsigned long long, float, double, and long double. For string
 * parameters, it writes each string individually, ensuring proper memory management and byte order
 * conversion. Parameters with fixed values are skipped during the write process. The function supports
 * different compression formats, including uncompressed, LZMA-compressed, and GZIP-compressed files.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to write to.
 *
 * @return int32_t Returns 1 on successful writing of all parameters, or 0 if an error occurred.
 * @retval 1 All non-native endian parameters were successfully written and byte-swapped.
 * @retval 0 An error occurred during the write operation, such as I/O failures, memory allocation issues,
 *             or corrupted parameter definitions.
 *
 * @note This function modifies the dataset's parameter data during the write process. Ensure that the
 *       dataset is properly initialized and opened for writing before invoking this function. After
 *       writing, the dataset's state is updated to reflect the written parameters.
 */
int32_t SDDS_WriteNonNativeBinaryParameters(SDDS_DATASET *SDDS_dataset) {
  int32_t i;
  SDDS_LAYOUT *layout;
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;
#if defined(zLib)
  gzFile gzfp;
#endif

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteNonNativeBinaryParameters"))
    return (0);

  SDDS_SwapEndsParameterData(SDDS_dataset);

  layout = &SDDS_dataset->layout;
  fBuffer = &SDDS_dataset->fBuffer;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!(gzfp = layout->gzfp)) {
      SDDS_SetError("Unable to write parameters--file pointer is NULL (SDDS_WriteNonNativeBinaryParameters)");
      return (0);
    }
    for (i = 0; i < layout->n_parameters; i++) {
      if (layout->parameter_definition[i].fixed_value)
        continue;
      if (layout->parameter_definition[i].type == SDDS_STRING) {
        if (!SDDS_GZipWriteNonNativeBinaryString(*((char **)SDDS_dataset->parameter[i]), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write parameters--failure writing string (SDDS_WriteNonNativeBinaryParameters)");
          SDDS_SwapEndsParameterData(SDDS_dataset);
          return (0);
        }
      } else if (!SDDS_GZipBufferedWrite(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], gzfp, fBuffer)) {
        SDDS_SetError("Unable to write parameters--failure writing value (SDDS_WriteBinaryParameters)");
        SDDS_SwapEndsParameterData(SDDS_dataset);
        return (0);
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!(lzmafp = layout->lzmafp)) {
        SDDS_SetError("Unable to write parameters--file pointer is NULL (SDDS_WriteNonNativeBinaryParameters)");
        return (0);
      }
      for (i = 0; i < layout->n_parameters; i++) {
        if (layout->parameter_definition[i].fixed_value)
          continue;
        if (layout->parameter_definition[i].type == SDDS_STRING) {
          if (!SDDS_LZMAWriteNonNativeBinaryString(*((char **)SDDS_dataset->parameter[i]), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write parameters--failure writing string (SDDS_WriteNonNativeBinaryParameters)");
            SDDS_SwapEndsParameterData(SDDS_dataset);
            return (0);
          }
        } else if (!SDDS_LZMABufferedWrite(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], lzmafp, fBuffer)) {
          SDDS_SetError("Unable to write parameters--failure writing value (SDDS_WriteBinaryParameters)");
          SDDS_SwapEndsParameterData(SDDS_dataset);
          return (0);
        }
      }
    } else {
      fp = layout->fp;
      for (i = 0; i < layout->n_parameters; i++) {
        if (layout->parameter_definition[i].fixed_value)
          continue;
        if (layout->parameter_definition[i].type == SDDS_STRING) {
          if (!SDDS_WriteNonNativeBinaryString(*((char **)SDDS_dataset->parameter[i]), fp, fBuffer)) {
            SDDS_SetError("Unable to write parameters--failure writing string (SDDS_WriteNonNativeBinaryParameters)");
            SDDS_SwapEndsParameterData(SDDS_dataset);
            return (0);
          }
        } else if (!SDDS_BufferedWrite(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], fp, fBuffer)) {
          SDDS_SetError("Unable to write parameters--failure writing value (SDDS_WriteBinaryParameters)");
          SDDS_SwapEndsParameterData(SDDS_dataset);
          return (0);
        }
      }
    }
#if defined(zLib)
  }
#endif

  SDDS_SwapEndsParameterData(SDDS_dataset);
  return (1);
}

/**
 * @brief Writes non-native endian binary arrays to an SDDS dataset.
 *
 * This function iterates through all array definitions in the specified SDDS dataset and writes their
 * binary data to the underlying file. It handles various data types, including short, unsigned short,
 * long, unsigned long, long long, unsigned long long, float, double, and long double. For string
 * arrays, it writes each string individually, ensuring proper memory management and byte order
 * conversion. The function supports different compression formats, including uncompressed, LZMA-compressed,
 * and GZIP-compressed files. After writing, it swaps the endianness of the array data to match the system's
 * native byte order.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to write to.
 *
 * @return int32_t Returns 1 on successful writing of all arrays, or 0 if an error occurred.
 * @retval 1 All non-native endian arrays were successfully written and byte-swapped.
 * @retval 0 An error occurred during the write operation, such as I/O failures, memory allocation issues,
 *             or corrupted array definitions.
 *
 * @note This function modifies the dataset's array data during the write process. Ensure that the
 *       dataset is properly initialized and opened for writing before invoking this function. After
 *       writing, the dataset's state is updated to reflect the written arrays.
 */
int32_t SDDS_WriteNonNativeBinaryArrays(SDDS_DATASET *SDDS_dataset) {
  int32_t i, j, dimension, zero = 0;
  SDDS_LAYOUT *layout;
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;
#if defined(zLib)
  gzFile gzfp;
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteNonNativeBinaryArrays"))
    return (0);
  SDDS_SwapEndsArrayData(SDDS_dataset);

  layout = &SDDS_dataset->layout;
  fBuffer = &SDDS_dataset->fBuffer;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    for (i = 0; i < layout->n_arrays; i++) {
      if (!SDDS_dataset->array[i].dimension) {
        for (j = 0; j < layout->array_definition[i].dimensions; j++)
          if (!SDDS_GZipBufferedWrite(&zero, sizeof(zero), gzfp, fBuffer)) {
            SDDS_SetError("Unable to write null array--failure writing dimensions (SDDS_WriteNonNativeBinaryArrays)");
            SDDS_SwapEndsArrayData(SDDS_dataset);
            return 0;
          }
        continue;
      }

      for (j = 0; j < layout->array_definition[i].dimensions; j++) {
        dimension = SDDS_dataset->array[i].dimension[j];
        SDDS_SwapLong(&dimension);
        if (!SDDS_GZipBufferedWrite(&dimension, sizeof(dimension), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write arrays--failure writing dimensions (SDDS_WriteNonNativeBinaryArrays)");
          SDDS_SwapEndsArrayData(SDDS_dataset);
          return (0);
        }
      }
      if (layout->array_definition[i].type == SDDS_STRING) {
        for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
          if (!SDDS_GZipWriteNonNativeBinaryString(((char **)SDDS_dataset->array[i].data)[j], gzfp, fBuffer)) {
            SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteNonNativeBinaryArrays)");
            SDDS_SwapEndsArrayData(SDDS_dataset);
            return (0);
          }
        }
      } else if (!SDDS_GZipBufferedWrite(SDDS_dataset->array[i].data, SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements, gzfp, fBuffer)) {
        SDDS_SetError("Unable to write arrays--failure writing values (SDDS_WriteNonNativeBinaryArrays)");
        SDDS_SwapEndsArrayData(SDDS_dataset);
        return (0);
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      for (i = 0; i < layout->n_arrays; i++) {
        if (!SDDS_dataset->array[i].dimension) {
          for (j = 0; j < layout->array_definition[i].dimensions; j++)
            if (!SDDS_LZMABufferedWrite(&zero, sizeof(zero), lzmafp, fBuffer)) {
              SDDS_SetError("Unable to write null array--failure writing dimensions (SDDS_WriteNonNativeBinaryArrays)");
              SDDS_SwapEndsArrayData(SDDS_dataset);
              return 0;
            }
          continue;
        }

        for (j = 0; j < layout->array_definition[i].dimensions; j++) {
          dimension = SDDS_dataset->array[i].dimension[j];
          SDDS_SwapLong(&dimension);
          if (!SDDS_LZMABufferedWrite(&dimension, sizeof(dimension), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write arrays--failure writing dimensions (SDDS_WriteNonNativeBinaryArrays)");
            SDDS_SwapEndsArrayData(SDDS_dataset);
            return (0);
          }
        }
        if (layout->array_definition[i].type == SDDS_STRING) {
          for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
            if (!SDDS_LZMAWriteNonNativeBinaryString(((char **)SDDS_dataset->array[i].data)[j], lzmafp, fBuffer)) {
              SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteNonNativeBinaryArrays)");
              SDDS_SwapEndsArrayData(SDDS_dataset);
              return (0);
            }
          }
        } else if (!SDDS_LZMABufferedWrite(SDDS_dataset->array[i].data, SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements, lzmafp, fBuffer)) {
          SDDS_SetError("Unable to write arrays--failure writing values (SDDS_WriteNonNativeBinaryArrays)");
          SDDS_SwapEndsArrayData(SDDS_dataset);
          return (0);
        }
      }
    } else {
      fp = layout->fp;
      for (i = 0; i < layout->n_arrays; i++) {
        if (!SDDS_dataset->array[i].dimension) {
          for (j = 0; j < layout->array_definition[i].dimensions; j++)
            if (!SDDS_BufferedWrite(&zero, sizeof(zero), fp, fBuffer)) {
              SDDS_SetError("Unable to write null array--failure writing dimensions (SDDS_WriteNonNativeBinaryArrays)");
              SDDS_SwapEndsArrayData(SDDS_dataset);
              return 0;
            }
          continue;
        }

        for (j = 0; j < layout->array_definition[i].dimensions; j++) {
          dimension = SDDS_dataset->array[i].dimension[j];
          SDDS_SwapLong(&dimension);
          if (!SDDS_BufferedWrite(&dimension, sizeof(dimension), fp, fBuffer)) {
            SDDS_SetError("Unable to write arrays--failure writing dimensions (SDDS_WriteNonNativeBinaryArrays)");
            SDDS_SwapEndsArrayData(SDDS_dataset);
            return (0);
          }
        }
        if (layout->array_definition[i].type == SDDS_STRING) {
          for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
            if (!SDDS_WriteNonNativeBinaryString(((char **)SDDS_dataset->array[i].data)[j], fp, fBuffer)) {
              SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteNonNativeBinaryArrays)");
              SDDS_SwapEndsArrayData(SDDS_dataset);
              return (0);
            }
          }
        } else if (!SDDS_BufferedWrite(SDDS_dataset->array[i].data, SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements, fp, fBuffer)) {
          SDDS_SetError("Unable to write arrays--failure writing values (SDDS_WriteNonNativeBinaryArrays)");
          SDDS_SwapEndsArrayData(SDDS_dataset);
          return (0);
        }
      }
    }
#if defined(zLib)
  }
#endif
  SDDS_SwapEndsArrayData(SDDS_dataset);
  return (1);
}

/**
 * @brief Writes a non-native endian binary row to an SDDS dataset.
 *
 * This function writes a single row of data to the specified SDDS dataset, handling byte order reversal
 * to convert between little-endian and big-endian formats. It supports various compression formats,
 * including uncompressed, GZIP-compressed, and LZMA-compressed files. The function iterates through all
 * column definitions, writing each column's data appropriately based on its type. For string columns,
 * it ensures proper memory management and byte order conversion by utilizing specialized string writing
 * functions. For non-string data types, it writes the binary data directly with the correct byte ordering.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to write to.
 * @param[in] row The index of the row to write to the dataset.
 *
 * @return int32_t Returns 1 on successful writing of the binary row, or 0 if an error occurred.
 * @retval 1 The binary row was successfully written and byte-swapped.
 * @retval 0 An error occurred during the write operation, such as I/O failures or corrupted data.
 *
 * @note This function modifies the dataset's internal data structures during the write process. Ensure that
 *       the dataset is properly initialized and opened for writing before invoking this function.
 *       After writing, the dataset's state is updated to reflect the newly written row.
 */
int32_t SDDS_WriteNonNativeBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row) {
  int64_t i, type, size;
  SDDS_LAYOUT *layout;
  FILE *fp;
  struct lzmafile *lzmafp;
  SDDS_FILEBUFFER *fBuffer;
#if defined(zLib)
  gzFile gzfp;
#endif

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteNonNativeBinaryRow"))
    return (0);
  layout = &SDDS_dataset->layout;
  fBuffer = &SDDS_dataset->fBuffer;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    for (i = 0; i < layout->n_columns; i++) {
      if ((type = layout->column_definition[i].type) == SDDS_STRING) {
        if (!SDDS_GZipWriteNonNativeBinaryString(*((char **)SDDS_dataset->data[i] + row), gzfp, fBuffer)) {
          SDDS_SetError("Unable to write rows--failure writing string (SDDS_WriteNonNativeBinaryRows)");
          return (0);
        }
      } else {
        size = SDDS_type_size[type - 1];
        if (!SDDS_GZipBufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, gzfp, fBuffer)) {
          SDDS_SetError("Unable to write row--failure writing value (SDDS_WriteNonNativeBinaryRow)");
          return (0);
        }
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      for (i = 0; i < layout->n_columns; i++) {
        if ((type = layout->column_definition[i].type) == SDDS_STRING) {
          if (!SDDS_LZMAWriteNonNativeBinaryString(*((char **)SDDS_dataset->data[i] + row), lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write rows--failure writing string (SDDS_WriteNonNativeBinaryRows)");
            return (0);
          }
        } else {
          size = SDDS_type_size[type - 1];
          if (!SDDS_LZMABufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, lzmafp, fBuffer)) {
            SDDS_SetError("Unable to write row--failure writing value (SDDS_WriteNonNativeBinaryRow)");
            return (0);
          }
        }
      }
    } else {
      fp = layout->fp;
      for (i = 0; i < layout->n_columns; i++) {
        if ((type = layout->column_definition[i].type) == SDDS_STRING) {
          if (!SDDS_WriteNonNativeBinaryString(*((char **)SDDS_dataset->data[i] + row), fp, fBuffer)) {
            SDDS_SetError("Unable to write rows--failure writing string (SDDS_WriteNonNativeBinaryRows)");
            return (0);
          }
        } else {
          size = SDDS_type_size[type - 1];
          if (!SDDS_BufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, fp, fBuffer)) {
            SDDS_SetError("Unable to write row--failure writing value (SDDS_WriteNonNativeBinaryRow)");
            return (0);
          }
        }
      }
    }
#if defined(zLib)
  }
#endif
  return (1);
}

/**
 * @brief Writes a non-native endian binary string to a file.
 *
 * This function writes a binary string to the specified file pointer, handling non-native endianness.
 * It first writes the length of the string as a 32-bit integer with byte order swapped. If the string
 * is not to be skipped, it then writes the string data itself followed by a null terminator. If the
 * input string is NULL, an empty string is written instead.
 *
 * @param[in] string The string to write. If NULL, an empty string is written.
 * @param[in] fp Pointer to the FILE where the string will be written.
 * @param[in] fBuffer Pointer to the SDDS_FILEBUFFER structure used for buffered writing.
 *
 * @return int32_t Returns 1 on successful writing of the string, or 0 if an error occurred.
 * @retval 1 The string was successfully written and byte-swapped.
 * @retval 0 An error occurred during the write operation, such as I/O failures or memory allocation issues.
 *
 * @note The caller is responsible for ensuring that the file pointer `fp` is valid and open for writing.
 *       This function does not perform memory allocation for the string; it assumes that the string
 *       is already allocated and managed appropriately.
 */
int32_t SDDS_WriteNonNativeBinaryString(char *string, FILE *fp, SDDS_FILEBUFFER *fBuffer) {
  int32_t length;
  static char *dummy_string = "";
  if (!string)
    string = dummy_string;
  length = strlen(string);
  SDDS_SwapLong(&length);
  if (!SDDS_BufferedWrite(&length, sizeof(length), fp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing length");
    return (0);
  }
  SDDS_SwapLong(&length);
  if (length && !SDDS_BufferedWrite(string, sizeof(*string) * length, fp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing contents");
    return (0);
  }
  return (1);
}

/**
 * @brief Writes a non-native endian binary string to an LZMA-compressed file.
 *
 * This function writes a binary string to the specified LZMA-compressed file pointer, handling
 * non-native endianness. It first writes the length of the string as a 32-bit integer with byte
 * order swapped. If the string is not to be skipped, it then writes the string data itself
 * followed by a null terminator. If the input string is NULL, an empty string is written instead.
 *
 * @param[in] string The string to write. If NULL, an empty string is written.
 * @param[in] lzmafp Pointer to the LZMAFILE where the string will be written.
 * @param[in] fBuffer Pointer to the SDDS_FILEBUFFER structure used for buffered writing.
 *
 * @return int32_t Returns 1 on successful writing of the string, or 0 if an error occurred.
 * @retval 1 The string was successfully written and byte-swapped.
 * @retval 0 An error occurred during the write operation, such as I/O failures or memory allocation issues.
 *
 * @note The caller is responsible for ensuring that the LZMAFILE pointer `lzmafp` is valid and open for writing.
 *       This function does not perform memory allocation for the string; it assumes that the string
 *       is already allocated and managed appropriately.
 */
int32_t SDDS_LZMAWriteNonNativeBinaryString(char *string, struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer) {
  int32_t length;
  static char *dummy_string = "";
  if (!string)
    string = dummy_string;
  length = strlen(string);
  SDDS_SwapLong(&length);
  if (!SDDS_LZMABufferedWrite(&length, sizeof(length), lzmafp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing length");
    return (0);
  }
  SDDS_SwapLong(&length);
  if (length && !SDDS_LZMABufferedWrite(string, sizeof(*string) * length, lzmafp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing contents");
    return (0);
  }
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes a non-native endian binary string to a GZIP-compressed file.
 *
 * This function writes a binary string to the specified GZIP-compressed file pointer, handling
 * non-native endianness. It first writes the length of the string as a 32-bit integer with byte
 * order swapped. If the string is not to be skipped, it then writes the string data itself
 * followed by a null terminator. If the input string is NULL, an empty string is written instead.
 *
 * @param[in] string The string to write. If NULL, an empty string is written.
 * @param[in] gzfp Pointer to the gzFile where the string will be written.
 * @param[in] fBuffer Pointer to the SDDS_FILEBUFFER structure used for buffered writing.
 *
 * @return int32_t Returns 1 on successful writing of the string, or 0 if an error occurred.
 * @retval 1 The string was successfully written and byte-swapped.
 * @retval 0 An error occurred during the write operation, such as I/O failures or memory allocation issues.
 *
 * @note The caller is responsible for ensuring that the gzFile pointer `gzfp` is valid and open for writing.
 *       This function does not perform memory allocation for the string; it assumes that the string
 *       is already allocated and managed appropriately.
 */
int32_t SDDS_GZipWriteNonNativeBinaryString(char *string, gzFile gzfp, SDDS_FILEBUFFER *fBuffer) {
  int32_t length;
  static char *dummy_string = "";
  if (!string)
    string = dummy_string;
  length = strlen(string);
  SDDS_SwapLong(&length);
  if (!SDDS_GZipBufferedWrite(&length, sizeof(length), gzfp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing length");
    return (0);
  }
  SDDS_SwapLong(&length);
  if (length && !SDDS_GZipBufferedWrite(string, sizeof(*string) * length, gzfp, fBuffer)) {
    SDDS_SetError("Unable to write string--error writing contents");
    return (0);
  }
  return (1);
}
#endif

/**
 * @brief Updates a non-native endian binary page in an SDDS dataset.
 *
 * This function updates an existing binary page in the specified SDDS dataset, handling byte order
 * reversal to convert between little-endian and big-endian formats. It supports updating rows
 * based on the provided mode flags, such as flushing the table. The function ensures that the buffer
 * is flushed before performing the update and writes any new rows that have been flagged for writing.
 * It also handles fixed row counts and manages byte order conversions as necessary.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset to update.
 * @param[in] mode Bitmask indicating the update mode (e.g., FLUSH_TABLE).
 *
 * @return int32_t Returns 1 on successful update of the binary page, or 0 if an error occurred.
 * @retval 1 The binary page was successfully updated and byte-swapped.
 * @retval 0 An error occurred during the update operation, such as I/O failures, invalid row counts,
 *             or corrupted dataset definitions.
 *
 * @note This function modifies the dataset's internal structures during the update process. Ensure that
 *       the dataset is properly initialized and opened for writing before invoking this function.
 *       After updating, the dataset's state is updated to reflect the changes made to the page.
 */
int32_t SDDS_UpdateNonNativeBinaryPage(SDDS_DATASET *SDDS_dataset, uint32_t mode) {
  FILE *fp;
  int32_t code, min32 = INT32_MIN, rows32;
  int64_t i, rows, offset, fixed_rows;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_UpdateNonNativeBinaryPage"))
    return (0);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    SDDS_SetError("Unable to perform page updates on a gzip file (SDDS_UpdateNonNativeBinaryPage)");
    return 0;
  }
#endif
  if (SDDS_dataset->layout.lzmaFile) {
    SDDS_SetError("Unable to perform page updates on .lzma or .xz files (SDDS_UpdateNonNativeBinaryPage)");
    return 0;
  }
  if (SDDS_dataset->layout.data_mode.column_major) {
    SDDS_SetError("Unable to perform page updates on a column major order file (SDDS_UpdateNonNativeBinaryPage)");
    return 0;
  }
  if (!SDDS_dataset->writing_page) {
#ifdef DEBUG
    fprintf(stderr, "Page not being written---calling SDDS_UpdateNonNativeBinaryPage\n");
#endif
    if (!(code = SDDS_WriteNonNativeBinaryPage(SDDS_dataset))) {
      return 0;
    }
    if (mode & FLUSH_TABLE) {
      SDDS_FreeTableStrings(SDDS_dataset);
      SDDS_dataset->first_row_in_mem = SDDS_CountRowsOfInterest(SDDS_dataset);
      SDDS_dataset->last_row_written = -1;
      SDDS_dataset->n_rows = 0;
    }
    return code;
  }

  if (!(fp = SDDS_dataset->layout.fp)) {
    SDDS_SetError("Unable to update page--file pointer is NULL (SDDS_UpdateNonNativeBinaryPage)");
    return (0);
  }
  fBuffer = &SDDS_dataset->fBuffer;
  if (!SDDS_FlushBuffer(fp, fBuffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_UpdateNonNativeBinaryPage)");
    return 0;
  }
  offset = ftell(fp);

  rows = SDDS_CountRowsOfInterest(SDDS_dataset) + SDDS_dataset->first_row_in_mem;
#ifdef DEBUG
  fprintf(stderr, "%" PRId64 " rows stored in table, %" PRId32 " already written\n", rows, SDDS_dataset->n_rows_written);
#endif
  if (rows == SDDS_dataset->n_rows_written) {
    return (1);
  }
  if (rows < SDDS_dataset->n_rows_written) {
    SDDS_SetError("Unable to update page--new number of rows less than previous number (SDDS_UpdateNonNativeBinaryPage)");
    return (0);
  }
  SDDS_SwapLong(&min32);
  if ((!SDDS_dataset->layout.data_mode.fixed_row_count) || (((rows + rows - SDDS_dataset->n_rows_written / SDDS_dataset->layout.data_mode.fixed_row_increment)) != (rows / SDDS_dataset->layout.data_mode.fixed_row_increment))) {
    if (SDDS_fseek(fp, SDDS_dataset->rowcount_offset, 0) == -1) {
      SDDS_SetError("Unable to update page--failure doing fseek (SDDS_UpdateNonNativeBinaryPage)");
      return (0);
    }
    if (SDDS_dataset->layout.data_mode.fixed_row_count) {
      if ((rows - SDDS_dataset->n_rows_written) + 1 > SDDS_dataset->layout.data_mode.fixed_row_increment) {
        SDDS_dataset->layout.data_mode.fixed_row_increment = (rows - SDDS_dataset->n_rows_written) + 1;
      }
      fixed_rows = ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment;
#if defined(DEBUG)
      fprintf(stderr, "Setting %" PRId64 " fixed rows\n", fixed_rows);
#endif
      if ((fixed_rows > INT32_MAX) && (SDDS_dataset->n_rows_written <= INT32_MAX)) {
        SDDS_SetError("Unable to update page--crossed the INT32_MAX row boundary (SDDS_UpdateNonNativeBinaryPage)");
        return (0);
      }
      if (fixed_rows > INT32_MAX) {
        if (fwrite(&min32, sizeof(min32), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&fixed_rows);
        if (fwrite(&fixed_rows, sizeof(fixed_rows), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateNonNativeBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&fixed_rows);
      } else {
        rows32 = (int32_t)fixed_rows;
        SDDS_SwapLong(&rows32);
        if (fwrite(&rows32, sizeof(rows32), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateNonNativeBinaryPage)");
          return (0);
        }
      }
    } else {
#if defined(DEBUG)
      fprintf(stderr, "Setting %" PRId64 " rows\n", rows);
#endif
      if ((rows > INT32_MAX) && (SDDS_dataset->n_rows_written <= INT32_MAX)) {
        SDDS_SetError("Unable to update page--crossed the INT32_MAX row boundary (SDDS_UpdateNonNativeBinaryPage)");
        return (0);
      }
      if (rows > INT32_MAX) {
        if (fwrite(&min32, sizeof(min32), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&rows);
        if (fwrite(&rows, sizeof(rows), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateNonNativeBinaryPage)");
          return (0);
        }
        SDDS_SwapLong64(&rows);
      } else {
        rows32 = (int32_t)rows;
        SDDS_SwapLong(&rows32);
        if (fwrite(&rows32, sizeof(rows32), 1, fp) != 1) {
          SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateNonNativeBinaryPage)");
          return (0);
        }
      }
    }
    if (SDDS_fseek(fp, offset, 0) == -1) {
      SDDS_SetError("Unable to update page--failure doing fseek to end of page (SDDS_UpdateNonNativeBinaryPage)");
      return (0);
    }
  }
  SDDS_SwapEndsColumnData(SDDS_dataset);
  for (i = SDDS_dataset->last_row_written + 1; i < SDDS_dataset->n_rows; i++) {
    if (SDDS_dataset->row_flag[i] && !SDDS_WriteNonNativeBinaryRow(SDDS_dataset, i)) {
      SDDS_SetError("Unable to update page--failure writing row (SDDS_UpdateNonNativeBinaryPage)");
      return (0);
    }
  }
  SDDS_SwapEndsColumnData(SDDS_dataset);
#ifdef DEBUG
  fprintf(stderr, "Flushing buffer\n");
#endif
  if (!SDDS_FlushBuffer(fp, fBuffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_UpdateNonNativeBinaryPage)");
    return 0;
  }
  SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
  SDDS_dataset->n_rows_written = rows;
  if (mode & FLUSH_TABLE) {
    SDDS_FreeTableStrings(SDDS_dataset);
    SDDS_dataset->first_row_in_mem = rows;
    SDDS_dataset->last_row_written = -1;
    SDDS_dataset->n_rows = 0;
  }
  return (1);
}

/**
 * @brief Converts a 16-byte array representing a float80 value to a double.
 *
 * This function converts a 16-byte array, which represents an 80-bit floating-point (float80) value,
 * to a standard double-precision (64-bit) floating-point value. The conversion handles different
 * byte orders, supporting both big-endian and little-endian formats. On systems where `long double`
 * is implemented as 64-bit (such as Windows and Mac), this function allows reading SDDS_LONGDOUBLE
 * SDDS files with a loss of precision by translating float80 values to double.
 *
 * @param[in] x The 16-byte array representing the float80 value.
 * @param[in] byteOrder The byte order of the array, either `SDDS_BIGENDIAN_SEEN` or `SDDS_LITTLEENDIAN_SEEN`.
 *
 * @return double The converted double-precision floating-point value.
 *
 * @note This function assumes that the input array `x` is correctly formatted as an 80-bit floating-point
 *       value. On systems where `long double` is 80 bits, the conversion preserves as much precision as
 *       possible within the limitations of the double-precision format. On systems with 64-bit `long double`,
 *       the function translates the value with inherent precision loss.
 */
double makeFloat64FromFloat80(unsigned char x[16], int32_t byteOrder) {
  int exponent;
  uint64_t mantissa;
  unsigned char d[8] = {0};
  double result;

  if (byteOrder == SDDS_BIGENDIAN_SEEN) {
    /* conversion is done in little endian */
    char xx;
    int i;
    for (i = 0; i < 6; i++) {
      xx = x[0 + i];
      x[0 + i] = x[11 - i];
      x[11 - i] = xx;
    }
  }

  exponent = (((x[9] << 8) | x[8]) & 0x7FFF);
  mantissa =
    ((uint64_t)x[7] << 56) | ((uint64_t)x[6] << 48) | ((uint64_t)x[5] << 40) | ((uint64_t)x[4] << 32) |
    ((uint64_t)x[3] << 24) | ((uint64_t)x[2] << 16) | ((uint64_t)x[1] << 8) | (uint64_t)x[0];

  d[7] = x[9] & 0x80; /* Set sign. */

  if ((exponent == 0x7FFF) || (exponent == 0)) {
    /* Infinite, NaN or denormal */
    if (exponent == 0x7FFF) {
      /* Infinite or NaN */
      d[7] |= 0x7F;
      d[6] = 0xF0;
    } else {
      /* Otherwise it's denormal. It cannot be represented as double. Translate as singed zero. */
      memcpy(&result, d, 8);
      return result;
    }
  } else {
    /* Normal number. */
    exponent = exponent - 0x3FFF + 0x03FF; /*< exponent for double precision. */

    if (exponent <= -52) /*< Too small to represent. Translate as (signed) zero. */
    {
      memcpy(&result, d, 8);
      return result;
    } else if (exponent < 0) {
      /* Denormal, exponent bits are already zero here. */
    } else if (exponent >= 0x7FF) /*< Too large to represent. Translate as infinite. */
    {
      d[7] |= 0x7F;
      d[6] = 0xF0;
      memset(d, 0x00, 6);
      memcpy(&result, d, 8);
      return result;
    } else {
      /* Representable number */
      d[7] |= (exponent & 0x7F0) >> 4;
      d[6] |= (exponent & 0xF) << 4;
    }
  }
  /* Translate mantissa. */

  mantissa >>= 11;

  if (exponent < 0) {
    /* Denormal, further shifting is required here. */
    mantissa >>= (-exponent + 1);
  }

  d[0] = mantissa & 0xFF;
  d[1] = (mantissa >> 8) & 0xFF;
  d[2] = (mantissa >> 16) & 0xFF;
  d[3] = (mantissa >> 24) & 0xFF;
  d[4] = (mantissa >> 32) & 0xFF;
  d[5] = (mantissa >> 40) & 0xFF;
  d[6] |= (mantissa >> 48) & 0x0F;

  memcpy(&result, d, 8);

  if (byteOrder == SDDS_BIGENDIAN_SEEN) {
    /* convert back to big endian  */
    SDDS_SwapDouble(&result);
  }

  return result;
}
