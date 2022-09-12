#include <vr_engine/utils/data/map.h>
//#include <iostream>
//
#include <test_framework/test_framework.hpp>

struct Data {
    int a = 0;
    int b = 0;

    bool operator!=(const Data& other) const {
        return a != other.a || b != other.b;
    }
};

TEST {
    vre::Map<Data> map;
    map.set(42, Data{1, 2});
    map.set(43, Data{50, 54});

    auto data = map.get(42);
    ASSERT_NOT_NULL(data);
    EXPECT_EQ(data->a, 1);
    EXPECT_EQ(data->b, 2);

    data = map.get(43);
    ASSERT_NOT_NULL(data);
    EXPECT_EQ(data->a, 50);
    EXPECT_EQ(data->b, 54);

    data = map.get(44);
    EXPECT_NULL(data);

    map.remove(42);
    data = map.get(42);
    EXPECT_NULL(data);

    // Set multiple values
    map.set(42, Data{1, 2});
    map.set(43, Data{3, 4});
    map.set(44, Data{5, 6});
    map.set(45, Data{7, 8});
    map.set(46, Data{9, 10});
    map.set(47, Data{11, 12});
    map.set(48, Data{13, 14});

    EXPECT_EQ(map.count(), static_cast<size_t>(7));

    // Iterate over all values
    for (auto &entry: map) {
        entry.value().a += 1;
    }

    EXPECT_EQ(map.count(), static_cast<size_t>(7));

    ASSERT_NOT_NULL(map.get(42));
    EXPECT_EQ(map.get(42)->a, 2);
    ASSERT_NOT_NULL(map.get(43));
    EXPECT_EQ(map.get(43)->a, 4);
    ASSERT_NOT_NULL(map.get(44));
    EXPECT_EQ(map.get(44)->a, 6);
    ASSERT_NOT_NULL(map.get(45));
    EXPECT_EQ(map.get(45)->a, 8);
    ASSERT_NOT_NULL(map.get(46));
    EXPECT_EQ(map.get(46)->a, 10);
    ASSERT_NOT_NULL(map.get(47));
    EXPECT_EQ(map.get(47)->a, 12);
    ASSERT_NOT_NULL(map.get(48));
    EXPECT_EQ(map.get(48)->a, 14);

    // Try to update with get
    auto &v = *map.get(42);
    v.a = 100;

    // We can also use operators if we want a slot for that key to be created if it doesn't exist
    // Note: doing the operation each time will look up the map each time
    // It will be more performant to store the reference at the top
    map[42].a = 200;
    EXPECT_EQ(map[42].a, 200);
    EXPECT_EQ(map[42].b, 2);
    EXPECT_EQ(map.get(42)->a, 200);
    EXPECT_EQ(map.get(42)->b, 2);

    map[99].a = 300;
    EXPECT_EQ(map[99].a, 300);
    EXPECT_EQ(map[99].b, 0);
    EXPECT_EQ(map.get(99)->a, 300);
    EXPECT_EQ(map.get(99)->b, 0);

    // We can also use the operator in a const way
    const auto &foo = map[42];
    EXPECT_EQ(foo.a, 200);
    EXPECT_EQ(foo.b, 2);
}