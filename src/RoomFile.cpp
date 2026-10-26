#include "RoomFile.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
    std::string trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }

    bool isSide(char c)
    {
        return c == 'L' || c == 'R' || c == 'T' || c == 'B';
    }
}

bool isSolidChar(char c)
{
    return c == '#' || c == '%' || c == 'o' || c == '~' || c == '&' || c == '=' || c == 'm';
}

bool isEntityChar(char c)
{
    return !entities::identifierOf(c).empty();
}

RoomFile RoomFile::blank(const std::string& name, int width, int height)
{
    RoomFile room;
    room.name = name;
    room.title = name;
    room.grid.assign(static_cast<size_t>(height), std::string(static_cast<size_t>(width), '.'));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1) room.set(x, y, '#');
        }
    }
    return room;
}

bool RoomFile::looksLikeOne(const std::string& text)
{
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);) {
        if (trim(line) == "tiles") return true;
    }
    return false;
}

RoomFile RoomFile::parse(const std::string& text, const std::string& name)
{
    RoomFile room;
    room.name = name;
    room.title = name;
    std::istringstream stream(text);
    std::vector<std::string> rows; // top first
    struct Entity
    {
        int x, y;
        Fields fields;
    };
    std::vector<Entity> entities;
    bool inTiles = false;
    int lineNumber = 0;
    auto fail = [&](const std::string& what) {
        throw std::runtime_error(name + ".txt line " + std::to_string(lineNumber) + ": " + what);
    };
    for (std::string line; std::getline(stream, line);) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (inTiles) {
            rows.push_back(line);
            continue;
        }
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream words(line);
        std::string key;
        words >> key;
        std::string rest;
        std::getline(words, rest);
        rest = trim(rest);
        if (key == "tiles") inTiles = true;
        else if (key == "title") room.title = rest;
        else if (key == "theme") room.theme = rest;
        else if (key == "region") room.region = rest;
        else if (key == "wind") room.wind = std::strtof(rest.c_str(), nullptr);
        else if (key == "dark") room.dark = rest == "1";
        else if (key == "space") room.space = rest == "1";
        else if (key == "join") {
            Join join;
            std::string side, otherSide;
            std::istringstream parts(rest);
            if (!(parts >> side >> join.start >> join.room >> otherSide >> join.otherStart) || side.size() != 1 ||
                otherSide.size() != 1 || !isSide(side[0]) || !isSide(otherSide[0])) {
                fail("a join is \"join <side> <start> <room> <side> <start>\", sides L R T B");
            }
            join.side = side[0];
            join.otherSide = otherSide[0];
            room.joins.push_back(join);
        } else if (key == "entity") {
            Entity entity{};
            std::istringstream parts(rest);
            if (!(parts >> entity.x >> entity.y)) fail("an entity is \"entity <column> <row> name=value ...\"");
            for (std::string pair; parts >> pair;) {
                const size_t eq = pair.find('=');
                if (eq == std::string::npos) fail("\"" + pair + "\" isn't name=value");
                entity.fields[pair.substr(0, eq)] = pair.substr(eq + 1);
            }
            entities.push_back(entity);
        } else {
            fail("don't know \"" + key + "\"");
        }
    }
    while (!rows.empty() && rows.back().empty()) rows.pop_back();
    if (rows.empty()) throw std::runtime_error(name + ".txt has no tiles (they go after a line saying \"tiles\")");
    const size_t width = rows[0].size();
    for (const std::string& row : rows) {
        if (row.size() != width) throw std::runtime_error(name + ".txt: the rows of tiles aren't all as long");
    }
    room.grid.assign(rows.rbegin(), rows.rend());
    for (const Entity& entity : entities) {
        const std::string at = std::to_string(entity.x) + " " + std::to_string(entity.y);
        if (!room.inside(entity.x, entity.y)) throw std::runtime_error(name + ".txt: the entity at " + at + " is outside the room");
        if (!isEntityChar(room.at(entity.x, entity.y))) throw std::runtime_error(name + ".txt: the entity at " + at + " isn't on a thing's tile");
        room.fields[{entity.x, entity.y}] = entity.fields;
    }
    return room;
}

RoomFile RoomFile::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Can't open " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string name = path.substr(path.find_last_of("/\\") + 1);
    if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") name.resize(name.size() - 4);
    return parse(buffer.str(), name);
}

std::string RoomFile::write() const
{
    std::ostringstream out;
    out << "# Made with the room editor: ./build/Release/editor " << name << " (see tools/editor.cpp)\n";
    out << "title " << title << "\n";
    out << "theme " << theme << "\n";
    if (!region.empty()) out << "region " << region << "\n";
    if (wind != 0.f) {
        char number[32];
        std::snprintf(number, sizeof number, "%g", wind);
        out << "wind " << number << "\n";
    }
    if (dark) out << "dark 1\n";
    if (space) out << "space 1\n";
    for (const Join& join : joins) {
        out << "join " << join.side << ' ' << join.start << ' ' << join.room << ' ' << join.otherSide << ' ' << join.otherStart << "\n";
    }
    // Top to bottom, like the tiles.
    std::vector<std::pair<std::pair<int, int>, Fields>> sorted(fields.begin(), fields.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.first.second != b.first.second ? a.first.second > b.first.second : a.first.first < b.first.first;
    });
    for (const auto& [at, values] : sorted) {
        if (values.empty()) continue;
        out << "entity " << at.first << ' ' << at.second;
        for (const auto& [key, value] : values) out << ' ' << key << '=' << value;
        out << "\n";
    }
    out << "tiles\n";
    for (int y = height() - 1; y >= 0; --y) out << grid[static_cast<size_t>(y)] << "\n";
    return out.str();
}

void RoomFile::save(const std::string& path) const
{
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Can't write " + path);
    file << write();
    if (!file) throw std::runtime_error("Couldn't write all of " + path);
}

int RoomFile::width() const
{
    return grid.empty() ? 0 : static_cast<int>(grid[0].size());
}

int RoomFile::height() const
{
    return static_cast<int>(grid.size());
}

bool RoomFile::inside(int x, int y) const
{
    return x >= 0 && y >= 0 && x < width() && y < height();
}

char RoomFile::at(int x, int y) const
{
    return inside(x, y) ? grid[static_cast<size_t>(y)][static_cast<size_t>(x)] : '.';
}

void RoomFile::set(int x, int y, char c)
{
    if (!inside(x, y)) return;
    grid[static_cast<size_t>(y)][static_cast<size_t>(x)] = c;
    if (!isEntityChar(c)) fields.erase({x, y});
}

void RoomFile::grow(int left, int right, int bottom, int top)
{
    const int oldWidth = width(), oldHeight = height();
    const int newWidth = std::max(MIN_SIZE, oldWidth + left + right), newHeight = std::max(MIN_SIZE, oldHeight + bottom + top);
    right = newWidth - oldWidth - left;
    top = newHeight - oldHeight - bottom;
    // Where each new tile was: the walls move out (or in) and what's inside stays put, -1 for a new
    // one. New tiles inside carry on from the nearest old ones inside, so floors and ceilings go on.
    auto oldX = [&](int x) {
        if (x == 0) return 0;
        if (x == newWidth - 1) return oldWidth - 1;
        const int ox = x - left;
        return ox >= 1 && ox <= oldWidth - 2 ? ox : -1;
    };
    auto oldY = [&](int y) {
        if (y == 0) return 0;
        if (y == newHeight - 1) return oldHeight - 1;
        const int oy = y - bottom;
        return oy >= 1 && oy <= oldHeight - 2 ? oy : -1;
    };
    std::vector<std::string> resized(static_cast<size_t>(newHeight), std::string(static_cast<size_t>(newWidth), '#'));
    std::map<std::pair<int, int>, Fields> moved;
    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            const int ox = oldX(x), oy = oldY(y);
            char c = '#';
            if (ox >= 0 && oy >= 0) {
                c = at(ox, oy);
                if (auto it = fields.find({ox, oy}); it != fields.end()) moved[{x, y}] = it->second;
            } else if (x > 0 && y > 0 && x < newWidth - 1 && y < newHeight - 1) {
                c = at(ox >= 0 ? ox : std::clamp(x - left, 1, oldWidth - 2), oy >= 0 ? oy : std::clamp(y - bottom, 1, oldHeight - 2));
                if (isEntityChar(c)) c = '.';
            }
            resized[static_cast<size_t>(y)][static_cast<size_t>(x)] = c;
        }
    }
    grid = std::move(resized);
    fields = std::move(moved);
    // The doors in the side walls moved up with the bottom, the ones in the top and bottom along with the left.
    for (Join& join : joins) join.start += join.side == 'L' || join.side == 'R' ? bottom : left;
}

void RoomFile::resize(int newWidth, int newHeight)
{
    grow(0, newWidth - width(), 0, newHeight - height());
}

std::vector<RoomFile::Door> RoomFile::doors() const
{
    std::vector<Door> found;
    auto scan = [&](char side, int length, auto tileAt) {
        // Not the corners: those are both sides' walls.
        int start = -1;
        for (int i = 1; i <= length - 1; ++i) {
            const bool open = i < length - 1 && !isSolidChar(tileAt(i));
            if (open && start < 0) start = i;
            if (!open && start >= 0) {
                found.push_back({side, start, i - start});
                start = -1;
            }
        }
    };
    const int w = width(), h = height();
    scan('L', h, [&](int y) { return at(0, y); });
    scan('R', h, [&](int y) { return at(w - 1, y); });
    scan('B', w, [&](int x) { return at(x, 0); });
    scan('T', w, [&](int x) { return at(x, h - 1); });
    return found;
}

LevelInfo RoomFile::toLevel(bool needsStart) const
{
    // The tiles go through the same code as a text level's; the entities are added with their
    // fields, as loadWorld adds the LDtk project's.
    std::vector<std::string> rows;
    for (int y = height() - 1; y >= 0; --y) {
        std::string row = grid[static_cast<size_t>(y)];
        std::replace_if(row.begin(), row.end(), isEntityChar, '.');
        rows.push_back(row);
    }
    auto level = Level::fromRows(rows);
    for (int y = 0; y < height(); ++y) {
        for (int x = 0; x < width(); ++x) {
            const char c = at(x, y);
            if (!isEntityChar(c)) continue;
            const auto it = fields.find({x, y});
            entities::add(*level, entities::identifierOf(c), {x, y}, it == fields.end() ? Fields{} : it->second);
        }
    }
    applyRoomSettings(*level, {{"wind", std::to_string(wind)}, {"dark", dark ? "1" : "0"}, {"space", space ? "1" : "0"}});
    if (needsStart && !level->hasStart()) throw std::runtime_error("Room " + name + " has no Start");
    LevelInfo info;
    info.name = name;
    info.title = title;
    info.theme = theme;
    info.region = region;
    info.level = level;
    return info;
}
