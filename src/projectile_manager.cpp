#include "projectile_manager.h"

void ProjectileManager::addProjectile(std::unique_ptr<Projectile> projectile) {
    projectiles.push_back(std::move(projectile));
}

void ProjectileManager::updateAll(float dt) {
    for (auto& proj : projectiles) {
        proj->update(dt);
    }
}

void ProjectileManager::drawAll(sf::RenderWindow& window) const {
    for (const auto& proj : projectiles) {
        proj->draw(window);
    }
}

void ProjectileManager::clear() {
    projectiles.clear();
}
