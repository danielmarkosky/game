#include <gtest/gtest.h>
#include "src/Rope.h"
#include "src/Level.h"
#include "src/utils.h"

std::shared_ptr<Level> createMockLevel() {
    std::unordered_map<int, std::unordered_map<int, int>> tiles;
    return std::make_shared<Level>(tiles);
}

TEST(RopeTest, Initialization_NodeCountAndSpacing) {
    auto level = createMockLevel();
    sf::Vector2f start(0.f, 0.f);
    sf::Vector2f end(0.f, 20.f);

    Rope rope(start, end, level);
    const auto& nodes = rope.getNodes();

    ASSERT_EQ(nodes.size(), 6);

    EXPECT_FLOAT_EQ(nodes[0].position.x, 0.f);
    EXPECT_FLOAT_EQ(nodes[0].position.y, 0.f);
    EXPECT_FLOAT_EQ(nodes.back().position.x, 0.f);
    EXPECT_FLOAT_EQ(nodes.back().position.y, 20.f);

    for(size_t ii = 0; ii < nodes.size() - 1; ++ii) {
        float d = distance(nodes[ii].position, nodes[ii+1].position);
        EXPECT_NEAR(d, 4.0f, 0.001f);
    }
}

TEST(RopeTest, Initialization_FixedPoint) {
    auto level = createMockLevel();
    sf::Vector2f start(0.f, 0.f);
    sf::Vector2f end(10.f, 0.f);

    Rope rope(start, end, level);
    const auto& nodes = rope.getNodes();

    EXPECT_FALSE(nodes.front().isFixed);
    EXPECT_TRUE(nodes.back().isFixed);
}

TEST(RopeTest, Update_GravityMovesNonFixedNodes) {
    auto level = createMockLevel();
    sf::Vector2f start(0.f, 0.f);
    sf::Vector2f end(10.f, 0.f);

    Rope rope(start, end, level);

    rope.update(0.1f);

    const auto& nodes = rope.getNodes();

    EXPECT_LT(nodes[0].position.y, 0.f);
}

TEST(RopeTest, Update_ConstraintsKeepLength) {
    auto level = createMockLevel();
    sf::Vector2f start(0.f, 0.f);
    sf::Vector2f end(0.f, 20.f);

    Rope rope(start, end, level);

    for(int ii=0; ii<100; ++ii) {
        rope.update(0.016f);
    }

    const auto& nodes = rope.getNodes();
    float totalLen = 0.f;
    for(size_t i = 0; i < nodes.size() - 1; ++i) {
        totalLen += distance(nodes[i].position, nodes[i+1].position);
    }

    EXPECT_NEAR(totalLen, 20.f, 2.0f);
}
