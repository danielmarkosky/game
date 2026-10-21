#include "InputManager.h"
#include "Config.h"
#include <SFML/Graphics.hpp>

InputManager::InputManager(std::shared_ptr<sf::RenderWindow> window) : window(window) {}

void InputManager::update()
{
    // sf::Keyboard and sf::Mouse read the whole system, not just our window.
    const bool focused = window->hasFocus();

    m_previousKeys = m_currentKeys;
    for (int ii = 0; ii < sf::Keyboard::KeyCount; ++ii) {
        m_currentKeys[ii] = focused && sf::Keyboard::isKeyPressed(static_cast<sf::Keyboard::Key>(ii));
    }
    m_wasMousePressed = m_isMousePressed;
    m_isMousePressed = focused && sf::Mouse::isButtonPressed(sf::Mouse::Left);
    m_mousePosition = scaleMouseInput(sf::Mouse::getPosition(*window));
}

bool InputManager::isKeyPressed(sf::Keyboard::Key key) const
{
    if (key < 0 || key >= sf::Keyboard::KeyCount) return false;
    return m_currentKeys[key];
}

bool InputManager::wasKeyJustPressed(sf::Keyboard::Key key) const
{
    if (key < 0 || key >= sf::Keyboard::KeyCount) return false;
    return m_currentKeys[key] && !m_previousKeys[key];
}

bool InputManager::isMousePressed() const
{
    return m_isMousePressed;
}

bool InputManager::wasMouseClicked() const
{
    return (not m_wasMousePressed and m_isMousePressed);
}

bool InputManager::wasMouseReleased() const
{
    return (m_wasMousePressed and not m_isMousePressed);
}

sf::Vector2f InputManager::mousePosition() const
{
    return m_mousePosition;
}

sf::Vector2f InputManager::scaleMouseInput(sf::Vector2i pixelPos) const
{
    sf::Vector2u windowSize = window->getSize();

    float scaleX = static_cast<float>(windowSize.x) / config::GAME_WIDTH;
    float scaleY = static_cast<float>(windowSize.y) / config::GAME_HEIGHT;

    return sf::Vector2f(
        static_cast<float>(pixelPos.x) / scaleX,
        static_cast<float>(pixelPos.y) / scaleY
    );
}
