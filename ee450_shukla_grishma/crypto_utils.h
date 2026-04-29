#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <string>

std::string sha256_hash_trimmed(const std::string &text);
std::string hash_suffix5(const std::string &sha_hex);

#endif
