#ifdef RENDERER_VULKAN
#include "vr_engine/core/scene.h"

#include <vr_engine/utils/data/storage.h>
#include <vr_engine/utils/global_utils.h>
#include <vr_engine/utils/io.h>
#include <vr_engine/utils/vulkan_utils.h>

namespace vre
{
    // --=== Structs ===--

    struct ShaderModule
    {
        VkShaderModule        module = VK_NULL_HANDLE;
        VkShaderStageFlagBits stage  = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    };

    struct Scene::Data : ISharedPointerData
    {
        VkDevice              device = VK_NULL_HANDLE;
        VulkanFunctions       vk_functions;
        Storage<ShaderModule> shader_modules;

        ~Data() override
        {
            for (auto &shader_module : shader_modules)
            {
                vk_functions.vkDestroyShaderModule(device, shader_module.value().module, nullptr);
            }
        }
    };

    // --=== Utils ===--

    namespace scene_utils
    {
        VkShaderStageFlagBits convert_shader_stage(ShaderStage stage)
        {
            switch (stage)
            {
                case ShaderStage::VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
                case ShaderStage::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
                default: return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
            }
        }
    } // namespace scene_utils
    using namespace scene_utils;

    // --=== API ===--

    Scene Scene::create_scene()
    {
        auto scene                  = Scene();
        scene.m_shared_pointer_data = new Scene::Data();
        return scene;
    }

    void Scene::bind_renderer(const SceneRendererBinding &binding)
    {
        data()->device       = binding.device;
        data()->vk_functions = binding.functions;
    }

    Id Scene::load_shader_module(const char *file_path, ShaderStage stage)
    {
        ShaderModule module = {
            .stage = convert_shader_stage(stage),
        };

        // Load SPIR-V binary from file
        size_t    code_size = 0;
        uint32_t *code      = nullptr;
        try
        {
            code = static_cast<uint32_t *>(load_binary_file(file_path, &code_size));
        }
        catch (const std::exception &e)
        {
            // Always fail for now, later we can simply return invalid id (or let the exception propagate) to let the caller try to
            // recover
            check(false, "Could not load shader module: " + std::string(e.what()));
        }

        // Create shader vk module
        VkShaderModuleCreateInfo shader_module_create_info {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = code_size,
            .pCode    = code,
        };
        VkShaderModule vk_shader_module;
        vk_check(data()->vk_functions.vkCreateShaderModule(data()->device, &shader_module_create_info, nullptr, &vk_shader_module),
                 "Could not create shader module");

        // Free code
        delete[] code;

        // Store shader module
        module.module = vk_shader_module;

        // Add it to storage
        Id id = data()->shader_modules.push(module);

        std::cout << "Loaded shader module " << id << ": " << file_path << "\n";

        // Return the id
        return id;
    }
} // namespace vre

#endif