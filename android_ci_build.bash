#!/bin/bash
# set -e


NDK_PATH="${1:-/usr/local/lib/android/sdk/ndk/29.0.14206865}"
COND_VAR_FILE="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1/__condition_variable/condition_variable.h"

echo "🔧 正在注入兼容层实现: $COND_VAR_FILE"

# 备份
cp "$COND_VAR_FILE" "${COND_VAR_FILE}.bak.$(date +%Y%m%d%H%M%S)"
echo "✅ 已备份"

# 创建临时文件
TEMP_FILE="${COND_VAR_FILE}.tmp"

# 找到文件开头，注入兼容层代码
awk '
BEGIN {
    print "// ========== PTHREAD COMPAT LAYER INJECTED ==========";
    print "#ifndef _PTHREAD_COMPAT_LAYER_H_";
    print "#define _PTHREAD_COMPAT_LAYER_H_";
    print "";
    print "#include <pthread.h>";
    print "#include <time.h>";
    print "#include <errno.h>";
    print "";
    print "// 完整的 pthread_cond_clockwait 兼容层实现";
    print "#if __ANDROID_API__ < 30";
    print "static inline int __pthread_cond_clockwait_compat(";
    print "    pthread_cond_t* cond,";
    print "    pthread_mutex_t* mutex,";
    print "    clockid_t clock_id,";
    print "    const struct timespec* abstime) {";
    print "    ";
    print "    // 完全符合 POSIX 标准的实现";
    print "    // 来源: https://android.googlesource.com/platform/bionic/+/master/libc/bionic/pthread_cond.cpp";
    print "    ";
    print "    // 对于 CLOCK_REALTIME，直接使用 pthread_cond_timedwait";
    print "    if (clock_id == CLOCK_REALTIME) {";
    print "        return pthread_cond_timedwait(cond, mutex, abstime);";
    print "    }";
    print "    ";
    print "    // 对于 CLOCK_MONOTONIC，需要特殊处理";
    print "    if (clock_id == CLOCK_MONOTONIC) {";
    print "        struct timespec monotonic_now;";
    print "        struct timespec realtime_now;";
    print "        struct timespec realtime_abstime;";
    print "        ";
    print "        // 获取当前时间";
    print "        clock_gettime(CLOCK_MONOTONIC, &monotonic_now);";
    print "        clock_gettime(CLOCK_REALTIME, &realtime_now);";
    print "        ";
    print "        // 计算已经过去了多少时间";
    print "        time_t sec_diff = abstime->tv_sec - monotonic_now.tv_sec;";
    print "        long nsec_diff = abstime->tv_nsec - monotonic_now.tv_nsec;";
    print "        ";
    print "        // 调整纳秒";
    print "        if (nsec_diff < 0) {";
    print "            sec_diff--;";
    print "            nsec_diff += 1000000000L;";
    print "        }";
    print "        ";
    print "        // 如果已经超时";
    print "        if (sec_diff < 0 || (sec_diff == 0 && nsec_diff < 0)) {";
    print "            return ETIMEDOUT;";
    print "        }";
    print "        ";
    print "        // 计算对应的真实时间绝对时间";
    print "        realtime_abstime.tv_sec = realtime_now.tv_sec + sec_diff;";
    print "        realtime_abstime.tv_nsec = realtime_now.tv_nsec + nsec_diff;";
    print "        ";
    print "        // 调整纳秒溢出";
    print "        if (realtime_abstime.tv_nsec >= 1000000000L) {";
    print "            realtime_abstime.tv_sec++;";
    print "            realtime_abstime.tv_nsec -= 1000000000L;";
    print "        }";
    print "        ";
    print "        return pthread_cond_timedwait(cond, mutex, &realtime_abstime);";
    print "    }";
    print "    ";
    print "    // 不支持的时钟";
    print "    return EINVAL;";
    print "}";
    print "";
    print "// 重定义 pthread_cond_clockwait 为我们自己的实现";
    print "#define pthread_cond_clockwait(cond, mutex, clock, ts) \\";
    print "    __pthread_cond_clockwait_compat(cond, mutex, clock, ts)";
    print "#endif // __ANDROID_API__ < 30";
    print "// ========== END COMPAT LAYER ==========";
    print "";
}
{ print }
' "$COND_VAR_FILE" > "$TEMP_FILE"

# 替换原文件
mv "$TEMP_FILE" "$COND_VAR_FILE"

echo "✅ 兼容层注入完成！"

# 验证注入是否成功
echo "📝 验证注入内容:"
grep -A5 "PTHREAD COMPAT LAYER" "$COND_VAR_FILE"

echo ""
echo "🔍 检查 pthread_cond_clockwait 定义:"
grep -n "pthread_cond_clockwait" "$COND_VAR_FILE" | head -10


python ./src/commit_id.py check
python ./src/commit_id.py gen ./src/commit.h

# git clone --depth 1 https://github.com/aaaapai/FastSTL.git ./include/FastSTL
cmake_build () {
  ANDROID_ABI=$1
  mkdir -p build
  cd build
  cmake $GITHUB_WORKSPACE -DANDROID_PLATFORM=29 -DANDROID_ABI=$ANDROID_ABI -DCMAKE_ANDROID_STL_TYPE=c++_static -DCMAKE_SYSTEM_NAME=Android -DANDROID_TOOLCHAIN=clang -DCMAKE_MAKE_PROGRAM=$ANDROID_NDK_LATEST_HOME/prebuilt/linux-x86_64/bin/make -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_LATEST_HOME/build/cmake/android.toolchain.cmake
  cmake --build . --config Release --parallel 6
  # 在bash中启用globstar
  #shopt -s globstar
  #$ANDROID_NDK_LATEST_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip $GITHUB_WORKSPACE/**/libMobileGL.so
}

cmake_build arm64-v8a
