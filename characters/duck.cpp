#include "duck.h"
#include <SFML/Graphics.hpp>

Duck::Duck(const sf::Vector2f& startPos, std::shared_ptr<Level> level)
    : Player(startPos, level) // Call base Player constructor
{
    // Set duck-specific parameters
    moveSpeed = 100.f;
    jumpStrength = 220.f;
    gravity = 400.f;
    // Set duck texture (already handled by Player, but can override if needed)
    if (texture.loadFromFile("assets/duckpngsp.png")) {
        sprite.setTexture(texture);
    } else {
        // Fallback: transparent fill, yellow outline
        sprite.setColor(sf::Color::Transparent);
        // Optionally, you can draw a rectangle outline if no texture
    }
}

void Duck::draw(sf::RenderWindow& window) {
    // If texture is loaded, draw sprite
    if (texture.getSize().x > 0 && texture.getSize().y > 0) {
        window.draw(sprite);
    } else {
        // Draw a rectangle outline if no texture
        sf::RectangleShape outline(sf::Vector2f(sprite.getGlobalBounds().width, sprite.getGlobalBounds().height));
        outline.setPosition(sprite.getPosition());
        outline.setFillColor(sf::Color::Transparent);
        outline.setOutlineThickness(2.f);
        outline.setOutlineColor(sf::Color::Yellow);
        window.draw(outline);
    }
}
// Duck uses Player's update methods by default
