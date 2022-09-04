#pragma once

namespace vre
{
    struct Settings;
    class VrSystem;
    class Window;

    struct VrRenderer
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        // Shared pointer logic

        VrRenderer() = default;
        VrRenderer(const VrRenderer &other);
        VrRenderer(VrRenderer &&other) noexcept;
        VrRenderer               &operator=(const VrRenderer &other);
        VrRenderer               &operator=(VrRenderer &&other) noexcept;
        [[nodiscard]] inline bool is_valid() const {
            return m_data != nullptr;
        }

        ~VrRenderer();

      private:
        // OpenXR API: methods used by other OpenXR classes, but not exposed to the user
        friend VrSystem;

        /** Create a new renderer */
        VrRenderer(const VrSystem &parent, const Settings &settings, Window *mirror_window = nullptr);

        /** Returns the name of the extension required for the binding. */
        static const char *get_required_openxr_extension();

        [[nodiscard]] void *graphics_binding() const;
    };

} // namespace vre