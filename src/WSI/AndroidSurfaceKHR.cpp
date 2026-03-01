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

#include "AndroidSurfaceKHR.hpp"

#include "Vulkan/VkDeviceMemory.hpp"
#include "Vulkan/VkImage.hpp"
#include "Vulkan/VkStringify.hpp"

#include <android/native_window.h>
#include <algorithm>
#include <cstring>

namespace vk {

bool AndroidSurfaceKHR::isSupported()
{
    return true;
}

AndroidSurfaceKHR::AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, void *mem)
    : window(pCreateInfo->window)
    , width(0)
    , height(0)
    , format(0)
{
    ANativeWindow_acquire(window);
    width = ANativeWindow_getWidth(window);
    height = ANativeWindow_getHeight(window);
    format = ANativeWindow_getFormat(window);
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{
    ANativeWindow_release(window);
}

size_t AndroidSurfaceKHR::ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo)
{
    return 0;
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                                   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                                   void *pSurfaceCapabilitiesPNext) const
{
    int32_t currentWidth = ANativeWindow_getWidth(window);
    int32_t currentHeight = ANativeWindow_getHeight(window);

    pSurfaceCapabilities->minImageCount = 1;
    pSurfaceCapabilities->maxImageCount = 0;
    pSurfaceCapabilities->currentExtent = { (uint32_t)currentWidth, (uint32_t)currentHeight };
    pSurfaceCapabilities->minImageExtent = { 1, 1 };
    pSurfaceCapabilities->maxImageExtent = { 4096, 4096 };
    pSurfaceCapabilities->maxImageArrayLayers = 1;
    pSurfaceCapabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    pSurfaceCapabilities->supportedUsageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;

    SetCommonSurfaceCapabilities(pSurfaceInfoPNext, pSurfaceCapabilities, pSurfaceCapabilitiesPNext);

    return VK_SUCCESS;
}

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    (void)image;
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    (void)image;
}

VkResult AndroidSurfaceKHR::present(PresentImage *image)
{
    ANativeWindow_Buffer buffer;
    int result = ANativeWindow_lock(window, &buffer, nullptr);
    if (result != 0)
    {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    const Image *vkImage = image->getImage();
    const VkExtent3D &extent = vkImage->getExtent();

    // 计算目标行字节数，假设每个像素 4 字节（RGBA_8888）
    int dstRowPitch = buffer.stride * 4;

    // 复制图像数据到窗口缓冲区
    vkImage->copyTo(static_cast<uint8_t*>(buffer.bits), dstRowPitch);

    ANativeWindow_unlockAndPost(window);

    return VK_SUCCESS;
}

}  // namespace vk
