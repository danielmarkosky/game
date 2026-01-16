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
    // Increased iterations to prevent stretching with heavy mass ratios
    const int iterations = 80;
    const float stiffness = 1.0f;
    const float damping = 0.99f;

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
            if (currentLength < 0.001f) continue;

            float diff = (currentLength - m_segmentLength) / currentLength;

            // Only correct if stretched (allow slack / compression)
            if (diff > 0.0f) {
                float wA = a.isFixed ? 0.f : 1.0f;
                float wB = b.isFixed ? 0.f : 1.0f;

                // Player (index 0) is heavy (low inverse mass)
                if (ii == 0 && !a.isFixed) {
                    wA = 0.02f;
                }

                float totalMass = wA + wB;

                if (totalMass > 0.0001f) {
                    a.position += stiffness * diff * delta * (wA / totalMass);
                    b.position -= stiffness * diff * delta * (wB / totalMass);
                }
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
    // 1. Draw Lines
    for (size_t ii = 0; ii < nodes.size() - 1; ++ii) {
        sf::Vector2f p1 = viewCoordinates(nodes[ii].position, centr);
        sf::Vector2f p2 = viewCoordinates(nodes[ii + 1].position, centr);

        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(std::floor(p1.x), std::floor(p1.y)), sf::Color(200, 200, 200)),
            sf::Vertex(sf::Vector2f(std::floor(p2.x), std::floor(p2.y)), sf::Color(200, 200, 200))
        };
        target.draw(line, 2, sf::Lines);
    }

    // 2. Draw Points (Nodes)
    sf::CircleShape dot(1.f);
    dot.setOrigin(1.f, 1.f); // Center origin

    for (size_t ii = 0; ii < nodes.size(); ++ii) {
        sf::Vector2f p = viewCoordinates(nodes[ii].position, centr);
        dot.setPosition(std::floor(p.x), std::floor(p.y));

        // Color: Red for anchor, current player node, White for others
        if (ii == nodes.size() - 1) dot.setFillColor(sf::Color::Red);       // Wall Anchor
        else if (ii == 0) dot.setFillColor(sf::Color(100, 255, 100));       // Player Connection
        else dot.setFillColor(sf::Color::White);                            // Rope segments

        target.draw(dot);
    }
}

const std::vector<RopeNode>& Rope::getNodes() const { return nodes; }

sf::Vector2f Rope::getNodePosition(int index) const {
    if (index >= 0 && index < nodes.size()) return nodes[index].position;
    return {0.f, 0.f};
}

sf::Vector2f Rope::getNodeVelocity(int index, float dt) const {
    if (index >= 0 && index < nodes.size() && dt > 0.0001f) {
        return (nodes[index].position - nodes[index].previousPosition) / dt;
    }
    return {0.f, 0.f};
}

void Rope::setNodePosition(int index, sf::Vector2f pos) {
    if (index >= 0 && index < nodes.size()) {
        nodes[index].position = pos;
    }
}

void Rope::setNodeVelocity(int index, sf::Vector2f velocity, float dt) {
    if (index >= 0 && index < nodes.size() && !nodes[index].isFixed && dt > 0.0001f) {
        nodes[index].previousPosition = nodes[index].position - velocity * dt;
    }
}

void Rope::applyForceToNode(int index, sf::Vector2f force) {
    if (index >= 0 && index < nodes.size() && !nodes[index].isFixed) {
        nodes[index].position += force;
    }
}

void Rope::applyFrictionToNode(int index, float factor)
{
    if (index >= 0 && index < nodes.size() && !nodes[index].isFixed) {
        sf::Vector2f velocity = nodes[index].position - nodes[index].previousPosition;
        velocity *= factor;
        nodes[index].previousPosition = nodes[index].position - velocity;
    }
}
