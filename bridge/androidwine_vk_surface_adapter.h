/*
 * AndroidWine's first Android-surface-to-Horizon adapter contract.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 */
#ifndef ANDROIDWINE_VK_SURFACE_ADAPTER_H
#define ANDROIDWINE_VK_SURFACE_ADAPTER_H

/* Required Vulkan platform declarations; no Android SDK headers are used. */
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#ifndef VK_USE_PLATFORM_VI_NN
#define VK_USE_PLATFORM_VI_NN
#endif
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * One explicitly registered Android-side window token maps to one Horizon
 * window. The pointed-to ANativeWindow is opaque here: the eventual Android
 * runtime owns its ABI/lifecycle and must register it before surface creation.
 */
struct androidwine_vk_surface_adapter {
    const struct ANativeWindow *android_window;
    void *horizon_window;
    PFN_vkCreateViSurfaceNN create_vi_surface;
};

VkBool32 androidwine_vk_surface_adapter_bind(
    struct androidwine_vk_surface_adapter *adapter,
    const struct ANativeWindow *android_window,
    void *horizon_window,
    PFN_vkCreateViSurfaceNN create_vi_surface);

void androidwine_vk_surface_adapter_unbind(
    struct androidwine_vk_surface_adapter *adapter);

/*
 * Deliberately a shim function, not an exported Vulkan loader entry point.
 * It validates a synthetic/registered ANativeWindow token, then translates the
 * Android surface create request into VK_NN_vi_surface for NXVK on Horizon.
 */
VkResult androidwine_vkCreateAndroidSurfaceKHR(
    const struct androidwine_vk_surface_adapter *adapter,
    VkInstance instance,
    const VkAndroidSurfaceCreateInfoKHR *create_info,
    const VkAllocationCallbacks *allocator,
    VkSurfaceKHR *surface);

#ifdef __cplusplus
}
#endif

#endif
