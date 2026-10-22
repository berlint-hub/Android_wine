cmake_minimum_required(VERSION 3.22.1)
project(androidwine_loader_fixture C)

add_library(androidwine_loader_fixture SHARED native_import_fixture.c)
target_compile_features(androidwine_loader_fixture PRIVATE c_std_11)
target_compile_definitions(androidwine_loader_fixture PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
target_compile_options(androidwine_loader_fixture PRIVATE
    -Wall
    -Wextra
    -Werror
    -fvisibility=hidden
)
target_link_libraries(androidwine_loader_fixture PRIVATE android vulkan)
set_target_properties(androidwine_loader_fixture PROPERTIES
    OUTPUT_NAME androidwine_loader_fixture
    C_VISIBILITY_PRESET hidden
)
