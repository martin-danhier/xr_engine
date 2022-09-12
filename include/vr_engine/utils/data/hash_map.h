#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace vre
{
    template<typename T>
    class Optional;

    /**
     * An hash map stores a set of key-value pairs. Both the key and the value are 64-bit integers.
     *
     * In particular, the value is as long as a size_t, meaning it can store an index or a pointer. This is useful in combination with
     * a vector to store actual m_data. See the StructMap for an implementation of this.
     */
    class HashMap
    {
      private:
        struct Data;

        // Store opaque fields for implementation
        Data *m_data = nullptr;

        // Increase the capacity of the hash map
        void expand();

      public:
        typedef uint64_t     Key;
        constexpr static Key NULL_KEY = 0;
        union Value
        {
            size_t as_size;
            void  *as_ptr;

            Value() = default;

#pragma clang diagnostic push
#pragma ide diagnostic   ignored "google-explicit-constructor"
            Value(size_t value) : as_size(value) {}
            Value(void *value) : as_ptr(value) {}
#pragma clang diagnostic pop
        };
        struct Entry
        {
            HashMap::Key   key;
            HashMap::Value value;
        };

        // Constructors & destructors

        HashMap();
        HashMap(const HashMap &other);
        HashMap(HashMap &&other) noexcept;
        ~HashMap();

        // Operators

        HashMap &operator=(const HashMap &other);
        HashMap &operator=(HashMap &&other) noexcept;
        /** Returns the slot corresponding to the given key.
         *
         * If the key exists, it returns the existing slot. If it does not exist, it creates a new slot with value 0 and returns it.
         */
        Value   &operator[](const Key &key);

        // Methods

        [[nodiscard]] Optional<Value *> get(const Key &key) const;
        void                            set(const Key &key, const Value &value);
        void                            remove(const Key &key);
        [[nodiscard]] bool              exists(const Key &key) const;
        void                            clear();
        [[nodiscard]] size_t            count() const;
        [[nodiscard]] bool              is_empty() const;

        // Iterator
        class Iterator
        {
          private:
            const HashMap *m_map;
            size_t         m_index;
            Entry         *m_entry;

          public:
            // Iterator traits
            using difference_type   = std::ptrdiff_t;
            using value_type        = HashMap::Entry;
            using pointer           = value_type *;
            using reference         = value_type &;
            using iterator_category = std::forward_iterator_tag;

            // Constructors & destructors
            Iterator(const HashMap *map, size_t index);

            // Operators
            Entry     operator*();
            Iterator &operator++();
            bool      operator!=(const Iterator &other) const;
        };

        [[nodiscard]] Iterator begin() const;
        [[nodiscard]] Iterator end() const;
    };
} // namespace vre