#pragma once

#include "src/Controls.h"
#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

struct MenuItem
{
    MenuItem(int id, std::string label, std::string value = "", bool adjustable = false, std::string extra = "")
        : id(id), label(std::move(label)), value(std::move(value)), adjustable(adjustable), extra(std::move(extra)) {}

    int id;                  // what the owner of the menu acts on
    std::string label;
    std::string value;       // shown on the right, e.g. a setting or a best time. Empty for plain buttons
    bool adjustable = false; // a setting: left/right change it, and it's drawn with arrows
    std::string extra;       // a third column, right of the values. Empty for none
};

struct MenuChoice
{
    int id;
    int direction = 0;       // 0: picked (Enter, Space, click), -1 / +1: left / right on a setting
};

// A vertical list of items drawn with the HUD font. Works with the keyboard or a gamepad (up/down
// to move, OK or jump to pick, left/right to change a setting) and the mouse (hover to select,
// click to pick).
// Scrolls when there are more items than fit.
class Menu
{
public:
    // Replaces the items, keeping the selection where it was if it still exists.
    void setItems(std::vector<MenuItem> newItems);
    void select(size_t index);
    size_t selected() const;

    // What a key or button press means (see InputManager::Press).
    std::optional<MenuChoice> handle(controls::Actions actions);
    // `mouse` in game pixels. Hovering only selects when the mouse moved, so a mouse resting on
    // an item doesn't fight the keyboard.
    std::optional<MenuChoice> handleMouse(sf::Vector2f mouse, bool moved, bool clicked);
    void draw(sf::RenderTarget& target, float time) const;

    // Layout, in game pixels. With a third column the others move left to make room.
    static constexpr float LEFT = 60.f;   // labels start here, when there are values
    static constexpr float RIGHT = 260.f; // values end here
    static constexpr float EXTRA_WIDTH = 40.f;
    float top = 70.f;
    float offsetX = 0.f; // the whole menu moved right by this (left, negative), to leave room beside it
    float lineHeight = 11.f;
    size_t maxVisible = 9;

private:
    bool hasValues() const;
    float shift() const; // how far left the columns move for the third
    size_t visibleCount() const;
    // Screen rectangle of the item shown `row` lines below the top.
    sf::FloatRect rowRect(size_t row) const;
    void keepSelectionVisible();

    std::vector<MenuItem> items;
    size_t current = 0;
    size_t scroll = 0; // index of the first visible item
};
