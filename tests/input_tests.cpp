#include <gtest/gtest.h>
#include "src/InputManager.h"

namespace
{
    sf::Event keyEvent(sf::Event::EventType type, sf::Keyboard::Key key)
    {
        sf::Event event{};
        event.type = type;
        event.key.code = key;
        return event;
    }

    sf::Event mouseEvent(sf::Event::EventType type)
    {
        sf::Event event{};
        event.type = type;
        event.mouseButton.button = sf::Mouse::Left;
        return event;
    }
}

TEST(InputManager_Keys, HeldUntilReleased)
{
    InputManager input(nullptr);
    input.newFrame();
    input.handleEvent(keyEvent(sf::Event::KeyPressed, sf::Keyboard::D));
    EXPECT_TRUE(input.isKeyPressed(sf::Keyboard::D));
    EXPECT_TRUE(input.wasKeyJustPressed(sf::Keyboard::D));

    input.newFrame();
    EXPECT_TRUE(input.isKeyPressed(sf::Keyboard::D));
    EXPECT_FALSE(input.wasKeyJustPressed(sf::Keyboard::D));

    input.newFrame();
    input.handleEvent(keyEvent(sf::Event::KeyReleased, sf::Keyboard::D));
    EXPECT_FALSE(input.isKeyPressed(sf::Keyboard::D));
}

TEST(InputManager_Keys, QuickTapStillCountsAsPressed)
{
    InputManager input(nullptr);
    input.newFrame();
    input.handleEvent(keyEvent(sf::Event::KeyPressed, sf::Keyboard::Space));
    input.handleEvent(keyEvent(sf::Event::KeyReleased, sf::Keyboard::Space));
    EXPECT_TRUE(input.wasKeyJustPressed(sf::Keyboard::Space));
    EXPECT_FALSE(input.isKeyPressed(sf::Keyboard::Space));
}

TEST(InputManager_Keys, UnknownKeyIsIgnored)
{
    InputManager input(nullptr);
    input.newFrame();
    input.handleEvent(keyEvent(sf::Event::KeyPressed, sf::Keyboard::Unknown));
    EXPECT_FALSE(input.isKeyPressed(sf::Keyboard::Unknown));
}

TEST(InputManager_Mouse, ClickHoldRelease)
{
    InputManager input(nullptr);
    input.newFrame();
    input.handleEvent(mouseEvent(sf::Event::MouseButtonPressed));
    EXPECT_TRUE(input.wasMouseClicked());
    EXPECT_TRUE(input.isMousePressed());

    input.newFrame();
    EXPECT_FALSE(input.wasMouseClicked());
    EXPECT_TRUE(input.isMousePressed());

    input.newFrame();
    input.handleEvent(mouseEvent(sf::Event::MouseButtonReleased));
    EXPECT_TRUE(input.wasMouseReleased());
    EXPECT_FALSE(input.isMousePressed());
}

TEST(InputManager_Focus, LosingFocusLetsGoOfEverything)
{
    InputManager input(nullptr);
    input.newFrame();
    input.handleEvent(keyEvent(sf::Event::KeyPressed, sf::Keyboard::A));
    input.handleEvent(mouseEvent(sf::Event::MouseButtonPressed));

    input.newFrame();
    sf::Event lost{};
    lost.type = sf::Event::LostFocus;
    input.handleEvent(lost);
    EXPECT_FALSE(input.isKeyPressed(sf::Keyboard::A));
    EXPECT_FALSE(input.isMousePressed());
    EXPECT_TRUE(input.wasMouseReleased());
}
