// Copyright 2025 The SwiftShader Authors. All Rights Reserved.
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

namespace vk {

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
    ANativeWindow *window;
    int32_t cachedWidth;   // 缓存的窗口宽度（用于快速访问）
    int32_t cachedHeight;  // 缓存的窗口高度
    int32_t cachedFormat;  // 缓存的窗口格式（可能过时，present 时会重新获取）
};

}  // namespace vk

#endif  // SWIFTSHADER_ANDROIDSURFACEKHR_HPP
