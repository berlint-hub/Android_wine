/*
 * AndroidWine's first Android-surface-to-Horizon adapter contract.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 */
#include "androidwine_vk_surface_adapter.h"

#include <stddef.h>
#include <string.h>

VkBool32 androidwine_vk_surface_adapter_bind(
    struct androidwine_vk_surface_adapter *adapter,
    const struct ANativeWindow *android_window,
    void *horizon_window,
    PFN_vkCreateViSurfaceNN create_vi_surface)
{
    if (adapter == NULL || android_window == NULL || horizon_window == NULL ||
        create_vi_surface == NULL)
        return VK_FALSE;

    adapter->android_window = android_window;
    adapter->horizon_window = horizon_window;
    adapter->create_vi_surface = create_vi_surface;
    return VK_TRUE;
}

void androidwine_vk_surface_adapter_unbind(
    struct androidwine_vk_surface_adapter *adapter)
{
    if (adapter != NULL)
        memset(adapter, 0, sizeof(*adapter));
}

VkResult androidwine_vkCreateAndroidSurfaceKHR(
    const struct androidwine_vk_surface_adapter *adapter,
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

    if (adapter->android_window == NULL || adapter->horizon_window == NULL ||
        adapter->create_vi_surface == NULL ||
        create_info->window != adapter->android_window)
        return VK_ERROR_SURFACE_LOST_KHR;

    const VkViSurfaceCreateInfoNN vi_info = {
        .sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN,
        .pNext = NULL,
        .flags = 0,
        .window = adapter->horizon_window,
    };
    return adapter->create_vi_surface(instance, &vi_info, allocator, surface);
}
