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
#include <cstdio>      // for fprintf
#include <cstring>

namespace vk {

// 修复宏定义：正确处理可变参数和字符串连接
#define LOG_ENTRY(fmt, ...)  fprintf(stderr, "[AndroidSurfaceKHR] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   fprintf(stderr, "[AndroidSurfaceKHR] %s: " fmt "\n", __func__, ##__VA_ARGS__)
#define LOG_RESULT(ret)      fprintf(stderr, "[AndroidSurfaceKHR] %s -> %d\n", __func__, ret)

bool AndroidSurfaceKHR::isSupported()
{
    LOG_ENTRY("");  // 空参数
    bool supported = true;
    LOG_INFO("supported = %d", supported);
    return supported;
}

AndroidSurfaceKHR::AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, void *mem)
    : window(pCreateInfo->window)
    , width(0)
    , height(0)
    , format(0)
{
    LOG_ENTRY("pCreateInfo=%p, mem=%p", pCreateInfo, mem);
    ANativeWindow_acquire(window);
    width = ANativeWindow_getWidth(window);
    height = ANativeWindow_getHeight(window);
    format = ANativeWindow_getFormat(window);
    LOG_INFO("window=%p, width=%d, height=%d, format=%d", window, width, height, format);
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{
    LOG_ENTRY("pAllocator=%p", pAllocator);
    ANativeWindow_release(window);
    LOG_INFO("window released");
}

size_t AndroidSurfaceKHR::ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo)
{
    LOG_ENTRY("pCreateInfo=%p", pCreateInfo);
    size_t size = 0;
    LOG_INFO("return size=%zu", size);
    return size;
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                                   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                                   void *pSurfaceCapabilitiesPNext) const
{
    LOG_ENTRY("pSurfaceInfoPNext=%p, pSurfaceCapabilities=%p, pSurfaceCapabilitiesPNext=%p",
              pSurfaceInfoPNext, pSurfaceCapabilities, pSurfaceCapabilitiesPNext);

    int32_t currentWidth = ANativeWindow_getWidth(window);
    int32_t currentHeight = ANativeWindow_getHeight(window);
    LOG_INFO("current window size: %dx%d", currentWidth, currentHeight);

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

    LOG_RESULT(VK_SUCCESS);
    return VK_SUCCESS;
}

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    LOG_ENTRY("image=%p", image);
    (void)image;
    LOG_INFO("attachImage done");
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    LOG_ENTRY("image=%p", image);
    (void)image;
    LOG_INFO("detachImage done");
}

VkResult AndroidSurfaceKHR::present(PresentImage *image)
{
    LOG_ENTRY("image=%p", image);

    // 锁定窗口缓冲区
    ANativeWindow_Buffer buffer;
    int lockResult = ANativeWindow_lock(window, &buffer, nullptr);
    LOG_INFO("ANativeWindow_lock -> %d, buffer.bits=%p, buffer.stride=%d, buffer.width=%d, buffer.height=%d, buffer.format=%d",
             lockResult, buffer.bits, buffer.stride, buffer.width, buffer.height, buffer.format);
    if (lockResult != 0)
    {
        LOG_INFO("lock failed, returning VK_ERROR_SURFACE_LOST_KHR");
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    const Image *vkImage = image->getImage();
    if (!vkImage)
    {
        LOG_INFO("image->getImage() returned null");
        ANativeWindow_unlockAndPost(window);
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    const VkExtent3D &extent = vkImage->getExtent();
    LOG_INFO("image extent: %dx%d", extent.width, extent.height);

    // 检查窗口尺寸是否与图像尺寸匹配
    int32_t windowWidth = ANativeWindow_getWidth(window);
    int32_t windowHeight = ANativeWindow_getHeight(window);
    LOG_INFO("current window size: %dx%d", windowWidth, windowHeight);
    if (extent.width != (uint32_t)windowWidth || extent.height != (uint32_t)windowHeight)
    {
        LOG_INFO("size mismatch, returning VK_ERROR_OUT_OF_DATE_KHR");
        ANativeWindow_unlockAndPost(window);
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    // 根据窗口格式计算每像素字节数
    int bpp = 0;
    const char* formatStr = "unknown";
    switch (format)
    {
    case WINDOW_FORMAT_RGBA_8888:
        bpp = 4;
        formatStr = "RGBA_8888";
        break;
    case WINDOW_FORMAT_RGBX_8888:
        bpp = 4;
        formatStr = "RGBX_8888";
        break;
    case WINDOW_FORMAT_RGB_565:
        bpp = 2;
        formatStr = "RGB_565";
        break;
    default:
        bpp = 4; // 保守假设
        formatStr = "default (4)";
        break;
    }
    LOG_INFO("window format=%d (%s), bpp=%d", format, formatStr, bpp);

    int dstRowPitch = buffer.stride * bpp;
    LOG_INFO("dstRowPitch = stride(%d) * bpp(%d) = %d", buffer.stride, bpp, dstRowPitch);

    // 验证目标缓冲区是否足够容纳图像数据
    size_t requiredSize = extent.height * dstRowPitch;
    size_t actualSize = buffer.height * buffer.stride * bpp; // buffer.height 是行数，stride 是像素/行
    LOG_INFO("requiredSize=%zu, actualSize=%zu", requiredSize, actualSize);
    if (requiredSize > actualSize)
    {
        LOG_INFO("buffer too small, returning VK_ERROR_OUT_OF_DATE_KHR");
        ANativeWindow_unlockAndPost(window);
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    // 执行像素复制
    LOG_INFO("calling vkImage->copyTo(dst=%p, dstRowPitch=%d)", buffer.bits, dstRowPitch);
    vkImage->copyTo(static_cast<uint8_t*>(buffer.bits), dstRowPitch);
    LOG_INFO("copyTo finished");

    // 解锁并提交显示
    int unlockResult = ANativeWindow_unlockAndPost(window);
    LOG_INFO("ANativeWindow_unlockAndPost -> %d", unlockResult);

    LOG_RESULT(VK_SUCCESS);
    return VK_SUCCESS;
}

}  // namespace vk
