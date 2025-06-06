/**
 * @file sddslogserver.c
 * @brief Server program to log data to SDDS files.
 *
 * @details
 * This program listens on a specified port and handles multiple client connections
 * to log data into SDDS files. It supports various commands such as adding values,
 * creating channels, making directories, and generating SDDS plots. Key features include:
 * - Handling multiple client connections using forked processes.
 * - Dynamic creation and management of SDDS channels.
 * - Command restrictions through the forbid option.
 * - Generating SDDS plots and returning their URLs.
 *
 * @section Usage
 * ```
 * sddslogserver -port=<portNumber>
 *               [-root=<rootDirectory>]
 *               [-forbid=<command1,command2,...>]
 *               [-sddsplotPath=<path>]
 * ```
 *
 * @section Options
 * | Required                              | Description                                             |
 * |---------------------------------------|---------------------------------------------------------|
 * | `-port`                               | Port number on which the server listens.                |
 *
 * | Optional                              | Description                                             |
 * |---------------------------------------|---------------------------------------------------------|
 * | `-root`                               | Path of the root directory. Defaults to current directory. |
 * | `-forbid`                             | Comma-separated list of commands to forbid.             |
 * | `-sddsplotPath`                       | Pathname for SDDS plot output files.                    |
 *
 * @subsection Specific Requirements
 *   - `-port` must be a valid positive integer.
 *   - `-root` must specify an existing directory.
 *   - `-sddsplotPath` requires a valid directory path.
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
 * M. Borland,
 * R. Soliday
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#define BUFLEN 16384

int dostuff(int);
int createChannel(char *spec);
int addValue(char *spec);
int runSddsplot(char *returnBuffer, char *options);
void updateChannelDescription(void);
int makeDirectoryList(int64_t *returnNumber, char ***returnBuffer);
int getChannelList(int64_t *returnNumber, char ***returnBuffer);

char *rootDir;

#define DISCONNECT 0 /* Disconnect from the server—forces server to terminate the forked process */
#define ADD_VALUE 1
#define MAKE_DIRECTORY 2
#define CHANGE_DIRECTORY 3
#define GET_TIME_SPAN 4 /* Get values between two times */
#define GET_LAST_N 5    /* Get last N values */
#define SDDSPLOT 6      /* Make an sddsplot and return its URL */
#define ADD_CHANNEL 7
#define DELETE_VALUE 8
#define UPDATE_CHD 9
#define LIST_DIRS 10
#define LIST_CHANNELS 11
#define N_COMMANDS 12

char *command[N_COMMANDS] = {
  "disconnect",
  "addValue",
  "mkdir",
  "cd",
  "getTimeSpan",
  "getLastN",
  "sddsplot",
  "addChannel",
  "deleteValue",
  "updateChDesc",
  "listDirs",
  "listChannels",
};

short forbid[N_COMMANDS] = {
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
};

void error(const char *msg, char *progName) {
  char s[BUFLEN];
  snprintf(s, sizeof(s), "%s (%s)", msg, progName);
  perror(s);
  exit(EXIT_FAILURE);
}

int writeReply(int sock, const char *message, int code) {
  char buffer[BUFLEN + 20];
  if (code) {
    snprintf(buffer, sizeof(buffer), "error:%s (code %d)\n", message, code);
    return write(sock, buffer, strlen(buffer));
  } else {
    // snprintf(buffer, sizeof(buffer), "ok:%s\n", message);
    return write(sock, message, strlen(message));
  }
}

int writeReplyList(int sock, int64_t nItems, char **messageList) {
  int64_t i;
  char message[BUFLEN];
  strncpy(message, "ok:", sizeof(message) - 1);
  message[sizeof(message) - 1] = 0;

  /* Need to check for buffer overflow */
  printf("%" PRId64 " reply items\n", nItems);
  for (i = 0; i < nItems; i++) {
    printf("Item %" PRId64 ": %s\n", i, messageList[i]);
    strncat(message, messageList[i], sizeof(message) - strlen(message) - 1);
    if (i != (nItems - 1))
      strncat(message, ",", sizeof(message) - strlen(message) - 1);
  }
  strncat(message, "\n", sizeof(message) - strlen(message) - 1);
  printf("message: %s", message);
  return write(sock, message, strlen(message));
}

void freeReplyList(int64_t nItems, char **item) {
  int64_t i;
  if (!item)
    return;
  for (i = 0; i < nItems; i++)
    if (item[i])
      free(item[i]);
  free(item);
}

int chdirFromRoot(char *path) {
  char buffer[BUFLEN];
  snprintf(buffer, sizeof(buffer), "%s/%s", rootDir, (path && *path) ? path : "");
  fprintf(stderr, "Changing directory to %s\n", buffer);
  return chdir(buffer);
}

#define CLI_PORT 0
#define CLI_ROOT 1
#define CLI_FORBID 2
#define CLI_SDDSPLOT_PATH 3
#define N_OPTIONS 4

char *option[N_OPTIONS] = {
  "port",
  "root",
  "forbid",
  "sddsplotpath"
};

const char *USAGE =
  "Usage: sddslogserver -port=<portNumber> [-root=<rootDirectory>] \n"
  "                    [-forbid=<command1,command2,...>] \n"
  "                    [-sddsplotPath=<path>]\n\n"
  "Options:\n"
  "  -port          Port number on which the server listens (required).\n"
  "  -root          Path of the root directory (optional, defaults to current directory).\n"
  "  -forbid        Comma-separated list of commands to forbid (optional).\n"
  "  -sddsplotPath  Pathname for SDDS plot output files (optional).\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

char *sddsplotPath = NULL;

static int sockfd = -1;

void shutdownServer(int arg) {
  printf("Closing sockfd=%d\n", sockfd);
  fflush(stdout);
  if (sockfd >= 0)
    close(sockfd);
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  int newsockfd, portno, pid;
  int reuse = 1;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  int i_arg, j, code;
  SCANNED_ARG *s_arg;

  /* Allow zombie children to die */
  signal(SIGCHLD, SIG_IGN);
  signal(SIGINT, shutdownServer);

  /* Parse arguments */
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  portno = -1;
  rootDir = NULL;
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLI_PORT:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%d", &portno) != 1 ||
            portno <= 0) {
          fprintf(stderr, "Error: Invalid syntax/values for -port argument\n%s", USAGE);
          exit(EXIT_FAILURE);
        }
        break;
      case CLI_ROOT:
        if (s_arg[i_arg].n_items != 2 ||
            strlen(rootDir = s_arg[i_arg].list[1]) == 0) {
          fprintf(stderr, "Error: Invalid syntax/values for -root argument\n%s", USAGE);
          exit(EXIT_FAILURE);
        }
        break;
      case CLI_FORBID:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error: Invalid syntax/values for -forbid argument\n%s", USAGE);
          exit(EXIT_FAILURE);
        }
        for (j = 1; j < s_arg[i_arg].n_items; j++) {
          if ((code = match_string(s_arg[i_arg].list[j], command, N_COMMANDS, 0)) < 0) {
            fprintf(stderr, "Error: Unknown command for -forbid: %s\n", s_arg[i_arg].list[j]);
            exit(EXIT_FAILURE);
          }
          forbid[code] = 1;
        }
        break;
      case CLI_SDDSPLOT_PATH:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid syntax for -sddsplotPath option\n%s", USAGE);
          exit(EXIT_FAILURE);
        }
        sddsplotPath = s_arg[i_arg].list[1];
        break;
      default:
        fprintf(stderr, "Invalid or ambiguous option: %s\n%s", s_arg[i_arg].list[0], USAGE);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      fprintf(stderr, "Invalid or ambiguous option: %s\n%s", s_arg[i_arg].list[0], USAGE);
      exit(EXIT_FAILURE);
    }
  }

  if (!rootDir) {
    cp_str(&rootDir, ".");
  }
  if (!fexists(rootDir))
    error("Error: Root directory not found", argv[0]);
  if (chdir(rootDir) != 0)
    perror("chdir");

  /* Create socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("Error opening socket", argv[0]);
  printf("sockfd = %d\n", sockfd);
  memset(&serv_addr, 0, sizeof(serv_addr));

  /* Set the socket to SO_REUSEADDR */
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse, sizeof(reuse)) < 0)
    perror("setsockopt(SO_REUSEPORT) failed");
#endif

  /* Bind the socket to a port number */
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("Error on port binding. Check port number.", argv[0]);

  listen(sockfd, 5);
  clilen = sizeof(cli_addr);
  while (1) {
    printf("Waiting for new socket connection\n");
    fflush(stdout);
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    printf("Got new socket connection\n");
    fflush(stdout);
    if (newsockfd < 0) {
      if (sockfd >= 0)
        close(sockfd);
      error("Error on accept", argv[0]);
    }
    pid = fork();
    if (pid < 0)
      error("Error on fork", argv[0]);
    if (pid == 0) {
      close(sockfd);
      dostuff(newsockfd);
      printf("Returned from dostuff\n");
      fflush(stdout);
      exit(EXIT_SUCCESS);
    } else {
      printf("Forked process\n");
      fflush(stdout);
      close(newsockfd);
    }
  } /* end of while */
  printf("Exited while loop\n");
  fflush(stdout);
  if (sockfd > 0)
    close(sockfd);
  return EXIT_SUCCESS; /* We never get here */
}

/******** DOSTUFF() *********************
          There is a separate instance of this function
          for each connection. It handles all communication
          once a connection has been established.
*****************************************/

int dostuff(int sock) {
  int n, code;
  char buffer[BUFLEN];
  char *ptr1;
  short persist = 1;
  int64_t nItems;
  char **itemValue;

  while (persist) {
    memset(buffer, 0, BUFLEN);
    n = read(sock, buffer, BUFLEN - 1);
    if (n < 0)
      error("ERROR reading from socket", "sddslogserver");
    if (strlen(buffer) == 0)
      continue;
    chop_nl(buffer);
    printf("Here is the message: <%s>\n", buffer);

    if ((ptr1 = strchr(buffer, ':')))
      *ptr1++ = 0;
    if ((code = match_string(buffer, command, N_COMMANDS, EXACT_MATCH)) < 0 || forbid[code]) {
      writeReply(sock, "Forbidden operation.", 0);
      continue;
    }
    switch (code) {
    case DISCONNECT:
      printf("Disconnecting\n");
      fflush(stdout);
      persist = 0;
      break;
    case ADD_VALUE:
      /* syntax: channel=value */
      printf("Add value: %s\n", ptr1);
      if ((code = addValue(ptr1)) == 0)
        writeReply(sock, "ok", code);
      else
        writeReply(sock, "Error: Failed to add value.", code);
      break;
    case DELETE_VALUE:
      /* syntax: channel,sampleID */
      printf("Delete value: %s\n", ptr1);
      writeReply(sock, "Error: Can't do that yet.", EXIT_FAILURE);
      break;
    case ADD_CHANNEL:
      /* syntax: channel,type */
      printf("Add channel: %s\n", ptr1);
      if ((code = createChannel(ptr1)) == 0)
        writeReply(sock, "ok", code);
      else
        writeReply(sock, "Error: Failed to create channel.", code);
      break;
    case MAKE_DIRECTORY:
      /* syntax: directoryName */
      printf("Make directory: %s\n", ptr1);
      if ((code = mkdir(ptr1, 0700))) {
        writeReply(sock, "Error: Making directory.", code);
      } else {
        writeReply(sock, "ok", code);
      }
      break;
    case CHANGE_DIRECTORY:
      /* syntax: path */
      if (ptr1 && *ptr1 && strstr(ptr1, "..")) {
        /* Only absolute paths are allowed */
        writeReply(sock, "Error: Relative paths not supported.", EXIT_FAILURE);
      } else {
        printf("Change directory: %s\n", ptr1 && *ptr1 ? ptr1 : "base");
        if ((code = chdirFromRoot(ptr1)))
          writeReply(sock, "Error: CD failed.", code);
        else
          writeReply(sock, "CD ok.", code);
      }
      break;
    case GET_TIME_SPAN:
      /* syntax: channel,startTime,endTime */
      printf("Get time span: %s\n", ptr1);
      writeReply(sock, "Error: Can't do that yet.", EXIT_FAILURE);
      break;
    case GET_LAST_N:
      /* syntax: channel,N; if N<=0, return all data */
      printf("Get last N: %s\n", ptr1);
      writeReply(sock, "Error: Can't do that yet.", EXIT_FAILURE);
      break;
    case SDDSPLOT:
      /* syntax:<sddsplotCommand> */
      printf("sddsplot: %s\n", ptr1);
      code = runSddsplot(buffer, ptr1);
      writeReply(sock, buffer, code);
      break;
    case UPDATE_CHD:
      updateChannelDescription();
      writeReply(sock, "ok", EXIT_SUCCESS);
      break;
    case LIST_DIRS:
      if (makeDirectoryList(&nItems, &itemValue))
        writeReply(sock, "Failed to retrieve directory list.", EXIT_FAILURE);
      else {
        writeReplyList(sock, nItems, itemValue);
        freeReplyList(nItems, itemValue);
      }
      break;
    case LIST_CHANNELS:
      if (getChannelList(&nItems, &itemValue))
        writeReply(sock, "Failed to retrieve channel list.", EXIT_FAILURE);
      else {
        writeReplyList(sock, nItems, itemValue);
        freeReplyList(nItems, itemValue);
      }
      break;
    default:
      printf("Unknown command: %s\n", buffer);
      writeReply(sock, "Error: Unknown command.", EXIT_FAILURE);
      break;
    }
  }
  return EXIT_SUCCESS;
}

int createChannel(char *spec)
/* spec is of the form <channelName>,<type>,<units>,<description> */
{
  char *chName, *chType, *chUnits, *chDescription;
  char buffer[BUFLEN];
  SDDS_DATASET SDDSout;
  int32_t type;

  chName = spec;
  if (!(chType = strchr(spec, ',')))
    return 1;
  *chType++ = 0;
  if (!(chUnits = strchr(chType, ',')))
    return 2;
  *chUnits++ = 0;
  if ((type = SDDS_IdentifyType(chType)) == 0)
    return 2;
  if (!(chDescription = strchr(chUnits, ',')))
    return 3;
  *chDescription++ = 0;

  if (strcmp(chName, "Time") == 0)
    return 4;

  snprintf(buffer, sizeof(buffer), "%s.sdds", chName);
  if (fexists(buffer))
    return 5;

  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, NULL, buffer) ||
      !SDDS_DefineSimpleColumn(&SDDSout, "SampleIDNumber", NULL, SDDS_LONG64) ||
      !SDDS_DefineSimpleColumn(&SDDSout, "Time", "s", SDDS_DOUBLE) ||
      SDDS_DefineColumn(&SDDSout, chName, NULL, chUnits, chDescription, NULL, type, 0) < 0 ||
      !SDDS_WriteLayout(&SDDSout) ||
      !SDDS_StartPage(&SDDSout, 1) ||
      !SDDS_WritePage(&SDDSout) ||
      !SDDS_Terminate(&SDDSout))
    return 6;

  snprintf(buffer, sizeof(buffer), "sddsquery %s.sdds -sddsOutput=%s.chd -column", chName, chName);
  {
    int sysret = system(buffer);
    (void)sysret;
  }
  snprintf(buffer, sizeof(buffer), "%s.chd", chName);
  if (!fexists(buffer))
    return 6;

  updateChannelDescription();

  return 0;
}

void updateChannelDescription(void) {
  char *command = "sddscombine *.chd -merge -pipe=out | sddsprocess -pipe=in -match=col,Name=SampleIDNumber,! -match=col,Name=Time,! allChd.sdds";
  {
    int sysret = system(command);
    (void)sysret;
  }
}

int addValue(char *spec)
/* spec is of the form <channel>,<value> */
/* This routine is dangerous as there is no checking to ensure that <value> is valid.
 * Should read the .chd file, get the data type, then check for validity.
 */
{
  char *ptr;
  char buffer[BUFLEN];
  int32_t type;
  int64_t rows;
  SDDS_DATASET SDDSin;
  void *data = NULL;

  if (!(ptr = strchr(spec, ',')))
    return 1;
  *ptr++ = 0;

  snprintf(buffer, sizeof(buffer), "%s.sdds", spec);
  if (!SDDS_InitializeAppendToPage(&SDDSin, buffer, 1, &rows)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 2;
  }
  printf("Initialized, %" PRId64 " rows\n", rows);
  fflush(stdout);
  if (!SDDS_LengthenTable(&SDDSin, 1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 3;
  }
  printf("Lengthened table\n");
  fflush(stdout);

  if (SDDS_GetColumnInformation(&SDDSin, "type", &type, SDDS_GET_BY_NAME, spec) != SDDS_LONG) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&SDDSin);
    return 4;
  }
  printf("Got type information\n");
  fflush(stdout);

  if (type != SDDS_STRING)
    data = SDDS_Malloc(SDDS_type_size[type - 1]);
  if (SDDS_ScanData(ptr, type, 0, data, 0, 0) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&SDDSin);
    return 5;
  }
  printf("Scanned data\n");
  fflush(stdout);

  if (!SDDS_SetRowValues(&SDDSin, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rows, "SampleIDNumber", rows, "Time", getTimeInSecs(), NULL)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&SDDSin);
    return 6;
  }
  printf("Set row values\n");
  fflush(stdout);

  switch (type) {
  case SDDS_STRING:
    if (!SDDS_SetRowValues(&SDDSin, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rows, spec, ptr, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 7;
    }
    break;
  case SDDS_FLOAT:
    if (!SDDS_SetRowValues(&SDDSin, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rows, spec, *((float *)data), NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 7;
    }
    break;
  case SDDS_DOUBLE:
    if (!SDDS_SetRowValues(&SDDSin, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rows, spec, *((double *)data), NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 7;
    }
    break;
  case SDDS_SHORT:
    if (!SDDS_SetRowValues(&SDDSin, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rows, spec, *((short *)data), NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 7;
    }
    break;
  case SDDS_LONG:
    if (!SDDS_SetRowValues(&SDDSin, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rows, spec, *((int32_t *)data), NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 7;
    }
    break;
  case SDDS_LONG64:
    if (!SDDS_SetRowValues(&SDDSin, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rows, spec, *((int64_t *)data), NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 7;
    }
    break;
  default:
    break;
  }
  printf("Set row value\n");
  fflush(stdout);
  if (data)
    free(data);

  if (!SDDS_UpdatePage(&SDDSin, FLUSH_TABLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 8;
  }
  printf("Updated page\n");
  fflush(stdout);
  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 9;
  }
  printf("Terminated\n");
  fflush(stdout);
  return 0;
}

int runSddsplot(char *returnBuffer, char *options) {
    char command[BUFLEN];
    char template[BUFLEN];
    int fd;

    // Ensure sddsplotPath is set
    if (!sddsplotPath) {
        fprintf(stderr, "Error: sddsplotPath is not set.\n");
        return 1;
    }

    // Create the template with sddsplotPath and "png-XXXXXX.png"
    // mkstemps requires the template to end with "XXXXXX" followed by the suffix
    snprintf(template, sizeof(template), "%s/png-XXXXXX.png", sddsplotPath);

    // Create a mutable copy of the template for mkstemps
    char *tempTemplate = strdup(template);
    if (!tempTemplate) {
        perror("strdup");
        return 1;
    }

    // mkstemps replaces "XXXXXX" with a unique suffix and keeps the ".png" extension
    fd = mkstemps(tempTemplate, 4); // 4 is the length of ".png"
    if (fd == -1) {
        perror("mkstemps");
        free(tempTemplate);
        return 1;
    }

    // Close the file descriptor as we don't need it
    close(fd);

    // tempTemplate now contains the unique filename with ".png"
    snprintf(command, sizeof(command), "sddsplot -device=png -output=%s %s", tempTemplate, options);
    printf("Executing: %s\n", command);
    fflush(stdout);

    // Copy the filename to returnBuffer
    strncpy(returnBuffer, tempTemplate, BUFLEN - 1);
    returnBuffer[BUFLEN - 1] = '\0';

    // Free the duplicated template
    free(tempTemplate);

    // Execute the command
    return system(command);
}

/*
int runSddsplot(char *returnBuffer, char *options) {
  char command[BUFLEN];
  char *outputName;
  if (!(outputName = tempnam(sddsplotPath, "png-")))
    return 1;
  snprintf(command, sizeof(command), "sddsplot -device=png -output=%s.png %s", outputName, options);
  printf("Executing %s\n", command);
  fflush(stdout);
  strncpy(returnBuffer, outputName, BUFLEN - 1);
  returnBuffer[BUFLEN - 1] = 0;
  strncat(returnBuffer, ".png", BUFLEN - strlen(returnBuffer) - 1);
  free(outputName);
  return system(command);
}
*/

int makeDirectoryList(int64_t *returnNumber, char ***returnBuffer) {
  SDDS_DATASET SDDSin;
  char command[BUFLEN];

  if (returnNumber)
    *returnNumber = 0;
  if (returnBuffer)
    *returnBuffer = NULL;

  remove("dirList.sdds");
  snprintf(command, sizeof(command),
           "find . -type d -maxdepth 1 | tail -n +2 | plaindata2sdds -pipe=in dirList.sdds -input=ascii -column=DirectoryName,string -norow");
  {
    int sysret = system(command);
    (void)sysret;
  }
  if (!SDDS_InitializeInput(&SDDSin, "dirList.sdds")) {
    printf("Problem reading dirList.sdds\n");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (SDDS_ReadPage(&SDDSin) < 0)
    /* Assume file is empty */
    return 0;
  if ((*returnNumber = SDDS_RowCount(&SDDSin)) < 0) {
    printf("Row count: %" PRId64 "\n", *returnNumber);
    return 2;
  }
  if (*returnNumber == 0)
    return 0;
  if (!(*returnBuffer = SDDS_GetColumn(&SDDSin, "DirectoryName"))) {
    printf("Problem getting DirectoryName\n");
    return 3;
  }
  return 0;
}

int getChannelList(int64_t *returnNumber, char ***returnBuffer) {
  SDDS_DATASET SDDSin;

  *returnBuffer = NULL;
  *returnNumber = 0;

  updateChannelDescription();
  if (!fexists("allChd.sdds"))
    return 0;
  if (!SDDS_InitializeInput(&SDDSin, "allChd.sdds")) {
    printf("Problem reading allChd.sdds\n");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (SDDS_ReadPage(&SDDSin) < 0)
    /* Assume file is empty */
    return 0;
  if ((*returnNumber = SDDS_RowCount(&SDDSin)) < 0) {
    printf("Row count: %" PRId64 "\n", *returnNumber);
    return 2;
  }
  if (*returnNumber == 0)
    return 0;
  if (!(*returnBuffer = SDDS_GetColumn(&SDDSin, "Name"))) {
    printf("Problem getting Name\n");
    return 3;
  }
  return 0;
}
