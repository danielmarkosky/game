#include "Controls.h"
#include <algorithm>
#include <cctype>
#include <utility>

namespace controls
{
    namespace
    {
        using K = sf::Keyboard;

        struct ActionInfo
        {
            const char* label;
            const char* id;
        };
        const ActionInfo ACTION_INFO[ACTIONS] = {
            {"LEFT", "left"}, {"RIGHT", "right"}, {"UP / REEL IN", "up"}, {"DOWN / LET OUT", "down"},
            {"JUMP", "jump"}, {"ROPE", "rope"}, {"TALK / SIT", "interact"}, {"MAP", "map"}, {"RESTART", "restart"},
            {"PAUSE", "pause"}, {"OK", "confirm"}, {"BACK", "back"}, {"ZOOM", "zoom"},
        };

        // The pad buttons the actions from Jump to Restart start on. The directions are always
        // the left stick and the d-pad.
        const PadButton DEFAULT_BUTTONS[] = {PadButton::South, PadButton::West, PadButton::East, PadButton::North, PadButton::Select};
        constexpr bool onButton(size_t action) { return action >= index(Action::Jump) && action < BINDABLE; }

        const std::pair<K::Key, const char*> KEY_NAMES[] = {
            {K::Escape, "ESCAPE"}, {K::LControl, "LCTRL"}, {K::LShift, "LSHIFT"}, {K::LAlt, "LALT"}, {K::LSystem, "LSYSTEM"},
            {K::RControl, "RCTRL"}, {K::RShift, "RSHIFT"}, {K::RAlt, "RALT"}, {K::RSystem, "RSYSTEM"}, {K::Menu, "MENU"},
            {K::LBracket, "LBRACKET"}, {K::RBracket, "RBRACKET"}, {K::Semicolon, ";"}, {K::Comma, ","}, {K::Period, "."},
            {K::Apostrophe, "'"}, {K::Slash, "/"}, {K::Backslash, "BACKSLASH"}, {K::Grave, "GRAVE"}, {K::Equal, "="},
            {K::Hyphen, "-"}, {K::Space, "SPACE"}, {K::Enter, "ENTER"}, {K::Backspace, "BACKSPACE"}, {K::Tab, "TAB"},
            {K::PageUp, "PAGEUP"}, {K::PageDown, "PAGEDOWN"}, {K::End, "END"}, {K::Home, "HOME"}, {K::Insert, "INSERT"},
            {K::Delete, "DELETE"}, {K::Add, "NUM+"}, {K::Subtract, "NUM-"}, {K::Multiply, "NUMSTAR"}, {K::Divide, "NUM/"},
            {K::Left, "LEFT"}, {K::Right, "RIGHT"}, {K::Up, "UP"}, {K::Down, "DOWN"}, {K::Pause, "PAUSE"},
        };

        std::string upper(std::string text)
        {
            for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return text;
        }
    }

    std::string actionLabel(Action action)
    {
        return ACTION_INFO[index(action)].label;
    }

    std::string actionId(Action action)
    {
        return ACTION_INFO[index(action)].id;
    }

    PadLayout layoutFor(const std::string& padName)
    {
        const std::string name = upper(padName);
        // A DualShock 4 calls itself just "Wireless Controller"; an Xbox one says Xbox first.
        if (name == "WIRELESS CONTROLLER") return PadLayout::PlayStation;
        for (const char* sony : {"PLAYSTATION", "DUALSHOCK", "DUALSENSE", "PS3", "PS4", "PS5", "SONY"}) {
            if (name.find(sony) != std::string::npos) return PadLayout::PlayStation;
        }
        return PadLayout::Xbox;
    }

    int buttonNumber(PadButton button, PadLayout layout)
    {
        //                                South East West North LB  RB  Select Start
        static constexpr int XBOX[] = {0, 1, 2, 3, 4, 5, 6, 7};
        static constexpr int PLAYSTATION[] = {1, 2, 0, 3, 4, 5, 8, 9};
        return (layout == PadLayout::PlayStation ? PLAYSTATION : XBOX)[static_cast<size_t>(button)];
    }

    std::string buttonName(int button, PadLayout layout)
    {
        static const char* XBOX[] = {"A", "B", "X", "Y", "LB", "RB", "BACK", "START", "LSTICK", "RSTICK"};
        static const char* PLAYSTATION[] = {"SQUARE", "CROSS", "CIRCLE", "TRIANGLE", "L1", "R1", "L2", "R2", "SHARE", "OPTIONS", "L3", "R3"};
        if (button < 0) return "-";
        if (layout == PadLayout::Xbox && button < int(std::size(XBOX))) return XBOX[button];
        if (layout == PadLayout::PlayStation && button < int(std::size(PLAYSTATION))) return PLAYSTATION[button];
        return "BUTTON " + std::to_string(button);
    }

    std::string keyName(sf::Keyboard::Key key)
    {
        if (key >= K::A && key <= K::Z) return std::string(1, static_cast<char>('A' + (key - K::A)));
        if (key >= K::Num0 && key <= K::Num9) return std::string(1, static_cast<char>('0' + (key - K::Num0)));
        if (key >= K::Numpad0 && key <= K::Numpad9) return "NUM" + std::to_string(key - K::Numpad0);
        if (key >= K::F1 && key <= K::F15) return "F" + std::to_string(key - K::F1 + 1);
        for (const auto& [k, name] : KEY_NAMES) {
            if (k == key) return name;
        }
        return "?";
    }

    sf::Keyboard::Key keyNamed(const std::string& name)
    {
        const std::string wanted = upper(name);
        for (int key = 0; key < K::KeyCount; ++key) {
            if (keyName(static_cast<K::Key>(key)) == wanted) return static_cast<K::Key>(key);
        }
        return K::Unknown;
    }

    bool reserved(sf::Keyboard::Key key)
    {
        return key == K::Escape || key == K::Enter || key == K::Unknown;
    }

    bool Bindings::bindKey(Action action, sf::Keyboard::Key key)
    {
        const size_t a = index(action);
        if (a >= BINDABLE || reserved(key)) return false;
        if (keys[a][0] == key) return true;
        const K::Key old = keys[a][0];
        for (auto& pair : keys) {
            for (K::Key& k : pair) {
                if (k == key) k = old;
            }
        }
        keys[a][0] = key;
        // A second key the same as the first is no second key.
        for (auto& pair : keys) {
            if (pair[1] == pair[0]) pair[1] = K::Unknown;
        }
        return true;
    }

    bool Bindings::bindButton(Action action, int newButton, PadLayout layout)
    {
        const size_t a = index(action);
        if (!onButton(a) || newButton < 0 || newButton == buttonNumber(PadButton::Start, layout)) return false;
        const int old = button(action, layout);
        for (size_t other = 0; other < BINDABLE; ++other) {
            if (other != a && onButton(other) && button(static_cast<Action>(other), layout) == newButton) buttons[other] = old;
        }
        buttons[a] = newButton;
        return true;
    }

    int Bindings::button(Action action, PadLayout layout) const
    {
        const size_t a = index(action);
        if (!onButton(a)) return -1;
        return buttons[a] == PAD_DEFAULT ? buttonNumber(DEFAULT_BUTTONS[a - index(Action::Jump)], layout) : buttons[a];
    }

    Actions Bindings::forKey(sf::Keyboard::Key key) const
    {
        Actions actions;
        if (key == K::Unknown) return actions;
        for (size_t a = 0; a < BINDABLE; ++a) {
            if (keys[a][0] == key || keys[a][1] == key) actions.set(a);
        }
        if (key == K::Escape) actions.set(index(Action::Pause)).set(index(Action::Back));
        if (key == K::P && !actions.any()) actions.set(index(Action::Pause));
        if (key == K::Enter) actions.set(index(Action::Confirm));
        if (key == K::Z && !actions.any()) actions.set(index(Action::Zoom));
        return actions;
    }

    Actions Bindings::forButton(int pressed, PadLayout layout) const
    {
        Actions actions;
        for (size_t a = 0; a < BINDABLE; ++a) {
            if (button(static_cast<Action>(a), layout) == pressed) actions.set(a);
        }
        if (pressed == buttonNumber(PadButton::Start, layout)) actions.set(index(Action::Pause));
        if (pressed == buttonNumber(PadButton::South, layout)) actions.set(index(Action::Confirm));
        if (pressed == buttonNumber(PadButton::East, layout)) actions.set(index(Action::Back));
        if (pressed == buttonNumber(PadButton::LeftShoulder, layout) || pressed == buttonNumber(PadButton::RightShoulder, layout)) {
            actions.set(index(Action::Zoom));
        }
        return actions;
    }

    std::string Bindings::keysName(Action action) const
    {
        const auto& pair = keys[index(action)];
        if (pair[0] == K::Unknown) return pair[1] == K::Unknown ? "-" : keyName(pair[1]);
        return pair[1] == K::Unknown ? keyName(pair[0]) : keyName(pair[0]) + " / " + keyName(pair[1]);
    }
}
