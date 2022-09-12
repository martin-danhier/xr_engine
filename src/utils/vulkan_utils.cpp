#ifdef RENDERER_VULKAN

#include "vr_engine/utils/vulkan_utils.h"
#include <iostream>

namespace vre {

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

    void vk_check(VkResult result, const std::string &error_message)
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
}

#endif