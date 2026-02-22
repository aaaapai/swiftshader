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

#include <string.h>
#include <unistd.h>

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

    // Android 典型配置：最小2个缓冲区（双缓冲），最大3个（三缓冲）
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

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    if (!nativeWindow) return;

    AndroidImage *androidImage = new AndroidImage();
    memset(androidImage, 0, sizeof(AndroidImage));

    const VkExtent3D &extent = image->getImage()->getExtent();
    androidImage->width = extent.width;
    androidImage->height = extent.height;
    androidImage->format = getNativeWindowFormat(image->getImage()->getFormat());

    // 配置 native window 的缓冲区几何属性
    ANativeWindow_setBuffersGeometry(nativeWindow, extent.width, extent.height, androidImage->format);

    // 锁定窗口表面，获取 CPU 可访问的缓冲区
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(nativeWindow, &buffer, nullptr) != 0)
    {
        delete androidImage;
        return;
    }

    androidImage->buffer = buffer;
    androidImage->cpuAddr = static_cast<uint8_t*>(buffer.bits);
    androidImage->stride = buffer.stride;
    androidImage->locked = true;

    imageMap[image] = androidImage;
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    auto it = imageMap.find(image);
    if (it != imageMap.end())
    {
        AndroidImage *androidImage = it->second;

        // 如果缓冲区仍处于锁定状态，解锁并取消提交
        if (androidImage->locked && nativeWindow)
        {
            ANativeWindow_unlockAndPost(nativeWindow); // 实际会提交，但这里可能未准备好，用 cancel 更好？
            // 更好的做法是直接取消，但 ANativeWindow 没有直接的 cancel 接口，
            // unlockAndPost 会提交，可能导致显示不完整的内容，但 detach 通常发生在销毁时，可以接受。
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

    // 复制图像数据到窗口缓冲区
    if (androidImage->cpuAddr && androidImage->locked)
    {
        const VkExtent3D &extent = image->getImage()->getExtent();
        int srcRowPitch = image->getImage()->rowPitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);
        int dstRowPitch = androidImage->stride * 4; // 假设 RGBA_8888 格式，每像素4字节

        // 获取源图像数据指针
        VkImageSubresource subresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        uint8_t* src = static_cast<uint8_t*>(image->getImage()->getTexelPointer(VkOffset3D{0,0,0}, subresource));
        uint8_t* dst = androidImage->cpuAddr;

        for (uint32_t y = 0; y < extent.height; ++y)
        {
            memcpy(dst + y * dstRowPitch, src + y * srcRowPitch, extent.width * 4);
        }

        // 解锁并提交缓冲区到屏幕
        ANativeWindow_unlockAndPost(nativeWindow);
        androidImage->locked = false;
    }

    // 移除并释放资源
    delete androidImage;
    imageMap.erase(it);

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
        return WINDOW_FORMAT_RGBA_8888;
    }
}

}  // namespace vk
