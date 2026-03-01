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
    // 在 Android 平台上始终支持
    return true;
}

AndroidSurfaceKHR::AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, void *mem)
    : window(pCreateInfo->window)
    , width(0)
    , height(0)
    , format(0)
{
    // 增加对 ANativeWindow 的引用，防止外部提前销毁
    ANativeWindow_acquire(window);

    // 获取当前窗口的宽度、高度和格式（用于后续能力查询）
    width = ANativeWindow_getWidth(window);
    height = ANativeWindow_getHeight(window);
    format = ANativeWindow_getFormat(window);
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{
    // 释放对 ANativeWindow 的引用
    ANativeWindow_release(window);
}

size_t AndroidSurfaceKHR::ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo)
{
    return 0; // 无需额外动态内存
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                                   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                                   void *pSurfaceCapabilitiesPNext) const
{
    // 获取最新的窗口尺寸（窗口可能已被重新调整大小）
    int32_t currentWidth = ANativeWindow_getWidth(window);
    int32_t currentHeight = ANativeWindow_getHeight(window);

    // 设置能力结构体
    pSurfaceCapabilities->minImageCount = 1;                   // 至少 1 个图像
    pSurfaceCapabilities->maxImageCount = 0;                   // 无上限（由窗口系统决定）
    pSurfaceCapabilities->currentExtent = { (uint32_t)currentWidth, (uint32_t)currentHeight };
    pSurfaceCapabilities->minImageExtent = { 1, 1 };
    pSurfaceCapabilities->maxImageExtent = { 4096, 4096 };     // 常见的软件限制
    pSurfaceCapabilities->maxImageArrayLayers = 1;
    pSurfaceCapabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // 通常不支持透明
    pSurfaceCapabilities->supportedUsageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;

    // 填充 pNext 链（复用通用实现）
    SetCommonSurfaceCapabilities(pSurfaceInfoPNext, pSurfaceCapabilities, pSurfaceCapabilitiesPNext);

    return VK_SUCCESS;
}

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    // Android 不需要为每个 PresentImage 预先分配特殊资源，我们将在 present 时直接使用 ANativeWindow_lock。
    // 但可以在这里记录一下图像信息（可选）
    (void)image;
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    // 无特殊清理
    (void)image;
}

VkResult AndroidSurfaceKHR::present(PresentImage *image)
{
    // 1. 锁定当前窗口的下一个缓冲区
    ANativeWindow_Buffer buffer;
    int result = ANativeWindow_lock(window, &buffer, nullptr);
    if (result != 0)
    {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // 2. 获取源图像的信息
    const Image *vkImage = image->getImage();
    const VkExtent3D &extent = vkImage->getExtent();
    int srcRowPitch = vkImage->rowPitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);

    // 3. 确定像素格式（假设图像格式与窗口格式兼容，简化处理）
    //    实际应检查 buffer.format 并与 VkFormat 匹配，这里假设均为 RGBA_8888
    uint8_t *dst = static_cast<uint8_t *>(buffer.bits);
    int dstRowPitch = buffer.stride * 4; // 每个像素 4 字节（RGBA）

    // 4. 逐行复制图像数据（可能需处理 stride 不匹配的情况）
    uint8_t *src = nullptr;
    VkResult status = vkImage->lock(PresentImage::getImageMemory(), VK_IMAGE_LAYOUT_GENERAL, srcRowPitch, nullptr, &src);
    if (status != VK_SUCCESS)
    {
        ANativeWindow_unlockAndPost(window);
        return status;
    }

    for (uint32_t row = 0; row < extent.height; ++row)
    {
        memcpy(dst + row * dstRowPitch, src + row * srcRowPitch, std::min(srcRowPitch, dstRowPitch));
    }

    vkImage->unlock();

    // 5. 解锁并提交缓冲区，触发显示
    ANativeWindow_unlockAndPost(window);

    return VK_SUCCESS;
}

}  // namespace vk
