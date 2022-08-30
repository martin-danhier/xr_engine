#pragma once

#include <cstdint>
#include <vector>

// Forward declarations for vulkan
#ifdef RENDERER_VULKAN
typedef struct VkInstance_T   *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
#endif

namespace xre
{
    struct Settings;

    /**
     * Handle to a flat-screen window, that can be used for mirroring.
     * The handle works as a shared pointer, so all copies will still refer to the same window.
     * */
    class Window
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        Window() = default;
        explicit Window(const Settings &settings);

        // Create a new handle that points to the same window
        Window(const Window &other);
        Window(Window &&other) noexcept;
        ~Window();

        Window &operator=(const Window &other);
        Window &operator=(Window &&other) noexcept;

        /**
         * @return true if the window is valid, false otherwise
         * */
        [[nodiscard]] bool is_valid() const noexcept;

#ifdef RENDERER_VULKAN
        void get_required_vulkan_extensions(std::vector<const char *> &out_extensions) const;
#endif
    };
} // namespace xre