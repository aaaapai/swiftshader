// AndroidSurfaceKHR.cpp
#include "AndroidSurfaceKHR.hpp"

#include "Vulkan/VkDeviceMemory.hpp"
#include "Vulkan/VkImage.hpp"

#include <cstring>

namespace vk {

AndroidSurfaceKHR::AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo)
    : window_(pCreateInfo->window)
{
    if (window_) {
        ANativeWindow_acquire(window_);
    }
}

AndroidSurfaceKHR::~AndroidSurfaceKHR()
{
    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
}

VkResult AndroidSurfaceKHR::Create(const VkAllocationCallbacks *pAllocator,
                                   const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                   VkSurfaceKHR *pSurface)
{
    if (!pCreateInfo || !pCreateInfo->window) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    void *memory = pAllocator->pfnAllocation(nullptr, sizeof(AndroidSurfaceKHR),
                                             alignof(AndroidSurfaceKHR),
                                             VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (!memory) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    AndroidSurfaceKHR *surface = new (memory) AndroidSurfaceKHR(pCreateInfo);
    *pSurface = *surface;   // 利用基类的 operator VkSurfaceKHR()
    return VK_SUCCESS;
}

void AndroidSurfaceKHR::destroySurface(const VkAllocationCallbacks *pAllocator)
{
    this->~AndroidSurfaceKHR();
    pAllocator->pfnFree(nullptr, this);
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                                   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                                   void *pSurfaceCapabilitiesPNext) const
{
    if (surfaceLost_ || !window_) {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    int32_t width = ANativeWindow_getWidth(window_);
    int32_t height = ANativeWindow_getHeight(window_);
    if (width <= 0 || height <= 0) {
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    pSurfaceCapabilities->minImageCount = 2;
    pSurfaceCapabilities->maxImageCount = 0;        // 无上限
    pSurfaceCapabilities->currentExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    pSurfaceCapabilities->minImageExtent = { 1, 1 };
    pSurfaceCapabilities->maxImageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    pSurfaceCapabilities->maxImageArrayLayers = 1;
    pSurfaceCapabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    pSurfaceCapabilities->supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // 允许扩展信息链
    SetCommonSurfaceCapabilities(pSurfaceInfoPNext, pSurfaceCapabilities, pSurfaceCapabilitiesPNext);
    return VK_SUCCESS;
}

void AndroidSurfaceKHR::attachImage(PresentImage *image)
{
    // 简单实现：无需特殊操作
}

void AndroidSurfaceKHR::detachImage(PresentImage *image)
{
    // 简单实现：无需特殊操作
}

VkResult AndroidSurfaceKHR::present(PresentImage *image)
{
    if (surfaceLost_ || !window_) {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // 获取图像数据
    const vk::Image *vkImage = image->getImage();
    vk::DeviceMemory *memory = image->getImageMemory();
    if (!vkImage || !memory) {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    VkExtent3D imageExtent = vkImage->getExtent();
    int32_t windowWidth = ANativeWindow_getWidth(window_);
    int32_t windowHeight = ANativeWindow_getHeight(window_);
    if (windowWidth <= 0 || windowHeight <= 0) {
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // 检查尺寸是否匹配（若不一致返回重建设备）
    if (static_cast<uint32_t>(windowWidth) != imageExtent.width ||
        static_cast<uint32_t>(windowHeight) != imageExtent.height) {
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    // 获取图像内存指针
    uint8_t *imageData = static_cast<uint8_t*>(memory->getOffsetPointer(0));
    VkFormat format = vkImage->getFormat(VK_IMAGE_ASPECT_COLOR_BIT);
    if (format != VK_FORMAT_B8G8R8A8_UNORM && format != VK_FORMAT_R8G8B8A8_UNORM) {
        // 简单起见只支持常见 32 位格式，实际应做格式转换
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // 锁定窗口表面并获取缓冲区
    ANativeWindow_Buffer buffer;
    ARect dirtyRect = { 0, 0, windowWidth, windowHeight };
    int32_t lockResult = ANativeWindow_lock(window_, &buffer, &dirtyRect);
    if (lockResult != 0) {
        surfaceLost_ = true;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // 确保缓冲区格式匹配（假设都是 32bpp）
    if (buffer.format != AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM &&
        buffer.format != AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM) {
        ANativeWindow_unlockAndPost(window_);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // 拷贝图像数据到窗口缓冲区（逐行复制）
    uint32_t srcStride = imageExtent.width * 4;   // 假设 4 字节每像素
    uint32_t dstStride = buffer.stride * 4;
    for (uint32_t y = 0; y < imageExtent.height; ++y) {
        memcpy(static_cast<uint8_t*>(buffer.bits) + y * dstStride,
               imageData + y * srcStride,
               srcStride);
    }

    // 解锁并提交缓冲区到屏幕
    ANativeWindow_unlockAndPost(window_);
    return VK_SUCCESS;
}

}  // namespace vk
