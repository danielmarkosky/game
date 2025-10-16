#include "src/projectiles.h"

Projectile::Projectile() {}

void Projectile::update(float dt) {
    shape.move(velocity * dt);
}

void Projectile::draw(sf::RenderWindow& window) const {
    window.draw(shape);
}

sf::FloatRect Projectile::getBounds() const {
    return shape.getGlobalBounds();
}

sf::Vector2f Projectile::getVelocity() const {
    return velocity;
}

bool Projectile::handleCollision(const sf::FloatRect& playerBounds) const {
    return shape.getGlobalBounds().intersects(playerBounds);
}

void Projectile::reset(float startX, float startY, const sf::Vector2f& newVelocity) {
    shape.setPosition(startX, startY);
    velocity = newVelocity;
}
