#pragma once

#include "src/InputManager.h"
#include "src/WorldMap.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// The map screen: the explored rooms (see WorldMap), you on it, things still to pick up, and the
// benches you've sat on. Opened to look around, or from a bench to choose one to travel to.
class MapScreen
{
public:
    // What's on the map besides the rooms. The game makes these when it opens the map and
    // whenever it's drawn. Positions are in world tiles.
    struct Marks
    {
        std::vector<bool> explored; // per room
        size_t currentRoom = 0;
        sf::Vector2f player;
        std::vector<sf::Vector2f> pickups; // still to pick up, in explored rooms
        std::vector<sf::Vector2f> benches; // sat on, in the order travel goes through them
        std::vector<size_t> benchRooms;    // the room of each
        size_t benchHere = 0;              // in `benches`: the one sat on now
        std::function<std::string(size_t room)> describe; // e.g. "THE CROSSROADS  GEMS 1/2"
    };

    explicit MapScreen(std::vector<WorldMap::Room> rooms);

    // Where a world-tile point is: which room (by the map, not caring what's explored).
    std::optional<size_t> roomAt(sf::Vector2f tile) const;

    void open(const Marks& marks, bool travel);
    bool travelling() const;

    enum class Action { None, Close, Travel };
    // Moves the map with held keys or the mouse, zooms with the wheel, and in travel mode a click
    // on a bench chooses it, a second one goes there.
    Action update(float dt, const InputManager& input, sf::Vector2f mouseMove, const Marks& marks);
    Action handleKey(sf::Keyboard::Key key, const Marks& marks);
    // In travel mode: the bench chosen, as an index into Marks::benches.
    size_t destination() const;

    void draw(sf::RenderTarget& target, const Marks& marks, sf::Vector2f mouse, float time) const;

private:
    WorldMap::View view() const;
    void zoom(int steps); // + is closer

    WorldMap map;
    sf::Vector2f centre; // world tiles
    int zoomLevel = 1;   // one of WorldMap::ZOOMS
    bool travel = false;
    size_t choice = 0;
};
