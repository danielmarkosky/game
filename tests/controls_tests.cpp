#include <gtest/gtest.h>
#include "src/Controls.h"
#include "src/InputManager.h"

using controls::Action;
using controls::Bindings;
using controls::PadLayout;
using K = sf::Keyboard;

namespace
{
    bool has(controls::Actions actions, Action action)
    {
        return actions[controls::index(action)];
    }

    sf::Event button(sf::Event::EventType type, unsigned number, unsigned pad = 0)
    {
        sf::Event event{};
        event.type = type;
        event.joystickButton.joystickId = pad;
        event.joystickButton.button = number;
        return event;
    }

    sf::Event axis(sf::Joystick::Axis which, float position, unsigned pad = 0)
    {
        sf::Event event{};
        event.type = sf::Event::JoystickMoved;
        event.joystickMove.joystickId = pad;
        event.joystickMove.axis = which;
        event.joystickMove.position = position;
        return event;
    }

    sf::Event key(sf::Event::EventType type, K::Key code)
    {
        sf::Event event{};
        event.type = type;
        event.key.code = code;
        return event;
    }
}

TEST(Controls, TheUsualKeys) {
    const Bindings b;
    EXPECT_TRUE(has(b.forKey(K::Space), Action::Jump));
    EXPECT_TRUE(has(b.forKey(K::A), Action::Left));
    EXPECT_TRUE(has(b.forKey(K::Left), Action::Left));
    EXPECT_TRUE(has(b.forKey(K::LShift), Action::Rope));
    EXPECT_TRUE(has(b.forKey(K::Tab), Action::Map));
    // Escape pauses and goes back, Enter is OK, whatever is bound.
    EXPECT_TRUE(has(b.forKey(K::Escape), Action::Pause));
    EXPECT_TRUE(has(b.forKey(K::Escape), Action::Back));
    EXPECT_TRUE(has(b.forKey(K::Enter), Action::Confirm));
    EXPECT_TRUE(b.forKey(K::Q).none());
}

TEST(Controls, BindingAKeyTakesItFromWhereItWas) {
    Bindings b;
    // Jump onto W, which was Up's: Up gets Space instead.
    EXPECT_TRUE(b.bindKey(Action::Jump, K::W));
    EXPECT_TRUE(has(b.forKey(K::W), Action::Jump));
    EXPECT_FALSE(has(b.forKey(K::W), Action::Up));
    EXPECT_TRUE(has(b.forKey(K::Space), Action::Up));
    EXPECT_TRUE(has(b.forKey(K::Up), Action::Up));
    // A key never does two of the bindable things.
    for (int k = 0; k < K::KeyCount; ++k) {
        const controls::Actions actions = b.forKey(static_cast<K::Key>(k));
        int bound = 0;
        for (size_t a = 0; a < controls::BINDABLE; ++a) bound += actions[a];
        EXPECT_LE(bound, 1) << controls::keyName(static_cast<K::Key>(k));
    }
    // The keys that work the menus can't be taken.
    EXPECT_FALSE(b.bindKey(Action::Jump, K::Escape));
    EXPECT_FALSE(b.bindKey(Action::Jump, K::Enter));
    EXPECT_EQ(b.keysName(Action::Jump), "W");
    EXPECT_EQ(b.keysName(Action::Up), "SPACE / UP");
}

TEST(Controls, BindingAnActionsSecondKeySwapsThem) {
    Bindings b;
    EXPECT_TRUE(b.bindKey(Action::Up, K::Up));
    EXPECT_EQ(b.keysName(Action::Up), "UP / W");
}

TEST(Controls, PadButtonsByLayout) {
    Bindings b;
    // The button under your thumb jumps, whichever pad it is.
    EXPECT_EQ(b.button(Action::Jump, PadLayout::Xbox), 0);
    EXPECT_EQ(b.button(Action::Jump, PadLayout::PlayStation), 1);
    EXPECT_EQ(controls::buttonName(0, PadLayout::Xbox), "A");
    EXPECT_EQ(controls::buttonName(1, PadLayout::PlayStation), "CROSS");
    EXPECT_TRUE(has(b.forButton(7, PadLayout::Xbox), Action::Pause));
    EXPECT_TRUE(has(b.forButton(9, PadLayout::PlayStation), Action::Pause));
    EXPECT_TRUE(has(b.forButton(0, PadLayout::Xbox), Action::Confirm));
    EXPECT_EQ(b.button(Action::Left, PadLayout::Xbox), -1); // the stick

    // Rope onto A: Jump gets X, the rope's old one.
    EXPECT_TRUE(b.bindButton(Action::Rope, 0, PadLayout::Xbox));
    EXPECT_EQ(b.button(Action::Rope, PadLayout::Xbox), 0);
    EXPECT_EQ(b.button(Action::Jump, PadLayout::Xbox), 2);
    EXPECT_FALSE(b.bindButton(Action::Jump, 7, PadLayout::Xbox)); // Start pauses

    EXPECT_EQ(controls::layoutFor("Wireless Controller"), PadLayout::PlayStation);
    EXPECT_EQ(controls::layoutFor("DualSense Wireless Controller"), PadLayout::PlayStation);
    EXPECT_EQ(controls::layoutFor("Xbox Wireless Controller"), PadLayout::Xbox);
    EXPECT_EQ(controls::layoutFor(""), PadLayout::Xbox);
}

TEST(Controls, KeyNamesGoBothWays) {
    for (int k = 0; k < K::KeyCount; ++k) {
        const auto code = static_cast<K::Key>(k);
        const std::string name = controls::keyName(code);
        EXPECT_NE(name, "?") << k;
        EXPECT_EQ(name.find(' '), std::string::npos) << name; // one word, for the settings file
        EXPECT_EQ(controls::keyNamed(name), code) << name;
    }
    EXPECT_EQ(controls::keyNamed("lshift"), K::LShift);
    EXPECT_EQ(controls::keyNamed("nonsense"), K::Unknown);
}

TEST(Input, GamepadButtonsAreActions) {
    InputManager input(nullptr);
    input.newFrame();
    auto press = input.handleEvent(button(sf::Event::JoystickButtonPressed, 0));
    ASSERT_TRUE(press);
    EXPECT_EQ(press->button, 0);
    EXPECT_TRUE(has(press->actions, Action::Jump));
    EXPECT_TRUE(input.wasPressed(Action::Jump));
    EXPECT_TRUE(input.isHeld(Action::Jump));
    EXPECT_TRUE(input.usingPad());

    input.newFrame();
    EXPECT_FALSE(input.wasPressed(Action::Jump));
    EXPECT_TRUE(input.isHeld(Action::Jump));
    EXPECT_FALSE(input.handleEvent(button(sf::Event::JoystickButtonReleased, 0)));
    EXPECT_FALSE(input.isHeld(Action::Jump));

    // A key, and it's the keyboard again.
    input.handleEvent(key(sf::Event::KeyPressed, K::Space));
    EXPECT_TRUE(input.wasPressed(Action::Jump));
    EXPECT_FALSE(input.usingPad());
}

TEST(Input, TheStickPressesDirections) {
    InputManager input(nullptr);
    input.newFrame();
    // A little way over: moving nowhere yet. Further: Right, pressed once.
    EXPECT_FALSE(input.handleEvent(axis(sf::Joystick::X, 20.f)));
    EXPECT_FALSE(input.isHeld(Action::Right));
    auto press = input.handleEvent(axis(sf::Joystick::X, 80.f));
    ASSERT_TRUE(press);
    EXPECT_TRUE(has(press->actions, Action::Right));
    EXPECT_FALSE(input.handleEvent(axis(sf::Joystick::X, 95.f)));
    EXPECT_TRUE(input.isHeld(Action::Right));
    // The stick's y points down, the game's up.
    input.handleEvent(axis(sf::Joystick::Y, -90.f));
    EXPECT_TRUE(input.isHeld(Action::Up));
    EXPECT_GT(input.padDirection().y, 0.5f);
    EXPECT_GT(input.padDirection().x, 0.5f);

    // Let go: nothing, and inside the dead zone it points nowhere.
    input.handleEvent(axis(sf::Joystick::X, 10.f));
    input.handleEvent(axis(sf::Joystick::Y, -10.f));
    EXPECT_FALSE(input.isHeld(Action::Right));
    EXPECT_EQ(input.padDirection(), sf::Vector2f());

    // The d-pad wins over the stick while it's held.
    input.handleEvent(axis(sf::Joystick::PovX, -100.f));
    EXPECT_TRUE(input.isHeld(Action::Left));
    EXPECT_EQ(input.padDirection().x, -1.f);
}

TEST(Input, AnotherPadTakesOverWhenItsUsed) {
    InputManager input(nullptr);
    input.handleEvent(button(sf::Event::JoystickButtonPressed, 2, 0)); // the rope, held on pad 0
    EXPECT_TRUE(input.isHeld(Action::Rope));
    // Pad 1's stick drifting doesn't count; a button on it does, and pad 0 is forgotten.
    EXPECT_FALSE(input.handleEvent(axis(sf::Joystick::X, 40.f, 1)));
    EXPECT_TRUE(input.isHeld(Action::Rope));
    input.handleEvent(button(sf::Event::JoystickButtonPressed, 0, 1));
    EXPECT_FALSE(input.isHeld(Action::Rope));
    EXPECT_TRUE(input.isHeld(Action::Jump));

    sf::Event gone{};
    gone.type = sf::Event::JoystickDisconnected;
    gone.joystickConnect.joystickId = 1;
    input.handleEvent(gone);
    EXPECT_FALSE(input.isHeld(Action::Jump));
    EXPECT_FALSE(input.usingPad());
}

TEST(Input, RebindingChangesWhatKeysMean) {
    InputManager input(nullptr);
    Bindings b;
    b.bindKey(Action::Jump, K::C);
    input.setBindings(b);
    auto press = input.handleEvent(key(sf::Event::KeyPressed, K::C));
    ASSERT_TRUE(press);
    EXPECT_TRUE(has(press->actions, Action::Jump));
    EXPECT_TRUE(input.isHeld(Action::Jump));
    // Space was only Jump's: now it's nothing.
    press = input.handleEvent(key(sf::Event::KeyPressed, K::Space));
    ASSERT_TRUE(press);
    EXPECT_TRUE(press->actions.none());
}
