/*
 * Self-authored Android ARM64 shared-library import fixture.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 *
 * This library is built for ELF inspection only. It is not loaded or executed
 * on Android or Switch, and it does not constitute APK support.
 */
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <android/native_window.h>
#include <vulkan/vulkan.h>

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__)
#define ANDROIDWINE_FIXTURE_EXPORT __attribute__((visibility("default")))
#else
#define ANDROIDWINE_FIXTURE_EXPORT
#endif

ANDROIDWINE_FIXTURE_EXPORT
int32_t androidwine_loader_fixture_probe_window(ANativeWindow *window,
                                                VkInstance instance)
{
    if (window == NULL)
        return -1;

    ANativeWindow_acquire(window);
    int32_t width = ANativeWindow_getWidth(window);
    int32_t height = ANativeWindow_getHeight(window);
    int32_t format = ANativeWindow_getFormat(window);
    int32_t geometry_result = ANativeWindow_setBuffersGeometry(window, 0, 0, 0);
    PFN_vkVoidFunction create_android_surface =
        vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR");
    ANativeWindow_release(window);

    if (width <= 0 || height <= 0 || format < 0 || geometry_result != 0)
        return -2;
    if (create_android_surface == NULL)
        return -3;
    return 0;
}
