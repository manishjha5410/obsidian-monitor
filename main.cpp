#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>

#include "helper.h"
#include "Quantile.h"
#include "RollingEPS.h"
#include "FixedHashMap.h"

using namespace std;

struct MonHeader { // 16 bytes
    char magic[4];
    uint16_t version;
    uint16_t record_size;
    uint64_t reserved;
};

struct MonRecordRaw { // 72 bytes
    uint64_t ts_ns;     // be64
    uint8_t  type;      // 1=NEW,2=CONFIRM,3=FILL,4=ERROR
    uint8_t  _rsvA;     // 0
    uint16_t _rsvB;     // 0
    uint32_t qty;       // be32
    uint64_t price;     // be64 (paise)
    char account_id[12];
    char symbol[12];
    char client_order_id[16];
    uint32_t error_code; // be32
    uint32_t _rsvC;      // be32
};

struct Metrics {
    uint64_t total_events=0;
    uint64_t total_errors=0;
    uint64_t first_ts=0, last_ts=0;
    uint64_t peak_eps=0;
    uint64_t p50=0, p90=0, p99=0;
};

struct Processor
{
    FixedHashMap new_by_id;
    Quantile q50, q90, q99;
    RollingEPS eps;

    Processor(size_t map_pow2_cap) : new_by_id(map_pow2_cap), q50(0.50), q90(0.90), q99(0.99) {}

    Metrics run(istream &in)
    {
        Metrics m; 

        unsigned char hb[16];
        if(!in.read((char*)hb, 16)) throw runtime_error("Failed to read header");

        MonHeader H{};
        memcpy(H.magic, hb, 4);
        H.version = be16toh_u(hb+4);
        H.record_size = be16toh_u(hb+6);
        H.reserved = be64toh_u(hb+8);

        if(H.magic[0]!='M' || H.magic[1]!='O' || H.magic[2]!='N' || H.magic[3]!='1')
            throw runtime_error("Bad magic (expected MON1)");

        if(H.version!=1) 
            throw runtime_error("Unsupported version");

        if(H.record_size!=72) 
            throw runtime_error("record_size must be 72");

        const size_t REC = 72;
        const size_t BLOCK_SIZE = 4096;
        vector<unsigned char> buf(REC*BLOCK_SIZE);

        while(true)
        {
            in.read((char*)buf.data(), buf.size());

            streamsize got = in.gcount();
            if(got<=0) break;
            if(got % (streamsize)REC != 0) throw runtime_error("Truncated record block");

            size_t nrec = (size_t)got / REC;

            for(size_t i=0;i<nrec;i++)
            {
                const unsigned char* p = buf.data() + i*REC;
                uint64_t ts = be64toh_u(p);
                uint8_t type = p[8];

                const char* client = (const char*)(p + 24 + 12 + 12);
                uint32_t errc = be32toh_u(p + 24 + 12 + 12 + 16);

                if(m.total_events==0){ m.first_ts = ts; }
                m.total_events++; m.last_ts = ts;
                eps.add(ts);

                if(type==4) 
                    m.total_errors++;
                else if(type==1)
                {
                    Key16 k; memcpy(k.b, client, 16);
                    new_by_id.put(k, ts);
                } 
                else if(type==2)
                {
                    Key16 k; memcpy(k.b, client, 16);
                    uint64_t new_ts;
                    if(new_by_id.erase_get(k, new_ts)){
                        uint64_t lat = (ts >= new_ts) ? (ts - new_ts) : 0ULL;
                        q50.insert(lat); q90.insert(lat); q99.insert(lat);
                    }
                }
            }
        }

        m.peak_eps = eps.getPeak();
        m.p50 = q50.estimate(); m.p90 = q90.estimate(); m.p99 = q99.estimate();
        return m;
    }
};

void write_metrics_json(const Metrics &m, const string &out_path)
{
    double runtime_seconds = 0.0;

    if(m.last_ts > m.first_ts)
        runtime_seconds = (double)(m.last_ts - m.first_ts) / 1e9; 

    double average_eps = (runtime_seconds>0.0) ? (double)m.total_events / runtime_seconds : 0.0;

    ofstream o(out_path, ios::binary);
    if(!o) throw runtime_error("Failed to open --out file");
    o.setf(ios::fixed);
    
    o<<setprecision(6);
    o << "{\n";
    o << "  \"total_events\": " << m.total_events << ",\n";
    o << "  \"total_errors\": " << m.total_errors << ",\n";
    o << "  \"runtime_seconds\": " << runtime_seconds << ",\n";
    o << "  \"average_eps\": " << average_eps << ",\n";
    o << "  \"peak_1s_window_eps\": " << m.peak_eps << ",\n";
    o << "  \"latency_new_confirm_p50_ns\": " << m.p50 << ",\n";
    o << "  \"latency_new_confirm_p90_ns\": " << m.p90 << ",\n";
    o << "  \"latency_new_confirm_p99_ns\": " << m.p99 << "\n";
    o << "}\n";
}

int main(int argc, char **argv)
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string input_path, out_path;
    for(int i=1;i<argc;i++)
    {
        string a = argv[i];
        if(a=="--input" && i+1<argc){ input_path = argv[++i]; }
        else if(a=="--out" && i+1<argc){ out_path = argv[++i]; }
    }


    if((input_path.empty()) || (out_path.empty()))
    {
        cerr << "Usage: monitor --input <orders.monbin>|- --out <metrics.json>\n";
        return 2; 
    }

    unique_ptr<istream> inptr;
    if(input_path=="-" || input_path=="stdin")
    {
        inptr = make_unique<istream>(cin.rdbuf());
    }
    else 
    {
        auto f = make_unique<ifstream>(input_path, ios::binary);
        if(!*f){ cerr<<"Failed to open input file\n"; return 2; }
        inptr = std::move(f);
    }

    size_t map_cap = 1<<20;

    try
    {
        Processor P(map_cap);
        Metrics M = P.run(*inptr);
        write_metrics_json(M, out_path);
    }
    catch(const exception &ex)
    {
        cerr << "Error: " << ex.what() << "\n"; return 1;
    }
    return 0;
}