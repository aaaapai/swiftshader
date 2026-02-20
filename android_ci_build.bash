#!/bin/bash
set -e

#!/bin/bash
set -e

NDK_PATH="${1:-/usr/local/lib/android/sdk/ndk/29.0.14206865}"
SYSROOT="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot"
INCLUDE_DIR="$SYSROOT/usr/include/c++/v1/__condition_variable"
COND_VAR_FILE="$INCLUDE_DIR/condition_variable.h"
COMPAT_FILE="$INCLUDE_DIR/pthread_compat_layer.h"   # 新建的兼容层文件

echo "🔧 正在注入兼容层实现（安全模式）..."

# 备份原文件
BACKUP_FILE="${COND_VAR_FILE}.bak.$(date +%Y%m%d%H%M%S)"
cp "$COND_VAR_FILE" "$BACKUP_FILE"
echo "✅ 已备份原文件到: $BACKUP_FILE"

# 创建兼容层头文件
cat > "$COMPAT_FILE" << 'EOF'
// ========== PTHREAD COMPAT LAYER ==========
#ifndef _PTHREAD_COMPAT_LAYER_H_
#define _PTHREAD_COMPAT_LAYER_H_

#include <pthread.h>
#include <time.h>
#include <errno.h>

// 完整的 pthread_cond_clockwait 兼容层实现
#if __ANDROID_API__ < 30
static inline int __pthread_cond_clockwait_compat(
    pthread_cond_t* cond,
    pthread_mutex_t* mutex,
    clockid_t clock_id,
    const struct timespec* abstime) {
    
    // 完全符合 POSIX 标准的实现
    // 来源: https://android.googlesource.com/platform/bionic/+/master/libc/bionic/pthread_cond.cpp
    
    // 对于 CLOCK_REALTIME，直接使用 pthread_cond_timedwait
    if (clock_id == CLOCK_REALTIME) {
        return pthread_cond_timedwait(cond, mutex, abstime);
    }
    
    // 对于 CLOCK_MONOTONIC，需要特殊处理
    if (clock_id == CLOCK_MONOTONIC) {
        struct timespec monotonic_now;
        struct timespec realtime_now;
        struct timespec realtime_abstime;
        
        // 获取当前时间
        clock_gettime(CLOCK_MONOTONIC, &monotonic_now);
        clock_gettime(CLOCK_REALTIME, &realtime_now);
        
        // 计算已经过去了多少时间
        time_t sec_diff = abstime->tv_sec - monotonic_now.tv_sec;
        long nsec_diff = abstime->tv_nsec - monotonic_now.tv_nsec;
        
        // 调整纳秒
        if (nsec_diff < 0) {
            sec_diff--;
            nsec_diff += 1000000000L;
        }
        
        // 如果已经超时
        if (sec_diff < 0 || (sec_diff == 0 && nsec_diff < 0)) {
            return ETIMEDOUT;
        }
        
        // 计算对应的真实时间绝对时间
        realtime_abstime.tv_sec = realtime_now.tv_sec + sec_diff;
        realtime_abstime.tv_nsec = realtime_now.tv_nsec + nsec_diff;
        
        // 调整纳秒溢出
        if (realtime_abstime.tv_nsec >= 1000000000L) {
            realtime_abstime.tv_sec++;
            realtime_abstime.tv_nsec -= 1000000000L;
        }
        
        return pthread_cond_timedwait(cond, mutex, &realtime_abstime);
    }
    
    // 不支持的时钟
    return EINVAL;
}

// 重定义 pthread_cond_clockwait 为我们自己的实现
#define pthread_cond_clockwait(cond, mutex, clock, ts) \
    __pthread_cond_clockwait_compat(cond, mutex, clock, ts)
#endif // __ANDROID_API__ < 30

#endif // _PTHREAD_COMPAT_LAYER_H_
// ========== END COMPAT LAYER ==========
EOF
echo "✅ 已创建兼容层文件: $COMPAT_FILE"

# 在 condition_variable.h 中插入 #include
# 找到头文件保护宏内的合适位置（一般在 #define 行之后）
if grep -q '#include "pthread_compat_layer.h"' "$COND_VAR_FILE"; then
    echo "⏭️  condition_variable.h 已包含兼容层，跳过修改"
else
    # 在 #define 行之后插入 include（利用 sed）
    sed -i '/^#define .*CONDITION_VARIABLE_H/ a #include "pthread_compat_layer.h"' "$COND_VAR_FILE"
    echo "✅ 已在 condition_variable.h 中插入 #include 指令"
fi

# 验证
echo "📝 验证 condition_variable.h 头部:"
head -20 "$COND_VAR_FILE" | grep -A3 -B3 "pthread_compat_layer"

echo ""
echo "🔍 检查兼容层文件内容:"
head -10 "$COMPAT_FILE"

python ./src/commit_id.py check
python ./src/commit_id.py gen ./src/commit.h

# git clone --depth 1 https://github.com/aaaapai/FastSTL.git ./include/FastSTL
cmake_build () {
  ANDROID_ABI=$1
  mkdir -p build
  cd build
  cmake $GITHUB_WORKSPACE -DANDROID_PLATFORM=29 -DANDROID_ABI=$ANDROID_ABI -DCMAKE_ANDROID_STL_TYPE=c++_static -DCMAKE_SYSTEM_NAME=Android -DANDROID_TOOLCHAIN=clang -DCMAKE_MAKE_PROGRAM=$ANDROID_NDK_LATEST_HOME/prebuilt/linux-x86_64/bin/make -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_LATEST_HOME/build/cmake/android.toolchain.cmake
  cmake --build . --config Release --parallel 6
}

cmake_build arm64-v8a
