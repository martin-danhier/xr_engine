#ifdef RENDERER_VULKAN
#include "xr_engine/core/xr_renderer.h"

#include <volk.h>
#include <xr_engine/core/global.h>
#include <xr_engine/core/window.h>
#include <xr_engine/core/xr/xr_system.h>
#include <xr_engine/utils/global_utils.h>
#include <xr_engine/utils/openxr_utils.h>

// Needs to be after volk.h
#include <openxr/openxr_platform.h>
#include <vk_mem_alloc.h>

// Check dependencies versions
#ifndef VK_API_VERSION_1_3
// We need Vulkan SDK 1.3 for vk_mem_alloc, because it uses VK_API_VERSION_MAJOR which was introduced in 1.3
// We need this even if we use a lower version of Vulkan in the instance
#error "Vulkan SDK 1.3 is required"
#endif

namespace xre
{
    // --=== Function pointers ===--

    PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR       xrGetVulkanGraphicsDevice2KHR       = nullptr;
    PFN_xrCreateVulkanInstanceKHR           xrCreateVulkanInstanceKHR           = nullptr;
    PFN_xrCreateVulkanDeviceKHR             xrCreateVulkanDeviceKHR             = nullptr;

    // --=== Structs ===---

    // region Allocator

    struct AllocatedBuffer
    {
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkBuffer      buffer     = VK_NULL_HANDLE;
        uint32_t      size       = 0;

        [[nodiscard]] inline bool is_valid() const
        {
            return allocation != VK_NULL_HANDLE;
        }
    };

    struct AllocatedImage
    {
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImage       image      = VK_NULL_HANDLE;
        VkImageView   image_view = VK_NULL_HANDLE;
    };

    class Allocator
    {
      private:
        VmaAllocator m_allocator             = VK_NULL_HANDLE;
        VkDevice     m_device                = VK_NULL_HANDLE;
        uint32_t     m_graphics_queue_family = 0;
        uint32_t     m_transfer_queue_family = 0;

      public:
        Allocator() = default;
        Allocator(VkInstance       instance,
                  VkDevice         device,
                  VkPhysicalDevice physical_device,
                  uint32_t         graphics_queue_family,
                  uint32_t         transfer_queue_family);
        Allocator(Allocator &&other) noexcept;
        Allocator &operator=(Allocator &&other) noexcept;

        ~Allocator();

        [[nodiscard]] AllocatedImage create_image(VkFormat           image_format,
                                                  VkExtent3D         image_extent,
                                                  VkImageUsageFlags  image_usage,
                                                  VkImageAspectFlags image_aspect,
                                                  VmaMemoryUsage     memory_usage,
                                                  bool               concurrent = false) const;
        void                         destroy_image(AllocatedImage &image) const;

        [[nodiscard]] AllocatedBuffer create_buffer(size_t             allocation_size,
                                                    VkBufferUsageFlags buffer_usage,
                                                    VmaMemoryUsage     memory_usage,
                                                    bool               concurrent = false) const;
        void                          destroy_buffer(AllocatedBuffer &buffer) const;
        void                         *map_buffer(AllocatedBuffer &buffer) const;
        void                          unmap_buffer(AllocatedBuffer &buffer) const;
    };
    // endregion

    struct Queue
    {
        uint32_t family_index = 0;
        VkQueue  queue        = VK_NULL_HANDLE;
    };

    struct XrRenderer::Data
    {
        uint8_t  reference_count = 0;
        XrSystem xr_system       = {};
        Window   mirror_window   = {};

        // Vulkan core
        VkInstance                 vk_instance       = VK_NULL_HANDLE;
        VkDevice                   device            = VK_NULL_HANDLE;
        VkPhysicalDevice           physical_device   = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties device_properties = {};
#ifdef USE_VK_VALIDATION_LAYERS
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
#endif
        Allocator allocator = {};

        // Queues
        Queue graphics_queue = {};
        Queue transfer_queue = {};

        // XR
        XrGraphicsBindingVulkan2KHR graphics_binding = {};
        std::vector<XrViewConfigurationView> view_configurations;
        std::vector<XrView>                  views;

        // --- Methods ---
        template<typename T>
        void                 copy_buffer_to_gpu(const T &src, AllocatedBuffer &dst, size_t offset = 0);
        [[nodiscard]] size_t pad_uniform_buffer_size(size_t original_size) const;
    };

    // --=== Utils ===--

    namespace renderer
    {
        // region Checks and string conversions

        std::string vk_result_to_string(VkResult result)
        {
            switch (result)
            {
                case VK_SUCCESS: return "VK_SUCCESS";
                case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
                case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
                case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
                case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
                case VK_TIMEOUT: return "VK_TIMEOUT";
                default: return std::to_string(result);
            }
        }

        std::string vk_present_mode_to_string(VkPresentModeKHR present_mode)
        {
            switch (present_mode)
            {
                case VK_PRESENT_MODE_IMMEDIATE_KHR: return "Immediate";
                case VK_PRESENT_MODE_MAILBOX_KHR: return "Mailbox";
                case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
                case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO Relaxed";
                default: return std::to_string(present_mode);
            }
        }

        void vk_check(VkResult result, const std::string &error_message = "")
        {
            // Warnings
            if (result == VK_SUBOPTIMAL_KHR)
            {
                std::cout << "[Vulkan Warning] A Vulkan function call returned VkResult = " << vk_result_to_string(result) << "\n";
            }
            // Errors
            else if (result != VK_SUCCESS)
            {
                // Pretty print error
                std::cerr << "[Vulkan Error] A Vulkan function call returned VkResult = " << vk_result_to_string(result) << "\n";

                // Optional custom error message precision
                if (!error_message.empty())
                {
                    std::cerr << "Precision: " << error_message << "\n";
                }
            }
        }

        // endregion

        // region Instance creation

        bool check_instance_extension_support(const std::vector<const char *> &desired_extensions)
        {
            // Get the number of available desired_extensions
            uint32_t available_extensions_count = 0;
            vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, VK_NULL_HANDLE));
            // Create an array with enough room and fetch the available desired_extensions
            std::vector<VkExtensionProperties> available_extensions(available_extensions_count);
            vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, available_extensions.data()));

            // For each desired extension, rg_renderer_check if it is available
            bool valid = true;
            for (const auto &desired_extension : desired_extensions)
            {
                bool       found = false;
                const auto ext   = std::string(desired_extension);

                // Search available extensions until the desired one is found or not
                for (const auto &available_extension : available_extensions)
                {
                    if (ext == std::string(available_extension.extensionName))
                    {
                        found = true;
                        break;
                    }
                }

                // Stop looking if nothing was found
                if (!found)
                {
                    valid = false;
                    std::cerr << "[Vulkan Error] The extension \"" << ext << "\" is not available.\n";
                    break;
                }
            }

            return valid;
        }

        bool check_device_extension_support(const VkPhysicalDevice          &physical_device,
                                            const std::vector<const char *> &desired_extensions)
        {
            // Get the number of available desired_extensions
            uint32_t available_extensions_count = 0;
            vk_check(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extensions_count, VK_NULL_HANDLE));
            // Create an array with enough room and fetch the available desired_extensions
            std::vector<VkExtensionProperties> available_extensions(available_extensions_count);
            vk_check(vkEnumerateDeviceExtensionProperties(physical_device,
                                                          nullptr,
                                                          &available_extensions_count,
                                                          available_extensions.data()));

            // For each desired extension, rg_renderer_check if it is available
            bool valid = true;
            for (const auto &desired_extension : desired_extensions)
            {
                bool       found = false;
                const auto ext   = std::string(desired_extension);

                // Search available extensions until the desired one is found or not
                for (const auto &available_extension : available_extensions)
                {
                    if (ext == std::string(available_extension.extensionName))
                    {
                        found = true;
                        break;
                    }
                }

                // Stop looking if nothing was found
                if (!found)
                {
                    valid = false;
                    std::cerr << "[Error] The extension \"" << ext << "\" is not available.\n";
                    break;
                }
            }

            return valid;
        }

        bool check_layer_support(const std::vector<const char *> &desired_layers)
        {
            // Get the number of available desired_layers
            uint32_t available_layers_count = 0;
            vk_check(vkEnumerateInstanceLayerProperties(&available_layers_count, nullptr));
            // Create an array with enough room and fetch the available desired_layers
            std::vector<VkLayerProperties> available_layers(available_layers_count);
            vk_check(vkEnumerateInstanceLayerProperties(&available_layers_count, available_layers.data()));

            // For each desired layer, rg_renderer_check if it is available
            bool valid = true;
            for (const auto &desired_layer : desired_layers)
            {
                bool       found = false;
                const auto layer = std::string(desired_layer);

                // Search available layers until the desired one is found or not
                for (const auto &available_layer : available_layers)
                {
                    if (layer == std::string(available_layer.layerName))
                    {
                        found = true;
                        break;
                    }
                }

                // Stop looking if nothing was found
                if (!found)
                {
                    valid = false;
                    std::cerr << "[Error] The layer \"" << layer << "\" is not available.\n";
                    break;
                }
            }

            return valid;
        }

        /**
         * Callback for the vulkan debug messenger
         * @param message_severity Severity of the message
         * @param message_types Type of the message
         * @param callback_data Additional m_data concerning the message
         * @param user_data User m_data passed to the debug messenger
         */
        VkBool32 debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                          VkDebugUtilsMessageTypeFlagsEXT             message_types,
                                          const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                          void                                       *_)
        {
            // Inspired by VkBootstrap's default debug messenger. (Made by Charles Giessen)
            // Get severity
            const char *str_severity;
            switch (message_severity)
            {
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: str_severity = "VERBOSE"; break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: str_severity = "ERROR"; break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: str_severity = "WARNING"; break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: str_severity = "INFO"; break;
                default: str_severity = "UNKNOWN"; break;
            }

            // Get type
            const char *str_type;
            switch (message_types)
            {
                case 7: str_type = "General | Validation | Performance"; break;
                case 6: str_type = "Validation | Performance"; break;
                case 5: str_type = "General | Performance"; break;
                case 4: str_type = "Performance"; break;
                case 3: str_type = "General | Validation"; break;
                case 2: str_type = "Validation"; break;
                case 1: str_type = "General"; break;
                default: str_type = "Unknown"; break;
            }

            // Print the message to stderr if it is an error.
            auto &output = message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? std::cerr : std::cout;
            output << "[Vulkan " << str_severity << ": " << str_type << "]\n" << callback_data->pMessage << "\n";

            return VK_FALSE;
        }

        // endregion

    } // namespace renderer
    using namespace renderer;

    // region Allocator

    Allocator::Allocator(VkInstance       instance,
                         VkDevice         device,
                         VkPhysicalDevice physical_device,
                         uint32_t         graphics_queue_family,
                         uint32_t         transfer_queue_family)
        : m_device(device),
          m_graphics_queue_family(graphics_queue_family),
          m_transfer_queue_family(transfer_queue_family)
    {
        VmaVulkanFunctions vulkan_functions = {
            .vkGetPhysicalDeviceProperties           = vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties     = vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory                        = vkAllocateMemory,
            .vkFreeMemory                            = vkFreeMemory,
            .vkMapMemory                             = vkMapMemory,
            .vkUnmapMemory                           = vkUnmapMemory,
            .vkFlushMappedMemoryRanges               = vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges          = vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory                      = vkBindBufferMemory,
            .vkBindImageMemory                       = vkBindImageMemory,
            .vkGetBufferMemoryRequirements           = vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements            = vkGetImageMemoryRequirements,
            .vkCreateBuffer                          = vkCreateBuffer,
            .vkDestroyBuffer                         = vkDestroyBuffer,
            .vkCreateImage                           = vkCreateImage,
            .vkDestroyImage                          = vkDestroyImage,
            .vkCmdCopyBuffer                         = vkCmdCopyBuffer,
            .vkGetBufferMemoryRequirements2KHR       = vkGetBufferMemoryRequirements2KHR,
            .vkGetImageMemoryRequirements2KHR        = vkGetImageMemoryRequirements2KHR,
            .vkBindBufferMemory2KHR                  = vkBindBufferMemory2KHR,
            .vkBindImageMemory2KHR                   = vkBindImageMemory2KHR,
            .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR,
        };
        VmaAllocatorCreateInfo allocator_create_info = {
            .physicalDevice   = physical_device,
            .device           = device,
            .pVulkanFunctions = &vulkan_functions,
            .instance         = instance,
        };

        vk_check(vmaCreateAllocator(&allocator_create_info, &m_allocator), "Failed to create allocator");
    }

    Allocator::~Allocator()
    {
        if (m_allocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }
    }

    Allocator::Allocator(Allocator &&other) noexcept
        : m_allocator(other.m_allocator),
          m_device(other.m_device),
          m_graphics_queue_family(other.m_graphics_queue_family),
          m_transfer_queue_family(other.m_transfer_queue_family)
    {
        other.m_allocator = VK_NULL_HANDLE;
    }

    Allocator &Allocator::operator=(Allocator &&other) noexcept
    {
        if (this != &other)
        {
            if (m_allocator != VK_NULL_HANDLE)
            {
                vmaDestroyAllocator(m_allocator);
                m_allocator = VK_NULL_HANDLE;
            }
            m_allocator       = other.m_allocator;
            m_device          = other.m_device;
            other.m_allocator = VK_NULL_HANDLE;
        }
        return *this;
    }

    AllocatedImage Allocator::create_image(VkFormat           image_format,
                                           VkExtent3D         image_extent,
                                           VkImageUsageFlags  image_usage,
                                           VkImageAspectFlags image_aspect,
                                           VmaMemoryUsage     memory_usage,
                                           bool               concurrent) const
    {
        // We use VMA for now. We can always switch to a custom allocator later if we want to.
        AllocatedImage image;

        check(image_extent.width >= 1 && image_extent.height >= 1 && image_extent.depth >= 1,
              "Tried to create an image with an invalid extent. The extent must be at least 1 in each dimension.");

        // Create the image using VMA
        VkImageCreateInfo image_create_info = {
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = image_format,
            .extent                = image_extent,
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = image_usage,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        // Sharing mode
        if (concurrent && m_graphics_queue_family != m_transfer_queue_family)
        {
            image_create_info.sharingMode             = VK_SHARING_MODE_CONCURRENT;
            const std::vector<uint32_t> queue_indices = {
                m_graphics_queue_family,
                m_transfer_queue_family,
            };
            image_create_info.pQueueFamilyIndices   = queue_indices.data();
            image_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_indices.size());
        }

        VmaAllocationCreateInfo alloc_create_info = {
            .usage          = memory_usage,
            .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        // Create the image
        vk_check(vmaCreateImage(m_allocator, &image_create_info, &alloc_create_info, &image.image, &image.allocation, nullptr),
                 "Failed to create image");

        // Create image view
        VkImageViewCreateInfo image_view_create_info = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .image    = image.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = image_format,
            .components =
                {
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    image_aspect,
                    0,
                    1,
                    0,
                    1,
                },
        };
        vk_check(vkCreateImageView(m_device, &image_view_create_info, nullptr, &image.image_view), "Failed to create image view");

        return image;
    }

    void Allocator::destroy_image(AllocatedImage &image) const
    {
        vkDestroyImageView(m_device, image.image_view, nullptr);
        if (image.allocation != VK_NULL_HANDLE)
        {
            vmaDestroyImage(m_allocator, image.image, image.allocation);
        }
        image.image      = VK_NULL_HANDLE;
        image.allocation = VK_NULL_HANDLE;
        image.image_view = VK_NULL_HANDLE;
    }

    AllocatedBuffer Allocator::create_buffer(size_t             allocation_size,
                                             VkBufferUsageFlags buffer_usage,
                                             VmaMemoryUsage     memory_usage,
                                             bool               concurrent) const
    {
        // We use VMA for now. We can always switch to a custom allocator later if we want to.
        AllocatedBuffer buffer = {
            .size = static_cast<uint32_t>(allocation_size),
        };

        // Create the buffer using VMA
        VkBufferCreateInfo buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            // Buffer info
            .size  = allocation_size,
            .usage = buffer_usage,
        };

        // Sharing mode
        if (concurrent && m_graphics_queue_family != m_transfer_queue_family)
        {
            buffer_create_info.sharingMode            = VK_SHARING_MODE_CONCURRENT;
            const std::vector<uint32_t> queue_indices = {
                m_graphics_queue_family,
                m_transfer_queue_family,
            };
            buffer_create_info.pQueueFamilyIndices   = queue_indices.data();
            buffer_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_indices.size());
        }

        // Create an allocation info
        VmaAllocationCreateInfo allocation_create_info = {
            .usage = memory_usage,
        };

        // Create the buffer
        vk_check(vmaCreateBuffer(m_allocator,
                                 &buffer_create_info,
                                 &allocation_create_info,
                                 &buffer.buffer,
                                 &buffer.allocation,
                                 VK_NULL_HANDLE),
                 "Couldn't allocate buffer");

        return buffer;
    }

    void Allocator::destroy_buffer(AllocatedBuffer &buffer) const
    {
        if (buffer.buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
            buffer.buffer     = VK_NULL_HANDLE;
            buffer.allocation = VK_NULL_HANDLE;
            buffer.size       = 0;
        }
    }

    void *Allocator::map_buffer(AllocatedBuffer &buffer) const
    {
        void *data = nullptr;
        vk_check(vmaMapMemory(m_allocator, buffer.allocation, &data), "Failed to map buffer");
        return data;
    }

    void Allocator::unmap_buffer(AllocatedBuffer &buffer) const
    {
        vmaUnmapMemory(m_allocator, buffer.allocation);
    }

    template<typename T>
    void XrRenderer::Data::copy_buffer_to_gpu(const T &src, AllocatedBuffer &dst, size_t offset)
    {
        // Copy the data to the GPU
        char *data = static_cast<char *>(allocator.map_buffer(dst));

        // Pad the data if needed
        if (offset != 0)
        {
            data += pad_uniform_buffer_size(sizeof(T)) * offset;
        }

        memcpy(data, &src, sizeof(T));
        allocator.unmap_buffer(dst);
    }

    size_t XrRenderer::Data::pad_uniform_buffer_size(size_t original_size) const
    {
        // Get the alignment requirement
        const size_t &min_alignment = device_properties.limits.minUniformBufferOffsetAlignment;
        size_t        aligned_size  = original_size;
        if (min_alignment > 0)
        {
            aligned_size = (aligned_size + min_alignment - 1) & ~(min_alignment - 1);
        }
        return aligned_size;
    }

    // endregion

    // --=== API ===--

    // region Init and shared pointer logic

    XrRenderer::XrRenderer(const XrSystem &parent, const Settings &settings, Window *mirror_window) : m_data(new Data)
    {
        check(parent.is_valid(), "Invalid XrSystem. Unable to create XrRenderer");

        m_data->reference_count = 1;
        m_data->xr_system       = parent;
        if (mirror_window)
        {
            m_data->mirror_window = *mirror_window;
        }
        XrInstance xr_instance  = parent.instance();
        XrSystemId xr_system_id = parent.system_id();

        // Load XR functions
        xr_check(xrGetInstanceProcAddr(xr_instance,
                                       "xrGetVulkanGraphicsDevice2KHR",
                                       reinterpret_cast<PFN_xrVoidFunction *>(&xrGetVulkanGraphicsDevice2KHR)),
                 "Failed to load xrGetVulkanGraphicsDevice2KHR");
        xr_check(xrGetInstanceProcAddr(xr_instance,
                                       "xrCreateVulkanInstanceKHR",
                                       reinterpret_cast<PFN_xrVoidFunction *>(&xrCreateVulkanInstanceKHR)),
                 "Failed to load xrCreateVulkanInstanceKHR");
        xr_check(xrGetInstanceProcAddr(xr_instance,
                                       "xrCreateVulkanDeviceKHR",
                                       reinterpret_cast<PFN_xrVoidFunction *>(&xrCreateVulkanDeviceKHR)),
                 "Failed to load xrCreateVulkanDeviceKHR");
        xr_check(xrGetInstanceProcAddr(xr_instance,
                                       "xrGetVulkanGraphicsRequirements2KHR",
                                       reinterpret_cast<PFN_xrVoidFunction *>(&xrGetVulkanGraphicsRequirements2KHR)),
                 "Failed to load xrGetVulkanGraphicsRequirements2KHR");

        // Get requirements
        XrGraphicsRequirementsVulkanKHR graphics_requirements = {};
        graphics_requirements.type                            = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
        xr_check(xrGetVulkanGraphicsRequirements2KHR(xr_instance, xr_system_id, &graphics_requirements),
                 "Failed to get graphics requirements");

        auto vk_version = VK_MAKE_VERSION(XR_VERSION_MAJOR(graphics_requirements.maxApiVersionSupported),
                                          XR_VERSION_MINOR(graphics_requirements.maxApiVersionSupported),
                                          0);
        std::cout << "Using Vulkan backend, version " << make_version(graphics_requirements.maxApiVersionSupported) << "\n";

        // Initialize volk
        vk_check(volkInitialize(), "Couldn't initialize Volk.");

        // region Instance creation
        {
            // Set required extensions
            uint32_t extra_extension_count = 0;
#ifdef USE_VK_VALIDATION_LAYERS
            extra_extension_count += 1;
#endif
            // Get the extensions that the window and XR system need
            auto required_extensions = std::vector<const char *>();

            if (m_data->mirror_window.is_valid())
            {
                m_data->mirror_window.get_required_vulkan_extensions(required_extensions);
            }

            // Add other extensions
#ifdef USE_VK_VALIDATION_LAYERS
            required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

            check(check_instance_extension_support(required_extensions), "Not all required Vulkan extensions are supported.");

            // Get the validation layers if needed
#ifdef USE_VK_VALIDATION_LAYERS
            const std::vector<const char *> enabled_layers = {"VK_LAYER_KHRONOS_validation"};
            check(check_layer_support(enabled_layers), "Vulkan validation layers requested, but not available.");
#endif

            VkApplicationInfo application_info = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pNext = VK_NULL_HANDLE,
                // Application infos
                .pApplicationName   = settings.application_info.name,
                .applicationVersion = VK_MAKE_VERSION(settings.application_info.version.major,
                                                      settings.application_info.version.minor,
                                                      settings.application_info.version.patch),
                // Engine infos
                .pEngineName   = ENGINE_NAME,
                .engineVersion = VK_MAKE_VERSION(ENGINE_VERSION.major, ENGINE_VERSION.minor, ENGINE_VERSION.patch),
                .apiVersion    = vk_version,
            };

            VkInstanceCreateInfo vk_instance_create_info {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = VK_NULL_HANDLE,
                // App info
                .pApplicationInfo = &application_info,
            // Validation layers
#ifdef USE_VK_VALIDATION_LAYERS
                .enabledLayerCount   = static_cast<uint32_t>(enabled_layers.size()),
                .ppEnabledLayerNames = enabled_layers.data(),
#else
                .enabledLayerCount   = 0,
                .ppEnabledLayerNames = nullptr,
#endif
                // Extensions
                .enabledExtensionCount   = static_cast<uint32_t>(required_extensions.size()),
                .ppEnabledExtensionNames = required_extensions.data(),
            };

            XrVulkanInstanceCreateInfoKHR vulkan_instance_create_info {XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
            vulkan_instance_create_info.systemId               = xr_system_id;
            vulkan_instance_create_info.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
            vulkan_instance_create_info.vulkanCreateInfo       = &vk_instance_create_info;

            // Create instance
            VkResult result;
            xr_check(xrCreateVulkanInstanceKHR(xr_instance, &vulkan_instance_create_info, &m_data->vk_instance, &result),
                     "Failed to create Vulkan instance");
            vk_check(result);

            // Register instance in Volk
            volkLoadInstance(m_data->vk_instance);

            // Create debug messenger
#ifdef USE_VK_VALIDATION_LAYERS
            VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
                // Struct info
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext = VK_NULL_HANDLE,
                // Message settings
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                // Callback
                .pfnUserCallback = static_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debug_messenger_callback),
            };
            vk_check(
                vkCreateDebugUtilsMessengerEXT(m_data->vk_instance, &debug_messenger_create_info, nullptr, &m_data->debug_messenger),
                "Couldn't create debug messenger");
#endif
        }
        // endregion

        // --=== Physical device and queue families selection ===--

        // region Physical device and queue families selection
        {
            // Find physical device
            XrVulkanGraphicsDeviceGetInfoKHR graphics_device_get_info {
                .type           = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
                .next           = XR_NULL_HANDLE,
                .systemId       = xr_system_id,
                .vulkanInstance = m_data->vk_instance,
            };

            xr_check(xrGetVulkanGraphicsDevice2KHR(xr_instance, &graphics_device_get_info, &m_data->physical_device),
                     "Failed to get Vulkan graphics device");

            // Log chosen GPU
            VkPhysicalDeviceProperties physical_device_properties;
            vkGetPhysicalDeviceProperties(m_data->physical_device, &physical_device_properties);
            std::cout << "Suitable GPU found: " << physical_device_properties.deviceName << std::endl;

            // Get queue families
            uint32_t queue_family_properties_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(m_data->physical_device, &queue_family_properties_count, VK_NULL_HANDLE);
            std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_properties_count);
            vkGetPhysicalDeviceQueueFamilyProperties(m_data->physical_device,
                                                     &queue_family_properties_count,
                                                     queue_family_properties.data());

            // Find the queue families that we need
            bool found_graphics_queue         = false;
            bool found_transfer_queue         = false;
            bool found_optimal_transfer_queue = false;

            for (uint32_t i = 0; i < queue_family_properties_count; i++)
            {
                const auto &family_properties = queue_family_properties[i];

                // Save the graphics queue family_index
                if (!found_graphics_queue && family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    m_data->graphics_queue.family_index = i;
                    found_graphics_queue                = true;
                }

                // Save the transfer queue family_index
                if (family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT)
                {
                    // If it's not the same as the graphics queue, we can use it
                    if (!(family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    {
                        m_data->transfer_queue.family_index = i;
                        found_transfer_queue                = true;
                        found_optimal_transfer_queue        = true;
                    }
                    // It's the same as the graphics one, but we don't have one yet, so we'll take it
                    // But we'll keep looking for another one
                    else if (!found_transfer_queue)
                    {
                        m_data->transfer_queue.family_index = i;
                        found_transfer_queue                = true;
                    }
                }

                // Stop searching if we found everything we need
                if (found_graphics_queue && found_optimal_transfer_queue)
                {
                    break;
                }
            }

            // If we didn't find a graphics queue, we can't continue
            check(found_graphics_queue, "Unable to find a graphics queue family_index.");

            // If we didn't find a transfer queue, we can't continue
            check(found_transfer_queue, "Unable to find a transfer queue family_index.");

            // Get GPU properties
            vkGetPhysicalDeviceProperties(m_data->physical_device, &m_data->device_properties);
        }
        // endregion

        // --=== Logical device and queues creation ===--

        // region Device and queues creation

        {
            // Get required device extensions
            std::vector<const char *> required_device_extensions;

            if (m_data->mirror_window.is_valid())
            {
                required_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            }

            std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
            queue_create_infos.reserve(2);

            // Define the parameters for the graphics queue
            std::vector<float> priorities;
            priorities.reserve(2);
            priorities.push_back(1.0f);

            // Add the transfer queue if it is the same as the graphics one
            if (m_data->graphics_queue.family_index == m_data->transfer_queue.family_index)
            {
                priorities.push_back(0.7f);
            }
            queue_create_infos.push_back(VkDeviceQueueCreateInfo {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                // Queue info
                .queueFamilyIndex = m_data->graphics_queue.family_index,
                .queueCount       = static_cast<uint32_t>(priorities.size()),
                .pQueuePriorities = priorities.data(),
            });

            // Define the parameters for the transfer queue
            float transfer_queue_priority = 1.0f;
            if (m_data->graphics_queue.family_index != m_data->transfer_queue.family_index)
            {
                queue_create_infos.push_back(VkDeviceQueueCreateInfo {
                    // Struct infos
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = nullptr,
                    // Queue info
                    .queueFamilyIndex = m_data->transfer_queue.family_index,
                    .queueCount       = 1,
                    .pQueuePriorities = &transfer_queue_priority,
                });
            }

            // Create the logical device
            VkPhysicalDeviceFeatures features      = {};
            features.shaderStorageImageMultisample = VK_TRUE;

            VkDeviceCreateInfo vk_device_create_info = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = nullptr,
                // Queue infos
                .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
                .pQueueCreateInfos    = queue_create_infos.data(),
                // Layers
                .enabledLayerCount   = 0,
                .ppEnabledLayerNames = nullptr,
                // Extensions
                .enabledExtensionCount   = static_cast<uint32_t>(required_device_extensions.size()),
                .ppEnabledExtensionNames = required_device_extensions.data(),
                .pEnabledFeatures        = &features,
            };

            XrVulkanDeviceCreateInfoKHR device_create_info {
                .type                   = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
                .next                   = XR_NULL_HANDLE,
                .systemId               = xr_system_id,
                .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vulkanPhysicalDevice   = m_data->physical_device,
                .vulkanCreateInfo       = &vk_device_create_info,
            };

            // Create device
            VkResult result;
            xr_check(xrCreateVulkanDeviceKHR(xr_instance, &device_create_info, &m_data->device, &result),
                     "Failed to create Vulkan device");
            vk_check(result);

            // Load device in volk
            volkLoadDevice(m_data->device);

            // Get created queues
            vkGetDeviceQueue(m_data->device, m_data->graphics_queue.family_index, 0, &m_data->graphics_queue.queue);
            // Get the transfer queue. If it is the same as the graphics one, it will be a second queue on the same
            // family
            if (m_data->graphics_queue.family_index == m_data->transfer_queue.family_index)
            {
                vkGetDeviceQueue(m_data->device, m_data->transfer_queue.family_index, 1, &m_data->transfer_queue.queue);
            }
            // Otherwise, it is in a different family, so the index is 0
            else
            {
                vkGetDeviceQueue(m_data->device, m_data->transfer_queue.family_index, 0, &m_data->transfer_queue.queue);
            }
        }

        // endregion

        // --=== Allocator ===--

        m_data->allocator = std::move(Allocator(m_data->vk_instance,
                                                m_data->device,
                                                m_data->physical_device,
                                                m_data->graphics_queue.family_index,
                                                m_data->transfer_queue.family_index));

        // --=== XR ===--

        // Create graphics binding
        m_data->graphics_binding = XrGraphicsBindingVulkanKHR {
            .type             = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
            .next             = XR_NULL_HANDLE,
            .instance         = m_data->vk_instance,
            .physicalDevice   = m_data->physical_device,
            .device           = m_data->device,
            .queueFamilyIndex = m_data->graphics_queue.family_index,
            .queueIndex       = 0,
        };

        // Finish setup and create the session
        parent.finish_setup(&m_data->graphics_binding);
    }

    XrRenderer::XrRenderer(const XrRenderer &other)
    {
        // Copy data
        m_data = other.m_data;

        // Increase reference count
        m_data->reference_count++;
    }

    XrRenderer::XrRenderer(XrRenderer &&other) noexcept
    {
        // Move data
        m_data = other.m_data;

        // Reset other data
        other.m_data = nullptr;
    }

    XrRenderer &XrRenderer::operator=(const XrRenderer &other)
    {
        if (this == &other)
        {
            return *this;
        }

        // Call destructor
        this->~XrRenderer();

        // Copy data
        m_data = other.m_data;
        m_data->reference_count++;

        return *this;
    }

    XrRenderer &XrRenderer::operator=(XrRenderer &&other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        // Call destructor
        this->~XrRenderer();

        // Move data
        m_data       = other.m_data;
        other.m_data = nullptr;

        return *this;
    }

    XrRenderer::~XrRenderer()
    {
        if (m_data)
        {
            m_data->reference_count--;

            if (m_data->reference_count == 0)
            {
                // Wait
                vk_check(vkDeviceWaitIdle(m_data->device), "Failed to wait for device to become idle");

                // Destroy XR system, so that it releases its Vulkan resources
                m_data->xr_system.~XrSystem();
                m_data->xr_system = {};

                // Destroy allocator
                m_data->allocator.~Allocator();

                vkDestroyDevice(m_data->device, nullptr);

#ifdef USE_VK_VALIDATION_LAYERS
                vkDestroyDebugUtilsMessengerEXT(m_data->vk_instance, m_data->debug_messenger, nullptr);
#endif
                vkDestroyInstance(m_data->vk_instance, nullptr);

                delete m_data;
            }

            m_data = nullptr;
        }
    }

    // endregion

    // region OpenXR API

    const char *XrRenderer::get_required_openxr_extension()
    {
        return XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
    }

    void *XrRenderer::graphics_binding() const
    {
        check(m_data, "Invalid renderer");
        return &m_data->graphics_binding;
    }

    // endregion

} // namespace xre
#endif