/*
 * Minimal Android NDK ANativeWindow function subset for the Horizon prototype.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 *
 * Keep the public handle opaque, like Android's public NDK declaration. The
 * backing fields and the androidwine_* lifecycle helpers are private to this
 * bridge and are not Android's platform-private native_window ABI.
 */
#ifndef ANDROIDWINE_NATIVE_WINDOW_H
#define ANDROIDWINE_NATIVE_WINDOW_H

#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#ifndef VK_USE_PLATFORM_VI_NN
#define VK_USE_PLATFORM_VI_NN
#endif
#include <vulkan/vulkan.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ANativeWindow ANativeWindow;

/* Private bridge-side lifecycle/geometry helpers, not Android NDK exports. */
ANativeWindow *androidwine_anative_window_create(void *horizon_window,
                                                 int32_t width,
                                                 int32_t height,
                                                 int32_t format);
void androidwine_anative_window_destroy(ANativeWindow *window);
VkBool32 androidwine_anative_window_is_valid(ANativeWindow *window);
void *androidwine_anative_window_horizon_window(ANativeWindow *window);
uint32_t androidwine_anative_window_reference_count(ANativeWindow *window);
uint64_t androidwine_anative_window_geometry_generation(ANativeWindow *window);
VkBool32 androidwine_anative_window_try_acquire(ANativeWindow *window);
VkBool32 androidwine_anative_window_try_release(ANativeWindow *window);
VkBool32 androidwine_anative_window_update_geometry(ANativeWindow *window,
                                                    int32_t width,
                                                    int32_t height,
                                                    int32_t format);
void androidwine_anative_window_invalidate(ANativeWindow *window);

/* Public NDK C function ABI subset. These exports can later be resolved from
 * Android native libraries by the planned ELF/import shim. */
void ANativeWindow_acquire(ANativeWindow *window);
void ANativeWindow_release(ANativeWindow *window);
int32_t ANativeWindow_getWidth(ANativeWindow *window);
int32_t ANativeWindow_getHeight(ANativeWindow *window);
int32_t ANativeWindow_getFormat(ANativeWindow *window);
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *window,
                                         int32_t width,
                                         int32_t height,
                                         int32_t format);

#ifdef __cplusplus
}
#endif

#endif
