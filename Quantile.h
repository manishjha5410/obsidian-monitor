#ifndef QUANTILE_H
#define QUANTILE_H

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>


class Quantile
{
    private:
        double q;
        bool initialized = false;

        double n[5];
        double ns[5];
        double d[5];
        double x[5];
        size_t count = 0;

        inline double parabolic(int i, int s)
        {
            double n_i = n[i], n_im1 = n[i-1], n_ip1 = n[i+1];
            double x_i = x[i], x_im1 = x[i-1], x_ip1 = x[i+1];
            double a = s*(n_i - n_im1 + s) * (x_ip1 - x_i) / (n_ip1 - n_i);
            double b = s*(n_ip1 - n_i - s) * (x_i - x_im1) / (n_i - n_im1);
            return x_i + (a + b) / (n_ip1 - n_im1);
        }
        
        inline double linear(int i)
        {
            return (x[i + (int) ( (ns[i] - n[i] > 0) ? 1 : -1 )] - x[i]) / (n[i + (int)( (ns[i]-n[i] > 0) ? 1 : -1 )] - n[i]);
        }
    public:
        explicit Quantile(double quant): q(quant){}
        
        inline void reset()
        { 
            initialized=false;
            count=0;
        }

        inline void insert(uint64_t v)
        {
            double value = (double)v;

            if(!initialized)
            {
                x[count++] = value;
                if(count==5)
                {
                    std::sort(x, x+5);
                    for(int i=0;i<5;i++){ n[i]=i+1; }
                    ns[0]=1; ns[1]=1+2*q; ns[2]=1+4*q; ns[3]=3+2*q; ns[4]=5;
                    d[0]=0; d[1]=q/2; d[2]=q; d[3]=(1+q)/2; d[4]=1;
                    initialized=true;
                }
                return;
            }

            count++;

            int k;
            if(value < x[0]){ x[0]=value; k=0; }
            else if(value < x[1]) k=0;
            else if(value < x[2]) k=1;
            else if(value < x[3]) k=2;
            else if(value <= x[4]) k=3;
            else { x[4]=value; k=3; }
            for(int i=k+1;i<5;i++) n[i] += 1.0;
            for(int i=0;i<5;i++) ns[i] += d[i];

            for(int i=1;i<=3;i++)
            {
                double di = ns[i] - n[i];
                if((di>=1 && n[i+1]-n[i]>1) || (di<=-1 && n[i-1]-n[i]<-1))
                {
                    int s = (di>=0)? 1 : -1;
                    double qp = parabolic(i, s);
                    if(qp > x[i-1] && qp < x[i+1])
                    {
                        x[i] = qp;
                    }
                    else
                    {
                        x[i] = x[i] + s * linear(i);
                    }
                    n[i] += s;
                }
            }
        }
        
        inline uint64_t estimate()
        {
            if(!initialized)
            {
                if(count==0)
                    return 0;
                
                double tmp[5];
                for(size_t i=0;i<count;i++) 
                    tmp[i]=x[i]; 
                
                std::sort(tmp,tmp+count);
                size_t idx = (size_t)floor(q*(count-1)+0.5);
                
                return (uint64_t) llround(tmp[idx]);
            }
            return (uint64_t) llround(x[2]);
        }
};

#endif
