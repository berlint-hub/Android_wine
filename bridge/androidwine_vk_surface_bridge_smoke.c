/*
 * AndroidWine's first Android-surface-to-Horizon adapter contract test.
 * Copyright (c) 2026 Berlint. MIT License; see the repository LICENSE.
 *
 * This test deliberately uses a synthetic opaque ANativeWindow token. It tests
 * the adapter's Android surface request -> VK_NN_vi_surface translation on the
 * Switch; it does not implement ANativeWindow, ART, JNI, or APK loading.
 */
#define VK_USE_PLATFORM_ANDROID_KHR
#include "nvk_harness.h"
#include "androidwine_vk_surface_adapter.h"
/* NXVK's one-source smoke build compiles the small adapter into this test NRO. */
#include "androidwine_vk_surface_adapter.c"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define BRIDGE_FRAMES 60u

int main(void)
{
    nvk_log_open("sdmc:/androidwine_surface_bridge.log");
    LOG("=== AndroidWine Android-surface -> VI bridge contract ===");
    LOG("scope=synthetic_window_token; no Android runtime or APK loader");

    const char *instance_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_NN_VI_SURFACE_EXTENSION_NAME,
    };
    const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    struct nvk_ctx context;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkSemaphore acquired = VK_NULL_HANDLE;
    VkSemaphore render_complete = VK_NULL_HANDLE;
    VkImage *images = NULL;
    uint32_t image_count = 0;
    struct androidwine_vk_surface_adapter adapter = {0};
    bool passed = false;
    uint32_t presented = 0;

    memset(&context, 0, sizeof(context));
    if (nvk_bringup_ex(&context, instance_extensions,
                       (uint32_t)(sizeof(instance_extensions) / sizeof(instance_extensions[0])),
                       device_extensions,
                       (uint32_t)(sizeof(device_extensions) / sizeof(device_extensions[0]))) != VK_SUCCESS) {
        LOG("FAIL: NXVK Vulkan bring-up");
        goto teardown;
    }

    LOAD_INST(&context, CreateViSurfaceNN);
    LOAD_INST(&context, DestroySurfaceKHR);
    LOAD_INST(&context, GetPhysicalDeviceSurfaceSupportKHR);
    LOAD_INST(&context, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_INST(&context, GetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_DEV(&context, CreateSwapchainKHR);
    LOAD_DEV(&context, DestroySwapchainKHR);
    LOAD_DEV(&context, GetSwapchainImagesKHR);
    LOAD_DEV(&context, AcquireNextImageKHR);
    LOAD_DEV(&context, QueuePresentKHR);
    LOAD_DEV(&context, CreateCommandPool);
    LOAD_DEV(&context, DestroyCommandPool);
    LOAD_DEV(&context, AllocateCommandBuffers);
    LOAD_DEV(&context, BeginCommandBuffer);
    LOAD_DEV(&context, EndCommandBuffer);
    LOAD_DEV(&context, ResetCommandBuffer);
    LOAD_DEV(&context, CmdPipelineBarrier);
    LOAD_DEV(&context, CmdClearColorImage);
    LOAD_DEV(&context, CreateSemaphore);
    LOAD_DEV(&context, DestroySemaphore);
    LOAD_DEV(&context, QueueSubmit);
    LOAD_DEV(&context, QueueWaitIdle);

    if (CreateViSurfaceNN == NULL || DestroySurfaceKHR == NULL ||
        GetPhysicalDeviceSurfaceSupportKHR == NULL ||
        GetPhysicalDeviceSurfaceCapabilitiesKHR == NULL ||
        GetPhysicalDeviceSurfaceFormatsKHR == NULL ||
        CreateSwapchainKHR == NULL || DestroySwapchainKHR == NULL ||
        GetSwapchainImagesKHR == NULL || AcquireNextImageKHR == NULL ||
        QueuePresentKHR == NULL || CreateCommandPool == NULL ||
        DestroyCommandPool == NULL || AllocateCommandBuffers == NULL ||
        BeginCommandBuffer == NULL || EndCommandBuffer == NULL ||
        ResetCommandBuffer == NULL || CmdPipelineBarrier == NULL ||
        CmdClearColorImage == NULL || CreateSemaphore == NULL ||
        DestroySemaphore == NULL || QueueSubmit == NULL || QueueWaitIdle == NULL) {
        LOG("FAIL: missing Vulkan entrypoint needed by the surface adapter test");
        goto cleanup;
    }

    /* ANativeWindow is opaque to Vulkan. This stable token stands in for the
     * object an Android runtime would create and register for an Activity. */
    union { uint64_t alignment; unsigned char token; } android_window_token;
    union { uint64_t alignment; unsigned char token; } unregistered_window_token;
    struct ANativeWindow *android_window =
        (struct ANativeWindow *)&android_window_token;
    struct ANativeWindow *unregistered_window =
        (struct ANativeWindow *)&unregistered_window_token;
    NWindow *horizon_window = nwindowGetDefault();
    if (horizon_window == NULL) {
        LOG("FAIL: nwindowGetDefault returned NULL");
        goto cleanup;
    }

    if (!androidwine_vk_surface_adapter_bind(&adapter, android_window,
                                              horizon_window, CreateViSurfaceNN)) {
        LOG("FAIL: adapter rejected valid Android/Horizon window binding");
        goto cleanup;
    }
    LOG("window_binding=registered opaque Android token -> NWindow");

    VkAndroidSurfaceCreateInfoKHR android_surface_info = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .window = unregistered_window,
    };
    VkSurfaceKHR rejected_surface = VK_NULL_HANDLE;
    VkResult result = androidwine_vkCreateAndroidSurfaceKHR(
        &adapter, context.instance, &android_surface_info, NULL, &rejected_surface);
    if (result != VK_ERROR_SURFACE_LOST_KHR || rejected_surface != VK_NULL_HANDLE) {
        LOG("FAIL: unregistered Android window must be rejected (result=%d)", result);
        goto cleanup;
    }
    LOG("unregistered_window=correctly_rejected");

    android_surface_info.window = android_window;
    android_surface_info.flags = 1;
    result = androidwine_vkCreateAndroidSurfaceKHR(
        &adapter, context.instance, &android_surface_info, NULL, &rejected_surface);
    if (result != VK_ERROR_INITIALIZATION_FAILED || rejected_surface != VK_NULL_HANDLE) {
        LOG("FAIL: unsupported Android surface flags must be rejected (result=%d)", result);
        goto cleanup;
    }
    android_surface_info.flags = 0;
    android_surface_info.pNext = &android_window_token;
    result = androidwine_vkCreateAndroidSurfaceKHR(
        &adapter, context.instance, &android_surface_info, NULL, &rejected_surface);
    if (result != VK_ERROR_FEATURE_NOT_PRESENT || rejected_surface != VK_NULL_HANDLE) {
        LOG("FAIL: unsupported Android pNext must be rejected (result=%d)", result);
        goto cleanup;
    }
    LOG("unsupported_flags_and_pnext=correctly_rejected");
    android_surface_info.pNext = NULL;

    result = androidwine_vkCreateAndroidSurfaceKHR(
        &adapter, context.instance, &android_surface_info, NULL, &surface);
    LOG("androidwine_vkCreateAndroidSurfaceKHR -> %d", result);
    if (result != VK_SUCCESS || surface == VK_NULL_HANDLE)
        goto cleanup;
    LOG("surface_translation=VK_KHR_android_surface request -> VK_NN_vi_surface");

    VkBool32 present_supported = VK_FALSE;
    result = GetPhysicalDeviceSurfaceSupportKHR(context.phys, context.qfi,
                                                surface, &present_supported);
    LOG("surface present support result=%d supported=%u", result, present_supported);
    if (result != VK_SUCCESS || present_supported != VK_TRUE)
        goto cleanup;

    VkSurfaceCapabilitiesKHR capabilities;
    result = GetPhysicalDeviceSurfaceCapabilitiesKHR(context.phys, surface, &capabilities);
    LOG("vkGetPhysicalDeviceSurfaceCapabilitiesKHR -> %d", result);
    if (result != VK_SUCCESS ||
        (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        goto cleanup;

    uint32_t format_count = 0;
    result = GetPhysicalDeviceSurfaceFormatsKHR(context.phys, surface, &format_count, NULL);
    if (result != VK_SUCCESS || format_count == 0) {
        LOG("FAIL: no VI surface formats (result=%d count=%u)", result, format_count);
        goto cleanup;
    }
    VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(*formats));
    if (formats == NULL) {
        LOG("FAIL: out of memory enumerating VI surface formats");
        goto cleanup;
    }
    result = GetPhysicalDeviceSurfaceFormatsKHR(context.phys, surface, &format_count, formats);
    if (result != VK_SUCCESS) {
        free(formats);
        goto cleanup;
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

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = 1280;
        extent.height = 720;
    }
    if (extent.width == 0 || extent.height == 0) {
        LOG("FAIL: invalid surface extent %ux%u", extent.width, extent.height);
        goto cleanup;
    }

    uint32_t requested_images = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount != 0 && requested_images > capabilities.maxImageCount)
        requested_images = capabilities.maxImageCount;

    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((capabilities.supportedCompositeAlpha & composite_alpha) == 0) {
        const VkCompositeAlphaFlagBitsKHR candidates[] = {
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        bool found = false;
        for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
            if ((capabilities.supportedCompositeAlpha & candidates[i]) != 0) {
                composite_alpha = candidates[i];
                found = true;
                break;
            }
        }
        if (!found) {
            LOG("FAIL: no supported composite alpha mode");
            goto cleanup;
        }
    }

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = requested_images,
        .imageFormat = selected_format.format,
        .imageColorSpace = selected_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = composite_alpha,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    result = CreateSwapchainKHR(context.dev, &swapchain_info, NULL, &swapchain);
    LOG("vkCreateSwapchainKHR -> %d extent=%ux%u", result, extent.width, extent.height);
    if (result != VK_SUCCESS)
        goto cleanup;

    result = GetSwapchainImagesKHR(context.dev, swapchain, &image_count, NULL);
    if (result != VK_SUCCESS || image_count == 0) {
        LOG("FAIL: vkGetSwapchainImagesKHR count query result=%d count=%u", result, image_count);
        goto cleanup;
    }
    images = calloc(image_count, sizeof(*images));
    if (images == NULL) {
        LOG("FAIL: out of memory allocating swapchain image list");
        goto cleanup;
    }
    result = GetSwapchainImagesKHR(context.dev, swapchain, &image_count, images);
    LOG("vkGetSwapchainImagesKHR -> %d count=%u", result, image_count);
    if (result != VK_SUCCESS)
        goto cleanup;

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.qfi,
    };
    result = CreateCommandPool(context.dev, &pool_info, NULL, &command_pool);
    if (result != VK_SUCCESS) {
        LOG("FAIL: vkCreateCommandPool -> %d", result);
        goto cleanup;
    }
    VkCommandBufferAllocateInfo command_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    result = AllocateCommandBuffers(context.dev, &command_info, &command_buffer);
    if (result != VK_SUCCESS) {
        LOG("FAIL: vkAllocateCommandBuffers -> %d", result);
        goto cleanup;
    }
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    result = CreateSemaphore(context.dev, &semaphore_info, NULL, &acquired);
    if (result != VK_SUCCESS) {
        LOG("FAIL: acquire semaphore creation -> %d", result);
        goto cleanup;
    }
    result = CreateSemaphore(context.dev, &semaphore_info, NULL, &render_complete);
    if (result != VK_SUCCESS) {
        LOG("FAIL: render semaphore creation -> %d", result);
        goto cleanup;
    }

    LOG("starting %u frame surface bridge present test", BRIDGE_FRAMES);
    for (uint32_t frame = 0; frame < BRIDGE_FRAMES && appletMainLoop(); ++frame) {
        uint32_t image_index = 0;
        result = AcquireNextImageKHR(context.dev, swapchain, UINT64_MAX,
                                     acquired, VK_NULL_HANDLE, &image_index);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            LOG("FAIL: vkAcquireNextImageKHR frame=%u -> %d", frame, result);
            goto cleanup;
        }
        result = ResetCommandBuffer(command_buffer, 0);
        if (result != VK_SUCCESS)
            goto cleanup;
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        result = BeginCommandBuffer(command_buffer, &begin_info);
        if (result != VK_SUCCESS)
            goto cleanup;

        VkImageSubresourceRange range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        VkImageMemoryBarrier to_transfer = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = images[image_index],
            .subresourceRange = range,
        };
        CmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                           0, NULL, 0, NULL, 1, &to_transfer);

        float t = (float)frame / 30.0f;
        VkClearColorValue color = { .float32 = {
            0.5f + 0.5f * sinf(t),
            0.5f + 0.5f * sinf(t + 2.094f),
            0.5f + 0.5f * sinf(t + 4.188f),
            1.0f,
        } };
        CmdClearColorImage(command_buffer, images[image_index],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

        VkImageMemoryBarrier to_present = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = images[image_index],
            .subresourceRange = range,
        };
        CmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                           0, NULL, 0, NULL, 1, &to_present);
        result = EndCommandBuffer(command_buffer);
        if (result != VK_SUCCESS)
            goto cleanup;

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &acquired,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_complete,
        };
        result = QueueSubmit(context.queue, 1, &submit_info, VK_NULL_HANDLE);
        if (result != VK_SUCCESS) {
            LOG("FAIL: vkQueueSubmit frame=%u -> %d", frame, result);
            goto cleanup;
        }
        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_complete,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index,
        };
        result = QueuePresentKHR(context.queue, &present_info);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            LOG("FAIL: vkQueuePresentKHR frame=%u -> %d", frame, result);
            goto cleanup;
        }
        result = QueueWaitIdle(context.queue);
        if (result != VK_SUCCESS) {
            LOG("FAIL: vkQueueWaitIdle frame=%u -> %d", frame, result);
            goto cleanup;
        }
        ++presented;
    }

    LOG("presented_frames=%u expected=%u", presented, BRIDGE_FRAMES);
    if (presented == BRIDGE_FRAMES) {
        LOG("=== ANDROIDWINE_SURFACE_BRIDGE_CONTRACT PASSED ===");
        passed = true;
    }

cleanup:
    if (!passed)
        LOG("=== ANDROIDWINE_SURFACE_BRIDGE_CONTRACT FAILED ===");
    if (QueueWaitIdle != NULL && context.queue != VK_NULL_HANDLE)
        (void)QueueWaitIdle(context.queue);
    if (acquired != VK_NULL_HANDLE)
        DestroySemaphore(context.dev, acquired, NULL);
    if (render_complete != VK_NULL_HANDLE)
        DestroySemaphore(context.dev, render_complete, NULL);
    if (command_pool != VK_NULL_HANDLE)
        DestroyCommandPool(context.dev, command_pool, NULL);
    if (swapchain != VK_NULL_HANDLE)
        DestroySwapchainKHR(context.dev, swapchain, NULL);
    if (surface != VK_NULL_HANDLE)
        DestroySurfaceKHR(context.instance, surface, NULL);
    free(images);
    androidwine_vk_surface_adapter_unbind(&adapter);

teardown:
    nvk_teardown(&context);
    if (g_nvk_log != NULL)
        fclose(g_nvk_log);
    return passed ? 0 : 1;
}
