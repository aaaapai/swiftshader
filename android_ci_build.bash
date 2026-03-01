#!/bin/bash
set -e

NDK_PATH="${1:-/usr/local/lib/android/sdk/ndk/29.0.14206865}"
COND_VAR_FILE="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1/__condition_variable/condition_variable.h"

echo "🔧 正在注入宏定义，将 pthread_cond_clockwait 映射到 pthread_cond_timedwait ..."

# 备份
BACKUP_FILE="${COND_VAR_FILE}.bak.$(date +%Y%m%d%H%M%S)"
cp "$COND_VAR_FILE" "$BACKUP_FILE"
echo "✅ 已备份到: $BACKUP_FILE"

# 创建临时文件
TEMP_FILE="${COND_VAR_FILE}.tmp"

# 在头文件保护内部插入宏定义
awk '
BEGIN {
    in_header_guard = 0;
    injected = 0;
}
# 检测头文件保护的开始（匹配 libc++ 常用的宏名）
/^#ifndef _LIBCPP___CONDITION_VARIABLE_CONDITION_VARIABLE_H/ || \
/^#ifndef _LIBCPP_CONDITION_VARIABLE/ {
    in_header_guard = 1;
    print;
    next;
}
# 在 #define 行之后注入宏定义（确保在保护区内）
in_header_guard && !injected && /^#define / {
    print;
    print "";
    print "// ===== PATCH: Redirect pthread_cond_clockwait to pthread_cond_timedwait =====";
    print "#ifndef pthread_cond_clockwait";
    print "#define pthread_cond_clockwait(cond, mutex, clock, ts) pthread_cond_timedwait(cond, mutex, ts)";
    print "#endif";
    print "";
    injected = 1;
    next;
}
# 打印所有行
{ print }
' "$COND_VAR_FILE" > "$TEMP_FILE"

# 替换原文件
mv "$TEMP_FILE" "$COND_VAR_FILE"

echo "✅ 宏定义注入完成！"

# 验证
echo "📝 验证注入的宏："
grep -A5 "pthread_cond_clockwait" "$COND_VAR_FILE" | head -10


python ./src/commit_id.py check
python ./src/commit_id.py gen ./src/commit.h

# git clone --depth 1 https://github.com/aaaapai/FastSTL.git ./include/FastSTL
cmake_build () {
  ANDROID_ABI=$1
  mkdir -p build
  cd build
  cmake $GITHUB_WORKSPACE -DANDROID_PLATFORM=29 -DANDROID_ABI=$ANDROID_ABI -DCMAKE_ANDROID_STL_TYPE=c++_static -DCMAKE_SYSTEM_NAME=Android -DANDROID_TOOLCHAIN=clang -DCMAKE_MAKE_PROGRAM=$ANDROID_NDK_LATEST_HOME/prebuilt/linux-x86_64/bin/make -DREACTOR_ENABLE_PRINT=TRUE -DSWIFTSHADER_WARNINGS_AS_ERRORS=false -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_LATEST_HOME/build/cmake/android.toolchain.cmake
  cmake --build . --config Release --parallel 6

  $ANDROID_NDK_LATEST_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip ./build/libvk_swiftshader.so
}

cmake_build arm64-v8a
