#include "InputManager.h"
#include "Config.h"
#include "utils.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>

namespace
{
    using controls::Action;

    // SFML gives the d-pad's up as positive, except on Linux.
#ifdef __linux__
    constexpr float HAT_UP = -1.f;
#else
    constexpr float HAT_UP = 1.f;
#endif
}

InputManager::InputManager(std::shared_ptr<sf::RenderWindow> window) : window(window) {}

void InputManager::setBindings(const controls::Bindings& newBindings)
{
    bindings = newBindings;
}

void InputManager::newFrame()
{
    m_pressedKeys.fill(false);
    m_pressedButtons.fill(false);
    m_pressedDirections.reset();
    m_mouseClicked = false;
    m_mouseReleased = false;
    m_wheel = 0.f;

    if (!window) return;
    // sf::Mouse reads the whole system, not just our window.
    if (!m_focused) m_focused = window->hasFocus();
    const bool polled = *m_focused && sf::Mouse::isButtonPressed(sf::Mouse::Left);
    if (polled && !m_mouseFromPolling && !m_mouseFromEvents) m_mouseClicked = true;
    if (!polled && m_mouseFromPolling && !m_mouseFromEvents) m_mouseReleased = true;
    m_mouseFromPolling = polled;
}

std::optional<InputManager::Press> InputManager::handleEvent(const sf::Event& event)
{
    const auto known = [](sf::Keyboard::Key key) { return key >= 0 && key < sf::Keyboard::KeyCount; };
    // Listens to the gamepad `id` from now on, if it wasn't already.
    const auto usePad = [this](unsigned id) {
        m_usingPad = true;
        if (m_pad == id) return;
        m_pad = id;
        m_layout = controls::layoutFor(sf::Joystick::getIdentification(id).name.toAnsiString());
        m_heldButtons.fill(false);
        m_stick = m_hat = {};
    };
    switch (event.type) {
        case sf::Event::KeyPressed:
            m_focused = true; // only the focused window gets key and click events
            m_usingPad = false;
            if (known(event.key.code)) {
                m_heldKeys[event.key.code] = true;
                m_pressedKeys[event.key.code] = true;
            }
            return Press{bindings.forKey(event.key.code), event.key.code, -1};
        case sf::Event::KeyReleased:
            if (known(event.key.code)) m_heldKeys[event.key.code] = false;
            break;
        case sf::Event::MouseMoved:
            m_usingPad = false;
            break;
        case sf::Event::MouseButtonPressed:
            m_focused = true;
            m_usingPad = false;
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
        case sf::Event::MouseWheelScrolled:
            if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) m_wheel += event.mouseWheelScroll.delta;
            break;
        case sf::Event::JoystickButtonPressed: {
            const unsigned button = event.joystickButton.button;
            if (button >= sf::Joystick::ButtonCount) break;
            usePad(event.joystickButton.joystickId);
            m_heldButtons[button] = true;
            m_pressedButtons[button] = true;
            const int number = static_cast<int>(button);
            return Press{bindings.forButton(number, m_layout), sf::Keyboard::Unknown, number};
        }
        case sf::Event::JoystickButtonReleased:
            if (m_pad == event.joystickButton.joystickId && event.joystickButton.button < sf::Joystick::ButtonCount) {
                m_heldButtons[event.joystickButton.button] = false;
            }
            break;
        case sf::Event::JoystickMoved: {
            const auto& move = event.joystickMove;
            const float value = move.position / 100.f;
            float* axis = nullptr;
            float sign = 1.f;
            switch (move.axis) {
                case sf::Joystick::X: axis = &m_stick.x; break;
                case sf::Joystick::Y: axis = &m_stick.y; sign = -1.f; break; // the stick's down is positive
                case sf::Joystick::PovX: axis = &m_hat.x; break;
                case sf::Joystick::PovY: axis = &m_hat.y; sign = HAT_UP; break;
                default: break;
            }
            if (!axis) break;
            if (m_pad != move.joystickId) {
                // Another pad's stick only takes over when it's really pushed, not drifting.
                if (std::abs(value) < DIRECTION_AT) break;
                usePad(move.joystickId);
            }
            const controls::Actions before = padDirections();
            *axis = std::clamp(value * sign, -1.f, 1.f);
            if (std::abs(value) > DEAD_ZONE) m_usingPad = true;
            const controls::Actions pressed = padDirections() & ~before;
            m_pressedDirections |= pressed;
            if (pressed.any()) return Press{pressed, sf::Keyboard::Unknown, -1};
            break;
        }
        case sf::Event::JoystickDisconnected:
            if (m_pad == event.joystickConnect.joystickId) {
                m_pad.reset();
                m_heldButtons.fill(false);
                m_stick = m_hat = {};
                m_usingPad = false;
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
    return std::nullopt;
}

void InputManager::releaseAll()
{
    m_heldKeys.fill(false);
    m_heldButtons.fill(false);
    m_stick = m_hat = {};
    if (isMousePressed()) m_mouseReleased = true;
    m_mouseFromEvents = false;
    m_mouseFromPolling = false;
}

bool InputManager::isHeld(Action action) const
{
    const size_t a = controls::index(action);
    if (a < controls::BINDABLE) {
        for (sf::Keyboard::Key key : bindings.keys[a]) {
            if (isKeyPressed(key)) return true;
        }
    }
    if (padDirections()[a]) return true;
    const int button = bindings.button(action, m_layout);
    return button >= 0 && m_heldButtons[static_cast<size_t>(button)];
}

bool InputManager::wasPressed(Action action) const
{
    const size_t a = controls::index(action);
    if (a < controls::BINDABLE) {
        for (sf::Keyboard::Key key : bindings.keys[a]) {
            if (wasKeyJustPressed(key)) return true;
        }
    }
    if (m_pressedDirections[a]) return true;
    const int button = bindings.button(action, m_layout);
    return button >= 0 && m_pressedButtons[static_cast<size_t>(button)];
}

bool InputManager::usingPad() const
{
    return m_usingPad;
}

controls::PadLayout InputManager::padLayout() const
{
    return m_layout;
}

sf::Vector2f InputManager::padDirection() const
{
    // The d-pad when it's pressed, or else the stick.
    if (m_hat != sf::Vector2f()) return m_hat;
    const auto dead = [](float v) { return std::abs(v) < DEAD_ZONE ? 0.f : v; };
    return {dead(m_stick.x), dead(m_stick.y)};
}

controls::Actions InputManager::padDirections() const
{
    const sf::Vector2f d = m_hat != sf::Vector2f() ? m_hat : m_stick;
    controls::Actions held;
    held[controls::index(Action::Left)] = d.x <= -DIRECTION_AT;
    held[controls::index(Action::Right)] = d.x >= DIRECTION_AT;
    held[controls::index(Action::Up)] = d.y >= DIRECTION_AT;
    held[controls::index(Action::Down)] = d.y <= -DIRECTION_AT;
    return held;
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

float InputManager::wheelDelta() const
{
    return m_wheel;
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
