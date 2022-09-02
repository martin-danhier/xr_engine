#pragma once

#include "xr_engine/core/xr_renderer.h"
#include <cstdint>

// OpenXR forward declarations
typedef struct XrInstance_T *XrInstance;
typedef uint64_t XrSystemId;

namespace xre
{
    struct Settings;
    class XrRenderer;
    class XrSystem
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        // Shared pointer logic
        XrSystem() = default;
        explicit XrSystem(const Settings &settings);
        XrSystem(const XrSystem &other);
        XrSystem(XrSystem &&other) noexcept;
        XrSystem &operator=(const XrSystem &other);
        XrSystem &operator=(XrSystem &&other) noexcept;
        [[nodiscard]] inline bool is_valid() const {
            return m_data != nullptr;
        }

        XrRenderer create_renderer(const Settings &settings, Window *mirror_window = nullptr);

        ~XrSystem();
      private:
        friend XrRenderer;

        [[nodiscard]] XrInstance instance() const;
        [[nodiscard]] XrSystemId system_id() const;

        void finish_setup(void *graphics_binding) const;

    };

} // namespace xre