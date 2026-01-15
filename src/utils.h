#pragma once

#include <SFML/Graphics.hpp>
#include "src/Level.h"
#include <optional>
#include <memory>

sf::Vector2f viewCoordinates(sf::Vector2f pos, sf::Vector2f centr);

sf::Vector2f worldCoordinates(sf::Vector2f screenPos, sf::Vector2f centr);

std::pair<int, int> tailNumbers(sf::Vector2f pos);

void drawDebugGrid(sf::RenderTarget& target, sf::Vector2f centr, int cols, int rows, float tileSize, sf::Color color = sf::Color(255,255,0,128));

float distance(sf::Vector2f p1, sf::Vector2f p2);

std::optional<sf::Vector2f> nearestSolidPoint(sf::Vector2f from, sf::Vector2f to, std::shared_ptr<const Level> level);