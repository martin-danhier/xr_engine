/**
 * @brief Implementation of core OpenXR features
 * @author Martin Danhier
 */

#ifdef XR_OPENXR

#include "xr_engine/core/xr/xr_system.h"

#ifdef RENDERER_VULKAN
#include <vulkan/vulkan.h>
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <vector>
#include <xr_engine/core/global.h>
#include <xr_engine/utils/global_utils.h>

#ifndef _MSC_VER
// Not used with MSVC
#include <cstring>
#endif

// Dynamic functions

#ifdef USE_OPENXR_DEBUG_UTILS
PFN_xrCreateDebugUtilsMessengerEXT  xrCreateDebugUtilsMessengerEXT  = nullptr;
PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT = nullptr;
#endif

#ifdef RENDERER_VULKAN
PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = nullptr;
PFN_xrGetVulkanGraphicsDevice2KHR       xrGetVulkanGraphicsDevice2KHR       = nullptr;
PFN_xrCreateVulkanInstanceKHR           xrCreateVulkanInstanceKHR           = nullptr;
PFN_xrCreateVulkanDeviceKHR             xrCreateVulkanDeviceKHR             = nullptr;
#endif

namespace xre
{
    // ---=== Structs ===---

    struct XrSystem::Data
    {
        uint8_t reference_count = 0;

        XrInstance instance = XR_NULL_HANDLE;
#ifdef USE_OPENXR_DEBUG_UTILS
        XrDebugUtilsMessengerEXT debug_messenger = XR_NULL_HANDLE;
#endif
        XrSystemId system_id       = XR_NULL_SYSTEM_ID;
        XrSession  session         = XR_NULL_HANDLE;
        bool       session_running = false;

#ifdef RENDERER_VULKAN
        // Data passed by the renderer required for the binding between OpenXR and Vulkan
        XrGraphicsBindingVulkan2KHR graphics_binding = {XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR, nullptr};
#endif
    };

    // ---=== Utils ===---

    namespace xr
    {
        std::string xr_result_to_string(XrResult result)
        {
            switch (result)
            {
                case XR_SUCCESS: return "XR_SUCCESS";
                case XR_ERROR_API_VERSION_UNSUPPORTED: return "XR_ERROR_API_VERSION_UNSUPPORTED";
                case XR_ERROR_FUNCTION_UNSUPPORTED: return "XR_ERROR_FUNCTION_UNSUPPORTED";
                case XR_ERROR_GRAPHICS_DEVICE_INVALID: return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
                default: return std::to_string(result);
            }
        }

        void xr_check(XrResult result, const std::string &error_message = "")
        {
            // Warnings

            // Errors
            if (result != XR_SUCCESS)
            {
                // Pretty print error
                std::cerr << "[OpenXR Error] An OpenXR function call returned XrResult = " << xr_result_to_string(result) << "\n";

                // Optional custom error message precision
                if (!error_message.empty())
                {
                    std::cerr << "Precision: " << error_message << "\n";
                }
            }
        }

        Version make_version(const XrVersion &version)
        {
            return Version {static_cast<uint8_t>(XR_VERSION_MAJOR(version)),
                            static_cast<uint8_t>(XR_VERSION_MINOR(version)),
                            static_cast<uint16_t>(XR_VERSION_PATCH(version))};
        }

        bool check_xr_instance_extension_support(const std::vector<const char *> &desired_extensions)
        {
            // Get the number of available desired_extensions
            uint32_t available_extensions_count = 0;
            xr_check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &available_extensions_count, nullptr));
            // Create an array with enough room and fetch the available desired_extensions
            std::vector<XrExtensionProperties> available_extensions(available_extensions_count, {XR_TYPE_EXTENSION_PROPERTIES});
            xr_check(xrEnumerateInstanceExtensionProperties(nullptr,
                                                            available_extensions_count,
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
            // Get the number of available API layers
            uint32_t available_layers_count = 0;
            xr_check(xrEnumerateApiLayerProperties(0, &available_layers_count, nullptr));
            // Create an array with enough room and fetch the available layers
            std::vector<XrApiLayerProperties> available_layers(available_layers_count, {XR_TYPE_API_LAYER_PROPERTIES});
            xr_check(xrEnumerateApiLayerProperties(available_layers_count, &available_layers_count, available_layers.data()));

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
        XrBool32 debug_messenger_callback(XrDebugUtilsMessageSeverityFlagsEXT         message_severity,
                                          XrDebugUtilsMessageTypeFlagsEXT             message_types,
                                          const XrDebugUtilsMessengerCallbackDataEXT *callback_data,
                                          void                                       *_)
        {
            // Inspired by VkBootstrap's default debug messenger. (Made by Charles Giessen)
            // Get severity
            const char *str_severity;
            switch (message_severity)
            {
                case XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: str_severity = "VERBOSE"; break;
                case XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: str_severity = "ERROR"; break;
                case XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: str_severity = "WARNING"; break;
                case XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: str_severity = "INFO"; break;
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
            auto &output = message_severity == XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? std::cerr : std::cout;
            output << "[OpenXR " << str_severity << ": " << str_type << "]\n" << callback_data->message << "\n";

            return XR_FALSE;
        }
    } // namespace xr
    using namespace xr;

    // ---=== Manager API ===---

    XrSystem::XrSystem(const Settings &settings) : m_data(new Data)
    {
        std::cout << "Using OpenXR, version " << make_version(XR_CURRENT_API_VERSION) << "\n";

        // === Create instance ===
        {
            XrApplicationInfo xr_app_info {
                "",
                settings.application_info.version.to_uint32(),
                ENGINE_NAME,
                ENGINE_VERSION.to_uint32(),
                XR_CURRENT_API_VERSION,
            };
            // Copy title, using strcpy_s if available
#ifdef _MSC_VER
            check(strcpy_s(xr_app_info.applicationName, settings.application_info.name) == 0, "Failed to copy application name");
#else
            check(strcpy(xr_app_info.applicationName, settings.application_info.name) != nullptr, "Failed to copy application name");
#endif

            // Create instance
            std::vector<const char *> required_extensions = {
#ifdef USE_OPENXR_DEBUG_UTILS
                XR_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
#ifdef RENDERER_VULKAN
                XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME,
#endif
            };

            check(check_xr_instance_extension_support(required_extensions), "Not all required OpenXR extensions are supported.");

#ifdef USE_OPENXR_DEBUG_UTILS
            std::vector<const char *> enabled_layers;
            enabled_layers.push_back("XR_APILAYER_LUNARG_core_validation");
            check(check_layer_support(enabled_layers), "OpenXR validation layers requested, but not available.");
#endif

            XrInstanceCreateInfo instance_create_info {
                .type            = XR_TYPE_INSTANCE_CREATE_INFO,
                .next            = XR_NULL_HANDLE,
                .applicationInfo = xr_app_info,
                // Layers
#ifdef USE_OPENXR_DEBUG_UTILS
                .enabledApiLayerCount = static_cast<uint32_t>(enabled_layers.size()),
                .enabledApiLayerNames = enabled_layers.data(),
#else
                .enabledApiLayerCount = 0,
                .enabledApiLayerNames = XR_NULL_HANDLE,
#endif
                // Extensions
                .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
                .enabledExtensionNames = required_extensions.data(),
            };
            xr_check(xrCreateInstance(&instance_create_info, &m_data->instance), "Failed to create instance");

            // Print runtime name
            XrInstanceProperties instance_properties {
                .type = XR_TYPE_INSTANCE_PROPERTIES,
            };
            xr_check(xrGetInstanceProperties(m_data->instance, &instance_properties), "Failed to get instance properties");

            std::cout << "Using runtime \"" << instance_properties.runtimeName << "\", version "
                      << make_version(instance_properties.runtimeVersion) << "\n";
        }

        // === Load dynamic functions ===
        {
#ifdef USE_OPENXR_DEBUG_UTILS
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrCreateDebugUtilsMessengerEXT",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrCreateDebugUtilsMessengerEXT)),
                     "Failed to load xrCreateDebugUtilsMessengerEXT");
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrDestroyDebugUtilsMessengerEXT",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrDestroyDebugUtilsMessengerEXT)),
                     "Failed to load xrDestroyDebugUtilsMessengerEXT");
#endif
#ifdef RENDERER_VULKAN
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrGetVulkanGraphicsDevice2KHR",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrGetVulkanGraphicsDevice2KHR)),
                     "Failed to load xrGetVulkanGraphicsDevice2KHR");
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrCreateVulkanInstanceKHR",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrCreateVulkanInstanceKHR)),
                     "Failed to load xrCreateVulkanInstanceKHR");
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrCreateVulkanDeviceKHR",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrCreateVulkanDeviceKHR)),
                     "Failed to load xrCreateVulkanDeviceKHR");
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrGetVulkanGraphicsRequirements2KHR",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrGetVulkanGraphicsRequirements2KHR)),
                     "Failed to load xrGetVulkanGraphicsRequirements2KHR");
#endif
        }

        // === Create debug messenger ===
#ifdef USE_OPENXR_DEBUG_UTILS
        {
            XrDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
                // Struct info
                .type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .next = XR_NULL_HANDLE,
                // Message settings
                .messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                .messageTypes      = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                // Callback
                .userCallback = static_cast<PFN_xrDebugUtilsMessengerCallbackEXT>(debug_messenger_callback),
            };
            xr_check(xrCreateDebugUtilsMessengerEXT(m_data->instance, &debug_messenger_create_info, &m_data->debug_messenger),
                     "Failed to create debug messenger");
        }
#endif

        // === Get system ===
        {
            XrSystemGetInfo system_info {
                .type = XR_TYPE_SYSTEM_GET_INFO,
                .next = XR_NULL_HANDLE,
                // We only support headsets
                .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
            };
            xr_check(xrGetSystem(m_data->instance, &system_info, &m_data->system_id), "Failed to get system");

            // Print system info
            XrSystemProperties system_properties {
                .type = XR_TYPE_SYSTEM_PROPERTIES,
                .next = XR_NULL_HANDLE,
            };
            xr_check(xrGetSystemProperties(m_data->instance, m_data->system_id, &system_properties),
                     "Failed to get system properties");
            std::cout << "System name: " << system_properties.systemName << "\n";
        }

        // Set reference count
        m_data->reference_count = 1;
    }

    XrSystem::XrSystem(const XrSystem &other) : m_data(other.m_data)
    {
        m_data->reference_count++;
    }

    XrSystem::XrSystem(XrSystem &&other) noexcept : m_data(other.m_data)
    {
        other.m_data = nullptr;
    }

    XrSystem &XrSystem::operator=(const XrSystem &other)
    {
        if (this == &other)
        {
            return *this;
        }

        this->~XrSystem();

        m_data = other.m_data;
        m_data->reference_count++;

        return *this;
    }

    XrSystem &XrSystem::operator=(XrSystem &&other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        this->~XrSystem();

        m_data       = other.m_data;
        other.m_data = nullptr;

        return *this;
    }

    XrSystem::~XrSystem()
    {
        if (m_data)
        {
            // Decrement reference count
            m_data->reference_count--;

            if (m_data->reference_count == 0)
            {
                if (m_data->session != XR_NULL_HANDLE)
                {
                    xr_check(xrDestroySession(m_data->session), "Failed to destroy session");
                }

#ifdef USE_OPENXR_DEBUG_UTILS
                if (m_data->debug_messenger != XR_NULL_HANDLE)
                {
                    xr_check(xrDestroyDebugUtilsMessengerEXT(m_data->debug_messenger), "Failed to destroy debug messenger");
                }
#endif
                if (m_data->instance != XR_NULL_HANDLE)
                {
                    xr_check(xrDestroyInstance(m_data->instance), "Failed to destroy instance");
                }

                delete m_data;
            }

            m_data = nullptr;
        }
    }

    void XrSystem::finish_setup()
    {
        // Create session
        {
            XrSessionCreateInfo session_create_info {
                .type = XR_TYPE_SESSION_CREATE_INFO,
#ifdef RENDERER_VULKAN
                // We need to give the binding so that OpenXR knows about our Vulkan setup
                .next = &m_data->graphics_binding,
#else
                .next = XR_NULL_HANDLE,
#endif
                .systemId = m_data->system_id,
            };
            xr_check(xrCreateSession(m_data->instance, &session_create_info, &m_data->session),
                     "Failed to create session. Is the headset plugged in?");
        }
    }

#ifdef RENDERER_VULKAN

    VulkanCompatibility XrSystem::get_vulkan_compatibility() const
    {
        XrGraphicsRequirementsVulkanKHR requirements {
            .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
            .next = XR_NULL_HANDLE,
        };
        xr_check(xrGetVulkanGraphicsRequirements2KHR(m_data->instance, m_data->system_id, &requirements),
                 "Failed to get Vulkan requirements");

        return VulkanCompatibility {
            .min_version = make_version(requirements.minApiVersionSupported),
            .max_version = make_version(requirements.maxApiVersionSupported),
        };
    }

    int XrSystem::create_vulkan_instance(const VkInstanceCreateInfo &create_info,
                                         PFN_vkGetInstanceProcAddr   vk_get_instance_proc_addr,
                                         VkInstance                 &out_instance) const
    {
        XrVulkanInstanceCreateInfoKHR vulkan_instance_create_info {XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        vulkan_instance_create_info.systemId               = m_data->system_id;
        vulkan_instance_create_info.pfnGetInstanceProcAddr = vk_get_instance_proc_addr;
        vulkan_instance_create_info.vulkanCreateInfo       = &create_info;

        // Create instance
        VkResult result;
        xr_check(xrCreateVulkanInstanceKHR(m_data->instance, &vulkan_instance_create_info, &out_instance, &result),
                 "Failed to create Vulkan instance");
        m_data->graphics_binding.instance = out_instance;
        return result;
    }

    VkPhysicalDevice XrSystem::get_vulkan_physical_device() const
    {
        if (!m_data)
        {
            throw std::runtime_error("XrManager not initialized");
        }

        // Find physical device
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;

        XrVulkanGraphicsDeviceGetInfoKHR graphics_device_get_info {
            .type           = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
            .next           = XR_NULL_HANDLE,
            .systemId       = m_data->system_id,
            .vulkanInstance = m_data->graphics_binding.instance,
        };

        xr_check(xrGetVulkanGraphicsDevice2KHR(m_data->instance, &graphics_device_get_info, &physical_device),
                 "Failed to get Vulkan graphics device");

        m_data->graphics_binding.physicalDevice = physical_device;

        return physical_device;
    }

    int XrSystem::create_vulkan_device(const VkDeviceCreateInfo &create_info,
                                       PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr,
                                       VkDevice                 &out_device) const
    {
        XrVulkanDeviceCreateInfoKHR vulkan_device_create_info {
            .type                   = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
            .next                   = XR_NULL_HANDLE,
            .systemId               = m_data->system_id,
            .pfnGetInstanceProcAddr = vk_get_instance_proc_addr,
            .vulkanPhysicalDevice   = m_data->graphics_binding.physicalDevice,
            .vulkanCreateInfo       = &create_info,
        };

        // Create device
        VkResult result;
        xr_check(xrCreateVulkanDeviceKHR(m_data->instance, &vulkan_device_create_info, &out_device, &result),
                 "Failed to create Vulkan device");
        m_data->graphics_binding.device = out_device;

        return result;
    }

    void XrSystem::register_graphics_queue(uint32_t queue_family, uint32_t queue_index) const
    {
        m_data->graphics_binding.queueFamilyIndex = queue_family;
        m_data->graphics_binding.queueIndex       = queue_index;
    }
#endif
} // namespace xre

#endif