/*
 * Fixture-scoped Android ELF import resolver.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 *
 * This is not a dynamic loader or a Bionic implementation. It only recognizes
 * the SONAMEs and imports observed in the self-authored loader fixture.
 */
#ifndef ANDROIDWINE_ELF_IMPORTS_H
#define ANDROIDWINE_ELF_IMPORTS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*androidwine_elf_symbol_fn)(void);

struct androidwine_elf_import_context {
    /* Must be an AndroidWine-owned Vulkan frontend, never raw NXVK GIPA. */
    androidwine_elf_symbol_fn vk_get_instance_proc_addr;
};

enum androidwine_elf_import_status {
    ANDROIDWINE_ELF_IMPORT_RESOLVED = 0,
    ANDROIDWINE_ELF_IMPORT_UNKNOWN_LIBRARY,
    ANDROIDWINE_ELF_IMPORT_UNKNOWN_SYMBOL,
    ANDROIDWINE_ELF_IMPORT_VERSION_MISMATCH,
    ANDROIDWINE_ELF_IMPORT_BACKEND_UNAVAILABLE,
};

/* True only for the fixture's explicit DT_NEEDED allowlist. */
bool androidwine_elf_dependency_is_known(const char *soname);

/*
 * Resolve one import from the fixture's exact SONAME/symbol/version allowlist.
 * An unresolved function returns NULL with a specific status; callers must
 * fail the module load rather than substitute an arbitrary host symbol.
 */
androidwine_elf_symbol_fn androidwine_elf_resolve_import(
    const char *soname,
    const char *symbol,
    const char *version,
    const char *version_file,
    const struct androidwine_elf_import_context *context,
    enum androidwine_elf_import_status *status);

#ifdef __cplusplus
}
#endif

#endif
