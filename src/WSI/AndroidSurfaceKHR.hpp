// Copyright 2020 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SWIFTSHADER_ANDROIDSURFACEKHR_HPP
#define SWIFTSHADER_ANDROIDSURFACEKHR_HPP

#include "VkSurfaceKHR.hpp"
#include "Vulkan/VkObject.hpp"

#include <vulkan/vulkan_android.h>
#include <android/native_window.h>

#include <unordered_map>

// 定义缺失的窗口格式常量（如果未定义）
#ifndef WINDOW_FORMAT_RGBA_FP16
#define WINDOW_FORMAT_RGBA_FP16 0x16
#endif

#ifndef WINDOW_FORMAT_RGBA_1010102
#define WINDOW_FORMAT_RGBA_1010102 0x2B
#endif

namespace vk {

struct AndroidImage
{
    ANativeWindow_Buffer buffer;   // 锁定的窗口缓冲区信息
    uint8_t* cpuAddr;              // CPU可访问的地址
    int stride;                    // 缓冲区行步长（像素）
    bool locked;                   // 是否已锁定
    int width;
    int height;
    int format;
};

class AndroidSurfaceKHR : public SurfaceKHR, public ObjectBase<AndroidSurfaceKHR, VkSurfaceKHR>
{
public:
    static bool isSupported();
    AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, void *mem);

    void destroySurface(const VkAllocationCallbacks *pAllocator) override;

    static size_t ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo);

    VkResult getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                   void *pSurfaceCapabilitiesPNext) const override;

    void attachImage(PresentImage *image) override;
    void detachImage(PresentImage *image) override;
    VkResult present(PresentImage *image) override;

private:
    int getNativeWindowFormat(VkFormat format) const;

    ANativeWindow* window;
    ANativeWindow* nativeWindow;
    std::unordered_map<PresentImage *, AndroidImage *> imageMap;
};

}  // namespace vk

#endif  // SWIFTSHADER_ANDROIDSURFACEKHR_HPP
