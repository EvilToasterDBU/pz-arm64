#ifndef PZB_H
#define PZB_H
/* Public API used by the generated per-library JNI shims (arm64). No libc headers on purpose.
 * 'unsigned long' is uint64_t on LP64 aarch64 Linux. */
int           pzb_ensure(const char* x86_lib);
unsigned long pzb_sym(const char* x86_lib, const char* sym);
unsigned long pzb_env(void);
void          pzb_thread(void);
float         pzb_ret_f(void);
double        pzb_ret_d(void);
unsigned long pzb_call_arr(unsigned long fn, int n, const char* types, const unsigned long* vals);
unsigned long RunFunctionFmt(unsigned long fnc, const char* fmt, ...);   /* libbox64.so */
#endif
