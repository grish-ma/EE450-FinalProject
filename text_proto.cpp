#include "text_proto.h"

#include <cctype>
#include <sstream>

std::string trim_copy(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::vector<std::string> split_char(const std::string &s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == delim) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(s[i]);
        }
    }
    out.push_back(cur);
    return out;
}

std::string join_strings(const std::vector<std::string> &parts, const std::string &sep) {
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            oss << sep;
        }
        oss << parts[i];
    }
    return oss.str();
}

std::string proto_serialize(const ProtoMessage &msg) {
    std::vector<std::string> parts;
    parts.push_back(msg.type);
    for (std::map<std::string, std::string>::const_iterator it = msg.fields.begin(); it != msg.fields.end(); ++it) {
        parts.push_back(it->first + "=" + it->second);
    }
    return join_strings(parts, "|");
}

bool proto_parse(const std::string &line, ProtoMessage &msg_out) {
    std::string s = trim_copy(line);
    if (s.empty()) {
        return false;
    }
    std::vector<std::string> parts = split_char(s, '|');
    if (parts.empty()) {
        return false;
    }
    msg_out.type = parts[0];
    msg_out.fields.clear();
    for (size_t i = 1; i < parts.size(); ++i) {
        size_t eq = parts[i].find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = parts[i].substr(0, eq);
        std::string value = parts[i].substr(eq + 1);
        msg_out.fields[key] = value;
    }
    return true;
}
