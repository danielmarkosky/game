#pragma once

#include "src/Controls.h"
#include <SFML/Graphics.hpp>
#include <array>
#include <memory>
#include <optional>

// Tracks the keyboard, the mouse and a gamepad, so callers can ask for "held" and "just pressed"
// / "just released", by key or by what the player bound it to. Keys come from the window's
// events: unlike sf::Keyboard::isKeyPressed they don't need macOS's Input Monitoring permission.
// The left mouse button is also polled while the window is focused, so a press counts if either
// way sees it. Of the gamepads, the one last pressed is the one listened to.
class InputManager
{
public:
    // A key or button going down: what it is, and what it means with the bindings.
    struct Press
    {
        controls::Actions actions;
        sf::Keyboard::Key key = sf::Keyboard::Unknown;
        int button = -1; // on the gamepad
    };

    InputManager(std::shared_ptr<sf::RenderWindow> window);
    void setBindings(const controls::Bindings& bindings);
    // Call once per frame before polling the frame's events: forgets last frame's presses.
    void newFrame();
    // A press, if the event is one. Pushing the stick or d-pad over in a direction counts as a
    // press of that direction.
    std::optional<Press> handleEvent(const sf::Event& event);

    // By what the player bound it to: on the keyboard or the gamepad.
    bool isHeld(controls::Action action) const;
    bool wasPressed(controls::Action action) const; // this frame

    bool isKeyPressed(sf::Keyboard::Key key) const;
    bool wasKeyJustPressed(sf::Keyboard::Key key) const;
    bool isMousePressed() const;
    bool wasMouseClicked() const;
    bool wasMouseReleased() const;
    // Mouse wheel turned this frame: positive is up (away from you).
    float wheelDelta() const;
    // In game pixels (0..GAME_WIDTH, 0..GAME_HEIGHT), not window pixels.
    sf::Vector2f mousePosition() const;
    sf::Vector2f scaleMouseInput(sf::Vector2i pixelPos) const;

    // The gamepad was used since the mouse last moved: aim with the stick, and show its buttons.
    bool usingPad() const;
    controls::PadLayout padLayout() const;
    // Where the left stick or the d-pad points, each axis -1 to 1 with up and right positive.
    // Zero inside the dead zone.
    sf::Vector2f padDirection() const;

    static constexpr float DEAD_ZONE = 0.3f;   // of the stick's travel, ignored
    static constexpr float DIRECTION_AT = 0.5f; // how far over the stick counts as pressing a direction

private:
    void releaseAll();
    controls::Actions padDirections() const; // Left, Right, Up and Down that the pad holds now

    std::shared_ptr<sf::RenderWindow> window;
    controls::Bindings bindings;
    std::array<bool, sf::Keyboard::KeyCount> m_heldKeys{};
    // Pressed this frame, even if already let go again: a quick tap between frames still counts.
    std::array<bool, sf::Keyboard::KeyCount> m_pressedKeys{};
    bool m_mouseFromEvents = false;
    bool m_mouseFromPolling = false;
    std::optional<bool> m_focused; // unknown until the first frame
    bool m_mouseClicked = false;
    bool m_mouseReleased = false;
    float m_wheel = 0.f;

    // The gamepad
    std::optional<unsigned> m_pad;
    controls::PadLayout m_layout = controls::PadLayout::Xbox;
    std::array<bool, sf::Joystick::ButtonCount> m_heldButtons{};
    std::array<bool, sf::Joystick::ButtonCount> m_pressedButtons{};
    sf::Vector2f m_stick, m_hat; // -1 to 1, up positive
    controls::Actions m_pressedDirections; // this frame
    bool m_usingPad = false;
};
