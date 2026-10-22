#include <gtest/gtest.h>
#include "src/Menu.h"

namespace
{
    Menu makeMenu()
    {
        Menu menu;
        menu.setItems({{1, "PLAY", "", false}, {2, "VOLUME", "5", true}, {3, "QUIT", "", false}});
        return menu;
    }

    // A key, as it works with the usual bindings.
    std::optional<MenuChoice> press(Menu& menu, sf::Keyboard::Key key)
    {
        return menu.handle(controls::Bindings{}.forKey(key));
    }

    // Middle of the item shown `row` lines from the top.
    sf::Vector2f rowCentre(const Menu& menu, int row)
    {
        return {160.f, menu.top + row * menu.lineHeight + 2.f};
    }
}

TEST(Menu, ArrowsMoveAndWrapAround) {
    Menu menu = makeMenu();
    EXPECT_EQ(menu.selected(), 0u);
    press(menu, sf::Keyboard::Down);
    EXPECT_EQ(menu.selected(), 1u);
    press(menu, sf::Keyboard::S);
    press(menu, sf::Keyboard::Down);
    EXPECT_EQ(menu.selected(), 0u);
    press(menu, sf::Keyboard::Up);
    EXPECT_EQ(menu.selected(), 2u);
}

TEST(Menu, EnterPicksAndSidewaysOnlyChangesSettings) {
    Menu menu = makeMenu();
    auto picked = press(menu, sf::Keyboard::Enter);
    ASSERT_TRUE(picked.has_value());
    EXPECT_EQ(picked->id, 1);
    EXPECT_EQ(picked->direction, 0);
    EXPECT_FALSE(press(menu, sf::Keyboard::Right).has_value());

    press(menu, sf::Keyboard::Down);
    auto changed = press(menu, sf::Keyboard::Left);
    ASSERT_TRUE(changed.has_value());
    EXPECT_EQ(changed->id, 2);
    EXPECT_EQ(changed->direction, -1);
}

TEST(Menu, MouseHoverSelectsOnlyWhenMoved) {
    Menu menu = makeMenu();
    EXPECT_FALSE(menu.handleMouse(rowCentre(menu, 2), false, false).has_value());
    EXPECT_EQ(menu.selected(), 0u); // resting mouse doesn't steal the selection
    menu.handleMouse(rowCentre(menu, 2), true, false);
    EXPECT_EQ(menu.selected(), 2u);

    auto clicked = menu.handleMouse(rowCentre(menu, 1), false, true);
    ASSERT_TRUE(clicked.has_value());
    EXPECT_EQ(clicked->id, 2);
    EXPECT_FALSE(menu.handleMouse({5.f, 5.f}, true, true).has_value()); // clicking nowhere
}

TEST(Menu, ScrollsToKeepTheSelectionVisible) {
    Menu menu;
    menu.maxVisible = 3;
    std::vector<MenuItem> items;
    for (int ii = 0; ii < 6; ++ii) items.push_back({ii, "ITEM", "", false});
    menu.setItems(items);

    menu.select(4);
    // Items 2, 3 and 4 are shown, so the top row is item 2.
    auto clicked = menu.handleMouse(rowCentre(menu, 0), false, true);
    ASSERT_TRUE(clicked.has_value());
    EXPECT_EQ(clicked->id, 2);
}

TEST(Menu, KeepsTheSelectionWhenItemsAreRefreshed) {
    Menu menu = makeMenu();
    press(menu, sf::Keyboard::Down);
    menu.setItems({{1, "PLAY", "", false}, {2, "VOLUME", "6", true}, {3, "QUIT", "", false}});
    EXPECT_EQ(menu.selected(), 1u);
    menu.setItems({{1, "PLAY", "", false}});
    EXPECT_EQ(menu.selected(), 0u);
}
