#pragma once

#include <cstdint>

// OpenXR forward declarations
typedef struct XrInstance_T *XrInstance;
typedef uint64_t XrSystemId;

namespace vre
{
    struct Settings;
    class VrRenderer;
    class Window;
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

        void create_renderer(const Settings &settings, Window *mirror_window = nullptr);

        ~VrSystem();
    };

} // namespace vre