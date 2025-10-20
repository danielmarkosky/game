#include "wall.h"

Wall::Wall(const sf::FloatRect& rect, std::shared_ptr<sf::Texture> texture)
    : rect(rect), texture(texture)
{
    shape.setPosition(rect.left, rect.top);
    shape.setSize(sf::Vector2f(rect.width, rect.height));
    if (texture) {
        shape.setTexture(texture.get());
        shape.setFillColor(sf::Color::White); // Texture visible, no outline
        shape.setOutlineThickness(0.f);
    } else {
        shape.setTexture(nullptr);
        shape.setFillColor(sf::Color::Transparent); // No fill
        shape.setOutlineThickness(1.f); // Visible outline
        shape.setOutlineColor(sf::Color::White);
    }
}

sf::FloatRect Wall::getBounds() const {
    return rect;
}

void Wall::draw(sf::RenderWindow& window) const {
    window.draw(shape);
}
