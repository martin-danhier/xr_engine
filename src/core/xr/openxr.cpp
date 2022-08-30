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
PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR = nullptr;
PFN_xrGetVulkanGraphicsDeviceKHR       xrGetVulkanGraphicsDeviceKHR       = nullptr;
PFN_xrGetVulkanInstanceExtensionsKHR   xrGetVulkanInstanceExtensionsKHR   = nullptr;
PFN_xrGetVulkanDeviceExtensionsKHR     xrGetVulkanDeviceExtensionsKHR     = nullptr;
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
        XrSystemId system_id = XR_NULL_SYSTEM_ID;
        XrSession  session   = XR_NULL_HANDLE;
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
        std::cout << "Using OpenXR, version " << XR_VERSION_MAJOR(XR_CURRENT_API_VERSION) << "."
                  << XR_VERSION_MINOR(XR_CURRENT_API_VERSION) << "\n";

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
            check(strcpy_s(xr_app_info.applicationName, application_info.name) == 0, "Failed to copy application name");
#else
            check(strcpy(xr_app_info.applicationName, settings.application_info.name) != nullptr, "Failed to copy application name");
#endif

            // Create instance
            std::vector<const char *> required_extensions = {
#ifdef USE_OPENXR_DEBUG_UTILS
                XR_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
#ifdef RENDERER_VULKAN
                XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,
#endif
            };

            check(check_xr_instance_extension_support(required_extensions), "Not all required OpenXR extensions are supported.");

            XrInstanceCreateInfo instance_create_info {
                .type            = XR_TYPE_INSTANCE_CREATE_INFO,
                .next            = XR_NULL_HANDLE,
                .applicationInfo = xr_app_info,
                // Layers
                .enabledApiLayerCount = 0,
                .enabledApiLayerNames = XR_NULL_HANDLE,
                // Extensions
                .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
                .enabledExtensionNames = required_extensions.data(),
            };
            xr_check(xrCreateInstance(&instance_create_info, &m_data->instance), "Failed to create instance");
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
                                           "xrGetVulkanInstanceExtensionsKHR",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrGetVulkanInstanceExtensionsKHR)),
                     "Failed to load xrGetVulkanInstanceExtensionsKHR");
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrGetVulkanDeviceExtensionsKHR",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrGetVulkanDeviceExtensionsKHR)),
                     "Failed to load xrGetVulkanDeviceExtensionsKHR");
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrGetVulkanGraphicsDeviceKHR",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrGetVulkanGraphicsDeviceKHR)),
                     "Failed to load xrGetVulkanGraphicsDeviceKHR");
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrGetVulkanGraphicsRequirementsKHR",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrGetVulkanGraphicsRequirementsKHR)),
                     "Failed to load xrGetVulkanGraphicsRequirementsKHR");
#endif
        }

        // === Create debug messenger ===
#ifdef USE_OPENXR_DEBUG_UTILS
        {
            XrDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
                // Struct info
                .type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .next = VK_NULL_HANDLE,
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
#ifdef USE_OPENXR_DEBUG_UTILS
                xr_check(xrDestroyDebugUtilsMessengerEXT(m_data->debug_messenger), "Failed to destroy debug messenger");
#endif
                xr_check(xrDestroyInstance(m_data->instance), "Failed to destroy instance");

                delete m_data;
            }

            m_data = nullptr;
        }
    }

#ifdef RENDERER_VULKAN
    VulkanCompatibility XrSystem::get_vulkan_compatibility() const
    {
        if (!m_data)
        {
            throw std::runtime_error("XrManager not initialized");
        }

        // Find requirements for renderer
        XrGraphicsRequirementsVulkanKHR graphics_requirements {
            .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
            .next = XR_NULL_HANDLE,
        };
        xr_check(xrGetVulkanGraphicsRequirementsKHR(m_data->instance, m_data->system_id, &graphics_requirements),
                 "Failed to get graphics requirements");

        return VulkanCompatibility {
            .min_version = make_version(graphics_requirements.minApiVersionSupported),
            .max_version = make_version(graphics_requirements.maxApiVersionSupported),
        };
    }

    std::vector<std::string> XrSystem::get_required_vulkan_extensions(std::vector<const char *> &out_extensions) const
    {
        if (!m_data)
        {
            throw std::runtime_error("XrManager not initialized");
        }

        // Get required buffer capacity
        uint32_t capacity = 0;
        xr_check(xrGetVulkanInstanceExtensionsKHR(m_data->instance, m_data->system_id, 0, &capacity, nullptr));
        // Get space-delimited list of extension names in a buffer
        char *buffer = new char[capacity];
        xr_check(xrGetVulkanInstanceExtensionsKHR(m_data->instance, m_data->system_id, capacity, &capacity, buffer));

        std::string_view extensions(buffer, capacity);
        std::vector<std::string> extension_names;
        extension_names.reserve(6);

        // Create new dynamic std::strings for each extension name and add it
        while (!extensions.empty())
        {
            auto space = extensions.find(' ');
            if (space == std::string_view::npos)
            {
                extension_names.emplace_back(extensions);
                out_extensions.push_back(extension_names.back().c_str());
                break;
            }
            else
            {
                extension_names.emplace_back(extensions.substr(0, space));
                out_extensions.push_back(extension_names.back().c_str());
                extensions.remove_prefix(space + 1);
            }
        }

        delete[] buffer;

        return extension_names;
    }
#endif
} // namespace xre

#endif