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
#include <android/hardware_buffer.h>

#include <unordered_map>

// 定义缺失的常量
#ifndef WINDOW_FORMAT_RGBA_FP16
#define WINDOW_FORMAT_RGBA_FP16 0x16  // AHardwareBuffer格式对应值
#endif

#ifndef WINDOW_FORMAT_RGBA_1010102
#define WINDOW_FORMAT_RGBA_1010102 0x2B  // AHardwareBuffer格式对应值
#endif

namespace vk {

// 前向声明
struct AndroidImage;

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

    virtual void attachImage(PresentImage *image) override;
    virtual void detachImage(PresentImage *image) override;
    VkResult present(PresentImage *image) override;

private:
    int getNativeWindowFormat(VkFormat format) const;
    bool waitForFence(int fenceFd) const;

    ANativeWindow* window;
    ANativeWindow* nativeWindow;
    std::unordered_map<PresentImage *, AndroidImage *> imageMap;
};

// AndroidImage结构体定义（放在类外面）
struct AndroidImage
{
    ANativeWindow_Buffer buffer;  // 使用 ANativeWindow_Buffer 而不是 ANativeWindowBuffer
    AHardwareBuffer* hardwareBuffer;
    uint8_t* cpuAddr;
    int fenceFd;
    bool locked;
    int width;
    int height;
    int stride;
    uint32_t format;
};

}  // namespace vk

#endif  // SWIFTSHADER_ANDROIDSURFACEKHR_HPP
