#ifndef PZB_LOG_H
#define PZB_LOG_H
#include <stdio.h>
#ifndef LOG_TAG
#define LOG_TAG "pzb"
#endif
#define LOGE(fmt, ...) fprintf(stderr, "[" LOG_TAG "][E] " fmt "\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) fprintf(stderr, "[" LOG_TAG "][W] " fmt "\n", ##__VA_ARGS__)
#define LOGI(fmt, ...) fprintf(stderr, "[" LOG_TAG "][I] " fmt "\n", ##__VA_ARGS__)
#define LOGD(fmt, ...) do { if (getenv("PZB_DEBUG")) fprintf(stderr, "[" LOG_TAG "][D] " fmt "\n", ##__VA_ARGS__); } while (0)
#endif
