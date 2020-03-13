#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>

#include <logging.h>

static struct sockaddr_in server_address;
const int server_port = 9000;

static void setup_server_address(void) {
  if (server_address.sin_port)
    return;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(server_port);
  inet_aton("127.0.0.1", &server_address.sin_addr);
}

int udp_client(void) {
  int client = socket(AF_INET, SOCK_DGRAM, 0);
  if (client == -1)
    return -1;

  struct sockaddr_in cli_addr = server_address;
  cli_addr.sin_port = htons(0);

  if (bind(client, (struct sockaddr *) &cli_addr, sizeof(cli_addr)) == -1) {
    close(client);
    return -1;
  }

  return client;
}

static int send_message(int client, const char *message, size_t len) {
  int nbytes = sendto(client, message, len, 0,
                      (struct sockaddr *) &server_address,
                      sizeof(server_address));
  return nbytes == -1;
}

int subscribe(int client, const char *type) {
  static const char ack_message[] = "subscribe : success";

  char message[128];
  int num_written = snprintf(message, sizeof(message), "subscribe : %s", type);
  if (send_message(client, message, num_written))
    return 1;

  struct timeval t = {.tv_sec = 1};
  setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));

  char recv_buffer[32] = {0};
  ssize_t nbytes = recvfrom(client, recv_buffer, sizeof(recv_buffer), 0, 0, 0);

  if (nbytes == -1) {
    logging_error("Could not connect to control server: Timeout.");
    return 1;
  }

  if (strncmp(recv_buffer, ack_message, sizeof(ack_message)) != 0) {
    logging_error("Received no or ill-formed acknowledgement.");
    return 1;
  }

  return 0;
}

int ctrl_client_new(ctrl_service_t type) {
  static const char *services[] = {
      "INPUT", "CONFIG"
  };
  setup_server_address();

  int client = udp_client();
  if (client == -1)
    return -1;

  if (subscribe(client, services[type]))
    goto fail;

  int flags = fcntl(client, F_GETFL);
  if (flags == -1)
    goto fail;

  if (fcntl(client, F_SETFL, flags | O_NONBLOCK) == -1)
    goto fail;

  logging_message("Connected to control-server successfully.");
  return client;

  fail:
  close(client);
  return -1;
}
