#include "utils.h"
#include "Config.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float HALF_W = config::GAME_WIDTH * 0.5f;
    constexpr float HALF_H = config::GAME_HEIGHT * 0.5f;
}

sf::Vector2f viewCoordinates(sf::Vector2f pos , sf::Vector2f centr)
{
    return {pos.x + HALF_W - centr.x, -pos.y + HALF_H + centr.y};
}

sf::Vector2f worldCoordinates(sf::Vector2f screenPos, sf::Vector2f centr)
{
    float wx = screenPos.x - HALF_W + centr.x;
    float wy = centr.y + HALF_H - screenPos.y;
    return {wx, wy};
}

sf::FloatRect canvasRect(sf::Vector2u windowSize)
{
    float scale = std::min(static_cast<float>(windowSize.x) / config::GAME_WIDTH,
                           static_cast<float>(windowSize.y) / config::GAME_HEIGHT);
    if (scale >= 1.f) scale = std::floor(scale);
    const float w = config::GAME_WIDTH * scale, h = config::GAME_HEIGHT * scale;
    return {std::floor((windowSize.x - w) * 0.5f), std::floor((windowSize.y - h) * 0.5f), w, h};
}

std::pair<int, int> tailNumbers(sf::Vector2f pos)
{
    int x = static_cast<int>(std::floor(pos.x / config::TILE_SIZE));
    int y = static_cast<int>(std::floor(pos.y / config::TILE_SIZE));
    return {x, y};
}

void drawDebugGrid(sf::RenderTarget& target, sf::Vector2f centr, int cols, int rows, float tileSize, sf::Color color)
{
    for (int x = 0; x <= cols; ++x) {
        float gx = x * tileSize;
        auto p1 = viewCoordinates(sf::Vector2f(gx, 0), centr);
        auto p2 = viewCoordinates(sf::Vector2f(gx, rows * tileSize), centr);
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(std::floor(p1.x), std::floor(p1.y)), color),
            sf::Vertex(sf::Vector2f(std::floor(p2.x), std::floor(p2.y)), color)
        };
        target.draw(line, 2, sf::Lines);
    }
    for (int y = 0; y <= rows; ++y) {
        float gy = y * tileSize;
        auto p1 = viewCoordinates(sf::Vector2f(0, gy), centr);
        auto p2 = viewCoordinates(sf::Vector2f(cols * tileSize, gy), centr);
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(std::floor(p1.x), std::floor(p1.y)), color),
            sf::Vertex(sf::Vector2f(std::floor(p2.x), std::floor(p2.y)), color)
        };
        target.draw(line, 2, sf::Lines);
    }

    sf::Vector2f logicPos(0.f, 0.f);
    auto screenPos = viewCoordinates(logicPos, centr);

    sf::CircleShape dot(2.f);
    dot.setFillColor(sf::Color::Red);
    dot.setOrigin(2.f, 2.f);
    dot.setPosition(std::floor(screenPos.x), std::floor(screenPos.y));
    target.draw(dot);
}

float distance(sf::Vector2f p1, sf::Vector2f p2)
{
    return magnitude(p1 - p2);
}

float magnitude(sf::Vector2f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

float dot(sf::Vector2f a, sf::Vector2f b)
{
    return a.x * b.x + a.y * b.y;
}

std::optional<sf::Vector2f> nearestSolidPoint(sf::Vector2f from, sf::Vector2f to, std::shared_ptr<const Level> level,
                                              float minDistance, float maxDistance)
{
    float dist = distance(from, to);
    if (dist < 0.001f) return std::nullopt;

    sf::Vector2f dirNorm = (to - from) / dist;
    const float step = 0.25f;

    sf::Vector2f lastEmpty = from;
    for (float currentDist = 0.f; currentDist <= maxDistance; currentDist += step) {
        sf::Vector2f currentPos = from + dirNorm * currentDist;
        auto [tileX, tileY] = tailNumbers(currentPos);
        if (level->isSolid(tileX, tileY)) {
            if (currentDist < minDistance) return std::nullopt;
            return lastEmpty;
        }
        lastEmpty = currentPos;
    }
    return std::nullopt;
}
