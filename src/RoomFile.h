#pragma once

#include "src/World.h"
#include <map>
#include <string>
#include <vector>

// A room made by hand in the room editor (tools/editor.cpp), as it's saved in
// tools/world/rooms/<name>.txt. tools/make_world.py puts these into the world next to the rooms
// made in Python (tools/world/hand.py reads them), and the game plays one on its own
// (./game tools/world/rooms/<name>.txt). The file:
//
//   title The Ledges                   # shown the first time you go in
//   theme glade                        # one of assets/gfx/themes/
//   region vale                        # one of assets/regions.txt
//   wind -70                           # optional: px/s each second, + to the right
//   dark 1                             # optional
//   space 1                            # optional: no gravity
//   join R 7 camp L 8                  # the door on this room's right starting 7 up leads to camp's left door starting 8 up
//   entity 12 3 dialogue=fen look=fen  # the fields of the entity at column 12, row 3 (rows count up from the bottom)
//   tiles                              # then the tiles, the top row first, in the characters of assets/levels/README.md
//   ##########
//   #S.......#
//   ##########
//
// Doors are the gaps in the outside walls: L and R ones count their start up from the bottom, T
// and B ones from the left, like door() in tools/world/build.py.
struct RoomFile
{
    struct Join
    {
        char side = 'R';   // of this room: L, R, T or B
        int start = 0;     // of the door on that side
        std::string room;  // the room it leads into
        char otherSide = 'L';
        int otherStart = 0;
    };
    struct Door
    {
        char side;
        int start, length;
    };
    using Fields = std::map<std::string, std::string>;

    std::string name; // the file's name, without .txt: the room's identifier
    std::string title;
    std::string theme = Theme::DEFAULT;
    std::string region;
    float wind = 0.f;
    bool dark = false;
    bool space = false;
    std::vector<Join> joins;
    std::vector<std::string> grid;           // grid[y][x], y = 0 the bottom row
    std::map<std::pair<int, int>, Fields> fields; // by (x, y)

    // A new room of that size, walled all round.
    static RoomFile blank(const std::string& name, int width, int height);
    // Throws std::runtime_error saying what's wrong.
    static RoomFile parse(const std::string& text, const std::string& name);
    static RoomFile load(const std::string& path);
    // Whether `text` is a room file rather than a plain text level (just the tiles).
    static bool looksLikeOne(const std::string& text);
    std::string write() const;
    void save(const std::string& path) const;

    int width() const;
    int height() const;
    bool inside(int x, int y) const;
    char at(int x, int y) const; // '.' outside
    void set(int x, int y, char c);
    // The smallest a room can be: its walls round one tile.
    static constexpr int MIN_SIZE = 3;
    // Grows the room by that many tiles on each side (shrinks it, for fewer), the walls moving out
    // with them: what's inside stays where it is, and new tiles carry on from the nearest old ones
    // inside (a floor goes on, but not the things on it). Joins move with their doors.
    void grow(int left, int right, int bottom, int top);
    // That size, grown or shrunk on the right and at the top.
    void resize(int width, int height);

    // The gaps in the outside walls, bottom to top and left to right along each side.
    std::vector<Door> doors() const;
    // The room, ready to play. Throws std::runtime_error if it has no Start (unless it's only to
    // look at, in the editor).
    LevelInfo toLevel(bool needsStart = true) const;
};

// Whether a tile character is solid, as Level::isSolid has it (with gates shut): the doors are
// where the outside walls aren't.
bool isSolidChar(char c);
// Whether a character is an entity's rather than a tile's (see entities::all in src/World.h).
bool isEntityChar(char c);
