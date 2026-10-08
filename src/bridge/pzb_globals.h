#ifndef PZB_GLOBALS_H
#define PZB_GLOBALS_H
#include <jni.h>
#include <stdint.h>
extern JavaVM* g_zomdroid_jvm;
extern __thread JNIEnv* g_zomdroid_jni_env;
extern uint64_t g_wrapped_jni_env;   /* address of the x86-side JNIEnv table */
extern uint64_t g_wrapped_jvm;       /* address of the x86-side JavaVM table */
#endif
