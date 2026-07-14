#include "mpl_qt.h"
#include <cerrno>
#include <climits>
#include <cstring>
#if defined(_WIN32)
#  include <io.h>
#  include <fcntl.h>
#else
#  include <unistd.h>
#endif

#define MPL_INPUT_BUFFER_SIZE (256 * 1024)

static unsigned char inputBuffer[MPL_INPUT_BUFFER_SIZE];
static size_t inputPosition = 0;
static size_t inputLength = 0;

void resetReaddataBuffer() {
  inputPosition = inputLength = 0;
}

bool readdataBufferHasData() {
  return inputPosition < inputLength;
}

static int readBuffered(FILE *input, void *destination, size_t bytes) {
  unsigned char *output = static_cast<unsigned char *>(destination);
#if defined(_WIN32)
  int fd = _fileno(input);
#else
  int fd = fileno(input);
#endif

  while (bytes) {
    if (inputPosition == inputLength) {
      long bytesRead;
      do {
#if defined(_WIN32)
        bytesRead = _read(fd, inputBuffer, sizeof(inputBuffer));
#else
        bytesRead = read(fd, inputBuffer, sizeof(inputBuffer));
#endif
      } while (bytesRead < 0 && errno == EINTR);
      if (bytesRead < 0)
        return -1;
      if (bytesRead == 0)
        return 0;
      inputPosition = 0;
      inputLength = bytesRead;
    }

    size_t available = inputLength - inputPosition;
    size_t copied = bytes < available ? bytes : available;
    memcpy(output, inputBuffer + inputPosition, copied);
    output += copied;
    inputPosition += copied;
    bytes -= copied;
  }
  return 1;
}

long readdata() {
  bool displayed = false;
  int numvals, readStatus = 1;
  size_t ncMax = 0;
  char command;
  char *bufptr = NULL;
  FILE *input = ifp ? ifp : stdin;

#if defined(_WIN32)
  _setmode(_fileno(input), _O_BINARY);
#endif

  while (1) {
    readStatus = readBuffered(input, &command, sizeof(command));
    if (readStatus != 1)
      break;
    if (command == 'G') {
      if (curwrite && curwrite->buffer) {
        char *trimmed = static_cast<char *>(realloc(curwrite->buffer, curwrite->nc));
        if (trimmed)
          curwrite->buffer = trimmed;
      }
      curwrite = makeplotrec();
      if (!curwrite) {
        fprintf(stderr, "Unable to allocate plot record\n");
        return 1;
      }
      if (domovie || (curwrite->nplot == currentPlot))
        cur = curwrite;
      ncMax = 0;

      if (!cur)
        cur = curwrite;
      bufptr = curwrite->buffer;
    } else if (!curwrite) {
      continue;
    } else if (command == 'E') {
      if (curwrite->buffer) {
        char *trimmed = static_cast<char *>(realloc(curwrite->buffer, curwrite->nc));
        if (trimmed)
          curwrite->buffer = trimmed;
      }
      ncMax = curwrite->nc;
      bufptr = curwrite->buffer + curwrite->nc;

      if (domovie || (keep > 0) || (curwrite->nplot == currentPlot)) {
        refreshCanvas();
        displayed = true;
        if (domovie || (keep > 0))
          return 0;
      }
      continue;
    } else if (command == 'R') {
      quit();
    }

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
      fprintf(stderr, "Unknown mpl_qt command 0x%02x\n", static_cast<unsigned char>(command));
      return 1;
    }

    size_t valueBytes = static_cast<size_t>(numvals) * sizeof(VTYPE);
    size_t required = static_cast<size_t>(curwrite->nc) + sizeof(command) + valueBytes;
    if (required > INT_MAX) {
      fprintf(stderr, "mpl_qt plot command buffer exceeds supported size\n");
      return 1;
    }
    if (required > ncMax) {
      size_t newCapacity = ncMax ? ncMax : DNC;
      while (newCapacity < required) {
        if (newCapacity > static_cast<size_t>(INT_MAX) / 2) {
          newCapacity = required;
          break;
        }
        newCapacity *= 2;
      }
      char *newBuffer = static_cast<char *>(realloc(curwrite->buffer, newCapacity));
      if (!newBuffer) {
        fprintf(stderr, "Unable to allocate mpl_qt plot command buffer\n");
        return 1;
      }
      curwrite->buffer = newBuffer;
      ncMax = newCapacity;
      bufptr = curwrite->buffer + curwrite->nc;
    }

    *bufptr++ = command;
    curwrite->nc += sizeof(command);
    if (valueBytes) {
      readStatus = readBuffered(input, bufptr, valueBytes);
      if (readStatus != 1) {
        if (readStatus < 0)
          fprintf(stderr, "Error reading input: %s\n", strerror(errno));
        else
          fprintf(stderr, "Unexpected end of mpl_qt input\n");
        return 1;
      }
      bufptr += valueBytes;
      curwrite->nc += valueBytes;
    }
  }

  if (readStatus < 0) {
    fprintf(stderr, "Error reading input: %s\n", strerror(errno));
    return 1;
  }
  if (!displayed)
    refreshCanvas();
  domovie = false;
  return 1;
}
