#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include <control_server/client.h>
#include <logging.h>
#include "remote_input.h"

typedef struct remote_input_strategy {
  input_strategy_t base;
  int client_fd;
  input_button_t last_key;
} remote_input_strategy_t;

static bool handle_button_press(input_strategy_t *this) {
  assert(this->controller);
  remote_input_strategy_t *strategy = (remote_input_strategy_t *) this;

  input_ctrl_t *controller = strategy->base.controller;
  if (strategy->last_key != -1) {
    controller->release(controller, strategy->last_key);
    strategy->last_key = -1;
  }

  char buffer[32];

  ssize_t nbytes = recvfrom(strategy->client_fd, buffer, sizeof(buffer),
                            0, 0, 0);
  if (nbytes == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      logging_std_error();
    }
    return false;
  }

  if (strncmp(buffer, "INPUT : ", 8) != 0)
    return false;

  char *end;
  long value = strtol(buffer + 8, &end, 10);
  if (end == buffer + 8)
    return false;

  if (value == CTRL_GAME_BOY_QUIT)
    return true;

  if (value > CTRL_GAME_BOY_QUIT)
    return false;

  input_button_t key = 1 << value;

  controller->press(controller, key);
  strategy->last_key = key;
  return false;
}

static void remote_joy_pad_delete(input_strategy_t *this) {
  close(((remote_input_strategy_t *) this)->client_fd);
  free(this);
}

input_strategy_t *remote_joy_pad_new(void) {
  int client = -1;

  remote_input_strategy_t *strategy = calloc(1,
                                             sizeof(remote_input_strategy_t));
  if (!strategy)
    goto fail;

  client = ctrl_client_new(CTRL_INPUT);
  if (client == -1)
    goto fail;

  strategy->client_fd = client;

  strategy->base.handle_button_press = handle_button_press;
  strategy->base.delete = remote_joy_pad_delete;

  strategy->last_key = -1;

  return (input_strategy_t *) strategy;

  fail:
  if (client != -1)
    close(client);
  logging_std_error();
  return 0;
}
