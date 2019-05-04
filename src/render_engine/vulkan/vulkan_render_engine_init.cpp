#include <set>

#include <fmt/format.h>

#include "../../util/logger.hpp"
#include "swapchain.hpp"
#include "vulkan.hpp"
#include "vulkan_render_engine.hpp"
#include "vulkan_utils.hpp"

namespace nova::renderer {
    vulkan_render_engine::vulkan_render_engine(nova_settings& settings, RENDERDOC_API_1_3_0* renderdoc)
        : render_engine(settings), renderdoc(renderdoc) {
        NOVA_LOG(INFO) << "Initializing Vulkan rendering";

        validate_mesh_options(settings.vertex_memory_settings);

        const auto& version = settings.vulkan.application_version;

        VkApplicationInfo application_info;
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pNext = nullptr;
        application_info.pApplicationName = this->settings.vulkan.application_name.c_str();
        application_info.applicationVersion = VK_MAKE_VERSION(version.major, version.minor, version.patch);
        application_info.pEngineName = "Nova renderer 0.8";
        application_info.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo create_info;
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pNext = nullptr;
        create_info.flags = 0;
        create_info.pApplicationInfo = &application_info;
        if(settings.debug.enabled && settings.debug.enable_validation_layers) {
            enabled_layer_names.push_back("VK_LAYER_LUNARG_standard_validation");
            // enabled_layer_names.push_back("VK_LAYER_LUNARG_api_dump");
        }
        create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layer_names.size());
        create_info.ppEnabledLayerNames = enabled_layer_names.data();

        std::vector<const char*> enabled_extension_names;
        enabled_extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef NOVA_LINUX
        enabled_extension_names.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(NOVA_WINDOWS)
        enabled_extension_names.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
#error Unsupported Operating system
#endif

        if(settings.debug.enabled) {
            enabled_extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            enabled_extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extension_names.size());
        create_info.ppEnabledExtensionNames = enabled_extension_names.data();

        NOVA_CHECK_RESULT(vkCreateInstance(&create_info, nullptr, &vk_instance));

        uint32_t num_extensions;
        vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, nullptr);
        gpu.available_extensions.resize(num_extensions);
        vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, gpu.available_extensions.data());

        fmt::memory_buffer buf;
        for(const VkExtensionProperties& props : gpu.available_extensions) {
            format_to(buf, fmt("\t{:s} version {:d}\n"), props.extensionName, props.specVersion);
        }

        NOVA_LOG(TRACE) << fmt::format(fmt("Supported extensions:\n{:s}"), fmt::to_string(buf));

        if(settings.debug.enabled) {
            vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(vk_instance, "vkCreateDebugUtilsMessengerEXT"));
            vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
                vkGetInstanceProcAddr(vk_instance, "vkDestroyDebugReportCallbackEXT"));

            VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
            debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_create_info.pNext = nullptr;
            debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_create_info.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(&debug_report_callback);
            debug_create_info.pUserData = this;

            NOVA_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(vk_instance, &debug_create_info, nullptr, &debug_callback));
        }

        // First we open the window. This doesn't depend on anything except the VkInstance/ This method also creates
        // the VkSurfaceKHR we can render to
        vulkan_render_engine::open_window(settings.window.width, settings.window.height);

        // Create the device. This depends on both the VkInstance and the VkSurfaceKHR: we need the VkSurfaceKHR to
        // make sure we find a device that can present to that surface
        create_device();

        create_per_thread_command_pools();

        // Create the swapchain. This depends on the VkInstance, VkPhysicalDevice, our pre-thread command pools, and
        // VkSurfaceKHR. This method also fills out a lot of the information in our vk_gpu_info
        create_swapchain();
        max_in_flight_frames = swapchain->get_num_images();
        NOVA_LOG(DEBUG) << "Using " << max_in_flight_frames << " swapchain images";

        create_memory_allocator();

        create_global_sync_objects();
        create_per_thread_descriptor_pools();
        create_default_samplers();

        create_builtin_uniform_buffers();

        if(settings.debug.enabled) {
            vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));

            if(vkSetDebugUtilsObjectNameEXT == nullptr) {
                NOVA_LOG(ERROR) << "Could not load the debug name function";
            }
        }

        NOVA_LOG(INFO) << "Finished initializing the Vulkan render engine";
    }

    void vulkan_render_engine::validate_mesh_options(const nova_settings::block_allocator_settings& options) const {
        if(options.buffer_part_size % sizeof(full_vertex) != 0) {
            throw std::runtime_error("vertex_memory_settings.buffer_part_size must be a multiple of sizeof(full_vertex) (which equals " +
                                     std::to_string(sizeof(full_vertex)) + ")");
        }
        if(options.new_buffer_size % options.buffer_part_size != 0) {
            throw std::runtime_error(
                "vertex_memory_settings.new_buffer_size must be a multiple of vertex_memory_settings.buffer_part_size (which equals " +
                std::to_string(options.buffer_part_size) + ")");
        }
        if(options.max_total_allocation % options.new_buffer_size != 0) {
            throw std::runtime_error(
                "vertex_memory_settings.max_total_allocation must be a multiple of vertex_memory_settings.new_buffer_size (which equals " +
                std::to_string(options.max_total_allocation) + ")");
        }
    }

    void vulkan_render_engine::open_window(uint32_t width, uint32_t height) {
#ifdef NOVA_LINUX
        window = std::make_shared<x11_window>(width, height, settings.window.title);

        VkXlibSurfaceCreateInfoKHR x_surface_create_info;
        x_surface_create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        x_surface_create_info.pNext = nullptr;
        x_surface_create_info.flags = 0;
        x_surface_create_info.dpy = window->get_display();
        x_surface_create_info.window = window->get_x11_window();

        NOVA_CHECK_RESULT(vkCreateXlibSurfaceKHR(vk_instance, &x_surface_create_info, nullptr, &surface));
#elif defined(NOVA_WINDOWS)
        window = std::make_shared<win32_window>(width, height);

        VkWin32SurfaceCreateInfoKHR win32_surface_create = {};
        win32_surface_create.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        win32_surface_create.hwnd = window->get_window_handle();

        NOVA_CHECK_RESULT(vkCreateWin32SurfaceKHR(vk_instance, &win32_surface_create, nullptr, &surface));

#else
#error Unsuported window system
#endif
    }

    void vulkan_render_engine::create_device() {
        uint32_t device_count;
        NOVA_CHECK_RESULT(vkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr));
        auto physical_devices = std::vector<VkPhysicalDevice>(device_count);
        NOVA_CHECK_RESULT(vkEnumeratePhysicalDevices(vk_instance, &device_count, physical_devices.data()));

        uint32_t graphics_family_idx = 0xFFFFFFFF;
        uint32_t compute_family_idx = 0xFFFFFFFF;
        uint32_t copy_family_idx = 0xFFFFFFFF;

        for(uint32_t device_idx = 0; device_idx < device_count; device_idx++) {
            graphics_family_idx = 0xFFFFFFFF;
            // NOLINTNEXTLINE(misc-misplaced-const)
            const VkPhysicalDevice current_device = physical_devices[device_idx];
            vkGetPhysicalDeviceProperties(current_device, &gpu.props);

            if(gpu.props.vendorID == 0x8086 &&
               device_count - 1 > device_idx) { // Intel GPU... they are not powerful and we have more available, so skip it
                continue;
            }

            if(!does_device_support_extensions(current_device)) {
                continue;
            }

            uint32_t queue_family_count;
            vkGetPhysicalDeviceQueueFamilyProperties(current_device, &queue_family_count, nullptr);
            gpu.queue_family_props.resize(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(current_device, &queue_family_count, gpu.queue_family_props.data());

            for(uint32_t queue_idx = 0; queue_idx < queue_family_count; queue_idx++) {
                const VkQueueFamilyProperties current_properties = gpu.queue_family_props[queue_idx];
                if(current_properties.queueCount < 1) {
                    continue;
                }

                VkBool32 supports_present = VK_FALSE;
                NOVA_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(current_device, queue_idx, surface, &supports_present));
                const VkQueueFlags supports_graphics = current_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
                if((supports_graphics != 0U) && supports_present == VK_TRUE && graphics_family_idx == 0xFFFFFFFF) {
                    graphics_family_idx = queue_idx;
                }

                const VkQueueFlags supports_compute = current_properties.queueFlags & VK_QUEUE_COMPUTE_BIT;
                if((supports_compute != 0U) && compute_family_idx == 0xFFFFFFFF) {
                    compute_family_idx = queue_idx;
                }

                const VkQueueFlags supports_copy = current_properties.queueFlags & VK_QUEUE_TRANSFER_BIT;
                if((supports_copy != 0U) && copy_family_idx == 0xFFFFFFFF) {
                    copy_family_idx = queue_idx;
                }
            }

            if(graphics_family_idx != 0xFFFFFFFF) {
                NOVA_LOG(INFO) << fmt::format(fmt("Selected GPU {:s}"), gpu.props.deviceName);
                gpu.phys_device = current_device;
                break;
            }
        }

        if(gpu.phys_device == nullptr) {
            NOVA_LOG(ERROR) << "Failed to find working GPU";
            throw std::runtime_error("Failed to find good GPU");
        }

        vkGetPhysicalDeviceFeatures(gpu.phys_device, &gpu.supported_features);

        const float priority = 1.0;

        VkDeviceQueueCreateInfo graphics_queue_create_info{};
        graphics_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphics_queue_create_info.pNext = nullptr;
        graphics_queue_create_info.flags = 0;
        graphics_queue_create_info.queueCount = 1;
        graphics_queue_create_info.queueFamilyIndex = graphics_family_idx;
        graphics_queue_create_info.pQueuePriorities = &priority;

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos = {graphics_queue_create_info};

        VkPhysicalDeviceFeatures physical_device_features{};
        physical_device_features.geometryShader = VK_TRUE;
        physical_device_features.tessellationShader = VK_TRUE;
        physical_device_features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pNext = nullptr;
        device_create_info.flags = 0;
        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.pEnabledFeatures = &physical_device_features;
        device_create_info.enabledExtensionCount = 1;
        const char* swapchain_extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        device_create_info.ppEnabledExtensionNames = &swapchain_extension;
        device_create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layer_names.size());
        if(!enabled_layer_names.empty()) {
            device_create_info.ppEnabledLayerNames = enabled_layer_names.data();
        }

        NOVA_CHECK_RESULT(vkCreateDevice(gpu.phys_device, &device_create_info, nullptr, &device));

        graphics_family_index = graphics_family_idx;
        vkGetDeviceQueue(device, graphics_family_idx, 0, &graphics_queue);
        compute_family_index = compute_family_idx;
        vkGetDeviceQueue(device, compute_family_idx, 0, &compute_queue);
        transfer_family_index = copy_family_idx;
        vkGetDeviceQueue(device, copy_family_idx, 0, &copy_queue);
    }

    bool vulkan_render_engine::does_device_support_extensions(VkPhysicalDevice device) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> available(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available.data());

        std::set<std::string> required = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        for(const auto& extension : available) {
            required.erase(static_cast<const char*>(extension.extensionName));
        }

        return required.empty();
    }

    void vulkan_render_engine::create_per_thread_command_pools() {
        const uint32_t num_threads = 1;
        command_pools_by_thread_idx.reserve(num_threads);

        for(uint32_t i = 0; i < num_threads; i++) {
            command_pools_by_thread_idx.push_back(make_new_command_pools());
        }
    }

    std::unordered_map<uint32_t, VkCommandPool> vulkan_render_engine::make_new_command_pools() const {
        std::vector<uint32_t> queue_indices;
        queue_indices.push_back(graphics_family_index);
        queue_indices.push_back(transfer_family_index);
        queue_indices.push_back(compute_family_index);

        std::unordered_map<uint32_t, VkCommandPool> pools_by_queue;
        pools_by_queue.reserve(3);

        for(const uint32_t queue_index : queue_indices) {
            VkCommandPoolCreateInfo command_pool_create_info;
            command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            command_pool_create_info.pNext = nullptr;
            command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            command_pool_create_info.queueFamilyIndex = queue_index;

            VkCommandPool command_pool;
            NOVA_CHECK_RESULT(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));
            pools_by_queue[queue_index] = command_pool;
        }

        return pools_by_queue;
    }

    void vulkan_render_engine::create_swapchain() {
        NOVA_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.phys_device, surface, &gpu.surface_capabilities));

        uint32_t num_surface_formats;
        NOVA_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.phys_device, surface, &num_surface_formats, nullptr));
        gpu.surface_formats.resize(num_surface_formats);
        NOVA_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.phys_device, surface, &num_surface_formats, gpu.surface_formats.data()));

        uint32_t num_surface_present_modes;
        NOVA_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.phys_device, surface, &num_surface_present_modes, nullptr));
        std::vector<VkPresentModeKHR> present_modes(num_surface_present_modes);
        NOVA_CHECK_RESULT(
            vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.phys_device, surface, &num_surface_present_modes, present_modes.data()));

        swapchain = std::make_unique<swapchain_manager>(max_in_flight_frames, *this, window->get_window_size(), present_modes);
    }

    void vulkan_render_engine::create_memory_allocator() {
        VmaAllocatorCreateInfo allocator_create_info = {};
        allocator_create_info.physicalDevice = gpu.phys_device;
        allocator_create_info.device = device;

        NOVA_CHECK_RESULT(vmaCreateAllocator(&allocator_create_info, &vma_allocator));
    }

    void vulkan_render_engine::create_global_sync_objects() {
        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        frame_fences.resize(max_in_flight_frames);
        image_available_semaphores.resize(max_in_flight_frames);
        render_finished_semaphores.resize(max_in_flight_frames);

        for(uint32_t i = 0; i < max_in_flight_frames; i++) {
            NOVA_CHECK_RESULT(vkCreateFence(device, &fence_info, nullptr, &frame_fences[i]));
            NOVA_CHECK_RESULT(vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]));
            NOVA_CHECK_RESULT(vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]));

            NOVA_LOG(TRACE) << "render_finished_semaphores[" << i << "] = " << render_finished_semaphores[i];
        }
    }

    void vulkan_render_engine::create_per_thread_descriptor_pools() {
        const uint32_t num_threads = 1;
        descriptor_pools_by_thread_idx.reserve(num_threads);

        for(uint32_t i = 0; i < num_threads; i++) {
            descriptor_pools_by_thread_idx.push_back(make_new_descriptor_pool());
        }
    }

    VkDescriptorPool vulkan_render_engine::make_new_descriptor_pool() const {
        std::vector<VkDescriptorPoolSize> pool_sizes;
        pool_sizes.emplace_back(
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5}); // Virtual textures greatly reduces the number of total textures
        pool_sizes.emplace_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 5});
        pool_sizes.emplace_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5000});

        VkDescriptorPoolCreateInfo pool_create_info = {};
        pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_create_info.maxSets = 5000;
        pool_create_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_create_info.pPoolSizes = pool_sizes.data();

        VkDescriptorPool pool;
        NOVA_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &pool));

        return pool;
    }

    void vulkan_render_engine::create_default_samplers() {
        VkSamplerCreateInfo point_sampler_create = {};
        point_sampler_create.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        point_sampler_create.magFilter = VK_FILTER_NEAREST;
        point_sampler_create.minFilter = VK_FILTER_NEAREST;

        NOVA_CHECK_RESULT(vkCreateSampler(device, &point_sampler_create, nullptr, &point_sampler));
    }

} // namespace nova::renderer
