// Copyright 2026 The SwiftShader Authors. All Rights Reserved.
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

#include <android/hardware_buffer.h>
#include <android/native_window.h>

#include <unordered_map>
#include <mutex>

namespace vk {

class AndroidSurfaceKHR : public SurfaceKHR, public ObjectBase<AndroidSurfaceKHR, VkSurfaceKHR>
{
public:
    AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, void *mem);
    ~AndroidSurfaceKHR() = default;

    void destroySurface(const VkAllocationCallbacks *pAllocator) override;

    static size_t ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo);
    static VkResult Create(const VkAllocationCallbacks *pAllocator,
                           const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                           VkSurfaceKHR *pSurface);

    VkResult getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                    VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                    void *pSurfaceCapabilitiesPNext) const override;

    void *allocateImageMemory(PresentImage *image, const VkMemoryAllocateInfo &allocateInfo) override;
    void releaseImageMemory(PresentImage *image) override;
    void attachImage(PresentImage *image) override;
    void detachImage(PresentImage *image) override;
    VkResult present(PresentImage *image) override;

    // 获取原始 ANativeWindow（用于 EGL 创建）
    ANativeWindow* getNativeWindow() const { return window_; }

private:
    struct HardwareBufferResource
    {
        AHardwareBuffer *buffer = nullptr;
        void *mappedPtr = nullptr;
        uint32_t stride = 0;
    };

    ANativeWindow *window_;
    mutable std::mutex mutex_;
    mutable bool surfaceLost_;
    std::unordered_map<PresentImage *, HardwareBufferResource> buffers_;
};

}  // namespace vk

#endif  // SWIFTSHADER_ANDROIDSURFACEKHR_HPP
