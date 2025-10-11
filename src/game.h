#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "characters/duck.h"
#include "levels/first_level.h"

class Game {
public:
    Game();
    void run();

private:
    sf::RenderWindow window;
    sf::View view;
    std::shared_ptr<Level> level;
    std::unique_ptr<Player> player;

    sf::Font font;
    sf::Text scoreText;
    int score = 0;
    bool gameOver = false;

    void handleEvents();
    void update(float dt);
    void render();
    void reset();
};
