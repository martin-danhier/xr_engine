#pragma once

#include <string>
#include <vector>

#ifdef RENDERER_VULKAN
#include <vulkan/vk_platform.h>
typedef struct VkInstance_T       *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef struct VkDevice_T         *VkDevice;
typedef void(VKAPI_PTR *PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction(VKAPI_PTR *PFN_vkGetInstanceProcAddr)(VkInstance instance, const char *pName);
struct VkInstanceCreateInfo;
struct VkDeviceCreateInfo;
#endif

namespace xre
{
    struct Settings;
#ifdef RENDERER_VULKAN
    struct VulkanCompatibility;
#endif

    /** An XR system represents the headset, the display, the controllers, etc.
     * It is through this object that the application can interact with the headset.
     *
     * There can be only one system at a time.
     *
     * Like many classes in the XR engine, it is actually a shared pointer, so it can be copied and passed around and still point to
     * the same system.
     */
    class XrSystem
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        XrSystem() = default;
        explicit XrSystem(const Settings &settings);
        XrSystem(const XrSystem &other);
        XrSystem(XrSystem &&other) noexcept;
        XrSystem &operator=(const XrSystem &other);
        XrSystem &operator=(XrSystem &&other) noexcept;

        ~XrSystem();


      private:
        friend class Renderer;

        void finish_setup();

#ifdef RENDERER_VULKAN

        [[nodiscard]] VulkanCompatibility get_vulkan_compatibility() const;
        int                               create_vulkan_instance(const VkInstanceCreateInfo &create_info,
                                                                 PFN_vkGetInstanceProcAddr   vk_get_instance_proc_addr,
                                                                 VkInstance                 &out_instance) const;
        int                               create_vulkan_device(const VkDeviceCreateInfo &create_info,
                                                               PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr,
                                                               VkDevice                 &out_device) const;

        [[nodiscard]] VkPhysicalDevice get_vulkan_physical_device() const;
        void                           register_graphics_queue(uint32_t queue_family, uint32_t queue_index) const;
#endif
    };
} // namespace xre