#include "InputManager.h"
#include <SFML/Graphics.hpp>

InputManager::InputManager() : m_isMousePressed(false) {}

void InputManager::update()
{
    m_previousKeys = m_currentKeys;
    for (int ii = 0; ii < sf::Keyboard::KeyCount; ++ii) {
        sf::Keyboard::Key key = static_cast<sf::Keyboard::Key>(ii);
        m_currentKeys[key] = sf::Keyboard::isKeyPressed(key);
    }
    m_wasMousePressed = m_isMousePressed;
    m_isMousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
}

bool InputManager::isKeyPressed(sf::Keyboard::Key key) const
{
    return m_currentKeys.at(key);
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
