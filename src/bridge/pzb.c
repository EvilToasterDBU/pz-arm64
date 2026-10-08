/* pzb.c - in-process Box64 bridge (glibc/aarch64) for running x86_64 JNI libraries inside a native arm64 JVM.
 * Derived from Zomdroid's emulation.c (MIT). Android-specific parts removed; JNI-signature trampolines are
 * replaced by statically generated per-library shims (see gen_shims.py), so no dlopen/dlsym hooking is needed. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <jni.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define LOG_TAG "pzb"
#include "pzb_log.h"
#include "pzb_globals.h"
#include "pzb.h"

#include "box64context.h"
#include "x64emu.h"
#include "box64stack.h"
#include "librarian.h"
#include "callback.h"
#include "custommem.h"
#include "env.h"
#include "threads.h"
#include "library.h"
#include "dynarec.h"
#include "emu/x64emu_private.h"

extern emuthread_t* thread_get_et(void);
extern void RunDeferredElfInit(x64emu_t *emu);   /* runs .init_array of libs loaded so far (C++ static constructors) */

JavaVM* g_zomdroid_jvm;
__thread JNIEnv* g_zomdroid_jni_env;
uint64_t g_wrapped_jni_env;
uint64_t g_wrapped_jvm;

extern int box64_pagesize;
extern void* internal_mmap(void *addr, unsigned long length, int prot, int flags, int fd, ssize_t offset);

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_init_state;            /* 0 = not done, 1 = ok, -1 = failed */

#define MAX_LIBS 32
static char* g_libs[MAX_LIBS];
static lib_t* g_maplibs[MAX_LIBS];   /* every game library gets its own symbol scope (like dlopen(RTLD_LOCAL)):
                                      * PZPathFind64 and PZBullet64 both define a different 'Chunk' class */
static int g_nlibs;

static void* get_self_handle(void) {
    Dl_info info;
    if (!dladdr((void*)get_self_handle, &info)) { LOGE("dladdr failed"); return NULL; }
    return dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD);
}

static int load_x86_lib(const char* name, int local, lib_t** maplib_out) {
    needed_libs_t* nl = new_neededlib(1);
    nl->names[0] = strdup(name);
    if (AddNeededLib(local ? NULL : my_context->maplib, local, 0, 0, nl, NULL, my_context, thread_get_emu()) != 0) {
        LOGE("box64 failed to load x86 library '%s'", name);
        RemoveNeededLib(my_context->maplib, 0, nl, my_context, thread_get_emu());
        free_neededlib(nl);
        return -1;
    }
    if (maplib_out) *maplib_out = local && nl->libs[0] ? GetMaplib(nl->libs[0]) : my_context->maplib;
    free_neededlib(nl);
    return 0;
}

static int do_init(void) {
    jsize n = 0;
    typedef jint (*GetVMs_t)(JavaVM**, jsize, jsize*);
    GetVMs_t gv = (GetVMs_t)dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs");
    if (!gv || gv(&g_zomdroid_jvm, 1, &n) != JNI_OK || n < 1) { LOGE("cannot find the JavaVM"); return -1; }

    LOGI("initialising box64 (library mode)");
    box64_pagesize = sysconf(_SC_PAGESIZE);
    if (!box64_pagesize) box64_pagesize = 4096;
    LoadEnvVariables();

    box64context_t* context = NewBox64Context(0);
    context->argv[0] = strdup("java");
    {   /* Box64 only fills context->fullpath when it runs a main program; x86 code (steamclient) calls
         * readlink("/proc/self/exe"), which Box64 answers from fullpath -> NULL crash in library mode. */
        char exe[4096];
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        context->fullpath = strdup(n > 0 ? (exe[n] = 0, exe) : "/usr/bin/java");
    }
    dlclose(context->box64lib);
    context->box64lib = get_self_handle();
    PrependList(&context->box64_ld_lib, getenv("BOX64_LD_LIBRARY_PATH"), 1);
    if (CalcStackSize(context) != 0) { LOGE("CalcStackSize failed"); return -1; }
    x64emu_t* emu = NewX64Emu(context, context->ep, (uintptr_t)context->stack, context->stacksz, 0);
    thread_set_emu(emu);

    if (load_x86_lib("libjniwrapper.so", 0, NULL) != 0) return -1;
    /* Box64 defers library constructors until the main program starts; in library mode that never happens,
     * so run them now (libstdc++, libjniwrapper) - later loads then initialise immediately. */
    RunDeferredElfInit(thread_get_emu());
    uint64_t init_fn = FindGlobalSymbol(my_context->maplib, "jni_wrapper_init", -1, NULL, 0);
    if (!init_fn) { LOGE("jni_wrapper_init not found"); return -1; }
    RunFunction(init_fn, 0);
    g_wrapped_jni_env = FindGlobalSymbol(my_context->maplib, "g_wrapped_jni_env", -1, NULL, 0);
    g_wrapped_jvm = FindGlobalSymbol(my_context->maplib, "g_wrapped_jvm", -1, NULL, 0);
    if (!g_wrapped_jni_env || !g_wrapped_jvm) { LOGE("wrapped JNI env/vm not found"); return -1; }
    LOGI("box64 ready, fake JNIEnv table at %p", (void*)g_wrapped_jni_env);
    return 0;
}

int pzb_ensure(const char* x86_lib) {
    pthread_mutex_lock(&g_lock);
    if (g_init_state == 0) g_init_state = do_init() == 0 ? 1 : -1;
    int rc = g_init_state == 1 ? 0 : -1;
    if (rc == 0 && x86_lib) {
        int found = 0;
        for (int i = 0; i < g_nlibs; i++) if (!strcmp(g_libs[i], x86_lib)) found = 1;
        if (!found) {
            lib_t* ml = NULL;
            if (g_nlibs < MAX_LIBS && load_x86_lib(x86_lib, 1, &ml) == 0) { g_maplibs[g_nlibs] = ml; g_libs[g_nlibs++] = strdup(x86_lib); }
            else rc = -1;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return rc;
}

uint64_t pzb_sym(const char* x86_lib, const char* sym) {
    if (pzb_ensure(x86_lib) != 0) return 0;
    lib_t* ml = my_context->maplib;
    for (int i = 0; i < g_nlibs; i++) if (!strcmp(g_libs[i], x86_lib)) ml = g_maplibs[i];
    uint64_t a = FindGlobalSymbol(ml, sym, -1, NULL, 0);
    if (!a) LOGE("symbol %s not found in %s", sym, x86_lib);
    return a;
}

uint64_t pzb_env(void) { return g_wrapped_jni_env; }

static __thread int t_thread_ready;
void pzb_thread(void) {
    if (t_thread_ready) return;
    t_thread_ready = 1;
    if (thread_get_et()) return;                       /* this thread already has an emulator */
    size_t stacksize = 8u << 20;                       /* JNI natives (path-finding, lighting) can recurse deeply */
    void* stack = internal_mmap(NULL, stacksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
    if (stack == MAP_FAILED) { LOGE("cannot allocate an emulator stack: %s", strerror(errno)); return; }
    setProtection((uintptr_t)stack, stacksize, PROT_READ | PROT_WRITE);
    x64emu_t* emu = NewX64Emu(my_context, 0, (uintptr_t)stack, stacksize, 1);
    SetupX64Emu(emu, NULL);
    thread_set_emu(emu);
}

float  pzb_ret_f(void) { return thread_get_emu()->xmm[0].f[0]; }
double pzb_ret_d(void) { return thread_get_emu()->xmm[0].d[0]; }

/* Call an emulated x86_64 function with arguments given as an array (SysV ABI), like RunFunctionFmt but without
 * varargs. types[i]: 'p' pointer, 'i'/'u' 32-bit, 'I' 64-bit, 'c','C','w','W' small ints (already extended in vals),
 * 'f' float (low 32 bits of vals[i] are the float bits), 'd' double (vals[i] are the double bits).
 * Result: RAX; read XMM0 with pzb_ret_f/pzb_ret_d for float/double returns. */
uint64_t pzb_call_arr(uint64_t fn, int n, const char* types, const uint64_t* vals) {
    x64emu_t* emu = thread_get_emu();
    static const int nn[] = {_DI, _SI, _DX, _CX, _R8, _R9};
    int ni = 0, ndf = 0, nstack = 0;
    for (int i = 0; i < n; i++) {
        if (types[i] == 'f' || types[i] == 'd') { if (ndf < 8) ndf++; else nstack++; }
        else { if (ni < 6) ni++; else nstack++; }
    }
    int stackn = nstack + (nstack & 1);                 /* keep 16-byte alignment */
    R_RSP -= 8; *(uint64_t*)R_RSP = R_RBP;              /* push rbp */
    R_RBP = R_RSP;
    R_RSP -= (uint64_t)stackn * 8;
    uint64_t* p = (uint64_t*)R_RSP;
    ni = 0; ndf = 0;
    for (int i = 0; i < n; i++) {
        uint64_t v = vals[i];
        if (types[i] == 'f') {
            float f; uint32_t b = (uint32_t)v; memcpy(&f, &b, 4);
            if (ndf < 8) emu->xmm[ndf++].f[0] = f; else { *p = 0; memcpy(p, &f, 4); p++; }
        } else if (types[i] == 'd') {
            double d; memcpy(&d, &v, 8);
            if (ndf < 8) emu->xmm[ndf++].d[0] = d; else { memcpy(p, &d, 8); p++; }
        } else {
            if (ni < 6) emu->regs[nn[ni++]].q[0] = v; else *p++ = v;
        }
    }
    uintptr_t oldip = R_RIP;
    DynaCall(emu, (uintptr_t)fn);
    if (oldip == R_RIP) {
        R_RSP = R_RBP;                                    /* mov rsp, rbp */
        R_RBP = *(uint64_t*)R_RSP; R_RSP += 8;            /* pop rbp */
    }
    return R_RAX;
}
