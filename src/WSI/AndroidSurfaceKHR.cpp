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
#include <cstring>

namespace vk {

// Helper: map Vulkan format to AHardwareBuffer_Format
static uint32_t VulkanFormatToAHBFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
        return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8X8_UNORM:
        return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
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
{
    if (window_)
    {
        ANativeWindow_acquire(window_);
    }
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{
    // Release all allocated hardware buffers
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

    if (window_)
    {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }

    // Call destructor and free memory
    this->~AndroidSurfaceKHR();
    if (pAllocator)
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
    if (!pCreateInfo || !pCreateInfo->window || !pSurface)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    void *memory;
    if (pAllocator)
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
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    AndroidSurfaceKHR *surface = new (memory) AndroidSurfaceKHR(pCreateInfo, memory);
    *pSurface = *surface; // Convert via ObjectBase's operator VkSurfaceKHR
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                                   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                                   void *pSurfaceCapabilitiesPNext) const
{
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
    pSurfaceCapabilities->maxImageCount = 0; // no limit
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
    if (!window_ || surfaceLost_)
        return nullptr;

    const vk::Image *vkImage = image->getImage();
    VkExtent3D extent = vkImage->getExtent();
    VkFormat format = vkImage->getFormat(VK_IMAGE_ASPECT_COLOR_BIT);

    uint32_t ahbFormat = VulkanFormatToAHBFormat(format);
    if (ahbFormat == 0)
        return nullptr; // unsupported format

    // Configure hardware buffer description
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
        return nullptr;
    }

    // Lock to get CPU writable pointer
    void *mappedPtr = nullptr;
    ARect rect = { 0, 0, static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height) };
    if (AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                             -1, &rect, &mappedPtr) != 0)
    {
        AHardwareBuffer_release(buffer);
        return nullptr;
    }

    // Obtain stride (bytes per row) – we may need it for later copying
    AHardwareBuffer_Desc actualDesc;
    AHardwareBuffer_describe(buffer, &actualDesc);
    uint32_t stride = actualDesc.stride * 4; // assume 4 bytes per pixel, adjust for different formats

    buffers_[image] = { buffer, mappedPtr, stride };
    return mappedPtr;
}

void AndroidSurfaceKHR::releaseImageMemory(PresentImage *image)
{
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
    // No special handling required
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    // No special handling required (release is called separately)
}

VkResult AndroidSurfaceKHR::present(PresentImage *image)
{
    if (surfaceLost_ || !window_)
    {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // Find the hardware buffer associated with this image
    auto it = buffers_.find(image);
    if (it == buffers_.end())
    {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }
    HardwareBufferResource &res = it->second;

    // Check window size
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

    // Option 1: Directly use AHardwareBuffer with ANativeWindow (requires API level 29+)
    // Here we fall back to simple lock/copy for compatibility.
    ANativeWindow_Buffer outBuffer;
    ARect dirty = { 0, 0, windowWidth, windowHeight };
    if (ANativeWindow_lock(window_, &outBuffer, &dirty) != 0)
    {
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // Copy from hardware buffer to window buffer
    // Assumes both are 32bpp RGBA/X
    uint32_t srcStride = res.stride; // bytes per row in AHB
    uint32_t dstStride = outBuffer.stride * 4; // bytes per row in window buffer
    uint8_t *srcBase = static_cast<uint8_t*>(res.mappedPtr);
    uint8_t *dstBase = static_cast<uint8_t*>(outBuffer.bits);
    for (uint32_t y = 0; y < imageExtent.height; ++y)
    {
        memcpy(dstBase + y * dstStride, srcBase + y * srcStride, imageExtent.width * 4);
    }

    ANativeWindow_unlockAndPost(window_);
    return VK_SUCCESS;
}

} // namespace vk
