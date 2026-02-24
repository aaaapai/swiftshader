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

#include "AndroidSurfaceKHR.hpp"

#include "Vulkan/VkDeviceMemory.hpp"
#include "Vulkan/VkImage.hpp"

#include <android/data_space.h>
#include <cstdio>
#include <cstring>

namespace vk {

// Helper: map Vulkan format to AHardwareBuffer_Format
static uint32_t VulkanFormatToAHBFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
        return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8_UNORM:
        return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
        return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    default:
        return 0; // unsupported
    }
}

AndroidSurfaceKHR::AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, void *mem)
    : window_(pCreateInfo->window)
    , surfaceLost_(false)
{
    if (window_)
    {
        fprintf(stderr, "AndroidSurfaceKHR created with window %p\n", window_);
    }
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{
    std::lock_guard<std::mutex> lock(mutex_);

    fprintf(stderr, "AndroidSurfaceKHR::destroySurface\n");

    // 释放所有硬件缓冲
    for (auto &pair : buffers_)
    {
        auto &res = pair.second;
        if (res.mappedPtr)
        {
            AHardwareBuffer_unlock(res.buffer, nullptr);
        }
        if (res.buffer)
        {
            AHardwareBuffer_release(res.buffer);
        }
    }
    buffers_.clear();

    // 标记表面已丢失
    surfaceLost_ = true;

    // 注意：不释放 window_，因为生命周期由调用者管理
    window_ = nullptr;

    // 调用析构函数并释放内存
    this->~AndroidSurfaceKHR();
    if (pAllocator && pAllocator->pfnFree)
    {
        pAllocator->pfnFree(pAllocator->pUserData, this);
    }
    else
    {
        free(this);
    }
}

size_t AndroidSurfaceKHR::ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo)
{
    return sizeof(AndroidSurfaceKHR);
}

VkResult AndroidSurfaceKHR::Create(const VkAllocationCallbacks *pAllocator,
                                   const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                   VkSurfaceKHR *pSurface)
{
    fprintf(stderr, "AndroidSurfaceKHR::Create\n");

    if (!pCreateInfo || !pCreateInfo->window || !pSurface)
    {
        fprintf(stderr, "AndroidSurfaceKHR::Create: invalid parameters\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    void *memory;
    if (pAllocator && pAllocator->pfnAllocation)
    {
        memory = pAllocator->pfnAllocation(pAllocator->pUserData,
                                           sizeof(AndroidSurfaceKHR),
                                           alignof(AndroidSurfaceKHR),
                                           VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    }
    else
    {
        memory = malloc(sizeof(AndroidSurfaceKHR));
    }
    if (!memory)
    {
        fprintf(stderr, "AndroidSurfaceKHR::Create: out of host memory\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    AndroidSurfaceKHR *surface = new (memory) AndroidSurfaceKHR(pCreateInfo, memory);
    *pSurface = *surface; // 通过 ObjectBase 的 operator VkSurfaceKHR 转换
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                                   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                                   void *pSurfaceCapabilitiesPNext) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (surfaceLost_ || !window_)
    {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    int32_t width = ANativeWindow_getWidth(window_);
    int32_t height = ANativeWindow_getHeight(window_);
    if (width <= 0 || height <= 0)
    {
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    pSurfaceCapabilities->minImageCount = 2;
    pSurfaceCapabilities->maxImageCount = 0; // 无限制
    pSurfaceCapabilities->currentExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    pSurfaceCapabilities->minImageExtent = { 1, 1 };
    pSurfaceCapabilities->maxImageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    pSurfaceCapabilities->maxImageArrayLayers = 1;
    pSurfaceCapabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    pSurfaceCapabilities->supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    SetCommonSurfaceCapabilities(pSurfaceInfoPNext, pSurfaceCapabilities, pSurfaceCapabilitiesPNext);
    return VK_SUCCESS;
}

void *AndroidSurfaceKHR::allocateImageMemory(PresentImage *image, const VkMemoryAllocateInfo &allocateInfo)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!window_ || surfaceLost_)
    {
        fprintf(stderr, "AndroidSurfaceKHR::allocateImageMemory: surface lost\n");
        return nullptr;
    }

    const vk::Image *vkImage = image->getImage();
    if (!vkImage)
    {
        fprintf(stderr, "AndroidSurfaceKHR::allocateImageMemory: null image\n");
        return nullptr;
    }

    VkExtent3D extent = vkImage->getExtent();
    VkFormat format = vkImage->getFormat(VK_IMAGE_ASPECT_COLOR_BIT);

    uint32_t ahbFormat = VulkanFormatToAHBFormat(format);
    if (ahbFormat == 0)
    {
        fprintf(stderr, "AndroidSurfaceKHR::allocateImageMemory: unsupported format %d\n", format);
        return nullptr;
    }

    // 配置硬件缓冲描述
    AHardwareBuffer_Desc desc = {};
    desc.width = extent.width;
    desc.height = extent.height;
    desc.layers = 1;
    desc.format = ahbFormat;
    desc.usage = AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
                 AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

    AHardwareBuffer *buffer;
    if (AHardwareBuffer_allocate(&desc, &buffer) != 0)
    {
        fprintf(stderr, "AndroidSurfaceKHR::allocateImageMemory: AHardwareBuffer_allocate failed\n");
        return nullptr;
    }

    // 锁定获取 CPU 可写指针
    void *mappedPtr = nullptr;
    ARect rect = { 0, 0, static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height) };
    if (AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                             -1, &rect, &mappedPtr) != 0)
    {
        fprintf(stderr, "AndroidSurfaceKHR::allocateImageMemory: AHardwareBuffer_lock failed\n");
        AHardwareBuffer_release(buffer);
        return nullptr;
    }

    // 获取实际描述信息，计算步幅
    AHardwareBuffer_Desc actualDesc;
    AHardwareBuffer_describe(buffer, &actualDesc);
    uint32_t stride = actualDesc.stride * 4; // 假设 4 字节每像素

    buffers_[image] = { buffer, mappedPtr, stride };
    return mappedPtr;
}

void AndroidSurfaceKHR::releaseImageMemory(PresentImage *image)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = buffers_.find(image);
    if (it != buffers_.end())
    {
        if (it->second.mappedPtr)
        {
            AHardwareBuffer_unlock(it->second.buffer, nullptr);
        }
        if (it->second.buffer)
        {
            AHardwareBuffer_release(it->second.buffer);
        }
        buffers_.erase(it);
    }
}

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    // 无需特殊处理
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    // 无需特殊处理
}

VkResult AndroidSurfaceKHR::present(PresentImage *image)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (surfaceLost_ || !window_)
    {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // 查找与此图像关联的硬件缓冲
    auto it = buffers_.find(image);
    if (it == buffers_.end())
    {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }
    HardwareBufferResource &res = it->second;

    // 检查窗口大小
    int32_t windowWidth = ANativeWindow_getWidth(window_);
    int32_t windowHeight = ANativeWindow_getHeight(window_);
    if (windowWidth <= 0 || windowHeight <= 0)
    {
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    const vk::Image *vkImage = image->getImage();
    VkExtent3D imageExtent = vkImage->getExtent();
    if (static_cast<uint32_t>(windowWidth) != imageExtent.width ||
        static_cast<uint32_t>(windowHeight) != imageExtent.height)
    {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    // 锁定窗口缓冲区
    ANativeWindow_Buffer outBuffer;
    ARect dirty = { 0, 0, windowWidth, windowHeight };
    if (ANativeWindow_lock(window_, &outBuffer, &dirty) != 0)
    {
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // 从硬件缓冲复制到窗口缓冲区
    // 假设两者都是 32bpp RGBA
    uint32_t srcStride = res.stride;          // AHB 的字节步幅
    uint32_t dstStride = outBuffer.stride * 4; // 窗口缓冲区的字节步幅
    uint8_t *srcBase = static_cast<uint8_t*>(res.mappedPtr);
    uint8_t *dstBase = static_cast<uint8_t*>(outBuffer.bits);
    
    for (uint32_t y = 0; y < imageExtent.height; ++y)
    {
        memcpy(dstBase + y * dstStride, 
               srcBase + y * srcStride, 
               imageExtent.width * 4);
    }

    ANativeWindow_unlockAndPost(window_);
    return VK_SUCCESS;
}

} // namespace vk
