#include "game.h"
#include "levels/first_level.h"
#include <cmath>

Game::Game()
    : window(sf::VideoMode(320, 180), "Jump & Dodge Game", sf::Style::Close)
{
    level = std::make_shared<FirstLevel>();
    player = std::make_unique<Player>(sf::Vector2f(32.f, 180.f - 32.f), level);

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
            if (event.key.code == sf::Keyboard::Left)
                player->moveLeft();
            if (event.key.code == sf::Keyboard::Right)
                player->moveRight();
            if (event.key.code == sf::Keyboard::Space)
                player->jump();
        }
        if (!gameOver && event.type == sf::Event::KeyReleased) {
            if (event.key.code == sf::Keyboard::Left || event.key.code == sf::Keyboard::Right)
                player->stopHorizontal();
        }
        if (gameOver && event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::R) {
            reset();
        }
    }
}

void Game::update(float dt) {
    player->update(dt);
    projectileSpawnTimer += dt;
    if (projectileSpawnTimer > 1.0f) {
        spawnProjectile();
        projectileSpawnTimer = 0.f;
    }
    for (auto& proj : projectiles) {
        proj.shape.move(proj.velocity * dt);
    }
    handleCollisions();
}

void Game::render() {
    window.clear(sf::Color::Black);
    level->draw(window);
    player->draw(window);
    for (const auto& proj : projectiles) {
        window.draw(proj.shape);
    }
    window.draw(scoreText);
    if (gameOver) {
        sf::Text overText("Game Over! Press R to restart", font, 16);
        overText.setFillColor(sf::Color::Red);
        overText.setPosition(40.f, 80.f);
        window.draw(overText);
    }
    window.display();
}

void Game::spawnProjectile() {
    Projectile proj;
    proj.shape = sf::CircleShape(8.f);
    proj.shape.setOutlineThickness(2.f);
    proj.shape.setOutlineColor(sf::Color::Red);
    proj.shape.setFillColor(sf::Color::Transparent);
    float y = 32.f + static_cast<float>(std::rand() % 120);
    proj.shape.setPosition(320.f, y);
    proj.velocity = sf::Vector2f(-120.f, 0.f);
    projectiles.push_back(proj);
}

void Game::handleCollisions() {
    auto playerBounds = player->getBounds();
    for (auto it = projectiles.begin(); it != projectiles.end();) {
        if (it->shape.getGlobalBounds().intersects(playerBounds)) {
            gameOver = true;
            return;
        }
        if (it->shape.getPosition().x + it->shape.getRadius() * 2 < 0) {
            score++;
            scoreText.setString("Score: " + std::to_string(score));
            it = projectiles.erase(it);
        } else {
            ++it;
        }
    }
}

void Game::reset() {
    score = 0;
    scoreText.setString("Score: 0");
    gameOver = false;
    projectiles.clear();
    player = std::make_unique<Player>(sf::Vector2f(32.f, 180.f - 32.f), level);
}

