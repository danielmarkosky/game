#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <memory>
#include <optional>

// Tracks keyboard and mouse buttons, so callers can ask for "held" and "just pressed" / "just
// released". Keys come from the window's events: unlike sf::Keyboard::isKeyPressed they don't need
// macOS's Input Monitoring permission. The left mouse button is also polled while the window is
// focused, so a press counts if either way sees it.
class InputManager
{
public:
    InputManager(std::shared_ptr<sf::RenderWindow> window);
    // Call once per frame before polling the frame's events: forgets last frame's presses.
    void newFrame();
    void handleEvent(const sf::Event& event);
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

private:
    void releaseAll();

    std::shared_ptr<sf::RenderWindow> window;
    std::array<bool, sf::Keyboard::KeyCount> m_heldKeys{};
    // Pressed this frame, even if already let go again: a quick tap between frames still counts.
    std::array<bool, sf::Keyboard::KeyCount> m_pressedKeys{};
    bool m_mouseFromEvents = false;
    bool m_mouseFromPolling = false;
    std::optional<bool> m_focused; // unknown until the first frame
    bool m_mouseClicked = false;
    bool m_mouseReleased = false;
    float m_wheel = 0.f;
};
