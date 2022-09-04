#include "vr_engine/core/vr/vr_system.h"

#include <vector>
#include <vr_engine/core/global.h>
#include <vr_engine/core/vr/vr_renderer.h>
#include <vr_engine/utils/global_utils.h>
#include <vr_engine/utils/openxr_utils.h>

#ifndef _MSC_VER
// Not used with MSVC
#include <cstring>
#endif

namespace vre
{
    // ---=== Constants ===---

#define FORM_FACTOR             XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY

    constexpr XrPosef XR_POSE_IDENTITY = {{0.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 0.f}};

    // ---=== Function pointers ===---

#ifdef USE_OPENXR_VALIDATION_LAYERS
    PFN_xrCreateDebugUtilsMessengerEXT  xrCreateDebugUtilsMessengerEXT  = nullptr;
    PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT = nullptr;
#endif

    // --=== Structs ===---

    struct VrSystem::Data
    {
        uint8_t    reference_count = 0;
        VrRenderer renderer        = {};

        XrInstance instance = XR_NULL_HANDLE;
#ifdef USE_OPENXR_VALIDATION_LAYERS
        XrDebugUtilsMessengerEXT debug_messenger = XR_NULL_HANDLE;
#endif
        XrSystemId system_id       = XR_NULL_SYSTEM_ID;
        XrSession  session         = XR_NULL_HANDLE;
        bool       session_running = false;

        XrSpace reference_space = XR_NULL_HANDLE;
    };

    // ---=== Utils ===---

    namespace xr
    {
        // region Conversions

        std::string xr_reference_space_type_to_string(XrReferenceSpaceType space_type)
        {
            switch (space_type)
            {
                case XR_REFERENCE_SPACE_TYPE_STAGE: return "Stage";
                case XR_REFERENCE_SPACE_TYPE_LOCAL: return "Local";
                case XR_REFERENCE_SPACE_TYPE_VIEW: return "View";
                default: return std::to_string(space_type);
            }
        }

        // endregion

        // Check support

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

        XrReferenceSpaceType choose_reference_space_type(XrSession session)
        {
            // Define the priority of each space type. We will take the first one that is available.
            XrReferenceSpaceType space_type_preference[] = {
                XR_REFERENCE_SPACE_TYPE_STAGE, // Based on play area center (most common for room-scale VR)
                XR_REFERENCE_SPACE_TYPE_LOCAL, // Based on starting location
            };

            // Get available space types
            uint32_t available_spaces_count = 0;
            xr_check(xrEnumerateReferenceSpaces(session, 0, &available_spaces_count, nullptr));
            std::vector<XrReferenceSpaceType> available_spaces(available_spaces_count);
            xr_check(xrEnumerateReferenceSpaces(session, available_spaces_count, &available_spaces_count, available_spaces.data()));

            // Choose the first available space type
            for (const auto &space_type : space_type_preference)
            {
                for (const auto &available_space : available_spaces)
                {
                    if (space_type == available_space)
                    {
                        return space_type;
                    }
                }
            }

            check(false, "No supported reference space type found.");
            throw;
        }

        // endregion

        // region Debug utils

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

        // endregion

    } // namespace xr
    using namespace xr;

    // --=== API ===--

    VrSystem::VrSystem(const Settings &settings) : m_data(new Data)
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
#ifdef USE_OPENXR_VALIDATION_LAYERS
                XR_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
                VrRenderer::get_required_openxr_extension(),
            };

            check(check_xr_instance_extension_support(required_extensions), "Not all required OpenXR extensions are supported.");

#ifdef USE_OPENXR_VALIDATION_LAYERS
            std::vector<const char *> enabled_layers;
            enabled_layers.push_back("XR_APILAYER_LUNARG_core_validation");
            check(check_layer_support(enabled_layers), "OpenXR validation layers requested, but not available.");
#endif

            XrInstanceCreateInfo instance_create_info {
                .type            = XR_TYPE_INSTANCE_CREATE_INFO,
                .next            = XR_NULL_HANDLE,
                .applicationInfo = xr_app_info,
            // Layers
#ifdef USE_OPENXR_VALIDATION_LAYERS
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
#ifdef USE_OPENXR_VALIDATION_LAYERS
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrCreateDebugUtilsMessengerEXT",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrCreateDebugUtilsMessengerEXT)),
                     "Failed to load xrCreateDebugUtilsMessengerEXT");
            xr_check(xrGetInstanceProcAddr(m_data->instance,
                                           "xrDestroyDebugUtilsMessengerEXT",
                                           reinterpret_cast<PFN_xrVoidFunction *>(&xrDestroyDebugUtilsMessengerEXT)),
                     "Failed to load xrDestroyDebugUtilsMessengerEXT");
#endif
        }

        // === Create debug messenger ===
#ifdef USE_OPENXR_VALIDATION_LAYERS
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
                .formFactor = FORM_FACTOR,
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

        // Init reference count
        m_data->reference_count = 1;
    }

    VrSystem::VrSystem(const VrSystem &other)
    {
        m_data = other.m_data;

        // Increment reference count
        m_data->reference_count++;
    }

    VrSystem::VrSystem(VrSystem &&other) noexcept
    {
        m_data = other.m_data;

        // Take the other's data
        other.m_data = nullptr;
    }

    VrSystem &VrSystem::operator=(const VrSystem &other)
    {
        if (this == &other)
        {
            return *this;
        }

        // Decrement reference count
        this->~VrSystem();

        // Copy data
        m_data = other.m_data;
        m_data->reference_count++;

        return *this;
    }

    VrSystem &VrSystem::operator=(VrSystem &&other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        // Decrement reference count
        this->~VrSystem();

        // Take the other's data
        m_data       = other.m_data;
        other.m_data = nullptr;

        return *this;
    }

    VrSystem::~VrSystem()
    {
        if (m_data)
        {
            m_data->reference_count--;

            if (m_data->reference_count == 0)
            {
                if (m_data->renderer.is_valid()) {
                    m_data->renderer.wait_idle();
                    m_data->renderer.cleanup_vr_views();
                }

                if (m_data->reference_space != XR_NULL_HANDLE)
                {
                    xr_check(xrDestroySpace(m_data->reference_space), "Failed to destroy reference space");
                }

                if (m_data->session != XR_NULL_HANDLE)
                {
                    xr_check(xrDestroySession(m_data->session), "Failed to destroy session");
                }

#ifdef USE_OPENXR_VALIDATION_LAYERS
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

    // --=== Friend API ===--

    // region Friend API

    void VrSystem::create_renderer(const Settings &settings, Window *mirror_window)
    {
        check(!m_data->renderer.is_valid(), "Renderer already created");
        m_data->renderer = VrRenderer(m_data->instance, m_data->system_id, settings, mirror_window);

        // Create session
        {
            XrSessionCreateInfo session_create_info {
                .type = XR_TYPE_SESSION_CREATE_INFO,
                // We need to give the binding so that OpenXR knows about our Vulkan setup
                .next        = m_data->renderer.graphics_binding(),
                .createFlags = 0,
                .systemId    = m_data->system_id,
            };
            xr_check(xrCreateSession(m_data->instance, &session_create_info, &m_data->session),
                     "Failed to create session. Is the headset plugged in?");
        }

        // Create reference space
        {
            auto space_type = choose_reference_space_type(m_data->session);

            // Print space type
            std::cout << "Chosen space type: " << xr_reference_space_type_to_string(space_type) << std::endl;

            // Create space
            XrReferenceSpaceCreateInfo ref_space_info {
                .type                 = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                .next                 = XR_NULL_HANDLE,
                .referenceSpaceType   = space_type,
                .poseInReferenceSpace = XR_POSE_IDENTITY,
            };
            xr_check(xrCreateReferenceSpace(m_data->session, &ref_space_info, &m_data->reference_space),
                     "Failed to create reference space");
        }

        // Init VR views (swapchains)
        m_data->renderer.init_vr_views(m_data->session);
    }

    // endregion
} // namespace vre