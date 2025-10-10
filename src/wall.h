#pragma once
#include <SFML/Graphics.hpp>
#include <memory>

class Wall {
public:
    explicit Wall(const sf::FloatRect& rect, std::shared_ptr<sf::Texture> texture = nullptr);
    sf::FloatRect getRect() const;
    void draw(sf::RenderWindow& window) const;
private:
    sf::FloatRect rect;
    sf::RectangleShape shape;
    std::shared_ptr<sf::Texture> texture;
};
