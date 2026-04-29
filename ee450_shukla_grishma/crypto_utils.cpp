#include "crypto_utils.h"
#include "sha256.h"
#include "text_proto.h"

#include <cstring>

std::string sha256_hash_trimmed(const std::string &text) {
    std::string t = trim_copy(text);
    char out[65];
    std::memset(out, 0, sizeof(out));
    sha256_easy_hash_hex(t.c_str(), static_cast<unsigned int>(t.size()), out);
    return std::string(out);
}

std::string hash_suffix5(const std::string &sha_hex) {
    if (sha_hex.size() < 5) {
        return sha_hex;
    }
    return sha_hex.substr(sha_hex.size() - 5);
}
