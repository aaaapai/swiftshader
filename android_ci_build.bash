#!/bin/bash
# set -e


NDK_PATH="${1:-/usr/local/lib/android/sdk/ndk/29.0.14206865}"

echo "正在修改 NDK 头文件: $NDK_PATH"

# 要修改的文件
COND_VAR_FILE="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1/__condition_variable/condition_variable.h"

if [ ! -f "$COND_VAR_FILE" ]; then
    echo "错误: 找不到文件 $COND_VAR_FILE"
    exit 1
fi

# 备份原文件
cp "$COND_VAR_FILE" "${COND_VAR_FILE}.bak"
echo "已备份到: ${COND_VAR_FILE}.bak"

# 方法1: 注释掉 pthread_cond_clockwait 调用，替换为 pthread_cond_timedwait
# 并强制使用 CLOCK_REALTIME
sed -i 's/\(int __ec = pthread_cond_clockwait(.*CLOCK_MONOTONIC.*\);\)/\/\/ \1\n    int __ec = pthread_cond_timedwait(\&__cv_, __lk.mutex()->native_handle(), \&__ts);/g' "$COND_VAR_FILE"

# 方法2: 如果上面的替换不工作，用更简单的方法 - 直接注释整行
# sed -i '226s/^/\/\/ /' "$COND_VAR_FILE"

echo "修改完成！"

# 可选：验证修改
if grep -q "pthread_cond_clockwait" "$COND_VAR_FILE"; then
    echo "警告: 文件中仍包含 pthread_cond_clockwait"
    grep -n "pthread_cond_clockwait" "$COND_VAR_FILE"
else
    echo "成功: 文件中已没有 pthread_cond_clockwait"
fi


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
