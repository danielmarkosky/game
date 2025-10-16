#pragma once
#include "projectiles.h"
#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>

class ProjectileManager {
public:
    void addProjectile(std::unique_ptr<Projectile> projectile);
    void updateAll(float dt);
    void drawAll(sf::RenderWindow& window) const;
    void clear();

private:
    std::vector<std::unique_ptr<Projectile>> projectiles;
};
