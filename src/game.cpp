#include "game.h"
#include "levels/first_level.h"
#include "src/projectiles.h"
#include <cmath>

namespace
{
    constexpr unsigned int VIEW_WIDTH = 320;
    constexpr unsigned int VIEW_HEIGHT = 180;
}

Game::Game()
    : window(sf::VideoMode(VIEW_WIDTH * 4, VIEW_HEIGHT * 4), "Jump & Dodge Game", sf::Style::Close),
      projectileManager(VIEW_WIDTH, VIEW_HEIGHT)
{
    window.setView(sf::View(sf::FloatRect(0, 0, VIEW_WIDTH, VIEW_HEIGHT)));
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);
    level = std::make_shared<FirstLevel>();
    player = std::make_unique<Player>(sf::Vector2f(32.f, VIEW_HEIGHT - 64.f), level);

    font.loadFromFile("assets/BBHSansBogle-Regular.ttf");
    scoreText.setFont(font);
    scoreText.setCharacterSize(16);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(8.f, 8.f);
    scoreText.setString("Score: 0");
}

void Game::run() {
    sf::Clock clock;
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        processEvents();
        if (!gameOver) {
            update(dt);
        }
        render();
    }
}

void Game::processEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed)
            window.close();
        if (!gameOver && event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::A)
                player->moveLeft();
            if (event.key.code == sf::Keyboard::D)
                player->moveRight();
            if (event.key.code == sf::Keyboard::Space)
                player->jump();
        }
        if (!gameOver && event.type == sf::Event::KeyReleased) {
            if (event.key.code == sf::Keyboard::A || event.key.code == sf::Keyboard::D)
                player->stopHorizontal();
        }
        if (gameOver && event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::R) {
            reset();
        }
    }
}

void Game::update(float dt) {
    player->update(dt);
    projectileManager.update(dt);
    if (projectileManager.handleCollisions(player->getBounds(), score)) {
        gameOver = true;
        return;
    }
    scoreText.setString("Score: " + std::to_string(score));
}

void Game::render() {
    window.clear(sf::Color::Black);
    level->draw(window);
    player->draw(window);
    projectileManager.draw(window);
    window.draw(scoreText);
    if (gameOver) {
        sf::Text overText("Game Over! Press R to restart", font, 16);
        overText.setFillColor(sf::Color::Red);
        overText.setPosition(40.f, 80.f);
        window.draw(overText);
    }
    window.display();
}

void Game::reset() {
    score = 0;
    scoreText.setString("Score: 0");
    gameOver = false;
    projectileManager.reset();
    player = std::make_unique<Player>(sf::Vector2f(32.f, VIEW_HEIGHT - 64.f), level);
}
