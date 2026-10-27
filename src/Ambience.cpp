#include "Ambience.h"
#include "Config.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float W = config::GAME_WIDTH, H = config::GAME_HEIGHT;

    sf::Texture makeVignette()
    {
        sf::Image image;
        image.create(config::GAME_WIDTH, config::GAME_HEIGHT);
        for (unsigned y = 0; y < config::GAME_HEIGHT; ++y) {
            for (unsigned x = 0; x < config::GAME_WIDTH; ++x) {
                const float dx = (x - W * 0.5f) / (W * 0.5f), dy = (y - H * 0.5f) / (H * 0.5f);
                const float d = std::min(1.f, std::sqrt(dx * dx * 0.8f + dy * dy * 1.2f) / 1.3f);
                image.setPixel(x, y, sf::Color(0, 0, 0, static_cast<sf::Uint8>(255.f * std::pow(d, 1.6f))));
            }
        }
        sf::Texture t;
        t.loadFromImage(image);
        return t;
    }

    sf::Texture makeGlow()
    {
        constexpr unsigned SIZE = 64;
        sf::Image image;
        image.create(SIZE, SIZE);
        for (unsigned y = 0; y < SIZE; ++y) {
            for (unsigned x = 0; x < SIZE; ++x) {
                const float dx = (x + 0.5f) / (SIZE * 0.5f) - 1.f, dy = (y + 0.5f) / (SIZE * 0.5f) - 1.f;
                const float d = std::min(1.f, std::sqrt(dx * dx + dy * dy));
                image.setPixel(x, y, sf::Color(255, 255, 255, static_cast<sf::Uint8>(255.f * (1.f - d) * (1.f - d))));
            }
        }
        sf::Texture t;
        t.loadFromImage(image);
        t.setSmooth(true);
        return t;
    }
}

Ambience::Ambience() : vignette(makeVignette()), glow(makeGlow()) {}

Ambience::Style Ambience::styleFor(const std::string& theme)
{
    Style s;
    if (theme == "cave") {
        s = {40, {-2.f, 3.f}, 4.f, false, {sf::Color(200, 190, 230, 110), sf::Color(150, 140, 190, 90)}, 0.55f, sf::Color(170, 160, 220)};
    } else if (theme == "garden") {
        s = {26, {3.f, -2.f}, 9.f, true, {sf::Color(230, 250, 140, 230), sf::Color(180, 245, 120, 200)}, 0.3f, sf::Color(200, 240, 170)};
    } else if (theme == "snow") {
        s = {70, {-9.f, 18.f}, 6.f, false, {sf::Color(255, 255, 255, 220), sf::Color(220, 230, 255, 170)}, 0.2f, sf::Color(200, 215, 255)};
    } else if (theme == "ember") {
        s = {45, {3.f, -16.f}, 5.f, true, {sf::Color(255, 170, 70, 230), sf::Color(255, 110, 50, 210), sf::Color(255, 230, 140, 200)}, 0.45f, sf::Color(255, 160, 90)};
    } else if (theme == "ruins") {
        s = {40, {1.f, -6.f}, 7.f, true, {sf::Color(110, 240, 210, 220), sf::Color(255, 140, 220, 200), sf::Color(190, 255, 240, 180)}, 0.6f, sf::Color(150, 255, 225)};
    } else if (theme == "ice") {
        s = {50, {-3.f, 6.f}, 5.f, true, {sf::Color(200, 240, 255, 220), sf::Color(140, 210, 255, 190), sf::Color(255, 255, 255, 200)}, 0.5f, sf::Color(170, 220, 255)};
    } else if (theme == "sky") {
        // The same night, up high: loose stars blowing past on the wind, the camp's magic among them.
        s = {40, {24.f, -3.f}, 4.f, true, {sf::Color(255, 255, 255, 200), sf::Color(190, 210, 255, 170), sf::Color(130, 235, 255, 160)},
             0.2f, sf::Color(200, 215, 255)};
    } else if (theme == "grotto") {
        // Magic in the air: specks of light, blue, violet and gold, glowing and fading as they drift up.
        s = {60, {1.f, -5.f}, 8.f, true, {sf::Color(120, 240, 255, 230), sf::Color(200, 150, 255, 220), sf::Color(255, 225, 140, 210), sf::Color(220, 255, 250, 200)},
             0.45f, sf::Color(150, 220, 255)};
    } else if (theme == "glade") {
        // A night outside, the same magic in the air, and fireflies.
        s = {55, {2.f, -3.f}, 10.f, true, {sf::Color(230, 250, 140, 230), sf::Color(130, 235, 255, 220), sf::Color(210, 160, 255, 210)},
             0.3f, sf::Color(190, 230, 255)};
    } else if (theme == "still") {
        // Between the stars: a few tiny ones adrift, twinkling.
        s = {30, {0.5f, -0.5f}, 2.f, true, {sf::Color(220, 225, 255, 200), sf::Color(190, 170, 255, 170), sf::Color(255, 250, 220, 160)},
             0.35f, sf::Color(180, 190, 255)};
    } else if (theme == "mine") {
        // Dust in the lamp light, and the cold air of the mountain.
        s = {30, {-6.f, 2.f}, 4.f, false, {sf::Color(210, 190, 160, 110), sf::Color(170, 160, 150, 90)}, 0.4f, sf::Color(230, 200, 150)};
    } else if (theme == "daylight") {
        s = {24, {10.f, -1.f}, 8.f, false, {sf::Color(255, 250, 210, 200), sf::Color(255, 255, 255, 160)}, 0.f, sf::Color::White};
    }
    return s;
}

void Ambience::setTheme(const std::string& name)
{
    if (name == theme) return;
    theme = name;
    style = styleFor(name);
    // Quiet: fewer of them, and fainter, than the styles say, so the air is there without crowding
    // the screen.
    style.count = style.count * 3 / 5;
    for (sf::Color& c : style.colors) c.a = static_cast<sf::Uint8>(c.a * 3 / 4);
    // The last place's motes fade out while the new ones fade in, the first time straight away.
    const bool first = time == 0.f;
    if (!first) {
        leaving = std::move(motes);
    } else {
        shownDarkness = style.darkness;
    }
    motes.clear();
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    for (int i = 0; i < style.count; ++i) {
        const float depth = i % 3 == 0 ? 1.f : 0.35f + 0.3f * unit(rng);
        motes.push_back({{unit(rng) * W, unit(rng) * H}, depth, unit(rng) * 6.28f, depth > 0.9f ? 2.f : 1.f,
                         style.colors[static_cast<size_t>(i) % style.colors.size()], first ? 1.f : 0.f});
    }
}

float Ambience::darkness() const
{
    return shownDarkness;
}

void Ambience::update(float dt, sf::Vector2f cameraMove)
{
    time += dt;
    // Into a darker or lighter place, or a dark room, the light changes over a moment.
    constexpr float CHANGE = 2.f; // of the way a second
    const float darkness = style.darkness + (blendDarkness - style.darkness) * blendAmount;
    shownDarkness += std::clamp(darkness - shownDarkness, -CHANGE * dt, CHANGE * dt);
    shownDark += std::clamp((dark ? 1.f : 0.f) - shownDark, -CHANGE * dt, CHANGE * dt);
    for (Mote& m : leaving) m.fade -= dt * 1.5f;
    std::erase_if(leaving, [](const Mote& m) { return m.fade <= 0.f; });
    for (Mote& m : motes) m.fade = std::min(1.f, m.fade + dt * 1.5f);
    for (std::vector<Mote>* list : {&motes, &leaving}) {
        for (Mote& m : *list) {
            // Nearer ones move faster, and with the camera more: a little depth.
            m.position += style.drift * (0.5f + 0.5f * m.depth) * dt - cameraMove * m.depth;
            m.position.x = std::fmod(m.position.x + W + 8.f, W + 8.f);
            m.position.y = std::fmod(m.position.y + H + 8.f, H + 8.f);
        }
    }
}

void Ambience::draw(sf::RenderTarget& target, bool near) const
{
    sf::RectangleShape dot;
    for (const std::vector<Mote>* list : {&leaving, &motes}) {
        for (const Mote& m : *list) {
            if ((m.depth > 0.9f) != near) continue;
            sf::Color c = m.color;
            c.a = static_cast<sf::Uint8>(c.a * m.fade);
            if (style.blink) c.a = static_cast<sf::Uint8>(c.a * (0.5f + 0.5f * std::sin(time * 2.3f + m.phase * 3.f)));
            if (!near) c.a = static_cast<sf::Uint8>(c.a * 0.6f);
            const float sway = std::sin(time * 1.3f + m.phase) * style.wobble * m.depth;
            dot.setSize({m.size, m.size});
            dot.setPosition(std::floor(m.position.x - 4.f + sway), std::floor(m.position.y - 4.f));
            dot.setFillColor(c);
            target.draw(dot);
        }
    }
}

void Ambience::drawFar(sf::RenderTarget& target) const
{
    draw(target, false);
}

void Ambience::drawNear(sf::RenderTarget& target) const
{
    draw(target, true);
}

void Ambience::setBlend(const std::string& other, float amount)
{
    blendAmount = other.empty() ? 0.f : amount;
    if (!other.empty()) blendDarkness = styleFor(other).darkness;
}

void Ambience::setDark(bool isDark)
{
    dark = isDark;
}

bool Ambience::isDark() const
{
    return dark;
}

void Ambience::drawLight(sf::RenderTarget& target, const std::vector<Light>& lights) const
{
    if (shownDark > 0.f) {
        // The dark, with the lights adding to it, laid over everything: where it's black you see
        // nothing, where a light falls you see what's there. Coming into it, or out, it closes in
        // or lifts over a moment.
        const sf::Vector2u size = target.getSize();
        if (lightMap.getSize() != size) lightMap.create(size.x, size.y);
        const auto towards = [&](float black) { return static_cast<sf::Uint8>(255.f + (black - 255.f) * shownDark); };
        lightMap.clear(sf::Color(towards(14.f), towards(12.f), towards(22.f)));
        sf::Sprite spot(glow);
        const float glowSize = static_cast<float>(glow.getSize().x);
        for (const Light& light : lights) {
            spot.setOrigin(glowSize * 0.5f, glowSize * 0.5f);
            spot.setScale(light.radius * 2.6f / glowSize, light.radius * 2.6f / glowSize);
            spot.setPosition(light.position);
            sf::Color colour = light.color;
            colour.a = 255;
            spot.setColor(colour);
            lightMap.draw(spot, sf::BlendAdd);
            lightMap.draw(spot, sf::BlendAdd);
        }
        lightMap.display();
        target.draw(sf::Sprite(lightMap.getTexture()), sf::BlendMultiply);
        return;
    }
    if (shownDarkness > 0.f) {
        sf::Sprite shade(vignette);
        shade.setColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(255.f * shownDarkness)));
        target.draw(shade);
    }
    // Lights add their colour, so they show most where it's dark.
    sf::Sprite spot(glow);
    const float size = static_cast<float>(glow.getSize().x);
    for (const Light& light : lights) {
        spot.setOrigin(size * 0.5f, size * 0.5f);
        spot.setScale(light.radius * 2.f / size, light.radius * 2.f / size);
        spot.setPosition(light.position);
        spot.setColor(light.color);
        target.draw(spot, sf::BlendAdd);
    }
}
