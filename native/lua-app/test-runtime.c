/* ASan/UBSan tests use the exact patched interpreter and worker implementation. */
#include <assert.h>
#include <stdio.h>
#define __data_start host__data_start
#define __data_end host__data_end
#define __data_load host__data_load
#define __bss_start host__bss_start
#define __bss_end host__bss_end
#include "runtime.c"
unsigned char __data_start[1], __data_end[1], __data_load[1], __bss_start[1], __bss_end[1];
static unsigned clock_ms, allocations, frames;
static int fail_alloc;
void *stock_alloc(unsigned n) {
    if (fail_alloc) return NULL;
    unsigned char *p = malloc(n+4);
    if (p) ++allocations;
    return p ? p+4 : NULL;
}
void stock_free(void *p) { if (p) { --allocations; free((char *)p-4); } }
void stock_heap_init(void) {}
void *stock_task(void (*f)(void *),const char *n,void *a,unsigned w,unsigned p,unsigned i) {
    (void)f;(void)n;(void)a;(void)w;(void)p;(void)i;return (void *)1;
}
void runtime_worker_entry(void *p) { (void)p; }
void stock_delay(unsigned n) { clock_ms += n; }
unsigned stock_ticks(void) { return clock_ms; }
unsigned stock_volume(unsigned n) { return n == 255 ? 3 : n; }
unsigned stock_event(void *p) { (void)p;return 0; }
void stock_calendar(void *p) { unsigned char t[8] = {0xea,7,9,28,12,34,56,1}; memcpy(p,t,8); }
unsigned stock_screen_command(unsigned c,const void *p,unsigned n) { (void)c;(void)p;(void)n;return 1; }
unsigned runtime_screen(const void *p) { assert(p == app.frame); ++frames; return 1; }
void stock_version(unsigned c) { (void)c; }
void stock_set_version(const unsigned char *p) { (void)p; }
static unsigned char reply[256];
static unsigned reply_size;
void stock_reply(unsigned c,unsigned o,const void *p,unsigned n) {
    (void)c;assert(o == 0x37); assert(n <= sizeof reply); memcpy(reply,p,n);reply_size=n;
}

static void load(const char *source, unsigned resident) {
    assert(!app.L && !allocations);
    memset(&app,0,sizeof app);
    app.source_size = strlen(source); assert(app.source_size <= SOURCE_LIMIT);
    memcpy(app.source,source,app.source_size); app.resident = resident; app.state = RUNNING;
    launch();
    assert(app.peak <= MEMORY_LIMIT);
}
static void check(const char *source, unsigned state, const char *value) {
    load(source,0);
    if (app.state != state || (value && strcmp(app.result,value))) {
        fprintf(stderr,"source: %s\nstate %u result %s\n",source,app.state,app.result); abort();
    }
    assert(!app.owner && !app.L && !app.arena && !allocations);
}
static void tick(void) { clock_ms += FRAME_MS; service(); }

int main(void) {
    check("return 6*7",DONE,"42");
    check("return 7/2",DONE,"3.5");
    check("return math.floor(math.sqrt(81))",DONE,"9");
    check("return string.upper('ditoo')",DONE,"DITOO");
    check("return tostring(time.calendar().year)",DONE,"2026");
    check("return io or os or package or debug or load or string.dump or string.format",DONE,"nil");
    const char *attacks[] = {
        "while true do end",
        "while true do pcall(function() while true do end end) end",
        "pcall(function() coroutine.wrap(function() while true do end end)() end)",
        "while true do coroutine.resume(coroutine.create(function() while true do end end)) end",
        "pcall(function() local x <close> = setmetatable({}, {__close=function() while true do end end}) end)",
        "while true do local t=setmetatable({}, {__gc=function() while true do end end}) end",
        "pcall(function() tostring(setmetatable({}, {__tostring=function() while true do end end})) end)",
        "table.sort({3,2,1},function() while true do end end)",
        "('abc'):gsub('.',function() while true do end end)",
        "string.match(string.rep('a',100),string.rep('a*',20)..'b')",
        "local t={}; while true do t[#t+1]=string.rep('x',100) end",
        "while true do pcall(string.rep,'a',2147483647) end",
        "local function f() return 1+f() end; return f()",
        "while true do brightness(0) end",
        "local function f() return coroutine.wrap(f)() end; return f()",
        NULL
    };
    for (int i=0;attacks[i];++i) {
        check(attacks[i],ERROR,NULL);
        check("return 42",DONE,"42");
    }
    check("return function (",ERROR,NULL);
    check("\x1bLua",ERROR,NULL);
    fail_alloc=1; check("return 1",ERROR,"Lua arena allocation failed"); fail_alloc=0;
    load("local n=0; return {init=function() display.clear(0); timer.every(40,function() n=n+1 end) end,"
         "update=function(dt) display.pixel(n%16,0,0xff00); display.present(); print(n) end,"
         "key=function(k,e) print(k..':'..e) end, message=function(s) comms.send(s) end}",1);
    assert(app.state==ACTIVE && app.owner);
    for (unsigned i=0;i<100;++i) tick();
    assert(app.frames==100 && app.state==ACTIVE && frames==100);
    assert(strcmp(app.result,"100")==0);
    app.action=1;tick();assert(app.state==PAUSED && !app.owner);
    tick();assert(app.frames==100);
    app.action=2;tick();tick();assert(app.state==ACTIVE && app.frames==101);
    unsigned char down[]={2,0,1}, up[]={2,0,2}, power[]={0,1,1};
    assert(runtime_key(down)==1 && (app.held & 4));
    assert(runtime_key(power)==0);
    assert(runtime_key(up)==1 && !app.held);
    assert(runtime_adc_result(0xff)==0xff);
    assert(runtime_adc_result(1U<<16 | 32)==(1U<<16 | 32));
    assert(runtime_adc_result(1U<<16 | 3)==0xff && (app.held & 8));
    assert(runtime_adc_result(2U<<16 | 3)==0xff && !(app.held & 8));
    for (unsigned i=0;i<100;++i) runtime_key(down);
    assert(app.dropped>0);
    clock_ms+=5000;service();assert(app.state==DONE && !app.owner && !allocations);
    assert(runtime_adc_result(4U<<16 | 2)==0xff);
    assert(runtime_adc_result(5U<<16 | 2)==0xff);
    assert(runtime_adc_result(1U<<16 | 2)==(1U<<16 | 2));
    assert(runtime_adc_result(1U<<16 | 3)==(1U<<16 | 3));
    load("return {update=function() while true do pcall(function() while true do end end) end end}",1);
    assert(app.state==ACTIVE);tick();assert(app.state==ERROR && !allocations && !app.owner);
    check("return 42",DONE,"42");
    /* Cancellation remains independent of callback dispatch. */
    load("return {}",1);app.cancel=1;service();assert(app.state==DONE && !allocations);
    /* Corrupted upload sequencing cannot start partial or oversized programs. */
    memset(&app,0,sizeof app);
    unsigned char request[524]={0x37,0x7f,'D','L','U','A',3,9,0,1};
    runtime_command(0,request,12);assert(reply[6]==0 && app.state==UPLOADING);
    request[6]=5;runtime_command(0,request,9);assert(reply[6]==2);
    request[6]=4;request[7]=1;runtime_command(0,request,12);assert(reply[6]==1);
    request[7]=0;memcpy(request+9,"return {}",9);
    runtime_command(0,request,20);assert(reply[6]==0 && app.received==9);
    request[6]=5;runtime_command(0,request,9);assert(reply[6]==0 && app.state==RUNNING);
    launch();assert(app.state==ACTIVE);app.cancel=1;service();assert(!allocations);
    for (unsigned n=0;n<sizeof request;++n) {
        request[6]=4;runtime_command(0,request,n);
    }
    assert(reply_size>=40);
    puts("Resident lifecycle, upload, keys, arena reclamation, and adversarial guard checks passed");
}
