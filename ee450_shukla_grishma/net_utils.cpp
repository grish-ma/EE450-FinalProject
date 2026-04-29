#include "net_utils.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

int create_udp_bound_socket(const std::string &host, int port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int create_tcp_listener(const std::string &host, int port, int backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, backlog) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int connect_tcp(const std::string &host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool send_all(int fd, const std::string &data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool recv_line_tcp(int fd, std::string &line_out) {
    line_out.clear();
    char ch = '\0';
    while (true) {
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n <= 0) {
            return false;
        }
        if (ch == '\n') {
            break;
        }
        line_out.push_back(ch);
        if (line_out.size() > 1024 * 1024) {
            return false;
        }
    }
    return true;
}

int get_local_port(int fd) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    std::memset(&addr, 0, sizeof(addr));
    if (getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) != 0) {
        return -1;
    }
    return static_cast<int>(ntohs(addr.sin_port));
}

bool udp_send_to(int fd, const std::string &host, int port, const std::string &payload) {
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        return false;
    }
    ssize_t n = sendto(fd, payload.data(), payload.size(), 0, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    return n == static_cast<ssize_t>(payload.size());
}

bool udp_recv_from(int fd, std::string &payload_out, std::string &ip_out, int &port_out) {
    char buf[65536];
    sockaddr_in src;
    socklen_t len = sizeof(src);
    std::memset(&src, 0, sizeof(src));
    ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr *>(&src), &len);
    if (n < 0) {
        return false;
    }
    payload_out.assign(buf, buf + n);
    char ipbuf[INET_ADDRSTRLEN];
    const char *res = inet_ntop(AF_INET, &src.sin_addr, ipbuf, sizeof(ipbuf));
    ip_out = (res != NULL) ? std::string(res) : "";
    port_out = static_cast<int>(ntohs(src.sin_port));
    return true;
}

bool udp_recv_with_timeout(int fd, int timeout_ms, std::string &payload_out) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0) {
        return false;
    }
    std::string ip;
    int port = 0;
    return udp_recv_from(fd, payload_out, ip, port);
}
