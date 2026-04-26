#ifndef TEXT_PROTO_H
#define TEXT_PROTO_H

#include <map>
#include <string>
#include <vector>

struct ProtoMessage {
    std::string type;
    std::map<std::string, std::string> fields;
};

std::string trim_copy(const std::string &s);
std::vector<std::string> split_char(const std::string &s, char delim);
std::string join_strings(const std::vector<std::string> &parts, const std::string &sep);
std::string proto_serialize(const ProtoMessage &msg);
bool proto_parse(const std::string &line, ProtoMessage &msg_out);

#endif
