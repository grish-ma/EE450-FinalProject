#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <string>

// UDP/TCP socket setup helpers.
int create_udp_bound_socket(const std::string &host, int port);
int create_tcp_listener(const std::string &host, int port, int backlog);
int connect_tcp(const std::string &host, int port);

// TCP send/receive helpers.
bool send_all(int fd, const std::string &data);
bool recv_line_tcp(int fd, std::string &line_out);
int get_local_port(int fd);

// UDP send/receive helpers.
bool udp_send_to(int fd, const std::string &host, int port, const std::string &payload);
bool udp_recv_from(int fd, std::string &payload_out, std::string &ip_out, int &port_out);
bool udp_recv_with_timeout(int fd, int timeout_ms, std::string &payload_out);

#endif
