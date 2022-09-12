#include <vr_engine/utils/data/storage.h>

#include <test_framework/test_framework.hpp>

struct Data
{
    int a = 0;
    int b = 0;
};

TEST
{
    vre::Storage<Data> storage;

    // Should be empty
    EXPECT_TRUE(storage.is_empty());
    EXPECT_EQ(storage.count(), static_cast<size_t>(0));

    // Push some m_data
    auto i1 = storage.push(Data {1, 2});
    EXPECT_EQ(i1, static_cast<size_t>(1));
    auto i2 = storage.push(Data {3, 4});
    EXPECT_EQ(i2, static_cast<size_t>(2));
    auto i3 = storage.push(Data {5, 6});
    EXPECT_EQ(i3, static_cast<size_t>(3));

    // Should have 3 items
    EXPECT_FALSE(storage.is_empty());
    EXPECT_EQ(storage.count(), static_cast<size_t>(3));

    // Should be able to get the m_data
    auto v1 = storage.get(i1);
    ASSERT_NOT_NULL(v1);
    EXPECT_EQ(v1->a, 1);
    EXPECT_EQ(v1->b, 2);

    auto v2 = storage.get(i2);
    ASSERT_NOT_NULL(v2);
    EXPECT_EQ(v2->a, 3);
    EXPECT_EQ(v2->b, 4);

    auto v3 = storage.get(i3);
    ASSERT_NOT_NULL(v3);
    EXPECT_EQ(v3->a, 5);
    EXPECT_EQ(v3->b, 6);

    // Also works with operators
    EXPECT_EQ(storage[i1].a, 1);
    EXPECT_EQ(storage[i1].b, 2);
    EXPECT_EQ(storage[i2].a, 3);
    EXPECT_EQ(storage[i2].b, 4);
    EXPECT_EQ(storage[i3].a, 5);
    EXPECT_EQ(storage[i3].b, 6);

    // With operators, we can modify a value in place
    storage[i1].a = 7;
    storage[i1].b = 8;
    EXPECT_EQ(storage[i1].a, 7);
    EXPECT_EQ(storage[i1].b, 8);

    // But we can't access an invalid index
    ASSERT_NULL(storage.get(999));
    EXPECT_THROWS(storage[999]);

    // We can iterate
    for (auto &v : storage)
    {
        auto  id = v.key();
        auto &val = v.value();

        EXPECT_TRUE(id >= 1 && id <= 3);
        val.a += 1;
    }

    EXPECT_EQ(storage[i1].a, 8);
    EXPECT_EQ(storage[i2].a, 4);
    EXPECT_EQ(storage[i3].a, 6);

}