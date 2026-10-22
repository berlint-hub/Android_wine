/*
 * Fixture-scoped Android ELF import resolver.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 */
#include "androidwine_elf_imports.h"

#include "androidwine_native_window.h"

#include <errno.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CXX_ATEXIT_CAPACITY 64u

struct cxa_atexit_entry {
    void (*function)(void *);
    void *argument;
    void *dso_handle;
    uint64_t order;
    bool active;
};

static struct cxa_atexit_entry g_cxa_entries[CXX_ATEXIT_CAPACITY];
static uint64_t g_cxa_next_order;
static atomic_flag g_cxa_lock = ATOMIC_FLAG_INIT;

static void cxa_lock(void)
{
    while (atomic_flag_test_and_set_explicit(&g_cxa_lock, memory_order_acquire))
        ;
}

static void cxa_unlock(void)
{
    atomic_flag_clear_explicit(&g_cxa_lock, memory_order_release);
}

static int androidwine_runtime_cxa_atexit(void (*function)(void *),
                                         void *argument,
                                         void *dso_handle)
{
    if (function == NULL)
        return -1;
    cxa_lock();
    for (size_t i = 0; i < CXX_ATEXIT_CAPACITY; ++i) {
        if (!g_cxa_entries[i].active) {
            if (g_cxa_next_order == UINT64_MAX) {
                cxa_unlock();
                return -1;
            }
            g_cxa_entries[i].function = function;
            g_cxa_entries[i].argument = argument;
            g_cxa_entries[i].dso_handle = dso_handle;
            g_cxa_entries[i].order = ++g_cxa_next_order;
            g_cxa_entries[i].active = true;
            cxa_unlock();
            return 0;
        }
    }
    cxa_unlock();
    return -1;
}

static void androidwine_runtime_cxa_finalize(void *dso_handle)
{
    for (;;) {
        struct cxa_atexit_entry entry = {0};
        bool found = false;
        size_t newest_index = CXX_ATEXIT_CAPACITY;
        cxa_lock();
        for (size_t i = 0; i < CXX_ATEXIT_CAPACITY; ++i) {
            struct cxa_atexit_entry *candidate = &g_cxa_entries[i];
            if (candidate->active &&
                (dso_handle == NULL || candidate->dso_handle == dso_handle) &&
                (newest_index == CXX_ATEXIT_CAPACITY ||
                 candidate->order > g_cxa_entries[newest_index].order))
                newest_index = i;
        }
        if (newest_index != CXX_ATEXIT_CAPACITY) {
            entry = g_cxa_entries[newest_index];
            memset(&g_cxa_entries[newest_index], 0, sizeof(g_cxa_entries[newest_index]));
            found = true;
        }
        cxa_unlock();
        if (!found)
            return;
        entry.function(entry.argument);
    }
}

static int androidwine_runtime_register_atfork(void (*prepare)(void),
                                              void (*parent)(void),
                                              void (*child)(void),
                                              void *dso_handle)
{
    (void)prepare;
    (void)parent;
    (void)child;
    (void)dso_handle;
    /* Horizon has no fork semantics in this prototype. Do not silently claim it. */
    errno = ENOSYS;
    return -1;
}

static bool strings_equal(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

bool androidwine_elf_dependency_is_known(const char *soname)
{
    static const char *const allowlist[] = {
        "libandroid.so",
        "libvulkan.so",
        "libm.so",
        "libdl.so",
        "libc.so",
    };
    if (soname == NULL)
        return false;
    for (size_t i = 0; i < sizeof(allowlist) / sizeof(allowlist[0]); ++i) {
        if (strcmp(soname, allowlist[i]) == 0)
            return true;
    }
    return false;
}

static bool libc_version_matches(const char *version, const char *version_file)
{
    return strings_equal(version, "LIBC") && strings_equal(version_file, "libc.so");
}

#define ANDROIDWINE_SYMBOL(fn) ((androidwine_elf_symbol_fn)(fn))

static androidwine_elf_symbol_fn resolve_android_symbol(const char *symbol)
{
    if (strcmp(symbol, "ANativeWindow_acquire") == 0)
        return ANDROIDWINE_SYMBOL(ANativeWindow_acquire);
    if (strcmp(symbol, "ANativeWindow_release") == 0)
        return ANDROIDWINE_SYMBOL(ANativeWindow_release);
    if (strcmp(symbol, "ANativeWindow_getWidth") == 0)
        return ANDROIDWINE_SYMBOL(ANativeWindow_getWidth);
    if (strcmp(symbol, "ANativeWindow_getHeight") == 0)
        return ANDROIDWINE_SYMBOL(ANativeWindow_getHeight);
    if (strcmp(symbol, "ANativeWindow_getFormat") == 0)
        return ANDROIDWINE_SYMBOL(ANativeWindow_getFormat);
    if (strcmp(symbol, "ANativeWindow_setBuffersGeometry") == 0)
        return ANDROIDWINE_SYMBOL(ANativeWindow_setBuffersGeometry);
    return NULL;
}

androidwine_elf_symbol_fn androidwine_elf_resolve_import(
    const char *soname,
    const char *symbol,
    const char *version,
    const char *version_file,
    const struct androidwine_elf_import_context *context,
    enum androidwine_elf_import_status *status)
{
    if (status != NULL)
        *status = ANDROIDWINE_ELF_IMPORT_UNKNOWN_LIBRARY;
    if (!androidwine_elf_dependency_is_known(soname) || symbol == NULL)
        return NULL;

    androidwine_elf_symbol_fn result = NULL;
    if (strcmp(soname, "libandroid.so") == 0) {
        if (version != NULL || version_file != NULL) {
            if (status != NULL)
                *status = ANDROIDWINE_ELF_IMPORT_VERSION_MISMATCH;
            return NULL;
        }
        result = resolve_android_symbol(symbol);
    } else if (strcmp(soname, "libvulkan.so") == 0) {
        if (version != NULL || version_file != NULL) {
            if (status != NULL)
                *status = ANDROIDWINE_ELF_IMPORT_VERSION_MISMATCH;
            return NULL;
        }
        if (strcmp(symbol, "vkGetInstanceProcAddr") == 0) {
            if (context == NULL || context->vk_get_instance_proc_addr == NULL) {
                if (status != NULL)
                    *status = ANDROIDWINE_ELF_IMPORT_BACKEND_UNAVAILABLE;
                return NULL;
            }
            result = context->vk_get_instance_proc_addr;
        }
    } else if (strcmp(soname, "libc.so") == 0) {
        if (!libc_version_matches(version, version_file)) {
            if (status != NULL)
                *status = ANDROIDWINE_ELF_IMPORT_VERSION_MISMATCH;
            return NULL;
        }
        if (strcmp(symbol, "__cxa_atexit") == 0)
            result = ANDROIDWINE_SYMBOL(androidwine_runtime_cxa_atexit);
        else if (strcmp(symbol, "__cxa_finalize") == 0)
            result = ANDROIDWINE_SYMBOL(androidwine_runtime_cxa_finalize);
        else if (strcmp(symbol, "__register_atfork") == 0)
            result = ANDROIDWINE_SYMBOL(androidwine_runtime_register_atfork);
    }
    /* libm.so and libdl.so are allowlisted as empty modules for this fixture;
     * any import from either library is still rejected below. */

    if (result == NULL) {
        if (status != NULL)
            *status = ANDROIDWINE_ELF_IMPORT_UNKNOWN_SYMBOL;
        return NULL;
    }
    if (status != NULL)
        *status = ANDROIDWINE_ELF_IMPORT_RESOLVED;
    return result;
}
