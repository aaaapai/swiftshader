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
#include "System/Debug.hpp"

#include <sync/sync.h>
#include <string.h>
#include <unistd.h>
#include <android/hardware_buffer.h>

// 检查是否支持AHardwareBuffer API
#if __ANDROID_API__ >= 26
#define HAVE_AHARDWAREBUFFER 1
#else
#define HAVE_AHARDWAREBUFFER 0
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
    
    // Android典型值：min=2 (双缓冲), max=3 (三缓冲)
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

    // 调用基类函数设置通用capabilities
    SetCommonSurfaceCapabilities(pSurfaceInfoPNext, pSurfaceCapabilities, pSurfaceCapabilitiesPNext);
    return VK_SUCCESS;
}

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    if (!nativeWindow) return;

    AndroidImage *androidImage = new AndroidImage();
    memset(androidImage, 0, sizeof(AndroidImage));
    
    const VkExtent3D &extent = image->getImage()->getExtent();
    androidImage->width = extent.width;
    androidImage->height = extent.height;
    androidImage->format = getNativeWindowFormat(image->getImage()->getFormat());
    
    // 配置native window buffer
    ANativeWindow_setBuffersGeometry(nativeWindow, extent.width, extent.height, androidImage->format);
    
    // 使用ANativeWindow_Buffer方式（更简单可靠）
    ANativeWindow_Buffer buffer;
    int result = ANativeWindow_lock(nativeWindow, &buffer, nullptr);
    if (result != 0)
    {
        delete androidImage;
        return;
    }
    
    androidImage->buffer = buffer;
    androidImage->cpuAddr = static_cast<uint8_t*>(buffer.bits);
    androidImage->stride = buffer.stride;
    androidImage->locked = true;
    
    // 尝试获取AHardwareBuffer（如果支持）
#if HAVE_AHARDWAREBUFFER
    AHardwareBuffer* hardwareBuffer = AHardwareBuffer_from_ANativeWindow(nativeWindow);
    if (hardwareBuffer)
    {
        androidImage->hardwareBuffer = hardwareBuffer;
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
        
        if (androidImage->locked && nativeWindow)
        {
            ANativeWindow_unlockAndPost(nativeWindow);
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
    
    // 复制图像数据到buffer
    if (androidImage->cpuAddr && androidImage->locked)
    {
        int bufferRowPitch = image->getImage()->rowPitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);
        int destRowPitch = androidImage->stride * 4; // 假设RGBA_8888格式，每像素4字节
        
        const VkExtent3D &extent = image->getImage()->getExtent();
        uint8_t* src = static_cast<uint8_t*>(image->getImage()->getTexelPointer(VkOffset3D{0,0,0}, VK_IMAGE_ASPECT_COLOR_BIT));
        uint8_t* dst = androidImage->cpuAddr;
        
        for (int y = 0; y < extent.height; y++)
        {
            memcpy(dst + y * destRowPitch, src + y * bufferRowPitch, extent.width * 4);
        }
        
        // 解锁并提交buffer
        ANativeWindow_unlockAndPost(nativeWindow);
        androidImage->locked = false;
    }
    
    imageMap.erase(it);
    delete androidImage;
    
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
        // 使用RGBA_FP16格式（如果支持）
        return WINDOW_FORMAT_RGBA_FP16;
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        // 使用RGBA_1010102格式（如果支持）
        return WINDOW_FORMAT_RGBA_1010102;
    default:
        return WINDOW_FORMAT_RGBA_8888;
    }
}

bool AndroidSurfaceKHR::waitForFence(int fenceFd) const
{
    if (fenceFd < 0) return true;
    
    // 等待fence
    const int timeoutMs = 3000;
    int result = sync_wait(fenceFd, timeoutMs);
    close(fenceFd);
    
    return result == 0;
}

}  // namespace vk
