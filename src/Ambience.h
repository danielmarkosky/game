#pragma once

#include <SFML/Graphics.hpp>
#include <random>
#include <string>
#include <vector>

// The air of a place: things drifting in it (dust in the cave, fireflies in the garden, snow,
// embers, spores, pollen), and its light. Dark places get darker towards the edges of the screen,
// and lights glow in them: the player, benches, gems and things to pick up.
class Ambience
{
public:
    // Something that gives off light, in screen pixels.
    struct Light
    {
        sf::Vector2f position;
        float radius;
        sf::Color color;
    };

    Ambience();

    // Changes what drifts about, for a theme ("cave", "garden"...).
    void setTheme(const std::string& theme);
    // `cameraMove` is how far the camera moved, in screen pixels, since the last update: the
    // motes drift with it at their depth.
    void update(float dt, sf::Vector2f cameraMove);

    // The far motes go behind the rooms, the near ones in front of everything.
    void drawFar(sf::RenderTarget& target) const;
    void drawNear(sf::RenderTarget& target) const;
    // Darkens the edges by how dark the place is, then lets the lights shine through.
    void drawLight(sf::RenderTarget& target, const std::vector<Light>& lights) const;
    // A dark room: nothing to see but what's lit.
    void setDark(bool dark);
    bool isDark() const;
    // How dark the theme is: 0 bright daylight, 1 pitch black.
    float darkness() const;

private:
    struct Mote
    {
        sf::Vector2f position; // screen pixels
        float depth;           // 0.3 far .. 1 near
        float phase;
        float size;
        sf::Color color;
    };
    struct Style
    {
        int count = 0;
        sf::Vector2f drift;   // pixels a second
        float wobble = 0.f;   // pixels of sideways sway
        bool blink = false;   // fades in and out, like fireflies
        std::vector<sf::Color> colors;
        float darkness = 0.f;
        sf::Color light = sf::Color::White; // the colour of the player's own glow
    };
    static Style styleFor(const std::string& theme);
    void draw(sf::RenderTarget& target, bool near) const;

    Style style;
    std::string theme;
    std::vector<Mote> motes;
    float time = 0.f;
    std::mt19937 rng{11};
    sf::Texture vignette; // black, see-through in the middle
    sf::Texture glow;     // a soft white spot
    bool dark = false;
    mutable sf::RenderTexture lightMap; // in a dark room: the dark, with the lights cut out of it
};
