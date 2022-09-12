#include <test_framework/test_framework.hpp>
#include <vr_engine/utils/data/optional.h>

using namespace vre;

struct Data
{
    int value;
};

Optional<Data> get_data(bool should_return)
{
    if (should_return)
    {
        return Data {42};
    }
    else
    {
        return NONE;
    }
}

TEST
{
    Optional<Data> opt;

    EXPECT_FALSE(opt.has_value());

    EXPECT_THROWS(opt.value());
    EXPECT_THROWS(opt.take());

    opt = Data {42};

    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value().value, 42);
    EXPECT_EQ(opt->value, 42);

    // Modification is possible
    opt->value = 43;
    EXPECT_EQ(opt.value().value, 43);
    EXPECT_EQ(opt->value, 43);

    // Take the value, it becomes empty
    EXPECT_EQ(opt.take().value, 43);
    EXPECT_FALSE(opt.has_value());
    EXPECT_THROWS(opt.value());
    EXPECT_THROWS(opt.take());

    // Set it again
    opt = Optional(Data {44});
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value().value, 44);
    EXPECT_EQ(opt->value, 44);

    opt = NONE;
    EXPECT_FALSE(opt.has_value());
    EXPECT_THROWS(opt.value());
    EXPECT_THROWS(opt.take());

    // Test function
    opt = get_data(true);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value().value, 42);
    EXPECT_EQ(opt->value, 42);

    opt = get_data(false);
    EXPECT_FALSE(opt.has_value());
    EXPECT_THROWS(opt.value());
    EXPECT_THROWS(opt.take());

}