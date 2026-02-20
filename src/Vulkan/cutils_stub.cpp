#include <cutils/properties.h>
#include <cutils/trace.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

// ================== property 相关函数包装 ==================

static void* sPropertyHandle = NULL;
static atomic_int isinitPropertyWrapper = 0;

// 函数指针类型定义
typedef int (*property_get_t)(const char*, char*, const char*);
typedef int (*property_set_t)(const char*, const char*);
typedef int (*property_list_t)(void (*)(const char*, const char*, void*), void*);

// 函数指针变量
static property_get_t fp_property_get = NULL;
static property_set_t fp_property_set = NULL;
static property_list_t fp_property_list = NULL;

static void initPropertyWrapper() {
    const char* propertyLibPaths[] = {
        "/system/lib64/libcutils.so",
        "/system/lib/libcutils.so",
        "libcutils.so",
        NULL
    };
    
    for (int i = 0; propertyLibPaths[i] != NULL; i++) {
        // 先尝试获取已加载的库
        sPropertyHandle = dlopen("libcutils.so", RTLD_NOLOAD);
        if (sPropertyHandle != NULL) {
            break;
        } else {
            sPropertyHandle = dlopen(propertyLibPaths[i], RTLD_LAZY | RTLD_LOCAL);
            if (sPropertyHandle != NULL) {
                break;
            }
        }
    }
    
    if (sPropertyHandle == NULL) {
        return;
    }
    
    // 解析 property 函数符号
    fp_property_get = (property_get_t)dlsym(sPropertyHandle, "property_get");
    fp_property_set = (property_set_t)dlsym(sPropertyHandle, "property_set");
    fp_property_list = (property_list_t)dlsym(sPropertyHandle, "property_list");
}

// ================== trace 相关函数包装 ==================

static void* sTraceHandle = NULL;
static atomic_int isinitTraceWrapper = 0;

// 函数指针类型定义
typedef void (*atrace_init_t)(void);
typedef uint64_t (*atrace_get_enabled_tags_t)(void);
typedef void (*atrace_begin_body_t)(const char*);
typedef void (*atrace_end_body_t)(void);
typedef void (*atrace_async_begin_body_t)(const char*, int32_t);
typedef void (*atrace_async_end_body_t)(const char*, int32_t);
typedef void (*atrace_int_body_t)(const char*, int32_t);
typedef void (*atrace_int64_body_t)(const char*, int64_t);

// 函数指针变量
static atrace_init_t fp_atrace_init = NULL;
static atrace_get_enabled_tags_t fp_atrace_get_enabled_tags = NULL;
static atrace_begin_body_t fp_atrace_begin_body = NULL;
static atrace_end_body_t fp_atrace_end_body = NULL;
static atrace_async_begin_body_t fp_atrace_async_begin_body = NULL;
static atrace_async_end_body_t fp_atrace_async_end_body = NULL;
static atrace_int_body_t fp_atrace_int_body = NULL;
static atrace_int64_body_t fp_atrace_int64_body = NULL;

static void initTraceWrapper() {
    const char* traceLibPaths[] = {
        "/system/lib64/libcutils.so",
        "/system/lib/libcutils.so",
        "libcutils.so",
        NULL
    };
    
    for (int i = 0; traceLibPaths[i] != NULL; i++) {
        // 先尝试获取已加载的库
        sTraceHandle = dlopen("libcutils.so", RTLD_NOLOAD);
        if (sTraceHandle != NULL) {
            break;
        } else {
            sTraceHandle = dlopen(traceLibPaths[i], RTLD_LAZY | RTLD_LOCAL);
            if (sTraceHandle != NULL) {
                break;
            }
        }
    }
    
    if (sTraceHandle == NULL) {
        return;
    }
    
    // 解析 trace 函数符号
    fp_atrace_init = (atrace_init_t)dlsym(sTraceHandle, "atrace_init");
    fp_atrace_get_enabled_tags = (atrace_get_enabled_tags_t)dlsym(sTraceHandle, "atrace_get_enabled_tags");
    fp_atrace_begin_body = (atrace_begin_body_t)dlsym(sTraceHandle, "atrace_begin_body");
    fp_atrace_end_body = (atrace_end_body_t)dlsym(sTraceHandle, "atrace_end_body");
    fp_atrace_async_begin_body = (atrace_async_begin_body_t)dlsym(sTraceHandle, "atrace_async_begin_body");
    fp_atrace_async_end_body = (atrace_async_end_body_t)dlsym(sTraceHandle, "atrace_async_end_body");
    fp_atrace_int_body = (atrace_int_body_t)dlsym(sTraceHandle, "atrace_int_body");
    fp_atrace_int64_body = (atrace_int64_body_t)dlsym(sTraceHandle, "atrace_int64_body");
}

// ================== 暴露的函数实现 ==================

extern "C" {

// property_get 函数实现
int property_get(const char *key, char *value, const char *default_value) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitPropertyWrapper, &expected, 1)) {
        initPropertyWrapper();
    }
    
    if (fp_property_get) {
        return fp_property_get(key, value, default_value);
    }
    
    // Stub 实现
    if (default_value && value) {
        strcpy(value, default_value);
    }
    return default_value ? strlen(default_value) : 0;
}

// 可选：添加 property_set 的包装
int property_set(const char *key, const char *value) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitPropertyWrapper, &expected, 1)) {
        initPropertyWrapper();
    }
    
    if (fp_property_set) {
        return fp_property_set(key, value);
    }
    
    // Stub 实现
    return 0;
}

// 可选：添加 property_list 的包装
int property_list(void (*propfn)(const char*, const char*, void*), void *data) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitPropertyWrapper, &expected, 1)) {
        initPropertyWrapper();
    }
    
    if (fp_property_list) {
        return fp_property_list(propfn, data);
    }
    
    // Stub 实现
    return 0;
}

// atrace_init 函数实现
void atrace_init() {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitTraceWrapper, &expected, 1)) {
        initTraceWrapper();
    }
    
    if (fp_atrace_init) {
        fp_atrace_init();
    }
    // Stub 实现 - 无操作
}

// atrace_get_enabled_tags 函数实现
uint64_t atrace_get_enabled_tags() {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitTraceWrapper, &expected, 1)) {
        initTraceWrapper();
    }
    
    if (fp_atrace_get_enabled_tags) {
        return fp_atrace_get_enabled_tags();
    }
    
    // Stub 实现
    return ATRACE_TAG_NOT_READY;
}

// atrace_begin_body 函数实现
void atrace_begin_body(const char *name) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitTraceWrapper, &expected, 1)) {
        initTraceWrapper();
    }
    
    if (fp_atrace_begin_body) {
        fp_atrace_begin_body(name);
    }
    // Stub 实现 - 无操作
}

// atrace_end_body 函数实现
void atrace_end_body() {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitTraceWrapper, &expected, 1)) {
        initTraceWrapper();
    }
    
    if (fp_atrace_end_body) {
        fp_atrace_end_body();
    }
    // Stub 实现 - 无操作
}

// 可选：添加 atrace_async_begin_body 的包装
void atrace_async_begin_body(const char *name, int32_t cookie) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitTraceWrapper, &expected, 1)) {
        initTraceWrapper();
    }
    
    if (fp_atrace_async_begin_body) {
        fp_atrace_async_begin_body(name, cookie);
    }
    // Stub 实现 - 无操作
}

// 可选：添加 atrace_async_end_body 的包装
void atrace_async_end_body(const char *name, int32_t cookie) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitTraceWrapper, &expected, 1)) {
        initTraceWrapper();
    }
    
    if (fp_atrace_async_end_body) {
        fp_atrace_async_end_body(name, cookie);
    }
    // Stub 实现 - 无操作
}

// 可选：添加 atrace_int_body 的包装
void atrace_int_body(const char *name, int32_t value) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitTraceWrapper, &expected, 1)) {
        initTraceWrapper();
    }
    
    if (fp_atrace_int_body) {
        fp_atrace_int_body(name, value);
    }
    // Stub 实现 - 无操作
}

// 可选：添加 atrace_int64_body 的包装
void atrace_int64_body(const char *name, int64_t value) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&isinitTraceWrapper, &expected, 1)) {
        initTraceWrapper();
    }
    
    if (fp_atrace_int64_body) {
        fp_atrace_int64_body(name, value);
    }
    // Stub 实现 - 无操作
}

} // extern "C"
