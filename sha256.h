#ifndef SHA256_H
#define SHA256_H

#ifdef __cplusplus
extern "C" {
#endif

void sha256_easy_hash_hex(const void* data, unsigned int len, char* hash);

#ifdef __cplusplus
}
#endif

#endif
