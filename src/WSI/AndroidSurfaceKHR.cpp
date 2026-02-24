#include "AndroidSurfaceKHR.hpp"
#include <cstring>  // For memset
#include <cstdio>   // For debug logs

namespace vk {

AndroidSurfaceKHR::AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo)
    : SurfaceKHR(),
      window_(pCreateInfo->window),  // 指定的 Android Window
      width_(0),
      height_(0),
      format_(0),
      currentImageIndex_(0),
      imageCount_(2)  // 简单实现双缓冲
{
    if (window_)
    {
        ANativeWindow_acquire(window_);
        // 查询窗口属性
        ANativeWindow_query(window_, NATIVE_WINDOW_WIDTH, &width_);
        ANativeWindow_query(window_, NATIVE_WINDOW_HEIGHT, &height_);
        ANativeWindow_query(window_, NATIVE_WINDOW_FORMAT, &format_);
    }
}

AndroidSurfaceKHR::~AndroidSurfaceKHR()
{
    if (window_)
    {
        ANativeWindow_release(window_);
    }
}

VkResult AndroidSurfaceKHR::Create(const VkAllocationCallbacks *pAllocator,
                                   const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                   VkSurfaceKHR *pSurface)
{
    if (!pCreateInfo || !pSurface || !pCreateInfo->window)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    AndroidSurfaceKHR *surface = new (pAllocator) AndroidSurfaceKHR(pCreateInfo);
    *pSurface = reinterpret_cast<VkSurfaceKHR>(surface);
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getSurfaceSupport(uint32_t queueFamilyIndex, VkBool32 *pSupported) const
{
    *pSupported = VK_TRUE;  // 简化实现，全部支持
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getSurfaceCapabilities(VkSurfaceCapabilitiesKHR *pSurfaceCapabilities) const
{
    if (!pSurfaceCapabilities) return VK_ERROR_INITIALIZATION_FAILED;

    memset(pSurfaceCapabilities, 0, sizeof(VkSurfaceCapabilitiesKHR));
    pSurfaceCapabilities->minImageCount = 2;
    pSurfaceCapabilities->maxImageCount = 3;
    pSurfaceCapabilities->currentExtent = { uint32_t(width_), uint32_t(height_) };
    pSurfaceCapabilities->minImageExtent = { 1, 1 };
    pSurfaceCapabilities->maxImageExtent = { uint32_t(width_), uint32_t(height_) };
    pSurfaceCapabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getSurfaceFormats(uint32_t *pSurfaceFormatCount, VkSurfaceFormatKHR *pSurfaceFormats) const
{
    static const VkSurfaceFormatKHR formats[] = {
        { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
    };

    if (pSurfaceFormats)
    {
        if (*pSurfaceFormatCount >= 1)
        {
            std::memcpy(pSurfaceFormats, formats, sizeof(formats));
        }
        else
        {
            return VK_INCOMPLETE;
        }
    }

    *pSurfaceFormatCount = 1;
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::getPresentModes(uint32_t *pPresentModeCount, VkPresentModeKHR *pPresentModes) const
{
    static const VkPresentModeKHR modes[] = {
        VK_PRESENT_MODE_FIFO_KHR,
    };

    if (pPresentModes)
    {
        if (*pPresentModeCount >= 1)
        {
            std::memcpy(pPresentModes, modes, sizeof(modes));
        }
        else
        {
            return VK_INCOMPLETE;
        }
    }

    *pPresentModeCount = 1;
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::acquireImage(uint64_t timeout, uint32_t *imageIndex)
{
    *imageIndex = currentImageIndex_;  // 简单模拟双缓冲
    currentImageIndex_ = (currentImageIndex_ + 1) % imageCount_;
    return VK_SUCCESS;
}

VkResult AndroidSurfaceKHR::present(uint32_t imageIndex, const VkPresentInfoKHR *presentInfo)
{
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(window_, &buffer, nullptr) < 0)
    {
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    // 填充屏幕 (简单清空为绿色)
    memset(buffer.bits, 0, buffer.stride * buffer.height * 4);
    ANativeWindow_unlockAndPost(window_);
    return VK_SUCCESS;
}

}  // namespace vk
