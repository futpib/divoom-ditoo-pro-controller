/* Newlib must never invoke semihosting or grow an independent embedded heap. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
extern void *stock_alloc(unsigned size);
extern void stock_free(void *ptr);
extern void stock_delay(unsigned ticks);
struct _reent;
void *_malloc_r(struct _reent *r, size_t n) {
    (void)r;
    if (n > 8192) return NULL;
    void *base = stock_alloc(n + 16);
    if (!base) return NULL;
    uintptr_t p = ((uintptr_t)base + 15) & ~(uintptr_t)7;
    ((uintptr_t *)p)[-2] = (uintptr_t)base;
    ((uintptr_t *)p)[-1] = n;
    return (void *)p;
}
void _free_r(struct _reent *r, void *p) {
    (void)r;
    if (p) stock_free((void *)((uintptr_t *)p)[-2]);
}
void *_realloc_r(struct _reent *r, void *p, size_t n) {
    if (!n) { _free_r(r, p); return NULL; }
    void *q = _malloc_r(r, n);
    if (q && p) {
        size_t old = ((uintptr_t *)p)[-1];
        memcpy(q, p, old < n ? old : n); _free_r(r, p);
    }
    return q;
}
void *_calloc_r(struct _reent *r, size_t n, size_t size) {
    if (size && n > 8192 / size) return NULL;
    void *p = _malloc_r(r, n * size);
    if (p) memset(p, 0, n * size);
    return p;
}
void *_sbrk(ptrdiff_t n) { (void)n; return (void *)-1; }
int _write(int fd, const void *p, size_t n) { (void)fd;(void)p;(void)n;return -1; }
int _read(int fd, void *p, size_t n) { (void)fd;(void)p;(void)n;return -1; }
int _close(int fd) { (void)fd;return -1; }
off_t _lseek(int fd, off_t p, int w) { (void)fd;(void)p;(void)w;return -1; }
int _fstat(int fd, struct stat *s) { (void)fd;(void)s;return -1; }
int _isatty(int fd) { (void)fd;return 0; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid;(void)sig;return -1; }
void _exit(int status) { (void)status; for (;;) stock_delay(1000); }
