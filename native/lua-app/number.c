/* Bounded seven-significant-digit conversion for the 32-bit Lua number type.
 * Newlib nano omits float printf; no variadic formatting is needed here. */
#include <stdint.h>
#include <string.h>
#include <math.h>
int runtime_number(char *out, unsigned size, float value) {
    char buffer[40], digits[8]; unsigned n=0;
    if (isnan(value)) { strcpy(buffer,"nan"); n=3; }
    else if (isinf(value)) { strcpy(buffer,value < 0 ? "-inf" : "inf"); n=strlen(buffer); }
    else {
        if (signbit(value)) { buffer[n++]='-'; value=-value; }
        if (value == 0) buffer[n++]='0';
        else {
            int exponent=0;
            while (value >= 10) { value *= 0.1f; ++exponent; }
            while (value < 1) { value *= 10; --exponent; }
            unsigned mantissa=(unsigned)(value*1000000.0f+0.5f);
            if (mantissa >= 10000000) { mantissa/=10; ++exponent; }
            for (int i=6;i>=0;--i) { digits[i]='0'+mantissa%10; mantissa/=10; }
            unsigned count=7;
            while (count>1 && digits[count-1]=='0') --count;
            if (exponent >= -4 && exponent < 7) {
                int point=exponent+1;
                if (point <= 0) {
                    buffer[n++]='0'; buffer[n++]='.';
                    for (int i=0;i<-point;++i) buffer[n++]='0';
                    for (unsigned i=0;i<count;++i) buffer[n++]=digits[i];
                } else {
                    for (int i=0;i<point;++i) buffer[n++]=(unsigned)i<count ? digits[i] : '0';
                    if ((unsigned)point<count) {
                        buffer[n++]='.';
                        for (unsigned i=point;i<count;++i) buffer[n++]=digits[i];
                    }
                }
            } else {
                buffer[n++]=digits[0];
                if (count>1) buffer[n++]='.';
                for (unsigned i=1;i<count;++i) buffer[n++]=digits[i];
                buffer[n++]='e'; buffer[n++]=exponent<0 ? '-' : '+';
                if (exponent<0) exponent=-exponent;
                buffer[n++]='0'+exponent/10; buffer[n++]='0'+exponent%10;
            }
        }
    }
    if (n+1>size) return 0;
    memcpy(out,buffer,n); out[n]=0; return n;
}
