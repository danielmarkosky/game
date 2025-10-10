#include "levels/first_level.h"
#include "src/wall.h"
#include <SFML/Graphics.hpp>
#include <memory>

namespace {

std::shared_ptr<sf::Texture> loadTexture(const std::string& path) {
    static std::map<std::string, std::shared_ptr<sf::Texture>> cache;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    auto tex = std::make_shared<sf::Texture>();
    if (tex->loadFromFile(path)) {
        cache[path] = tex;
        return tex;
    }
    return nullptr;
}

} // anonymous namespace

FirstLevel::FirstLevel() {
    // Left wall
    addWall(Wall(
        sf::FloatRect(0.f, 0.f, 8.f, 180.f),
        loadTexture("assets/wall.png")
    ));
    // Right wall
    addWall(Wall(
        sf::FloatRect(320.f - 8.f, 0.f, 8.f, 180.f),
        loadTexture("assets/wall.png")
    ));
    // Top wall
    addWall(Wall(
        sf::FloatRect(0.f, 0.f, 320.f, 8.f),
        loadTexture("assets/wall.png")
    ));
    // Bottom wall
    addWall(Wall(
        sf::FloatRect(0.f, 180.f - 8.f, 320.f, 8.f),
        loadTexture("assets/floor.png")
    ));
}
