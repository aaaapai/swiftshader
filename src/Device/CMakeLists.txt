# Copyright 2020 The SwiftShader Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(ROOT_PROJECT_COMPILE_OPTIONS
    ${SWIFTSHADER_COMPILE_OPTIONS}
    ${WARNINGS_AS_ERRORS}
)

set(CMAKE_ANDROID_LD "lld")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(MOBILEGL_FORCE_RELEASE_OPT "Enable Release optimization flags in Debug build" ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -pipe -integrated-as -mllvm -polly -mllvm -polly-vectorizer=stripmine -mllvm -polly-invariant-load-hoisting -mllvm -polly-run-inliner -mllvm -polly-run-dce -mllvm -polly-invariant-load-hoisting -flto -mllvm -polly-run-inliner -mllvm -polly-run-dce -fno-emulated-tls -fwhole-program-vtables -fdata-sections -ffunction-sections -fmerge-all-constants -mllvm -polly-detect-keep-going -mllvm -polly-ast-use-context -march=armv8-a+simd -fvisibility=hidden -Wall -Wextra -ferror-limit=0 -Wno-return-type -Wno-unused-parameter -Wno-c++98-compat -Wno-c++98-compat-pedantic -DGLM_FORCE_INTRINSICS -Wno-unicode-whitespace -Wno-format -Wno-unused-function -DUseFastSTL -DUseJEMALLOC -pthread  -I${CMAKE_SOURCE_DIR}/include/Android -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -I${CMAKE_SOURCE_DIR}/include/vulkan")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pipe -integrated-as -mllvm -polly -mllvm -polly-vectorizer=stripmine -mllvm -polly-invariant-load-hoisting -mllvm -polly-run-inliner -mllvm -polly-run-dce -mllvm -polly-invariant-load-hoisting -flto -mllvm -polly-run-inliner -mllvm -polly-run-dce -fno-emulated-tls -fwhole-program-vtables -fdata-sections -ffunction-sections -fmerge-all-constants -mllvm -polly-detect-keep-going -mllvm -polly-ast-use-context -march=armv8-a+simd -fvisibility=hidden -Wall -Wextra -ferror-limit=0 -Wno-return-type -Wno-unused-parameter -Wno-c++98-compat -Wno-c++98-compat-pedantic -DGLM_FORCE_INTRINSICS -stdlib=libc++ -Wno-unicode-whitespace -Wno-format -Wno-unused-function -Wno-vla-cxx-extension -DUseFastSTL -frtti -DUseJEMALLOC -pthread -I${CMAKE_SOURCE_DIR}/include/vulkan -I${CMAKE_SOURCE_DIR}/include/Android")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -flto -O3 -Wl,-plugin-opt=-emulated-tls=0 -Wl,--strip-all -fvisibility=hidden -Wl,-O3 -Wl,--gc-sections -Wl,--as-needed -static-libstdc++ -llog -lcutils")

set(DEVICE_SRC_FILES
    ASTC_Decoder.cpp
    ASTC_Decoder.hpp
    BC_Decoder.cpp
    BC_Decoder.hpp
    Blitter.cpp
    Blitter.hpp
    Clipper.cpp
    Clipper.hpp
    Config.hpp
    Context.cpp
    Context.hpp
    ETC_Decoder.cpp
    ETC_Decoder.hpp
    Memset.hpp
    PixelProcessor.cpp
    PixelProcessor.hpp
    Polygon.hpp
    Primitive.hpp
    QuadRasterizer.cpp
    QuadRasterizer.hpp
    Rasterizer.hpp
    Renderer.cpp
    Renderer.hpp
    RoutineCache.hpp
    Sampler.hpp
    SetupProcessor.cpp
    SetupProcessor.hpp
    Stream.hpp
    Vertex.hpp
    VertexProcessor.cpp
    VertexProcessor.hpp
)

add_library(vk_device EXCLUDE_FROM_ALL
    ${DEVICE_SRC_FILES}
)

set_target_properties(vk_device PROPERTIES
    POSITION_INDEPENDENT_CODE 1
    FOLDER "SwiftShader VK"
)

target_include_directories(vk_device
    PUBLIC
        ".."
        "${SWIFTSHADER_DIR}/include"
)

target_compile_options(vk_device
    PRIVATE
        ${ROOT_PROJECT_COMPILE_OPTIONS}
)

target_link_options(vk_device
    PUBLIC
        ${SWIFTSHADER_LINK_FLAGS}
)

target_link_libraries(vk_device
    PUBLIC
        vk_pipeline
        $<$<BOOL:${SWIFTSHADER_ENABLE_ASTC}>:astc-encoder>
)
