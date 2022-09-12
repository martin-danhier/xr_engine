#pragma once

#include <cstdint>

// OpenXR forward declarations
typedef struct XrSession_T  *XrSession;
typedef struct XrInstance_T *XrInstance;
typedef uint64_t             XrSystemId;

namespace vre
{
    struct Settings;
    class Scene;
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
        /** Create a new renderer */
        VrRenderer(XrInstance      xr_instance,
                   XrSystemId      xr_system_id,
                   const Settings &settings,
                   const Scene    &scene,
                   Window         *mirror_window = nullptr);
        VrRenderer(const VrRenderer &other);
        VrRenderer(VrRenderer &&other) noexcept;
        VrRenderer               &operator=(const VrRenderer &other);
        VrRenderer               &operator=(VrRenderer &&other) noexcept;
        [[nodiscard]] inline bool is_valid() const { return m_data != nullptr; }

        ~VrRenderer();

        // API logic

        /** Returns the name of the extension required for the binding. */
        static const char *get_required_openxr_extension();

        /** Get an OpenXR graphics binding that can be used to open a session */
        [[nodiscard]] void *graphics_binding() const;

        /** Wait until the GPU is idle. Should only be used at the end because it prevents multiple frames from drawing at once. */
        void wait_idle() const;

        /** Loads and inits the available VR views */
        void init_vr_views(XrSession session) const;

        void cleanup_vr_views() const;
    };

} // namespace vre