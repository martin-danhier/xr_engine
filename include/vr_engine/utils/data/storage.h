#pragma once

#include <vr_engine/utils/data/map.h>

namespace vre
{
    /**
     * A storage is a map with an automatic key generator.
     *
     * New objects can be pushed inside it, which returns an unique ID. The object can then be retrieved using this ID.
     * Under the hood, the storage uses a Map, which stores all the objects in a compact vector. Thus, iterations are fast.
     *
     * Also, since the ids are indexed using a HashMap, an object can be retrieved in constant time. This is especially useful as
     * the number of objects in the storage grows.
     *
     * However, when an object is removed, some other objects may be moved in the vector to keep it compact. After a deletion, objects
     * should thus be retrieved again to ensure they are still valid.
     *
     * ID 0 is reserved for NULL_ID.
     */
    template<typename T>
    class Storage
    {
      public:
        typedef uint64_t    Id;
        constexpr static Id NULL_ID = 0;

      private:
        Id     m_id_counter = 0;
        Map<T> m_map        = {};

      public:
        Id push(const T &value)
        {
            // Increment counter
            m_id_counter++;

            // Set value
            m_map.set(m_id_counter, value);
            return m_id_counter;
        }

        Id push(T &&value)
        {
            // Increment counter
            m_id_counter++;

            // Set value
            m_map.set(m_id_counter, std::move(value));
            return m_id_counter;
        }

        T *get(Id id) { return m_map.get(id); }

        T *get(Id id) const { return m_map.get(id); }

        T &operator[](Id id)
        {
            auto res = get(id);

            if (res == nullptr)
            {
                throw std::runtime_error("No such id");
            }
            return *res;
        }

        void remove(Id id) { m_map.remove(id); }

        void clear() { m_map.clear(); }

        [[nodiscard]] bool is_empty() const { return m_map.is_empty(); }

        [[nodiscard]] size_t count() const { return m_map.count(); }

        [[nodiscard]] bool exists(Id id) const { return m_map.exists(id); }

        // Iterator
        class Iterator
        {
          public:
            // Traits
            using iterator_category = std::forward_iterator_tag;
            using value_type        = typename Map<T>::Entry;
            using difference_type   = std::ptrdiff_t;
            using pointer           = value_type *;
            using reference         = value_type &;

          private:
            typename Map<T>::Iterator m_it;

          public:
            // Contents

            explicit Iterator(typename Map<T>::Iterator &&it) : m_it(std::move(it)) {}

            Iterator &operator++()
            {
                ++m_it;
                return *this;
            }

            [[nodiscard]] bool operator!=(const Iterator &other) const { return m_it != other.m_it; }

            // Access

            [[nodiscard]] reference operator*() { return *m_it; }

            [[nodiscard]] pointer operator->() const { return &*m_it; }
        };

        [[nodiscard]] Iterator begin() { return Iterator(m_map.begin()); }

        [[nodiscard]] Iterator end() { return Iterator(m_map.end()); }

        // Const iterator

        class ConstIterator
        {
          public:
            // Traits
            using iterator_category = std::forward_iterator_tag;
            using value_type        = typename Map<T>::Entry;
            using difference_type   = std::ptrdiff_t;
            using pointer           = const value_type *;
            using reference         = const value_type &;

          private:
            typename Map<T>::ConstIterator m_it;

          public:
            // Contents

            explicit ConstIterator(typename Map<T>::ConstIterator &&it) : m_it(std::move(it)) {}

            ConstIterator &operator++()
            {
                ++m_it;
                return *this;
            }

            [[nodiscard]] bool operator!=(const ConstIterator &other) const { return m_it != other.m_it; }

            // Access

            [[nodiscard]] reference operator*() { return *m_it; }

            [[nodiscard]] pointer operator->() const { return &*m_it; }
        };

        [[nodiscard]] ConstIterator begin() const { return ConstIterator(m_map.begin()); }

        [[nodiscard]] ConstIterator end() const { return ConstIterator(m_map.end()); }
    };
} // namespace vre