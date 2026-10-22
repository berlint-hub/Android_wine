/*
 * AndroidWine's Android-surface-to-Horizon adapter contract.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 */
#include "androidwine_vk_surface_adapter.h"

#include <stdint.h>
#include <string.h>

static VkBool32 window_matches(const struct androidwine_vk_surface_adapter *adapter,
                               ANativeWindow *android_window)
{
    return adapter != NULL && adapter->active == VK_TRUE &&
           adapter->android_window != NULL &&
           adapter->android_window == android_window &&
           adapter->horizon_window != NULL &&
           androidwine_anative_window_horizon_window(android_window) == adapter->horizon_window &&
           androidwine_anative_window_is_valid(android_window);
}

VkBool32 androidwine_vk_surface_adapter_bind(
    struct androidwine_vk_surface_adapter *adapter,
    ANativeWindow *android_window,
    void *horizon_window,
    PFN_vkCreateViSurfaceNN create_vi_surface)
{
    if (adapter == NULL || adapter->active == VK_TRUE ||
        adapter->surface_references != 0 || android_window == NULL ||
        horizon_window == NULL || create_vi_surface == NULL ||
        !androidwine_anative_window_is_valid(android_window) ||
        androidwine_anative_window_horizon_window(android_window) != horizon_window)
        return VK_FALSE;

    memset(adapter, 0, sizeof(*adapter));
    adapter->android_window = android_window;
    adapter->horizon_window = horizon_window;
    adapter->create_vi_surface = create_vi_surface;
    adapter->active = VK_TRUE;
    return VK_TRUE;
}

VkBool32 androidwine_vk_surface_adapter_acquire_window(
    struct androidwine_vk_surface_adapter *adapter,
    ANativeWindow *android_window)
{
    if (!window_matches(adapter, android_window))
        return VK_FALSE;
    return androidwine_anative_window_try_acquire(android_window);
}

VkBool32 androidwine_vk_surface_adapter_release_window(
    struct androidwine_vk_surface_adapter *adapter,
    ANativeWindow *android_window)
{
    if (!window_matches(adapter, android_window))
        return VK_FALSE;
    return androidwine_anative_window_try_release(android_window);
}

VkBool32 androidwine_vk_surface_adapter_update_geometry(
    struct androidwine_vk_surface_adapter *adapter,
    ANativeWindow *android_window,
    int32_t width,
    int32_t height)
{
    if (!window_matches(adapter, android_window))
        return VK_FALSE;
    return androidwine_anative_window_update_geometry(android_window, width, height, 0);
}

VkBool32 androidwine_vk_surface_adapter_unbind(
    struct androidwine_vk_surface_adapter *adapter)
{
    if (adapter == NULL || adapter->surface_references != 0)
        return VK_FALSE;
    memset(adapter, 0, sizeof(*adapter));
    return VK_TRUE;
}

VkResult androidwine_vkCreateAndroidSurfaceKHR(
    struct androidwine_vk_surface_adapter *adapter,
    VkInstance instance,
    const VkAndroidSurfaceCreateInfoKHR *create_info,
    const VkAllocationCallbacks *allocator,
    VkSurfaceKHR *surface)
{
    if (surface != NULL)
        *surface = VK_NULL_HANDLE;

    if (adapter == NULL || create_info == NULL || surface == NULL ||
        instance == VK_NULL_HANDLE ||
        create_info->sType != VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR ||
        create_info->flags != 0)
        return VK_ERROR_INITIALIZATION_FAILED;

    /* This first slice intentionally has no Android pNext-chain support. */
    if (create_info->pNext != NULL)
        return VK_ERROR_FEATURE_NOT_PRESENT;

    ANativeWindow *window = create_info->window;
    if (!window_matches(adapter, window) || ANativeWindow_getWidth(window) <= 0 ||
        ANativeWindow_getHeight(window) <= 0)
        return VK_ERROR_SURFACE_LOST_KHR;
    if (adapter->surface_references == UINT32_MAX ||
        androidwine_anative_window_reference_count(window) == UINT32_MAX)
        return VK_ERROR_TOO_MANY_OBJECTS;

    const VkViSurfaceCreateInfoNN vi_info = {
        .sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN,
        .pNext = NULL,
        .flags = 0,
        .window = adapter->horizon_window,
    };

    if (!androidwine_anative_window_try_acquire(window))
        return VK_ERROR_SURFACE_LOST_KHR;

    VkResult result = adapter->create_vi_surface(instance, &vi_info, allocator, surface);
    if (result == VK_SUCCESS) {
        ++adapter->surface_references;
    } else {
        (void)androidwine_anative_window_try_release(window);
    }
    return result;
}

void androidwine_vkDestroyAndroidSurfaceKHR(
    struct androidwine_vk_surface_adapter *adapter,
    VkInstance instance,
    VkSurfaceKHR surface,
    const VkAllocationCallbacks *allocator,
    PFN_vkDestroySurfaceKHR destroy_surface)
{
    if (surface == VK_NULL_HANDLE || destroy_surface == NULL)
        return;

    destroy_surface(instance, surface, allocator);
    if (adapter == NULL || adapter->surface_references == 0 ||
        adapter->android_window == NULL)
        return;

    --adapter->surface_references;
    (void)androidwine_anative_window_try_release(adapter->android_window);
}
