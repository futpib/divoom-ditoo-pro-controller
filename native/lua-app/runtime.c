/* One worker owns Lua. Interrupts and protocol handlers never enter the VM. */
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SOURCE_LIMIT 8192
#define RESULT_LIMIT 192
#define MEMORY_LIMIT 40960
#define STEP_LIMIT 100000
#define TIME_LIMIT 50
#define FRAME_MS 40
#define KEY_COUNT 32
#define QUEUE_SIZE 16
#define TIMER_COUNT 12
#define MESSAGE_LIMIT 128
#define LED_COUNT 12
#define STOCK_HEAP_RESERVE 24576

extern void *stock_alloc(unsigned);
extern void stock_free(void *);
extern unsigned stock_free_heap(void);
extern void *stock_task(void (*)(void *), const char *, void *, unsigned, unsigned, unsigned);
extern void stock_delay(unsigned);
extern unsigned stock_ticks(void);
extern unsigned stock_volume(unsigned);
extern unsigned stock_event(void *);
extern void stock_calendar(void *);
extern unsigned stock_screen_command(unsigned, const void *, unsigned);
extern unsigned runtime_screen(const void *);
extern unsigned char *runtime_led_context(void);
extern void stock_led_write(const void *, unsigned);
extern void stock_version(unsigned);
extern void stock_set_version(const unsigned char *);
extern void stock_reply(unsigned, unsigned, const void *, unsigned);
extern void stock_heap_init(void);
extern void runtime_worker_entry(void *);
extern void runtime_storage_diagnostic(unsigned, const unsigned char *, unsigned);
extern unsigned char __data_start[], __data_end[], __data_load[], __bss_start[], __bss_end[];

enum { IDLE, RUNNING, DONE, ERROR, ACTIVE, PAUSED, UPLOADING };
struct key_event { unsigned char key, event, kind, pad; };
struct timer { int ref; unsigned deadline, interval; };
struct block { unsigned size, free; };
static struct {
    volatile unsigned state, cancel, action, owner, key_read, key_write, dropped;
    volatile unsigned held, suppressed;
    unsigned held_since[KEY_COUNT];
    struct key_event keys[QUEUE_SIZE];
    volatile unsigned message_size;
    char message[MESSAGE_LIMIT];
    char outbox[MESSAGE_LIMIT];
    unsigned outbox_size;
    void *task, *arena_base;
    unsigned char *arena;
    lua_State *L;
    int app_ref;
    unsigned resident, source_size, received, used, peak, steps, started, guarded;
    unsigned last_tick, frames, callbacks, dirty, native_calls, generation;
    char source[SOURCE_LIMIT];
    char result[RESULT_LIMIT];
    unsigned char frame[768];
    unsigned char leds[LED_COUNT * 3], led_buffers[2][LED_COUNT * 3];
    volatile unsigned led_owner, led_active;
    unsigned led_enabled, led_dirty;
    struct timer timers[TIMER_COUNT];
    jmp_buf escape;
} app;

void runtime_init(void) {
    stock_heap_init();
    memcpy(__data_start, __data_load, __data_end - __data_start);
    memset(__bss_start, 0, __bss_end - __bss_start);
}

static void result(const char *s) {
    size_t n = strlen(s);
    if (n >= RESULT_LIMIT) n = RESULT_LIMIT - 1;
    memmove(app.result, s, n); app.result[n] = 0;
}

/* This jump target is private to the worker, outside all Lua pcall boundaries.
 * An aborted VM is discarded as an arena; no finalizer is run during teardown. */
static void abort_script(const char *why) {
    result(why);
    longjmp(app.escape, 1);
}

void runtime_poll(void) {
    if (!app.guarded) return;
    ++app.steps;
    if (app.cancel) abort_script("cancelled");
    if (app.steps >= STEP_LIMIT) abort_script("instruction budget exceeded");
    if (!(app.steps & 127) && (unsigned)(stock_ticks() - app.started) >= TIME_LIMIT)
        abort_script("callback time budget exceeded");
}

/* The quota includes headers, alignment and realloc's temporary old allocation.
 * Coalescing is bounded by the number of blocks in the fixed-size arena. */
static void *allocate(void *ud, void *ptr, size_t old, size_t size) {
    (void)ud;
    runtime_poll();
    if (!size) {
        if (ptr) {
            struct block *b = (struct block *)ptr - 1;
            b->free = 1; app.used -= b->size;
        }
        return NULL;
    }
    if (size > MEMORY_LIMIT - sizeof(struct block)) return NULL;
    unsigned needed = (size + 7) & ~7U;
    needed += sizeof(struct block);
    if (ptr && needed <= ((struct block *)ptr - 1)->size) return ptr;
    for (unsigned offset = 0; offset < MEMORY_LIMIT;) {
        struct block *b = (struct block *)(app.arena + offset);
        if (b->free) {
            while (offset + b->size < MEMORY_LIMIT) {
                struct block *next = (struct block *)(app.arena + offset + b->size);
                if (!next->free) break;
                b->size += next->size;
            }
            if (b->size >= needed) {
                if (b->size >= needed + sizeof(struct block) + 8) {
                    struct block *next = (struct block *)((unsigned char *)b + needed);
                    next->size = b->size - needed; next->free = 1; b->size = needed;
                }
                b->free = 0; app.used += b->size;
                if (app.used > app.peak) app.peak = app.used;
                void *p = b + 1;
                if (ptr) {
                    memcpy(p, ptr, old < size ? old : size);
                    struct block *previous = (struct block *)ptr - 1;
                    previous->free = 1; app.used -= previous->size;
                }
                return p;
            }
        }
        offset += b->size;
    }
    return NULL;
}

static void led_refresh(void) {
    unsigned char *context = runtime_led_context();
    if (context) context[0x48] = 1;
}

static void discard(unsigned state) {
    app.suppressed |= app.held;
    app.guarded = app.owner = app.dirty = 0;
    app.led_owner = app.led_dirty = 0; led_refresh();
    app.L = NULL; app.used = 0; app.held = 0;
    if (app.arena_base) stock_free(app.arena_base);
    app.arena_base = NULL; app.arena = NULL;
    app.key_read = app.key_write; app.message_size = 0;
    memset(app.timers, 0, sizeof app.timers);
    __asm__ volatile ("" ::: "memory");
    app.state = state;
}

/* These hooks only copy bounded records. Power keys always remain native. */
unsigned runtime_key(const unsigned char *raw) {
    if (!raw || raw[1] != 0 || raw[0] >= KEY_COUNT) return 0;
    unsigned key = raw[0], event = raw[2], mask = 1U << key;
    if (app.suppressed & mask) {
        if (event == 2 || event == 5) app.suppressed &= ~mask;
        return 1;
    }
    if (app.state != RUNNING && app.state != ACTIVE && app.state != PAUSED) return 0;
    if (event == 1 || event == 3 || event == 4) {
        if (!(app.held & mask)) app.held_since[key] = stock_ticks();
        app.held |= mask;
    } else if (event == 2 || event == 5) app.held &= ~mask;
    if (!app.owner) return 0;
    unsigned w = app.key_write;
    if ((unsigned)(w - app.key_read) < QUEUE_SIZE) {
        app.keys[w % QUEUE_SIZE] = (struct key_event){key, event, raw[1], 0};
        __asm__ volatile ("" ::: "memory");
        app.key_write = w + 1;
    } else ++app.dropped;
    return 1;
}

/* KeyScan drops presses whose stock action is zero. Intercept the packed ADC
 * result before that mapping, and return the scanner's no-event value when
 * consumed. The independent power-key scanner still runs in stock KeyScan. */
unsigned runtime_adc_result(unsigned packed) {
    unsigned key = packed & 0xffff, event = packed >> 16;
    if (key >= 11 || event < 1 || event > 5) return packed;
    const unsigned char raw[] = {key,0,event};
    return runtime_key(raw) ? 0xff : packed;
}

unsigned runtime_screen_allowed(const void *frame) {
    return !app.owner || frame == app.frame;
}

/* The stock LED task performs I/O. Lua only publishes a complete packed buffer
 * and requests its normal refresh, so no script enters the LED driver. */
unsigned runtime_led_override(void) {
    if (!app.led_owner || app.state != ACTIVE) return 0;
    stock_led_write(app.led_buffers[app.led_active], LED_COUNT * 3);
    return 1;
}

static lua_Integer integer(lua_State *L, int arg, int lo, int hi) {
    lua_Integer n = luaL_checkinteger(L, arg);
    luaL_argcheck(L, n >= lo && n <= hi, arg, "out of range");
    return n;
}

static unsigned color(lua_State *L, int arg) {
    return integer(L, arg, 0, 0xffffff);
}

static void pixel(int x, int y, unsigned rgb) {
    if ((unsigned)x >= 16 || (unsigned)y >= 16) return;
    unsigned char *p = app.frame + (y * 16 + x) * 3;
    p[0] = rgb >> 16; p[1] = rgb >> 8; p[2] = rgb;
}

static int clear(lua_State *L) {
    unsigned c = lua_isnoneornil(L, 1) ? 0 : color(L, 1);
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) pixel(x, y, c);
    return 0;
}

static int setpixel(lua_State *L) {
    pixel(integer(L, 1, -128, 128), integer(L, 2, -128, 128), color(L, 3));
    return 0;
}

static int getpixel(lua_State *L) {
    int x = integer(L, 1, 0, 15), y = integer(L, 2, 0, 15);
    unsigned char *p = app.frame + (y * 16 + x) * 3;
    lua_pushinteger(L, (p[0] << 16) | (p[1] << 8) | p[2]); return 1;
}

static int line(lua_State *L) {
    int x = integer(L, 1, -128, 128), y = integer(L, 2, -128, 128);
    int tx = integer(L, 3, -128, 128), ty = integer(L, 4, -128, 128);
    unsigned c = color(L, 5);
    int dx = abs(tx-x), sx = x < tx ? 1 : -1, dy = -abs(ty-y), sy = y < ty ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pixel(x, y, c); if (x == tx && y == ty) break;
        int e = err * 2;
        if (e >= dy) { err += dy; x += sx; }
        if (e <= dx) { err += dx; y += sy; }
    }
    return 0;
}

static int rect(lua_State *L) {
    int x = integer(L, 1, -128, 128), y = integer(L, 2, -128, 128);
    int w = integer(L, 3, 0, 128), h = integer(L, 4, 0, 128);
    unsigned c = color(L, 5); int fill = lua_toboolean(L, 6);
    for (int j = 0; j < 16; ++j) for (int i = 0; i < 16; ++i)
        if (i >= x && i < x+w && j >= y && j < y+h &&
            (fill || i == x || i == x+w-1 || j == y || j == y+h-1)) pixel(i,j,c);
    return 0;
}

static int blit(lua_State *L) {
    int x = integer(L, 1, -16, 16), y = integer(L, 2, -16, 16);
    int w = integer(L, 3, 1, 16), h = integer(L, 4, 1, 16);
    size_t n; const unsigned char *data = (const unsigned char *)luaL_checklstring(L, 5, &n);
    luaL_argcheck(L, n == (size_t)(w*h*3), 5, "expected packed RGB888");
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) {
        unsigned c = (data[0] << 16) | (data[1] << 8) | data[2]; data += 3;
        pixel(x+i, y+j, c);
    }
    return 0;
}

static int present(lua_State *L) { (void)L; app.dirty = 1; return 0; }
static int frame(lua_State *L) { lua_pushlstring(L, (char *)app.frame, sizeof app.frame); return 1; }

/* Compact 3x5 font, rows in low-to-high groups of three bits. */
static const unsigned short font[] = {
    31599, 29850, 29671, 31207, 18925, 31183, 31695, 18727, 31727, 31215,
    23530, 15083, 29262, 15211, 29391, 4815, 31566, 23533, 29847, 15140,
    23277, 29257, 23549, 23547, 15214, 4843, 26478, 23275, 31182, 9367,
    15213, 11117, 24429, 23213, 9389, 29351
};
static int text(lua_State *L) {
    int x = integer(L, 1, -128, 128), y = integer(L, 2, -128, 128);
    size_t n; const unsigned char *s = (const unsigned char *)luaL_checklstring(L, 3, &n);
    luaL_argcheck(L, n <= 64, 3, "text exceeds 64 bytes");
    unsigned c = color(L, 4);
    for (unsigned k = 0; k < n; ++k, x += 4) {
        unsigned ch = s[k], bits = 0;
        if (ch >= 'a' && ch <= 'z') ch -= 'a'-'A';
        if (ch >= '0' && ch <= '9') bits = font[ch-'0'];
        else if (ch >= 'A' && ch <= 'Z') bits = font[ch-'A'+10];
        else if (ch == ':') bits = 1040;
        else if (ch == '-') bits = 448;
        else if (ch == '.') bits = 8192;
        for (int j = 0; j < 5; ++j) for (int i = 0; i < 3; ++i)
            if (bits & (1U << (j*3+i))) pixel(x+i,y+j,c);
    }
    return 0;
}

static void native_budget(void) {
    runtime_poll();
    if (++app.native_calls > 32) abort_script("device call budget exceeded");
}

static int brightness(lua_State *L) {
    unsigned char value = integer(L, 1, 0, 100);
    struct { uint16_t a; uint8_t group,b,command,c; uint16_t d; void *data; }
        event = {0,0x11,0,4,0,0,&value};
    native_budget(); stock_event(&event); return 0;
}
static int volume(lua_State *L) {
    unsigned n = lua_isnoneornil(L, 1) ? 255 : integer(L, 1, 0, 15);
    native_budget(); lua_pushinteger(L, stock_volume(n)); return 1;
}
static int ticks(lua_State *L) { lua_pushinteger(L, stock_ticks() & 0x7fffffff); return 1; }
static int calendar(lua_State *L) {
    struct { uint16_t year; uint8_t month,day,hour,min,sec,wday; } t;
    stock_calendar(&t);
    lua_createtable(L, 0, 7);
    const char *names[] = {"year","month","day","hour","min","sec","wday"};
    unsigned values[] = {t.year,t.month,t.day,t.hour,t.min,t.sec,t.wday};
    for (int i = 0; i < 7; ++i) { lua_pushinteger(L, values[i]); lua_setfield(L,-2,names[i]); }
    return 1;
}
static int held(lua_State *L) { lua_pushboolean(L, app.held & (1U << integer(L,1,0,31))); return 1; }
static int claim(lua_State *L) { app.owner = lua_toboolean(L,1); return 0; }
static int stop(lua_State *L) { (void)L; app.cancel = 1; return 0; }
static int send(lua_State *L) {
    size_t n; const char *s = luaL_checklstring(L,1,&n);
    luaL_argcheck(L, n <= MESSAGE_LIMIT,1,"message exceeds 128 bytes");
    if (app.outbox_size) { lua_pushboolean(L,0); return 1; }
    memcpy(app.outbox,s,n); app.outbox_size = n; lua_pushboolean(L,1); return 1;
}
static int logging(lua_State *L) {
    size_t n; const char *s = luaL_tolstring(L,1,&n);
    if (n >= RESULT_LIMIT) n = RESULT_LIMIT-1;
    memcpy(app.result,s,n); app.result[n] = 0; return 0;
}

static int led_fill(lua_State *L) {
    unsigned c = color(L,1);
    for (unsigned i=0;i<LED_COUNT;++i) {
        app.leds[i*3] = c >> 16; app.leds[i*3+1] = c >> 8; app.leds[i*3+2] = c;
    }
    return 0;
}
static int led_pixel(lua_State *L) {
    unsigned i = integer(L,1,0,LED_COUNT-1), c = color(L,2);
    app.leds[i*3] = c >> 16; app.leds[i*3+1] = c >> 8; app.leds[i*3+2] = c;
    return 0;
}
static int led_frame(lua_State *L) {
    if (!lua_isnoneornil(L,1)) {
        size_t n; const char *data = luaL_checklstring(L,1,&n);
        luaL_argcheck(L,n==sizeof app.leds,1,"expected 36 RGB888 bytes");
        memcpy(app.leds,data,n);
    }
    lua_pushlstring(L,(const char *)app.leds,sizeof app.leds); return 1;
}
static int led_enable(lua_State *L) {
    if (!lua_isnoneornil(L,1)) { app.led_enabled = lua_toboolean(L,1); app.led_dirty = 1; }
    lua_pushboolean(L,app.led_enabled); return 1;
}
static int led_present(lua_State *L) { (void)L; app.led_dirty = 1; return 0; }
static int led_claim(lua_State *L) {
    app.led_owner = lua_toboolean(L,1);
    if (app.led_owner) app.led_dirty = 1;
    else { app.led_dirty = 0; led_refresh(); }
    return 0;
}
static int stats(lua_State *L) {
    const char *names[] = {"free_heap","lua_used","lua_peak","frames","callbacks","dropped_keys"};
    unsigned values[] = {stock_free_heap(),app.used,app.peak,app.frames,app.callbacks,app.dropped};
    lua_createtable(L,0,6);
    for (unsigned i=0;i<6;++i) { lua_pushinteger(L,values[i]); lua_setfield(L,-2,names[i]); }
    return 1;
}

static void present_outputs(void) {
    if (app.dirty) {
        if (runtime_screen(app.frame) == 1) ++app.frames;
        app.dirty = 0;
    }
    if (app.led_dirty && app.state == ACTIVE) {
        unsigned next = app.led_active ^ 1;
        for (unsigned i=0;i<LED_COUNT;++i) {
            /* The device driver consumes GRB; values are raw 8-bit intensities. */
            app.led_buffers[next][i*3] = app.led_enabled ? app.leds[i*3+1] : 0;
            app.led_buffers[next][i*3+1] = app.led_enabled ? app.leds[i*3] : 0;
            app.led_buffers[next][i*3+2] = app.led_enabled ? app.leds[i*3+2] : 0;
        }
        __asm__ volatile ("" ::: "memory");
        app.led_active = next; app.led_owner = 1; app.led_dirty = 0;
        led_refresh();
    }
}

static int newtimer(lua_State *L, unsigned repeating) {
    unsigned ms = integer(L,1,10,86400000); luaL_checktype(L,2,LUA_TFUNCTION);
    for (unsigned i = 0; i < TIMER_COUNT; ++i) if (!app.timers[i].ref) {
        lua_pushvalue(L,2); int ref = luaL_ref(L,LUA_REGISTRYINDEX);
        app.timers[i] = (struct timer){ref,stock_ticks()+ms,repeating ? ms : 0};
        lua_pushinteger(L,i+1); return 1;
    }
    return luaL_error(L,"timer limit reached");
}
static int after(lua_State *L) { return newtimer(L,0); }
static int every(lua_State *L) { return newtimer(L,1); }
static int untimer(lua_State *L) {
    unsigned i = integer(L,1,1,TIMER_COUNT)-1;
    if (app.timers[i].ref) luaL_unref(L,LUA_REGISTRYINDEX,app.timers[i].ref);
    app.timers[i].ref = 0; return 0;
}

static int panic(lua_State *L) { (void)L; abort_script("Lua panic"); return 0; }
static void module(lua_State *L, const char *name, const luaL_Reg *functions) {
    lua_newtable(L); luaL_setfuncs(L,functions,0); lua_setglobal(L,name);
}
static void remove_field(lua_State *L, const char *key) { lua_pushnil(L); lua_setfield(L,-2,key); }
static int setup(lua_State *L) {
    luaL_requiref(L,"_G",luaopen_base,1);
    /* No filesystem, dynamically loaded code, or mutable interpreter hooks. */
    const char *removed[] = {"dofile","loadfile","load","collectgarbage","print",NULL};
    for (int i = 0; removed[i]; ++i) remove_field(L,removed[i]);
    lua_pop(L,1);
    luaL_requiref(L,"math",luaopen_math,1); lua_pop(L,1);
    luaL_requiref(L,"table",luaopen_table,1); lua_pop(L,1);
    luaL_requiref(L,"coroutine",luaopen_coroutine,1); lua_pop(L,1);
    luaL_requiref(L,"string",luaopen_string,1);
    /* printf widths and binary chunks have no reason to be script-controlled. */
    const char *strings[] = {"format","dump","pack","unpack","packsize",NULL};
    for (int i = 0; strings[i]; ++i) remove_field(L,strings[i]);
    lua_pop(L,1);
    static const luaL_Reg drawing[] = {{"clear",clear},{"pixel",setpixel},{"get",getpixel},
        {"line",line},{"rect",rect},{"blit",blit},{"text",text},{"present",present},{"frame",frame},{NULL,NULL}};
    static const luaL_Reg timing[] = {{"millis",ticks},{"calendar",calendar},{NULL,NULL}};
    static const luaL_Reg timers[] = {{"after",after},{"every",every},{"cancel",untimer},{NULL,NULL}};
    static const luaL_Reg keyboard[] = {{"held",held},{NULL,NULL}};
    static const luaL_Reg device[] = {{"brightness",brightness},{"volume",volume},{"stats",stats},{NULL,NULL}};
    static const luaL_Reg lights[] = {{"fill",led_fill},{"pixel",led_pixel},{"frame",led_frame},
        {"enabled",led_enable},{"present",led_present},{"claim",led_claim},{NULL,NULL}};
    static const luaL_Reg control[] = {{"claim",claim},{"stop",stop},{"log",logging},{NULL,NULL}};
    static const luaL_Reg comms[] = {{"send",send},{NULL,NULL}};
    module(L,"display",drawing); module(L,"time",timing); module(L,"timer",timers);
    module(L,"keys",keyboard); module(L,"device",device); module(L,"app",control); module(L,"comms",comms);
    module(L,"lights",lights);
    lua_getglobal(L,"lights"); lua_pushinteger(L,LED_COUNT); lua_setfield(L,-2,"count"); lua_pop(L,1);
    lua_pushcfunction(L,brightness); lua_setglobal(L,"brightness");
    lua_pushcfunction(L,volume); lua_setglobal(L,"volume");
    lua_pushcfunction(L,logging); lua_setglobal(L,"print");
    return 0;
}

static void begin_budget(void) {
    app.started = stock_ticks(); app.steps = 0; app.native_calls = 0; app.guarded = 1;
}
static void check_call(lua_State *L, int args, int results) {
    if (lua_pcall(L,args,results,0) != LUA_OK) {
        const char *s = lua_type(L,-1) == LUA_TSTRING ? lua_tostring(L,-1) : "Lua error";
        abort_script(s);
    }
}
static int callback(const char *name) {
    lua_State *L = app.L;
    lua_rawgeti(L,LUA_REGISTRYINDEX,app.app_ref);
    lua_pushstring(L,name); lua_rawget(L,-2); lua_remove(L,-2);
    if (lua_isnil(L,-1)) { lua_pop(L,1); return 0; }
    if (!lua_isfunction(L,-1)) abort_script("app callback must be a function");
    ++app.callbacks; return 1;
}
static void scalar(lua_State *L) {
    if (lua_isnil(L,-1)) result("nil");
    else if (lua_isboolean(L,-1)) result(lua_toboolean(L,-1) ? "true" : "false");
    else if (lua_isstring(L,-1)) result(lua_tostring(L,-1));
    else result("non-scalar result");
}
static void launch(void) {
    app.used = app.peak = app.frames = app.callbacks = app.dropped = app.steps = 0;
    app.outbox_size = 0; app.app_ref = 0; app.last_tick = stock_ticks();
    app.led_enabled = 1; app.led_dirty = 0; memset(app.leds,0,sizeof app.leds);
    if (stock_free_heap() < MEMORY_LIMIT + 8 + STOCK_HEAP_RESERVE) {
        result("insufficient stock heap headroom"); discard(ERROR); return;
    }
    app.arena_base = stock_alloc(MEMORY_LIMIT + 8);
    if (!app.arena_base) { result("Lua arena allocation failed"); discard(ERROR); return; }
    app.arena = (void *)(((uintptr_t)app.arena_base+7) & ~(uintptr_t)7);
    *(struct block *)app.arena = (struct block){MEMORY_LIMIT,1};
    if (setjmp(app.escape)) { discard(ERROR); return; }
    begin_budget();
    app.L = lua_newstate(allocate,NULL);
    if (!app.L) abort_script("Lua allocation failed");
    lua_atpanic(app.L,panic);
    lua_pushcfunction(app.L,setup); check_call(app.L,0,0);
    if (luaL_loadbufferx(app.L,app.source,app.source_size,"app","t")) abort_script(lua_tostring(app.L,-1));
    check_call(app.L,0,1);
    if (!app.resident) { scalar(app.L); present_outputs(); discard(DONE); return; }
    if (!lua_istable(app.L,-1)) abort_script("app must return a callback table");
    app.app_ref = luaL_ref(app.L,LUA_REGISTRYINDEX);
    app.owner = 1;
    if (callback("init")) check_call(app.L,0,0);
    app.guarded = 0; app.state = ACTIVE; ++app.generation;
}

static void service(void) {
    unsigned now = stock_ticks();
    for (unsigned k = 0; k < KEY_COUNT; ++k)
        if ((app.held & (1U << k)) && (unsigned)(now-app.held_since[k]) >= 5000) app.cancel = 1;
    if (app.cancel) { result("stopped"); discard(DONE); return; }
    if (app.action) {
        unsigned action = app.action; app.action = 0;
        app.state = action == 1 ? PAUSED : ACTIVE;
        app.owner = action == 1 ? 0 : 1; app.last_tick = now;
        if (app.led_owner) led_refresh();
        app.key_read = app.key_write;
    }
    if (app.state != ACTIVE || (unsigned)(now-app.last_tick) < FRAME_MS) return;
    unsigned dt = now-app.last_tick; app.last_tick = now;
    if (setjmp(app.escape)) { discard(ERROR); return; }
    begin_budget();
    lua_State *L = app.L;
    /* One aggregate budget bounds the entire tick, including queued callbacks. */
    for (unsigned count = 0; count < 4 && app.key_read != app.key_write; ++count) {
        struct key_event key = app.keys[app.key_read % QUEUE_SIZE]; ++app.key_read;
        if (callback("key")) {
            lua_pushinteger(L,key.key); lua_pushinteger(L,key.event); check_call(L,2,0);
        }
    }
    if (app.message_size) {
        if (callback("message")) {
            lua_pushlstring(L,app.message,app.message_size); app.message_size = 0; check_call(L,1,0);
        } else app.message_size = 0;
    }
    for (unsigned i = 0, count = 0; i < TIMER_COUNT && count < 4; ++i) {
        struct timer *t = &app.timers[i];
        if (t->ref && (int32_t)(now-t->deadline) >= 0) {
            lua_rawgeti(L,LUA_REGISTRYINDEX,t->ref);
            if (t->interval) t->deadline = now+t->interval;
            else { luaL_unref(L,LUA_REGISTRYINDEX,t->ref); t->ref = 0; }
            ++count; check_call(L,0,0);
        }
    }
    if (callback("update")) { lua_pushinteger(L,dt); check_call(L,1,0); }
    app.guarded = 0;
    present_outputs();
}

void runtime_worker(void *unused) {
    (void)unused;
    for (;;) {
        if (app.state == RUNNING) launch();
        else if (app.state == ACTIVE || app.state == PAUSED) service();
        stock_delay(10);
    }
}

void runtime_command(unsigned context, const unsigned char *data, unsigned length) {
    if (length < 4) return;
    if (data[1] == 0) { stock_version(0); return; }
    if (length < 9 || data[1] != 0x7f || memcmp(data+2,"DLUA",4)) {
        stock_set_version(data+2); return;
    }
    unsigned op = data[6], error = 0;
    if (op == 10) { runtime_storage_diagnostic(context,data,length); return; }
    unsigned busy = app.state == RUNNING || app.state == ACTIVE || app.state == PAUSED;
    if (op == 1 || op == 3) {
        if (busy) error = 2;
        else if (length < 11) error = 1;
        else {
            unsigned n = data[7] | ((unsigned)data[8] << 8);
            if (!n || n > SOURCE_LIMIT || (op == 1 && length != n+11) || (op == 3 && length != 12)) error = 1;
            else {
                if (!app.task) app.task = stock_task(runtime_worker_entry,"lua_app",NULL,4096,1,0);
                if (!app.task) error = 3;
                else {
                    app.cancel = app.action = 0; app.result[0] = 0;
                    app.source_size = n; app.received = op == 1 ? n : 0;
                    app.resident = op == 3 ? data[9] != 0 : 0;
                    if (op == 1) memcpy(app.source,data+9,n);
                    __asm__ volatile ("" ::: "memory");
                    app.state = op == 1 ? RUNNING : UPLOADING;
                }
            }
        }
    } else if (op == 2) {
        app.cancel = 1;
        if (app.state == UPLOADING) app.state = IDLE;
    } else if (op == 4) {
        if (app.state != UPLOADING) error = 2;
        else if (length < 12) error = 1;
        else {
            unsigned offset = data[7] | ((unsigned)data[8] << 8), n = length-11;
            if (offset != app.received || n > 512 || n > app.source_size-app.received) error = 1;
            else { memcpy(app.source+offset,data+9,n); app.received += n; }
        }
    } else if (op == 5) {
        if (app.state != UPLOADING || app.received != app.source_size) error = 2;
        else { __asm__ volatile ("" ::: "memory"); app.state = RUNNING; }
    } else if (op == 6 || op == 7) {
        if (app.state != ACTIVE && app.state != PAUSED) error = 2;
        else app.action = op == 6 ? 1 : 2;
    } else if (op == 8) {
        unsigned n = length-9;
        if (app.state != ACTIVE || app.message_size) error = 2;
        else if (!n || n > MESSAGE_LIMIT) error = 1;
        else { memcpy(app.message,data+7,n); __asm__ volatile ("" ::: "memory"); app.message_size = n; }
    } else if (op != 0 && op != 9) error = 4;
    unsigned char reply[RESULT_LIMIT+40] = {'D','L','U','A',2};
    reply[5] = app.state; reply[6] = error;
    unsigned n = op == 9 ? app.outbox_size : strlen(app.result);
    reply[7] = n;
    memcpy(reply+8,&app.peak,4); memcpy(reply+12,&app.steps,4);
    memcpy(reply+16,&app.used,4); memcpy(reply+20,&app.frames,4);
    memcpy(reply+24,&app.callbacks,4); memcpy(reply+28,(const void *)&app.dropped,4);
    memcpy(reply+32,(const void *)&app.held,4); memcpy(reply+36,&app.generation,4);
    memcpy(reply+40,op == 9 ? app.outbox : app.result,n);
    if (op == 9) app.outbox_size = 0;
    stock_reply(context,0x37,reply,n+40);
}
