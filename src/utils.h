#pragma once

#include <SFML/Graphics.hpp>
#include "src/Level.h"
#include <optional>
#include <memory>

// World -> screen. `centr` is the world point shown in the middle of the screen.
sf::Vector2f viewCoordinates(sf::Vector2f pos, sf::Vector2f centr);

// Screen -> world. Exact inverse of viewCoordinates.
sf::Vector2f worldCoordinates(sf::Vector2f screenPos, sf::Vector2f centr);

// Where the game's low-resolution canvas goes in a window of `windowSize` pixels: as big as fits,
// scaled by a whole number when possible so the pixels stay crisp, centred with black bars around it.
sf::FloatRect canvasRect(sf::Vector2u windowSize);

std::pair<int, int> tailNumbers(sf::Vector2f pos);

void drawDebugGrid(sf::RenderTarget& target, sf::Vector2f centr, int cols, int rows, float tileSize, sf::Color color = sf::Color(255,255,0,128));

float distance(sf::Vector2f p1, sf::Vector2f p2);

float magnitude(sf::Vector2f v);

float dot(sf::Vector2f a, sf::Vector2f b);

// Marches from `from` towards `to` and returns the last empty point before the first solid tile,
// i.e. a point on the surface of the wall. Nothing is returned if no wall is hit within
// `maxDistance`, or if the wall is closer than `minDistance`.
// A point that's ended up inside rock (or any solid tile) moved out through the nearest face of its
// tile that isn't covered by more rock. False if it wasn't in rock.
bool pushOutOfRock(const Level& level, sf::Vector2f& point);

std::optional<sf::Vector2f> nearestSolidPoint(sf::Vector2f from, sf::Vector2f to, std::shared_ptr<const Level> level,
                                              float minDistance = 8.f, float maxDistance = 100.f);
