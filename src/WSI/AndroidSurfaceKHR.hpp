// AndroidSurfaceKHR.hpp
#ifndef SWIFTSHADER_ANDROIDSURFACEKHR_HPP_INCLUDED
#define SWIFTSHADER_ANDROIDSURFACEKHR_HPP_INCLUDED

#include "VkSurfaceKHR.hpp"
#include <android/native_window.h>

namespace vk {

class AndroidSurfaceKHR : public SurfaceKHR
{
public:
    AndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo);
    ~AndroidSurfaceKHR() override;

    static VkResult Create(const VkAllocationCallbacks *pAllocator,
                           const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                           VkSurfaceKHR *pSurface);

    // 实现基类纯虚函数
    void destroySurface(const VkAllocationCallbacks *pAllocator) override;
    VkResult getSurfaceCapabilities(const void *pSurfaceInfoPNext,
                                    VkSurfaceCapabilitiesKHR *pSurfaceCapabilities,
                                    void *pSurfaceCapabilitiesPNext) const override;
    void attachImage(PresentImage *image) override;
    void detachImage(PresentImage *image) override;
    VkResult present(PresentImage *image) override;

private:
    ANativeWindow *window_;
    mutable bool surfaceLost_ = false;   // 标记窗口是否已失效
};

}  // namespace vk

#endif
