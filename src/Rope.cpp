#include "Rope.h"
#include "utils.h"
#include <cmath>

Rope::Rope(sf::Vector2f startPos, sf::Vector2f endPos, std::shared_ptr<Level> level)
    : m_level(std::move(level))
{
    float dis = distance(startPos, endPos);

    int numSegments = static_cast<int>(std::max(1.f, std::round(dis / 4.f)));

    m_segmentLength = dis / static_cast<float>(numSegments);

    int numNodes = numSegments + 1;

    nodes.reserve(numNodes);
    for (int ii = 0; ii < numNodes; ii++) {
        float t = static_cast<float>(ii) / static_cast<float>(numSegments);
        sf::Vector2f pos = startPos + t * (endPos - startPos);

        bool isFixed = (ii == numNodes - 1);
        nodes.emplace_back(pos, isFixed);
    }
}

void Rope::update(float dt)
{
    const int iterations = 30;
    const float stiffness = 1.0f;
    const float damping = 0.98f;

    for (auto& node : nodes) {
        if (node.isFixed) continue;

        sf::Vector2f velocity = node.position - node.previousPosition;
        node.previousPosition = node.position;

        node.position += velocity * damping;
        node.position.y -= 890.f * dt * dt;
    }

    for (int it = 0; it < iterations; ++it) {
        for (size_t ii = 0; ii < nodes.size() - 1; ii++) {
            RopeNode& a = nodes[ii];
            RopeNode& b = nodes[ii + 1];

            sf::Vector2f delta = b.position - a.position;
            float currentLength = std::sqrt(delta.x * delta.x + delta.y * delta.y);

            if (currentLength < 0.05f) continue;

            float diff = (currentLength - m_segmentLength) / currentLength;

            if (!a.isFixed) {
                a.position += stiffness * diff * delta * 0.5f;
            }
            if (!b.isFixed) {
                b.position -= stiffness * diff * delta * 0.5f;
            }
        }
        for (auto& node : nodes) {
            if (node.isFixed) continue;
            auto [tileX, tileY] = tailNumbers(node.position);
            if (m_level->isSolid(tileX, tileY)) {
                node.position = node.previousPosition;
            }
        }
    }
}

void Rope::draw(sf::RenderTarget& target, sf::Vector2f centr) const
{
    for (size_t ii = 0; ii < nodes.size() - 1; ++ii) {
        sf::Vector2f p1 = viewCoordinates(nodes[ii].position, centr);
        sf::Vector2f p2 = viewCoordinates(nodes[ii + 1].position, centr);

        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(std::floor(p1.x), std::floor(p1.y)), sf::Color::White),
            sf::Vertex(sf::Vector2f(std::floor(p2.x), std::floor(p2.y)), sf::Color::White)
        };
        target.draw(line, 2, sf::Lines);
    }

    sf::CircleShape nodeShape(1.f);
    nodeShape.setFillColor(sf::Color(255, 192, 203));
    nodeShape.setOrigin(1.f, 1.f);

    for (const auto& node : nodes) {
        sf::Vector2f p = viewCoordinates(node.position, centr);
        nodeShape.setPosition(std::floor(p.x), std::floor(p.y));
        target.draw(nodeShape);
    }
}

const std::vector<RopeNode>& Rope::getNodes() const
{
    return nodes;
}
