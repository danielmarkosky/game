#include <SFML/Graphics.hpp>
#include "src/Level.h"
#include "src/Player.h"
#include "src/utils.h"
#include <memory>

int main() {
    constexpr unsigned GAME_WIDTH = 320;
    constexpr unsigned GAME_HEIGHT = 180;

    auto desktop = sf::VideoMode::getDesktopMode();

    sf::RenderWindow window(desktop, "Celeste-like Game", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    sf::RenderTexture renderTexture;
    renderTexture.create(GAME_WIDTH, GAME_HEIGHT);
    renderTexture.setSmooth(false);

    auto level = std::make_shared<Level>();
    auto player = std::make_unique<Player>(sf::Vector2f{100.f, 100.f}, level);

    sf::Clock clock;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        float dt = clock.restart().asSeconds();
        player->update(dt);

        renderTexture.clear(sf::Color::Black);
        sf::Vector2f center = player->centre();
        level->draw(renderTexture, center);
        player->draw(renderTexture);
        // drawDebugGrid(renderTexture, center, 25, 25, Level::TILE_SIZE, sf::Color::Green);
        renderTexture.display();

        window.clear();
        sf::Sprite scaledSprite(renderTexture.getTexture());

        float scaleX = desktop.width / static_cast<float>(GAME_WIDTH);
        float scaleY = desktop.height / static_cast<float>(GAME_HEIGHT);
        float scale = std::min(scaleX, scaleY);
        scale = static_cast<int>(scale);

        float offsetX = (desktop.width - GAME_WIDTH * scale) / 2.f;
        float offsetY = (desktop.height - GAME_HEIGHT * scale) / 2.f;

        scaledSprite.setScale(scale, scale);
        scaledSprite.setPosition(offsetX, offsetY);

        window.draw(scaledSprite);
        window.display();
    }
    return 0;
}
