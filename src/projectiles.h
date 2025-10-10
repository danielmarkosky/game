#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

struct Projectile {
    sf::CircleShape shape;
    sf::Vector2f velocity;
};

class ProjectileManager {
public:
    ProjectileManager(unsigned int viewWidth, unsigned int viewHeight);
    void spawnProjectile();
    void update(float dt);
    void draw(sf::RenderWindow& window) const;
    bool handleCollisions(const sf::FloatRect& playerBounds, int& score);
    void reset();
private:
    std::vector<Projectile> projectiles;
    float spawnTimer = 0.f;
    unsigned int viewWidth, viewHeight;
};
