#pragma once

typedef enum {
  CTRL_INPUT, CTRL_CONFIG
} ctrl_service_t;

#define CTRL_PROTOCOL_INPUT
typedef enum {
#include <control_server/protocol>
} ctrl_input_t ;
#undef CTRL_PROTOCOL_INPUT

/* sets up a client to the control server that receives messages of type 'type'.
 * returns the corresponding, non-blocking udp socket.
 */
int ctrl_client_new(ctrl_service_t);

//int ctrl_client_receive(char *buffer, size_t len);
