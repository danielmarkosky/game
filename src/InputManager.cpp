#include "InputManager.h"
#include "Config.h"
#include "utils.h"
#include <SFML/Graphics.hpp>

InputManager::InputManager(std::shared_ptr<sf::RenderWindow> window) : window(window) {}

void InputManager::newFrame()
{
    m_pressedKeys.fill(false);
    m_mouseClicked = false;
    m_mouseReleased = false;

    if (!window) return;
    // sf::Mouse reads the whole system, not just our window.
    if (!m_focused) m_focused = window->hasFocus();
    const bool polled = *m_focused && sf::Mouse::isButtonPressed(sf::Mouse::Left);
    if (polled && !m_mouseFromPolling && !m_mouseFromEvents) m_mouseClicked = true;
    if (!polled && m_mouseFromPolling && !m_mouseFromEvents) m_mouseReleased = true;
    m_mouseFromPolling = polled;
}

void InputManager::handleEvent(const sf::Event& event)
{
    const auto known = [](sf::Keyboard::Key key) { return key >= 0 && key < sf::Keyboard::KeyCount; };
    switch (event.type) {
        case sf::Event::KeyPressed:
            m_focused = true; // only the focused window gets key and click events
            if (known(event.key.code)) {
                m_heldKeys[event.key.code] = true;
                m_pressedKeys[event.key.code] = true;
            }
            break;
        case sf::Event::KeyReleased:
            if (known(event.key.code)) m_heldKeys[event.key.code] = false;
            break;
        case sf::Event::MouseButtonPressed:
            m_focused = true;
            if (event.mouseButton.button == sf::Mouse::Left) {
                if (!isMousePressed()) m_mouseClicked = true;
                m_mouseFromEvents = true;
            }
            break;
        case sf::Event::MouseButtonReleased:
            if (event.mouseButton.button == sf::Mouse::Left && m_mouseFromEvents) {
                m_mouseFromEvents = false;
                if (!m_mouseFromPolling) m_mouseReleased = true;
            }
            break;
        case sf::Event::GainedFocus:
            m_focused = true;
            break;
        case sf::Event::LostFocus:
            m_focused = false;
            // The key-up events will go to another window, so don't leave anything stuck down.
            releaseAll();
            break;
        default:
            break;
    }
}

void InputManager::releaseAll()
{
    m_heldKeys.fill(false);
    if (isMousePressed()) m_mouseReleased = true;
    m_mouseFromEvents = false;
    m_mouseFromPolling = false;
}

bool InputManager::isKeyPressed(sf::Keyboard::Key key) const
{
    if (key < 0 || key >= sf::Keyboard::KeyCount) return false;
    return m_heldKeys[key];
}

bool InputManager::wasKeyJustPressed(sf::Keyboard::Key key) const
{
    if (key < 0 || key >= sf::Keyboard::KeyCount) return false;
    return m_pressedKeys[key];
}

bool InputManager::isMousePressed() const
{
    return m_mouseFromEvents || m_mouseFromPolling;
}

bool InputManager::wasMouseClicked() const
{
    return m_mouseClicked;
}

bool InputManager::wasMouseReleased() const
{
    return m_mouseReleased;
}

sf::Vector2f InputManager::mousePosition() const
{
    return scaleMouseInput(sf::Mouse::getPosition(*window));
}

sf::Vector2f InputManager::scaleMouseInput(sf::Vector2i pixelPos) const
{
    // The canvas may not fill the whole window (fullscreen adds black bars around it).
    const sf::FloatRect canvas = canvasRect(window->getSize());
    return {
        (static_cast<float>(pixelPos.x) - canvas.left) * config::GAME_WIDTH / canvas.width,
        (static_cast<float>(pixelPos.y) - canvas.top) * config::GAME_HEIGHT / canvas.height
    };
}
