#include "Particles.h"
#include "utils.h"
#include <algorithm>
#include <cmath>

void Particles::spawn(sf::Vector2f pos, const Burst& burst)
{
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    const auto& colors = burst.colors;
    for (int ii = 0; ii < burst.count; ++ii) {
        float angle = burst.direction + (unit(m_rng) - 0.5f) * burst.spread;
        float speed = burst.speed * (0.4f + 0.6f * unit(m_rng));
        float life = burst.life * (0.6f + 0.4f * unit(m_rng));
        sf::Color color = colors[static_cast<size_t>(unit(m_rng) * colors.size()) % colors.size()];
        m_particles.push_back({pos, {std::cos(angle) * speed, std::sin(angle) * speed}, life, life, burst.gravity, burst.size, color});
    }
}

void Particles::update(float dt)
{
    for (auto& p : m_particles) {
        p.vel.y -= p.gravity * dt;
        p.vel *= std::pow(0.9f, dt * 60.f);
        p.pos += p.vel * dt;
        p.life -= dt;
    }
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(), [](const Particle& p) { return p.life <= 0.f; }),
                      m_particles.end());
}

void Particles::draw(sf::RenderTarget& target, sf::Vector2f centr) const
{
    sf::RectangleShape dot;
    for (const auto& p : m_particles) {
        // Fade out over the last half of their life.
        float fade = std::min(1.f, 2.f * p.life / p.maxLife);
        sf::Color color = p.color;
        color.a = static_cast<sf::Uint8>(color.a * fade);
        sf::Vector2f screen = viewCoordinates(p.pos, centr);
        dot.setSize({p.size, p.size});
        dot.setPosition(std::floor(screen.x), std::floor(screen.y));
        dot.setFillColor(color);
        target.draw(dot);
    }
}

void Particles::clear()
{
    m_particles.clear();
}
