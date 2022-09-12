#include "vr_engine/utils/data/optional.h"

#include <test_framework/test_framework.hpp>
#include <vr_engine/utils/data/hash_map.h>

TEST
{
    vre::HashMap map;

    // Define the values that will be used to populate the map
#define TEST_VALUES_COUNT 21
    size_t values[TEST_VALUES_COUNT] = {4,    2, 27,     22, 999, 1,  55, 0,  100000, 28,      888,
                                        6432, 1, 999988, 4,  19,  32, 22, 11, 75,     99999999};

    // The 0 key should not be allowed
    EXPECT_THROWS(map.set(0, static_cast<size_t>(0)));

    // Populate the map
    for (uint64_t i = 0; i < TEST_VALUES_COUNT; i++)
    {
        EXPECT_NO_THROWS(map.set(i + 1, values[i]));
    }

    // Check that all values are in the map
    for (uint64_t i = 0; i < TEST_VALUES_COUNT; i++)
    {
        auto result = map.get(i + 1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.take()->as_size, values[i]);
    }

    // Iterator
    int  values_in_iterator_count         = 0;
    bool found_indexes[TEST_VALUES_COUNT] = {false};
    for (const auto &v : map)
    {
        values_in_iterator_count++;

        // We need to check that all values are in the array and that the values are correct
        // However, the order is not necessarily the same as in the values array
        // Thus, we need to find the key each time manually
        bool found = false;
        for (uint64_t i = 0; i < TEST_VALUES_COUNT && !found; i++)
        {
            if (v.key == i + 1)
            {
                // Found the key, the value should match
                EXPECT_EQ(v.value.as_size, values[i]);
                found = true;

                // The key should not already have been found
                EXPECT_FALSE(found_indexes[i]);
                found_indexes[i] = true;
            }
        }
        EXPECT_TRUE(found);
    }
    // We must have found the same number of elements
    EXPECT_TRUE(values_in_iterator_count == TEST_VALUES_COUNT);

    // Check that a get with a wrong key returns NULL
    EXPECT_FALSE(map.get(87543656).has_value());
    EXPECT_FALSE(map.get(vre::HashMap::NULL_KEY).has_value());
    EXPECT_FALSE(map.get(TEST_VALUES_COUNT + 1).has_value());

    // Test the editing of existing keys
    size_t new_value = 789456123;
    EXPECT_NO_THROWS(map.set(12, {new_value}));
    auto new_value_get_result = map.get(12);
    EXPECT_TRUE(new_value_get_result.has_value());
    EXPECT_TRUE(new_value_get_result.take()->as_size == new_value);

    // Test the erasing of keys

    // Initial state: the key exists, and we have some number of elements in the map
    EXPECT_TRUE(map.get(5).has_value());
    size_t old_count = map.count();
    // Do the erasing
    map.remove(5);
    // Final state: the key does not exist anymore, and the number of elements has decreased by one
    size_t new_count = map.count();
    EXPECT_EQ(new_count, old_count - 1);
    EXPECT_FALSE(map.get(5).has_value());

    // Test operator[]
    size_t new_value2 = 123456789;
    // Non-existing key should be added
    map[5] = {new_value2};
    EXPECT_TRUE(map.get(5).has_value());
    EXPECT_EQ(map.get(5).take()->as_size, new_value2);
    map[27454] = {new_value2};
    EXPECT_TRUE(map.get(27454).has_value());
    EXPECT_EQ(map.get(27454).take()->as_size, new_value2);
    // But it should also be usable for gets
    EXPECT_EQ(map[5].as_size, new_value2);
    EXPECT_EQ(map[27454].as_size, new_value2);
    // Get for non-existing key returns nullptr
    EXPECT_EQ(map[9999999].as_size, static_cast<size_t>(0));
}