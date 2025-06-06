#include <gtest/gtest.h>

// Basic test to verify test framework is working
TEST(BasicTest, SanityCheck) {
    EXPECT_EQ(2 + 2, 4);
    EXPECT_TRUE(true);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}