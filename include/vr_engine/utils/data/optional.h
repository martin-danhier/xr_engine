#pragma once

// MSVC doesn't have exceptions available by default
#include <stdexcept>

namespace vre
{
    // Null option
    namespace _optional_impl
    {
        struct None
        {
            enum class Tag
            {
                None
            };
            explicit constexpr None(Tag) {}
        };
    } // namespace _optional_impl
    inline constexpr _optional_impl::None NONE {_optional_impl::None::Tag::None};

    /** The Optional type can store either a value or NONE. */
    template<typename T>
    class Optional
    {
      private:
        bool m_has_value = false;
        T    m_value;

      public:
        Optional() = default;

#pragma clang diagnostic push
#pragma ide diagnostic   ignored "google-explicit-constructor"
        // These three constructors are intentionally not explicit to allow implicit conversions
        // in return statements
        Optional(const T &value) : m_has_value(true), m_value(value) {}
        Optional(T &&value) : m_has_value(true), m_value(std::move(value)) {}
        Optional(_optional_impl::None) : m_has_value(false) {}
#pragma clang diagnostic pop

        // Copy operator
        Optional &operator=(const Optional &other)
        {
            m_has_value = other.m_has_value;
            if (m_has_value)
            {
                m_value = other.m_value;
            }
            return *this;
        }
        Optional &operator=(Optional &&other) noexcept
        {
            m_has_value = other.m_has_value;
            if (m_has_value)
            {
                m_value = std::move(other.m_value);
            }
            return *this;
        }
        // Implicit initialization with value
        Optional &operator=(const T &value)
        {
            m_has_value = true;
            m_value     = value;
            return *this;
        }
        Optional &operator=(T &&value)
        {
            m_has_value = true;
            m_value     = std::move(value);
            return *this;
        }
        // Implicit initialization with null
        Optional &operator=(const _optional_impl::None &)
        {
            m_has_value = false;
            m_value     = T();
            return *this;
        }

        [[nodiscard]] inline bool has_value() const
        {
            return m_has_value;
        }

        /**
         * @brief Returns the value of the optional. Throws an exception if the optional is empty.
         */
        const T &value() const
        {
            if (!m_has_value)
            {
                throw std::runtime_error("Optional has no value");
            }
            return m_value;
        }
        T &value()
        {
            if (!m_has_value)
            {
                throw std::runtime_error("Optional has no value");
            }
            return m_value;
        }

        /**
         * Takes the value of the optional and returns it.
         * The optional is then empty.
         * */
        T &&take()
        {
            if (!m_has_value)
            {
                throw std::runtime_error("Optional has no value");
            }
            m_has_value = false;
            return std::move(m_value);
        }

        T &expect(const std::string &msg)
        {
            if (!m_has_value)
            {
                throw std::runtime_error(msg);
            }
            return m_value;
        }

        // Access operators
        T &operator*()
        {
            return value();
        }
        const T &operator*() const
        {
            return value();
        }
        T *operator->()
        {
            return &value();
        }
        const T *operator->() const
        {
            return &value();
        }
    };

} // namespace vre