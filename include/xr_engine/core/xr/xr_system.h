#pragma once

#include <vector>
#include <string>

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

#ifdef RENDERER_VULKAN
        [[nodiscard]] VulkanCompatibility get_vulkan_compatibility() const;
        std::vector<std::string> get_required_vulkan_extensions(std::vector<const char *> &out_extensions) const;
#endif
    };
} // namespace xre