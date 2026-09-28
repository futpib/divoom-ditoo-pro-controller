/* Lua runs in its own FreeRTOS task; Bluetooth only copies bounded messages. */
#include "lua.h"
#include "lauxlib.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SOURCE_LIMIT 2048
#define RESULT_LIMIT 192
#define MEMORY_LIMIT (32 * 1024)
extern void *stock_alloc(unsigned size);
extern void stock_free(void *ptr);
extern void *stock_task(void (*entry)(void *), const char *name, void *arg,
                        unsigned words, unsigned priority, unsigned ignored);
extern void stock_delay(unsigned ticks);
extern unsigned stock_volume(unsigned value);
extern unsigned stock_event(void *event);
extern void stock_version(unsigned context);
extern void stock_set_version(const unsigned char *data);
extern void stock_reply(unsigned context, unsigned opcode, const void *data, unsigned size);
extern void stock_heap_init(void);
extern void runtime_worker_entry(void *arg);
extern unsigned char __data_start[], __data_end[], __data_load[], __bss_start[], __bss_end[];

static struct {
    volatile unsigned state; /* 0 idle, 1 queued/running, 2 done, 3 error */
    volatile unsigned cancel;
    void *task;
    unsigned source_size, used, peak, steps;
    char source[SOURCE_LIMIT];
    char result[RESULT_LIMIT];
} job;

void runtime_init(void) {
    stock_heap_init();
    memcpy(__data_start, __data_load, __data_end - __data_start);
    memset(__bss_start, 0, __bss_end - __bss_start);
}

static void *lua_alloc(void *ud, void *ptr, size_t old, size_t size) {
    (void)ud;
    if (!ptr) old = 0;
    if (!size) {
        if (ptr) stock_free(((void **)ptr)[-1]);
        job.used -= old;
        return NULL;
    }
    if (size > MEMORY_LIMIT || job.used - old > MEMORY_LIMIT - size) return NULL;
    /* NDS32 requires eight-byte alignment for Lua's GC-object union. */
    void *base = stock_alloc(size + 16);
    if (!base) return NULL;
    void *next = (void *)(((uintptr_t)base + 15) & ~(uintptr_t)7);
    ((void **)next)[-1] = base;
    if (ptr) { memcpy(next, ptr, old < size ? old : size); stock_free(((void **)ptr)[-1]); }
    job.used = job.used - old + size;
    if (job.used > job.peak) job.peak = job.used;
    return next;
}

static void budget(lua_State *L, lua_Debug *ar) {
    (void)ar;
    job.steps += 100;
    if (job.cancel || job.steps >= 20000) lua_yield(L, 0);
}

static int brightness(lua_State *L) {
    lua_Integer n = luaL_checkinteger(L, 1);
    luaL_argcheck(L, n >= 0 && n <= 100, 1, "expected 0..100");
    unsigned char value = n;
    struct { unsigned short a; unsigned char group, b, command, c; unsigned short d; void *data; }
        event = {0, 0x11, 0, 4, 0, 0, &value};
    stock_event(&event);
    return 0;
}

static int volume(lua_State *L) {
    lua_Integer n = luaL_optinteger(L, 1, 255);
    luaL_argcheck(L, n == 255 || (n >= 0 && n <= 15), 1, "expected 0..15");
    lua_pushinteger(L, stock_volume(n));
    return 1;
}

static int type(lua_State *L) {
    luaL_checkany(L, 1);
    lua_pushstring(L, luaL_typename(L, 1));
    return 1;
}

static int tostring(lua_State *L) {
    luaL_tolstring(L, 1, NULL);
    return 1;
}

static int setup(lua_State *L) {
    lua_pushcfunction(L, brightness); lua_setglobal(L, "brightness");
    lua_pushcfunction(L, volume); lua_setglobal(L, "volume");
    lua_pushcfunction(L, type); lua_setglobal(L, "type");
    lua_pushcfunction(L, tostring); lua_setglobal(L, "tostring");
    lua_newthread(L);
    return 1;
}

static void execute(void) {
    job.used = job.peak = job.steps = 0;
    lua_State *L = lua_newstate(lua_alloc, NULL);
    if (!L) { strcpy(job.result, "Lua allocation failed"); job.state = 3; return; }
    lua_pushcfunction(L, setup);
    if (lua_pcall(L, 0, 1, 0)) {
        strcpy(job.result, "Lua setup allocation failed");
        lua_close(L); job.state = 3; return;
    }
    lua_State *co = lua_tothread(L, -1);
    int rc = luaL_loadbufferx(co, job.source, job.source_size, "upload", "t");
    if (!rc) {
        lua_sethook(co, budget, LUA_MASKCOUNT, 100);
        int results;
        rc = lua_resume(co, NULL, 0, &results);
    }
    const char *result;
    if (rc == LUA_YIELD) result = job.cancel ? "cancelled" : "instruction budget exceeded";
    else if (lua_type(co, -1) == LUA_TBOOLEAN) result = lua_toboolean(co, -1) ? "true" : "false";
    else if (lua_isnil(co, -1) || lua_gettop(co) == 0) result = "nil";
    else if (lua_type(co, -1) == LUA_TNUMBER) {
        if (lua_isinteger(co, -1))
            snprintf(job.result, RESULT_LIMIT, "%d", (int)lua_tointeger(co, -1));
        else
            snprintf(job.result, RESULT_LIMIT, "%.7g", (double)lua_tonumber(co, -1));
        result = job.result;
    } else if (lua_type(co, -1) == LUA_TSTRING) result = lua_tostring(co, -1);
    else result = NULL;
    if (!result) result = rc ? "Lua error" : "non-scalar result";
    size_t n = strlen(result); if (n >= RESULT_LIMIT) n = RESULT_LIMIT - 1;
    if (result != job.result) memcpy(job.result, result, n);
    job.result[n] = 0;
    lua_close(L);
    job.state = rc ? 3 : 2;
}

void runtime_worker(void *unused) {
    (void)unused;
    for (;;) {
        if (job.state == 1) execute();
        stock_delay(10);
    }
}

void runtime_command(unsigned context, const unsigned char *data, unsigned length) {
    if (length < 4) return;
    if (data[1] == 0) { stock_version(0); return; }
    if (length < 9 || data[1] != 0x7f || memcmp(data + 2, "DLUA", 4)) {
        stock_set_version(data + 2); return;
    }
    unsigned op = data[6], error = 0;
    if (op == 1) {
        if (length < 11) error = 1;
        else {
            unsigned size = data[7] | ((unsigned)data[8] << 8);
            if (!size || size > SOURCE_LIMIT || length != size + 11) error = 1;
            else if (job.state == 1) error = 2;
            else {
                if (!job.task) job.task = stock_task(runtime_worker_entry, "divoom_lua", NULL, 4096, 1, 0);
                if (!job.task) error = 3;
                else {
                    memcpy(job.source, data + 9, size);
                    job.source_size = size; job.cancel = 0; job.result[0] = 0;
                    __asm__ volatile ("" ::: "memory");
                    job.state = 1;
                }
            }
        }
    } else if (op == 2) job.cancel = 1;
    else if (op != 0) error = 4;
    unsigned char reply[RESULT_LIMIT + 16] = {'D','L','U','A',1};
    unsigned state = job.state;
    reply[5] = state; reply[6] = error;
    unsigned n = (state == 2 || state == 3) ? strlen(job.result) : 0;
    reply[7] = n;
    memcpy(reply + 8, &job.peak, 4);
    memcpy(reply + 12, &job.steps, 4);
    memcpy(reply + 16, job.result, n);
    stock_reply(context, 0x37, reply, n + 16);
}
