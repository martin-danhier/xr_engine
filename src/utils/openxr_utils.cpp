#include "vr_engine/utils/openxr_utils.h"

#include <iostream>
#include <vr_engine/core/global.h>

namespace vre
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

    void xr_check(XrResult result, const std::string &error_message)
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
} // namespace vre