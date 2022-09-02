#pragma once

namespace xre
{
    struct Settings;
    class XrSystem;
    class Window;

    struct XrRenderer
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        // Shared pointer logic

        XrRenderer() = default;
        XrRenderer(const XrRenderer &other);
        XrRenderer(XrRenderer &&other) noexcept;
        XrRenderer &operator=(const XrRenderer &other);
        XrRenderer &operator=(XrRenderer &&other) noexcept;
        [[nodiscard]] inline bool is_valid() const {
            return m_data != nullptr;
        }

        ~XrRenderer();

      private:
        // OpenXR API: methods used by other OpenXR classes, but not exposed to the user
        friend XrSystem;

        /** Create a new renderer */
        XrRenderer(const XrSystem &parent, const Settings &settings, Window *mirror_window = nullptr);

        /** Returns the name of the extension required for the binding. */
        static const char *get_required_openxr_extension();

        [[nodiscard]] void *graphics_binding() const;
    };

} // namespace xre