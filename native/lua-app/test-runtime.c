/* ASan/UBSan tests use the exact patched interpreter and worker implementation. */
#include <assert.h>
#include <stdio.h>
#define __data_start host__data_start
#define __data_end host__data_end
#define __data_load host__data_load
#define __bss_start host__bss_start
#define __bss_end host__bss_end
#define ASSET_TEST_ROM
#include "runtime.c"
#include "storage.c"
#include "bluetooth-trace.c"
#include "bluetooth-advertising.c"
unsigned runtime_irq_save(void) { return 1; }
void runtime_irq_restore(unsigned irq) { (void)irq; }
unsigned char __data_start[1], __data_end[1], __data_load[1], __bss_start[1], __bss_end[1];
static unsigned clock_ms, allocations, frames, event_delay_ms, load_delay_ms;
static unsigned free_heap = 100000, led_writes;
static unsigned char led_context[0x50], last_leds[36];
static int fail_alloc, track_heap;
static unsigned fail_allocation_at;
static unsigned allocated_bytes, tasks_created, tasks_deleted, fail_task;
void *stock_alloc(unsigned n) {
    if (app.guarded && app.budget_ms==LOAD_TIME_LIMIT) { clock_ms+=load_delay_ms;load_delay_ms=0; }
    if (fail_alloc || (fail_allocation_at && !--fail_allocation_at)) return NULL;
    unsigned char *p = malloc(n+4);
    if (p) { ++allocations; *(unsigned *)p=n;allocated_bytes+=n; }
    return p ? p+4 : NULL;
}
void stock_free(void *p) { if (p) { --allocations;allocated_bytes-=*(unsigned *)((char *)p-4);free((char *)p-4); } }
unsigned stock_free_heap(void) { return free_heap-(track_heap ? allocated_bytes : 0); }
unsigned char *runtime_led_context(void) { return led_context; }
void stock_led_write(const void *p, unsigned n) {
    assert(n==sizeof last_leds);memcpy(last_leds,p,n);++led_writes;
}
void stock_heap_init(void) {}
void *stock_task(void (*f)(void *),const char *n,void *a,unsigned w,unsigned p,unsigned i) {
    assert(f==runtime_worker_entry && !strcmp(n,"lua_app") && !a && w==4096 && p==1 && !i);
    if(fail_task) return NULL;++tasks_created;return (void *)1;
}
void stock_task_delete(void *task) { assert(task==(void *)1 && app.task!=task);++tasks_deleted; }
void runtime_worker_entry(void *p) { (void)p; }
void stock_delay(unsigned n) { clock_ms += n; }
unsigned stock_ticks(void) { return clock_ms; }
unsigned stock_volume(unsigned n) { return n == 255 ? 3 : n; }
static void fake_menu_open(void);
unsigned stock_event(void *p) { clock_ms+=event_delay_ms;if(((unsigned char *)p)[2]==22) fake_menu_open();return 0; }
void stock_calendar(void *p) { unsigned char t[8] = {0xea,7,9,28,12,34,56,1}; memcpy(p,t,8); }
unsigned stock_screen_command(unsigned c,const void *p,unsigned n) { (void)c;(void)p;(void)n;return 1; }
unsigned runtime_screen(const void *p) { assert(p == app.frame); ++frames; return 1; }
void stock_version(unsigned c) { (void)c; }
void stock_set_version(const unsigned char *p) { (void)p; }
static unsigned char reply[1200];
static unsigned reply_size;
static uint32_t fs_context[8] = {0,0x9200,0x9000,0,0x9100,0,0,1760};
static unsigned page_reads, compactions;
unsigned char stock_asset_rom[0x80000];
static unsigned font_reads,font_page,font_bad_layout,font_read_error;
static unsigned char config_context[20];
unsigned char *volatile stock_config_context;
static unsigned char persisted[8][SOURCE_LIMIT+24];
static unsigned persisted_size[8],persist_writes,persist_torn;
unsigned stock_partition(unsigned kind,unsigned *p) {
    if (kind==3) { *p=font_bad_layout ? 0 : 0x1f30;return 0x119000; }
    assert(kind==5);*p=0x6000;return 0;
}
unsigned stock_config_find(unsigned model,unsigned id,void *e,void *scratch) {
    assert(model==PERSIST_MODEL && id<8);(void)scratch;
    if (!persisted_size[id]) return 0xffff;
    unsigned char *p=e;memset(p,0,8);p[0]=model;p[1]=id;
    p[2]=(persisted_size[id]+4+255)/256;uint16_t page=id*100;memcpy(p+4,&page,2);return id;
}
void stock_config_entry(void *entry,unsigned index,void *scratch) {
    unsigned char *out=entry,*in=(unsigned char *)scratch+1+(index%51)*5;
    memset(out,0,8);memcpy(out,in,3);memcpy(out+4,in+3,2);
}

void stock_config_compact(void) {
    ++compactions;unsigned used=0;
    for(unsigned i=0;i<8;++i) used+=(persisted_size[i]+4+255)/256;
    memcpy(config_context+4,&used,4);config_context[2]=config_context[0]>51 ? 2 : 1;config_context[3]=0;
}
unsigned char *runtime_fs_context(void) { return (unsigned char *)fs_context; }
unsigned stock_page_read(unsigned page, void *data, unsigned n) {
    if (page>=0x1f30 && page<0x30c0) {
        assert(n==1);++font_reads;font_page=page;
        for (unsigned i=0;i<256;++i) ((unsigned char *)data)[i]=i;
        return font_read_error;
    }
    if(page>=0x6000 && page<0x6100) {
        assert(n==1);unsigned char *p=data;memset(p,0,256);unsigned at=1;
        for(unsigned i=0;i<8;++i) if(persisted_size[i]) {
            p[at]=PERSIST_MODEL;p[at+1]=i;p[at+2]=(persisted_size[i]+4+255)/256;at+=5;
        }
        return 0;
    }
    if (page>=0x6100 && page<0x6500) {
        assert(n==1);unsigned slot=(page-0x6100)/100;assert(slot<8);
        unsigned char *p=data;memset(p,255,256);p[0]=persisted_size[slot];p[1]=persisted_size[slot]>>8;
        memcpy(p+2,persisted[slot],persisted_size[slot]<254 ? persisted_size[slot] : 254);return 0;
    }
    assert(page>=0x9000 && page+n<=0x9200 && n>0 && n<=4);
    ++page_reads; memset(data,0xff,n*256);return 0;
}
void stock_reply(unsigned c,unsigned o,const void *p,unsigned n) {
    (void)c;assert(o == 0x37); assert(n <= sizeof reply); memcpy(reply,p,n);reply_size=n;
}

#include "test-peripherals-stubs.c"
#include "test-menu-stubs.c"

static void load(const char *source, unsigned resident) {
    runtime_native_service(); clock_ms += 100;
    assert(!app.L && !allocations);
    memset(&app,0,sizeof app);
    app.source_size = strlen(source); assert(app.source_size <= SOURCE_LIMIT);
    int failure = fail_alloc; fail_alloc = 0;
    app.source = source_new(app.source_size); assert(app.source);
    fail_alloc = failure;
    source_copy(app.source,0,(void *)source,app.source_size,1); app.resident = resident; app.state = RUNNING;
    launch();
    assert(app.peak <= MEMORY_LIMIT);
}
static void check(const char *source, unsigned state, const char *value) {
    load(source,0);
    if (app.state != state || (value && strcmp(app.result,value))) {
        fprintf(stderr,"source: %s\nstate %u result %s\n",source,app.state,app.result); abort();
    }
    assert(!app.owner && !app.L && !app.work && !allocations);
}
static void tick(void) { clock_ms += FRAME_MS; service(); }

#include "test-peripherals.c"
#include "test-advertising.c"
#include "test-persistence.c"
#include "test-menu.c"
#include "test-allocator.c"
#include "test-source.c"
#include "test-assets.c"
#include "test-lifecycle.c"

int main(void) {
    test_advertising();
    test_lifecycle();
    test_assets();
    test_allocator();
    test_source();
    test_persistence();
    check("return 6*7",DONE,"42");
    check("return 7/2",DONE,"3.5");
    check("return math.floor(math.sqrt(81))",DONE,"9");
    check("return string.upper('ditoo')",DONE,"DITOO");
    check("return tostring(time.calendar().year)",DONE,"2026");
    check("return io or os or package or debug or load or string.dump or string.format",DONE,"nil");
    check("return device.stats().free_heap",DONE,"100000");
    check("assert(rawget(_G,'audio')==nil);local a=audio;assert(a==audio and rawget(_G,'audio')==a);"
        "assert(lights.count==12 and _G[false]==nil and unknown_native_api==nil);"
        "assert(not pcall(setmetatable,_G,{}));return type(device.result)",DONE,"function");
    check("local function outer(x) local function inner(y) return x+y end return inner end return outer(19)(23)",DONE,"42");
    check("local function inner() error('line preserved') end\nreturn inner()",ERROR,"[string \"app\"]:1: line preserved");
    free_heap = STARTUP_HEAP_BUDGET+STOCK_HEAP_RESERVE-1024;
    check("return 1",ERROR,"insufficient stock heap headroom");free_heap=100000;
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
        "local t=setmetatable({}, {__gc=function() while true do end end});t=nil;microphone.record()",
        "local function f() return coroutine.wrap(f)() end; return f()",
        NULL
    };
    for (int i=0;attacks[i];++i) {
        check(attacks[i],ERROR,NULL);
        check("return 42",DONE,"42");
    }
    check("return function (",ERROR,NULL);
    check("\x1bLua",ERROR,NULL);
    fail_alloc=1; check("return 1",ERROR,"Lua allocation failed"); fail_alloc=0;
    /* Launch work cannot spend init's callback budget. Both phases remain
     * independently bounded, including malicious init callbacks. */
    load_delay_ms=80;check("local n=0;for i=1,256 do n=n+i end;return 42",DONE,"42");
    load_delay_ms=300;check("return 42",ERROR,"load time budget exceeded");
    event_delay_ms=30;
    load("brightness(20);return {init=function() brightness(20);local n=0;for i=1,256 do n=n+i end end}",1);
    assert(app.state==ACTIVE);discard(DONE);
    load("return {init=function() brightness(20);brightness(20);local n=0;for i=1,256 do n=n+i end end}",1);
    assert(app.state==ERROR && !strcmp(app.result,"callback time budget exceeded"));
    check("brightness(20);brightness(20);local n=0;for i=1,256 do n=n+i end;return n",ERROR,"callback time budget exceeded");
    event_delay_ms=0;
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
    unsigned before_frames=frames;
    check("display.clear(1); display.present(); return 42",DONE,"42");
    assert(frames==before_frames+1);
    load("return {init=function() lights.fill(0x123456); lights.pixel(0,0xabcd01); lights.present() end}",1);
    tick();assert(led_context[0x48] && runtime_led_override());
    assert(led_writes==1 && last_leds[0]==0xcd && last_leds[1]==0xab && last_leds[2]==1);
    assert(last_leds[3]==0x34 && last_leds[4]==0x12 && last_leds[5]==0x56);
    app.action=1;tick();assert(!runtime_led_override());
    app.action=2;tick();assert(runtime_led_override());
    app.cancel=1;service();assert(!runtime_led_override() && led_context[0x48]);
    load("return {init=function() lights.fill(0xffffff); lights.enabled(false) end}",1);
    tick();assert(runtime_led_override());
    for (unsigned i=0;i<sizeof last_leds;++i) assert(last_leds[i]==0);
    app.cancel=1;service();
    load("return {init=function() lights.fill(0x080008); lights.present() end,"
         "message=function() while true do end end}",1);
    tick();assert(runtime_led_override());
    memcpy(app.work->message,"crash",5);app.message_size=5;
    tick();assert(app.state==ERROR && !allocations && !runtime_led_override());
    assert(led_context[0x48]);
    /* Cancellation remains independent of callback dispatch. */
    load("return {}",1);app.cancel=1;service();assert(app.state==DONE && !allocations);
    /* Corrupted upload sequencing cannot start partial or oversized programs. */
    memset(&app,0,sizeof app);
    unsigned char request[524]={0x37,0x7f,'D','L','U','A',3,9,0,1};
    runtime_command(0,request,12);assert(reply[6]==0 && app.state==UPLOADING);
    assert(app.source && allocations==2);
    struct source *pending_source=app.source;
    fail_alloc=1;runtime_command(0,request,12);fail_alloc=0;
    assert(reply[6]==3 && app.source==pending_source && allocations==2);
    runtime_command(0,request,12);assert(reply[6]==0 && allocations==2);
    request[6]=2;runtime_command(0,request,9);
    assert(app.state==IDLE && !app.source && !allocations);
    request[6]=3;runtime_command(0,request,12);assert(reply[6]==0 && allocations==2);
    request[6]=5;runtime_command(0,request,9);assert(reply[6]==2);
    request[6]=4;request[7]=1;runtime_command(0,request,12);assert(reply[6]==1);
    request[7]=0;memcpy(request+9,"return {}",9);
    runtime_command(0,request,20);assert(reply[6]==0 && app.received==9);
    request[6]=5;runtime_command(0,request,9);assert(reply[6]==0 && app.state==RUNNING);
    launch();assert(app.state==ACTIVE && !app.source);app.cancel=1;service();assert(!allocations);
    for (unsigned n=0;n<sizeof request;++n) {
        request[6]=4;runtime_command(0,request,n);
    }
    assert(reply_size>=40);
    request[6]=10;request[7]=0;request[8]=0;request[9]=1;
    runtime_command(0,request,12);
    assert(!memcmp(reply,"DFSP",4) && reply[5]==0 && reply_size==176 && page_reads==1);
    assert(reply[48]==0xff && reply[175]==0xff);
    request[7]=0;request[8]=4;
    runtime_command(0,request,12);assert(reply[5]==1 && page_reads==1);
    request[7]=0;request[8]=0;request[9]=0;
    runtime_command(0,request,12);assert(reply[5]==0 && reply_size==48 && page_reads==1);
    fs_context[2]=0;runtime_command(0,request,12);assert(reply[5]==4 && page_reads==1);
    request[6]=12;request[7]=0;request[8]=0;request[9]=0;
    stock_config_context=NULL;runtime_command(0,request,12);
    assert(!memcmp(reply,"DCFG",4) && reply[5]==3 && reply_size==48);
    stock_config_context=config_context;runtime_command(0,request,12);
    assert(reply[5]==0 && reply_size==48 && !memcmp(reply+16,config_context,20));
    request[8]=10;runtime_command(0,request,12);assert(reply[5]==1);
    request[8]=0;runtime_command(0,request,11);assert(reply[5]==1);
    config_context[12]=1;runtime_command(0,request,12);assert(reply[5]==4);config_context[12]=0;
    stock_config_context=NULL;
    test_peripherals();
    test_keyboard_bonds();
    test_tv_keyboard();
    const char *bundles[]={"ui-test","bundle-test","tree-test"};
    const char *results[]={"UI helpers passed","Bundle execution passed","Tree shaking passed"};
    for(unsigned i=0;i<3;++i) {
        char path[128],source[8193];snprintf(path,sizeof path,"target/lua-app/runtime/%s.bundle.lua",bundles[i]);
        FILE *f=fopen(path,"rb");assert(f);size_t n=fread(source,1,sizeof source-1,f);
        assert(!ferror(f) && feof(f));fclose(f);source[n]=0;check(source,DONE,results[i]);
    }
    puts("Bundled modules: deduplication, lazy evaluation, cached values, lexical boundaries and UI rendering passed");
    test_keyboard_lifecycle();
    test_keyboard_profile();
    load("return {}",1);free_heap=STOCK_HEAP_RESERVE;
    assert(!allocate(NULL,NULL,0,2048));free_heap=100000;
    app.cancel=1;service();runtime_native_service();assert(!allocations);
    puts("Resident lifecycle, upload, keys, arena reclamation, and adversarial guard checks passed");
    test_menu();
}
