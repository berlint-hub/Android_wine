#include "androidwine_elf_imports.h"
#include "androidwine_native_window.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

static int g_finalizer_order[4];
static size_t g_finalizer_count;
static int g_first_dso;
static int g_second_dso;
static int g_first_argument;
static int g_second_argument;
static int g_reused_slot_argument;

static void fake_android_vk_get_instance_proc_addr(void)
{
}

static void first_finalizer(void *argument)
{
    assert(argument == &g_first_argument);
    g_finalizer_order[g_finalizer_count++] = 1;
}

static void second_finalizer(void *argument)
{
    assert(argument == &g_second_argument);
    g_finalizer_order[g_finalizer_count++] = 2;
}

static void reused_slot_finalizer(void *argument)
{
    assert(argument == &g_reused_slot_argument);
    g_finalizer_order[g_finalizer_count++] = 3;
}

static androidwine_elf_symbol_fn resolve(const char *soname,
                                         const char *symbol,
                                         const char *version,
                                         const char *version_file,
                                         const struct androidwine_elf_import_context *context,
                                         enum androidwine_elf_import_status *status)
{
    return androidwine_elf_resolve_import(soname, symbol, version, version_file,
                                          context, status);
}

int main(void)
{
    enum androidwine_elf_import_status status;
    struct androidwine_elf_import_context context = {0};
    assert(androidwine_elf_dependency_is_known("libandroid.so"));
    assert(androidwine_elf_dependency_is_known("libvulkan.so"));
    assert(androidwine_elf_dependency_is_known("libm.so"));
    assert(androidwine_elf_dependency_is_known("libdl.so"));
    assert(androidwine_elf_dependency_is_known("libc.so"));
    assert(!androidwine_elf_dependency_is_known("/system/lib64/libc.so"));
    assert(!androidwine_elf_dependency_is_known("libart.so"));

    static const char *const window_symbols[] = {
        "ANativeWindow_acquire",
        "ANativeWindow_release",
        "ANativeWindow_getWidth",
        "ANativeWindow_getHeight",
        "ANativeWindow_getFormat",
        "ANativeWindow_setBuffersGeometry",
    };
    for (size_t i = 0; i < sizeof(window_symbols) / sizeof(window_symbols[0]); ++i) {
        assert(resolve("libandroid.so", window_symbols[i], NULL, NULL,
                       &context, &status) != NULL);
        assert(status == ANDROIDWINE_ELF_IMPORT_RESOLVED);
    }
    assert(resolve("libandroid.so", "ANativeWindow_fromSurface", NULL, NULL,
                   &context, &status) == NULL);
    assert(status == ANDROIDWINE_ELF_IMPORT_UNKNOWN_SYMBOL);
    assert(resolve("libandroid.so", "ANativeWindow_getWidth", "LIBC", "libc.so",
                   &context, &status) == NULL);
    assert(status == ANDROIDWINE_ELF_IMPORT_VERSION_MISMATCH);

    assert(resolve("libm.so", "sin", NULL, NULL, &context, &status) == NULL);
    assert(status == ANDROIDWINE_ELF_IMPORT_UNKNOWN_SYMBOL);
    assert(resolve("libc.so", "memcpy", NULL, NULL, &context, &status) == NULL);
    assert(status == ANDROIDWINE_ELF_IMPORT_VERSION_MISMATCH);

    assert(resolve("libc.so", "__cxa_atexit", "GLIBC", "libc.so",
                   &context, &status) == NULL);
    assert(status == ANDROIDWINE_ELF_IMPORT_VERSION_MISMATCH);
    androidwine_elf_symbol_fn atexit_symbol = resolve(
        "libc.so", "__cxa_atexit", "LIBC", "libc.so", &context, &status);
    assert(atexit_symbol != NULL && status == ANDROIDWINE_ELF_IMPORT_RESOLVED);
    androidwine_elf_symbol_fn finalize_symbol = resolve(
        "libc.so", "__cxa_finalize", "LIBC", "libc.so", &context, &status);
    assert(finalize_symbol != NULL && status == ANDROIDWINE_ELF_IMPORT_RESOLVED);
    androidwine_elf_symbol_fn atfork_symbol = resolve(
        "libc.so", "__register_atfork", "LIBC", "libc.so", &context, &status);
    assert(atfork_symbol != NULL && status == ANDROIDWINE_ELF_IMPORT_RESOLVED);

    union {
        androidwine_elf_symbol_fn generic;
        int (*typed)(void (*)(void *), void *, void *);
    } atexit_fn = { .generic = atexit_symbol };
    union {
        androidwine_elf_symbol_fn generic;
        void (*typed)(void *);
    } finalize_fn = { .generic = finalize_symbol };
    union {
        androidwine_elf_symbol_fn generic;
        int (*typed)(void (*)(void), void (*)(void), void (*)(void), void *);
    } atfork_fn = { .generic = atfork_symbol };

    assert(atexit_fn.typed(first_finalizer, &g_first_argument, &g_first_dso) == 0);
    assert(atexit_fn.typed(second_finalizer, &g_second_argument, &g_second_dso) == 0);
    finalize_fn.typed(&g_first_dso);
    assert(g_finalizer_count == 1 && g_finalizer_order[0] == 1);
    /* Reuse the freed slot; finalizers must still run in reverse registration order. */
    assert(atexit_fn.typed(reused_slot_finalizer, &g_reused_slot_argument,
                           &g_first_dso) == 0);
    finalize_fn.typed(NULL);
    assert(g_finalizer_count == 3 && g_finalizer_order[1] == 3 &&
           g_finalizer_order[2] == 2);
    finalize_fn.typed(NULL);
    assert(g_finalizer_count == 3);

    errno = 0;
    assert(atfork_fn.typed(NULL, NULL, NULL, NULL) == -1);
    assert(errno == ENOSYS);

    assert(resolve("libvulkan.so", "vkGetInstanceProcAddr", NULL, NULL,
                   &context, &status) == NULL);
    assert(status == ANDROIDWINE_ELF_IMPORT_BACKEND_UNAVAILABLE);
    context.vk_get_instance_proc_addr =
        (androidwine_elf_symbol_fn)fake_android_vk_get_instance_proc_addr;
    assert(resolve("libvulkan.so", "vkGetInstanceProcAddr", NULL, NULL,
                   &context, &status) == context.vk_get_instance_proc_addr);
    assert(status == ANDROIDWINE_ELF_IMPORT_RESOLVED);
    assert(resolve("libvulkan.so", "vkCreateAndroidSurfaceKHR", NULL, NULL,
                   &context, &status) == NULL);
    assert(status == ANDROIDWINE_ELF_IMPORT_UNKNOWN_SYMBOL);
    assert(resolve("libbinder.so", "unknown", NULL, NULL,
                   &context, &status) == NULL);
    assert(status == ANDROIDWINE_ELF_IMPORT_UNKNOWN_LIBRARY);

    puts("fixture import resolver tests passed");
    return 0;
}
