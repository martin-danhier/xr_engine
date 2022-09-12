#pragma once

#include <cstdint>

namespace vre
{
    struct ISharedPointerData
    {
        uint8_t reference_count       = 1;
        virtual ~ISharedPointerData() = default;
    };

    class ISharedPointer
    {
      protected:
        ISharedPointerData *m_shared_pointer_data = nullptr;
        explicit ISharedPointer(ISharedPointerData *data) : m_shared_pointer_data(data) {}

      public:
        // Shared pointer logic
        ISharedPointer() = default;

        ISharedPointer(const ISharedPointer &other) : m_shared_pointer_data(other.m_shared_pointer_data)
        {
            // Create a copy of the shared pointer, thus increasing the reference count
            if (m_shared_pointer_data != nullptr)
            {
                m_shared_pointer_data->reference_count++;
            }
        }

        ISharedPointer(ISharedPointer &&other) noexcept : m_shared_pointer_data(other.m_shared_pointer_data)
        {
            // Take other's data
            other.m_shared_pointer_data = nullptr;
        }

        ISharedPointer &operator=(const ISharedPointer &other)
        {
            if (this != &other)
            {
                // Call destructor
                this->~ISharedPointer();

                // Copy data
                m_shared_pointer_data = other.m_shared_pointer_data;
                if (m_shared_pointer_data != nullptr)
                {
                    m_shared_pointer_data->reference_count++;
                }
            }
            return *this;
        }

        ISharedPointer &operator=(ISharedPointer &&other) noexcept
        {
            if (this != &other)
            {
                // Call destructor
                this->~ISharedPointer();

                // Take other's data
                m_shared_pointer_data       = other.m_shared_pointer_data;
                other.m_shared_pointer_data = nullptr;
            }
            return *this;
        }

        [[nodiscard]] inline bool is_valid() const { return m_shared_pointer_data != nullptr; }

        ~ISharedPointer()
        {
            if (m_shared_pointer_data)
            {
                m_shared_pointer_data->reference_count--;

                if (m_shared_pointer_data->reference_count == 0)
                {
                    delete m_shared_pointer_data;
                }

                m_shared_pointer_data = nullptr;
            }
        }
    };
} // namespace vre