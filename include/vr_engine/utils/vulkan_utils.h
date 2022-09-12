#pragma once

#ifdef RENDERER_VULKAN

#include <vulkan/vulkan.h>
#include <string>

// --=== Structs ===--
namespace vre
{
    struct VulkanFunctions
    {
        PFN_vkCreateShaderModule  vkCreateShaderModule  = nullptr;
        PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
    };

    struct SceneRendererBinding
    {
        VkDevice        device = VK_NULL_HANDLE;
        VulkanFunctions functions;
    };

    // --=== Functions ===--

    void vk_check(VkResult result, const std::string &error_message = "");

} // namespace vre
#endif