#ifndef HELPER_H
#define HELPER_H

#include <cstdint>
#include <cstddef>

struct Key16 { unsigned char b[16]; };

inline uint16_t be16toh_u(const unsigned char *p){
    return (uint16_t)p[0] << 8 | (uint16_t)p[1];
}
inline uint32_t be32toh_u(const unsigned char *p){
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
inline uint64_t be64toh_u(const unsigned char *p){
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) |
           ((uint64_t)p[3] << 32) | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8)  | (uint64_t)p[7];
}

inline uint64_t hash_key(const Key16 &k)
{
    const uint64_t OFFSET = 1469598103934665603ULL;
    const uint64_t PRIME  = 1099511628211ULL;
    uint64_t h = OFFSET;
    for(int i=0;i<16;i++){ h ^= (uint64_t)k.b[i]; h *= PRIME; }
    return h;
}

#endif