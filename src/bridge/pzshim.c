/* pzshim.c - universal JNI shim. One copy of this library is installed under every name the game passes to
 * System.loadLibrary(); JNI_OnLoad reads <PZ_TABLE_DIR>/<own file name>.tbl and registers every listed native
 * method (RegisterNatives) with a stub from stubs.S. A call lands in pzs_dispatch, which rebuilds the argument list
 * from the saved registers according to the method descriptor and runs the matching x86_64 JNI function in Box64.
 *
 * Table format (tab separated):   first line:   X86LIB<TAB>lib file name inside the game dir (may be empty)
 *                                 then:         class<TAB>method<TAB>descriptor<TAB>x86 symbol   (symbol empty = no-op stub)
 */
#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>
#include <jni.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pzb.h"

#define MAX_STUBS 4096
#define MAXP 40

extern char pzs_stubs[] __attribute__((visibility("hidden")));

typedef struct {
    char *name, *desc, *xsym, *cls;
    uint64_t fn;
    int nparams;
    char ptypes[MAXP];      /* Z B C S I J F D L (L = any reference) */
    char rtype;             /* V Z B C S I J F D L */
} entry_t;

static entry_t g_entries[MAX_STUBS];
static int g_count;
static char g_xlib[256];

static int parse_desc(const char* d, entry_t* e) {
    int n = 0, i = 1;
    while (d[i] && d[i] != ')') {
        char c = d[i];
        if (c == 'L') { while (d[i] != ';') i++; c = 'L'; }
        else if (c == '[') { while (d[i] == '[') i++; if (d[i] == 'L') while (d[i] != ';') i++; c = 'L'; }
        if (n >= MAXP) return -1;
        e->ptypes[n++] = c;
        i++;
    }
    if (d[i] != ')') return -1;
    e->nparams = n;
    e->rtype = (d[i + 1] == '[') ? 'L' : d[i + 1];
    return 0;
}

__attribute__((visibility("hidden"))) void pzs_dispatch(unsigned idx, uint64_t* regs) {
    /* regs: [0..7] x0-x7, [8..15] d0-d7 (raw bits), [16] ret int, [17] ret double bits, [18] caller stack pointer */
    entry_t* e = &g_entries[idx];
    regs[16] = 0; regs[17] = 0;
    if (!e->xsym[0]) return;                                 /* no-op stub */
    if (!e->fn) {
        e->fn = pzb_sym(g_xlib, e->xsym);
        if (!e->fn) { fprintf(stderr, "[pzshim] unresolved native %s.%s -> %s\n", e->cls, e->name, e->xsym); return; }
    }
    pzb_thread();
    static int trace = -1;
    if (trace < 0) trace = getenv("PZ_TRACE") != NULL;
    if (trace) fprintf(stderr, "[pzshim] %s.%s(%llx,%llx,%llx,%llx) tid=%ld\n", e->cls, e->name, (unsigned long long)regs[2], (unsigned long long)regs[3], (unsigned long long)regs[4], (unsigned long long)regs[5], (long)gettid());
    if (trace) {
        JNIEnv* je = (JNIEnv*)regs[0]; int xi2 = 2;
        for (int k = 0; k < e->nparams && xi2 < 8; k++) {
            char t = e->ptypes[k];
            if (t == 'F' || t == 'D') continue;
            if (t == 'L' && regs[xi2]) {
                unsigned char* p = (*je)->GetDirectBufferAddress(je, (jobject)regs[xi2]);
                fprintf(stderr, "[pzshim]   arg%d ref=%llx addr=%p\n", k, (unsigned long long)regs[xi2], (void*)p);
                if (p) {
                    fprintf(stderr, "[pzshim]   buf arg%d cap=%lld:", k, (long long)(*je)->GetDirectBufferCapacity(je, (jobject)regs[xi2]));
                    for (int b = 0; b < 24; b++) fprintf(stderr, " %02x", p[b]);
                    fprintf(stderr, "\n");
                }
            }
            xi2++;
        }
    }
    char types[MAXP + 2]; uint64_t vals[MAXP + 2]; int n = 0;
    types[n] = 'p'; vals[n++] = pzb_env();                   /* fake x86 JNIEnv* */
    types[n] = 'p'; vals[n++] = regs[1];                     /* jclass / jobject */
    int xi = 2, di = 0;
    uint64_t* sp = (uint64_t*)regs[18];
    for (int k = 0; k < e->nparams; k++) {
        char t = e->ptypes[k];
        if (t == 'F' || t == 'D') {
            uint64_t raw = (di < 8) ? regs[8 + di++] : *sp++;
            types[n] = (t == 'F') ? 'f' : 'd';
            vals[n++] = (t == 'F') ? (raw & 0xffffffffu) : raw;
        } else {
            uint64_t raw = (xi < 8) ? regs[xi++] : *sp++;
            switch (t) {
            case 'Z': types[n] = 'C'; raw = (uint8_t)raw; break;
            case 'B': types[n] = 'c'; raw = (uint64_t)(int64_t)(int8_t)raw; break;
            case 'C': types[n] = 'W'; raw = (uint16_t)raw; break;
            case 'S': types[n] = 'w'; raw = (uint64_t)(int64_t)(int16_t)raw; break;
            case 'I': types[n] = 'i'; raw = (uint64_t)(int64_t)(int32_t)raw; break;
            case 'J': types[n] = 'I'; break;
            default:  types[n] = 'p'; break;
            }
            vals[n++] = raw;
        }
    }
    uint64_t r = pzb_call_arr(e->fn, n, types, vals);
    switch (e->rtype) {
    case 'V': break;
    case 'Z': regs[16] = (uint8_t)r; break;
    case 'B': regs[16] = (uint64_t)(int64_t)(int8_t)r; break;
    case 'C': regs[16] = (uint16_t)r; break;
    case 'S': regs[16] = (uint64_t)(int64_t)(int16_t)r; break;
    case 'I': regs[16] = (uint64_t)(int64_t)(int32_t)r; break;
    case 'F': { float f = pzb_ret_f(); uint32_t b; memcpy(&b, &f, 4); regs[17] = b; break; }
    case 'D': { double d = pzb_ret_d(); memcpy(&regs[17], &d, 8); break; }
    default:  regs[16] = r; break;                           /* J and references */
    }
}

static char* dup_field(char** cur) {
    char* start = *cur; char* tab = strchr(start, '\t');
    if (tab) { *tab = 0; *cur = tab + 1; } else { *cur = start + strlen(start); }
    return strdup(start);
}

/* dladdr() must be given an address that cannot be interposed: a reference to the exported JNI_OnLoad resolves,
 * through the GOT, to the first JNI_OnLoad in the lookup scope (e.g. libawt_headless's) and would name the wrong file. */
static void self_anchor(void) {}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    Dl_info info;
    if (!dladdr((void*)self_anchor, &info) || !info.dli_fname) return JNI_VERSION_1_8;
    char path[1024]; snprintf(path, sizeof path, "%s", info.dli_fname);
    const char* base = basename(path);
    const char* dir = getenv("PZ_TABLE_DIR");
    if (!dir) return JNI_VERSION_1_8;
    char tbl[1400]; snprintf(tbl, sizeof tbl, "%s/%s.tbl", dir, base);
    int dbg = getenv("PZ_TRACE") != NULL;
    FILE* f = fopen(tbl, "r");
    if (dbg) fprintf(stderr, "[pzshim] JNI_OnLoad %s table=%s open=%d\n", info.dli_fname, tbl, f != NULL);
    if (!f) return JNI_VERSION_1_8;                          /* nothing to bridge for this name */

    char line[2048];
    if (!fgets(line, sizeof line, f)) { fclose(f); return JNI_VERSION_1_8; }
    line[strcspn(line, "\r\n")] = 0;
    if (strncmp(line, "X86LIB\t", 7) == 0) snprintf(g_xlib, sizeof g_xlib, "%s", line + 7);
    if (g_xlib[0] && pzb_ensure(g_xlib) != 0) { fprintf(stderr, "[pzshim] cannot load x86 library %s\n", g_xlib); fclose(f); return JNI_VERSION_1_8; }

    while (fgets(line, sizeof line, f) && g_count < MAX_STUBS) {
        line[strcspn(line, "\r\n")] = 0;
        char* cur = line;
        entry_t* e = &g_entries[g_count];
        e->cls = dup_field(&cur); e->name = dup_field(&cur); e->desc = dup_field(&cur); e->xsym = dup_field(&cur);
        if (parse_desc(e->desc, e) != 0) continue;
        g_count++;
    }
    fclose(f);

    /* register per class */
    for (int i = 0; i < g_count; ) {
        const char* cls = g_entries[i].cls;
        int j = i; while (j < g_count && strcmp(g_entries[j].cls, cls) == 0) j++;
        jclass c = (*env)->FindClass(env, cls);
        if (!c) { if (dbg) fprintf(stderr, "[pzshim] FindClass(%s) failed\n", cls); (*env)->ExceptionClear(env); i = j; continue; }
        JNINativeMethod ms[MAX_STUBS];
        for (int k = i; k < j; k++) { ms[k - i].name = g_entries[k].name; ms[k - i].signature = g_entries[k].desc; ms[k - i].fnPtr = pzs_stubs + 8 * k; }
        if ((*env)->RegisterNatives(env, c, ms, j - i) != 0) {
            if (dbg) { fprintf(stderr, "[pzshim] RegisterNatives(%s) failed\n", cls); (*env)->ExceptionDescribe(env); }
            (*env)->ExceptionClear(env);
        } else if (dbg) fprintf(stderr, "[pzshim] registered %d natives in %s\n", j - i, cls);
        (*env)->DeleteLocalRef(env, c);
        i = j;
    }
    return JNI_VERSION_1_8;
}
