#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <array>
#include <bitset>
#include <string>

// What the player can do, and the keys and gamepad buttons that do it. The first BINDABLE
// actions can be changed in the options; the rest are always on the same keys (Escape, Enter, Z)
// and buttons, so nobody can lock themselves out of the menus.
namespace controls
{
    enum class Action { Left, Right, Up, Down, Jump, Rope, Interact, Map, Restart, Pause, Confirm, Back, Zoom, Count };
    constexpr size_t BINDABLE = 9; // Left to Restart
    constexpr size_t ACTIONS = static_cast<size_t>(Action::Count);
    using Actions = std::bitset<ACTIONS>; // what one key or button press means

    constexpr size_t index(Action action) { return static_cast<size_t>(action); }
    // "JUMP", for the options; "jump", for the settings file.
    std::string actionLabel(Action action);
    std::string actionId(Action action);

    // Gamepads number their buttons differently. Xbox-style pads (and anything unrecognised)
    // put A, B, X, Y first, PlayStation pads square, cross, circle, triangle.
    enum class PadLayout { Xbox, PlayStation };
    PadLayout layoutFor(const std::string& padName);
    // The buttons where they are on the pad: South is A or cross, the one under your thumb.
    enum class PadButton { South, East, West, North, LeftShoulder, RightShoulder, Select, Start };
    int buttonNumber(PadButton button, PadLayout layout);
    // "A", "CROSS", "LB", "BUTTON 12".
    std::string buttonName(int button, PadLayout layout);

    // "SPACE", "W", "LSHIFT". "?" for keys without a name.
    std::string keyName(sf::Keyboard::Key key);
    // Back from keyName, ignoring case. Unknown if it isn't a key.
    sf::Keyboard::Key keyNamed(const std::string& name);
    // Keys that can't be bound: they work the menus.
    bool reserved(sf::Keyboard::Key key);

    struct Bindings
    {
        static constexpr int PAD_DEFAULT = -1; // the layout's default button for that action

        // Two keys an action (Unknown: none), and a gamepad button.
        std::array<std::array<sf::Keyboard::Key, 2>, BINDABLE> keys = {{
            {sf::Keyboard::A, sf::Keyboard::Left},
            {sf::Keyboard::D, sf::Keyboard::Right},
            {sf::Keyboard::W, sf::Keyboard::Up},
            {sf::Keyboard::S, sf::Keyboard::Down},
            {sf::Keyboard::Space, sf::Keyboard::Unknown},
            {sf::Keyboard::LShift, sf::Keyboard::RShift},
            {sf::Keyboard::E, sf::Keyboard::Unknown},
            {sf::Keyboard::M, sf::Keyboard::Tab},
            {sf::Keyboard::R, sf::Keyboard::Unknown},
        }};
        std::array<int, BINDABLE> buttons = [] { std::array<int, BINDABLE> b; b.fill(PAD_DEFAULT); return b; }();

        // Makes `key` the action's first key. If another action had it, that one gets the
        // action's old first key instead, so a key never does two things. Reserved keys are refused.
        bool bindKey(Action action, sf::Keyboard::Key key);
        // The same for a gamepad button. Start is refused: it pauses.
        bool bindButton(Action action, int button, PadLayout layout);

        Actions forKey(sf::Keyboard::Key key) const;
        Actions forButton(int button, PadLayout layout) const;
        int button(Action action, PadLayout layout) const; // -1 if none
        // "W / UP", for the options.
        std::string keysName(Action action) const;

        bool operator==(const Bindings&) const = default;
    };
}
