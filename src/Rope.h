#pragma once

#include <SFML/Graphics.hpp>

class Rope : public sf::Drawable {
public:
    Rope(const sf::Vector2f& start, const sf::Vector2f& end, float segmentLength);

    void setStart(const sf::Vector2f& start);

    void setEnd(const sf::Vector2f& end);

    void update();

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
}