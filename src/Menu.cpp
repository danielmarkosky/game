#include "Menu.h"
#include "src/Config.h"
#include "src/Hud.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float HALF_W = config::GAME_WIDTH * 0.5f;
    const sf::Color NORMAL(235, 235, 245);
    const sf::Color SELECTED(255, 210, 60);
    const sf::Color DIM(150, 150, 170);
}

void Menu::setItems(std::vector<MenuItem> newItems)
{
    items = std::move(newItems);
    if (current >= items.size()) current = items.empty() ? 0 : items.size() - 1;
    keepSelectionVisible();
}

void Menu::select(size_t index)
{
    current = items.empty() ? 0 : std::min(index, items.size() - 1);
    scroll = 0;
    keepSelectionVisible();
}

size_t Menu::selected() const
{
    return current;
}

std::optional<MenuChoice> Menu::handle(controls::Actions actions)
{
    if (items.empty()) return std::nullopt;
    using controls::Action;
    const auto has = [&](Action action) { return actions[controls::index(action)]; };
    if (has(Action::Up) || has(Action::Down)) {
        current = (current + (has(Action::Up) ? items.size() - 1 : 1)) % items.size();
        keepSelectionVisible();
        return std::nullopt;
    }
    if (has(Action::Left) || has(Action::Right)) {
        if (items[current].adjustable) return MenuChoice{items[current].id, has(Action::Left) ? -1 : 1};
        return std::nullopt;
    }
    if (has(Action::Confirm) || has(Action::Jump)) return MenuChoice{items[current].id, 0};
    return std::nullopt;
}

std::optional<MenuChoice> Menu::handleMouse(sf::Vector2f mouse, bool moved, bool clicked)
{
    for (size_t row = 0; row < visibleCount(); ++row) {
        if (!rowRect(row).contains(mouse)) continue;
        if (moved || clicked) current = scroll + row;
        if (clicked) return MenuChoice{items[current].id, 0};
        return std::nullopt;
    }
    return std::nullopt;
}

void Menu::draw(sf::RenderTarget& target, float time) const
{
    const bool columns = hasValues();
    const float left = LEFT - shift() + offsetX, right = RIGHT - shift() + offsetX;
    for (size_t row = 0; row < visibleCount(); ++row) {
        const size_t index = scroll + row;
        const MenuItem& item = items[index];
        const bool isSelected = index == current;
        const sf::Color color = isSelected ? SELECTED : NORMAL;
        const float y = top + row * lineHeight;

        std::string value = item.value;
        if (isSelected && item.adjustable) value = "< " + value + " >";

        float labelX;
        if (columns) {
            labelX = left;
            hud::drawText(target, value, {right - hud::textWidth(value), y}, isSelected ? SELECTED : DIM);
            if (!item.extra.empty()) hud::drawText(target, item.extra, {right + 12.f, y}, isSelected ? SELECTED : DIM);
        } else {
            labelX = std::floor(HALF_W - hud::textWidth(item.label) * 0.5f);
        }
        hud::drawText(target, item.label, {labelX, y}, color);

        if (isSelected) {
            // A little arrow that bobs towards the item.
            const float bob = std::floor(std::fmod(time * 2.f, 1.f) * 2.f);
            hud::drawText(target, ">", {labelX - 8.f + bob, y}, SELECTED);
        }
    }

    // Hints that the list goes on.
    const float hintX = columns ? (left + right) * 0.5f : HALF_W;
    if (scroll > 0) hud::drawCentred(target, "...", hintX, top - lineHeight + 3.f, DIM);
    if (scroll + visibleCount() < items.size()) hud::drawCentred(target, "...", hintX, top + visibleCount() * lineHeight - 3.f, DIM);
}

float Menu::shift() const
{
    const bool extras = std::any_of(items.begin(), items.end(), [](const MenuItem& item) { return !item.extra.empty(); });
    return extras ? EXTRA_WIDTH * 0.5f : 0.f;
}

bool Menu::hasValues() const
{
    return std::any_of(items.begin(), items.end(), [](const MenuItem& item) { return !item.value.empty(); });
}

size_t Menu::visibleCount() const
{
    return std::min(items.size(), maxVisible);
}

sf::FloatRect Menu::rowRect(size_t row) const
{
    const float y = top + row * lineHeight - (lineHeight - hud::GLYPH_HEIGHT) * 0.5f;
    if (hasValues()) return {LEFT - shift() + offsetX - 10.f, y, RIGHT - LEFT + 20.f + (shift() > 0.f ? EXTRA_WIDTH : 0.f), lineHeight};

    float widest = 0.f;
    for (const auto& item : items) widest = std::max(widest, hud::textWidth(item.label));
    return {HALF_W - widest * 0.5f - 10.f, y, widest + 20.f, lineHeight};
}

void Menu::keepSelectionVisible()
{
    if (current < scroll) scroll = current;
    if (current >= scroll + maxVisible) scroll = current + 1 - maxVisible;
    scroll = std::min(scroll, items.size() - visibleCount());
}
