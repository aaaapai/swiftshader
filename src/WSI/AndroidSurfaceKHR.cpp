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
#include <unistd.h>  // for gettid()

namespace vk {

// 获取线程 ID 用于日志
static int getThreadId()
{
    return static_cast<int>(gettid());
}

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
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, window=%p\n",
                __FUNCTION__, getThreadId(), this, window_);
    }
}

AndroidSurfaceKHR::~AndroidSurfaceKHR()
{
    fprintf(stderr, "[AndroidSurfaceKHR::~%s] tid=%d, this=%p\n",
            __FUNCTION__, getThreadId(), this);
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, entering\n",
            __FUNCTION__, getThreadId(), this);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, lock acquired, buffers size=%zu\n",
                __FUNCTION__, getThreadId(), this, buffers_.size());

        // 释放所有硬件缓冲
        for (auto &pair : buffers_)
        {
            auto &res = pair.second;
            fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, releasing buffer for image %p (buffer=%p, mappedPtr=%p)\n",
                    __FUNCTION__, getThreadId(), this, pair.first, res.buffer, res.mappedPtr);
            if (res.mappedPtr)
            {
                int unlockRes = AHardwareBuffer_unlock(res.buffer, nullptr);
                fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, AHardwareBuffer_unlock returned %d\n",
                        __FUNCTION__, getThreadId(), this, unlockRes);
            }
            if (res.buffer)
            {
                AHardwareBuffer_release(res.buffer);
                fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, AHardwareBuffer_release called\n",
                        __FUNCTION__, getThreadId(), this);
            }
        }
        buffers_.clear();

        // 标记表面已丢失
        surfaceLost_ = true;
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, surface marked lost\n",
                __FUNCTION__, getThreadId(), this);

        // 注意：不释放 window_，因为生命周期由调用者管理
        window_ = nullptr;
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, window_ set to null\n",
                __FUNCTION__, getThreadId(), this);
    }

    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, exiting (will not free memory, that's done by vkDestroySurfaceKHR)\n",
            __FUNCTION__, getThreadId(), this);
}

size_t AndroidSurfaceKHR::ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo)
{
    return sizeof(AndroidSurfaceKHR);
}

VkResult AndroidSurfaceKHR::Create(const VkAllocationCallbacks *pAllocator,
                                   const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                   VkSurfaceKHR *pSurface)
{
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d\n", __FUNCTION__, getThreadId());

    if (!pCreateInfo || !pCreateInfo->window || !pSurface)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, invalid parameters\n", __FUNCTION__, getThreadId());
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    void *memory;
    if (pAllocator && pAllocator->pfnAllocation)
    {
        memory = pAllocator->pfnAllocation(pAllocator->pUserData,
                                           sizeof(AndroidSurfaceKHR),
                                           alignof(AndroidSurfaceKHR),
                                           VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, allocated memory via pfnAllocation: %p\n",
                __FUNCTION__, getThreadId(), memory);
    }
    else
    {
        memory = malloc(sizeof(AndroidSurfaceKHR));
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, allocated memory via malloc: %p\n",
                __FUNCTION__, getThreadId(), memory);
    }
    if (!memory)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, out of host memory\n", __FUNCTION__, getThreadId());
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    AndroidSurfaceKHR *surface = new (memory) AndroidSurfaceKHR(pCreateInfo, memory);
    *pSurface = *surface; // 通过 ObjectBase 的 operator VkSurfaceKHR 转换
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, surface created at %p, window=%p, VkSurfaceKHR=%p\n",
            __FUNCTION__, getThreadId(), surface, surface->window_, *pSurface);
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                                   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                                   void *pSurfaceCapabilitiesPNext) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (surfaceLost_ || !window_)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, surface lost or window null\n",
                __FUNCTION__, getThreadId(), this);
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    int32_t width = ANativeWindow_getWidth(window_);
    int32_t height = ANativeWindow_getHeight(window_);
    if (width <= 0 || height <= 0)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, window size invalid (%d x %d), marking lost\n",
                __FUNCTION__, getThreadId(), this, width, height);
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
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, surface lost or window null\n",
                __FUNCTION__, getThreadId(), this);
        return nullptr;
    }

    const vk::Image *vkImage = image->getImage();
    if (!vkImage)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, null image\n",
                __FUNCTION__, getThreadId(), this);
        return nullptr;
    }

    VkExtent3D extent = vkImage->getExtent();
    VkFormat format = vkImage->getFormat(VK_IMAGE_ASPECT_COLOR_BIT);
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, image extent=%ux%u, format=%d\n",
            __FUNCTION__, getThreadId(), this, extent.width, extent.height, format);

    uint32_t ahbFormat = VulkanFormatToAHBFormat(format);
    if (ahbFormat == 0)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, unsupported format %d\n",
                __FUNCTION__, getThreadId(), this, format);
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
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, AHardwareBuffer_allocate failed\n",
                __FUNCTION__, getThreadId(), this);
        return nullptr;
    }
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, allocated AHB %p\n",
            __FUNCTION__, getThreadId(), this, buffer);

    // 锁定获取 CPU 可写指针
    void *mappedPtr = nullptr;
    ARect rect = { 0, 0, static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height) };
    if (AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                             -1, &rect, &mappedPtr) != 0)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, AHardwareBuffer_lock failed\n",
                __FUNCTION__, getThreadId(), this);
        AHardwareBuffer_release(buffer);
        return nullptr;
    }
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, AHB locked, mappedPtr=%p\n",
            __FUNCTION__, getThreadId(), this, mappedPtr);

    // 获取实际描述信息，计算步幅
    AHardwareBuffer_Desc actualDesc;
    AHardwareBuffer_describe(buffer, &actualDesc);
    uint32_t stride = actualDesc.stride * 4; // 假设 4 字节每像素
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, AHB stride=%u\n",
            __FUNCTION__, getThreadId(), this, stride);

    buffers_[image] = { buffer, mappedPtr, stride };
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, stored in buffers_, size now %zu\n",
            __FUNCTION__, getThreadId(), this, buffers_.size());

    return mappedPtr;
}

void AndroidSurfaceKHR::releaseImageMemory(PresentImage *image)
{
    std::lock_guard<std::mutex> lock(mutex_);
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, releasing for image %p\n",
            __FUNCTION__, getThreadId(), this, image);

    auto it = buffers_.find(image);
    if (it != buffers_.end())
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, found buffer %p, mappedPtr=%p\n",
                __FUNCTION__, getThreadId(), this, it->second.buffer, it->second.mappedPtr);
        if (it->second.mappedPtr)
        {
            int unlockRes = AHardwareBuffer_unlock(it->second.buffer, nullptr);
            fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, AHardwareBuffer_unlock returned %d\n",
                    __FUNCTION__, getThreadId(), this, unlockRes);
        }
        if (it->second.buffer)
        {
            AHardwareBuffer_release(it->second.buffer);
            fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, AHardwareBuffer_release called\n",
                    __FUNCTION__, getThreadId(), this);
        }
        buffers_.erase(it);
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, erased, size now %zu\n",
                __FUNCTION__, getThreadId(), this, buffers_.size());
    }
    else
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, image not found in buffers_\n",
                __FUNCTION__, getThreadId(), this);
    }
}

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, image=%p\n",
            __FUNCTION__, getThreadId(), this, image);
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, image=%p\n",
            __FUNCTION__, getThreadId(), this, image);
}

VkResult AndroidSurfaceKHR::present(PresentImage *image)
{
    std::lock_guard<std::mutex> lock(mutex_);
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, presenting image %p\n",
            __FUNCTION__, getThreadId(), this, image);

    if (surfaceLost_ || !window_)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, surface lost or window null\n",
                __FUNCTION__, getThreadId(), this);
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // 查找与此图像关联的硬件缓冲
    auto it = buffers_.find(image);
    if (it == buffers_.end())
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, image not found in buffers_\n",
                __FUNCTION__, getThreadId(), this);
        return VK_ERROR_OUT_OF_DATE_KHR;
    }
    HardwareBufferResource &res = it->second;
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, found buffer %p, mappedPtr=%p\n",
            __FUNCTION__, getThreadId(), this, res.buffer, res.mappedPtr);

    // 检查窗口大小
    int32_t windowWidth = ANativeWindow_getWidth(window_);
    int32_t windowHeight = ANativeWindow_getHeight(window_);
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, window size %dx%d\n",
            __FUNCTION__, getThreadId(), this, windowWidth, windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, invalid window size, marking lost\n",
                __FUNCTION__, getThreadId(), this);
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    const vk::Image *vkImage = image->getImage();
    VkExtent3D imageExtent = vkImage->getExtent();
    if (static_cast<uint32_t>(windowWidth) != imageExtent.width ||
        static_cast<uint32_t>(windowHeight) != imageExtent.height)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, size mismatch: window=%dx%d, image=%dx%d\n",
                __FUNCTION__, getThreadId(), this, windowWidth, windowHeight,
                imageExtent.width, imageExtent.height);
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    // 锁定窗口缓冲区
    ANativeWindow_Buffer outBuffer;
    ARect dirty = { 0, 0, windowWidth, windowHeight };
    if (ANativeWindow_lock(window_, &outBuffer, &dirty) != 0)
    {
        fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, ANativeWindow_lock failed, marking lost\n",
                __FUNCTION__, getThreadId(), this);
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, window locked, buffer bits=%p, stride=%d\n",
            __FUNCTION__, getThreadId(), this, outBuffer.bits, outBuffer.stride);

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
    fprintf(stderr, "[AndroidSurfaceKHR::%s] tid=%d, this=%p, unlockAndPost done\n",
            __FUNCTION__, getThreadId(), this);
    return VK_SUCCESS;
}

} // namespace vk
