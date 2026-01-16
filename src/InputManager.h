#pragma once

#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <memory>

class InputManager
{
public:
    InputManager(std::shared_ptr<sf::RenderWindow> window);
    void update();
    bool isKeyPressed(sf::Keyboard::Key key) const;
    bool wasKeyJustPressed(sf::Keyboard::Key key) const; // New method
    bool wasMouseClicked() const;
    bool wasMouseReleased() const;
    sf::Vector2f mousePosition() const;
    sf::Vector2f scaleMouseInput(sf::Vector2i pixelPos) const;

private:
    std::shared_ptr<sf::RenderWindow> window;
    std::unordered_map<sf::Keyboard::Key, bool> m_currentKeys;
    std::unordered_map<sf::Keyboard::Key, bool> m_previousKeys;
    bool m_wasMousePressed;
    bool m_isMousePressed;
    sf::Vector2f m_mousePosition;
};
