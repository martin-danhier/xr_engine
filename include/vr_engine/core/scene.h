#pragma once

#include <cstdint>
#include <vr_engine/utils/shared_pointer.h>

namespace vre
{
    enum class ShaderStage
    {
        VERTEX,
        FRAGMENT,
    };

    typedef uint64_t Id;

    /** This struct is used by the renderer to give some information to the scene.
     * For example, the renderer will give the scene the device handle. */
    struct SceneRendererBinding;
    class VrRenderer;

    /** A scene is a collection of objects, shader programs, materials, that are rendered together. */
    class Scene : public ISharedPointer
    {
      private:
        struct Data;
        // Create reference to directly access the data in the right type
        inline Data *&data() { return reinterpret_cast<Data *&>(m_shared_pointer_data); }

      public:
        Scene() = default;
        // For now, since there is no needed parameter, we need a factory to differentiate between the default constructor (null
        // pointer) and create a new empty scene
        static Scene create_scene();

        // Shaders
        Id load_shader_module(const char *file_path, ShaderStage stage);

      private:
        friend VrRenderer;
        void bind_renderer(const SceneRendererBinding &binding);
    };
} // namespace vre