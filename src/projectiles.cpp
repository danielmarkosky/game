#include "src/projectiles.h"
#include <cstdlib>

ProjectileManager::ProjectileManager(unsigned int vw, unsigned int vh)
    : viewWidth(vw), viewHeight(vh) {}

void ProjectileManager::spawnProjectile() {
    Projectile proj;
    proj.shape = sf::CircleShape(8.f);
    proj.shape.setOutlineThickness(2.f);
    proj.shape.setOutlineColor(sf::Color::Red);
    proj.shape.setFillColor(sf::Color::Transparent);
    float y = 32.f + static_cast<float>(std::rand() % (viewHeight - 64));
    proj.shape.setPosition(viewWidth, y);
    proj.velocity = sf::Vector2f(-120.f, 0.f);
    projectiles.push_back(proj);
}

void ProjectileManager::update(float dt) {
    spawnTimer += dt;
    if (spawnTimer > 1.0f) {
        spawnProjectile();
        spawnTimer = 0.f;
    }
    for (auto& proj : projectiles) {
        proj.shape.move(proj.velocity * dt);
    }
    // Remove projectiles that are out of the screen
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(), [this](const Projectile& proj) {
            float x = proj.shape.getPosition().x;
            float radius = proj.shape.getRadius();
            return (x + radius * 2 < 0) || (x > viewWidth);
        }),
        projectiles.end()
    );
}

void ProjectileManager::draw(sf::RenderWindow& window) const {
    for (const auto& proj : projectiles) {
        window.draw(proj.shape);
    }
}

bool ProjectileManager::handleCollisions(const sf::FloatRect& playerBounds, int& score) {
    for (auto it = projectiles.begin(); it != projectiles.end();) {
        if (it->shape.getGlobalBounds().intersects(playerBounds)) {
            return true;
        }
        if (it->shape.getPosition().x + it->shape.getRadius() * 2 < 0) {
            score++;
            it = projectiles.erase(it);
        } else {
            ++it;
        }
    }
    return false;
}

void ProjectileManager::reset() {
    projectiles.clear();
    spawnTimer = 0.f;
}
