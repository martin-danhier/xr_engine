#pragma once

namespace xre
{
    struct Settings;

    class Engine
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        Engine() = default;
        explicit Engine(const Settings &settings);
        Engine(const Engine &other);
        Engine(Engine &&other) noexcept;
        Engine &operator=(const Engine &other);
        Engine &operator=(Engine &&other) noexcept;
        ~Engine();

    };

} // namespace xre