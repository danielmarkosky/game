#pragma once

#include <SFML/Graphics.hpp>
#include <random>
#include <vector>

// Little square bits of dust, sparks and confetti. Purely cosmetic.
class Particles
{
public:
    struct Burst
    {
        int count = 8;
        float speed = 40.f;          // how fast they fly out, pixels per second
        float spread = 6.2832f;      // angle range in radians around `direction`
        float direction = 1.5708f;   // radians, 0 = right, pi/2 = up
        float life = 0.4f;           // seconds
        float gravity = 200.f;
        float size = 1.f;
        std::vector<sf::Color> colors = {sf::Color::White};
    };

    void spawn(sf::Vector2f pos, const Burst& burst);
    void update(float dt);
    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;
    void clear();

private:
    struct Particle
    {
        sf::Vector2f pos;
        sf::Vector2f vel;
        float life;
        float maxLife;
        float gravity;
        float size;
        sf::Color color;
    };

    std::vector<Particle> m_particles;
    std::mt19937 m_rng{12345};
};
