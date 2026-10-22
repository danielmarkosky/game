#include "MapScreen.h"
#include "Config.h"
#include "Hud.h"
#include <algorithm>
#include <cmath>

namespace
{
    const sf::FloatRect AREA(8.f, 20.f, 304.f, 138.f);
    constexpr float PAN_SPEED = 120.f; // screen pixels a second, with the arrow keys
    constexpr float HALF_W = config::GAME_WIDTH * 0.5f;

    // A view that only draws inside `area` of the screen, leaving the coordinates as they are.
    sf::View clipView(sf::FloatRect area)
    {
        const sf::Vector2f screen(config::GAME_WIDTH, config::GAME_HEIGHT);
        sf::View view(area);
        view.setViewport({area.left / screen.x, area.top / screen.y, area.width / screen.x, area.height / screen.y});
        return view;
    }
}

MapScreen::MapScreen(std::vector<WorldMap::Room> rooms) : map(std::move(rooms)) {}

std::optional<size_t> MapScreen::roomAt(sf::Vector2f tile) const
{
    return map.roomAt(tile);
}

bool MapScreen::travelling() const
{
    return travel;
}

size_t MapScreen::destination() const
{
    return choice;
}

void MapScreen::open(const Marks& marks, bool travelMode)
{
    map.reveal(marks.explored);
    travel = travelMode && !marks.benches.empty();
    choice = std::min(marks.benchHere, marks.benches.empty() ? size_t(0) : marks.benches.size() - 1);
    // As close as still shows everything explored, if anything does. Otherwise all the way out,
    // around the player.
    const sf::FloatRect shown = map.shownBounds();
    zoomLevel = WorldMap::ZOOMS.back();
    centre = marks.player;
    for (int z : WorldMap::ZOOMS) {
        if (shown.width / z <= AREA.width && shown.height / z <= AREA.height) {
            zoomLevel = z;
            centre = {shown.left + shown.width * 0.5f, shown.top + shown.height * 0.5f};
            break;
        }
    }
}

WorldMap::View MapScreen::view() const
{
    return {AREA, {std::round(centre.x), std::round(centre.y)}, zoomLevel};
}

void MapScreen::zoom(int steps)
{
    const auto& zooms = WorldMap::ZOOMS;
    const int current = static_cast<int>(std::find(zooms.begin(), zooms.end(), zoomLevel) - zooms.begin());
    zoomLevel = zooms[static_cast<size_t>(std::clamp(current - steps, 0, static_cast<int>(zooms.size()) - 1))];
}

MapScreen::Action MapScreen::update(float dt, const InputManager& input, sf::Vector2f mouseMove, const Marks& marks)
{
    using C = controls::Action;
    auto held = [&](C action) { return input.isHeld(action) ? 1.f : 0.f; };
    // In travel mode left and right choose a bench instead.
    if (!travel) {
        const sf::Vector2f keys(held(C::Right) - held(C::Left), held(C::Up) - held(C::Down));
        centre += keys * PAN_SPEED * dt * float(zoomLevel);
    }
    // Dragging with the mouse moves the map with it. Screen y points down, the map's up.
    if (input.isMousePressed() && !input.wasMouseClicked()) centre += sf::Vector2f(-mouseMove.x, mouseMove.y) * float(zoomLevel);
    if (input.wheelDelta() != 0.f) zoom(input.wheelDelta() > 0.f ? 1 : -1);

    const sf::FloatRect shown = map.shownBounds();
    centre.x = std::clamp(centre.x, shown.left, shown.left + shown.width);
    centre.y = std::clamp(centre.y, shown.top, shown.top + shown.height);

    if (travel && input.wasMouseClicked()) {
        for (size_t ii = 0; ii < marks.benches.size(); ++ii) {
            const sf::Vector2f d = map.toScreen(marks.benches[ii], view()) - input.mousePosition();
            if (std::abs(d.x) > 5.f || std::abs(d.y) > 5.f) continue;
            if (ii == choice) return Action::Travel;
            choice = ii;
        }
    }
    return Action::None;
}

MapScreen::Action MapScreen::handle(controls::Actions actions, const Marks& marks)
{
    using C = controls::Action;
    const auto has = [&](C action) { return actions[controls::index(action)]; };
    if (has(C::Pause) || has(C::Back) || has(C::Map)) return Action::Close;
    if (travel) {
        const int count = static_cast<int>(marks.benches.size());
        if (has(C::Left)) choice = static_cast<size_t>((static_cast<int>(choice) + count - 1) % count);
        if (has(C::Right)) choice = static_cast<size_t>((static_cast<int>(choice) + 1) % count);
        if (has(C::Confirm) || has(C::Jump) || has(C::Interact)) return Action::Travel;
    } else if (has(C::Zoom)) {
        zoom(zoomLevel == WorldMap::ZOOMS.back() ? -int(WorldMap::ZOOMS.size()) : -1); // further out, then all the way in
    }
    return Action::None;
}

void MapScreen::draw(sf::RenderTarget& target, const Marks& marks, sf::Vector2f mouse, float time) const
{
    hud::dim(target, 215);
    hud::drawCentred(target, travel ? "TRAVEL" : "MAP", HALF_W, 4.f, hud::GOLD, 2.f);
    const WorldMap::View v = view();
    hud::frame(target, AREA, hud::withAlpha(hud::GREY, 90));
    map.draw(target, v);

    target.setView(clipView(AREA));
    auto at = [&](sf::Vector2f tile) {
        const sf::Vector2f p = map.toScreen(tile, v);
        return sf::Vector2f(std::round(p.x), std::round(p.y));
    };
    // Things still to pick up in the rooms you've seen, glinting.
    const bool bright = std::fmod(time, 0.8f) < 0.4f;
    for (const sf::Vector2f& pickup : marks.pickups) {
        hud::fillRect(target, {at(pickup).x - 1.f, at(pickup).y - 1.f, 3.f, 3.f}, bright ? sf::Color(255, 246, 200) : hud::GOLD);
    }
    // The benches you've sat on, and in travel mode the one you'd go to, framed.
    for (size_t ii = 0; ii < marks.benches.size(); ++ii) {
        const sf::Vector2f mark = at(marks.benches[ii]) - sf::Vector2f(2.f, 1.f);
        hud::fillRect(target, {mark.x - 1.f, mark.y - 1.f, 6.f, 4.f}, sf::Color(0, 0, 0, 200));
        hud::fillRect(target, {mark.x, mark.y, 4.f, 2.f}, sf::Color(220, 170, 108));
        if (travel && ii == choice && std::fmod(time, 0.6f) < 0.45f) hud::frame(target, {mark.x - 2.f, mark.y - 2.f, 8.f, 6.f}, hud::WHITE);
    }
    // The room you're in, outlined, and you in it, blinking.
    const sf::FloatRect room = map.roomRect(marks.currentRoom);
    const sf::Vector2f corner = at({room.left, room.top + room.height});
    sf::RectangleShape outline({std::round(room.width / zoomLevel) + 1.f, std::round(room.height / zoomLevel) + 1.f});
    outline.setPosition(corner.x - 1.f, corner.y - 1.f);
    outline.setFillColor(sf::Color::Transparent);
    outline.setOutlineColor(hud::withAlpha(hud::WHITE, 110));
    outline.setOutlineThickness(1.f);
    target.draw(outline);
    if (std::fmod(time, 0.6f) < 0.4f) {
        const sf::Vector2f dot = at(marks.player) - sf::Vector2f(1.f, 1.f);
        hud::fillRect(target, {dot.x - 1.f, dot.y - 1.f, 5.f, 5.f}, sf::Color(0, 0, 0, 200));
        hud::fillRect(target, {dot.x, dot.y, 3.f, 3.f}, hud::GOLD);
    }
    target.setView(target.getDefaultView());

    const float below = AREA.top + AREA.height + 4.f;
    if (travel) {
        const size_t where = marks.benchRooms[choice];
        hud::drawCentred(target, "TRAVEL TO " + marks.describe(where) + (where == marks.currentRoom ? "  (YOU ARE HERE)" : ""), HALF_W, below, hud::WHITE);
        hud::drawCentred(target, marks.hint, HALF_W, 171.f, hud::GREY);
        return;
    }
    // The name of the explored room under the mouse, or else the one you're in.
    size_t shown = marks.currentRoom;
    if (AREA.contains(mouse)) {
        if (auto hovered = map.roomAt(map.toWorld(mouse, v)); hovered && marks.explored[*hovered]) shown = *hovered;
    }
    hud::drawCentred(target, marks.describe(shown) + (shown == marks.currentRoom ? "  (YOU ARE HERE)" : ""), HALF_W, below, hud::WHITE);
    hud::drawCentred(target, marks.hint, HALF_W, 171.f, hud::GREY);
}
