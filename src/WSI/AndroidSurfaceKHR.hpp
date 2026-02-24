#ifndef SWIFTSHADER_ANDROIDSURFACEKHR_HPP_INCLUDED
#define SWIFTSHADER_ANDROIDSURFACEKHR_HPP_INCLUDED

#include "VkSurfaceKHR.hpp"
#include <vulkan/vulkan.h>
#include <Android/android/native_window.h>

namespace vk {

class AndroidSurfaceKHR : public SurfaceKHR
{
public:
    AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo);
    ~AndroidSurfaceKHR() override;

    static VkResult Create(const VkAllocationCallbacks *pAllocator,
                           const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                           VkSurfaceKHR *pSurface);

    VkResult getSurfaceSupport(uint32_t queueFamilyIndex, VkBool32 *pSupported) const override;
    VkResult getSurfaceCapabilities(VkSurfaceCapabilitiesKHR *pSurfaceCapabilities) const override;
    VkResult getSurfaceFormats(uint32_t *pSurfaceFormatCount, VkSurfaceFormatKHR *pSurfaceFormats) const override;
    VkResult getPresentModes(uint32_t *pPresentModeCount, VkPresentModeKHR *pPresentModes) const override;

    VkResult acquireImage(uint64_t timeout, uint32_t *imageIndex) override;
    VkResult present(uint32_t imageIndex, const VkPresentInfoKHR *presentInfo) override;

private:
    ANativeWindow *window_;  // 描述 Android 绘图窗口
    int32_t width_;
    int32_t height_;
    int32_t format_;

    // Buffer image management (for acquire/present logic)
    uint32_t currentImageIndex_;
    uint32_t imageCount_;
};

}  // namespace vk

#endif  // SWIFTSHADER_ANDROIDSURFACEKHR_HPP_INCLUDED
