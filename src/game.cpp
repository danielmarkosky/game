#include "game.h"
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <cstdlib>

Game::Game() : window(sf::VideoMode(800, 600), "Jump & Dodge"), player(sf::Vector2f(50, 50)) {
    player.setFillColor(sf::Color::Green);
    player.setPosition(100, groundY);
}

void Game::run() {
    sf::Clock clock;
    while (window.isOpen()) {
        processEvents();
        sf::Time dt = clock.restart();
        update(dt);
        render();
    }
}

void Game::processEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed)
            window.close();
        if (event.type == sf::Event::KeyPressed)
            handlePlayerInput(event.key.code, true);
    }
}

void Game::handlePlayerInput(sf::Keyboard::Key key, bool isPressed) {
    if (gameOver && key == sf::Keyboard::R) {
        reset();
        return;
    }
    if (key == sf::Keyboard::Space && !isJumping && !gameOver) {
        isJumping = true;
        playerVelocityY = jumpStrength;
    }
}

void Game::update(sf::Time dt) {
    if (gameOver) return;
    // Player jump physics
    if (isJumping) {
        player.move(0, playerVelocityY * dt.asSeconds());
        playerVelocityY += gravity * dt.asSeconds();
        if (player.getPosition().y >= groundY) {
            player.setPosition(player.getPosition().x, groundY);
            isJumping = false;
            playerVelocityY = 0.f;
        }
    }
    // Spawn projectiles
    if (projectileClock.getElapsedTime().asSeconds() > 1.0f) {
        spawnProjectile();
        projectileClock.restart();
    }
    // Move projectiles and check for dodges
    for (auto& proj : projectiles) {
        proj.shape.move(-300.f * dt.asSeconds(), 0);
        // If projectile passed player and not scored, and no collision
        if (!proj.scored && proj.shape.getPosition().x + proj.shape.getSize().x < player.getPosition().x) {
            if (!player.getGlobalBounds().intersects(proj.shape.getGlobalBounds())) {
                score++;
                proj.scored = true;
            }
        }
    }
    // Remove off-screen projectiles
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& proj) {
        return proj.shape.getPosition().x + proj.shape.getSize().x < 0;
    }), projectiles.end());
    // Collision detection
    for (const auto& proj : projectiles) {
        if (player.getGlobalBounds().intersects(proj.shape.getGlobalBounds())) {
            gameOver = true;
        }
    }
}

void Game::spawnProjectile() {
    Projectile proj;
    proj.shape = sf::RectangleShape(sf::Vector2f(30, 30));
    proj.shape.setFillColor(sf::Color::Red);
    proj.shape.setPosition(800, groundY + (std::rand() % 2 == 0 ? 0 : -100)); // ground or air
    proj.scored = false;
    projectiles.push_back(proj);
}

void Game::render() {
    window.clear(bgColor);
    window.draw(player);
    for (const auto& proj : projectiles)
        window.draw(proj.shape);
    // Draw score at the top
    sf::Font font;
    if (font.loadFromFile("assets/BBHSansBogle-Regular.ttf")) {
        sf::Text scoreText("Score: " + std::to_string(score), font, 24);
        scoreText.setFillColor(sf::Color::White);
        scoreText.setPosition(10, 10);
        window.draw(scoreText);
    }
    if (gameOver) {
        sf::Font font;
        if (font.loadFromFile("assets/BBHSansBogle-Regular.ttf")) {
            sf::Text text("Game Over! Press R to restart", font, 32);
            text.setFillColor(sf::Color::White);
            text.setPosition(200, 250);
            window.draw(text);
        }
    }
    window.display();
}

void Game::reset() {
    player.setPosition(100, groundY);
    isJumping = false;
    playerVelocityY = 0.f;
    projectiles.clear();
    gameOver = false;
    projectileClock.restart();
    score = 0;
}
