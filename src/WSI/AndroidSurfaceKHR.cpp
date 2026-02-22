// Copyright 2020 The SwiftShader Authors. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.

#include "AndroidSurfaceKHR.hpp"

#include "Vulkan/VkDeviceMemory.hpp"
#include "Vulkan/VkImage.hpp"
#include "System/Debug.hpp"

#include <android/native_window.h>
#include <cstdio>
#include <cstring>
#include <algorithm>  // for std::min
#include <unistd.h>

// 使用标准错误输出打印调试信息，确保在控制台可见
#define LOGI(fmt, ...) do { fprintf(stderr, "INFO: " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOGE(fmt, ...) do { fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__); } while(0)

namespace vk {

bool AndroidSurfaceKHR::isSupported()
{
    LOGI("isSupported() = true");
    return true;
}

AndroidSurfaceKHR::AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, void* mem)
    : window(pCreateInfo->window)
    , nativeWindow(nullptr)
{
    LOGI("AndroidSurfaceKHR constructor: pCreateInfo->window = %p", window);
    if (window)
    {
        nativeWindow = window;
        ANativeWindow_acquire(nativeWindow);
        LOGI("Acquired nativeWindow = %p", nativeWindow);
    }
    else
    {
        LOGE("pCreateInfo->window is NULL!");
    }
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks* pAllocator)
{
    LOGI("destroySurface: nativeWindow = %p", nativeWindow);
    if (nativeWindow)
    {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
        LOGI("Released nativeWindow");
    }
}

size_t AndroidSurfaceKHR::ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR* pCreateInfo)
{
    LOGI("ComputeRequiredAllocationSize = 0");
    return 0;
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void* pSurfaceInfoPNext,
                                                    VkSurfaceCapabilitiesKHR* pSurfaceCapabilities,
                                                    void* pSurfaceCapabilitiesPNext) const
{
    if (!nativeWindow)
    {
        LOGE("getSurfaceCapabilities: nativeWindow is null");
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    int32_t width = ANativeWindow_getWidth(nativeWindow);
    int32_t height = ANativeWindow_getHeight(nativeWindow);
    LOGI("getSurfaceCapabilities: window size = %dx%d", width, height);

    pSurfaceCapabilities->currentExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    pSurfaceCapabilities->minImageExtent = { 1, 1 };
    pSurfaceCapabilities->maxImageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    pSurfaceCapabilities->minImageCount = 2;
    pSurfaceCapabilities->maxImageCount = 3;
    pSurfaceCapabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    pSurfaceCapabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->supportedUsageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;
    pSurfaceCapabilities->maxImageArrayLayers = 1;

    SetCommonSurfaceCapabilities(pSurfaceInfoPNext, pSurfaceCapabilities, pSurfaceCapabilitiesPNext);
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getSurfaceFormats(const void* pSurfaceInfoPNext,
                                              uint32_t* pSurfaceFormatCount,
                                              VkSurfaceFormatKHR* pSurfaceFormats,
                                              void* pSurfaceFormatsPNext) const
{
    // Android 通常支持以下格式
    static const VkSurfaceFormatKHR formats[] = {
        { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_R8G8B8A8_SRGB,   VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_R5G6B5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
    };
    const uint32_t formatCount = sizeof(formats) / sizeof(formats[0]);

    if (pSurfaceFormats == nullptr)
    {
        *pSurfaceFormatCount = formatCount;
        LOGI("getSurfaceFormats: returning count = %u", formatCount);
        return VK_SUCCESS;
    }

    uint32_t toCopy = std::min(*pSurfaceFormatCount, formatCount);
    for (uint32_t i = 0; i < toCopy; ++i)
    {
        pSurfaceFormats[i] = formats[i];
    }
    *pSurfaceFormatCount = toCopy;
    LOGI("getSurfaceFormats: copied %u formats", toCopy);
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getPresentModes(const void* pSurfaceInfoPNext,
                                            uint32_t* pPresentModeCount,
                                            VkPresentModeKHR* pPresentModes,
                                            void* pPresentModesPNext) const
{
    static const VkPresentModeKHR modes[] = {
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
    };
    const uint32_t modeCount = sizeof(modes) / sizeof(modes[0]);

    if (pPresentModes == nullptr)
    {
        *pPresentModeCount = modeCount;
        LOGI("getPresentModes: returning count = %u", modeCount);
        return VK_SUCCESS;
    }

    uint32_t toCopy = std::min(*pPresentModeCount, modeCount);
    for (uint32_t i = 0; i < toCopy; ++i)
    {
        pPresentModes[i] = modes[i];
    }
    *pPresentModeCount = toCopy;
    LOGI("getPresentModes: copied %u modes", toCopy);
    return VK_SUCCESS;
}

void AndroidSurfaceKHR::attachImage(PresentImage* image)
{
    if (!nativeWindow)
    {
        LOGE("attachImage: nativeWindow is null");
        return;
    }

    // 使用头文件中定义的 AndroidImage 结构
    AndroidImage* androidImage = new AndroidImage();
    memset(androidImage, 0, sizeof(AndroidImage));

    const VkExtent3D& extent = image->getImage()->getExtent();
    androidImage->width = extent.width;
    androidImage->height = extent.height;
    androidImage->format = getNativeWindowFormat(image->getImage()->getFormat());

    LOGI("attachImage: trying to set buffer geometry %dx%d, format %d",
         extent.width, extent.height, androidImage->format);

    int32_t err = ANativeWindow_setBuffersGeometry(nativeWindow, extent.width, extent.height, androidImage->format);
    if (err != 0)
    {
        LOGE("ANativeWindow_setBuffersGeometry failed: %d", err);
        delete androidImage;
        return;
    }

    ANativeWindow_Buffer buffer;
    err = ANativeWindow_lock(nativeWindow, &buffer, nullptr);
    if (err != 0)
    {
        LOGE("ANativeWindow_lock failed: %d", err);
        delete androidImage;
        return;
    }

    androidImage->buffer = buffer;
    androidImage->cpuAddr = static_cast<uint8_t*>(buffer.bits);
    androidImage->stride = buffer.stride;
    androidImage->locked = true;

    imageMap[image] = androidImage;
    LOGI("attachImage: success, cpuAddr=%p, stride=%d", androidImage->cpuAddr, androidImage->stride);
}

void AndroidSurfaceKHR::detachImage(PresentImage* image)
{
    auto it = imageMap.find(image);
    if (it != imageMap.end())
    {
        AndroidImage* androidImage = it->second;
        LOGI("detachImage: image %p, locked=%d", image, androidImage->locked);

        if (androidImage->locked && nativeWindow)
        {
            ANativeWindow_unlockAndPost(nativeWindow);
            LOGI("detachImage: unlocked window");
        }

        delete androidImage;
        imageMap.erase(it);
        LOGI("detachImage: removed and deleted AndroidImage");
    }
    else
    {
        LOGE("detachImage: image %p not found in map", image);
    }
}

VkResult AndroidSurfaceKHR::present(PresentImage* image)
{
    auto it = imageMap.find(image);
    if (it == imageMap.end() || !nativeWindow)
    {
        LOGE("present: image not found or nativeWindow null");
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    AndroidImage* androidImage = it->second;
    if (!androidImage->cpuAddr || !androidImage->locked)
    {
        LOGE("present: androidImage buffer not ready (cpuAddr=%p, locked=%d)",
             androidImage->cpuAddr, androidImage->locked);
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    const VkExtent3D& extent = image->getImage()->getExtent();
    int srcRowPitch = image->getImage()->rowPitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);
    int dstRowPitch = androidImage->stride * 4; // 假设目标格式为 RGBA_8888

    VkImageSubresource subresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    uint8_t* src = static_cast<uint8_t*>(image->getImage()->getTexelPointer(VkOffset3D{0,0,0}, subresource));
    if (!src)
    {
        LOGE("present: source image texel pointer is null");
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    uint8_t* dst = androidImage->cpuAddr;
    LOGI("present: copying %dx%d, srcRowPitch=%d, dstRowPitch=%d", extent.width, extent.height, srcRowPitch, dstRowPitch);

    for (uint32_t y = 0; y < extent.height; ++y)
    {
        memcpy(dst + y * dstRowPitch, src + y * srcRowPitch, extent.width * 4);
    }

    int err = ANativeWindow_unlockAndPost(nativeWindow);
    if (err != 0)
    {
        LOGE("ANativeWindow_unlockAndPost failed: %d", err);
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    androidImage->locked = false;

    delete androidImage;
    imageMap.erase(it);
    LOGI("present: success");
    return VK_SUCCESS;
}

int AndroidSurfaceKHR::getNativeWindowFormat(VkFormat format) const
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        return WINDOW_FORMAT_RGBA_8888;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
        return WINDOW_FORMAT_RGB_565;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return WINDOW_FORMAT_RGBA_FP16;
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        return WINDOW_FORMAT_RGBA_1010102;
    default:
        LOGI("getNativeWindowFormat: unknown VkFormat %d, defaulting to RGBA_8888", format);
        return WINDOW_FORMAT_RGBA_8888;
    }
}

}  // namespace vk
