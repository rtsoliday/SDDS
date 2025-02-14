/**
 * @file agilentcomm.cpp
 * @brief Communication interface for Agilent Oscilloscopes via VXI-11 protocol.
 *
 * @details
 * This software enables Ethernet-based communication with Agilent oscilloscopes using the VXI-11 protocol. It supports sending commands, queries, and retrieving responses for both control and data acquisition tasks. The tool facilitates integration into automated testing systems.
 *
 * @section Usage
 * ```
 * ./agilentcomm -ip <scope_ip> -c <command> [-h]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-ip <scope_ip>`                      | Specifies the IP address of the Agilent oscilloscope.                                 |
 * | `-c <command>`                        | The command or query to send to the oscilloscope.                                     |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-h`                                  | Displays help information and usage instructions.                                     |
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author 
 * Based on software from Steve Sharples of the Univ. of Nottingham.
 * Modified by R. Soliday.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <rpc/rpc.h>
#include <pthread.h>

using Device_Link = long;
using Device_Flags = long;
using Device_ErrorCode = long;

struct Device_Error {
  Device_ErrorCode error;
};

struct Create_LinkParms {
  long clientId;
  bool_t lockDevice;
  unsigned long lock_timeout;
  char *device;
};

struct Create_LinkResp {
  Device_ErrorCode error;
  Device_Link lid;
  unsigned short abortPort;
  unsigned long maxRecvSize;
};

struct Device_WriteParms {
  Device_Link lid;
  unsigned long io_timeout;
  unsigned long lock_timeout;
  Device_Flags flags;
  struct {
    unsigned int data_len;
    char *data_val;
  } data;
};

struct Device_WriteResp {
  Device_ErrorCode error;
  unsigned long size;
};

struct Device_ReadParms {
  Device_Link lid;
  unsigned long requestSize;
  unsigned long io_timeout;
  unsigned long lock_timeout;
  Device_Flags flags;
  char termChar;
};

struct Device_ReadResp {
  Device_ErrorCode error;
  long reason;
  struct {
    unsigned int data_len;
    char *data_val;
  } data;
};

constexpr int CREATE_LINK = 10;
constexpr int DEVICE_WRITE = 11;
constexpr int DEVICE_READ = 12;
constexpr int DESTROY_LINK = 23;

constexpr int VXI11_DEFAULT_TIMEOUT = 10000; // in ms
constexpr int VXI11_READ_TIMEOUT = 2000;     // in ms
using VXI11_CLIENT = CLIENT;
using VXI11_LINK = Create_LinkResp;
constexpr int VXI11_MAX_CLIENTS = 256;    // maximum number of unique IP addresses/clients
constexpr int VXI11_NULL_READ_RESP = 50;  // vxi11_receive() return value if a query times out on the instrument
constexpr int VXI11_NULL_WRITE_RESP = 51; // vxi11_send() return value if a sent command times out on the instrument

struct CLINK {
  VXI11_CLIENT *client;
  VXI11_LINK *link;
};

constexpr int DEVICE_CORE = 0x0607AF;
constexpr int DEVICE_CORE_VERSION = 1;
constexpr int RCV_END_BIT = 0x04; // An end indicator has been read
constexpr int RCV_CHR_BIT = 0x02; // A termchar is set in flags and a character which matches termChar is transferred

char VXI11_IP_ADDRESS[VXI11_MAX_CLIENTS][20];
CLIENT *VXI11_CLIENT_ADDRESS[VXI11_MAX_CLIENTS];
int VXI11_DEVICE_NO = 0;
int VXI11_LINK_COUNT[VXI11_MAX_CLIENTS] = {0};

static struct timeval TIMEOUT = {25, 0};

constexpr int BUF_LEN = 1000000;

// Function prototypes
bool sc(const char *con, const char *var);

int vxi11_open_device(const char *ip, CLINK *clink);
int vxi11_open_device(const char *ip, CLINK *clink, const char *device);
int vxi11_open_device(const char *ip, CLIENT **client, VXI11_LINK **link, const char *device);
int vxi11_open_link(const char *ip, CLIENT **client, VXI11_LINK **link, const char *device);
int vxi11_send(CLINK *clink, const char *cmd);
int vxi11_send(CLINK *clink, const char *cmd, unsigned long len);
int vxi11_send(CLIENT *client, VXI11_LINK *link, const char *cmd);
int vxi11_send(CLIENT *client, VXI11_LINK *link, const char *cmd, unsigned long len);
int vxi11_close_device(const char *ip, CLINK *clink);
int vxi11_close_device(const char *ip, CLIENT *client, VXI11_LINK *link);
int vxi11_close_link(const char *ip, CLIENT *client, VXI11_LINK *link);
double vxi11_obtain_double_value(CLINK *clink, const char *cmd);
double vxi11_obtain_double_value(CLINK *clink, const char *cmd, unsigned long timeout);
long vxi11_send_and_receive(CLINK *clink, const char *cmd, char *buf, unsigned long buf_len, unsigned long timeout);
long vxi11_receive(CLINK *clink, char *buffer, unsigned long len);
long vxi11_receive(CLIENT *client, VXI11_LINK *link, char *buffer, unsigned long len, unsigned long timeout);
long vxi11_receive(CLINK *clink, char *buffer, unsigned long len, unsigned long timeout);
long vxi11_obtain_long_value(CLINK *clink, const char *cmd, unsigned long timeout);
long vxi11_obtain_long_value(CLINK *clink, const char *cmd);
long vxi11_receive_data_block(CLINK *clink, char *buffer, unsigned long len, unsigned long timeout);

// RPC function prototypes
enum clnt_stat create_link_1(Create_LinkParms *argp, Create_LinkResp *clnt_res, CLIENT *clnt);
enum clnt_stat device_write_1(Device_WriteParms *argp, Device_WriteResp *clnt_res, CLIENT *clnt);
enum clnt_stat destroy_link_1(Device_Link *argp, Device_Error *clnt_res, CLIENT *clnt);
enum clnt_stat device_read_1(Device_ReadParms *argp, Device_ReadResp *clnt_res, CLIENT *clnt);

// XDR function prototypes
bool_t xdr_Create_LinkParms(XDR *xdrs, Create_LinkParms *objp);
bool_t xdr_Create_LinkResp(XDR *xdrs, Create_LinkResp *objp);
bool_t xdr_Device_ErrorCode(XDR *xdrs, Device_ErrorCode *objp);
bool_t xdr_Device_Link(XDR *xdrs, Device_Link *objp);
bool_t xdr_Device_WriteParms(XDR *xdrs, Device_WriteParms *objp);
bool_t xdr_Device_WriteResp(XDR *xdrs, Device_WriteResp *objp);
bool_t xdr_Device_Flags(XDR *xdrs, Device_Flags *objp);
bool_t xdr_Device_Error(XDR *xdrs, Device_Error *objp);
bool_t xdr_Device_ReadParms(XDR *xdrs, Device_ReadParms *objp);
bool_t xdr_Device_ReadResp(XDR *xdrs, Device_ReadResp *objp);

int main(int argc, char **argv) {
  const char *progname = argv[0];
  const char *serverIP = nullptr;
  char comm[256] = {0};
  long bytes_returned = 0;
  bool got_ip = false;
  bool got_comm = false;
  bool got_help = false;
  int index = 1;
  CLINK *clink = new CLINK{nullptr, nullptr};
  char outputbuf[BUF_LEN] = {0};

  // Parse command-line arguments
  while (index < argc) {
    if (sc(argv[index], "-ip") || sc(argv[index], "-ip_address") || sc(argv[index], "-IP")) {
      if (++index < argc) {
        serverIP = argv[index];
        got_ip = true;
      }
    }

    if (sc(argv[index], "-command") || sc(argv[index], "-c") || sc(argv[index], "-comm")) {
      if (++index < argc) {
        std::snprintf(comm, sizeof(comm), "%s", argv[index]);
        got_comm = true;
      }
    }

    // Handle help option
    if (sc(argv[index], "-h") || sc(argv[index], "-help")) {
      got_help = true;
    }

    index++;
  }

  // Validate required arguments
  if (!got_ip || !got_comm || got_help) {
    if (!got_ip || !got_comm) {
      std::fprintf(stderr, "Error: Missing required arguments.\n\n");
    }
    std::printf("Usage: %s [OPTIONS]\n\n", progname);
    std::printf("Options:\n");
    std::printf("  -ip, -ip_address, -IP    IP address of scope (e.g., 128.243.74.232)\n");
    std::printf("  -c, -command, -comm      Command or query to send\n");
    std::printf("  -h, -help                Display this help message\n\n");
    std::printf("Documentation:\n");
    std::printf("  http://cp.literature.agilent.com/litweb/pdf/54855-97017.pdf\n");
    delete clink;
    return EXIT_FAILURE;
  }

  // Open device
  if (vxi11_open_device(serverIP, clink) != 0) {
    std::fprintf(stderr, "Error: Failed to open device at IP %s.\n", serverIP);
    delete clink;
    return EXIT_FAILURE;
  }

  // Send command
  if (vxi11_send(clink, comm) != 0) {
    std::fprintf(stderr, "Error: Failed to send command '%s' to device.\n", comm);
    vxi11_close_device(serverIP, clink);
    delete clink;
    return EXIT_FAILURE;
  }

  // Check if it's a query
  if (std::strchr(comm, '?') != nullptr) {
    bytes_returned = vxi11_receive(clink, outputbuf, BUF_LEN);
    vxi11_close_device(serverIP, clink);

    if (bytes_returned > 0) {
      std::printf("%s\n", outputbuf);
    } else if (bytes_returned == -VXI11_NULL_READ_RESP) {
      std::fprintf(stderr, "Error: Nothing received after sending scope command '%s'.\n", comm);
      delete clink;
      return EXIT_FAILURE;
    } else {
      std::fprintf(stderr, "Error: Failed to receive response for command '%s'.\n", comm);
      delete clink;
      return EXIT_FAILURE;
    }
  } else {
    vxi11_close_device(serverIP, clink);
  }

  delete clink;
  return EXIT_SUCCESS;
}

// String comparison function for parsing
bool sc(const char *con, const char *var) {
  return std::strcmp(con, var) == 0;
}

int vxi11_open_device(const char *ip, CLINK *clink) {
  const char device[] = "inst0";
  return vxi11_open_device(ip, clink, device);
}

int vxi11_open_device(const char *ip, CLIENT **client, VXI11_LINK **link, const char *device) {
#ifdef __APPLE__
  *client = clnt_create(reinterpret_cast<char *>(ip), DEVICE_CORE, DEVICE_CORE_VERSION, "tcp");
#else
  *client = clnt_create(ip, DEVICE_CORE, DEVICE_CORE_VERSION, "tcp");
#endif

  if (*client == nullptr) {
#ifdef __APPLE__
    clnt_pcreateerror(reinterpret_cast<char *>(ip));
#else
    clnt_pcreateerror(ip);
#endif
    return -1;
  }

  return vxi11_open_link(ip, client, link, device);
}

int vxi11_open_device(const char *ip, CLINK *clink, const char *device) {
  int ret;
  int device_no = -1;

  for (int l = 0; l < VXI11_MAX_CLIENTS; l++) {
    if (std::strcmp(ip, VXI11_IP_ADDRESS[l]) == 0) {
      device_no = l;
      break;
    }
  }

  if (device_no < 0) {
    if (VXI11_DEVICE_NO >= VXI11_MAX_CLIENTS) {
      std::fprintf(stderr, "Error: Maximum of %d clients allowed.\n", VXI11_MAX_CLIENTS);
      ret = -VXI11_MAX_CLIENTS;
    } else {
      ret = vxi11_open_device(ip, &(clink->client), &(clink->link), device);
      std::strncpy(VXI11_IP_ADDRESS[VXI11_DEVICE_NO], ip, sizeof(VXI11_IP_ADDRESS[VXI11_DEVICE_NO]) - 1);
      VXI11_IP_ADDRESS[VXI11_DEVICE_NO][sizeof(VXI11_IP_ADDRESS[VXI11_DEVICE_NO]) - 1] = '\0';
      VXI11_CLIENT_ADDRESS[VXI11_DEVICE_NO] = clink->client;
      VXI11_LINK_COUNT[VXI11_DEVICE_NO] = 1;
      VXI11_DEVICE_NO++;
    }
  } else {
    clink->client = VXI11_CLIENT_ADDRESS[device_no];
    ret = vxi11_open_link(ip, &(clink->client), &(clink->link), device);
    VXI11_LINK_COUNT[device_no]++;
  }
  return ret;
}

int vxi11_open_link(const char *ip, CLIENT **client, VXI11_LINK **link, const char *device) {
  Create_LinkParms link_parms;

  // Set link parameters
  link_parms.clientId = reinterpret_cast<long>(*client);
  link_parms.lockDevice = false;
  link_parms.lock_timeout = VXI11_DEFAULT_TIMEOUT;
  link_parms.device = const_cast<char *>(device);

  *link = new Create_LinkResp();

  if (create_link_1(&link_parms, *link, *client) != RPC_SUCCESS) {
#ifdef __APPLE__
    clnt_perror(*client, reinterpret_cast<char *>(ip));
#else
    clnt_perror(*client, ip);
#endif
    return -2;
  }
  return 0;
}

enum clnt_stat create_link_1(Create_LinkParms *argp, Create_LinkResp *clnt_res, CLIENT *clnt) {
  return clnt_call(clnt, CREATE_LINK,
                   reinterpret_cast<xdrproc_t>(xdr_Create_LinkParms), reinterpret_cast<caddr_t>(argp),
                   reinterpret_cast<xdrproc_t>(xdr_Create_LinkResp), reinterpret_cast<caddr_t>(clnt_res),
                   TIMEOUT);
}

bool_t xdr_Create_LinkParms(XDR *xdrs, Create_LinkParms *objp) {
#if defined(SOLARIS) && !defined(_LP64)
  long *buf;
#else
  int32_t *buf;
#endif

  if (xdrs->x_op == XDR_ENCODE) {
    buf = reinterpret_cast<int32_t *>(XDR_INLINE(xdrs, 3 * BYTES_PER_XDR_UNIT));
    if (buf == nullptr) {
      if (!xdr_long(xdrs, &objp->clientId))
        return FALSE;
      if (!xdr_bool(xdrs, &objp->lockDevice))
        return FALSE;
      if (!xdr_u_long(xdrs, &objp->lock_timeout))
        return FALSE;
    } else {
      IXDR_PUT_INT32(buf, objp->clientId);
      IXDR_PUT_BOOL(buf, objp->lockDevice);
      IXDR_PUT_U_INT32(buf, objp->lock_timeout);
    }
    if (!xdr_string(xdrs, &objp->device, ~0))
      return FALSE;
    return TRUE;
  } else if (xdrs->x_op == XDR_DECODE) {
    buf = reinterpret_cast<int32_t *>(XDR_INLINE(xdrs, 3 * BYTES_PER_XDR_UNIT));
    if (buf == nullptr) {
      if (!xdr_long(xdrs, &objp->clientId))
        return FALSE;
      if (!xdr_bool(xdrs, &objp->lockDevice))
        return FALSE;
      if (!xdr_u_long(xdrs, &objp->lock_timeout))
        return FALSE;
    } else {
      objp->clientId = IXDR_GET_INT32(buf);
      objp->lockDevice = IXDR_GET_BOOL(buf);
      objp->lock_timeout = IXDR_GET_U_INT32(buf);
    }
    if (!xdr_string(xdrs, &objp->device, ~0))
      return FALSE;
    return TRUE;
  }

  if (!xdr_long(xdrs, &objp->clientId))
    return FALSE;
  if (!xdr_bool(xdrs, &objp->lockDevice))
    return FALSE;
  if (!xdr_u_long(xdrs, &objp->lock_timeout))
    return FALSE;
  if (!xdr_string(xdrs, &objp->device, ~0))
    return FALSE;
  return TRUE;
}

bool_t xdr_Create_LinkResp(XDR *xdrs, Create_LinkResp *objp) {
  if (!xdr_Device_ErrorCode(xdrs, &objp->error))
    return FALSE;
  if (!xdr_Device_Link(xdrs, &objp->lid))
    return FALSE;
  if (!xdr_u_short(xdrs, &objp->abortPort))
    return FALSE;
  if (!xdr_u_long(xdrs, &objp->maxRecvSize))
    return FALSE;
  return TRUE;
}

bool_t xdr_Device_ErrorCode(XDR *xdrs, Device_ErrorCode *objp) {
  return xdr_long(xdrs, objp);
}

bool_t xdr_Device_Link(XDR *xdrs, Device_Link *objp) {
  return xdr_long(xdrs, objp);
}

int vxi11_send(CLINK *clink, const char *cmd) {
  return vxi11_send(clink, cmd, std::strlen(cmd));
}

int vxi11_send(CLINK *clink, const char *cmd, unsigned long len) {
  return vxi11_send(clink->client, clink->link, cmd, len);
}

int vxi11_send(CLIENT *client, VXI11_LINK *link, const char *cmd) {
  return vxi11_send(client, link, cmd, std::strlen(cmd));
}

int vxi11_send(CLIENT *client, VXI11_LINK *link, const char *cmd, unsigned long len) {
  Device_WriteParms write_parms;
  int bytes_left = static_cast<int>(len);
  char *send_cmd = new char[len];
  std::memcpy(send_cmd, cmd, len);

  write_parms.lid = link->lid;
  write_parms.io_timeout = VXI11_DEFAULT_TIMEOUT;
  write_parms.lock_timeout = VXI11_DEFAULT_TIMEOUT;

  // We can only write (link->maxRecvSize) bytes at a time
  while (bytes_left > 0) {
    Device_WriteResp write_resp{};

    if (static_cast<unsigned int>(bytes_left) <= link->maxRecvSize) {
      write_parms.flags = 8;
      write_parms.data.data_len = bytes_left;
    } else {
      write_parms.flags = 0;
      write_parms.data.data_len = (link->maxRecvSize > 0) ? link->maxRecvSize : 4096;
    }
    write_parms.data.data_val = send_cmd + (len - bytes_left);

    if (device_write_1(&write_parms, &write_resp, client) != RPC_SUCCESS) {
      delete[] send_cmd;
      return -VXI11_NULL_WRITE_RESP;
    }
    if (write_resp.error != 0) {
      std::fprintf(stderr, "vxi11_user: write error: %ld\n", write_resp.error);
      delete[] send_cmd;
      return -(write_resp.error);
    }
    bytes_left -= write_resp.size;
  }

  delete[] send_cmd;
  return 0;
}

enum clnt_stat device_write_1(Device_WriteParms *argp, Device_WriteResp *clnt_res, CLIENT *clnt) {
  return clnt_call(clnt, DEVICE_WRITE,
                   reinterpret_cast<xdrproc_t>(xdr_Device_WriteParms), reinterpret_cast<caddr_t>(argp),
                   reinterpret_cast<xdrproc_t>(xdr_Device_WriteResp), reinterpret_cast<caddr_t>(clnt_res),
                   TIMEOUT);
}

bool_t xdr_Device_WriteParms(XDR *xdrs, Device_WriteParms *objp) {
  if (!xdr_Device_Link(xdrs, &objp->lid))
    return FALSE;
  if (!xdr_u_long(xdrs, &objp->io_timeout))
    return FALSE;
  if (!xdr_u_long(xdrs, &objp->lock_timeout))
    return FALSE;
  if (!xdr_Device_Flags(xdrs, &objp->flags))
    return FALSE;
  if (!xdr_bytes(xdrs, reinterpret_cast<char **>(&objp->data.data_val), reinterpret_cast<u_int *>(&objp->data.data_len), ~0))
    return FALSE;
  return TRUE;
}

bool_t xdr_Device_WriteResp(XDR *xdrs, Device_WriteResp *objp) {
  if (!xdr_Device_ErrorCode(xdrs, &objp->error))
    return FALSE;
  if (!xdr_u_long(xdrs, &objp->size))
    return FALSE;
  return TRUE;
}

bool_t xdr_Device_Flags(XDR *xdrs, Device_Flags *objp) {
  return xdr_long(xdrs, objp);
}

int vxi11_close_device(const char *ip, CLINK *clink) {
  int ret;
  int device_no = -1;

  // Identify the device number based on IP
  for (int l = 0; l < VXI11_MAX_CLIENTS; l++) {
    if (std::strcmp(ip, VXI11_IP_ADDRESS[l]) == 0) {
      device_no = l;
      break;
    }
  }

  if (device_no == -1) {
    std::fprintf(stderr, "vxi11_close_device: error: No record of opening device with IP address %s.\n", ip);
    ret = -4;
  } else {
    if (VXI11_LINK_COUNT[device_no] > 1) {
      ret = vxi11_close_link(ip, clink->client, clink->link);
      VXI11_LINK_COUNT[device_no]--;
    } else {
      ret = vxi11_close_device(ip, clink->client, clink->link);
    }
  }
  return ret;
}

int vxi11_close_device(const char *ip, CLIENT *client, VXI11_LINK *link) {
  int ret = vxi11_close_link(ip, client, link);
  clnt_destroy(client);
  return ret;
}

int vxi11_close_link(const char *ip, CLIENT *client, VXI11_LINK *link) {
  Device_Error dev_error{};

  if (destroy_link_1(&link->lid, &dev_error, client) != RPC_SUCCESS) {
#ifdef __APPLE__
    clnt_perror(client, reinterpret_cast<char *>(ip));
#else
    clnt_perror(client, ip);
#endif
    return -1;
  }

  return 0;
}

enum clnt_stat destroy_link_1(Device_Link *argp, Device_Error *clnt_res, CLIENT *clnt) {
  return clnt_call(clnt, DESTROY_LINK,
                   reinterpret_cast<xdrproc_t>(xdr_Device_Link), reinterpret_cast<caddr_t>(argp),
                   reinterpret_cast<xdrproc_t>(xdr_Device_Error), reinterpret_cast<caddr_t>(clnt_res),
                   TIMEOUT);
}

bool_t xdr_Device_Error(XDR *xdrs, Device_Error *objp) {
  return xdr_Device_ErrorCode(xdrs, &objp->error);
}

double vxi11_obtain_double_value(CLINK *clink, const char *cmd) {
  return vxi11_obtain_double_value(clink, cmd, VXI11_READ_TIMEOUT);
}

double vxi11_obtain_double_value(CLINK *clink, const char *cmd, unsigned long timeout) {
  char buf[50] = {0}; // 50=arbitrary length... more than enough for one number in ascii
  if (vxi11_send_and_receive(clink, cmd, buf, sizeof(buf), timeout) != 0) {
    std::fprintf(stderr, "Warning: Failed to obtain double value. Returning 0.0.\n");
    return 0.0;
  }
  return std::strtod(buf, nullptr);
}

long vxi11_send_and_receive(CLINK *clink, const char *cmd, char *buf, unsigned long buf_len, unsigned long timeout) {
  int ret;
  long bytes_returned;
  do {
    ret = vxi11_send(clink, cmd);
    if (ret != 0) {
      if (ret != -VXI11_NULL_WRITE_RESP) {
        std::fprintf(stderr, "Error: vxi11_send_and_receive: Could not send command '%s'. Return code: %d.\n", cmd, ret);
        return -1;
      } else {
        std::printf("(Info: VXI11_NULL_WRITE_RESP in vxi11_send_and_receive, resending query)\n");
      }
    }

    bytes_returned = vxi11_receive(clink, buf, buf_len, timeout);
    if (bytes_returned <= 0) {
      if (bytes_returned > -VXI11_NULL_READ_RESP) {
        std::fprintf(stderr, "Error: vxi11_send_and_receive: Problem reading reply. Return code: %ld.\n", bytes_returned);
        return -2;
      } else {
        std::printf("(Info: VXI11_NULL_READ_RESP in vxi11_send_and_receive, resending query)\n");
      }
    }
  } while (bytes_returned == -VXI11_NULL_READ_RESP || ret == -VXI11_NULL_WRITE_RESP);
  return 0;
}

long vxi11_receive(CLIENT *client, VXI11_LINK *link, char *buffer, unsigned long len, unsigned long timeout) {
  Device_ReadParms read_parms;
  Device_ReadResp read_resp{};
  long curr_pos = 0;

  read_parms.lid = link->lid;
  read_parms.requestSize = len;
  read_parms.io_timeout = timeout;   // in ms
  read_parms.lock_timeout = timeout; // in ms
  read_parms.flags = 0;
  read_parms.termChar = 0;

  while (true) {
    read_resp = Device_ReadResp{};
    read_resp.data.data_val = buffer + curr_pos;
    read_parms.requestSize = len - curr_pos; // Never request more total data than originally specified in len

    if (device_read_1(&read_parms, &read_resp, client) != RPC_SUCCESS) {
      return -VXI11_NULL_READ_RESP; // No data to read
    }
    if (read_resp.error != 0) {
      std::fprintf(stderr, "vxi11_user: read error: %ld\n", read_resp.error);
      return -(read_resp.error);
    }

    if ((unsigned long)(curr_pos + read_resp.data.data_len) <= len) {
      curr_pos += read_resp.data.data_len;
    }
    if ((read_resp.reason & RCV_END_BIT) || (read_resp.reason & RCV_CHR_BIT)) {
      break;
    } else if ((unsigned long)curr_pos == len) {
      std::fprintf(stderr, "vxi11_user: read error: Buffer too small. Read %ld bytes without hitting terminator.\n", curr_pos);
      return -100;
    }
  }
  return curr_pos; // Actual number of bytes received
}

long vxi11_receive(CLINK *clink, char *buffer, unsigned long len) {
  return vxi11_receive(clink, buffer, len, VXI11_READ_TIMEOUT);
}

long vxi11_receive(CLINK *clink, char *buffer, unsigned long len, unsigned long timeout) {
  return vxi11_receive(clink->client, clink->link, buffer, len, timeout);
}

enum clnt_stat device_read_1(Device_ReadParms *argp, Device_ReadResp *clnt_res, CLIENT *clnt) {
  return clnt_call(clnt, DEVICE_READ,
                   reinterpret_cast<xdrproc_t>(xdr_Device_ReadParms), reinterpret_cast<caddr_t>(argp),
                   reinterpret_cast<xdrproc_t>(xdr_Device_ReadResp), reinterpret_cast<caddr_t>(clnt_res),
                   TIMEOUT);
}

bool_t xdr_Device_ReadParms(XDR *xdrs, Device_ReadParms *objp) {
#if defined(SOLARIS) && !defined(_LP64)
  long *buf;
#else
  int32_t *buf;
#endif

  if (xdrs->x_op == XDR_ENCODE) {
    if (!xdr_Device_Link(xdrs, &objp->lid))
      return FALSE;
    buf = reinterpret_cast<int32_t *>(XDR_INLINE(xdrs, 3 * BYTES_PER_XDR_UNIT));
    if (buf == nullptr) {
      if (!xdr_u_long(xdrs, &objp->requestSize))
        return FALSE;
      if (!xdr_u_long(xdrs, &objp->io_timeout))
        return FALSE;
      if (!xdr_u_long(xdrs, &objp->lock_timeout))
        return FALSE;
    } else {
      IXDR_PUT_U_INT32(buf, objp->requestSize);
      IXDR_PUT_U_INT32(buf, objp->io_timeout);
      IXDR_PUT_U_INT32(buf, objp->lock_timeout);
    }
    if (!xdr_Device_Flags(xdrs, &objp->flags))
      return FALSE;
    if (!xdr_char(xdrs, &objp->termChar))
      return FALSE;
    return TRUE;
  } else if (xdrs->x_op == XDR_DECODE) {
    if (!xdr_Device_Link(xdrs, &objp->lid))
      return FALSE;
    buf = reinterpret_cast<int32_t *>(XDR_INLINE(xdrs, 3 * BYTES_PER_XDR_UNIT));
    if (buf == nullptr) {
      if (!xdr_u_long(xdrs, &objp->requestSize))
        return FALSE;
      if (!xdr_u_long(xdrs, &objp->io_timeout))
        return FALSE;
      if (!xdr_u_long(xdrs, &objp->lock_timeout))
        return FALSE;
    } else {
      objp->requestSize = IXDR_GET_U_INT32(buf);
      objp->io_timeout = IXDR_GET_U_INT32(buf);
      objp->lock_timeout = IXDR_GET_U_INT32(buf);
    }
    if (!xdr_Device_Flags(xdrs, &objp->flags))
      return FALSE;
    if (!xdr_char(xdrs, &objp->termChar))
      return FALSE;
    return TRUE;
  }

  if (!xdr_Device_Link(xdrs, &objp->lid))
    return FALSE;
  if (!xdr_u_long(xdrs, &objp->requestSize))
    return FALSE;
  if (!xdr_u_long(xdrs, &objp->io_timeout))
    return FALSE;
  if (!xdr_u_long(xdrs, &objp->lock_timeout))
    return FALSE;
  if (!xdr_Device_Flags(xdrs, &objp->flags))
    return FALSE;
  if (!xdr_char(xdrs, &objp->termChar))
    return FALSE;
  return TRUE;
}

bool_t xdr_Device_ReadResp(XDR *xdrs, Device_ReadResp *objp) {
  if (!xdr_Device_ErrorCode(xdrs, &objp->error))
    return FALSE;
  if (!xdr_long(xdrs, &objp->reason))
    return FALSE;
  if (!xdr_bytes(xdrs, reinterpret_cast<char **>(&objp->data.data_val), reinterpret_cast<u_int *>(&objp->data.data_len), ~0))
    return FALSE;
  return TRUE;
}

long vxi11_obtain_long_value(CLINK *clink, const char *cmd, unsigned long timeout) {
  char buf[50] = {0}; // 50=arbitrary length... more than enough for one number in ascii
  if (vxi11_send_and_receive(clink, cmd, buf, sizeof(buf), timeout) != 0) {
    std::fprintf(stderr, "Warning: Failed to obtain long value. Returning 0.\n");
    return 0;
  }
  return std::strtol(buf, nullptr, 10);
}

// Lazy wrapper function with default read timeout
long vxi11_obtain_long_value(CLINK *clink, const char *cmd) {
  return vxi11_obtain_long_value(clink, cmd, VXI11_READ_TIMEOUT);
}

long vxi11_receive_data_block(CLINK *clink, char *buffer, unsigned long len, unsigned long timeout) {
  // Assuming the maximum length of the header is 11 (#9 + 9 digits)
  unsigned long necessary_buffer_size = len + 12;
  char *in_buffer = new char[necessary_buffer_size];
  long ret = vxi11_receive(clink, in_buffer, necessary_buffer_size, timeout);
  if (ret < 0) {
    delete[] in_buffer;
    return ret;
  }
  if (in_buffer[0] != '#') {
    std::fprintf(stderr, "vxi11_user: data block error: Data block does not begin with '#'.\n");
    std::fprintf(stderr, "First 20 characters received were: '");
    for (size_t l = 0; l < 20 && l < (size_t)necessary_buffer_size; l++) {
      std::fprintf(stderr, "%c", in_buffer[l]);
    }
    std::fprintf(stderr, "'\n");
    delete[] in_buffer;
    return -3;
  }

  // First, find out how many digits
  int ndigits = 0;
  sscanf(in_buffer, "#%1d", &ndigits);

  // Some instruments, if there is a problem acquiring the data, return only "#0"
  if (ndigits > 0) {
    // Convert the next <ndigits> bytes into an unsigned long
    char scan_cmd[20];
    std::snprintf(scan_cmd, sizeof(scan_cmd), "#%%1d%%%dlu", ndigits);
    unsigned long returned_bytes = 0;
    sscanf(in_buffer, scan_cmd, &ndigits, &returned_bytes);
    std::memcpy(buffer, in_buffer + (ndigits + 2), returned_bytes);
    delete[] in_buffer;
    return static_cast<long>(returned_bytes);
  } else {
    delete[] in_buffer;
    return 0;
  }
}
