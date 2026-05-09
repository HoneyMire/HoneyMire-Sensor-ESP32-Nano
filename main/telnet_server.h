#pragma once

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define TELNET_PORT 23
#define MAX_CONNECTIONS 4
#define BUFFER_SIZE 256
#define CMD_BUFFER_SIZE 512

typedef struct {
    const char *command;
    const char *response;
} command_map_t;

void start_telnet_server(void);
void handle_telnet_client(void *pvParameters);
