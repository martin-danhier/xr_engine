#pragma once

#include "vr_engine/core/vr_renderer.h"

#include <cstdint>

// OpenXR forward declarations
typedef struct XrInstance_T *XrInstance;
typedef uint64_t XrSystemId;

namespace vre
{
    struct Settings;
    class VrRenderer;
    class VrSystem
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        // Shared pointer logic
        VrSystem() = default;
        explicit VrSystem(const Settings &settings);
        VrSystem(const VrSystem &other);
        VrSystem(VrSystem &&other) noexcept;
        VrSystem                 &operator=(const VrSystem &other);
        VrSystem                 &operator=(VrSystem &&other) noexcept;
        [[nodiscard]] inline bool is_valid() const {
            return m_data != nullptr;
        }

        VrRenderer create_renderer(const Settings &settings, Window *mirror_window = nullptr);

        ~VrSystem();
      private:
        friend VrRenderer;

        [[nodiscard]] XrInstance instance() const;
        [[nodiscard]] XrSystemId system_id() const;

        void finish_setup(void *graphics_binding) const;

    };

} // namespace vre