#ifndef ROLLING_EPS_H
#define ROLLING_EPS_H

#include <cstdint>

class RollingEPS 
{
    private:
        static constexpr uint64_t BUCKET_NS = 100000000ULL;
        static constexpr int NB = 10;
        uint64_t counts[NB]{};
        uint64_t base_bucket_start = 0;
        int idx = 0;
        uint64_t window_sum = 0;
        uint64_t peak = 0;
        bool initialized=false;

        uint64_t align100ms(uint64_t ts) const { return (ts / BUCKET_NS) * BUCKET_NS; }

        void advance_to(uint64_t ts)
        {
            uint64_t target = align100ms(ts);
            if(!initialized)
            {
                base_bucket_start = target;
                initialized=true; 
            }
            
            while(target >= base_bucket_start + BUCKET_NS)
            {
                base_bucket_start += BUCKET_NS;
                idx = (idx + 1) % NB;
                window_sum -= counts[idx];
                counts[idx] = 0;
            }
        }

    public:
        inline int getPeak(){ return peak; }
        inline void add(uint64_t ts)
        {
            advance_to(ts);
            counts[idx] += 1; window_sum += 1; if(window_sum > peak) peak = window_sum;
        }
};

#endif
