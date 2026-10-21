/*
 * Minimal Android NDK ANativeWindow function subset for the Horizon prototype.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 */
#include "androidwine_native_window.h"

#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>

#define ANDROIDWINE_WINDOW_MAGIC UINT32_C(0x41574e44)

struct ANativeWindow {
    uint32_t androidwine_magic;
    void *horizon_window;
    atomic_bool state_lock;
    uint32_t references;
    bool active;
    int32_t width;
    int32_t height;
    int32_t format;
    uint64_t geometry_generation;
};

static void window_lock(ANativeWindow *window)
{
    bool expected = false;
    while (!atomic_compare_exchange_weak_explicit(&window->state_lock, &expected, true,
                                                   memory_order_acquire,
                                                   memory_order_relaxed))
        expected = false;
}

static void window_unlock(ANativeWindow *window)
{
    atomic_store_explicit(&window->state_lock, false, memory_order_release);
}

static bool window_is_usable_locked(const ANativeWindow *window)
{
    return window->androidwine_magic == ANDROIDWINE_WINDOW_MAGIC &&
           window->active && window->references != 0 &&
           window->horizon_window != NULL;
}

ANativeWindow *androidwine_anative_window_create(void *horizon_window,
                                                 int32_t width,
                                                 int32_t height,
                                                 int32_t format)
{
    if (horizon_window == NULL || width < 0 || height < 0 || format < 0 ||
        ((width == 0) != (height == 0)))
        return NULL;

    ANativeWindow *window = calloc(1, sizeof(*window));
    if (window == NULL)
        return NULL;
    window->horizon_window = horizon_window;
    atomic_init(&window->state_lock, false);
    window->references = 1;
    window->active = true;
    window->width = width;
    window->height = height;
    window->format = format;
    window->geometry_generation = (width > 0 && height > 0) ? 1 : 0;
    window->androidwine_magic = ANDROIDWINE_WINDOW_MAGIC;
    return window;
}

void androidwine_anative_window_destroy(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return;
    window_lock(window);
    if (window->references != 0) {
        window_unlock(window);
        return;
    }
    window->active = false;
    window->horizon_window = NULL;
    window->androidwine_magic = 0;
    window_unlock(window);
    free(window);
}

VkBool32 androidwine_anative_window_is_valid(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return VK_FALSE;
    window_lock(window);
    VkBool32 valid = window_is_usable_locked(window) ? VK_TRUE : VK_FALSE;
    window_unlock(window);
    return valid;
}

void *androidwine_anative_window_horizon_window(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return NULL;
    window_lock(window);
    void *horizon_window = window_is_usable_locked(window) ? window->horizon_window : NULL;
    window_unlock(window);
    return horizon_window;
}

uint32_t androidwine_anative_window_reference_count(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return 0;
    window_lock(window);
    uint32_t references = window->references;
    window_unlock(window);
    return references;
}

uint64_t androidwine_anative_window_geometry_generation(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return 0;
    window_lock(window);
    uint64_t generation = window->geometry_generation;
    window_unlock(window);
    return generation;
}

VkBool32 androidwine_anative_window_update_geometry(ANativeWindow *window,
                                                    int32_t width,
                                                    int32_t height,
                                                    int32_t format)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC ||
        width <= 0 || height <= 0 || format < 0)
        return VK_FALSE;
    window_lock(window);
    if (!window_is_usable_locked(window)) {
        window_unlock(window);
        return VK_FALSE;
    }
    window->width = width;
    window->height = height;
    if (format != 0)
        window->format = format;
    ++window->geometry_generation;
    if (window->geometry_generation == 0)
        window->geometry_generation = 1;
    window_unlock(window);
    return VK_TRUE;
}

void androidwine_anative_window_invalidate(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return;
    window_lock(window);
    window->active = false;
    window_unlock(window);
}

VkBool32 androidwine_anative_window_try_acquire(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return VK_FALSE;
    window_lock(window);
    VkBool32 acquired = VK_FALSE;
    if (window_is_usable_locked(window) && window->references != UINT32_MAX) {
        ++window->references;
        acquired = VK_TRUE;
    }
    window_unlock(window);
    return acquired;
}

VkBool32 androidwine_anative_window_try_release(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return VK_FALSE;
    window_lock(window);
    VkBool32 released = VK_FALSE;
    if (window->references != 0) {
        --window->references;
        if (window->references == 0)
            window->active = false;
        released = VK_TRUE;
    }
    window_unlock(window);
    return released;
}

void ANativeWindow_acquire(ANativeWindow *window)
{
    (void)androidwine_anative_window_try_acquire(window);
}

void ANativeWindow_release(ANativeWindow *window)
{
    (void)androidwine_anative_window_try_release(window);
}

int32_t ANativeWindow_getWidth(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return 0;
    window_lock(window);
    int32_t width = window_is_usable_locked(window) ? window->width : 0;
    window_unlock(window);
    return width;
}

int32_t ANativeWindow_getHeight(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return 0;
    window_lock(window);
    int32_t height = window_is_usable_locked(window) ? window->height : 0;
    window_unlock(window);
    return height;
}

int32_t ANativeWindow_getFormat(ANativeWindow *window)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC)
        return 0;
    window_lock(window);
    int32_t format = window_is_usable_locked(window) ? window->format : 0;
    window_unlock(window);
    return format;
}

int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *window,
                                         int32_t width,
                                         int32_t height,
                                         int32_t format)
{
    if (window == NULL || window->androidwine_magic != ANDROIDWINE_WINDOW_MAGIC ||
        width < 0 || height < 0 || format < 0)
        return -EINVAL;
    window_lock(window);
    if (!window_is_usable_locked(window) ||
        (width != 0 && width != window->width) ||
        (height != 0 && height != window->height) ||
        (format != 0 && format != window->format)) {
        window_unlock(window);
        return -EINVAL;
    }
    window_unlock(window);
    return 0;
}
