#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "lwip/sockets.h"
#include "lwip/inet.h"

struct tcpclient_conf {
  uint8_t enable;
  struct sockaddr_in server;
	int8_t cam;
  uint32_t reserved[8];
};

int read_tcpclient_setting(void);
int write_tcpclient_setting(void);
#endif
