/*!
 * \author ddubois 
 * \date 15-Oct-17.
 */

#include <easylogging++.h>
#include <unordered_set>
#include "render_device.h"

namespace nova {
    render_device render_device::instance;

    bool layers_are_supported(std::vector<const char*>& validation_layers);

    std::vector<const char *> get_required_extensions(glfw_vk_window &window);

    VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
            vk::DebugReportFlagsEXT flags,
            vk::DebugReportObjectTypeEXT objType,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char *layerPrefix,
            const char *msg,
            void *userData);

    void render_device::create_instance(glfw_vk_window &window) {
        validation_layers = {
                "VK_LAYER_LUNARG_core_validation",
                "VK_LAYER_LUNARG_standard_validation"
        };

        vk::ApplicationInfo app_info = {};
        app_info.pApplicationName = "Minecraft Nova Renderer";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "Nova Renderer 0.5";
        app_info.engineVersion = VK_MAKE_VERSION(0, 5, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;
        LOG(TRACE) << "Created vk::ApplicationInfo struct";

        vk::InstanceCreateInfo create_info = {};
        create_info.pApplicationInfo = &app_info;

        extensions = get_required_extensions(window);
        create_info.ppEnabledExtensionNames = extensions.data();
        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());

#ifdef NDEBUG
        create_info.enabledLayerCount = 0;
#else
        if(!layers_are_supported(validation_layers)) {
            LOG(FATAL) << "The layers we need aren't available";
        }

        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
#endif

        vk_instance = vk::createInstance(create_info, nullptr);
    }

    void render_device::setup_debug_callback() {
#ifndef NDEBUG
        /*vk::DebugReportCallbackCreateInfoEXT create_info = {};
        create_info.flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning;
        create_info.pfnCallback = debug_callback;

        if(CreateDebugReportCallbackEXT(vk_instance, &create_info, nullptr, &callback) != vk::Result::eSuccess) {
            LOG(FATAL) << "Could not set up debug callback";
        }*/
#endif
    }

    void render_device::find_device_and_queues() {
        enumerate_gpus();
        LOG(TRACE) << "Enumerated GPUs";
        select_physical_device();
        LOG(TRACE) << "Found a physical device that will work I guess";
        create_logical_device_and_queues();
        LOG(TRACE) << "Basic queue and logical device was found";
    }

    void render_device::enumerate_gpus() {
        auto devices = vk_instance.enumeratePhysicalDevices();
        LOG(TRACE) << "There are " << devices.size() << " physical devices";
        if(devices.empty()) {
            LOG(FATAL) << "Apparently you have zero devices. You know you need a GPU to run Nova, right?";
        }

        this->gpus.resize(devices.size());
        LOG(TRACE) << "Reserved " << devices.size() << " slots for devices";
        for(uint32_t i = 0; i < devices.size(); i++) {
            gpu_info& gpu = this->gpus[i];
            gpu.device = devices[i];

            // get the queues the device supports
            gpu.queue_family_props = gpu.device.getQueueFamilyProperties();
            LOG(TRACE) << "Got the physical device queue properties";

            // Get the extensions the device supports
            gpu.extention_props = gpu.device.enumerateDeviceExtensionProperties();
            LOG(TRACE) << "Got the device extension properties";

            gpu.surface_capabilities = gpu.device.getSurfaceCapabilitiesKHR(surface);
            LOG(TRACE) << "Got the physical device surface capabilities";

            gpu.surface_formats = gpu.device.getSurfaceFormatsKHR(surface);
            LOG(TRACE) << "Got the physical device's surface formats";

            gpu.present_modes = gpu.device.getSurfacePresentModesKHR(surface);
            LOG(TRACE) << "Got the surface present modes";

            gpu.mem_props = gpu.device.getMemoryProperties();
            gpu.props = gpu.device.getProperties();
            gpu.supported_features = gpu.device.getFeatures();
            LOG(TRACE) << "Got the memory properties and deice properties";
        }
    }

    void render_device::select_physical_device() {
        // TODO: More complex logic to try and use a non-Intel GPU if possible (Vulkan book page 9)
        for(auto &gpu : gpus) {
            uint32_t graphics_idx = 0xFFFFFFFF;
            uint32_t present_idx = 0xFFFFFFFF;

            if(gpu.surface_formats.empty()) {
                continue;
            }

            if(gpu.present_modes.empty()) {
                continue;
            }

            // Find graphics queue family
            for(int i = 0; i < gpu.queue_family_props.size(); i++) {
                auto &props = gpu.queue_family_props[i];
                if(props.queueCount == 0) {
                    continue;
                }

                if(props.queueFlags & vk::QueueFlagBits::eGraphics) {
                    graphics_idx = i;
                    break;
                }
            }

            // Find present queue family
            for(int i = 0; i < gpu.queue_family_props.size(); i++) {
                auto &props = gpu.queue_family_props[i];

                if(props.queueCount == 0) {
                    continue;
                }

                vk::Bool32 supports_present = VK_FALSE;
                gpu.device.getSurfaceSupportKHR(i, surface, &supports_present);
                if(supports_present == VK_TRUE) {
                    present_idx = i;
                    break;
                }
            }

            if(graphics_idx >= 0 && present_idx >= 0) {
                graphics_family_idx = graphics_idx;
                present_family_idx = present_idx;
                physical_device = gpu.device;
                this->gpu = gpu;
                return;
            }
        }

        LOG(FATAL) << "Could not find a device with both present and graphics queues";
    }

    void render_device::create_logical_device_and_queues() {
        std::unordered_set<uint32_t> unique_idx;
        unique_idx.insert(graphics_family_idx);
        unique_idx.insert(present_family_idx);

        std::vector<vk::DeviceQueueCreateInfo> devq_info;

        // TODO: Possibly create a queue for texture streaming and another for geometry streaming?
        const float priority = 1.0;
        for(auto idx : unique_idx) {
            vk::DeviceQueueCreateInfo qinfo = {};
            qinfo.queueFamilyIndex = idx;
            qinfo.queueCount = 1;

            qinfo.pQueuePriorities = &priority;
            devq_info.push_back(qinfo);
        }

        // Do I have to look at the loaded shaderpack and see what features it needs? For now I'll just add whatever looks
        // good
        vk::PhysicalDeviceFeatures device_features = {};
        device_features.geometryShader = VK_TRUE;
        device_features.tessellationShader = VK_TRUE;
        device_features.samplerAnisotropy = VK_TRUE;

        vk::DeviceCreateInfo info = {};
        info.queueCreateInfoCount = static_cast<uint32_t>(devq_info.size());
        info.pQueueCreateInfos = devq_info.data();
        info.pEnabledFeatures = &device_features;
        info.enabledExtensionCount = 1;
        const char* swapchain_extension = "VK_KHR_swapchain";
        info.ppEnabledExtensionNames = &swapchain_extension;

        info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        if(!validation_layers.empty()) {
            info.ppEnabledLayerNames = validation_layers.data();
        }

        device = physical_device.createDevice(info, nullptr);

        graphics_queue = device.getQueue(graphics_family_idx, 0);
        present_queue = device.getQueue(graphics_family_idx, 0);
    }

    void render_device::create_semaphores() {
        acquire_semaphores.resize(NUM_FRAME_DATA);
        render_complete_semaphores.resize(NUM_FRAME_DATA);

        vk::SemaphoreCreateInfo semaphore_create_info = {};
        for(int i = 0; i < NUM_FRAME_DATA; i++) {
            acquire_semaphores[i] = device.createSemaphore(semaphore_create_info, nullptr);
            render_complete_semaphores[i] = device.createSemaphore(semaphore_create_info, nullptr);
        }
    }

    void render_device::create_command_pool_and_command_buffers() {
        // TODO: Get the number of threads dynamically based on the user's CPU core count
        command_buffer_pool = std::make_unique<command_pool>(device, graphics_family_idx, 8);
    }

    void render_device::create_swapchain(glm::ivec2 window_dimensions) {
        auto& device = render_device::instance.device;

        auto surface_format = choose_surface_format(gpu.surface_formats);
        auto present_mode = choose_present_mode(gpu.present_modes);
        auto extent = choose_surface_extent(gpu.surface_capabilities, window_dimensions);

        vk::SwapchainCreateInfoKHR info = {};
        info.surface = render_device::instance.surface;

        info.minImageCount = NUM_FRAME_DATA;

        info.imageFormat = surface_format.format;
        info.imageColorSpace = surface_format.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;

        info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;

        if(render_device::instance.graphics_family_idx not_eq render_device::instance.present_family_idx) {
            // If the indices are different then we need to share the images
            uint32_t indices[] = {render_device::instance.graphics_family_idx, render_device::instance.present_family_idx};

            info.imageSharingMode = vk::SharingMode::eConcurrent;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = indices;
        } else {
            // If the indices are the same, we can have exclusive access
            info.imageSharingMode = vk::SharingMode::eExclusive;
        }

        info.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
        info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        info.presentMode = present_mode;

        info.clipped = VK_TRUE;

        swapchain = device.createSwapchainKHR(info);

        swapchain_format = surface_format.format;
        this->present_mode = present_mode;
        swapchain_extent = extent;

        std::vector<vk::Image> swapchain_images = device.getSwapchainImagesKHR(swapchain);
        if(swapchain_images.empty()) {
            LOG(FATAL) << "The swapchain returned zero images";
        }

        // More than 255 images in the swapchain? Good lord what are you doing? and will you please stop?
        for(uint8_t i = 0; i < NUM_FRAME_DATA; i++) {
            vk::ImageViewCreateInfo image_view_create_info = {};

            image_view_create_info.image = swapchain_images[i];
            image_view_create_info.viewType = vk::ImageViewType::e2D;
            image_view_create_info.format = swapchain_format;

            image_view_create_info.components.r = vk::ComponentSwizzle::eR;
            image_view_create_info.components.g = vk::ComponentSwizzle::eG;
            image_view_create_info.components.b = vk::ComponentSwizzle::eB;
            image_view_create_info.components.a = vk::ComponentSwizzle::eA;

            image_view_create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            image_view_create_info.subresourceRange.baseMipLevel = 0;
            image_view_create_info.subresourceRange.levelCount = 1;
            image_view_create_info.subresourceRange.baseArrayLayer = 0;
            image_view_create_info.subresourceRange.layerCount = 1;

            vk::ImageView image_view = device.createImageView(image_view_create_info);

            this->swapchain_images.push_back(image_view);
        }

        // This block just kinda checks that thhe depth bufer we want is available
        {
            vk::Format formats[] = {
                    vk::Format::eD32SfloatS8Uint,
                    vk::Format::eD24UnormS8Uint
            };
            depth_format = choose_supported_format(formats, 2, vk::ImageTiling::eOptimal,
                                                   vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        }
    }

    vk::SurfaceFormatKHR render_device::choose_surface_format(std::vector<vk::SurfaceFormatKHR> &formats) {
        vk::SurfaceFormatKHR result = {};

        if(formats.size() == 1 and formats[0].format == vk::Format::eUndefined) {
            result.format = vk::Format::eB8G8R8A8Unorm;
            result.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
            return result;
        }

        // We want 32 bit rgba and srgb nonlinear... I think? Will have to read up on it more and figure out what's up
        for(auto& fmt : formats) {
            if(fmt.format == vk::Format::eB8G8R8A8Unorm and fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return fmt;
            }
        }

        // We can't have what we want, so I guess we'll just use what we got
        return formats[0];
    }

    vk::PresentModeKHR render_device::choose_present_mode(std::vector<vk::PresentModeKHR> &modes) {
        const vk::PresentModeKHR desired_mode = vk::PresentModeKHR::eMailbox;

        // Mailbox mode is best mode (also not sure why)
        for(auto& mode : modes) {
            if(mode == desired_mode) {
                return desired_mode;
            }
        }

        // FIFO, like FIFA, is forever
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D render_device::choose_surface_extent(vk::SurfaceCapabilitiesKHR &caps, glm::ivec2& window_dimensions) {
        vk::Extent2D extent;

        if(caps.currentExtent.width == -1) {
            extent.width = static_cast<uint32_t>(window_dimensions.x);
            extent.height = static_cast<uint32_t>(window_dimensions.y);
        } else {
            extent = caps.currentExtent;
        }

        return extent;
    }

    vk::Format render_device::choose_supported_format(vk::Format *formats, int num_formats, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
        for(int i = 0; i < num_formats; i++) {
            vk::Format& format = formats[i];

            vk::FormatProperties props = physical_device.getFormatProperties(format);

            if(tiling == vk::ImageTiling::eLinear and (props.linearTilingFeatures & features) == features) {
                return format;
            } else if(tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        LOG(FATAL) << "Failed to fine a suitable depth buffer format";
    }

    // This function should really be outside of this file, but I want to keep vulkan creation things in here
    // to avoid making nova_renderer.cpp any larger than it needs to be
    std::vector<const char *> get_required_extensions(glfw_vk_window &window) {
        uint32_t glfw_extensions_count = 0;
        auto glfw_extensions = window.get_required_extensions(&glfw_extensions_count);

        std::vector<const char *> extensions;

        for(uint32_t i = 0; i < glfw_extensions_count; i++) {
            LOG(DEBUG) << "GLFW requires " << glfw_extensions[i];
            extensions.push_back(glfw_extensions[i]);
        }

#ifndef NDEBUG
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
        //extensions.push_back("VK_KHR_swapchain");

        return extensions;
    }

    bool layers_are_supported(std::vector<const char*>& validation_layers) {
        auto available_layers = vk::enumerateInstanceLayerProperties();

        for(auto layer_name : validation_layers) {
            LOG(TRACE) << "Checking for layer " << layer_name;

            bool layer_found = false;

            for(const auto &layer_propeties : available_layers) {
                if(strcmp(layer_name, layer_propeties.layerName) == 0) {
                    LOG(TRACE) << "Found it!";
                    layer_found = true;
                    break;
                }
            }

            if(!layer_found) {
                LOG(ERROR) << "Could not find layer " << layer_name;
                return false;
            }
        }

        return true;
    }

    VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
            vk::DebugReportFlagsEXT flags,
            vk::DebugReportObjectTypeEXT objType,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char *layerPrefix,
            const char *msg,
            void *userData) {

        LOG(INFO) << "validation layer: " << msg;

        return VK_FALSE;
    }
}