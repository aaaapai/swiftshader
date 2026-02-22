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

#include "AndroidSurfaceKHR.hpp"

#include "Vulkan/VkDeviceMemory.hpp"
#include "Vulkan/VkImage.hpp"

#include <vndk/window.h>
#include <vndk/hardware_buffer.h>
#include <sync/sync.h>
#include <string.h>
#include <unistd.h>

// 检查是否可以使用AHardwareBuffer API
#if __ANDROID_API__ >= 26
#define HAVE_AHARDWAREBUFFER 1
#endif

namespace vk {

bool AndroidSurfaceKHR::isSupported()
{
    return true;
}

AndroidSurfaceKHR::AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo, void *mem)
    : window(pCreateInfo->window)
    , nativeWindow(nullptr)
{
    if (window)
    {
        nativeWindow = window;
        ANativeWindow_acquire(nativeWindow);
    }
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{
    if (nativeWindow)
    {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
}

size_t AndroidSurfaceKHR::ComputeRequiredAllocationSize(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo)
{
    return 0;
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                                  VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                                  void *pSurfaceCapabilitiesPNext) const
{
    int32_t width = ANativeWindow_getWidth(nativeWindow);
    int32_t height = ANativeWindow_getHeight(nativeWindow);
    
    pSurfaceCapabilities->currentExtent = { 
        static_cast<uint32_t>(width), 
        static_cast<uint32_t>(height) 
    };
    pSurfaceCapabilities->minImageExtent = { 1, 1 };
    pSurfaceCapabilities->maxImageExtent = { 
        static_cast<uint32_t>(width), 
        static_cast<uint32_t>(height) 
    };
    
    // 查询native window支持的最小和最大buffer计数
    int minUndeqeueudBuffers = 0;
    int maxDequeuedBuffers = 0;
    native_window_get_min_undequeued_buffer_count(nativeWindow, &minUndeqeueudBuffers);
    
    // Android典型值：min=2 (双缓冲), max=3 (三缓冲)
    pSurfaceCapabilities->minImageCount = minUndeqeueudBuffers + 1;  // 至少需要一个dequeued buffer
    pSurfaceCapabilities->maxImageCount = 3;  // 或从native window查询
    
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

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    if (!nativeWindow) return;

    AndroidImage *androidImage = new AndroidImage();
    memset(androidImage, 0, sizeof(AndroidImage));
    
    const VkExtent3D &extent = image->getImage()->getExtent();
    int format = getNativeWindowFormat(image->getImage()->getFormat());
    
    // 配置native window buffer
    ANativeWindow_setBuffersGeometry(nativeWindow, extent.width, extent.height, format);
    
    // 从native window获取buffer
    ANativeWindowBuffer* buffer = nullptr;
    int fenceFd = -1;
    
    if (ANativeWindow_dequeueBuffer(nativeWindow, &buffer, &fenceFd) != 0)
    {
        delete androidImage;
        return;
    }
    
    // 等待fence
    if (fenceFd != -1)
    {
        sync_wait(fenceFd, -1);
        close(fenceFd);
    }
    
    androidImage->buffer = buffer;
    androidImage->fenceFd = -1;
    
#ifdef HAVE_AHARDWAREBUFFER
    // 使用AHardwareBuffer API获取CPU访问权限（现代方式）
    AHardwareBuffer* hardwareBuffer = AHardwareBuffer_from_ANativeWindowBuffer(buffer);
    if (hardwareBuffer)
    {
        void* cpuAddr = nullptr;
        // 锁定硬件buffer用于写入
        if (AHardwareBuffer_lock(hardwareBuffer, 
                                 AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                                 -1, nullptr, &cpuAddr) == 0)
        {
            androidImage->cpuAddr = static_cast<uint8_t*>(cpuAddr);
            androidImage->hardwareBuffer = hardwareBuffer;
        }
    }
#else
    // 回退到ANativeWindowBuffer的直接操作（传统方式）
    void* cpuAddr = nullptr;
    if (buffer->common.magic == ANDROID_NATIVE_BUFFER_MAGIC)
    {
        // 假设可以通过某种方式获取CPU地址
        // 实际可能需要使用gralloc或其他机制
    }
#endif
    
    imageMap[image] = androidImage;
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    auto it = imageMap.find(image);
    if (it != imageMap.end())
    {
        AndroidImage *androidImage = it->second;
        
#ifdef HAVE_AHARDWAREBUFFER
        if (androidImage->cpuAddr && androidImage->hardwareBuffer)
        {
            AHardwareBuffer_unlock(androidImage->hardwareBuffer, nullptr);
        }
#endif
        
        if (androidImage->buffer && nativeWindow)
        {
            ANativeWindow_cancelBuffer(nativeWindow, androidImage->buffer, -1);
        }
        
        delete androidImage;
        imageMap.erase(it);
    }
}

VkResult AndroidSurfaceKHR::present(PresentImage *image)
{
    auto it = imageMap.find(image);
    if (it == imageMap.end() || !nativeWindow)
    {
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    
    AndroidImage *androidImage = it->second;
    const VkExtent3D &extent = image->getImage()->getExtent();
    
    // 复制图像数据到buffer
    if (androidImage->cpuAddr)
    {
        int bufferRowPitch = image->getImage()->rowPitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);
        image->getImage()->copyTo(androidImage->cpuAddr, bufferRowPitch);
        
#ifdef HAVE_AHARDWAREBUFFER
        if (androidImage->hardwareBuffer)
        {
            AHardwareBuffer_unlock(androidImage->hardwareBuffer, nullptr);
            androidImage->cpuAddr = nullptr;
        }
#endif
    }
    
    // 提交buffer给SurfaceFlinger
    int fenceFd = -1;
    if (ANativeWindow_queueBuffer(nativeWindow, androidImage->buffer, &fenceFd) == 0)
    {
        if (fenceFd != -1)
        {
            close(fenceFd);
        }
        
        imageMap.erase(it);
        delete androidImage;
        return VK_SUCCESS;
    }
    
    return VK_ERROR_SURFACE_LOST_KHR;
}

int AndroidSurfaceKHR::getNativeWindowFormat(VkFormat format) const
{
    // 可以直接使用AHardwareBuffer格式
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        return WINDOW_FORMAT_RGBA_8888;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
        return WINDOW_FORMAT_RGB_565;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return WINDOW_FORMAT_RGBA_FP16;
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK:
        return WINDOW_FORMAT_RGBA_1010102;
    default:
        return WINDOW_FORMAT_RGBA_8888;
    }
}

}  // namespace vk
