/*
 * hwm_shim.c -- LD_PRELOAD malloc instrumentation: tracks live heap bytes
 * (via malloc_usable_size) and the high-water mark; samples to a file every
 * second and at exit.  Build: gcc -O2 -shared -fPIC hwm_shim.c -o hwm.so -ldl -lpthread
 * Use: HWM_OUT=/path/out.txt LD_PRELOAD=./hwm.so prog
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void *(*real_malloc)(size_t);
static void  (*real_free)(void *);
static void *(*real_calloc)(size_t, size_t);
static void *(*real_realloc)(void *, size_t);
static void *(*real_memalign)(size_t, size_t);
static int   (*real_posix_memalign)(void **, size_t, size_t);

static atomic_long live = 0, hwm = 0, nalloc = 0;
static char boot[65536];
static size_t boot_used = 0;
static int initing = 0, ready = 0;
static struct timespec t0;

static void account(long delta)
{
    long l = atomic_fetch_add(&live, delta) + delta;
    long h = atomic_load(&hwm);
    while (l > h && !atomic_compare_exchange_weak(&hwm, &h, l)) {}
}

static void init(void)
{
    if (ready || initing) return;
    initing = 1;
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    real_free = dlsym(RTLD_NEXT, "free");
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    real_memalign = dlsym(RTLD_NEXT, "memalign");
    real_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ready = 1;
    initing = 0;
}

static double now(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_sec - t0.tv_sec) + (t.tv_nsec - t0.tv_nsec) / 1e9;
}

static void dump(const char *tag)
{
    const char *path = getenv("HWM_OUT");
    FILE *f;
    if (!path) return;
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "t=%.1f live=%ld hwm=%ld allocs=%ld %s\n", now(),
            atomic_load(&live), atomic_load(&hwm), atomic_load(&nalloc), tag);
    fclose(f);
}

static void *sampler(void *arg)
{
    (void)arg;
    for (;;) { sleep(1); dump("sample"); }
    return NULL;
}

__attribute__((constructor)) static void start(void)
{
    pthread_t th;
    init();
    pthread_create(&th, NULL, sampler, NULL);
    pthread_detach(th);
}

__attribute__((destructor)) static void finish(void) { dump("exit"); }

void *malloc(size_t n)
{
    void *p;
    if (!ready) {
        if (initing) { /* dlsym bootstrap */
            p = boot + boot_used; boot_used += (n + 15) & ~15UL; return p;
        }
        init();
    }
    p = real_malloc(n);
    if (p) { account((long)malloc_usable_size(p)); atomic_fetch_add(&nalloc, 1); }
    return p;
}

void free(void *p)
{
    if (!p) return;
    if ((char *)p >= boot && (char *)p < boot + sizeof(boot)) return;
    if (!ready) init();
    account(-(long)malloc_usable_size(p));
    real_free(p);
}

void *calloc(size_t a, size_t b)
{
    void *p;
    if (!ready) {
        if (initing) { size_t n = a * b; p = boot + boot_used; boot_used += (n + 15) & ~15UL; memset(p, 0, n); return p; }
        init();
    }
    p = real_calloc(a, b);
    if (p) { account((long)malloc_usable_size(p)); atomic_fetch_add(&nalloc, 1); }
    return p;
}

void *realloc(void *p, size_t n)
{
    long old = 0; void *q;
    if (!ready) init();
    if (p && !((char *)p >= boot && (char *)p < boot + sizeof(boot))) old = (long)malloc_usable_size(p);
    q = real_realloc(p, n);
    if (q) { account((long)malloc_usable_size(q) - old); atomic_fetch_add(&nalloc, 1); }
    return q;
}

void *memalign(size_t al, size_t n)
{
    void *p;
    if (!ready) init();
    p = real_memalign(al, n);
    if (p) { account((long)malloc_usable_size(p)); atomic_fetch_add(&nalloc, 1); }
    return p;
}

int posix_memalign(void **out, size_t al, size_t n)
{
    int r;
    if (!ready) init();
    r = real_posix_memalign(out, al, n);
    if (r == 0 && *out) { account((long)malloc_usable_size(*out)); atomic_fetch_add(&nalloc, 1); }
    return r;
}

void *aligned_alloc(size_t al, size_t n) { return memalign(al, n); }
