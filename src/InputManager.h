#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <memory>

// Polls keyboard and mouse once per frame and remembers the previous frame, so callers can ask
// for "just pressed" / "just released". Reports nothing while the window isn't focused.
class InputManager
{
public:
    InputManager(std::shared_ptr<sf::RenderWindow> window);
    void update();
    bool isKeyPressed(sf::Keyboard::Key key) const;
    bool wasKeyJustPressed(sf::Keyboard::Key key) const;
    bool isMousePressed() const;
    bool wasMouseClicked() const;
    bool wasMouseReleased() const;
    // In game pixels (0..GAME_WIDTH, 0..GAME_HEIGHT), not window pixels.
    sf::Vector2f mousePosition() const;
    sf::Vector2f scaleMouseInput(sf::Vector2i pixelPos) const;

private:
    std::shared_ptr<sf::RenderWindow> window;
    std::array<bool, sf::Keyboard::KeyCount> m_currentKeys{};
    std::array<bool, sf::Keyboard::KeyCount> m_previousKeys{};
    bool m_isMousePressed = false;
    bool m_wasMousePressed = false;
    sf::Vector2f m_mousePosition;
};
