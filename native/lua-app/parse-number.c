/* Locale-independent binary32 conversion for Lua. Integer arithmetic keeps
 * newlib's double parser, heap-backed bignums and assertion I/O out of flash.
 * Every binary32 rounding boundary has fewer than 192 significant decimal
 * digits; a sticky tail therefore suffices even for very long input strings. */
#include <stdint.h>
#include <string.h>

extern void runtime_poll(void);
#define WORDS 64
struct natural { uint16_t w[WORDS]; unsigned n; };

static void multiply(struct natural *a,unsigned m,unsigned carry) {
    for(unsigned i=0;i<a->n;++i) {
        unsigned v=a->w[i]*m+carry;a->w[i]=v;carry=v>>16;
    }
    if(carry) a->w[a->n++]=carry;
}
static void shift(struct natural *a,unsigned bits) {
    while(bits>=15) { multiply(a,32768,0);bits-=15; }
    if(bits) multiply(a,1U<<bits,0);
}
static int compare(const struct natural *a,const struct natural *b) {
    if(a->n!=b->n) return a->n>b->n ? 1 : -1;
    for(unsigned i=a->n;i--;) if(a->w[i]!=b->w[i]) return a->w[i]>b->w[i] ? 1 : -1;
    return 0;
}
static void subtract(struct natural *a,const struct natural *b) {
    unsigned borrow=0;
    for(unsigned i=0;i<a->n;++i) {
        unsigned v=(unsigned)a->w[i]-(i<b->n ? b->w[i] : 0)-borrow;
        a->w[i]=v;borrow=v>>31;
    }
    while(a->n && !a->w[a->n-1]) --a->n;
}
static unsigned bits(const struct natural *a) {
    unsigned n=(a->n-1)*16,v=a->w[a->n-1];
    while(v) { ++n;v>>=1; }return n;
}
static int digit(unsigned c) {
    if(c>='0' && c<='9') return c-'0';
    c|=32;return c>='a' && c<='f' ? (int)(c-'a'+10) : -1;
}
static float number_bits(unsigned value) { float f;memcpy(&f,&value,4);return f; }

float runtime_parse_number(const char *input,char **end) {
    const char *s=input;unsigned sign=0,radix=10,any=0,dot=0,kept=0,sticky=0;
    int exponent=0;
    struct natural a={{0},0},b={{1},1},t;
    *end=(char *)input;
    while(*s==' ' || (*s>='\t' && *s<='\r')) ++s;
    if(*s=='-' || *s=='+') { if(*s=='-') sign=0x80000000U;++s; }
    if(s[0]=='0' && (s[1]=='x' || s[1]=='X')) { radix=16;s+=2; }
    for(;;++s) {
        runtime_poll();
        int d=digit((unsigned char)*s);
        if(d>=0 && (unsigned)d<radix) {
            any=1;
            if(dot) exponent-=radix==16 ? 4 : 1;
            if(!kept && !d) continue;
            if(kept<192) { multiply(&a,radix,d);++kept; }
            else { sticky|=d;exponent+=radix==16 ? 4 : 1; }
        } else if(*s=='.' && !dot) dot=1;
        else break;
    }
    if(!any) return 0;
    *end=(char *)s;
    if((radix==10 && (*s=='e' || *s=='E')) || (radix==16 && (*s=='p' || *s=='P'))) {
        const char *p=s+1;unsigned negative=0,n=0;
        if(*p=='-' || *p=='+') { negative=*p=='-';++p; }
        if(*p>='0' && *p<='9') {
            do { runtime_poll();if(n<20000) n=n*10+*p-'0';++p; } while(*p>='0' && *p<='9');
            exponent+=negative ? -(int)n : (int)n;*end=(char *)p;
        }
    }
    if(!a.n) return number_bits(sign);
    int order=(radix==10 ? (int)kept : (int)bits(&a))+exponent;
    if(order>(radix==10 ? 39 : 129)) return number_bits(sign|0x7f800000U);
    if(order<(radix==10 ? -46 : -150)) return number_bits(sign);
    struct natural *scaled=exponent<0 ? &b : &a;
    unsigned scale=exponent<0 ? -exponent : exponent;
    if(radix==16) shift(scaled,scale);
    else for(unsigned i=0;i<scale;++i) { runtime_poll();multiply(scaled,10,0); }
    int power=(int)bits(&a)-(int)bits(&b);
    if(power>=0) { t=b;shift(&t,power);if(compare(&a,&t)<0) --power; }
    else { t=a;shift(&t,-power);if(compare(&t,&b)<0) --power; }
    if(power>127) return number_bits(sign|0x7f800000U);
    if(power<-150) return number_bits(sign);
    int places=power<-126 ? 149 : 23-power;
    if(places>=0) shift(&a,places);else shift(&b,-places);
    unsigned mantissa=0;
    for(int i=24;i>=0;--i) {
        runtime_poll();t=b;shift(&t,i);
        if(compare(&a,&t)>=0) { subtract(&a,&t);mantissa|=1U<<i; }
    }
    shift(&a,1);int rounding=compare(&a,&b);
    if(rounding>0 || (!rounding && (sticky || (mantissa&1)))) ++mantissa;
    if(power<-126) return number_bits(sign|mantissa);
    if(mantissa==0x1000000U) { mantissa>>=1;++power; }
    return number_bits(sign|((unsigned)(power+127)<<23)|(mantissa&0x7fffffU));
}
