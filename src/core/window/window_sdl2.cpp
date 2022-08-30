#ifdef WINDOW_SDL2

#include "xr_engine/core/window.h"

#include <vector>
#include <SDL2/SDL.h>
#include <xr_engine/core/global.h>

#ifdef RENDERER_VULKAN
#include <SDL2/SDL_vulkan.h>
#endif

namespace xre
{
    // ---=== Structs ===---

    struct Window::Data
    {
        // Reference count
        uint8_t reference_count = 0;

        SDL_Window *sdl_window = nullptr;
        Extent2D    extent     = {0, 0};
    };

    // --==== WINDOW MANAGER ====--

    // This class manages the main functions of SDL, which must be called at the beginning and end of the program
    // We use a destructor of a global variable to ensure that the cleanup function is called at the end of the program
    // For the init, we cannot just create a constructor, so we check at the creation of each window if the manager is initialized or
    // not, and if not, we initialize it
    class WindowManager
    {
        bool m_initialized = false;

      public:
        [[nodiscard]] inline bool is_initialized() const
        {
            return m_initialized;
        }

        // Called when the first window is created
        void init()
        {
            SDL_SetMainReady();
            if (SDL_Init(SDL_INIT_VIDEO) != 0)
            {
                std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
                exit(1);
            }
            m_initialized = true;
        }

        // Destroyed at the end of the program
        ~WindowManager()
        {
            if (m_initialized)
            {
                SDL_Quit();
                m_initialized = false;
            }
        }
    };

    // Global variable that will allow the destructor to be called at the end of the program
    WindowManager WINDOW_MANAGER;

    // --==== UTILS FUNCTIONS ====--

    void sdl_check(SDL_bool result)
    {
        if (result != SDL_TRUE)
        {
            std::cerr << "[SDL Error] Got SDL_FALSE !\n";
            exit(1);
        }
    }

    // ---=== API ===---

    Window::Window(const Settings &settings) : m_data(new Data)
    {
        if (!WINDOW_MANAGER.is_initialized())
        {
            WINDOW_MANAGER.init();
        }

        // Save extent
        m_data->extent = settings.mirror_window_settings.extent;

        // Init SDL2
        SDL_WindowFlags window_flags;

// We need to know which renderer to use
#ifdef RENDERER_VULKAN
        window_flags = SDL_WINDOW_VULKAN;
#endif

        m_data->sdl_window = SDL_CreateWindow(settings.application_info.name,
                                              SDL_WINDOWPOS_UNDEFINED,
                                              SDL_WINDOWPOS_UNDEFINED,
                                              static_cast<int32_t>(m_data->extent.width),
                                              static_cast<int32_t>(m_data->extent.height),
                                              window_flags);

        // It may fail (in WSL for example)
        if (m_data->sdl_window == nullptr)
        {
            delete m_data;
            m_data = nullptr;
            throw std::runtime_error("Failed to create window");
        }

        // Save reference count
        m_data->reference_count = 1;
    }

    Window::~Window()
    {
        if (m_data != nullptr)
        {
            // Decrement reference count
            m_data->reference_count--;

            if (m_data->reference_count == 0)
            {
                SDL_DestroyWindow(m_data->sdl_window);
                delete m_data;
            }

            m_data = nullptr;
        }
    }
    Window::Window(Window &&other) noexcept : m_data(other.m_data)
    {
        other.m_data = nullptr;
    }

    Window::Window(const Window &other) : m_data(other.m_data)
    {
        m_data->reference_count++;
    }
    Window &Window::operator=(const Window &other)
    {
        if (this != &other)
        {
            // Call destructor
            this->~Window();

            // Copy data
            m_data = other.m_data;
            m_data->reference_count++;
        }
        return *this;
    }

    Window &Window::operator=(Window &&other) noexcept
    {
        if (this != &other)
        {
            // Call destructor
            this->~Window();

            // Copy data
            m_data       = other.m_data;
            other.m_data = nullptr;
        }
        return *this;
    }

    bool Window::is_valid() const noexcept
    {
        return m_data != nullptr;
    }

#ifdef RENDERER_VULKAN
    void Window::get_required_vulkan_extensions(std::vector<const char *> &out_extensions) const
    {
        // Get the number of required extensions
        uint32_t required_extensions_count = 0;

        sdl_check(SDL_Vulkan_GetInstanceExtensions(m_data->sdl_window, &required_extensions_count, nullptr));

        // Create an array with that number and fetch said extensions
        // We add the extra_array_size to allow the caller to add its own extensions at the end of the array
        std::vector<const char *> required_extensions(required_extensions_count);
        sdl_check(SDL_Vulkan_GetInstanceExtensions(m_data->sdl_window, &required_extensions_count, required_extensions.data()));

        // Add the extensions to the output array if they are not already present
        for (const char *extension : required_extensions)
        {
            bool already_present = false;
            for (const char *out_extension : out_extensions)
            {
                if (strcmp(extension, out_extension) == 0)
                {
                    already_present = true;
                    break;
                }
            }

            if (!already_present)
            {
                out_extensions.push_back(extension);
            }
        }
    }
    bool Window::handle_events()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    return true;
            }
        }
        return false;
    }
#endif



} // namespace xre

#endif