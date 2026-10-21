/*
 * AndroidWine's Android-surface-to-Horizon adapter contract.
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
 * One explicitly registered Android-side window handle maps to one Horizon
 * window. The ANativeWindow pointer is still an opaque token, not an Android
 * ABI-compatible object. The runtime must own its real ABI and lifecycle.
 */
struct androidwine_vk_surface_adapter {
    const struct ANativeWindow *android_window;
    void *horizon_window;
    PFN_vkCreateViSurfaceNN create_vi_surface;
    uint32_t width;
    uint32_t height;
    uint32_t window_references;
    uint32_t surface_references;
    uint64_t geometry_generation;
    VkBool32 active;
};

/* Zero-initialize the adapter before first use. Binding starts with one owner
 * reference; width/height may be zero until the runtime reports initial geometry.
 * A live binding cannot be overwritten; unbind it after destroying all surfaces. */
VkBool32 androidwine_vk_surface_adapter_bind(
    struct androidwine_vk_surface_adapter *adapter,
    const struct ANativeWindow *android_window,
    void *horizon_window,
    uint32_t width,
    uint32_t height,
    PFN_vkCreateViSurfaceNN create_vi_surface);

VkBool32 androidwine_vk_surface_adapter_acquire_window(
    struct androidwine_vk_surface_adapter *adapter,
    const struct ANativeWindow *android_window);

VkBool32 androidwine_vk_surface_adapter_release_window(
    struct androidwine_vk_surface_adapter *adapter,
    const struct ANativeWindow *android_window);

VkBool32 androidwine_vk_surface_adapter_update_geometry(
    struct androidwine_vk_surface_adapter *adapter,
    const struct ANativeWindow *android_window,
    uint32_t width,
    uint32_t height);

VkBool32 androidwine_vk_surface_adapter_unbind(
    struct androidwine_vk_surface_adapter *adapter);

/*
 * Deliberately a shim function, not an exported Vulkan loader entry point.
 * It retains the registered window for the lifetime of each created surface
 * and translates the Android request into VK_NN_vi_surface for NXVK.
 */
VkResult androidwine_vkCreateAndroidSurfaceKHR(
    struct androidwine_vk_surface_adapter *adapter,
    VkInstance instance,
    const VkAndroidSurfaceCreateInfoKHR *create_info,
    const VkAllocationCallbacks *allocator,
    VkSurfaceKHR *surface);

/* Paired wrapper to release the surface-held window reference. */
void androidwine_vkDestroyAndroidSurfaceKHR(
    struct androidwine_vk_surface_adapter *adapter,
    VkInstance instance,
    VkSurfaceKHR surface,
    const VkAllocationCallbacks *allocator,
    PFN_vkDestroySurfaceKHR destroy_surface);

#ifdef __cplusplus
}
#endif

#endif
