#ifndef TCP_SERVER_H_
#define TCP_SERVER_H_

#include "server_config.h"

typedef struct tcp_server_t tcp_server;

int tcp_server_create(struct server_config *config, tcp_server **server);

void tcp_server_join_thread(tcp_server *server);

void tcp_server_destroy(tcp_server *server);


#endif /* TCP_SERVER_H_ */
