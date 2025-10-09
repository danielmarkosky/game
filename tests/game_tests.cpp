#include "gtest/gtest.h"
#include "../src/game.h"
#include <SFML/Graphics.hpp>

class GameTest : public Game, public ::testing::Test {
protected:
    // Game's members are directly accessible
};

TEST_F(GameTest, PlayerJumpSetsIsJumpingAndVelocity) {
    handlePlayerInput(sf::Keyboard::Space, true);
    EXPECT_TRUE(isJumping);
    EXPECT_FLOAT_EQ(playerVelocityY, jumpStrength);
}

TEST_F(GameTest, PlayerFallsToGround) {
    handlePlayerInput(sf::Keyboard::Space, true);
    // Simulate enough frames for player to land
    for (int i = 0; i < 180; ++i) {
        update(sf::seconds(1.f/60));
    }
    EXPECT_NEAR(player.getPosition().y, groundY, 0.1f);
    EXPECT_FALSE(isJumping);
}

TEST_F(GameTest, ProjectileSpawnsAndMoves) {
    size_t initialCount = projectiles.size();
    spawnProjectile();
    EXPECT_EQ(projectiles.size(), initialCount + 1);
    float initialX = projectiles.back().shape.getPosition().x;
    update(sf::seconds(1.f));
    EXPECT_LT(projectiles.back().shape.getPosition().x, initialX);
}

TEST_F(GameTest, ScoreIncrementsWhenDodgingProjectile) {
    // Place player on ground
    player.setPosition(100, groundY);
    // Spawn projectile in air, far right
    Projectile proj;
    proj.shape = sf::RectangleShape(sf::Vector2f(30, 30));
    proj.shape.setFillColor(sf::Color::Red);
    proj.shape.setPosition(800, groundY - 100); // air
    proj.scored = false;
    projectiles.push_back(proj);
    int initialScore = score;
    // Simulate enough updates for projectile to pass player
    for (int i = 0; i < 300; ++i) {
        update(sf::seconds(1.f/60));
    }
    EXPECT_GT(score, initialScore);
    EXPECT_TRUE(projectiles.front().scored);
}

TEST_F(GameTest, CollisionSetsGameOver) {
    // Set player and projectile to same position and size for guaranteed collision
    player.setPosition(100, groundY);
    Projectile proj;
    proj.shape = sf::RectangleShape(player.getSize());
    proj.shape.setFillColor(sf::Color::Red);
    proj.shape.setPosition(player.getPosition());
    proj.scored = false;
    projectiles.push_back(proj);
    update(sf::seconds(0.1f));
    EXPECT_TRUE(gameOver);
}

TEST_F(GameTest, ResetRestoresInitialState) {
    spawnProjectile();
    gameOver = true;
    isJumping = true;
    playerVelocityY = 123.f;
    score = 42;
    reset();
    EXPECT_EQ(projectiles.size(), 0);
    EXPECT_FALSE(gameOver);
    EXPECT_FALSE(isJumping);
    EXPECT_FLOAT_EQ(playerVelocityY, 0.f);
    EXPECT_FLOAT_EQ(player.getPosition().y, groundY);
    EXPECT_EQ(score, 0);
}
