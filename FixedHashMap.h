#ifndef FIXED_HASH_MAP_H
#define FIXED_HASH_MAP_H

#include <vector>
#include <cstdint>
#include <cstring>
#include "helper.h"

struct MapEntry
{
    Key16 key;
    uint64_t ts_ns;
    uint8_t state;
};

class FixedHashMap
{
public:
    std::vector<MapEntry> t;
    uint64_t mask;

    FixedHashMap(size_t pow2cap)
    {
        t.resize(pow2cap);
        mask = pow2cap - 1;
        for(auto &e:t) e.state=0; 
    }

    bool put(const Key16 &k, uint64_t ts)
    {
        uint64_t i = hash_key(k) & mask; 
        size_t start = (size_t)i; 
        ssize_t first_del = -1;
        
        while(true)
        {
            MapEntry &e = t[i];
            if(e.state==0)
            {
                size_t idx = (first_del>=0)? (size_t)first_del : i;
                t[idx].key = k;
                t[idx].ts_ns = ts;
                t[idx].state = 1;
                return true;
            }
            else if(e.state==2)
            {
                if(first_del<0) first_del = (ssize_t)i;
            }
            else if(e.state==1)
            {
                if(memcmp(e.key.b,k.b,16)==0)
                {
                    e.ts_ns = ts;
                    return true;
                }
            }

            i = (i+1) & mask;

            if(i==start)
                return false;
        }
    }

    bool erase_get(const Key16 &k, uint64_t &ts)
    {
        uint64_t i = hash_key(k) & mask;
        size_t start = (size_t)i;
        
        while(true)
        {
            MapEntry &e = t[i];
            
            if(e.state==0)
                return false;
            
            if(e.state==1 && memcmp(e.key.b,k.b,16)==0)
            {
                ts = e.ts_ns;
                e.state=2;
                return true; 
            }
            
            i = (i+1) & mask;
            
            if(i==start)
                return false;
        }
    }
};

#endif
