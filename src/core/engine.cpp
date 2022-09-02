#include "xr_engine/core/engine.h"

#include <algorithm>
#include <xr_engine/core/global.h>
#include <xr_engine/core/xr_renderer.h>
#include <xr_engine/core/window.h>
#include <xr_engine/core/xr/xr_system.h>

namespace xre
{
    // --=== Structs ===---

    struct Engine::Data
    {
        uint8_t reference_count = 0;

        Settings settings      = {};
        XrRenderer renderer      = {};
        XrSystem xr_system     = {};
        Window   mirror_window = {};

        ~Data()
        {
            xr_system.~XrSystem();
            renderer.~XrRenderer();
            mirror_window.~Window();
        }
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
            auto window = settings.mirror_window_settings.enabled ? &m_data->mirror_window : nullptr;
            m_data->renderer = m_data->xr_system.create_renderer( settings, window);

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

    void Engine::run_main_loop()
    {
        if (m_data == nullptr)
        {
            throw std::runtime_error("Engine not initialized");
        }

        // Init variables
        bool     should_quit        = false;
        uint64_t current_frame_time = 0;
        double   delta_time         = 0.0;

        // Register close event
//        m_data->window.on_close()->subscribe([&should_quit](std::nullptr_t _) { should_quit = true; });

#ifdef NO_INTERACTIVE
        // In tests, we want a timeout
        uint64_t timeout = 5000;

        // Run sleep in another thread, then send event
        std::thread(
            [&timeout, &should_quit]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                should_quit = true;
            })
            .detach();
#endif

        // Main loop
        while (!should_quit)
        {
            // Update delta time
//            delta_time = Window::compute_delta_time(&current_frame_time);

            // TODO temp
            // Handle events
            should_quit = m_data->mirror_window.handle_events();

            // Run rendering
//            m_data->renderer.draw();

            // Trigger update
//            m_data->update_event.send(delta_time);
        }
    }

} // namespace xre
