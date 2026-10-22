#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// Scenery: the props drawn in rooms (a Prop entity's `kind`), from assets/gfx/props.png. What each
// one is, where it is in the sheet, how it moves, whether it hangs from the ceiling and what light
// it gives, is in assets/gfx/props.txt. tools/make_art.py makes both.
struct PropInfo
{
    std::string name;
    sf::IntRect rect;     // the first frame, in the sheet
    int frames = 1;       // side by side
    float fps = 0.f;
    bool hangs = false;   // from the top of its tile; otherwise standing on the bottom of it
    sf::Color light;      // what light it gives, if lightRadius > 0
    float lightRadius = 0.f;
};

namespace props
{
    // Not a picture: a shaft of light falling from its tile down to the ground.
    inline const std::string LIGHT_SHAFT = "light_shaft";

    // Every prop there is, read from props.txt the first time.
    const std::vector<PropInfo>& all();
    // The one called that, or none.
    const PropInfo* find(const std::string& name);
    const sf::Texture& texture();
}
