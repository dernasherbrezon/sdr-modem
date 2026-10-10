#include "tcp_utils.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

// how many consecutive read timeouts without any progress are tolerated in the middle of a message
#define TCP_UTILS_MAX_STALLED_TIMEOUTS 3

int tcp_utils_write_data(const uint8_t *buffer, size_t total_len_bytes, int client_socket) {
  size_t left = total_len_bytes;
  while (left > 0) {
    ssize_t written = write(client_socket, buffer + (total_len_bytes - left), left);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    left -= written;
  }
  return 0;
}

int tcp_utils_read_data_partially(void *result, size_t len_bytes, size_t *actually_read, int client_socket) {
  size_t left = len_bytes;
  int code = 0;
  while (left > 0) {
    ssize_t received = recv(client_socket, (char *) result + (len_bytes - left), left, 0);
    if (received < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        code = -errno;
        break;
      }
      if (errno == EINTR) {
        continue;
      }
      code = -1;
      break;
    }
    // client has closed the socket
    if (received == 0) {
      code = -1;
      break;
    }
    left -= received;
  }
  *actually_read = len_bytes - left;
  return code;
}

int tcp_utils_read_data(void *result, size_t len_bytes, int client_socket) {
  size_t total_read = 0;
  int stalled_timeouts = 0;
  while (1) {
    size_t actually_read = 0;
    int code = tcp_utils_read_data_partially((char *) result + total_read, len_bytes - total_read, &actually_read, client_socket);
    total_read += actually_read;
    if (code == 0) {
      return 0;
    }
    // real error or client disconnected
    if (code == -1) {
      return -1;
    }
    // read timeout (-EAGAIN)
    // nothing was read: caller can safely treat this as "no message yet"
    if (total_read == 0) {
      return code;
    }
    // timeout in the middle of the message. keep reading, but give up if client stalls
    if (actually_read > 0) {
      stalled_timeouts = 0;
    } else {
      stalled_timeouts++;
    }
    if (stalled_timeouts >= TCP_UTILS_MAX_STALLED_TIMEOUTS) {
      return -1;
    }
  }
}
