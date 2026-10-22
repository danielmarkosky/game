#pragma once

// What Game's own .cpp files share: Game.cpp (playing), GameMenus.cpp and GameDrawing.cpp.
// Not for anything else.

#include "src/Config.h"
#include "src/Hud.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace game_detail
{
    inline constexpr float HALF_W = config::GAME_WIDTH * 0.5f;
    inline constexpr float HALF_H = config::GAME_HEIGHT * 0.5f;
    inline constexpr float DEATH_TIME = 0.5f;      // how long you stay dead before respawning
    inline constexpr float SHAKE_TIME = 0.3f;
    inline constexpr float SHAKE_PIXELS = 3.f;
    inline constexpr float TITLE_TIME = 3.f;       // a room's name shows this long
    inline constexpr float MESSAGE_TIME = 4.f;
    inline constexpr float NOTICE_TIME = 3.f;
    inline constexpr float TRANSITION_TIME = 0.5f; // panning from one room to the next
    inline constexpr float FADE_TIME = 0.6f;       // fading in after travelling
    inline const char* const TITLE = "Tether";     // the game's name: the window's title and the title screen's
    inline const std::vector<sf::Color> CONFETTI = {hud::GOLD, sf::Color(255, 120, 160), sf::Color(120, 220, 255), sf::Color(160, 240, 120)};

    // A view that only draws inside `area` of the screen, leaving the coordinates as they are.
    inline sf::View clipView(sf::FloatRect area)
    {
        const sf::Vector2f screen(config::GAME_WIDTH, config::GAME_HEIGHT);
        sf::View view(area);
        view.setViewport({area.left / screen.x, area.top / screen.y, area.width / screen.x, area.height / screen.y});
        return view;
    }

    inline float easeInOut(float t)
    {
        t = std::clamp(t, 0.f, 1.f);
        return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) * 0.5f;
    }
}
