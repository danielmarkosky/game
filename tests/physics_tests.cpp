#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Level.h"
#include "src/Physics.h"
#include <fstream>
#include <sstream>

TEST(Physics, ReadsNumbersByName) {
    std::vector<std::string> problems;
    const Physics p = Physics::parse("# a comment\nrun_speed 60   # slower\n\ngravity 700\n", &problems);
    EXPECT_TRUE(problems.empty());
    EXPECT_FLOAT_EQ(p.runSpeed, 60.f);
    EXPECT_FLOAT_EQ(p.gravity, 700.f);
    EXPECT_FLOAT_EQ(p.jumpSpeed, Physics{}.jumpSpeed); // not given: the default
}

TEST(Physics, SaysWhatsWrongAndCarriesOn) {
    std::vector<std::string> problems;
    const Physics p = Physics::parse("run_sped 60\njump_speed fast\ngravity -5\nreel_speed 40\n", &problems);
    ASSERT_EQ(problems.size(), 3u);
    EXPECT_EQ(problems[0], "line 1: no number called run_sped");
    EXPECT_FLOAT_EQ(p.gravity, Physics{}.gravity);
    EXPECT_FLOAT_EQ(p.reelSpeed, 40.f);
}

TEST(Physics, RoundTripsThroughText) {
    Physics p;
    p.swingPush = 99.f;
    p.ropeJumpSpeed = 50.f;
    std::vector<std::string> problems;
    const Physics back = Physics::parse(p.serialize(), &problems);
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(back.serialize(), p.serialize());
}

TEST(Physics, TheGamesFileIsAllRight) {
    std::ifstream file(config::assetPath("physics.txt"));
    ASSERT_TRUE(file);
    std::stringstream text;
    text << file.rdbuf();
    std::vector<std::string> problems;
    Physics::parse(text.str(), &problems);
    for (const std::string& problem : problems) ADD_FAILURE() << problem;
    // Every number is in it, so it says how the game moves without looking at the code.
    std::istringstream all(Physics{}.serialize());
    for (std::string line; std::getline(all, line);) {
        const std::string name = line.substr(0, line.find(' '));
        EXPECT_NE(text.str().find("\n" + name + " "), std::string::npos) << name;
    }
}

TEST(Physics, ThePlaygroundLoads) {
    // assets/levels/playground.txt: a room for trying the feel out in.
    const auto level = Level::loadFromFile(config::assetPath("levels/playground.txt"));
    EXPECT_TRUE(level->hasStart());
    EXPECT_EQ(level->gems().size(), 2u);
}
