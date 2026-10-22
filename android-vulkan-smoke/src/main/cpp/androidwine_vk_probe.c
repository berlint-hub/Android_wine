#define VK_USE_PLATFORM_ANDROID_KHR

#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <vulkan/vulkan.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOG_TAG "AndroidWineVkSmoke"
#define FRAME_COUNT 180u
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct vk_state {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties properties;
    uint32_t queue_family;
    VkDevice device;
    VkQueue queue;
    VkSwapchainKHR swapchain;
    VkImage *images;
    uint32_t image_count;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore image_acquired;
    VkSemaphore render_complete;
    VkExtent2D extent;
    VkFormat format;
};

struct app_state {
    struct android_app *app;
    struct vk_state vk;
    FILE *log_file;
    bool window_ready;
    bool needs_init;
    bool initialized;
    bool failed;
    bool finished;
    bool quit;
    bool touch_seen;
    uint32_t frame;
};

static struct app_state *g_state;

static void log_message(const char *format, ...)
{
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", message);
    if (g_state != NULL && g_state->log_file != NULL) {
        fprintf(g_state->log_file, "%s\n", message);
        fflush(g_state->log_file);
    }
}

static bool has_instance_extension(const char *name)
{
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, NULL) != VK_SUCCESS)
        return false;

    VkExtensionProperties *extensions = calloc(count, sizeof(*extensions));
    if (extensions == NULL)
        return false;

    bool found = false;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, extensions) == VK_SUCCESS) {
        for (uint32_t i = 0; i < count; ++i) {
            if (strcmp(extensions[i].extensionName, name) == 0) {
                found = true;
                break;
            }
        }
    }
    free(extensions);
    return found;
}

static bool device_has_swapchain(VkPhysicalDevice device)
{
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL) != VK_SUCCESS)
        return false;

    VkExtensionProperties *extensions = calloc(count, sizeof(*extensions));
    if (extensions == NULL)
        return false;

    bool found = false;
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions) == VK_SUCCESS) {
        for (uint32_t i = 0; i < count; ++i) {
            if (strcmp(extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                found = true;
                break;
            }
        }
    }
    free(extensions);
    return found;
}

static bool choose_physical_device(struct vk_state *vk)
{
    uint32_t device_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(vk->instance, &device_count, NULL);
    log_message("vkEnumeratePhysicalDevices -> %d (count=%u)", result, device_count);
    if (result != VK_SUCCESS || device_count == 0)
        return false;

    VkPhysicalDevice *devices = calloc(device_count, sizeof(*devices));
    if (devices == NULL)
        return false;
    result = vkEnumeratePhysicalDevices(vk->instance, &device_count, devices);
    if (result != VK_SUCCESS) {
        free(devices);
        return false;
    }

    bool selected = false;
    for (uint32_t d = 0; d < device_count && !selected; ++d) {
        if (!device_has_swapchain(devices[d]))
            continue;

        uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &queue_count, NULL);
        VkQueueFamilyProperties *queues = calloc(queue_count, sizeof(*queues));
        if (queues == NULL)
            continue;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &queue_count, queues);

        for (uint32_t q = 0; q < queue_count; ++q) {
            VkBool32 present_supported = VK_FALSE;
            VkResult support_result = vkGetPhysicalDeviceSurfaceSupportKHR(
                devices[d], q, vk->surface, &present_supported);
            if (support_result == VK_SUCCESS && present_supported &&
                (queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                vk->physical_device = devices[d];
                vk->queue_family = q;
                selected = true;
                break;
            }
        }
        free(queues);
    }
    free(devices);

    if (selected) {
        vkGetPhysicalDeviceProperties(vk->physical_device, &vk->properties);
        log_message("physical_device=%s api=%u.%u.%u vendor=0x%04x device=0x%04x",
                    vk->properties.deviceName,
                    VK_VERSION_MAJOR(vk->properties.apiVersion),
                    VK_VERSION_MINOR(vk->properties.apiVersion),
                    VK_VERSION_PATCH(vk->properties.apiVersion),
                    vk->properties.vendorID, vk->properties.deviceID);
    }
    return selected;
}

static void destroy_vulkan(struct vk_state *vk)
{
    if (vk->device != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(vk->device);
        if (vk->image_acquired != VK_NULL_HANDLE)
            vkDestroySemaphore(vk->device, vk->image_acquired, NULL);
        if (vk->render_complete != VK_NULL_HANDLE)
            vkDestroySemaphore(vk->device, vk->render_complete, NULL);
        if (vk->command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk->device, vk->command_pool, NULL);
        if (vk->swapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(vk->device, vk->swapchain, NULL);
        vkDestroyDevice(vk->device, NULL);
    }
    free(vk->images);
    if (vk->surface != VK_NULL_HANDLE && vk->instance != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(vk->instance, vk->surface, NULL);
    if (vk->instance != VK_NULL_HANDLE)
        vkDestroyInstance(vk->instance, NULL);
    memset(vk, 0, sizeof(*vk));
}

static bool initialize_vulkan(struct app_state *state)
{
    struct vk_state *vk = &state->vk;
    memset(vk, 0, sizeof(*vk));

    const char *required_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };
    for (size_t i = 0; i < ARRAY_SIZE(required_extensions); ++i) {
        if (!has_instance_extension(required_extensions[i])) {
            log_message("FAIL: missing instance extension %s", required_extensions[i]);
            return false;
        }
        log_message("instance_extension=%s available", required_extensions[i]);
    }

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AndroidWine Android Vulkan Probe",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AndroidWine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = (uint32_t)ARRAY_SIZE(required_extensions),
        .ppEnabledExtensionNames = required_extensions,
    };
    VkResult result = vkCreateInstance(&instance_info, NULL, &vk->instance);
    log_message("vkCreateInstance -> %d", result);
    if (result != VK_SUCCESS)
        goto fail;

    VkAndroidSurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = state->app->window,
    };
    result = vkCreateAndroidSurfaceKHR(vk->instance, &surface_info, NULL, &vk->surface);
    log_message("vkCreateAndroidSurfaceKHR -> %d", result);
    if (result != VK_SUCCESS)
        goto fail;

    if (!choose_physical_device(vk)) {
        log_message("FAIL: no graphics queue with Android-surface present support and swapchain extension");
        goto fail;
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk->queue_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = (uint32_t)ARRAY_SIZE(device_extensions),
        .ppEnabledExtensionNames = device_extensions,
    };
    result = vkCreateDevice(vk->physical_device, &device_info, NULL, &vk->device);
    log_message("vkCreateDevice -> %d", result);
    if (result != VK_SUCCESS)
        goto fail;
    vkGetDeviceQueue(vk->device, vk->queue_family, 0, &vk->queue);

    VkSurfaceCapabilitiesKHR capabilities;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_device, vk->surface, &capabilities);
    log_message("vkGetPhysicalDeviceSurfaceCapabilitiesKHR -> %d", result);
    if (result != VK_SUCCESS)
        goto fail;
    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
        log_message("FAIL: Android surface does not support transfer-destination swapchain images (usage=0x%x)",
                    capabilities.supportedUsageFlags);
        goto fail;
    }

    uint32_t format_count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface,
                                                  &format_count, NULL);
    if (result != VK_SUCCESS || format_count == 0) {
        log_message("FAIL: no Android surface formats (result=%d count=%u)", result, format_count);
        goto fail;
    }
    VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(*formats));
    if (formats == NULL)
        goto fail;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface,
                                                  &format_count, formats);
    if (result != VK_SUCCESS) {
        free(formats);
        goto fail;
    }

    VkSurfaceFormatKHR selected_format = formats[0];
    if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
        selected_format.format = VK_FORMAT_R8G8B8A8_UNORM;
    for (uint32_t i = 0; i < format_count; ++i) {
        if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
            formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            selected_format = formats[i];
            break;
        }
    }
    free(formats);
    vk->format = selected_format.format;

    if (capabilities.currentExtent.width != UINT32_MAX) {
        vk->extent = capabilities.currentExtent;
    } else {
        vk->extent.width = (uint32_t)ANativeWindow_getWidth(state->app->window);
        vk->extent.height = (uint32_t)ANativeWindow_getHeight(state->app->window);
    }
    if (vk->extent.width == 0 || vk->extent.height == 0)
        goto fail;

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount != 0 && image_count > capabilities.maxImageCount)
        image_count = capabilities.maxImageCount;
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((capabilities.supportedCompositeAlpha & composite_alpha) == 0) {
        const VkCompositeAlphaFlagBitsKHR choices[] = {
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        bool found = false;
        for (size_t i = 0; i < ARRAY_SIZE(choices); ++i) {
            if ((capabilities.supportedCompositeAlpha & choices[i]) != 0) {
                composite_alpha = choices[i];
                found = true;
                break;
            }
        }
        if (!found)
            goto fail;
    }

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk->surface,
        .minImageCount = image_count,
        .imageFormat = selected_format.format,
        .imageColorSpace = selected_format.colorSpace,
        .imageExtent = vk->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = composite_alpha,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    result = vkCreateSwapchainKHR(vk->device, &swapchain_info, NULL, &vk->swapchain);
    log_message("vkCreateSwapchainKHR -> %d extent=%ux%u format=%d", result,
                vk->extent.width, vk->extent.height, vk->format);
    if (result != VK_SUCCESS)
        goto fail;

    result = vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->image_count, NULL);
    if (result != VK_SUCCESS || vk->image_count == 0)
        goto fail;
    vk->images = calloc(vk->image_count, sizeof(*vk->images));
    if (vk->images == NULL)
        goto fail;
    result = vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->image_count, vk->images);
    log_message("vkGetSwapchainImagesKHR -> %d count=%u", result, vk->image_count);
    if (result != VK_SUCCESS)
        goto fail;

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk->queue_family,
    };
    result = vkCreateCommandPool(vk->device, &pool_info, NULL, &vk->command_pool);
    if (result != VK_SUCCESS)
        goto fail;

    VkCommandBufferAllocateInfo command_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    result = vkAllocateCommandBuffers(vk->device, &command_info, &vk->command_buffer);
    if (result != VK_SUCCESS)
        goto fail;

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    result = vkCreateSemaphore(vk->device, &semaphore_info, NULL, &vk->image_acquired);
    if (result != VK_SUCCESS)
        goto fail;
    result = vkCreateSemaphore(vk->device, &semaphore_info, NULL, &vk->render_complete);
    if (result != VK_SUCCESS)
        goto fail;

    log_message("ANDROID_VK_INIT PASSED: swapchain_images=%u", vk->image_count);
    return true;

fail:
    log_message("FAIL: Android Vulkan initialization stopped at VkResult=%d", result);
    destroy_vulkan(vk);
    return false;
}

static bool present_frame(struct app_state *state, uint32_t frame)
{
    struct vk_state *vk = &state->vk;
    uint32_t image_index = 0;
    VkResult result = vkAcquireNextImageKHR(vk->device, vk->swapchain, UINT64_MAX,
                                            vk->image_acquired, VK_NULL_HANDLE, &image_index);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        log_message("vkAcquireNextImageKHR frame=%u -> %d", frame, result);
        return false;
    }

    result = vkResetCommandBuffer(vk->command_buffer, 0);
    if (result != VK_SUCCESS)
        return false;
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    result = vkBeginCommandBuffer(vk->command_buffer, &begin_info);
    if (result != VK_SUCCESS)
        return false;

    VkImageMemoryBarrier to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vk->images[image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(vk->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, NULL, 0, NULL, 1, &to_transfer);

    float time_value = (float)frame / 30.0f;
    VkClearColorValue color = { .float32 = {
        0.5f + 0.5f * sinf(time_value),
        0.5f + 0.5f * sinf(time_value + 2.094f),
        0.5f + 0.5f * sinf(time_value + 4.188f),
        1.0f,
    } };
    if (state->finished)
        color = (VkClearColorValue){ .float32 = { 0.05f, 0.8f, 0.2f, 1.0f } };
    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    vkCmdClearColorImage(vk->command_buffer, vk->images[image_index],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

    VkImageMemoryBarrier to_present = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vk->images[image_index],
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(vk->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                         0, NULL, 0, NULL, 1, &to_present);
    result = vkEndCommandBuffer(vk->command_buffer);
    if (result != VK_SUCCESS)
        return false;

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk->image_acquired,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk->command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vk->render_complete,
    };
    result = vkQueueSubmit(vk->queue, 1, &submit_info, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        log_message("vkQueueSubmit frame=%u -> %d", frame, result);
        return false;
    }

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk->render_complete,
        .swapchainCount = 1,
        .pSwapchains = &vk->swapchain,
        .pImageIndices = &image_index,
    };
    result = vkQueuePresentKHR(vk->queue, &present_info);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        log_message("vkQueuePresentKHR frame=%u -> %d", frame, result);
        return false;
    }
    result = vkQueueWaitIdle(vk->queue);
    if (result != VK_SUCCESS) {
        log_message("vkQueueWaitIdle frame=%u -> %d", frame, result);
        return false;
    }
    return true;
}

static int32_t handle_input(struct android_app *app, AInputEvent *event)
{
    struct app_state *state = app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY &&
        AKeyEvent_getKeyCode(event) == AKEYCODE_BACK &&
        AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN) {
        state->quit = true;
        return 1;
    }
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION &&
        AMotionEvent_getAction(event) == AMOTION_EVENT_ACTION_DOWN) {
        state->touch_seen = true;
        state->quit = true;
        log_message("touch_input=detected; exiting");
        return 1;
    }
    return 0;
}

static void handle_command(struct android_app *app, int32_t command)
{
    struct app_state *state = app->userData;
    switch (command) {
    case APP_CMD_INIT_WINDOW:
        state->window_ready = true;
        state->needs_init = true;
        state->finished = false;
        state->frame = 0;
        log_message("lifecycle=APP_CMD_INIT_WINDOW");
        break;
    case APP_CMD_TERM_WINDOW:
        log_message("lifecycle=APP_CMD_TERM_WINDOW");
        state->window_ready = false;
        state->initialized = false;
        destroy_vulkan(&state->vk);
        break;
    case APP_CMD_GAINED_FOCUS:
        log_message("lifecycle=APP_CMD_GAINED_FOCUS");
        break;
    case APP_CMD_LOST_FOCUS:
        log_message("lifecycle=APP_CMD_LOST_FOCUS");
        break;
    case APP_CMD_PAUSE:
        log_message("lifecycle=APP_CMD_PAUSE");
        break;
    case APP_CMD_RESUME:
        log_message("lifecycle=APP_CMD_RESUME");
        break;
    default:
        break;
    }
}

void android_main(struct android_app *app)
{
    app_dummy();
    struct app_state state;
    memset(&state, 0, sizeof(state));
    state.app = app;
    g_state = &state;

    char log_path[1024];
    snprintf(log_path, sizeof(log_path), "%s/vulkan-smoke.log", app->activity->internalDataPath);
    state.log_file = fopen(log_path, "w");
    log_message("=== AndroidWine Android Vulkan Probe boot ===");
    log_message("package_log_path=%s", log_path);
    log_message("android_native_api=%d", app->activity->sdkVersion);

    app->userData = &state;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;

    while (!app->destroyRequested && !state.quit) {
        int events = 0;
        struct android_poll_source *source = NULL;
        int timeout = (state.needs_init || (state.initialized && !state.finished)) ? 0 : -1;
        while (ALooper_pollOnce(timeout, NULL, &events, (void **)&source) >= 0) {
            if (source != NULL)
                source->process(app, source);
            if (app->destroyRequested || state.quit)
                break;
            timeout = (state.needs_init || (state.initialized && !state.finished)) ? 0 : -1;
        }

        if (app->destroyRequested || state.quit)
            break;
        if (state.window_ready && state.needs_init) {
            state.needs_init = false;
            state.initialized = initialize_vulkan(&state);
            state.failed = !state.initialized;
            if (state.failed)
                log_message("=== ANDROID_VK_PROBE FAILED ===");
        }
        if (state.initialized && !state.finished) {
            if (!present_frame(&state, state.frame)) {
                state.failed = true;
                state.finished = true;
                log_message("=== ANDROID_VK_PROBE FAILED during present ===");
            } else {
                state.frame++;
                if (state.frame == FRAME_COUNT) {
                    state.finished = true;
                    log_message("presented_frames=%u", state.frame);
                    log_message("=== ANDROID_VK_PROBE PASSED ===");
                    log_message("Touch screen or press Back to exit; final green frame indicates pass.");
                    (void)present_frame(&state, state.frame);
                }
            }
            usleep(16000);
        }
    }

    log_message("=== AndroidWine Android Vulkan Probe exit ===");
    destroy_vulkan(&state.vk);
    if (state.log_file != NULL)
        fclose(state.log_file);
    g_state = NULL;
}
