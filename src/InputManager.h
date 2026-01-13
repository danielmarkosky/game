#pragma once

#include <SFML/Graphics.hpp>
#include <unordered_map>

class InputManager
{
public:
    InputManager();
    void update();
    bool isKeyPressed(sf::Keyboard::Key key) const;
    bool isMouseClicked() const;
    sf::Vector2f mousePosition() const;

private:
    std::unordered_map<sf::Keyboard::Key, bool> m_currentKeys;
    std::unordered_map<sf::Keyboard::Key, bool> m_previousKeys;
    bool m_wasMousePressed;
    bool m_isMousePressed;
    sf::Vector2f m_mousePosition;
};