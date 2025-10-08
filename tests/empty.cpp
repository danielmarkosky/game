#include <gtest/gtest.h>
#include "../src/empty.h"

TEST(OneFunctionTest, ReturnsOne) {
    EXPECT_EQ(one(), 1);
}
