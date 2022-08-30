#include "xr_engine/core/engine.h"

#include <algorithm>
#include <xr_engine/core/global.h>
#include <xr_engine/core/renderer.h>
#include <xr_engine/core/window.h>
#include <xr_engine/core/xr/xr_system.h>

namespace xre
{
    // --=== Structs ===---

    struct Engine::Data
    {
        uint8_t reference_count = 0;

        Settings settings      = {};
        Renderer renderer      = {};
        XrSystem xr_system     = {};
        Window   mirror_window = {};
    };

    // --=== API ===--

    Engine::Engine(const Settings &settings)
    {
        m_data = new Data {
            .reference_count = 1,
            .settings        = settings,
        };

        try
        {
            // Create XR system
            m_data->xr_system = XrSystem(settings);

            // If needed, create mirror window
            if (settings.mirror_window_settings.enabled) {
                m_data->mirror_window = Window(settings);
            }

            // Create renderer
            m_data->renderer = Renderer(m_data->xr_system, settings, m_data->mirror_window);

        }
        catch (const std::exception &e)
        {
            // If something goes wrong, clean up
            delete m_data;
            m_data = nullptr;
            throw e;
        }
    }

    Engine::Engine(const Engine &other)
    {
        m_data = other.m_data;
        m_data->reference_count++;
    }

    Engine::Engine(Engine &&other) noexcept
    {
        m_data       = other.m_data;
        other.m_data = nullptr;
    }

    Engine &Engine::operator=(const Engine &other)
    {
        if (this != &other)
        {
            // Call destructor
            this->~Engine();

            // Copy data
            m_data = other.m_data;
            m_data->reference_count++;
        }
        return *this;
    }

    Engine &Engine::operator=(Engine &&other) noexcept
    {
        if (this != &other)
        {
            // Call destructor
            this->~Engine();

            // Take other's data
            m_data       = other.m_data;
            other.m_data = nullptr;
        }
        return *this;
    }

    Engine::~Engine()
    {
        if (m_data)
        {
            m_data->reference_count--;

            if (m_data->reference_count == 0)
            {
                delete m_data;
            }

            m_data = nullptr;
        }
    }

} // namespace xre
