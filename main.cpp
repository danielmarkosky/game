#include <SFML/Graphics.hpp>
#include "src/Level.h"
#include "src/Player.h"
#include "src/utils.h"
#include "src/InputManager.h"
#include <memory>

#include "src/Rope.h"

int main() {
    // TODO rope shooting
    constexpr unsigned GAME_WIDTH = 320;
    constexpr unsigned GAME_HEIGHT = 180;

    constexpr unsigned k = 4;

    sf::RenderWindow window(
        sf::VideoMode(GAME_WIDTH * k, GAME_HEIGHT * k),
        "Celeste-like Game",
        sf::Style::Close
    );
    window.setFramerateLimit(60);

    sf::RenderTexture renderTexture;
    renderTexture.create(GAME_WIDTH, GAME_HEIGHT);
    renderTexture.setSmooth(false);

    auto level = std::make_shared<Level>();
    auto inputManager = std::make_shared<InputManager>();
    auto rope = std::make_shared<Rope>(sf::Vector2f{20,80}, sf::Vector2f{100,100}, level);
    auto player = std::make_shared<Player>(level, inputManager, rope);

    sf::Clock clock;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        float dt = clock.restart().asSeconds();
        const float MAX_DT = 1.0f / 30.0f;
        dt = std::min(dt, MAX_DT);

        inputManager->update();
        // level->update(dt);
        player->update(dt);

        renderTexture.clear(sf::Color(24, 28, 36));
        sf::Vector2f center = player->centre();
        level->draw(renderTexture, center);
        player->draw(renderTexture);
        rope->draw(renderTexture, center);
        // drawDebugGrid(renderTexture, center, 25, 25, Level::TILE_SIZE, sf::Color::Green);
        renderTexture.display();

        window.clear();
        sf::Sprite scaledSprite(renderTexture.getTexture());
        scaledSprite.setScale(static_cast<float>(k), static_cast<float>(k));
        window.draw(scaledSprite);
        window.display();
    }
    return 0;
}
