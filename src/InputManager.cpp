#include "InputManager.h"
#include <SFML/Graphics.hpp>

InputManager::InputManager() : isMousePressed(false) {}

void InputManager::update()
{
    m_previousKeys = m_currentKeys;
    for (int ii = 0; ii < sf::Keyboard::KeyCount; ++ii) {
        sf::Keyboard::Key key = static_cast<sf::Keyboard::Key>(ii);
        m_currentKeys[key] = sf::Keyboard::isKeyPressed(key);
    }
    isMousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
}

bool InputManager::isKeyPressed(sf::Keyboard::Key key) const
{
    return m_currentKeys.at(key);
}
