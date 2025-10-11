#include "game.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include "characters/duck.h"
#include "levels/first_level.h"

namespace
{
    constexpr unsigned int VIEW_WIDTH = 320;
    constexpr unsigned int VIEW_HEIGHT = 180;
}

Game::Game()
{
    window.create(sf::VideoMode(VIEW_WIDTH * 4, VIEW_HEIGHT * 4), "Jump & Dodge Game", sf::Style::Close);
    view.setSize(VIEW_WIDTH, VIEW_HEIGHT);
    view.setCenter(VIEW_WIDTH / 2.f, VIEW_HEIGHT / 2.f);
    window.setView(view);

    level = std::make_shared<FirstLevel>();
    player = std::make_unique<Duck>(sf::Vector2f(32.f, 80.f), level);

    font.loadFromFile("assets/BBHSansBogle-Regular.ttf");
    scoreText.setFont(font);
    scoreText.setCharacterSize(16);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(8.f, 8.f);
    scoreText.setString("Score: 0");
    score = 0;
    gameOver = false;
}

void Game::run() {
    sf::Clock clock;
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        handleEvents();
        update(dt);
        render();
    }
}

void Game::handleEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed)
            window.close();
        // Handle input
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) player->moveLeft();
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) player->moveRight();
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) player->jump();
    }
}

void Game::update(float dt) {
    player->update(dt);
    // ...update projectiles, check collisions, update score...
}

void Game::render() {
    window.clear(sf::Color::Black);
    level->draw(window);
    player->draw(window);
    // ...draw projectiles, score...
    window.display();
}
