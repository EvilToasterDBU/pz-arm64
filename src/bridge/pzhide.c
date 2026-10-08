/* libpzhide.so - LD_PRELOAD helper: make selected /dev/input/eventN nodes invisible to the process.
 * PZ_HIDE_EVENTS="3,5" -> open("/dev/input/event3") fails with ENOENT. Used to keep the touchscreen
 * from showing up as joystick #0 in GLFW. Freestanding (raw syscalls); only needs libc's getenv/__errno_location. */
extern char *getenv(const char *);
extern int *__errno_location(void);
#define NR_openat 56
#define AT_FDCWD (-100)
static long sys4(long n, long a, long b, long c, long d) {
    register long x8 __asm__("x8") = n, x0 __asm__("x0") = a, x1 __asm__("x1") = b, x2 __asm__("x2") = c, x3 __asm__("x3") = d;
    __asm__ volatile("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
static int hidden(const char *p) {
    const char *pre = "/dev/input/event";
    int i = 0;
    for (; pre[i]; i++) if (p[i] != pre[i]) return 0;
    long num = 0; int digits = 0;
    for (; p[i] >= '0' && p[i] <= '9'; i++) { num = num * 10 + (p[i] - '0'); digits++; }
    if (!digits || p[i]) return 0;
    const char *env = getenv("PZ_HIDE_EVENTS");
    if (!env) return 0;
    long cur = 0; int have = 0;
    for (const char *q = env;; q++) {
        if (*q >= '0' && *q <= '9') { cur = cur * 10 + (*q - '0'); have = 1; }
        else { if (have && cur == num) return 1; cur = 0; have = 0; if (!*q) break; }
    }
    return 0;
}
static int do_open(int dirfd, const char *p, int flags, int mode) {
    if (p && hidden(p)) { *__errno_location() = 2; return -1; }
    long r = sys4(NR_openat, dirfd, (long)p, flags, mode);
    if (r < 0) { *__errno_location() = (int)-r; return -1; }
    return (int)r;
}
#define X __attribute__((visibility("default")))
X int openat(int dirfd, const char *p, int flags, ...) { __builtin_va_list a; __builtin_va_start(a, flags); int m = __builtin_va_arg(a, int); __builtin_va_end(a); return do_open(dirfd, p, flags, m); }
X int open(const char *p, int flags, ...) { __builtin_va_list a; __builtin_va_start(a, flags); int m = __builtin_va_arg(a, int); __builtin_va_end(a); return do_open(AT_FDCWD, p, flags, m); }
X int openat64(int dirfd, const char *p, int flags, ...) { __builtin_va_list a; __builtin_va_start(a, flags); int m = __builtin_va_arg(a, int); __builtin_va_end(a); return do_open(dirfd, p, flags, m); }
X int open64(const char *p, int flags, ...) { __builtin_va_list a; __builtin_va_start(a, flags); int m = __builtin_va_arg(a, int); __builtin_va_end(a); return do_open(AT_FDCWD, p, flags, m); }
X int __open_2(const char *p, int flags) { return do_open(AT_FDCWD, p, flags, 0); }
X int __open64_2(const char *p, int flags) { return do_open(AT_FDCWD, p, flags, 0); }
X int __openat_2(int dirfd, const char *p, int flags) { return do_open(dirfd, p, flags, 0); }
X int __openat64_2(int dirfd, const char *p, int flags) { return do_open(dirfd, p, flags, 0); }
