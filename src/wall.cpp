#include "src/wall.h"

Wall::Wall(const sf::FloatRect& rect, std::shared_ptr<sf::Texture> texture)
    : rect(rect), texture(std::move(texture))
{
    shape.setPosition(rect.left, rect.top);
    shape.setSize(sf::Vector2f(rect.width, rect.height));
    if (this->texture && this->texture->getSize().x > 0 && this->texture->getSize().y > 0) {
        shape.setTexture(this->texture.get());
        shape.setFillColor(sf::Color::White); // Texture will be visible
    } else {
        shape.setFillColor(sf::Color(128,128,128)); // Default color if no texture or failed to load
    }
}

sf::FloatRect Wall::getRect() const {
    return rect;
}

void Wall::draw(sf::RenderWindow& window) const {
    window.draw(shape);
}
