#include "InputManager.h"
#include <SFML/Graphics.hpp>

InputManager::InputManager(std::shared_ptr<sf::RenderWindow> window) : window(window), m_isMousePressed(false), m_wasMousePressed(false) {}

void InputManager::update()
{
    m_previousKeys = m_currentKeys;
    for (int ii = 0; ii < sf::Keyboard::KeyCount; ++ii) {
        sf::Keyboard::Key key = static_cast<sf::Keyboard::Key>(ii);
        m_currentKeys[key] = sf::Keyboard::isKeyPressed(key);
    }
    m_wasMousePressed = m_isMousePressed;
    m_isMousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
    m_mousePosition = scaleMouseInput(sf::Mouse::getPosition(*window));
}

bool InputManager::isKeyPressed(sf::Keyboard::Key key) const
{
    // Using count to avoid exceptions for uninitialized keys in obscure cases
    if (m_currentKeys.count(key)) return m_currentKeys.at(key);
    return false;
}

bool InputManager::wasKeyJustPressed(sf::Keyboard::Key key) const
{
    bool isPressed = false;
    if (m_currentKeys.count(key)) isPressed = m_currentKeys.at(key);

    bool wasPressed = false;
    if (m_previousKeys.count(key)) wasPressed = m_previousKeys.at(key);

    return isPressed && !wasPressed;
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

    constexpr float LOGICAL_WIDTH = 320.f;
    constexpr float LOGICAL_HEIGHT = 180.f;

    float scaleX = static_cast<float>(windowSize.x) / LOGICAL_WIDTH;
    float scaleY = static_cast<float>(windowSize.y) / LOGICAL_HEIGHT;

    return sf::Vector2f(
        static_cast<float>(pixelPos.x) / scaleX,
        static_cast<float>(pixelPos.y) / scaleY
    );
}
