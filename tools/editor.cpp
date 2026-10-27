// The room editor: lays a room out with the mouse, drawn in the game's own art, and puts it in the
// world.
//
//   ./build/Release/editor                         # pick a room to open, or make a new one
//   ./build/Release/editor <name>                  # tools/world/rooms/<name>.txt, made if it's new
//   ./build/Release/editor <name> <width> <height> # a new room that size, in tiles (40 x 23 is a screen)
//
// Pick a tile or a thing on the left and paint with the left mouse button; the right one erases.
// Shift drags a rectangle, Alt (Option) picks up what's under the mouse, the wheel zooms, the middle
// button (or Space) drags the view, and the handles on the room's sides make it bigger or smaller
// there. A gap in the outside wall is a door: say where it leads under DOORS. Ctrl+S (Cmd+S) saves
// the room and rebuilds the world with tools/make_world.py, so it's in the game. F5 plays the room
// right here, as it is (saved or not), with every ability: F5 or Esc again and you're back where you
// were, editing. Shift+F5 plays it from where the mouse is. F6 plays the world from it, in the game.
// The file is laid out as src/RoomFile.h says.
//
// A room made in Python (tools/world/*.py) opens as it is in the world; saving it makes its file in
// tools/world/rooms/, which the world uses instead from then on.
//
// Tab shows the world map: every room where it is, to drag into place (see "The world map" below).
#include "src/Config.h"
#include "src/Game.h"
#include "src/Hud.h"
#include "src/Level.h"
#include "src/Player.h"
#include "src/Props.h"
#include "src/Regions.h"
#include "src/RoomFile.h"
#include "src/Save.h"
#include "src/Text.h"
#include "src/Theme.h"
#include "src/World.h"
#include "src/utils.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr float UI = 2.f;          // window pixels to a pixel of the panel's font
    constexpr float PANEL_WIDTH = 184.f;
    constexpr float STATUS_HEIGHT = 13.f;
    constexpr float ROW = 11.f;
    constexpr int TILE = static_cast<int>(Level::TILE_SIZE);

    const sf::Color PANEL(26, 24, 36), PANEL_EDGE(60, 56, 80), CHOSEN(70, 64, 110), HOVER(44, 40, 62);
    const sf::Color GREEN(120, 220, 120), ORANGE(255, 170, 70);

    // What can be painted: its character in the room file, its name, its colour, and its tile in
    // the theme's tiles.png (as Level.cpp's sheet has them), if it has one.
    struct Brush
    {
        char c;
        const char* name;
        sf::Color colour;
        int sheet = -1;
    };
    const std::vector<Brush> TILES = {
        {'#', "Rock", {106, 92, 122}, 0},       {'%', "Hook block", {166, 182, 204}, 23}, {'-', "Platform", {180, 124, 72}, 21},
        {'^', "Spikes", {238, 241, 248}, 12},   {'x', "Crystal", {229, 72, 122}, 16},     {'=', "Gate", {136, 153, 170}, 24},
        {'o', "Slick stone", {154, 192, 224}, 25}, {'~', "Ice", {207, 239, 255}, 26},     {'&', "Crumbling", {168, 136, 108}, 27},
        {'_', "Cloud", {255, 255, 255}, 28},    {'w', "Water", {58, 143, 208}},           {'m', "Bounce mushroom", {232, 106, 154}, 29},
        {'t', "Rails", {138, 122, 106}, 30},     {'?', "Hidden passage", {90, 78, 106}, 0}, {'.', "Empty", {20, 20, 28}},
    };
    const std::vector<Brush> THINGS = {
        {'S', "Start", {99, 199, 77}},     {'C', "Checkpoint", {126, 240, 142}}, {'G', "Goal", {255, 216, 90}},
        {'P', "Pickup", {232, 185, 74}},   {'*', "Gem", {79, 208, 232}},         {'N', "Person", {215, 123, 186}},
        {'B', "Bench", {180, 124, 72}},    {'L', "Lever", {232, 68, 58}},        {'D', "Prop", {138, 138, 160}},
        {'Y', "Icicle", {207, 239, 255}},  {'U', "Updraft", {255, 140, 60}},     {'R', "Ring", {232, 185, 74}},
        {'Q', "Lamp", {255, 216, 106}},    {'K', "Mine cart", {138, 122, 106}},  {'E', "End of the line", {106, 90, 74}},
        {'T', "Timed run post", {126, 240, 142}}, {'O', "Stone block", {150, 140, 150}}, {'V', "Weight plate", {120, 200, 170}},
        {'F', "Fire", {255, 140, 60}},
    };
    const std::vector<std::string> ABILITIES = {"rope", "long_rope", "wall_climb", "air_throw", "double_jump", "stopwatch", "tie_off"};

    const Brush* brushOf(char c)
    {
        for (const auto* list : {&TILES, &THINGS}) {
            for (const Brush& b : *list) {
                if (b.c == c) return &b;
            }
        }
        return nullptr;
    }

    const char* sideName(char side)
    {
        switch (side) {
            case 'L': return "left";
            case 'R': return "right";
            case 'T': return "top";
            default: return "bottom";
        }
    }

    char opposite(char side)
    {
        switch (side) {
            case 'L': return 'R';
            case 'R': return 'L';
            case 'T': return 'B';
            default: return 'T';
        }
    }

    std::string doorName(char side, int start, int length)
    {
        return std::string(sideName(side)) + " wall, " + (side == 'L' || side == 'R' ? "rows " : "columns ") + std::to_string(start) + "-" +
               std::to_string(start + length - 1);
    }

    // The names of the files in a folder with that extension, sorted, without it.
    std::vector<std::string> filesIn(const fs::path& folder, const std::string& extension)
    {
        std::vector<std::string> names;
        std::error_code error;
        for (const auto& entry : fs::directory_iterator(folder, error)) {
            if (entry.path().extension() == extension) names.push_back(entry.path().stem().string());
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    std::vector<std::string> themes()
    {
        std::vector<std::string> names;
        std::error_code error;
        for (const auto& entry : fs::directory_iterator(config::assetPath("gfx/themes"), error)) {
            if (entry.is_directory()) names.push_back(entry.path().filename().string());
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    std::vector<std::string> propKinds()
    {
        // Not the ones the game draws for things that aren't props: updrafts' vents, rings, lamps, icicles.
        const std::set<std::string> notProps = {"vent", "ring", "lamp", "lamp_lit", "icicle"};
        std::vector<std::string> kinds;
        for (const PropInfo& info : props::all()) {
            if (!notProps.count(info.name)) kinds.push_back(info.name);
        }
        kinds.push_back(props::LIGHT_SHAFT);
        kinds.push_back(props::WATERFALL);
        return kinds;
    }

    std::string quoted(const std::string& s)
    {
#ifdef _WIN32
        return "\"" + s + "\"";
#else
        std::string out = "'";
        for (char c : s) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
        return out + "'";
#endif
    }

    // Runs a command, its output and whether it worked.
    std::pair<bool, std::string> runCommand(const std::string& command)
    {
#ifdef _WIN32
        FILE* pipe = _popen(command.c_str(), "r");
#else
        FILE* pipe = popen(command.c_str(), "r");
#endif
        if (!pipe) return {false, "couldn't run: " + command};
        std::string output;
        char buffer[512];
        while (std::fgets(buffer, sizeof buffer, pipe)) output += buffer;
#ifdef _WIN32
        const int status = _pclose(pipe);
#else
        const int status = pclose(pipe);
#endif
        return {status == 0, output};
    }

    // A room's name: small letters, digits and _.
    bool validName(const std::string& name)
    {
        return !name.empty() && std::all_of(name.begin(), name.end(), [](char c) {
            return std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_';
        });
    }

    std::string value(const RoomFile::Fields& fields, const std::string& key)
    {
        const auto it = fields.find(key);
        return it == fields.end() ? "" : it->second;
    }

    std::string lastLine(const std::string& s)
    {
        std::string line, last;
        for (char c : s) {
            if (c == '\n') {
                if (!line.empty()) last = line;
                line.clear();
            } else {
                line += c;
            }
        }
        return line.empty() ? last : line;
    }
}

class Editor
{
public:
    Editor(fs::path root, fs::path binaries) : m_root(std::move(root)), m_binaries(std::move(binaries))
    {
        const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
        window.create(sf::VideoMode(std::min(1600u, desktop.width * 9 / 10), std::min(1000u, desktop.height * 9 / 10)), "Room editor");
        window.setFramerateLimit(60);
        m_figures = std::make_unique<Player>(Level::loadFromString("###\n#S#\n###\n"));
        m_options['P'] = {{"ability", "rope"}};
        const std::vector<std::string> dialogues = filesIn(config::assetPath("dialogue"), ".txt");
        m_options['N'] = {{"dialogue", dialogues.empty() ? "" : dialogues[0]}, {"look", Player::LOOKS.size() > 1 ? Player::LOOKS[1] : "heroine"}};
        m_options['D'] = {{"kind", propKinds().front()}};
        m_options['R'] = {{"to", "6,0"}, {"period", "4"}};
        m_options['K'] = {{"dir", "1"}};
        m_options['T'] = {{"route", regions::all().empty() ? "" : regions::all().front().id}, {"mark", "start"}};
        m_room = RoomFile::blank("", 40, 23);
        rebuild();
        m_cursorArrow.loadFromSystem(sf::Cursor::Arrow);
        m_cursorSideways.loadFromSystem(sf::Cursor::SizeHorizontal);
        m_cursorUpDown.loadFromSystem(sf::Cursor::SizeVertical);
    }

    // Opens a room: its file in tools/world/rooms/; or, if it hasn't got one, the room of that name
    // made in Python as it is in the world; or else a new room that size. What went wrong, if it didn't.
    std::optional<std::string> open(const std::string& name, int width = 40, int height = 23)
    {
        if (!validName(name)) return "A room's name is small letters, digits and _, like ledges_2";
        const fs::path path = m_root / "tools" / "world" / "rooms" / (name + ".txt");
        const std::string file = fs::relative(path, m_root).string();
        RoomFile room;
        std::string greeting;
        bool fromPython = false;
        try {
            if (fs::exists(path)) {
                room = RoomFile::load(path.string());
                greeting = "Opened " + file;
            } else if (const auto names = worldRooms(); std::find(names.begin(), names.end(), name) != names.end()) {
                const fs::path exported = fs::temp_directory_path() / ("room_editor_" + name + ".txt");
                const auto [ok, output] = runCommand("cd " + quoted(m_root.string()) + " && python3 tools/make_world.py --export " + name + " " +
                                                     quoted(exported.string()) + " 2>&1");
                if (!ok) return "Can't get " + name + " from tools/world/: " + lastLine(output);
                room = RoomFile::load(exported.string());
                room.name = name;
                fs::remove(exported);
                fromPython = true;
                greeting = name + " is made in Python (tools/world/). Saving it makes " + file + ", which the world uses instead";
            } else {
                room = RoomFile::blank(name, std::max(RoomFile::MIN_SIZE, width), std::max(RoomFile::MIN_SIZE, height));
                if (!regions::all().empty()) room.region = regions::all().front().id;
                greeting = "A new room: Ctrl+S saves it to " + file;
            }
        } catch (const std::exception& e) {
            return e.what();
        }
        m_mapMode = false;
        m_path = path.string();
        m_room = std::move(room);
        m_fromPython = fromPython;
        m_undo.clear();
        m_redo.clear();
        m_unsaved = !fromPython && !fs::exists(path);
        m_closing = m_switching = false;
        // Open corners (from before the editor kept them shut) closed: they'd lead nowhere.
        int closed = 0;
        for (const auto& [x, y] : {std::pair{0, 0}, {m_room.width() - 1, 0}, {0, m_room.height() - 1}, {m_room.width() - 1, m_room.height() - 1}}) {
            if (isSolidChar(m_room.at(x, y))) continue;
            m_room.set(x, y, '#');
            ++closed;
        }
        m_needsRebuild = true;
        rebuild();
        loadNeighbours();
        fit();
        say(greeting);
        if (closed > 0) {
            m_unsaved = true;
            say(greeting + ". Closed " + (closed == 1 ? "an open corner" : std::to_string(closed) + " open corners") +
                    ": corners have to be rock (Ctrl+S saves it)",
                ORANGE);
        }
        return std::nullopt;
    }

    // Every room there is, to open: the world's, in its order, then the hand-made ones not in it yet.
    void showOpenList()
    {
        std::vector<std::string> names = worldRooms();
        for (const std::string& name : filesIn(m_root / "tools" / "world" / "rooms", ".txt")) {
            if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
        }
        std::vector<std::string> labels = {"A new room..."};
        for (const std::string& name : names) labels.push_back(name + (name == m_room.name ? "  (open now)" : ""));
        choose("Open a room", labels, [this, names](size_t i) {
            if (i == 0) {
                ask("The new room's name: small letters, digits and _", "", [this](const std::string& name) { switchTo(name); });
            } else {
                switchTo(names[i - 1]);
            }
        });
    }

    void run()
    {
        while (window.isOpen()) {
            for (sf::Event event; window.pollEvent(event);) handle(event);
            if (m_needsRebuild) rebuild();
            if (m_game) playFrame();
            draw();
        }
    }

    // The names of the world's rooms, in its order, as last built.
    std::vector<std::string> worldRooms()
    {
        std::vector<std::string> names;
        try {
            for (const LevelInfo& info : loadWorld(config::assetPath("world.ldtk"))) names.push_back(info.name);
        } catch (const std::exception& e) {
            say(std::string("Can't read the world: ") + e.what(), hud::RED);
        }
        return names;
    }

private:
    sf::RenderWindow window;
    std::string m_path;
    RoomFile m_room;
    fs::path m_root, m_binaries;

    // What it looks like, made again after each change.
    LevelInfo m_built;
    bool m_needsRebuild = true;
    std::unique_ptr<Player> m_figures; // draws the people
    sf::RenderTexture m_canvas;        // the whole room, a pixel to a pixel
    sf::Vector2f m_offset;             // where the room's top-left is in the window
    float m_zoom = 3.f;                // window pixels to a room pixel
    bool m_grid = true;
    sf::Clock m_clock;

    // Editing.
    char m_brush = '#';
    std::map<char, RoomFile::Fields> m_options; // the fields new things of each kind get
    std::vector<RoomFile> m_undo, m_redo;
    bool m_unsaved = false;
    bool m_closing = false;   // asked to close with unsaved changes: again, and it does
    bool m_switching = false; // the same, opening another room
    bool m_fromPython = false; // made in tools/world/*.py, not saved here yet
    enum class Drag { None, Paint, Erase, Fill, Clear, Pan, Resize };
    char m_resizing = 0;       // the side being dragged
    sf::Vector2i m_resizeMouse;
    sf::Vector2f m_resizeOffset;
    sf::Cursor m_cursorArrow, m_cursorSideways, m_cursorUpDown;
    const sf::Cursor* m_cursor = nullptr;

    // The world map (Tab): every room where it is in the world, to move about.
    bool m_mapMode = false;
    struct MapRoom
    {
        std::string name, title;
        RoomFile shape;                     // its tiles, as built: for its size and doors
        std::vector<RoomFile::Door> doors;
        sf::Vector2i position;              // world tiles, of its bottom-left tile
        bool inWorld = true;                // or waiting under it, not joined to it yet
        bool handMade = false;              // has a file in tools/world/rooms/
        std::shared_ptr<sf::RenderTexture> picture;
    };
    std::vector<MapRoom> m_map;
    struct Link
    {
        size_t a, b;
        RoomFile::Door da, db;
    };
    std::vector<Link> m_mapLinks;           // the doors that meet, as the rooms are
    sf::Vector2f m_mapOrigin;               // window pixels of the bottom-left corner of world tile (0, 0)
    float m_mapZoom = 1.f;                  // window pixels to a world pixel
    std::optional<size_t> m_mapPressed;     // the room the mouse went down on
    bool m_mapDragging = false, m_mapPanning = false;
    sf::Vector2i m_mapMouse;                // where it went down
    sf::Vector2f m_mapOriginBefore;
    sf::Vector2i m_mapDragPos;              // where the dragged room would go
    std::vector<Link> m_mapDragLinks;       // and the doors that would meet there
    std::optional<std::string> m_mapProblem; // why it can't go there
    struct FileState
    {
        fs::path path;
        std::optional<std::string> text; // none: there was no such file
    };
    std::vector<std::vector<FileState>> m_mapUndo; // the files before each move
    Drag m_drag = Drag::None;
    sf::Vector2i m_from, m_last;       // tiles: where the drag started, where it's been last
    sf::Vector2i m_panMouse;
    sf::Vector2f m_panOffset;
    RoomFile m_before;                 // the room when the drag started, to undo it

    // The panel and what's over the map.
    struct Hit
    {
        sf::FloatRect area; // panel pixels
        std::function<void(bool shift)> act;
    };
    std::vector<Hit> m_panelHits, m_popupHits;
    float m_panelScroll = 0.f, m_panelHeight = 0.f;
    float m_drawScroll = 0.f; // while the panel's drawn: its scroll
    float m_wheel = 0.f;      // turned so far, for trackpads' bits of a turn
    struct Popup
    {
        std::string title;
        std::vector<std::string> items;
        std::function<void(size_t)> pick;
        bool icons = false; // props: a picture of each
        float scroll = 0.f;
    };
    std::optional<Popup> m_popup;
    struct Prompt
    {
        std::string label, value;
        std::function<void(const std::string&)> done;
        bool spaces = false;
    };
    std::optional<Prompt> m_prompt;
    std::string m_message;
    sf::Color m_messageColour = hud::WHITE;

    // The other rooms' doors, to join this one's to.
    struct OtherDoor
    {
        std::string room;
        RoomFile::Door door;
        bool taken; // joined to some room but this one
    };
    std::vector<OtherDoor> m_otherDoors;

    // Playing the room right here (F5): the game itself, with no window of its own, its picture
    // where the room was; what's held and pressed goes to it. Its keys are the game's, as set in
    // its options.
    std::unique_ptr<Game> m_game;
    const sf::Texture* m_picture = nullptr; // its last frame
    std::optional<sf::Vector2i> m_playFrom;  // the tile it started from, if not the Start
    sf::FloatRect m_gameRect;                // where the picture is, in the window
    sf::Clock m_frameClock;
    bool m_jumpWas = false, m_ropeWas = false;
    controls::Bindings m_keys = SaveFiles(SaveFiles::defaultFolder()).loadSettings().controls;

    // ------------------------------------------------------------------------------------------
    // Where things are

    // Window pixels, whatever size it's been made.
    sf::View pixels() const
    {
        return sf::View(sf::FloatRect(0.f, 0.f, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));
    }

    sf::Vector2f uiSize() const
    {
        return sf::Vector2f(window.getSize()) / UI;
    }

    // The map: right of the panel, above the status bar, in window pixels.
    sf::FloatRect mapArea() const
    {
        const sf::Vector2f size(window.getSize());
        return {PANEL_WIDTH * UI, 0.f, size.x - PANEL_WIDTH * UI, size.y - STATUS_HEIGHT * UI};
    }

    sf::Vector2i tileAt(sf::Vector2i mouse) const
    {
        const sf::Vector2f p = (sf::Vector2f(mouse) - m_offset) / (m_zoom * TILE);
        return {static_cast<int>(std::floor(p.x)), m_room.height() - 1 - static_cast<int>(std::floor(p.y))};
    }

    // A tile's square in the window.
    sf::FloatRect tileRect(int x, int y) const
    {
        const float size = m_zoom * TILE;
        return {m_offset.x + x * size, m_offset.y + (m_room.height() - 1 - y) * size, size, size};
    }

    void fit()
    {
        const sf::FloatRect map = mapArea();
        const float w = static_cast<float>(m_room.width() * TILE), h = static_cast<float>(m_room.height() * TILE);
        m_zoom = std::max(1.f, std::floor(std::min((map.width - 40.f) / w, (map.height - 40.f) / h)));
        m_offset = {std::round(map.left + (map.width - w * m_zoom) / 2.f), std::round(map.top + (map.height - h * m_zoom) / 2.f)};
    }

    void zoomAt(sf::Vector2f mouse, float zoom)
    {
        zoom = std::clamp(zoom, 1.f, 16.f);
        m_offset = mouse - (mouse - m_offset) * (zoom / m_zoom);
        m_offset = {std::round(m_offset.x), std::round(m_offset.y)};
        m_zoom = zoom;
    }

    // ------------------------------------------------------------------------------------------
    // Changing the room

    void say(const std::string& message, sf::Color colour = hud::WHITE)
    {
        m_message = message;
        m_messageColour = colour;
    }

    // Before a change: keeps the room as it was, to undo to.
    void remember()
    {
        m_undo.push_back(m_room);
        if (m_undo.size() > 300) m_undo.erase(m_undo.begin());
        m_redo.clear();
        changed();
    }

    void changed()
    {
        m_unsaved = true;
        m_closing = m_switching = false;
        m_needsRebuild = true;
    }

    void undo(std::vector<RoomFile>& from, std::vector<RoomFile>& to, const char* what)
    {
        if (from.empty()) return say(std::string("Nothing to ") + what);
        to.push_back(m_room);
        m_room = from.back();
        from.pop_back();
        changed();
        say(std::string(what == std::string("undo") ? "Undone" : "Redone"));
    }

    void rebuild()
    {
        m_needsRebuild = false;
        m_built = m_room.toLevel(false);
        try {
            m_built.level->setTheme(Theme::get(m_room.theme));
        } catch (const std::exception& e) {
            say(e.what(), hud::RED);
        }
        const auto size = sf::Vector2u(static_cast<unsigned>(m_room.width() * TILE), static_cast<unsigned>(m_room.height() * TILE));
        if (m_canvas.getSize() != size) m_canvas.create(size.x, size.y);
    }

    bool corner(int x, int y) const
    {
        return (x == 0 || x == m_room.width() - 1) && (y == 0 || y == m_room.height() - 1);
    }

    void put(int x, int y, char c)
    {
        if (!m_room.inside(x, y)) return;
        if (corner(x, y) && !isSolidChar(c)) {
            // A door is in one wall: a gap in a corner would lead nowhere.
            return say("The corners stay rock: a door goes in one wall, beside them", ORANGE);
        }
        if (c == 'S') {
            // Only one Start: it moves.
            for (int yy = 0; yy < m_room.height(); ++yy) {
                for (int xx = 0; xx < m_room.width(); ++xx) {
                    if (m_room.at(xx, yy) == 'S') m_room.set(xx, yy, '.');
                }
            }
        }
        m_room.set(x, y, c);
        if (isEntityChar(c) && !m_options[c].empty()) m_room.fields[{x, y}] = m_options[c];
        else m_room.fields.erase({x, y});
        m_needsRebuild = true;
    }

    // Along a straight line, so a quick drag leaves no gaps.
    void paintLine(sf::Vector2i a, sf::Vector2i b, char c)
    {
        const int steps = std::max(std::abs(b.x - a.x), std::abs(b.y - a.y));
        for (int i = 0; i <= steps; ++i) {
            const float t = steps == 0 ? 0.f : static_cast<float>(i) / static_cast<float>(steps);
            put(static_cast<int>(std::round(a.x + (b.x - a.x) * t)), static_cast<int>(std::round(a.y + (b.y - a.y) * t)), c);
        }
    }

    void fillRect(sf::Vector2i a, sf::Vector2i b, char c)
    {
        for (int y = std::min(a.y, b.y); y <= std::max(a.y, b.y); ++y) {
            for (int x = std::min(a.x, b.x); x <= std::max(a.x, b.x); ++x) put(x, y, c);
        }
    }

    void pickUp(sf::Vector2i tile)
    {
        if (!m_room.inside(tile.x, tile.y)) return;
        m_brush = m_room.at(tile.x, tile.y);
        if (const auto it = m_room.fields.find({tile.x, tile.y}); it != m_room.fields.end()) m_options[m_brush] = it->second;
        const Brush* b = brushOf(m_brush);
        say(std::string("Picked up: ") + (b ? b->name : "?"));
    }

    // Bigger (or smaller) by that many tiles on each side. What's in the room stays where it is on
    // the screen.
    void grow(int left, int right, int bottom, int top)
    {
        RoomFile grown = m_room;
        grown.grow(left, right, bottom, top);
        if (grown.width() == m_room.width() && grown.height() == m_room.height()) return;
        remember();
        const float size = m_zoom * TILE;
        m_offset -= sf::Vector2f(static_cast<float>(grown.width() - m_room.width() - right) * size,
                                 static_cast<float>(grown.height() - m_room.height() - bottom) * size);
        m_room = std::move(grown);
    }

    void switchTo(const std::string& name)
    {
        if (m_unsaved && !m_switching) {
            m_switching = true;
            return say("There are unsaved changes: Ctrl+S saves them, opening another room again throws them away", ORANGE);
        }
        if (const auto error = open(name)) say(*error, hud::RED);
    }

    // The handle on a side of the room, to drag it out or in, in window pixels.
    sf::FloatRect handle(char side) const
    {
        const float size = m_zoom * TILE, thick = 12.f, gap = 4.f;
        const sf::FloatRect room(m_offset.x, m_offset.y, m_room.width() * size, m_room.height() * size);
        const float across = std::min(room.height, 90.f), along = std::min(room.width, 90.f);
        switch (side) {
            case 'L': return {room.left - gap - thick, room.top + (room.height - across) / 2.f, thick, across};
            case 'R': return {room.left + room.width + gap, room.top + (room.height - across) / 2.f, thick, across};
            case 'T': return {room.left + (room.width - along) / 2.f, room.top - gap - thick, along, thick};
            default: return {room.left + (room.width - along) / 2.f, room.top + room.height + gap, along, thick};
        }
    }

    char handleAt(sf::Vector2i mouse) const
    {
        for (const char side : {'L', 'R', 'T', 'B'}) {
            if (handle(side).contains(sf::Vector2f(mouse))) return side;
        }
        return 0;
    }

    // The join of a door of this room, if it has one.
    RoomFile::Join* joinOf(char side, int start)
    {
        for (RoomFile::Join& join : m_room.joins) {
            if (join.side == side && join.start == start) return &join;
        }
        return nullptr;
    }

    // ------------------------------------------------------------------------------------------
    // Saving, building the world, playing

    bool save()
    {
        try {
            fs::create_directories(fs::path(m_path).parent_path());
            m_room.save(m_path);
        } catch (const std::exception& e) {
            say(e.what(), hud::RED);
            return false;
        }
        m_unsaved = false;
        m_closing = m_switching = false;
        m_fromPython = false;
        return true;
    }

    // Saves, then rebuilds assets/world.ldtk with the room in it. Whether it's in the world now.
    bool saveToWorld()
    {
        if (!save()) return false;
        if (!m_built.level->hasStart()) {
            say("Saved, but it isn't in the world: it needs a Start (S)", ORANGE);
            return false;
        }
        say("Saved. Building the world...");
        draw();
        const auto [ok, output] = buildWorld();
        loadNeighbours();
        if (!ok) {
            std::cerr << output;
            say("Saved, but the world didn't build: " + lastLine(output), hud::RED);
            return false;
        }
        if (output.find(m_room.name + " (made by hand) isn't joined") != std::string::npos) {
            say("Saved. Not in the world yet: say where a door leads, under DOORS", ORANGE);
            return false;
        }
        if (const auto failing = checkWorld()) say("Saved and in the world, but " + *failing, ORANGE);
        else say("Saved, and it's in the world. python3 tools/check_world.py checks it can be played through", GREEN);
        return true;
    }

    // tools/make_world.py: whether it built the world, and what it said.
    std::pair<bool, std::string> buildWorld()
    {
        const auto result = runCommand("cd " + quoted(m_root.string()) + " && python3 tools/make_world.py 2>&1");
        if (!result.first) std::cerr << result.second;
        return result;
    }

    // The game's own checks of the world (tests/world_tests.cpp, in game_tests): which fail and
    // where, if any do. Their whole say goes to the terminal.
    std::optional<std::string> checkWorld()
    {
#ifdef _WIN32
        const fs::path tests = m_binaries / "game_tests.exe";
#else
        const fs::path tests = m_binaries / "game_tests";
#endif
        if (!fs::exists(tests)) return std::nullopt;
        const auto [ok, output] = runCommand(quoted(tests.string()) + " --gtest_filter=World.* --gtest_brief=1 2>&1");
        if (ok) return std::nullopt;
        std::cerr << output;
        std::vector<std::string> failed, where;
        const std::vector<std::string> rooms = worldRooms();
        std::istringstream lines(output);
        for (std::string line; std::getline(lines, line);) {
            const std::string mark = "[  FAILED  ] World.";
            if (line.rfind(mark, 0) == 0) {
                const std::string test = line.substr(mark.size(), line.find_first_of(" ,", mark.size()) - mark.size());
                if (std::find(failed.begin(), failed.end(), test) == failed.end()) failed.push_back(test);
            }
            // The rooms are named in the messages: "old_road: ..." or "...world_tests.cpp:196: old_road".
            for (const std::string& room : rooms) {
                const bool named = line.rfind(room + ":", 0) == 0 ||
                                   (line.size() > room.size() + 2 && line.compare(line.size() - room.size() - 2, std::string::npos, ": " + room) == 0);
                if (named && std::find(where.begin(), where.end(), room) == where.end()) where.push_back(room);
            }
        }
        std::string list, places;
        for (const std::string& test : failed) list += (list.empty() ? "" : ", ") + test;
        for (const std::string& room : where) places += (places.empty() ? "" : ", ") + room;
        return "the world's checks fail" + (places.empty() ? "" : " in " + places) + ": " + (list.empty() ? lastLine(output) : list) +
               " (the terminal has more)";
    }

    fs::path roomsFolder() const
    {
        return m_root / "tools" / "world" / "rooms";
    }

    void launch(const std::vector<std::string>& args)
    {
#ifdef _WIN32
        const fs::path game = m_binaries / "game.exe";
        std::string command = "start \"\" " + quoted(game.string());
#else
        const fs::path game = m_binaries / "game";
        std::string command = quoted(game.string());
#endif
        if (!fs::exists(game)) return say("No game next to the editor (" + game.string() + "): build it first", hud::RED);
        for (const std::string& arg : args) command += " " + quoted(arg);
#ifndef _WIN32
        command += " &";
#endif
        std::system(command.c_str());
    }

    // Plays the room as it is now, saved or not, from its Start or from `from`.
    void startPlaying(std::optional<sf::Vector2i> from)
    {
        if (from && (!m_room.inside(from->x, from->y) || isSolidChar(m_room.at(from->x, from->y)))) {
            return say("Shift+F5 plays from the tile under the mouse: an empty one, in the room", ORANGE);
        }
        LevelInfo info;
        try {
            info = m_room.toLevel(false);
        } catch (const std::exception& e) {
            return say(e.what(), hud::RED);
        }
        if (!from && !info.level->hasStart()) return say("Put a Start (S) in it first, or Shift+F5 to play from where the mouse is", hud::RED);
        if (!info.level->hasStart()) info.level->setStart(*from);
        Game::Options options;
        options.testing = true;
        options.headless = true;
        options.sounds = true;
        options.saveFolder = SaveFiles::defaultFolder(); // for the keys; a test game saves nothing else
        if (from) options.startAt = sf::Vector2f(*from) * Level::TILE_SIZE;
        try {
            m_game = std::make_unique<Game>(std::vector<LevelInfo>{info}, options);
        } catch (const std::exception& e) {
            return say(std::string("Can't play it: ") + e.what(), hud::RED);
        }
        m_playFrom = from;
        m_picture = nullptr;
        m_jumpWas = m_ropeWas = true; // what's held now doesn't count as pressed
        m_frameClock.restart();
        say("Playing: every ability (1-6 switch them), R back to the last place you stood. F5 or Esc: back to editing", GREEN);
    }

    // Back to editing, looking at where you'd got to.
    void stopPlaying()
    {
        const sf::Vector2f at = m_game->playerPosition() + sf::Vector2f(Player::WIDTH / 2.f, Player::HEIGHT / 2.f);
        m_game.reset();
        m_picture = nullptr;
        const sf::FloatRect map = mapArea();
        const float size = m_zoom * TILE;
        const sf::Vector2f tile(at.x / TILE, static_cast<float>(m_room.height()) - at.y / TILE); // from the room's top-left
        m_offset = {std::round(map.left + map.width / 2.f - tile.x * size), std::round(map.top + map.height / 2.f - tile.y * size)};
        say("Back to editing. F5 plays it again");
    }

    // One frame of the game: what's held now, and the time since the last.
    void playFrame()
    {
        using A = controls::Action;
        const auto held = [this](A action) {
            for (sf::Keyboard::Key key : m_keys.keys[controls::index(action)]) {
                if (key != sf::Keyboard::Unknown && sf::Keyboard::isKeyPressed(key)) return true;
            }
            return false;
        };
        PlayerInput in;
        if (window.hasFocus()) {
            in.moveX = static_cast<float>(held(A::Right)) - static_cast<float>(held(A::Left));
            in.moveY = static_cast<float>(held(A::Up)) - static_cast<float>(held(A::Down));
            in.jumpHeld = held(A::Jump);
            const sf::Vector2f mouse(sf::Mouse::getPosition(window));
            in.ropeHeld = held(A::Rope) || (m_gameRect.contains(mouse) && sf::Mouse::isButtonPressed(sf::Mouse::Left));
            if (m_gameRect.width > 0.f) in.aim = (mouse - sf::Vector2f(m_gameRect.left, m_gameRect.top)) * (config::GAME_WIDTH / m_gameRect.width);
        }
        in.jumpPressed = in.jumpHeld && !m_jumpWas;
        in.ropePressed = in.ropeHeld && !m_ropeWas;
        m_jumpWas = in.jumpHeld;
        m_ropeWas = in.ropeHeld;
        m_picture = &m_game->play(std::min(m_frameClock.restart().asSeconds(), 0.25f), in);
        if (m_game->screenName() == "complete") {
            // Out of the room: in again from where it started.
            say("Out of the room: in again from the start", ORANGE);
            startPlaying(m_playFrom);
        }
    }

    void drawGame()
    {
        const sf::FloatRect map = mapArea();
        hud::fillRect(window, map, sf::Color(0, 0, 0));
        const float scale = std::max(1.f, std::floor(std::min(map.width / config::GAME_WIDTH, map.height / config::GAME_HEIGHT)));
        const sf::Vector2f size(config::GAME_WIDTH * scale, config::GAME_HEIGHT * scale);
        m_gameRect = {std::round(map.left + (map.width - size.x) / 2.f), std::round(map.top + (map.height - size.y) / 2.f), size.x, size.y};
        if (!m_picture) return;
        sf::Sprite picture(*m_picture);
        picture.setPosition(m_gameRect.left, m_gameRect.top);
        picture.setScale(scale, scale);
        window.draw(picture);
    }

    void playWorld()
    {
        if (saveToWorld()) launch({"--room", m_room.name});
    }

    // The other rooms and their doors: the hand-made ones as saved, the rest from the world as last
    // built.
    void loadNeighbours()
    {
        m_otherDoors.clear();
        const fs::path folder = fs::path(m_path).parent_path();
        std::vector<RoomFile> made;
        for (const std::string& name : filesIn(folder, ".txt")) {
            if (name == m_room.name) continue;
            try {
                made.push_back(RoomFile::load((folder / (name + ".txt")).string()));
            } catch (const std::exception&) {
            }
        }
        // The doors other hand-made rooms have joined already.
        std::set<std::tuple<std::string, char, int>> claimed;
        for (const RoomFile& other : made) {
            for (const RoomFile::Join& join : other.joins) {
                if (join.room == m_room.name) continue;
                claimed.insert({other.name, join.side, join.start});
                claimed.insert({join.room, join.otherSide, join.otherStart});
            }
        }
        std::set<std::string> names;
        for (const RoomFile& other : made) {
            names.insert(other.name);
            for (const RoomFile::Door& door : other.doors()) {
                m_otherDoors.push_back({other.name, door, claimed.count({other.name, door.side, door.start}) > 0});
            }
        }
        std::vector<LevelInfo> world;
        try {
            world = loadWorld(config::assetPath("world.ldtk"));
        } catch (const std::exception& e) {
            say(std::string("Can't read the world: ") + e.what(), hud::RED);
        }
        // Whose is a world tile, but for this room's.
        auto roomAt = [&](int x, int y) -> const LevelInfo* {
            for (const LevelInfo& info : world) {
                const sf::FloatRect b = info.level->bounds();
                const int w = static_cast<int>(b.width) / TILE, h = static_cast<int>(b.height) / TILE;
                if (info.name != m_room.name && x >= info.position.x && y >= info.position.y && x < info.position.x + w && y < info.position.y + h) return &info;
            }
            return nullptr;
        };
        for (const LevelInfo& info : world) {
            if (info.name == m_room.name || names.count(info.name)) continue;
            const sf::FloatRect b = info.level->bounds();
            RoomFile shape = RoomFile::blank(info.name, static_cast<int>(b.width) / TILE, static_cast<int>(b.height) / TILE);
            for (int y = 0; y < shape.height(); ++y) {
                for (int x = 0; x < shape.width(); ++x) shape.set(x, y, info.level->isSolid(x, y) ? '#' : '.');
            }
            for (const RoomFile::Door& door : shape.doors()) {
                // Taken if there's a room on the other side of it.
                int x = info.position.x, y = info.position.y;
                if (door.side == 'L') x -= 1, y += door.start;
                if (door.side == 'R') x += shape.width(), y += door.start;
                if (door.side == 'B') x += door.start, y -= 1;
                if (door.side == 'T') x += door.start, y += shape.height();
                m_otherDoors.push_back({info.name, door, roomAt(x, y) != nullptr || claimed.count({info.name, door.side, door.start}) > 0});
            }
        }
    }

    // ------------------------------------------------------------------------------------------
    // Input

    bool command() const
    {
        return sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl) ||
               sf::Keyboard::isKeyPressed(sf::Keyboard::LSystem) || sf::Keyboard::isKeyPressed(sf::Keyboard::RSystem);
    }

    void handle(const sf::Event& event)
    {
        switch (event.type) {
            case sf::Event::Closed:
                if (m_unsaved && !m_closing) {
                    m_closing = true;
                    say("There are unsaved changes: Ctrl+S saves them, closing again throws them away", ORANGE);
                } else {
                    window.close();
                }
                break;
            case sf::Event::TextEntered: typed(event.text.unicode); break;
            case sf::Event::KeyPressed: key(event.key); break;
            case sf::Event::MouseWheelScrolled: wheel(event.mouseWheelScroll); break;
            case sf::Event::MouseButtonPressed: press(event.mouseButton); break;
            case sf::Event::MouseButtonReleased: release(); break;
            case sf::Event::MouseMoved: moved({event.mouseMove.x, event.mouseMove.y}); break;
            default: break;
        }
    }

    void typed(sf::Uint32 unicode)
    {
        if (unicode < 32 || unicode >= 127 || m_game) return;
        const char c = static_cast<char>(unicode);
        if (m_prompt) {
            if (c != ' ' || m_prompt->spaces) m_prompt->value += c;
            return;
        }
        if (m_popup || m_mapMode || command()) return;
        // A tile's or a thing's character picks it.
        if (const Brush* b = brushOf(c)) {
            m_brush = c;
            say(std::string(b->name));
        }
    }

    void key(const sf::Event::KeyEvent& k)
    {
        if (m_game) {
            // Playing: Esc or F5 stops, anything else is the game's (talking, benches, 1-6, R...).
            if (k.code == sf::Keyboard::Escape || k.code == sf::Keyboard::F5) stopPlaying();
            else m_game->pressKey(k.code);
            return;
        }
        if (m_prompt) {
            if (k.code == sf::Keyboard::Enter) {
                Prompt prompt = *m_prompt;
                m_prompt.reset();
                prompt.done(prompt.value);
            } else if (k.code == sf::Keyboard::Escape) {
                m_prompt.reset();
            } else if (k.code == sf::Keyboard::Backspace && !m_prompt->value.empty()) {
                m_prompt->value.pop_back();
            }
            return;
        }
        if (k.code == sf::Keyboard::Escape && m_popup) {
            m_popup.reset();
            return;
        }
        const bool cmd = k.control || k.system;
        if (m_mapMode) {
            if (k.code == sf::Keyboard::Tab || k.code == sf::Keyboard::Escape) leaveMap(m_room.name);
            else if (cmd && k.code == sf::Keyboard::Z) undoMove();
            else if (cmd && k.code == sf::Keyboard::Num0) fitMap();
            else if (cmd && k.code == sf::Keyboard::O) showOpenList();
            return;
        }
        if (k.code == sf::Keyboard::Tab) return enterMap();
        if (k.code == sf::Keyboard::Escape) return;
        if (cmd && k.code == sf::Keyboard::S) saveToWorld();
        else if (cmd && k.code == sf::Keyboard::Z && k.shift) undo(m_redo, m_undo, "redo");
        else if (cmd && k.code == sf::Keyboard::Z) undo(m_undo, m_redo, "undo");
        else if (cmd && k.code == sf::Keyboard::Y) undo(m_redo, m_undo, "redo");
        else if (cmd && k.code == sf::Keyboard::G) m_grid = !m_grid;
        else if (cmd && k.code == sf::Keyboard::Num0) fit();
        else if (cmd && k.code == sf::Keyboard::O) showOpenList();
        else if (k.code == sf::Keyboard::F5) {
            const sf::Vector2i mouse = sf::Mouse::getPosition(window);
            startPlaying(k.shift ? std::optional(tileAt(mouse)) : std::nullopt);
        }
        else if (k.code == sf::Keyboard::F6) playWorld();
        else if (k.code == sf::Keyboard::Left) m_offset.x += 64.f;
        else if (k.code == sf::Keyboard::Right) m_offset.x -= 64.f;
        else if (k.code == sf::Keyboard::Up) m_offset.y += 64.f;
        else if (k.code == sf::Keyboard::Down) m_offset.y -= 64.f;
    }

    void wheel(const sf::Event::MouseWheelScrollEvent& w)
    {
        if (m_prompt || m_game || w.wheel != sf::Mouse::VerticalWheel) return;
        const sf::Vector2f mouse(static_cast<float>(w.x), static_cast<float>(w.y));
        if (m_popup) {
            m_popup->scroll = std::max(0.f, m_popup->scroll - w.delta * 3.f * ROW);
        } else if (m_mapMode && mouse.x >= PANEL_WIDTH * UI) {
            mapZoomAt(mouse, m_mapZoom * (w.delta > 0.f ? 1.25f : 0.8f));
        } else if (mouse.x < PANEL_WIDTH * UI) {
            m_panelScroll = std::clamp(m_panelScroll - w.delta * 3.f * ROW, 0.f, std::max(0.f, m_panelHeight - (uiSize().y - STATUS_HEIGHT)));
        } else {
            m_wheel += w.delta;
            if (std::abs(m_wheel) >= 1.f) {
                zoomAt(mouse, m_zoom + (m_wheel > 0.f ? 1.f : -1.f));
                m_wheel = 0.f;
            }
        }
    }

    void press(const sf::Event::MouseButtonEvent& b)
    {
        if (m_prompt) return;
        if (m_game && b.x >= PANEL_WIDTH * UI) return; // playing, the mouse is the rope
        const sf::Vector2f ui = sf::Vector2f(static_cast<float>(b.x), static_cast<float>(b.y)) / UI;
        const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
        if (m_popup) {
            for (const Hit& hit : m_popupHits) {
                if (hit.area.contains(ui)) {
                    m_popup.reset();
                    hit.act(shift);
                    return;
                }
            }
            m_popup.reset();
            return;
        }
        if (ui.x < PANEL_WIDTH) {
            if (b.button != sf::Mouse::Left) return;
            for (const Hit& hit : m_panelHits) {
                if (hit.area.contains(ui + sf::Vector2f(0.f, m_panelScroll))) {
                    hit.act(shift);
                    return;
                }
            }
            return;
        }
        if (ui.y > uiSize().y - STATUS_HEIGHT) return;
        if (m_mapMode) return mapPress(b);
        const sf::Vector2i tile = tileAt({b.x, b.y});
        if (const char side = handleAt({b.x, b.y}); side && b.button == sf::Mouse::Left) {
            m_drag = Drag::Resize;
            m_resizing = side;
            m_resizeMouse = {b.x, b.y};
            m_resizeOffset = m_offset;
            m_before = m_room;
            return;
        }
        if (b.button == sf::Mouse::Middle || (b.button == sf::Mouse::Left && sf::Keyboard::isKeyPressed(sf::Keyboard::Space))) {
            m_drag = Drag::Pan;
            m_panMouse = {b.x, b.y};
            m_panOffset = m_offset;
            return;
        }
        const bool alt = sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt) || sf::Keyboard::isKeyPressed(sf::Keyboard::RAlt);
        if (b.button == sf::Mouse::Left && alt) return pickUp(tile);
        if (b.button != sf::Mouse::Left && b.button != sf::Mouse::Right) return;
        m_before = m_room;
        m_from = m_last = tile;
        const bool erase = b.button == sf::Mouse::Right;
        if (shift) {
            m_drag = erase ? Drag::Clear : Drag::Fill;
        } else if (erase) {
            m_drag = Drag::Erase;
            put(tile.x, tile.y, '.');
        } else {
            // Things go one at a time; tiles paint along the drag.
            m_drag = isEntityChar(m_brush) ? Drag::None : Drag::Paint;
            put(tile.x, tile.y, m_brush);
            if (m_drag == Drag::None) finishStroke();
        }
    }

    void moved(sf::Vector2i mouse)
    {
        if (m_game) return;
        if (m_mapMode) return mapMoved(mouse);
        const sf::Vector2i tile = tileAt(mouse);
        switch (m_drag) {
            case Drag::Paint: paintLine(m_last, tile, m_brush); break;
            case Drag::Erase: paintLine(m_last, tile, '.'); break;
            case Drag::Pan: m_offset = m_panOffset + sf::Vector2f(mouse - m_panMouse); break;
            case Drag::Resize: dragSide(mouse); break;
            default: break;
        }
        m_last = tile;
    }

    // Dragging a side of the room out or in, a tile at a time, from how it was when the drag began.
    void dragSide(sf::Vector2i mouse)
    {
        const float size = m_zoom * TILE;
        const int dx = static_cast<int>(std::round(static_cast<float>(mouse.x - m_resizeMouse.x) / size));
        const int dy = static_cast<int>(std::round(static_cast<float>(mouse.y - m_resizeMouse.y) / size));
        RoomFile grown = m_before;
        grown.grow(m_resizing == 'L' ? -dx : 0, m_resizing == 'R' ? dx : 0, m_resizing == 'B' ? dy : 0, m_resizing == 'T' ? -dy : 0);
        const float left = static_cast<float>(m_resizing == 'L' ? grown.width() - m_before.width() : 0);
        const float top = static_cast<float>(m_resizing == 'T' ? grown.height() - m_before.height() : 0);
        m_offset = m_resizeOffset - sf::Vector2f(left * size, top * size);
        if (grown.write() != m_room.write()) {
            m_room = std::move(grown);
            m_needsRebuild = true;
        }
        say("Size " + std::to_string(m_room.width()) + " x " + std::to_string(m_room.height()));
    }

    // Arrows on the handles, the usual one elsewhere.
    void updateCursor()
    {
        const char side = m_mapMode ? 0 : m_drag == Drag::Resize ? m_resizing : m_popup || m_prompt ? 0 : handleAt(sf::Mouse::getPosition(window));
        const sf::Cursor* want = side == 'L' || side == 'R' ? &m_cursorSideways : side ? &m_cursorUpDown : &m_cursorArrow;
        if (want != m_cursor) window.setMouseCursor(*want);
        m_cursor = want;
    }

    void release()
    {
        if (m_mapMode) return mapRelease();
        if (m_drag == Drag::Fill || m_drag == Drag::Clear) fillRect(m_from, m_last, m_drag == Drag::Fill ? m_brush : '.');
        if (m_drag != Drag::Pan && m_drag != Drag::None) finishStroke();
        m_drag = Drag::None;
        m_resizing = 0;
    }

    // A change made with the mouse: one step to undo.
    void finishStroke()
    {
        if (m_room.write() == m_before.write()) return;
        m_undo.push_back(m_before);
        if (m_undo.size() > 300) m_undo.erase(m_undo.begin());
        m_redo.clear();
        changed();
    }

    // ------------------------------------------------------------------------------------------
    // Drawing the room

    void drawRoom()
    {
        renderLevel(*m_built.level, m_canvas, m_clock.getElapsedTime().asSeconds());
    }

    // A room as it looks in the game, onto a canvas its size: a pixel to a pixel.
    void renderLevel(const Level& level, sf::RenderTexture& canvas, float time)
    {
        const float w = static_cast<float>(canvas.getSize().x), h = static_cast<float>(canvas.getSize().y);
        const float screenW = config::GAME_WIDTH, screenH = config::GAME_HEIGHT;
        canvas.clear(sf::Color::Transparent);
        // The level draws what's on a screen around a point: so a screen at a time, each clipped to
        // its own part of the canvas. A screen's centre `c` puts world point p at
        // (p.x - c.x + W/2, c.y + H/2 - p.y) on it, which is canvas pixel (p.x, h - p.y) less its corner.
        auto screens = [&](const std::function<void(sf::Vector2f)>& drawOne) {
            for (float top = 0.f; top < h; top += screenH) {
                for (float left = 0.f; left < w; left += screenW) {
                    sf::View view(sf::FloatRect(0.f, 0.f, screenW, screenH));
                    view.setViewport({left / w, top / h, screenW / w, screenH / h});
                    canvas.setView(view);
                    drawOne({left + screenW / 2.f, h - top - screenH / 2.f});
                }
            }
            canvas.setView(canvas.getDefaultView());
        };
        screens([&](sf::Vector2f centre) { level.draw(canvas, centre); });
        // The rest isn't clipped to a screen: drawn once, centred so the screen is the canvas.
        const sf::Vector2f all(screenW / 2.f, h - screenH / 2.f);
        drawFalls(level, canvas, all);
        level.drawProps(canvas, all, time, [](const std::string&) { return true; });
        level.drawMoving(canvas, all, time);
        level.drawCarts(canvas, all, [](const Cart&) { return true; });
        level.drawTrials(canvas, all, time);
        level.drawMarkers(canvas, all, std::nullopt, time);
        level.drawBenches(canvas, all, std::nullopt, time);
        level.drawLevers(canvas, all);
        level.drawPickups(canvas, all, time, [](const std::string&) { return false; });
        level.drawGems(canvas, all, time, [](size_t) { return false; });
        if (level.hasStart()) {
            const sf::Vector2f start = viewCoordinates(level.getStartPos() + sf::Vector2f(Level::TILE_SIZE / 2.f, 0.f), all);
            m_figures->drawFigure(canvas, 0, start, false, time);
        }
        for (const Npc& npc : level.npcs()) {
            const sf::Vector2f feet = viewCoordinates(npc.position + sf::Vector2f(Level::TILE_SIZE / 2.f, 0.f), all);
            m_figures->drawFigure(canvas, Player::lookNamed(npc.look), feet, true, time, 1.f, npc.sitting);
        }
        screens([&](sf::Vector2f centre) { level.drawWater(canvas, centre, time); });
        // Hidden passages: rock to look at, as in the game, but marked, to see where they are.
        screens([&](sf::Vector2f centre) { level.drawHidden(canvas, centre, {-1e6f, -1e6f}); });
        for (int y = 0; y < static_cast<int>(h) / TILE; ++y) {
            for (int x = 0; x < static_cast<int>(w) / TILE; ++x) {
                if (!level.isHidden(x, y)) continue;
                const float px = static_cast<float>(x * TILE), py = h - static_cast<float>((y + 1) * TILE);
                hud::fillRect(canvas, {px, py, static_cast<float>(TILE), static_cast<float>(TILE)}, sf::Color(150, 110, 220, 70));
                hud::fillRect(canvas, {px + 3.f, py + 3.f, 2.f, 2.f}, sf::Color(220, 200, 255, 200));
            }
        }
        canvas.display();
    }

    // Waterfalls and shafts of light, which the game draws itself: roughly, to see where they go.
    void drawFalls(const Level& level, sf::RenderTarget& canvas, sf::Vector2f centre)
    {
        for (const Prop& prop : level.props()) {
            const bool water = prop.kind == props::WATERFALL;
            if (!water && prop.kind != props::LIGHT_SHAFT) continue;
            const int x = static_cast<int>(prop.position.x) / TILE, y = static_cast<int>(prop.position.y) / TILE;
            int floor = y;
            while (floor > 0 && !level.isSolid(x, floor - 1) && !(water && level.isWater(x, floor - 1))) --floor;
            const sf::Vector2f top = viewCoordinates(prop.position + sf::Vector2f(0.f, Level::TILE_SIZE), centre);
            const float length = static_cast<float>((y + 1 - floor) * TILE);
            if (water) {
                hud::fillRect(canvas, {top.x + 1.f, top.y, 2.f * TILE - 2.f, length}, sf::Color(120, 200, 240, 140));
            } else {
                hud::fillRect(canvas, {top.x + 1.f, top.y, TILE - 2.f, length}, sf::Color(255, 238, 200, 50));
            }
        }
    }

    void drawBackground()
    {
        const sf::FloatRect map = mapArea();
        const Theme& theme = m_built.level->theme();
        for (const sf::Texture& layer : theme.layers) {
            const float scale = map.height / static_cast<float>(layer.getSize().y);
            sf::Sprite sprite(layer, sf::IntRect(0, 0, static_cast<int>(map.width / scale) + 1, static_cast<int>(layer.getSize().y)));
            sprite.setScale(scale, scale);
            sprite.setPosition(map.left, map.top);
            window.draw(sprite);
        }
        hud::fillRect(window, map, sf::Color(0, 0, 0, 90));
    }

    void drawMap()
    {
        if (m_game) return drawGame();
        drawBackground();
        drawRoom();
        sf::Sprite room(m_canvas.getTexture());
        room.setScale(m_zoom, m_zoom);
        room.setPosition(m_offset);
        window.draw(room);
        const float size = m_zoom * TILE;
        hud::frame(window, {m_offset.x, m_offset.y, m_room.width() * size, m_room.height() * size}, sf::Color(255, 255, 255, 60));

        if (m_grid && m_zoom >= 2.f) {
            sf::VertexArray lines(sf::Lines);
            const sf::Color colour(255, 255, 255, 22);
            for (int x = 1; x < m_room.width(); ++x) {
                lines.append({{m_offset.x + x * size, m_offset.y}, colour});
                lines.append({{m_offset.x + x * size, m_offset.y + m_room.height() * size}, colour});
            }
            for (int y = 1; y < m_room.height(); ++y) {
                lines.append({{m_offset.x, m_offset.y + y * size}, colour});
                lines.append({{m_offset.x + m_room.width() * size, m_offset.y + y * size}, colour});
            }
            window.draw(lines);
        }

        // Doors: green where they lead somewhere, orange where they don't yet (red: a pit).
        for (const RoomFile::Door& door : m_room.doors()) {
            const sf::Color colour = joinOf(door.side, door.start) ? GREEN : door.side == 'B' ? hud::RED : ORANGE;
            for (int i = 0; i < door.length; ++i) {
                const int x = door.side == 'L' ? 0 : door.side == 'R' ? m_room.width() - 1 : door.start + i;
                const int y = door.side == 'B' ? 0 : door.side == 'T' ? m_room.height() - 1 : door.start + i;
                hud::fillRect(window, tileRect(x, y), hud::withAlpha(colour, 70));
            }
        }

        // The handles to drag the sides out or in by.
        const sf::Vector2i mouse = sf::Mouse::getPosition(window);
        const char over = m_drag == Drag::Resize ? m_resizing : m_popup || m_prompt ? 0 : handleAt(mouse);
        for (const char side : {'L', 'R', 'T', 'B'}) {
            const sf::FloatRect h = handle(side);
            hud::fillRect(window, h, side == over ? sf::Color(255, 255, 255, 170) : sf::Color(255, 255, 255, 60));
            const bool across = side == 'L' || side == 'R';
            hud::fillRect(window, across ? sf::FloatRect(h.left + h.width / 2.f - 1.f, h.top + 6.f, 2.f, h.height - 12.f)
                                         : sf::FloatRect(h.left + 6.f, h.top + h.height / 2.f - 1.f, h.width - 12.f, 2.f),
                          sf::Color(20, 20, 30, 160));
        }

        // Under the mouse: the tile, or the rectangle being dragged.
        if (!m_popup && !m_prompt && !over && m_drag != Drag::Resize && mapArea().contains(sf::Vector2f(mouse))) {
            const sf::Vector2i tile = m_drag == Drag::Fill || m_drag == Drag::Clear ? m_last : tileAt(mouse);
            const sf::Vector2i from = m_drag == Drag::Fill || m_drag == Drag::Clear ? m_from : tile;
            const sf::FloatRect a = tileRect(std::min(from.x, tile.x), std::max(from.y, tile.y));
            const sf::FloatRect b = tileRect(std::max(from.x, tile.x), std::min(from.y, tile.y));
            const sf::FloatRect area(a.left, a.top, b.left + b.width - a.left, b.top + b.height - a.top);
            const bool erasing = m_drag == Drag::Clear || m_drag == Drag::Erase;
            const Brush* brush = brushOf(m_brush);
            hud::frame(window, area, erasing ? hud::RED : brush ? brush->colour : hud::WHITE);
        }
    }

    // ------------------------------------------------------------------------------------------
    // The world map: the rooms where they are, to drag about. Where rooms are comes from how they're
    // joined (tools/make_world.py lines joined doors up), so moving one joins it anew: to the rooms
    // whose doors meet its own where it's put down, and not to the ones it was joined to before. The
    // joins are all said in its file then; it gets one if it was made in Python.

    void enterMap()
    {
        if (m_unsaved) return say("Save the room first (Ctrl+S): moving rooms on the map saves them", ORANGE);
        m_mapMode = true;
        m_popup.reset();
        m_drag = Drag::None;
        say("Building the world...");
        draw();
        const auto [ok, output] = buildWorld();
        loadMap();
        fitMap();
        if (!ok) say("The world doesn't build, so this is how it was last: " + lastLine(output), hud::RED);
        else say("The world map: drag a room to move it, click one to edit it. Tab: back to the room");
    }

    void leaveMap(const std::string& name)
    {
        m_mapMode = false;
        m_mapPressed.reset();
        m_mapDragging = m_mapPanning = false;
        if (const auto error = open(name)) say(*error, hud::RED);
    }

    // The world as last built, and the hand-made rooms that aren't in it yet, in a row under it.
    void loadMap()
    {
        m_map.clear();
        auto picture = [this](const Level& level) {
            auto canvas = std::make_shared<sf::RenderTexture>();
            const sf::FloatRect b = level.bounds();
            canvas->create(static_cast<unsigned>(b.width), static_cast<unsigned>(b.height));
            renderLevel(level, *canvas, 0.f);
            canvas->setSmooth(true);
            return canvas;
        };
        std::vector<LevelInfo> world;
        try {
            world = loadWorld(config::assetPath("world.ldtk"));
        } catch (const std::exception& e) {
            say(std::string("Can't read the world: ") + e.what(), hud::RED);
        }
        int left = 0, bottom = 0;
        for (LevelInfo& info : world) {
            MapRoom room;
            room.name = info.name;
            room.title = info.title;
            room.position = info.position;
            room.handMade = fs::exists(roomsFolder() / (info.name + ".txt"));
            const sf::FloatRect b = info.level->bounds();
            room.shape = RoomFile::blank(info.name, static_cast<int>(b.width) / TILE, static_cast<int>(b.height) / TILE);
            for (int y = 0; y < room.shape.height(); ++y) {
                for (int x = 0; x < room.shape.width(); ++x) room.shape.set(x, y, info.level->isSolid(x, y) ? '#' : '.');
            }
            room.doors = room.shape.doors();
            try {
                info.level->setTheme(Theme::get(info.theme));
            } catch (const std::exception&) {
            }
            room.picture = picture(*info.level);
            left = m_map.empty() ? room.position.x : std::min(left, room.position.x);
            bottom = m_map.empty() ? room.position.y : std::min(bottom, room.position.y);
            m_map.push_back(std::move(room));
        }
        std::vector<RoomFile> waiting;
        for (const std::string& name : filesIn(roomsFolder(), ".txt")) {
            if (std::any_of(m_map.begin(), m_map.end(), [&](const MapRoom& r) { return r.name == name; })) continue;
            try {
                waiting.push_back(RoomFile::load((roomsFolder() / (name + ".txt")).string()));
            } catch (const std::exception&) {
            }
        }
        int tallest = 0;
        for (const RoomFile& file : waiting) tallest = std::max(tallest, file.height());
        int x = left;
        for (RoomFile& file : waiting) {
            MapRoom room;
            room.name = file.name;
            room.title = file.title;
            room.position = {x, bottom - 8 - tallest};
            room.inWorld = false;
            room.handMade = true;
            room.doors = file.doors();
            LevelInfo info = file.toLevel(false);
            try {
                info.level->setTheme(Theme::get(file.theme));
            } catch (const std::exception&) {
            }
            room.picture = picture(*info.level);
            room.shape = std::move(file);
            x += room.shape.width() + 4;
            m_map.push_back(std::move(room));
        }
        m_mapLinks = links(std::nullopt, {});
    }

    void fitMap()
    {
        if (m_map.empty()) return;
        int left = m_map[0].position.x, bottom = m_map[0].position.y, right = left, top = bottom;
        for (const MapRoom& r : m_map) {
            left = std::min(left, r.position.x);
            bottom = std::min(bottom, r.position.y);
            right = std::max(right, r.position.x + r.shape.width());
            top = std::max(top, r.position.y + r.shape.height());
        }
        const sf::FloatRect map = mapArea();
        const float w = static_cast<float>((right - left) * TILE), h = static_cast<float>((top - bottom) * TILE);
        m_mapZoom = std::clamp(std::min((map.width - 60.f) / w, (map.height - 80.f) / h), 0.05f, 4.f);
        m_mapOrigin = {map.left + (map.width - w * m_mapZoom) / 2.f - static_cast<float>(left * TILE) * m_mapZoom,
                       map.top + (map.height - h * m_mapZoom) / 2.f + 10.f + static_cast<float>(top * TILE) * m_mapZoom};
    }

    // Where a room put at `at` is in the window.
    sf::FloatRect mapRect(const MapRoom& room, sf::Vector2i at) const
    {
        const float size = m_mapZoom * TILE;
        return {m_mapOrigin.x + static_cast<float>(at.x) * size, m_mapOrigin.y - static_cast<float>(at.y + room.shape.height()) * size,
                static_cast<float>(room.shape.width()) * size, static_cast<float>(room.shape.height()) * size};
    }

    std::optional<size_t> mapRoomAt(sf::Vector2i mouse) const
    {
        for (size_t i = m_map.size(); i-- > 0;) {
            if (mapRect(m_map[i], m_map[i].position).contains(sf::Vector2f(mouse))) return i;
        }
        return std::nullopt;
    }

    // The doors of a and b that meet, a put at pa and b at pb: side by side, lined up, as wide.
    std::vector<std::pair<RoomFile::Door, RoomFile::Door>> meeting(const MapRoom& a, sf::Vector2i pa, const MapRoom& b, sf::Vector2i pb) const
    {
        std::vector<std::pair<RoomFile::Door, RoomFile::Door>> found;
        for (const RoomFile::Door& da : a.doors) {
            for (const RoomFile::Door& db : b.doors) {
                if (db.side != opposite(da.side) || db.length != da.length) continue;
                bool meet = false;
                switch (da.side) {
                    case 'R': meet = pb.x == pa.x + a.shape.width() && pa.y + da.start == pb.y + db.start; break;
                    case 'L': meet = pa.x == pb.x + b.shape.width() && pa.y + da.start == pb.y + db.start; break;
                    case 'T': meet = pb.y == pa.y + a.shape.height() && pa.x + da.start == pb.x + db.start; break;
                    default: meet = pa.y == pb.y + b.shape.height() && pa.x + da.start == pb.x + db.start; break;
                }
                if (meet) found.push_back({da, db});
            }
        }
        return found;
    }

    bool placed(size_t i, std::optional<size_t> moving) const
    {
        return m_map[i].inWorld || moving == i;
    }

    // Every pair of doors that meet, between the rooms in the world, with `moving` (if any) put at `at`.
    std::vector<Link> links(std::optional<size_t> moving, sf::Vector2i at) const
    {
        auto where = [&](size_t i) { return moving == i ? at : m_map[i].position; };
        std::vector<Link> found;
        for (size_t a = 0; a < m_map.size(); ++a) {
            for (size_t b = a + 1; b < m_map.size(); ++b) {
                if (!placed(a, moving) || !placed(b, moving)) continue;
                for (const auto& [da, db] : meeting(m_map[a], where(a), m_map[b], where(b))) found.push_back({a, b, da, db});
            }
        }
        return found;
    }

    // Where a room being dragged to `raw` goes: lined up with a door it's near, if it's near one.
    sf::Vector2i snap(size_t moving, sf::Vector2i raw) const
    {
        const MapRoom& d = m_map[moving];
        std::optional<sf::Vector2i> best;
        int bestDistance = 4; // tiles
        for (size_t i = 0; i < m_map.size(); ++i) {
            if (i == moving || !m_map[i].inWorld) continue;
            const MapRoom& o = m_map[i];
            for (const RoomFile::Door& dd : d.doors) {
                for (const RoomFile::Door& od : o.doors) {
                    if (od.side != opposite(dd.side) || od.length != dd.length) continue;
                    sf::Vector2i p;
                    switch (dd.side) {
                        case 'R': p = {o.position.x - d.shape.width(), o.position.y + od.start - dd.start}; break;
                        case 'L': p = {o.position.x + o.shape.width(), o.position.y + od.start - dd.start}; break;
                        case 'T': p = {o.position.x + od.start - dd.start, o.position.y - d.shape.height()}; break;
                        default: p = {o.position.x + od.start - dd.start, o.position.y + o.shape.height()}; break;
                    }
                    const int distance = std::max(std::abs(p.x - raw.x), std::abs(p.y - raw.y));
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        best = p;
                    }
                }
            }
        }
        return best.value_or(raw);
    }

    // Why the room can't go at `at`, if it can't: it would overlap another, or join none, or cut some
    // off from the world, or leave the ways out of it (the doors joined to nothing) in more than one room.
    std::optional<std::string> mapProblem(size_t moving, sf::Vector2i at, const std::vector<Link>& joined) const
    {
        const MapRoom& d = m_map[moving];
        if (!d.inWorld) {
            const auto starts = std::count_if(d.shape.grid.begin(), d.shape.grid.end(), [](const std::string& row) { return row.find('S') != std::string::npos; });
            if (starts == 0) return "It needs a Start (S) to go in the world: click it to edit it";
        }
        for (size_t i = 0; i < m_map.size(); ++i) {
            if (i == moving || !m_map[i].inWorld) continue;
            const MapRoom& o = m_map[i];
            if (at.x < o.position.x + o.shape.width() && o.position.x < at.x + d.shape.width() && at.y < o.position.y + o.shape.height() &&
                o.position.y < at.y + d.shape.height()) {
                return "It would overlap " + o.name;
            }
        }
        if (std::none_of(joined.begin(), joined.end(), [&](const Link& l) { return l.a == moving || l.b == moving; })) {
            return "None of its doors meets another room's there: put a door against one as wide";
        }
        // Everything has to be reachable from the first room, where the game starts.
        size_t first = 0;
        while (first < m_map.size() && !placed(first, moving)) ++first;
        std::vector<bool> seen(m_map.size(), false);
        std::vector<size_t> todo = {first};
        seen[first] = true;
        while (!todo.empty()) {
            const size_t i = todo.back();
            todo.pop_back();
            for (const Link& l : joined) {
                const size_t other = l.a == i ? l.b : l.b == i ? l.a : m_map.size();
                if (other < m_map.size() && !seen[other]) {
                    seen[other] = true;
                    todo.push_back(other);
                }
            }
        }
        for (size_t i = 0; i < m_map.size(); ++i) {
            if (placed(i, moving) && !seen[i]) return "That would cut " + m_map[i].name + " off from the world";
        }
        std::vector<std::string> waysOut;
        for (size_t i = 0; i < m_map.size(); ++i) {
            if (!placed(i, moving)) continue;
            for (const RoomFile::Door& door : m_map[i].doors) {
                const bool isJoined = std::any_of(joined.begin(), joined.end(), [&](const Link& l) {
                    return (l.a == i && l.da.side == door.side && l.da.start == door.start) || (l.b == i && l.db.side == door.side && l.db.start == door.start);
                });
                if (!isJoined && door.side != 'B' && std::find(waysOut.begin(), waysOut.end(), m_map[i].name) == waysOut.end()) {
                    waysOut.push_back(m_map[i].name);
                }
            }
        }
        if (waysOut.empty()) return "Then no door would lead out of the world, for the end of the game";
        if (waysOut.size() > 1) {
            std::string names;
            for (const std::string& n : waysOut) names += (names.empty() ? "" : ", ") + n;
            return "Then " + names + " would all have doors joined to nothing: only one room can (the last, the end of the game)";
        }
        return std::nullopt;
    }

    // Puts the room at `at`: its file says its joins there, the other hand-made rooms' files lose
    // their joins to it, and the world is built again. Undone if it doesn't build.
    void moveRoom(size_t moving, sf::Vector2i at)
    {
        const MapRoom& room = m_map[moving];
        const std::string name = room.name;
        const fs::path path = roomsFolder() / (name + ".txt");
        std::vector<FileState> before;
        auto keep = [&](const fs::path& file) {
            FileState state{file, std::nullopt};
            if (fs::exists(file)) {
                std::ifstream in(file, std::ios::binary);
                state.text = std::string(std::istreambuf_iterator<char>(in), {});
            }
            before.push_back(std::move(state));
        };
        try {
            keep(path);
            fs::create_directories(roomsFolder());
            if (!room.handMade) {
                // Made in Python: from now on it has a file.
                const auto [ok, output] = runCommand("cd " + quoted(m_root.string()) + " && python3 tools/make_world.py --export " + name + " " +
                                                     quoted(path.string()) + " 2>&1");
                if (!ok) throw std::runtime_error("Can't get " + name + " from tools/world/: " + lastLine(output));
            }
            RoomFile file = RoomFile::load(path.string());
            file.joins.clear();
            for (const Link& l : links(moving, at)) {
                if (l.a == moving) file.joins.push_back({l.da.side, l.da.start, m_map[l.b].name, l.db.side, l.db.start});
                if (l.b == moving) file.joins.push_back({l.db.side, l.db.start, m_map[l.a].name, l.da.side, l.da.start});
            }
            for (const std::string& other : filesIn(roomsFolder(), ".txt")) {
                if (other == name) continue;
                const fs::path otherPath = roomsFolder() / (other + ".txt");
                RoomFile otherFile = RoomFile::load(otherPath.string());
                const size_t had = otherFile.joins.size();
                otherFile.joins.erase(std::remove_if(otherFile.joins.begin(), otherFile.joins.end(), [&](const RoomFile::Join& j) { return j.room == name; }),
                                      otherFile.joins.end());
                if (otherFile.joins.size() == had) continue;
                keep(otherPath);
                otherFile.save(otherPath.string());
            }
            file.save(path.string());
        } catch (const std::exception& e) {
            restore(before);
            say(e.what(), hud::RED);
            return;
        }
        const auto [ok, output] = buildWorld();
        if (!ok) {
            restore(before);
            say("Put back: the world didn't build with it there: " + lastLine(output), hud::RED);
            return;
        }
        m_mapUndo.push_back(std::move(before));
        loadMap();
        // Still in sight?
        for (const MapRoom& r : m_map) {
            sf::FloatRect seen;
            if (r.name == name && (!mapArea().intersects(mapRect(r, r.position), seen) || seen.width < 40.f || seen.height < 40.f)) fitMap();
        }
        if (const auto failing = checkWorld()) say(name + " moved (Ctrl+Z puts it back), but " + *failing, ORANGE);
        else say(name + " moved, and the world's built with it there. Ctrl+Z puts it back", GREEN);
    }

    void restore(const std::vector<FileState>& files)
    {
        for (const FileState& file : files) {
            if (file.text) {
                std::ofstream(file.path, std::ios::binary) << *file.text;
            } else {
                std::error_code error;
                fs::remove(file.path, error);
            }
        }
    }

    void undoMove()
    {
        if (m_mapUndo.empty()) return say("No move to undo");
        restore(m_mapUndo.back());
        m_mapUndo.pop_back();
        const auto [ok, output] = buildWorld();
        loadMap();
        if (ok) say("Moved back");
        else say("Moved back, but the world doesn't build: " + lastLine(output), hud::RED);
    }

    void mapPress(const sf::Event::MouseButtonEvent& b)
    {
        const sf::Vector2i mouse(b.x, b.y);
        const auto room = mapRoomAt(mouse);
        if (b.button == sf::Mouse::Left && room) {
            m_mapPressed = room;
            m_mapDragging = false;
        } else if (b.button == sf::Mouse::Left || b.button == sf::Mouse::Middle) {
            m_mapPanning = true;
            m_mapOriginBefore = m_mapOrigin;
        }
        m_mapMouse = mouse;
    }

    void mapMoved(sf::Vector2i mouse)
    {
        const sf::Vector2i moved = mouse - m_mapMouse;
        if (m_mapPanning) m_mapOrigin = m_mapOriginBefore + sf::Vector2f(moved);
        if (!m_mapPressed) return;
        if (!m_mapDragging && std::abs(moved.x) + std::abs(moved.y) > 4) m_mapDragging = true;
        if (!m_mapDragging) return;
        const float size = m_mapZoom * TILE;
        const MapRoom& room = m_map[*m_mapPressed];
        const sf::Vector2i raw = room.position + sf::Vector2i(static_cast<int>(std::round(static_cast<float>(moved.x) / size)),
                                                              -static_cast<int>(std::round(static_cast<float>(moved.y) / size)));
        m_mapDragPos = snap(*m_mapPressed, raw);
        m_mapDragLinks = links(m_mapPressed, m_mapDragPos);
        m_mapProblem = mapProblem(*m_mapPressed, m_mapDragPos, m_mapDragLinks);
        if (m_mapProblem) say(*m_mapProblem, hud::RED);
        else say("Let go to put " + room.name + " here", GREEN);
    }

    void mapRelease()
    {
        if (m_mapPressed && m_mapDragging) {
            if (m_mapDragPos == m_map[*m_mapPressed].position) say("Left where it was");
            else if (m_mapProblem) say("Not moved: " + *m_mapProblem, hud::RED);
            else moveRoom(*m_mapPressed, m_mapDragPos);
        } else if (m_mapPressed) {
            const std::string name = m_map[*m_mapPressed].name;
            m_mapPressed.reset();
            return leaveMap(name);
        }
        m_mapPressed.reset();
        m_mapDragging = m_mapPanning = false;
    }

    void mapZoomAt(sf::Vector2f mouse, float zoom)
    {
        zoom = std::clamp(zoom, 0.05f, 6.f);
        m_mapOrigin = mouse - (mouse - m_mapOrigin) * (zoom / m_mapZoom);
        m_mapZoom = zoom;
    }

    void drawWorldMap()
    {
        const sf::FloatRect area = mapArea();
        hud::fillRect(window, area, sf::Color(18, 17, 26));
        const float size = m_mapZoom * TILE;
        const std::optional<size_t> dragged = m_mapDragging ? m_mapPressed : std::nullopt;
        const sf::Vector2i mouse = sf::Mouse::getPosition(window);
        const std::optional<size_t> hover = m_popup || m_prompt || dragged ? std::nullopt : mapRoomAt(mouse);
        // The doors: green where they meet another room's, orange where they lead nowhere (the
        // ways out), red in a floor (pits).
        auto drawDoors = [&](size_t i, sf::Vector2i at, const std::vector<Link>& joined, sf::Uint8 alpha) {
            const MapRoom& room = m_map[i];
            const sf::FloatRect r = mapRect(room, at);
            for (const RoomFile::Door& door : room.doors) {
                const bool isJoined = std::any_of(joined.begin(), joined.end(), [&](const Link& l) {
                    return (l.a == i && l.da.side == door.side && l.da.start == door.start) || (l.b == i && l.db.side == door.side && l.db.start == door.start);
                });
                const sf::Color colour = isJoined ? GREEN : door.side == 'B' ? hud::RED : ORANGE;
                const float thick = std::max(2.f, size), from = static_cast<float>(door.start) * size, length = static_cast<float>(door.length) * size;
                sf::FloatRect bar;
                switch (door.side) {
                    case 'L': bar = {r.left, r.top + r.height - from - length, thick, length}; break;
                    case 'R': bar = {r.left + r.width - thick, r.top + r.height - from - length, thick, length}; break;
                    case 'T': bar = {r.left + from, r.top, length, thick}; break;
                    default: bar = {r.left + from, r.top + r.height - thick, length, thick}; break;
                }
                hud::fillRect(window, bar, hud::withAlpha(colour, alpha));
            }
        };
        auto drawPicture = [&](const MapRoom& room, sf::Vector2i at, sf::Uint8 alpha) {
            const sf::FloatRect r = mapRect(room, at);
            sf::Sprite sprite(room.picture->getTexture());
            sprite.setScale(m_mapZoom, m_mapZoom);
            sprite.setPosition(r.left, r.top);
            sprite.setColor(sf::Color(255, 255, 255, alpha));
            hud::fillRect(window, r, sf::Color(30, 28, 44, alpha));
            window.draw(sprite);
        };
        for (size_t i = 0; i < m_map.size(); ++i) {
            const MapRoom& room = m_map[i];
            const bool isDragged = dragged == i;
            drawPicture(room, room.position, isDragged ? 70 : room.inWorld ? 255 : 170);
            drawDoors(i, room.position, m_mapLinks, isDragged ? 70 : 200);
            hud::frame(window, mapRect(room, room.position), hover == i ? hud::WHITE : sf::Color(255, 255, 255, room.inWorld ? 50 : 30));
        }
        if (dragged) {
            const MapRoom& room = m_map[*dragged];
            drawPicture(room, m_mapDragPos, 230);
            drawDoors(*dragged, m_mapDragPos, m_mapDragLinks, 255);
            const sf::FloatRect r = mapRect(room, m_mapDragPos);
            hud::frame(window, r, m_mapProblem ? hud::RED : GREEN);
            hud::frame(window, {r.left - 1.f, r.top - 1.f, r.width + 2.f, r.height + 2.f}, m_mapProblem ? hud::RED : GREEN);
        }
        // Names, in the panel's font.
        window.setView(sf::View(sf::FloatRect(0.f, 0.f, uiSize().x, uiSize().y)));
        bool waiting = false;
        for (size_t i = 0; i < m_map.size(); ++i) {
            const MapRoom& room = m_map[i];
            const sf::FloatRect r = mapRect(room, dragged == i ? m_mapDragPos : room.position);
            const std::string label = room.name + (room.handMade ? "" : " (Python)");
            if (r.width / UI > text::width(room.name) * 0.6f || hover == i) {
                text::draw(window, label, {std::round(r.left / UI) + 2.f, std::round(r.top / UI) + 1.f}, hover == i ? hud::GOLD : hud::WHITE);
            }
            if (!room.inWorld && !waiting) {
                waiting = true;
                const sf::FloatRect spot = mapRect(room, room.position);
                text::draw(window, "Not in the world yet: drag them in", {std::round(spot.left / UI), std::round(spot.top / UI) - 12.f}, ORANGE);
            }
        }
        window.setView(pixels());
    }

    // ------------------------------------------------------------------------------------------
    // The panel

    struct Cursor
    {
        float y;
    };

    void heading(Cursor& at, const std::string& title)
    {
        at.y += 4.f;
        text::draw(window, title, {5.f, at.y}, hud::GOLD);
        at.y += ROW + 1.f;
    }

    void line(Cursor& at, const std::string& s, sf::Color colour = hud::GREY)
    {
        for (const std::string& part : text::wrap(s, PANEL_WIDTH - 10.f)) {
            text::draw(window, part, {5.f, at.y}, colour);
            at.y += ROW;
        }
    }

    // "Label   value", the value clickable.
    void setting(Cursor& at, const std::string& label, const std::string& value, std::function<void(bool)> act)
    {
        const sf::FloatRect area(3.f, at.y - 1.f, PANEL_WIDTH - 6.f, ROW);
        if (!m_popup && hovered(area)) hud::fillRect(window, area, HOVER);
        text::draw(window, label, {5.f, at.y}, hud::GREY);
        text::draw(window, value, {62.f, at.y}, hud::CYAN);
        m_panelHits.push_back({area, std::move(act)});
        at.y += ROW;
    }

    sf::FloatRect button(float x, float y, float w, const std::string& label, std::function<void(bool)> act)
    {
        const sf::FloatRect area(x, y, w, ROW);
        hud::fillRect(window, area, !m_popup && hovered(area) ? CHOSEN : HOVER);
        text::draw(window, label, {x + (w - text::width(label)) / 2.f, y}, hud::WHITE);
        m_panelHits.push_back({area, std::move(act)});
        return area;
    }

    sf::Vector2f mouseUi() const
    {
        return sf::Vector2f(sf::Mouse::getPosition(window)) / UI;
    }

    // Whether the mouse is over something in the panel (scrolled by m_drawScroll) or the popup.
    bool hovered(const sf::FloatRect& area) const
    {
        return !m_prompt && area.contains(mouseUi() + sf::Vector2f(0.f, m_drawScroll));
    }

    void drawBrushIcon(const Brush& b, sf::Vector2f at)
    {
        if (b.sheet >= 0) {
            sf::Sprite sprite(m_built.level->theme().tiles, {(b.sheet % 8) * TILE, (b.sheet / 8) * TILE, TILE, TILE});
            sprite.setPosition(at);
            window.draw(sprite);
        } else {
            hud::fillRect(window, {at.x, at.y, 8.f, 8.f}, b.colour);
            if (b.c != 'w' && b.c != '.') text::draw(window, std::string(1, b.c), {at.x + 1.f, at.y - 1.f}, sf::Color(20, 20, 30));
        }
    }

    void brushes(Cursor& at, const std::vector<Brush>& list)
    {
        const float columnWidth = (PANEL_WIDTH - 6.f) / 2.f;
        for (size_t i = 0; i < list.size(); ++i) {
            const Brush& b = list[i];
            const float x = 3.f + static_cast<float>(i % 2) * columnWidth, y = at.y + static_cast<float>(i / 2) * ROW;
            const sf::FloatRect area(x, y - 1.f, columnWidth, ROW);
            if (m_brush == b.c) hud::fillRect(window, area, CHOSEN);
            else if (!m_popup && hovered(area)) hud::fillRect(window, area, HOVER);
            drawBrushIcon(b, {x + 2.f, y});
            text::draw(window, b.name, {x + 13.f, y}, hud::WHITE);
            m_panelHits.push_back({area, [this, c = b.c, name = b.name](bool) {
                                       m_brush = c;
                                       say(name);
                                   }});
        }
        at.y += static_cast<float>((list.size() + 1) / 2) * ROW;
    }

    void choose(const std::string& title, std::vector<std::string> items, std::function<void(size_t)> pick, bool icons = false)
    {
        m_popup = Popup{title, std::move(items), std::move(pick), icons};
    }

    void ask(const std::string& label, const std::string& value, std::function<void(const std::string&)> done, bool spaces = false)
    {
        m_prompt = Prompt{label, value, std::move(done), spaces};
    }

    // The fields new things of the chosen kind get.
    void brushOptions(Cursor& at)
    {
        const RoomFile::Fields& f = m_options[m_brush];
        switch (m_brush) {
            case 'P':
                setting(at, "Gives", value(f, "ability"), [this](bool) {
                    choose("What it gives", ABILITIES, [this](size_t i) { m_options['P']["ability"] = ABILITIES[i]; });
                });
                break;
            case 'N':
                setting(at, "Says", value(f, "dialogue").empty() ? "(nothing)" : value(f, "dialogue"), [this](bool) {
                    auto files = filesIn(config::assetPath("dialogue"), ".txt");
                    choose("What they say: assets/dialogue/", files, [this, files](size_t i) { m_options['N']["dialogue"] = files[i]; });
                });
                setting(at, "Looks", value(f, "look"), [this](bool) {
                    std::vector<std::string> looks(Player::LOOKS.begin(), Player::LOOKS.end());
                    choose("How they look", looks, [this, looks](size_t i) { m_options['N']["look"] = looks[i]; });
                });
                setting(at, "Pose", value(f, "pose") == "sit" ? "sitting" : "standing", [this](bool) {
                    if (value(m_options['N'], "pose") == "sit") m_options['N'].erase("pose");
                    else m_options['N']["pose"] = "sit";
                });
                factSetting(at, 'N', "needs", "Only once", "There only once this holds: an ability or a story flag (empty: always)");
                factSetting(at, 'N', "until", "Only until", "Gone once this holds: an ability or a story flag (empty: never)");
                break;
            case 'D':
                setting(at, "Kind", value(f, "kind"), [this](bool) {
                    auto kinds = propKinds();
                    choose("Which prop", kinds, [this, kinds](size_t i) { m_options['D']["kind"] = kinds[i]; }, true);
                });
                factSetting(at, 'D', "needs", "Lit once", "Drawn as its _off picture until this holds (a beacon: an ability)");
                break;
            case 'K':
                setting(at, "Rolls", value(f, "dir") == "-1" ? "left" : "right", [this](bool) {
                    m_options['K']["dir"] = value(m_options['K'], "dir") == "-1" ? "1" : "-1";
                });
                factSetting(at, 'K', "needs", "Needs", "What has to hold before it moves, by commas: long_rope,wall_climb (empty: nothing)");
                break;
            case 'T':
                setting(at, "Route", value(f, "route"), [this](bool) {
                    std::vector<std::string> ids, labels;
                    for (const regions::Region& r : regions::all()) {
                        ids.push_back(r.id);
                        labels.push_back(r.id + ": " + r.name);
                    }
                    choose("Which route's timed run: assets/regions.txt", labels, [this, ids](size_t i) { m_options['T']["route"] = ids[i]; });
                });
                setting(at, "Post", value(f, "mark") == "finish" ? "the finish" : "the start", [this](bool) {
                    m_options['T']["mark"] = value(m_options['T'], "mark") == "finish" ? "start" : "finish";
                });
                break;
            case 'R':
                setting(at, "Track to", value(f, "to") + " tiles", [this](bool) {
                    ask("Where its track ends, in tiles right and up from it: \"6,0\"", m_options['R']["to"], [this](const std::string& v) {
                        int dx = 0, dy = 0;
                        if (std::sscanf(v.c_str(), "%d,%d", &dx, &dy) == 2) m_options['R']["to"] = std::to_string(dx) + "," + std::to_string(dy);
                        else say("That isn't \"right,up\", like 6,0", hud::RED);
                    });
                });
                setting(at, "Period", value(f, "period") + " s", [this](bool) {
                    ask("Seconds there and back", m_options['R']["period"], [this](const std::string& v) {
                        if (std::strtof(v.c_str(), nullptr) > 0.f) m_options['R']["period"] = v;
                        else say("That isn't a number of seconds", hud::RED);
                    });
                });
                break;
            default: return;
        }
        line(at, "Click again on one to change it.");
    }

    // A fact a new thing of that kind waits for: typed in, empty for none.
    void factSetting(Cursor& at, char kind, const std::string& field, const char* label, const std::string& question)
    {
        const std::string now = value(m_options[kind], field);
        setting(at, label, now.empty() ? "(always)" : now, [this, kind, field, question, now](bool) {
            ask(question, now, [this, kind, field](const std::string& v) {
                if (v.empty()) m_options[kind].erase(field);
                else m_options[kind][field] = v;
            });
        });
    }

    void roomSettings(Cursor& at)
    {
        setting(at, "Name", m_room.name, [this](bool) { showOpenList(); });
        if (m_fromPython) line(at, "Made in Python (tools/world/): saving it makes its file, used instead from then on.", ORANGE);
        setting(at, "Title", m_room.title, [this](bool) {
            ask("The room's title, shown the first time you go in", m_room.title, [this](const std::string& v) {
                if (v.empty() || v == m_room.title) return;
                remember();
                m_room.title = v;
            }, true);
        });
        setting(at, "Theme", m_room.theme, [this](bool) {
            auto names = themes();
            choose("Theme: assets/gfx/themes/", names, [this, names](size_t i) {
                remember();
                m_room.theme = names[i];
            });
        });
        setting(at, "Region", m_room.region.empty() ? "(none)" : m_room.region, [this](bool) {
            std::vector<std::string> ids, labels;
            for (const regions::Region& r : regions::all()) {
                ids.push_back(r.id);
                labels.push_back(r.id + ": " + r.name);
            }
            choose("Region: assets/regions.txt", labels, [this, ids](size_t i) {
                remember();
                m_room.region = ids[i];
            });
        });
        // Size: typed in (grown or shrunk on the right and at the top), or a side at a time. Shift: by 5.
        const std::string size = std::to_string(m_room.width()) + " x " + std::to_string(m_room.height());
        setting(at, "Size", size + " tiles", [this, size](bool) {
            ask("Width x height, in tiles (a screen is 40 x 23)", size, [this](const std::string& v) {
                int w = 0, h = 0;
                if (std::sscanf(v.c_str(), "%d x %d", &w, &h) != 2 && std::sscanf(v.c_str(), "%d %d", &w, &h) != 2) {
                    return say("That isn't \"width x height\", like 40 x 23", hud::RED);
                }
                grow(0, w - m_room.width(), 0, h - m_room.height());
            }, true);
        });
        auto sides = [&](const char* first, char a, const char* second, char b) {
            const auto by = [](bool shift, int step) { return shift ? 5 * step : step; };
            auto growSide = [this](char side, int n) { grow(side == 'L' ? n : 0, side == 'R' ? n : 0, side == 'B' ? n : 0, side == 'T' ? n : 0); };
            text::draw(window, first, {5.f, at.y}, hud::GREY);
            button(44.f, at.y - 1.f, 18.f, "-", [=](bool shift) { growSide(a, by(shift, -1)); });
            button(64.f, at.y - 1.f, 18.f, "+", [=](bool shift) { growSide(a, by(shift, 1)); });
            text::draw(window, second, {96.f, at.y}, hud::GREY);
            button(140.f, at.y - 1.f, 18.f, "-", [=](bool shift) { growSide(b, by(shift, -1)); });
            button(160.f, at.y - 1.f, 18.f, "+", [=](bool shift) { growSide(b, by(shift, 1)); });
            at.y += ROW + 2.f;
        };
        sides("Left", 'L', "Right", 'R');
        sides("Bottom", 'B', "Top", 'T');
        char wind[32];
        std::snprintf(wind, sizeof wind, "%g", m_room.wind);
        setting(at, "Wind", m_room.wind == 0.f ? "none" : wind, [this](bool) {
            char now[32];
            std::snprintf(now, sizeof now, "%g", m_room.wind);
            ask("Wind: pixels a second each second, + to the right (0: none)", now, [this](const std::string& v) {
                remember();
                m_room.wind = std::strtof(v.c_str(), nullptr);
            });
        });
        setting(at, "Gravity", m_room.space ? "none: floating" : "yes", [this](bool) {
            remember();
            m_room.space = !m_room.space;
        });
        setting(at, "Dark", m_room.dark ? "yes" : "no", [this](bool) {
            remember();
            m_room.dark = !m_room.dark;
        });
    }

    void doorList(Cursor& at)
    {
        const auto doors = m_room.doors();
        if (doors.empty()) line(at, "None yet. Erase part of an outside wall (right button) to make one.");
        for (const RoomFile::Door& door : doors) {
            const RoomFile::Join* join = joinOf(door.side, door.start);
            line(at, doorName(door.side, door.start, door.length), hud::WHITE);
            const std::string to = join ? join->room + ", " + sideName(join->otherSide) + " " + std::to_string(join->otherStart)
                                        : door.side == 'B' ? "nowhere: a pit" : "nowhere yet";
            setting(at, "  leads to", to, [this, door](bool) { chooseJoin(door); });
        }
        // Joins whose door isn't there any more.
        for (size_t i = 0; i < m_room.joins.size(); ++i) {
            const RoomFile::Join& join = m_room.joins[i];
            const bool there = std::any_of(doors.begin(), doors.end(), [&](const RoomFile::Door& d) { return d.side == join.side && d.start == join.start; });
            if (there) continue;
            line(at, "No door in the " + std::string(sideName(join.side)) + " wall at " + std::to_string(join.start) + " any more, to " + join.room, hud::RED);
            setting(at, "", "forget it", [this, i](bool) {
                remember();
                m_room.joins.erase(m_room.joins.begin() + static_cast<long>(i));
            });
        }
    }

    void chooseJoin(RoomFile::Door door)
    {
        std::vector<std::string> labels = {door.side == 'B' ? "nowhere: a pit" : "nowhere (the end of the game, if no other door is)"};
        std::vector<std::optional<OtherDoor>> targets = {std::nullopt};
        for (const OtherDoor& other : m_otherDoors) {
            const RoomFile::Join* mine = joinOf(door.side, door.start);
            const bool joinedToMe = mine && mine->room == other.room && mine->otherSide == other.door.side && mine->otherStart == other.door.start;
            if (other.door.side != opposite(door.side) || other.door.length != door.length || (other.taken && !joinedToMe)) continue;
            labels.push_back(other.room + ": " + doorName(other.door.side, other.door.start, other.door.length));
            targets.push_back(other);
        }
        if (targets.size() == 1) {
            labels.push_back("(no other room has a free " + std::string(sideName(opposite(door.side))) + " door " + std::to_string(door.length) +
                             " tiles wide)");
            targets.push_back(std::nullopt);
        }
        // Doors in the facing wall that would do but for their size: to say what to change.
        for (const OtherDoor& other : m_otherDoors) {
            if (other.door.side != opposite(door.side) || other.door.length == door.length || other.taken) continue;
            labels.push_back("(" + other.room + ": " + doorName(other.door.side, other.door.start, other.door.length) + " is " +
                             std::to_string(other.door.length) + " tiles, this one " + std::to_string(door.length) + ": make them the same)");
            targets.push_back(std::nullopt);
        }
        choose("Where the " + doorName(door.side, door.start, door.length) + " leads", labels, [this, door, targets](size_t i) {
            if (i >= targets.size() || (i > 0 && !targets[i])) return;
            remember();
            auto& joins = m_room.joins;
            joins.erase(std::remove_if(joins.begin(), joins.end(), [&](const RoomFile::Join& j) { return j.side == door.side && j.start == door.start; }),
                        joins.end());
            if (targets[i]) joins.push_back({door.side, door.start, targets[i]->room, targets[i]->door.side, targets[i]->door.start});
        });
    }

    void mapPanel(Cursor& at)
    {
        heading(at, "World map");
        line(at, "Every room where it is in the world. Drag one to move it: it snaps to where one of its doors "
                 "meets another room's, and is joined to the rooms whose doors meet its own there (and only those). "
                 "Click a room to edit it.");
        at.y += 3.f;
        line(at, "Green: doors that meet. Orange: the ways out, which have to be in one room (the end of the game). "
                 "Red: pits. (Python): made in tools/world/; moving one gives it a file.");
        at.y += 4.f;
        const float half = (PANEL_WIDTH - 9.f) / 2.f;
        button(3.f, at.y, half, "Back (Tab)", [this](bool) { leaveMap(m_room.name); });
        button(6.f + half, at.y, half, "Undo move", [this](bool) { undoMove(); });
        at.y += ROW + 3.f;
        button(3.f, at.y, half, "New room...", [this](bool) {
            ask("The new room's name: small letters, digits and _", "", [this](const std::string& name) { leaveMap(name); });
        });
        button(6.f + half, at.y, half, "Fit", [this](bool) { fitMap(); });
        at.y += ROW + 3.f;
        heading(at, "Rooms");
        for (const MapRoom& room : m_map) {
            setting(at, room.name, room.inWorld ? room.title : "(not in the world)", [this, name = room.name](bool) { leaveMap(name); });
        }
        at.y += 4.f;
        line(at, "Wheel: zoom. Drag the background: move. Ctrl+Z: undo a move. Tab or Esc: back to the room.");
    }

    void drawPanel()
    {
        const sf::Vector2f ui = uiSize();
        const float height = ui.y - STATUS_HEIGHT;
        sf::View view(sf::FloatRect(0.f, 0.f, PANEL_WIDTH, height));
        view.setViewport({0.f, 0.f, PANEL_WIDTH / ui.x, height / ui.y});
        window.setView(view);
        hud::fillRect(window, {0.f, 0.f, PANEL_WIDTH, height}, PANEL);
        hud::fillRect(window, {PANEL_WIDTH - 1.f, 0.f, 1.f, height}, PANEL_EDGE);
        // Scrolled: everything is drawn moved up, and the mouse is compared to it the same way.
        view.move(0.f, m_panelScroll);
        window.setView(view);
        m_drawScroll = m_panelScroll;
        m_panelHits.clear();
        Cursor at{2.f};
        if (m_mapMode) {
            mapPanel(at);
            m_panelHeight = at.y + 4.f;
            m_drawScroll = 0.f;
            window.setView(sf::View(sf::FloatRect(0.f, 0.f, ui.x, ui.y)));
            return;
        }
        heading(at, "Tiles");
        brushes(at, TILES);
        heading(at, "Things");
        brushes(at, THINGS);
        if (isEntityChar(m_brush)) brushOptions(at);
        heading(at, "Room");
        roomSettings(at);
        if (!m_built.level->hasStart()) line(at, "No Start yet: put one in (S).", hud::RED);
        heading(at, "Doors");
        doorList(at);
        at.y += 4.f;
        const float third = (PANEL_WIDTH - 12.f) / 3.f;
        button(3.f, at.y, third, "Save", [this](bool) { saveToWorld(); });
        if (m_game) button(6.f + third, at.y, third, "Stop (F5)", [this](bool) { stopPlaying(); });
        else button(6.f + third, at.y, third, "Play (F5)", [this](bool) { startPlaying(std::nullopt); });
        button(9.f + 2.f * third, at.y, third, "Play world", [this](bool) { playWorld(); });
        at.y += ROW + 3.f;
        button(3.f, at.y, third, "Undo", [this](bool) { undo(m_undo, m_redo, "undo"); });
        button(6.f + third, at.y, third, "Redo", [this](bool) { undo(m_redo, m_undo, "redo"); });
        button(9.f + 2.f * third, at.y, third, "Open...", [this](bool) { showOpenList(); });
        at.y += ROW + 3.f;
        button(3.f, at.y, PANEL_WIDTH - 6.f, "World map (Tab)", [this](bool) { enterMap(); });
        at.y += ROW + 6.f;
        line(at, "Left: paint. Right: erase. Shift: a rectangle. Alt: pick up. Wheel: zoom. Middle or Space: move. "
                 "The handles on the room's sides: drag to make it bigger or smaller. Ctrl+S save. F5 play the room right here "
                 "(Shift+F5: from the mouse), F5 again to stop. F6 the world, in the game. Ctrl+O open another, Tab the world map. "
                 "Ctrl+Z undo, Ctrl+0 fit it in. Typing a tile's character picks it.");
        m_panelHeight = at.y + 4.f;
        m_drawScroll = 0.f;
        window.setView(sf::View(sf::FloatRect(0.f, 0.f, ui.x, ui.y)));
    }

    void drawStatus()
    {
        const sf::Vector2f ui = uiSize();
        const float top = ui.y - STATUS_HEIGHT;
        hud::fillRect(window, {0.f, top, ui.x, STATUS_HEIGHT}, PANEL);
        hud::fillRect(window, {0.f, top, ui.x, 1.f}, PANEL_EDGE);
        text::draw(window, m_message, {5.f, top + 2.f}, m_messageColour);
        // Right: where the mouse is, what's there.
        std::string where;
        if (m_game) {
            where = "playing " + m_room.name + (m_unsaved ? " (unsaved)" : "");
            text::draw(window, where, {ui.x - text::width(where) - 5.f, top + 2.f}, GREEN);
            return;
        }
        const sf::Vector2i mouse = sf::Mouse::getPosition(window);
        if (m_mapMode) {
            if (const auto i = mapRoomAt(mouse); i && !m_mapDragging) {
                const MapRoom& r = m_map[*i];
                where = r.name + ": " + r.title + "   " + std::to_string(r.shape.width()) + " x " + std::to_string(r.shape.height()) + "   at " +
                        std::to_string(r.position.x) + ", " + std::to_string(r.position.y);
            }
            text::draw(window, where, {ui.x - text::width(where) - 5.f, top + 2.f}, hud::GREY);
            return;
        }
        if (mapArea().contains(sf::Vector2f(mouse))) {
            const sf::Vector2i tile = tileAt(mouse);
            if (m_room.inside(tile.x, tile.y)) {
                const Brush* b = brushOf(m_room.at(tile.x, tile.y));
                where = "x " + std::to_string(tile.x) + "  y " + std::to_string(tile.y) + "   " + (b ? b->name : "?");
                if (const auto it = m_room.fields.find({tile.x, tile.y}); it != m_room.fields.end()) {
                    for (const auto& [k, v] : it->second) where += "  " + k + "=" + v;
                }
                where += "      ";
            }
        }
        where += m_room.name + (m_unsaved ? " (unsaved)" : "") + "   x" + std::to_string(static_cast<int>(m_zoom));
        text::draw(window, where, {ui.x - text::width(where) - 5.f, top + 2.f}, hud::GREY);
    }

    void drawPopup()
    {
        m_popupHits.clear();
        if (!m_popup) return;
        const sf::Vector2f ui = uiSize();
        hud::fillRect(window, {0.f, 0.f, ui.x, ui.y}, sf::Color(0, 0, 0, 120));
        const float row = m_popup->icons ? 20.f : ROW + 1.f;
        float width = std::max(260.f, text::width(m_popup->title) + text::width("(wheel: more)") + 24.f);
        for (const std::string& item : m_popup->items) width = std::max(width, text::width(item) + (m_popup->icons ? 40.f : 14.f));
        const float listHeight = std::min(static_cast<float>(m_popup->items.size()), std::floor((ui.y - 70.f) / row)) * row;
        const sf::FloatRect box(PANEL_WIDTH + (ui.x - PANEL_WIDTH - width) / 2.f, (ui.y - listHeight) / 2.f - 8.f, width, listHeight + 18.f);
        hud::fillRect(window, box, PANEL);
        hud::frame(window, box, PANEL_EDGE);
        text::draw(window, m_popup->title, {box.left + 5.f, box.top + 3.f}, hud::GOLD);
        const float maxScroll = std::max(0.f, static_cast<float>(m_popup->items.size()) * row - listHeight);
        m_popup->scroll = std::min(m_popup->scroll, maxScroll);
        const float listTop = box.top + 15.f;
        for (size_t i = 0; i < m_popup->items.size(); ++i) {
            const float y = listTop + static_cast<float>(i) * row - m_popup->scroll;
            if (y < listTop - 0.5f || y + row > listTop + listHeight + 0.5f) continue;
            const sf::FloatRect area(box.left + 2.f, y, width - 4.f, row);
            if (hovered(area)) hud::fillRect(window, area, CHOSEN);
            float textX = box.left + 6.f;
            if (m_popup->icons) {
                drawPropIcon(m_popup->items[i], {box.left + 14.f, y + row - 2.f});
                textX += 26.f;
            }
            text::draw(window, m_popup->items[i], {textX, y + (row - ROW) / 2.f + 1.f}, hud::WHITE);
            m_popupHits.push_back({area, [i, pick = m_popup->pick](bool) { pick(i); }});
        }
        if (maxScroll > 0.f) text::draw(window, "(wheel: more)", {box.left + width - text::width("(wheel: more)") - 6.f, box.top + 3.f}, hud::GREY);
    }

    // A prop, standing with its feet at `feet`, small enough for a list.
    void drawPropIcon(const std::string& kind, sf::Vector2f feet)
    {
        const PropInfo* info = props::find(kind);
        if (!info) {
            hud::fillRect(window, {feet.x - 4.f, feet.y - 16.f, 8.f, 16.f},
                          kind == props::WATERFALL ? sf::Color(120, 200, 240, 200) : sf::Color(255, 238, 200, 120));
            return;
        }
        sf::Sprite sprite(props::texture(), info->rect);
        const float scale = std::min(1.f, 18.f / static_cast<float>(std::max(info->rect.width, info->rect.height)));
        sprite.setScale(scale, scale);
        sprite.setOrigin(static_cast<float>(info->rect.width) / 2.f, static_cast<float>(info->rect.height));
        sprite.setPosition(feet);
        window.draw(sprite);
    }

    void drawPrompt()
    {
        if (!m_prompt) return;
        const sf::Vector2f ui = uiSize();
        hud::fillRect(window, {0.f, 0.f, ui.x, ui.y}, sf::Color(0, 0, 0, 120));
        const float width = std::max(280.f, text::width(m_prompt->label) + 16.f);
        const sf::FloatRect box(PANEL_WIDTH + (ui.x - PANEL_WIDTH - width) / 2.f, ui.y / 2.f - 24.f, width, 46.f);
        hud::fillRect(window, box, PANEL);
        hud::frame(window, box, PANEL_EDGE);
        text::draw(window, m_prompt->label, {box.left + 6.f, box.top + 4.f}, hud::GOLD);
        hud::fillRect(window, {box.left + 6.f, box.top + 17.f, width - 12.f, ROW + 1.f}, sf::Color(12, 12, 20));
        const bool blink = static_cast<int>(m_clock.getElapsedTime().asSeconds() * 2.f) % 2 == 0;
        text::draw(window, m_prompt->value + (blink ? "_" : ""), {box.left + 8.f, box.top + 18.f}, hud::WHITE);
        text::draw(window, "Enter: OK    Esc: never mind", {box.left + 6.f, box.top + 32.f}, hud::GREY);
    }

    void draw()
    {
        window.clear(sf::Color(14, 13, 20));
        window.setView(pixels());
        if (m_mapMode) drawWorldMap();
        else drawMap();
        window.setView(sf::View(sf::FloatRect(0.f, 0.f, uiSize().x, uiSize().y)));
        drawPanel();
        drawStatus();
        drawPopup();
        drawPrompt();
        window.setTitle("Room editor: " + m_room.name + (m_unsaved ? " *" : ""));
        updateCursor();
        window.display();
    }
};

int main(int argc, char** argv)
{
    if (argc != 1 && argc != 2 && argc != 4) {
        std::cerr << "Usage: editor [<name> [<width> <height>]]\n"
                     "  Opens tools/world/rooms/<name>.txt, or the room of that name made in tools/world/, or makes a new\n"
                     "  one that size in tiles (40 x 23 if not said). With no name: pick one.\n";
        return 1;
    }
    try {
        const fs::path root = fs::weakly_canonical(fs::path(config::assetPath("")) / "..");
        const fs::path binaries = fs::absolute(fs::path(argv[0])).parent_path();
        Editor editor(root, binaries);
        if (argc == 1) {
            // The first room of the world, to have something to look at, and the list.
            const auto names = editor.worldRooms();
            if (const auto error = editor.open(names.empty() ? "new_room" : names.front())) throw std::runtime_error(*error);
            editor.showOpenList();
        } else if (const auto error = editor.open(argv[1], argc == 4 ? std::atoi(argv[2]) : 40, argc == 4 ? std::atoi(argv[3]) : 23)) {
            std::cerr << *error << std::endl;
            return 1;
        }
        editor.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
