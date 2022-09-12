#pragma once

#include <vector>
#include <vr_engine/utils/data/hash_map.h>
#include <vr_engine/utils/data/optional.h>

namespace vre
{
    template<typename T>
    class Map
    {
      public:
        // Types
        using Key                     = typename HashMap::Key;
        constexpr static Key NULL_KEY = HashMap::NULL_KEY;

        class Entry
        {
          private:
            T   m_value;
            Key m_key;

          public:
            friend class Map;

            Entry(const Key &key, const T &value) : m_key(key), m_value(value) {}
            explicit Entry(const Key &key) : m_key(key) {}
            Entry(const Key &key, T &&value) : m_key(key), m_value(std::move(value)) {}
            [[nodiscard]] inline Key      key() const { return m_key; }
            [[nodiscard]] inline T       &value() { return m_value; }
            [[nodiscard]] inline const T &value() const { return m_value; }
        };

      private:
        // Hash map to store indices of the elements
        HashMap m_hash_map = {};
        // Storage to store the actual m_data
        std::vector<Entry> m_storage = {};

      public:
        // Constructors
        Map() = default;
        Map(const Map &other) : m_hash_map(other.m_hash_map), m_storage(other.m_storage) {}
        Map(Map &&other) noexcept : m_hash_map(std::move(other.m_hash_map)), m_storage(std::move(other.m_storage)) {}
        // All destruction is handled by hash_map and vector ! :D

        // Methods
        [[nodiscard]] inline size_t count() const { return m_hash_map.count(); }
        [[nodiscard]] inline bool   is_empty() const { return m_hash_map.is_empty(); }
        [[nodiscard]] inline bool   exists(const Key &key) const { return m_hash_map.exists(key); }
        const T                    *get(const Key &key) const
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                return &m_storage[res.value()->as_size].value();
            }
            return nullptr;
        }

        /** Get a mutable reference to a value by its key.
         * This can be useful if many modifications are needed to the value, to avoid having to look it up again.
         *
         * However, be cautious: the returned value is a pointer to the value. If the map is modified,
         * the pointer may not point to the right value anymore. Thus, only use the reference in the immediate scope and
         don't call
         * methods that can move values (e.g remove).
         */
        T *get(const Key &key)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                return &m_storage[res.value()->as_size].value();
            }
            return nullptr;
        }

        void set(const Key &key, const T &value)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                m_storage[res.value()->as_size].m_value = value;
            }
            else
            {
                auto index = m_storage.size();
                m_storage.push_back(Entry {key, value});
                m_hash_map.set(key, index);
            }
        }

        void set(const Key &key, T &&value)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                m_storage[res.value()->as_size].m_value = std::move(value);
            }
            else
            {
                auto index = m_storage.size();
                m_storage.push_back(Entry {key, std::move(value)});
                m_hash_map.set(key, HashMap::Value {index});
            }
        }

        void remove(const Key &key)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                auto index = res.value()->as_size;

                // Move the last element to the index of the removed element
                m_storage[index] = std::move(m_storage.back());
                m_storage.pop_back();

                // Remove deleted key from hash map
                m_hash_map.remove(key);
                // Update the entry in the hash map for the moved element
                if (index < m_storage.size())
                {
                    m_hash_map.set(m_storage[index].key(), HashMap::Value {index});
                }
            }
        }

        void clear()
        {
            m_hash_map.clear();
            m_storage.clear();
        }

        // Operators

        Map &operator=(const Map &other)
        {
            m_hash_map = other.m_hash_map;
            m_storage  = other.m_storage;
            return *this;
        }

        Map &operator=(Map &&other) noexcept
        {
            m_hash_map = std::move(other.m_hash_map);
            m_storage  = std::move(other.m_storage);
            return *this;
        }

        /** Shorthand for accesses: gets the slot of the key.
         *
         * If the key exists, it returns the existing slot. If it does not exist, it creates a new slot and returns it
         * (requires a default constructor for T).
         */
        T &operator[](const Key &key)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                return m_storage[res.take()->as_size].value();
            }
            else
            {
                auto index = m_storage.size();
                m_storage.push_back(Entry {key});
                m_hash_map.set(key, HashMap::Value {index});
                return m_storage[index].value();
            }
        }

        // Iterator

        class Iterator
        {
          public:
            // Traits
            using difference_type   = ptrdiff_t;
            using value_type        = Entry;
            using pointer           = value_type *;
            using reference         = value_type &;
            using iterator_category = std::forward_iterator_tag;

          private:
            // Iterator to the vector
            typename std::vector<value_type>::iterator m_it;

          public:
            // Constructors
            explicit Iterator(const typename std::vector<value_type>::iterator &&it) : m_it(std::move(it)) {}

            // Operators
            Iterator &operator++()
            {
                ++m_it;
                return *this;
            }

            bool operator!=(const Iterator &other) const { return m_it != other.m_it; }

            // Accessors
            reference operator*() { return *m_it; }

            pointer operator->() const { return &(operator*()); }
        };

        Iterator begin() { return Iterator(m_storage.begin()); }

        Iterator end() { return Iterator(m_storage.end()); }

        // Const Iterator

        class ConstIterator
        {
          public:
            // Traits
            using difference_type   = ptrdiff_t;
            using value_type        = Entry;
            using pointer           = const value_type *;
            using reference         = const value_type &;
            using iterator_category = std::forward_iterator_tag;

          private:
            // Iterator to the vector
            typename std::vector<value_type>::const_iterator m_it;

          public:
            // Constructors
            explicit ConstIterator(const typename std::vector<value_type>::const_iterator &&it) : m_it(std::move(it)) {}

            // Operators
            ConstIterator &operator++()
            {
                ++m_it;
                return *this;
            }

            bool operator!=(const ConstIterator &other) const { return m_it != other.m_it; }

            // Accessors
            reference operator*() { return *m_it; }

            pointer operator->() const { return &(operator*()); }
        };
        ConstIterator begin() const { return ConstIterator(m_storage.begin()); }

        ConstIterator end() const { return ConstIterator(m_storage.end()); }
    };
} // namespace vre