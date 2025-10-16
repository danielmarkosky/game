#include "game.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include "characters/duck.h"
#include "levels/first_level.h"
#include "ui/score.h"
#include "environment/firebomb.h"

namespace
{
    constexpr unsigned int VIEW_WIDTH = 320;
    constexpr unsigned int VIEW_HEIGHT = 180;
}

Game::Game()
{
    window.create(sf::VideoMode(VIEW_WIDTH * 4, VIEW_HEIGHT * 4), "Jump & Dodge Game", sf::Style::Close);
    window.setFramerateLimit(60); // Limit to 60 FPS to reduce CPU usage
    window.setVerticalSyncEnabled(true); // Enable vertical sync for smooth rendering
    view.setSize(VIEW_WIDTH, VIEW_HEIGHT);
    view.setCenter(VIEW_WIDTH / 2.f, VIEW_HEIGHT / 2.f);
    window.setView(view);

    level = std::make_shared<FirstLevel>();
    player = std::make_unique<Duck>(sf::Vector2f(32.f, 80.f), level);
    score = std::make_unique<Score>();
    gameOver = false;

    // Spawn a Firebomb projectile from the right edge, moving left
    projectileManager.addProjectile(std::make_unique<Firebomb>(312.f, 120.f));
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
        // Delegate input handling to player
        player->handleEvent(event);
    }
}

void Game::update(float dt) {
    player->update(dt);
    projectileManager.updateAll(dt);
    // Repeated projectile spawning
    projectileSpawnTimer += dt;
    if (projectileSpawnTimer >= projectileSpawnInterval) {
        projectileManager.addProjectile(std::make_unique<Firebomb>(312.f, 120.f));
        projectileSpawnTimer = 0.0f;
    }
    // Example: score->add(1); // Add score logic here as needed
    // ...update projectiles, check collisions, update score...
}

void Game::render() {
    window.clear(sf::Color::Black);
    level->draw(window);
    player->draw(window);
    projectileManager.drawAll(window);
    // score->draw(window);
    // ...draw projectiles, score...
    window.display();
}
