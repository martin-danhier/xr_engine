#pragma once

#include <cstdint>
#include <iostream>

namespace xre
{

    // ---=== Settings ===---

#define ENGINE_NAME "XR_Engine"

#define ENGINE_VERSION \
    xre::Version       \
    {                  \
        0, 1, 0        \
    }

    // ---=== Structs ===---

    struct Version
    {
        const uint8_t  major = 0;
        const uint8_t  minor = 0;
        const uint16_t patch = 0;

        [[nodiscard]] constexpr uint32_t to_uint32() const
        {
            return (major << 16) | (minor << 8) | patch;
        }

        // Compare two versions
        [[nodiscard]] constexpr bool operator==(const Version &other) const
        {
            return to_uint32() == other.to_uint32();
        }

        // Print
        friend std::ostream &operator<<(std::ostream &os, const Version &version);
    };

    struct Extent2D
    {
        uint32_t width  = 0;
        uint32_t height = 0;
    };

    struct ApplicationInfo
    {
        const char   *name    = "";
        const Version version = {0, 0, 0};
    };

    struct MirrorWindowSettings
    {
        bool     enabled = true;
        Extent2D extent  = {500, 500};
    };

    struct Settings
    {
        const ApplicationInfo      application_info       = {};
        const MirrorWindowSettings mirror_window_settings = {};
    };

#ifdef RENDERER_VULKAN
    struct VulkanCompatibility
    {
        Version min_version;
        Version max_version;
    };
#endif

} // namespace xre