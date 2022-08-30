#pragma once

#include <xr_engine/core/window.h>

namespace xre
{
    class XrSystem;
    struct Settings;

    /** The renderer handles the graphics part of the application.
     *
     * It renders to the given XR system, and mirrors the output on the given window.
     *
     * It is a shared pointer, so it can be copied and passed around.
     */
    class Renderer
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        Renderer() = default;
        /**
         * Create a new renderer that will render to the given XR system.
         * If no window is provided, the renderer will not mirror the output.
         */
        Renderer(const XrSystem &target_xr_system, const Settings &settings, Window mirror_window = {});
        Renderer(const Renderer &other);
        Renderer(Renderer &&other) noexcept;
        Renderer &operator=(const Renderer &other);
        Renderer &operator=(Renderer &&other) noexcept;
        ~Renderer();
    };
} // namespace xre
