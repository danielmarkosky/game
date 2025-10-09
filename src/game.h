#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

struct Projectile {
    sf::RectangleShape shape;
    bool scored = false;
};

class Game {
public:
    Game();
    void run();
protected:
    void processEvents();
    void update(sf::Time dt);
    void render();
    void handlePlayerInput(sf::Keyboard::Key key, bool isPressed);
    void spawnProjectile();
    void reset();

    sf::RenderWindow window;
    sf::RectangleShape player;
    std::vector<Projectile> projectiles;
    sf::Color bgColor = sf::Color::Black;
    bool isJumping = false;
    float playerVelocityY = 0.f;
    float gravity = 1000.f;
    float jumpStrength = -500.f;
    float groundY = 500.f;
    sf::Clock projectileClock;
    bool gameOver = false;
    int score = 0;
};
