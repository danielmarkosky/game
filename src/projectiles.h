#pragma once
#include <SFML/Graphics.hpp>

class Projectile {
public:
    Projectile();
    virtual ~Projectile() = default;
    virtual void update(float dt); // Moves the projectile
    virtual void draw(sf::RenderWindow& window) const; // Draws the projectile
    sf::FloatRect getBounds() const; // Returns bounding box
    sf::Vector2f getVelocity() const; // Returns velocity
    bool handleCollision(const sf::FloatRect& playerBounds) const; // Checks collision with player
    void reset(float startX, float startY, const sf::Vector2f& newVelocity); // Resets position/velocity
    sf::CircleShape shape;
    sf::Vector2f velocity;
};
