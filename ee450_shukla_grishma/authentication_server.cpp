#include "constants.h"
#include "crypto_utils.h"
#include "file_utils.h"
#include "net_utils.h"
#include "text_proto.h"

#include <unistd.h>

#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Read users.txt and store (username_hash, password_hash) pairs.
static std::set<std::pair<std::string, std::string> > load_users(const std::string &path) {
    std::set<std::pair<std::string, std::string> > out;
    std::vector<std::string> lines;
    if (!read_lines(path, lines)) {
        return out;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string s = trim_copy(lines[i]);
        if (s.empty()) {
            continue;
        }
        std::vector<std::string> parts = split_char(s, ' ');
        std::vector<std::string> compact;
        for (size_t j = 0; j < parts.size(); ++j) {
            if (!parts[j].empty()) {
                compact.push_back(parts[j]);
            }
        }
        if (compact.size() >= 2) {
            out.insert(std::make_pair(compact[0], compact[1]));
        }
    }
    return out;
}

int main() {
    // Load all valid users once when the server starts.
    std::set<std::pair<std::string, std::string> > users = load_users("users.txt");

    // Create and bind UDP socket for Authentication Server.
    int udp_fd = create_udp_bound_socket(HOST, AUTH_UDP_PORT);
    if (udp_fd < 0) {
        return 1;
    }

    std::cout << "Authentication Server is up and running using UDP on\nport " << AUTH_UDP_PORT << "." << std::endl;

    // Keep listening forever until Ctrl-C.
    while (true) {
        // Wait for one UDP request from Hospital Server.
        std::string payload;
        std::string ip;
        int port = 0;
        if (!udp_recv_from(udp_fd, payload, ip, port)) {
            continue;
        }

        ProtoMessage req;
        if (!proto_parse(payload, req)) {
            continue;
        }
        if (req.type != "auth_req") {
            continue;
        }

        // Get hashed username/password from request.
        std::string u_hash = req.fields["u_hash"];
        std::string p_hash = req.fields["p_hash"];
        std::string suffix = hash_suffix5(u_hash);

        std::cout << "Authentication Server has received an authentication\nrequest for a user with hash suffix: " << suffix << "." << std::endl;

        // Check if this exact pair exists in users.txt data.
        bool ok = users.find(std::make_pair(u_hash, p_hash)) != users.end();
        if (ok) {
            std::cout << "Authentication succeeded for a user with hash suffix:\n" << suffix << "." << std::endl;
        } else {
            std::cout << "Authentication failed for a user with hash suffix:\n" << suffix << "." << std::endl;
        }

        // Send auth result back to Hospital Server.
        ProtoMessage resp;
        resp.type = "auth_resp";
        resp.fields["ok"] = ok ? "1" : "0";
        resp.fields["u_hash"] = u_hash;
        udp_send_to(udp_fd, ip, port, proto_serialize(resp));
        std::cout << "The Authentication Server has sent the authentication\nresult to the Hospital Server." << std::endl;
    }

    close(udp_fd);
    return 0;
}
