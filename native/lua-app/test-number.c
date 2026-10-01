/* Differential rounding checks include subnormals and long midpoint tails. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
extern float runtime_parse_number(const char *,char **);
void runtime_poll(void) {}
static void check(const char *s) {
    char *a,*b;float actual=runtime_parse_number(s,&a),expected=strtof(s,&b);
    if(memcmp(&actual,&expected,4) || a-s!=b-s) {
        fprintf(stderr,"number %s: %a (%td) != %a (%td)\n",s,actual,a-s,expected,b-s);abort();
    }
}
static uint32_t rng=1;
static unsigned random_word(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
int main(void) {
    const char *edges[]={"", " ", "+", ".", "1e+", "-0e999999", "1e999999", "1e-999999",
        "1.000000059604644775390625", "1.000000178813934326171875",
        "3.40282346638528859811704183484516925440e38", "3.40282356779733661637539395458142568448e38",
        "1.175494350822287507968736537222245677819e-38",
        "7.00649232162408535461864791644958065640130970938257885878534141944895541342930300743319094181060791015625e-46",
        "0x1.000001p0", "0x1.000003p0", "0x1p-150", "-0x1p-149", "0x1.fffffep127",
        "0x1.ffffffp127", "12.5 rest", "0.0.1", "\t -42.25e2", NULL};
    for(unsigned i=0;edges[i];++i) check(edges[i]);
    char s[8192];
    for(unsigned i=0;i<30000;++i) {
        unsigned n=1+random_word()%250,p=0;
        if(i&1) s[p++]='-';
        for(unsigned j=0;j<n;++j) s[p++]='0'+random_word()%10;
        s[p++]='.';
        for(unsigned j=0;j<random_word()%40;++j) s[p++]='0'+random_word()%10;
        snprintf(s+p,sizeof s-p,"e%d",(int)(random_word()%400)-300);check(s);
        uint32_t raw=random_word();float value;memcpy(&value,&raw,4);
        if((raw&0x7f800000U)!=0x7f800000U) {
            snprintf(s,sizeof s,"%.80e",(double)value);check(s);
            snprintf(s,sizeof s,"%a",(double)value);check(s);
        }
    }
    strcpy(s,"1.000000059604644775390625");unsigned n=strlen(s);
    memset(s+n,'0',sizeof s-n-2);s[sizeof s-2]='1';s[sizeof s-1]=0;check(s);
    strcpy(s,"1.000000178813934326171875");n=strlen(s);
    memset(s+n,'0',sizeof s-n-1);s[sizeof s-1]=0;check(s);
    memset(s,'0',sizeof s-1);s[sizeof s-2]='1';s[sizeof s-1]=0;check(s);
    puts("Binary32 decimal/hex parsing: boundary, midpoint, sticky-tail and randomized differential checks passed");
}
