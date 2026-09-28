/* Host checks exercise the same Lua limits; device execution is tested separately. */
#include <assert.h>
#include <stdio.h>
#define __data_start host__data_start
#define __data_end host__data_end
#define __data_load host__data_load
#define __bss_start host__bss_start
#define __bss_end host__bss_end
#include "runtime.c"
unsigned char __data_start[1], __data_end[1], __data_load[1], __bss_start[1], __bss_end[1];
static int allocation_limit = -1;
void *stock_alloc(unsigned n) {
    if (allocation_limit == 0) return NULL;
    if (allocation_limit > 0) --allocation_limit;
    unsigned char *p = malloc(n + 4);
    return p ? p + 4 : NULL;
}
void stock_free(void *p) { if (p) free((unsigned char *)p - 4); }
void stock_heap_init(void) {}
void *stock_task(void (*f)(void *),const char *n,void *a,unsigned w,unsigned p,unsigned i) {
    (void)f;(void)n;(void)a;(void)w;(void)p;(void)i;return (void *)1;
}
void runtime_worker_entry(void *arg) { (void)arg; }
void stock_delay(unsigned n) { (void)n; }
unsigned stock_volume(unsigned n) { return n == 255 ? 3 : n; }
unsigned stock_event(void *p) { (void)p;return 0; }
void stock_version(unsigned c) { (void)c; }
void stock_set_version(const unsigned char *p) { (void)p;assert(0); }
void stock_reply(unsigned c,unsigned o,const void *d,unsigned n) { (void)c;(void)o;(void)d;(void)n; }
static void check(const char *source, unsigned state, const char *result) {
    memset(&job,0,sizeof job);
    strcpy(job.source,source); job.source_size = strlen(source);
    execute();
    assert(job.state == state);
    if (result) assert(strcmp(job.result,result) == 0);
    assert(job.used == 0);
    assert(job.peak <= MEMORY_LIMIT);
}
int main(void) {
    check("return 6*7",2,"42");
    check("return 7/2",2,"3.5");
    check("return 2147483647",2,"2147483647");
    check("local n=0; for i=1,100 do n=n+i end; return n",2,"5050");
    check("return 'hello from Lua'",2,"hello from Lua");
    check("return volume()",2,"3");
    check("brightness(0); return true",2,"true");
    check("return function (",3,NULL);
    check("while true do end",3,"instruction budget exceeded");
    check("local t={}; for i=1,10000 do t[i]={i,i,i,i} end",3,NULL);
    check("local function f() return 1+f() end; return f()",3,NULL);
    check("return io.open('x')",3,NULL);
    for (int limit=0;limit<80;++limit) {
        allocation_limit=limit;
        memset(&job,0,sizeof job); strcpy(job.source,"return 42");job.source_size=9;
        execute();assert(job.state==2 || job.state==3);assert(job.used==0);
    }
    puts("Lua execution, limits, and allocation-failure checks passed");
}
