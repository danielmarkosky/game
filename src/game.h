#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "characters/duck.h"
#include "levels/first_level.h"
#include "ui/score.h"
#include "projectile_manager.h"

class Game {
public:
    Game();
    void run();

private:
    sf::RenderWindow window;
    sf::View view;
    std::shared_ptr<Level> level;
    std::unique_ptr<Player> player;
    std::unique_ptr<Score> score; // Use Score class for score logic
    ProjectileManager projectileManager; // Manages projectiles
    float projectileSpawnTimer = 0.0f; // Timer for repeated projectile spawning
    const float projectileSpawnInterval = 1.0f; // Interval in seconds
    bool gameOver = false;

    void handleEvents();
    void update(float dt);
    void render();
    void reset();
};
