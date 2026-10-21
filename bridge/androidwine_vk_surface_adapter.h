/*
 * AndroidWine's Android-surface-to-Horizon adapter contract.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 */
#ifndef ANDROIDWINE_VK_SURFACE_ADAPTER_H
#define ANDROIDWINE_VK_SURFACE_ADAPTER_H

#include "androidwine_native_window.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One AndroidWine-owned ANativeWindow is associated with one Horizon window. */
struct androidwine_vk_surface_adapter {
    ANativeWindow *android_window;
    void *horizon_window;
    PFN_vkCreateViSurfaceNN create_vi_surface;
    uint32_t surface_references;
    VkBool32 active;
};

/* Zero-initialize before use. The window object must already be initialized. */
VkBool32 androidwine_vk_surface_adapter_bind(
    struct androidwine_vk_surface_adapter *adapter,
    ANativeWindow *android_window,
    void *horizon_window,
    PFN_vkCreateViSurfaceNN create_vi_surface);

VkBool32 androidwine_vk_surface_adapter_acquire_window(
    struct androidwine_vk_surface_adapter *adapter,
    ANativeWindow *android_window);

VkBool32 androidwine_vk_surface_adapter_release_window(
    struct androidwine_vk_surface_adapter *adapter,
    ANativeWindow *android_window);

VkBool32 androidwine_vk_surface_adapter_update_geometry(
    struct androidwine_vk_surface_adapter *adapter,
    ANativeWindow *android_window,
    int32_t width,
    int32_t height);

VkBool32 androidwine_vk_surface_adapter_unbind(
    struct androidwine_vk_surface_adapter *adapter);

/*
 * Direct shim used by the prototype; not yet exported via a Vulkan loader.
 * It retains the NDK-style window object for each created surface and
 * translates VkAndroidSurfaceCreateInfoKHR into VK_NN_vi_surface.
 */
VkResult androidwine_vkCreateAndroidSurfaceKHR(
    struct androidwine_vk_surface_adapter *adapter,
    VkInstance instance,
    const VkAndroidSurfaceCreateInfoKHR *create_info,
    const VkAllocationCallbacks *allocator,
    VkSurfaceKHR *surface);

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
