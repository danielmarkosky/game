#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "player.h"
#include "level.h"
#include "projectiles.h"

class Game {
public:
    Game();
    void run();

private:
    sf::RenderWindow window;
    std::shared_ptr<Level> level;
    std::unique_ptr<Player> player;

    sf::Font font;
    sf::Text scoreText;
    int score = 0;
    bool gameOver = false;

    ProjectileManager projectileManager;

    void processEvents();
    void update(float dt);
    void render();
    void reset();
};
