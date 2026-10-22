#include "Game.h"
#include "src/Config.h"
#include "src/Hud.h"
#include "src/utils.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
    constexpr float HALF_W = config::GAME_WIDTH * 0.5f;
    constexpr float HALF_H = config::GAME_HEIGHT * 0.5f;
    constexpr float CAMERA_FOLLOW = 8.f;    // higher = camera catches up faster
    constexpr float DEATH_TIME = 0.5f;      // how long you stay dead before respawning
    constexpr float SHAKE_TIME = 0.3f;
    constexpr float SHAKE_PIXELS = 3.f;
    constexpr float TITLE_TIME = 3.f;
    constexpr float MESSAGE_TIME = 4.f;
    constexpr float TRANSITION_TIME = 0.5f; // panning from one room to the next
    constexpr float FALL_OUT_MARGIN = 64.f; // how far below the level counts as falling out
    const sf::FloatRect MAP_AREA(8.f, 20.f, 304.f, 138.f);
    constexpr float MAP_PAN_SPEED = 120.f;  // screen pixels per second, with the arrow keys
    const char* const TITLE = "Celeste-like Game";

    const sf::Color WHITE(235, 235, 245);
    const sf::Color GOLD(255, 210, 60);
    const sf::Color RED(255, 90, 90);
    const sf::Color GREY(150, 150, 170);
    const sf::Color CYAN(110, 220, 240);
    constexpr float GEM_COUNTER_TIME = 3.f;
    const std::vector<sf::Color> CONFETTI = {GOLD, sf::Color(255, 120, 160), sf::Color(120, 220, 255), sf::Color(160, 240, 120)};

    // Menu item ids.
    enum Choice
    {
        CONTINUE, NEW_GAME, LEVELS, OPTIONS, QUIT,
        RESUME, RETRY, MAIN_MENU, PLAY_AGAIN, BACK, MAP,
        WINDOW_SIZE, FULLSCREEN, SCREEN_SHAKE, TIMER, SKIN,
        FIRST_LEVEL = 1000, // + level index
    };

    void drawRect(sf::RenderTarget& target, sf::Vector2f pos, sf::Vector2f size, sf::Color color)
    {
        sf::RectangleShape rect(size);
        rect.setPosition(pos);
        rect.setFillColor(color);
        target.draw(rect);
    }

    void dimScreen(sf::RenderTarget& target, sf::Uint8 alpha)
    {
        drawRect(target, {0.f, 0.f}, {float(config::GAME_WIDTH), float(config::GAME_HEIGHT)}, sf::Color(0, 0, 0, alpha));
    }

    // A view that only draws inside `area` of the screen, leaving the coordinates as they are.
    sf::View clipView(sf::FloatRect area)
    {
        const sf::Vector2f screen(config::GAME_WIDTH, config::GAME_HEIGHT);
        sf::View view(area);
        view.setViewport({area.left / screen.x, area.top / screen.y, area.width / screen.x, area.height / screen.y});
        return view;
    }

    sf::Color withAlpha(sf::Color color, sf::Uint8 alpha)
    {
        color.a = alpha;
        return color;
    }

    // The HUD font draws a lowercase 'x' as a cross, so titles are shown in capitals.
    std::string upper(std::string text)
    {
        for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return text;
    }

    // What picking each ability up tells you.
    struct AbilityInfo
    {
        const char* id;
        const char* name;
        const char* text;
    };
    const AbilityInfo ABILITIES[] = {
        {"climbing_pick", "CLIMBING PICK", "YOUR ROPE STICKS TO ROCK NOW - NOT JUST METAL"},
    };

    MenuItem button(int id, std::string label)
    {
        return {id, std::move(label), "", false};
    }

    std::string onOff(bool on)
    {
        return on ? "ON" : "OFF";
    }

    float easeInOut(float t)
    {
        t = std::clamp(t, 0.f, 1.f);
        return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) * 0.5f;
    }
}

std::vector<LevelInfo> loadLevelList(const std::string& listPath)
{
    std::ifstream file(listPath);
    if (!file) throw std::runtime_error("Can't open level list: " + listPath);
    const std::string folder = listPath.substr(0, listPath.find_last_of("/\\") + 1);

    std::vector<LevelInfo> levels;
    std::string theme = Theme::DEFAULT;
    for (std::string line; std::getline(file, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream words(line);
        LevelInfo info;
        words >> info.path;
        if (info.path == "theme") {
            words >> theme;
            continue;
        }
        info.theme = theme;
        if (!(words >> info.position.x >> info.position.y)) {
            throw std::runtime_error("No map position for " + info.path + " in " + listPath);
        }
        std::getline(words >> std::ws, info.title);
        info.path = folder + info.path;
        levels.push_back(info);
    }
    if (levels.empty()) throw std::runtime_error("No levels in " + listPath);
    return levels;
}

Game::Game(std::vector<LevelInfo> levelList, bool testing)
    : levels(std::move(levelList)),
      testing(testing),
      savePath(SaveData::defaultPath()),
      save(SaveData::load(savePath)),
      window(std::make_shared<sf::RenderWindow>()),
      input(window)
{
    applyWindowSettings();
    canvas.create(config::GAME_WIDTH, config::GAME_HEIGHT);
    canvas.setSmooth(false);
    for (const auto& info : levels) {
        rooms.push_back(Level::loadFromFile(info.path));
        rooms.back()->setTheme(Theme::get(info.theme));
    }
    std::vector<WorldMap::Room> mapRooms;
    for (size_t ii = 0; ii < rooms.size(); ++ii) mapRooms.push_back({rooms[ii], levels[ii].position});
    worldMap = std::make_unique<WorldMap>(std::move(mapRooms));

    if (testing) {
        startGame(0, Start::From);
    } else {
        // The level you'd continue from drifts by behind the title screen.
        loadLevel(continueIndex().value_or(0));
        openScreen(Screen::Title);
    }
}

// ---------------------------------------------------------------------------------------------
// Screens and menus

void Game::openScreen(Screen next)
{
    if (next == Screen::LevelSelect || next == Screen::Options) {
        if (screen == Screen::Title || screen == Screen::Paused) returnScreen = screen;
    }
    if (next == Screen::Playing && (screen == Screen::Paused || screen == Screen::Map)) ignorePressesThisFrame = true;
    screen = next;

    switch (next) {
        case Screen::Title: menu.top = 92.f; menu.maxVisible = 9; break;
        case Screen::LevelSelect: menu.top = 36.f; menu.maxVisible = 12; break;
        case Screen::Options: menu.top = 60.f; menu.maxVisible = 9; break;
        case Screen::Paused: menu.top = 78.f; menu.maxVisible = 9; break;
        case Screen::Complete: menu.top = 132.f; menu.maxVisible = 9; break;
        case Screen::Playing:
        case Screen::Map: break;
    }
    refreshMenu();
    menu.select(next == Screen::LevelSelect ? levelIndex : 0);
}

void Game::refreshMenu()
{
    std::vector<MenuItem> items;
    switch (screen) {
        case Screen::Title:
            if (auto cont = continueIndex()) {
                items.push_back(button(CONTINUE, "CONTINUE - LEVEL " + std::to_string(*cont + 1)));
                items.push_back(button(NEW_GAME, "NEW GAME"));
            } else {
                items.push_back(button(NEW_GAME, "PLAY"));
            }
            items.push_back(button(LEVELS, "LEVELS"));
            items.push_back(button(OPTIONS, "OPTIONS"));
            items.push_back(button(QUIT, "QUIT"));
            break;
        case Screen::LevelSelect:
            for (size_t ii = 0; ii < levels.size(); ++ii) {
                std::string number = std::to_string(ii + 1);
                if (number.size() < 2) number = " " + number;
                const LevelRecord* record = testing ? nullptr : save.record(levelKey(ii));
                std::string best = record ? hud::formatTime(record->bestTime) + "  x" + std::to_string(record->fewestDeaths) : "-";
                if (!gemTally(ii).empty()) best += "   " + gemTally(ii);
                items.push_back({FIRST_LEVEL + static_cast<int>(ii), number + "  " + upper(levels[ii].title), best, false});
            }
            items.push_back({BACK, "BACK", " ", false});
            break;
        case Screen::Options: {
            const Settings& s = save.settings;
            items.push_back({WINDOW_SIZE, "WINDOW SIZE", std::to_string(std::min(s.windowScale, maxWindowScale())) + "X", true});
            items.push_back({FULLSCREEN, "FULLSCREEN", onOff(s.fullscreen), true});
            items.push_back({SCREEN_SHAKE, "SCREEN SHAKE", onOff(s.screenShake), true});
            items.push_back({TIMER, "TIMER", onOff(s.showTimer), true});
            items.push_back({SKIN, "SKIN", upper(Player::SKINS[skinIndex()]), true});
            items.push_back({BACK, "BACK", " ", false});
            break;
        }
        case Screen::Paused:
            items.push_back(button(RESUME, "RESUME"));
            items.push_back(button(MAP, "MAP"));
            items.push_back(button(RETRY, "RETRY"));
            items.push_back(button(LEVELS, "LEVELS"));
            items.push_back(button(OPTIONS, "OPTIONS"));
            items.push_back(button(MAIN_MENU, "MAIN MENU"));
            break;
        case Screen::Complete:
            items.push_back(button(PLAY_AGAIN, "PLAY AGAIN"));
            items.push_back(button(LEVELS, "LEVELS"));
            items.push_back(button(MAIN_MENU, "MAIN MENU"));
            break;
        case Screen::Playing:
        case Screen::Map:
            break;
    }
    menu.setItems(std::move(items));
}

void Game::onMenuChoice(MenuChoice choice)
{
    switch (choice.id) {
        case CONTINUE: startGame(continueIndex().value_or(0), Start::Continue); break;
        case NEW_GAME: startGame(0, Start::New); break;
        case LEVELS: openScreen(Screen::LevelSelect); break;
        case OPTIONS: openScreen(Screen::Options); break;
        case QUIT: window->close(); break;
        case RESUME: openScreen(Screen::Playing); break;
        case MAP: openMap(); break;
        case RETRY:
            if (deathTimer <= 0.f && !transition) killPlayer();
            openScreen(Screen::Playing);
            break;
        case MAIN_MENU: openScreen(Screen::Title); break;
        case PLAY_AGAIN: startGame(firstLevelPlayed, Start::From); break;
        case BACK: back(); break;
        case WINDOW_SIZE:
        case FULLSCREEN:
        case SCREEN_SHAKE:
        case TIMER:
        case SKIN:
            // Clicking or pressing Enter on a setting steps it forwards.
            changeSetting(choice.id, choice.direction == 0 ? 1 : choice.direction);
            break;
        default:
            if (choice.id >= FIRST_LEVEL) startGame(static_cast<size_t>(choice.id - FIRST_LEVEL), Start::From);
            break;
    }
}

void Game::back()
{
    switch (screen) {
        case Screen::LevelSelect:
        case Screen::Options: openScreen(returnScreen); break;
        case Screen::Paused: openScreen(Screen::Playing); break;
        case Screen::Complete: openScreen(Screen::Title); break;
        case Screen::Map: openScreen(mapReturn); break;
        case Screen::Title:
        case Screen::Playing: break;
    }
}

void Game::changeSetting(int id, int direction)
{
    Settings& s = save.settings;
    switch (id) {
        case WINDOW_SIZE: {
            const int max = maxWindowScale();
            const int scale = std::clamp(s.windowScale, 1, max);
            s.windowScale = (scale - 1 + direction + max) % max + 1;
            if (!s.fullscreen) reapplyWindow = true;
            break;
        }
        case FULLSCREEN:
            s.fullscreen = !s.fullscreen;
            reapplyWindow = true;
            break;
        case SCREEN_SHAKE: s.screenShake = !s.screenShake; break;
        case TIMER: s.showTimer = !s.showTimer; break;
        case SKIN: {
            const int count = static_cast<int>(Player::SKINS.size());
            const size_t next = static_cast<size_t>((static_cast<int>(skinIndex()) + direction + count) % count);
            s.skin = Player::SKINS[next];
            player->setSkin(next);
            break;
        }
        default: break;
    }
    writeSave();
    refreshMenu();
}

void Game::applyWindowSettings()
{
    const Settings& s = save.settings;
    if (s.fullscreen) {
        window->create(sf::VideoMode::getDesktopMode(), TITLE, sf::Style::Fullscreen);
    } else {
        const unsigned scale = static_cast<unsigned>(std::clamp(s.windowScale, 1, maxWindowScale()));
        window->create(sf::VideoMode(config::GAME_WIDTH * scale, config::GAME_HEIGHT * scale), TITLE, sf::Style::Close);
    }
    window->setFramerateLimit(60);
    window->setMouseCursorVisible(false); // we draw our own crosshair
    window->setKeyRepeatEnabled(false);   // a held Esc shouldn't flicker the pause menu
}

int Game::maxWindowScale() const
{
    // Leave some room for the title bar and the dock.
    const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    const unsigned width = desktop.width * 9 / 10, height = desktop.height * 9 / 10;
    const int fits = static_cast<int>(std::min(width / config::GAME_WIDTH, height / config::GAME_HEIGHT));
    return std::max(1, fits);
}

bool Game::inGame() const
{
    switch (screen) {
        case Screen::Playing:
        case Screen::Paused:
        case Screen::Complete:
        case Screen::Map: return true;
        case Screen::LevelSelect:
        case Screen::Options: return returnScreen == Screen::Paused;
        case Screen::Title: return false;
    }
    return false;
}

std::optional<size_t> Game::continueIndex() const
{
    if (testing || save.continueLevel.empty()) return std::nullopt;
    for (size_t ii = 0; ii < levels.size(); ++ii) {
        if (levelKey(ii) == save.continueLevel) return ii;
    }
    return std::nullopt;
}

size_t Game::skinIndex() const
{
    for (size_t ii = 0; ii < Player::SKINS.size(); ++ii) {
        if (save.settings.skin == Player::SKINS[ii]) return ii;
    }
    return 0;
}

std::string Game::levelKey(size_t index) const
{
    return std::filesystem::path(levels[index].path).filename().string();
}

void Game::writeSave()
{
    save.save(savePath);
}

// ---------------------------------------------------------------------------------------------
// Playing

void Game::startGame(size_t index, Start how)
{
    totalTime = 0.f;
    totalDeaths = 0;
    firstLevelPlayed = index;
    lastResult.clear();
    messageTimer = 0.f;
    // Testing a level leaves the save's progress alone (and has every ability anyway).
    if (!testing) {
        if (how == Start::New) {
            save.explored.clear();
            save.gems.clear();
        }
        if (how != Start::Continue) save.abilities.clear();
        // Anything lying in the rooms before this one, so a room is never started without what it
        // needs. (On top of the save's, when continuing: older saves didn't keep abilities.)
        for (size_t ii = 0; ii < index; ++ii) {
            for (const Pickup& pickup : rooms[ii]->pickups()) save.abilities.insert(pickup.ability);
        }
    }
    gems = testing ? std::set<std::string>() : save.gems;
    loadLevel(index);
    visited.assign(levels.size(), false);
    visited[index] = true;
    if (!testing) {
        save.explored.insert(levelKey(index));
        save.continueLevel = levelKey(index);
        writeSave();
    }
    openScreen(Screen::Playing);
}

void Game::loadLevel(size_t index)
{
    levelIndex = index;
    level = rooms[index];
    player = std::make_unique<Player>(level);
    player->setSkin(skinIndex());
    applyAbilities();
    spawnAtStart();
    titleTimer = TITLE_TIME;
}

void Game::spawnAtStart()
{
    checkpoint = level->getStartPos();
    needsSpawnPoint = false;
    player->respawn(checkpoint);
    particles.clear();
    transition.reset();
    runTime = 0.f;
    deaths = 0;
    deathTimer = 0.f;
    deathFlash = 0.f;
    shake = 0.f;
    ignoreInputThisFrame = true;
    updateCamera(0.f, true);
}

void Game::enterRoom(size_t index, Level::Side side)
{
    totalTime += runTime;
    totalDeaths += deaths;
    // Going on to the next level in the list finishes this one, so its time counts. Going back doesn't.
    lastResult.clear();
    if (index == levelIndex + 1) recordResult();

    // The camera pans across from where it was in the old room.
    const sf::Vector2f shift = roomOrigin(levelIndex) - roomOrigin(index);
    transition = Transition{levelIndex, drawCamera() + shift, checkpoint};
    levelIndex = index;
    level = rooms[index];
    player->enterLevel(level, shift, side == Level::Side::Top);
    particles.clear();

    // Until the player lands somewhere in the new room, dying puts them back where they came in.
    checkpoint = player->getPosition();
    needsSpawnPoint = true;
    runTime = 0.f;
    deaths = 0;
    updateCamera(0.f, true);
    // The title (and the result of the room before) only shows the first time in.
    if (visited[index]) {
        lastResult.clear();
    } else {
        visited[index] = true;
        titleTimer = TITLE_TIME;
    }

    if (!testing) {
        save.explored.insert(levelKey(index));
        save.continueLevel = levelKey(index);
    }
    writeSave();
}

void Game::recordResult()
{
    if (testing) return;
    const LevelRecord* old = save.record(levelKey(levelIndex));
    const bool best = old && (runTime < old->bestTime || deaths < old->fewestDeaths);
    lastResult = hud::formatTime(runTime) + "  x" + std::to_string(deaths) + (best ? "  NEW BEST!" : "");
    save.addResult(levelKey(levelIndex), runTime, deaths);
}

void Game::finishGame()
{
    totalTime += runTime;
    totalDeaths += deaths;
    recordResult();
    if (!testing) save.continueLevel.clear();
    writeSave();
    particles.clear();
    openScreen(Screen::Complete);
}

std::optional<size_t> Game::roomAt(sf::Vector2f point) const
{
    const auto room = worldMap->roomAt((point + roomOrigin(levelIndex)) / Level::TILE_SIZE);
    if (room == levelIndex) return std::nullopt;
    return room;
}

sf::Vector2f Game::roomOrigin(size_t index) const
{
    return sf::Vector2f(levels[index].position) * Level::TILE_SIZE;
}

bool Game::hasAbility(const std::string& ability) const
{
    // A level played on its own from the command line has everything.
    return testing || save.abilities.count(ability) > 0;
}

void Game::applyAbilities()
{
    player->setCanHookRock(hasAbility("climbing_pick"));
}

void Game::collect(const Pickup& pickup)
{
    save.abilities.insert(pickup.ability);
    writeSave();
    applyAbilities();
    // Where it was is a good place to come back to.
    checkpoint = pickup.position;
    needsSpawnPoint = false;
    particles.spawn(pickup.position + sf::Vector2f(4.f, 8.f), {.count = 40, .speed = 70.f, .life = 0.9f, .gravity = 40.f, .size = 2.f,
                                                                .colors = {GOLD, sf::Color(255, 246, 200), sf::Color(232, 238, 248)}});
    for (const AbilityInfo& info : ABILITIES) {
        if (pickup.ability == info.id) showMessage(info.name, info.text);
    }
}

void Game::showMessage(std::string title, std::string text)
{
    messageTitle = std::move(title);
    messageText = std::move(text);
    messageTimer = MESSAGE_TIME;
    titleTimer = 0.f;
}

std::string Game::gemKey(size_t room, size_t gem) const
{
    const sf::Vector2f tile = rooms[room]->gems()[gem] / Level::TILE_SIZE;
    return levelKey(room) + " " + std::to_string(static_cast<int>(tile.x)) + " " + std::to_string(static_cast<int>(tile.y));
}

bool Game::gemFound(size_t room, size_t gem) const
{
    return gems.count(gemKey(room, gem)) > 0;
}

int Game::gemCount() const
{
    int count = 0;
    for (const auto& room : rooms) count += static_cast<int>(room->gems().size());
    return count;
}

std::string Game::gemTotal() const
{
    int found = 0;
    for (size_t room = 0; room < rooms.size(); ++room) {
        for (size_t gem = 0; gem < rooms[room]->gems().size(); ++gem) found += gemFound(room, gem) ? 1 : 0;
    }
    return "GEMS " + std::to_string(found) + "/" + std::to_string(gemCount());
}

std::string Game::gemTally(size_t room) const
{
    const size_t count = rooms[room]->gems().size();
    if (count == 0) return "";
    size_t found = 0;
    for (size_t gem = 0; gem < count; ++gem) found += gemFound(room, gem) ? 1 : 0;
    return std::to_string(found) + "/" + std::to_string(count);
}

// ---------------------------------------------------------------------------------------------
// Map

void Game::openMap()
{
    worldMap->reveal(exploredRooms());
    mapReturn = screen;
    // As close as still shows everything explored, if anything does. Otherwise all the way out,
    // around the player.
    const sf::FloatRect shown = worldMap->shownBounds();
    mapZoom = WorldMap::ZOOMS.back();
    mapCentre = playerTile();
    for (int zoom : WorldMap::ZOOMS) {
        if (shown.width / zoom <= MAP_AREA.width && shown.height / zoom <= MAP_AREA.height) {
            mapZoom = zoom;
            mapCentre = {shown.left + shown.width * 0.5f, shown.top + shown.height * 0.5f};
            break;
        }
    }
    openScreen(Screen::Map);
}

void Game::updateMap(float dt, sf::Vector2f mouseMove)
{
    using K = sf::Keyboard;
    auto held = [this](K::Key a, K::Key b) { return input.isKeyPressed(a) || input.isKeyPressed(b); };
    const sf::Vector2f keys((held(K::D, K::Right) ? 1.f : 0.f) - (held(K::A, K::Left) ? 1.f : 0.f),
                            (held(K::W, K::Up) ? 1.f : 0.f) - (held(K::S, K::Down) ? 1.f : 0.f));
    mapCentre += keys * MAP_PAN_SPEED * dt * float(mapZoom);
    // Dragging with the mouse moves the map with it. Screen y points down, the map's up.
    if (input.isMousePressed() && !input.wasMouseClicked()) mapCentre += sf::Vector2f(-mouseMove.x, mouseMove.y) * float(mapZoom);
    if (input.wheelDelta() != 0.f) zoomMap(input.wheelDelta() > 0.f ? 1 : -1);

    const sf::FloatRect shown = worldMap->shownBounds();
    mapCentre.x = std::clamp(mapCentre.x, shown.left, shown.left + shown.width);
    mapCentre.y = std::clamp(mapCentre.y, shown.top, shown.top + shown.height);
}

void Game::zoomMap(int steps)
{
    const auto& zooms = WorldMap::ZOOMS;
    const int current = static_cast<int>(std::find(zooms.begin(), zooms.end(), mapZoom) - zooms.begin());
    mapZoom = zooms[static_cast<size_t>(std::clamp(current - steps, 0, static_cast<int>(zooms.size()) - 1))];
}

std::vector<bool> Game::exploredRooms() const
{
    // Ever, from the save, and in this game (which is all there is when testing a level).
    std::vector<bool> explored(levels.size());
    for (size_t ii = 0; ii < levels.size(); ++ii) {
        explored[ii] = ii == levelIndex || (ii < visited.size() && visited[ii]) || save.explored.count(levelKey(ii)) > 0;
    }
    return explored;
}

sf::Vector2f Game::playerTile() const
{
    return (player->centre() + roomOrigin(levelIndex)) / Level::TILE_SIZE;
}

WorldMap::View Game::mapView() const
{
    return {MAP_AREA, {std::round(mapCentre.x), std::round(mapCentre.y)}, mapZoom};
}

void Game::run()
{
    sf::Clock clock;
    float accumulator = 0.f;

    while (window->isOpen()) {
        input.newFrame();
        sf::Event event;
        while (window->pollEvent(event)) {
            input.handleEvent(event);
            if (event.type == sf::Event::Closed) window->close();
            if (event.type == sf::Event::LostFocus && screen == Screen::Playing) openScreen(Screen::Paused);
            if (event.type == sf::Event::KeyPressed) handleKey(event.key.code);
        }
        if (reapplyWindow) {
            reapplyWindow = false;
            applyWindowSettings();
        }

        // Cap the frame time so a hitch (e.g. dragging the window) doesn't fast-forward the game.
        float frameTime = std::min(clock.restart().asSeconds(), 0.25f);

        const sf::Vector2f mouse = input.mousePosition();
        const sf::Vector2f mouseMove = mouse - lastMouse;
        const bool mouseMoved = mouse != lastMouse;
        lastMouse = mouse;
        if (screen == Screen::Playing) {
            if (input.wasMouseClicked() && pauseButton().contains(mouse)) openScreen(Screen::Paused);
        } else if (screen == Screen::Map) {
            updateMap(frameTime, mouseMove);
        } else if (auto choice = menu.handleMouse(mouse, mouseMoved, input.wasMouseClicked())) {
            onMenuChoice(*choice);
        }
        if (!window->isOpen()) break;

        // Menus leave the player alone, so a rope held when pausing is still held after.
        if (screen == Screen::Playing) {
            PlayerInput in;
            if (!transition && deathTimer <= 0.f && !ignoreInputThisFrame) {
                in = readInput();
                if (ignorePressesThisFrame) in.jumpPressed = in.ropePressed = false;
            }
            player->handleInput(in);
        }
        ignoreInputThisFrame = false;
        ignorePressesThisFrame = false;

        accumulator += frameTime;
        while (accumulator >= config::FIXED_DT) {
            step(config::FIXED_DT);
            accumulator -= config::FIXED_DT;
        }

        render();
    }
}

void Game::handleKey(sf::Keyboard::Key key)
{
    using K = sf::Keyboard;
    if (screen == Screen::Playing) {
        if (key == K::Escape || key == K::P) openScreen(Screen::Paused);
        else if (key == K::M || key == K::Tab) openMap();
        else if (key == K::R && !transition && deathTimer <= 0.f) killPlayer(); // back to the checkpoint
        return;
    }
    if (screen == Screen::Map) {
        if (key == K::Escape || key == K::M || key == K::Tab) back();
        else if (key == K::Z) zoomMap(mapZoom == WorldMap::ZOOMS.back() ? -int(WorldMap::ZOOMS.size()) : -1); // further out, then all the way in
        return;
    }
    if (key == K::Escape || (key == K::P && screen == Screen::Paused)) {
        back();
        return;
    }
    if (auto choice = menu.handleKey(key)) onMenuChoice(*choice);
}

PlayerInput Game::readInput() const
{
    using K = sf::Keyboard;
    auto held = [this](K::Key a, K::Key b) { return input.isKeyPressed(a) || input.isKeyPressed(b); };

    PlayerInput in;
    in.moveX = (held(K::D, K::Right) ? 1.f : 0.f) - (held(K::A, K::Left) ? 1.f : 0.f);
    in.moveY = (held(K::W, K::Up) ? 1.f : 0.f) - (held(K::S, K::Down) ? 1.f : 0.f);
    in.jumpPressed = input.wasKeyJustPressed(K::Space);
    in.jumpHeld = input.isKeyPressed(K::Space);
    // Shift works like the mouse button, for trackpads that can't hold a click.
    in.ropePressed = input.wasMouseClicked() || input.wasKeyJustPressed(K::LShift) || input.wasKeyJustPressed(K::RShift);
    in.ropeHeld = input.isMousePressed() || held(K::LShift, K::RShift);
    in.aim = worldCoordinates(input.mousePosition(), drawCamera());
    return in;
}

sf::FloatRect Game::pauseButton() const
{
    return {config::GAME_WIDTH - 13.f, 1.f, 12.f, 11.f};
}

void Game::step(float dt)
{
    clockTime += dt;
    if (screen == Screen::Complete) {
        // Confetti rains down while the results are up, about 40 bits a second.
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        if (unit(rng) < dt * 40.f) {
            const sf::Vector2f top = worldCoordinates({unit(rng) * config::GAME_WIDTH, -2.f}, drawCamera());
            particles.spawn(top, {.count = 1, .speed = 20.f, .spread = 1.f, .direction = -1.5708f, .life = 3.5f, .gravity = 500.f, .size = 2.f,
                                  .colors = CONFETTI});
        }
        particles.update(dt);
    }
    if (screen != Screen::Playing) return;

    deathFlash = std::max(0.f, deathFlash - dt);
    shake = std::max(0.f, shake - dt);
    titleTimer = std::max(0.f, titleTimer - dt);
    messageTimer = std::max(0.f, messageTimer - dt);
    gemCounterTimer = std::max(0.f, gemCounterTimer - dt);
    particles.update(dt);

    if (transition) {
        transition->time += dt;
        if (transition->time >= TRANSITION_TIME) transition.reset();
        return;
    }
    runTime += dt;

    if (deathTimer > 0.f) {
        deathTimer -= dt;
        if (deathTimer <= 0.f) {
            deathTimer = 0.f;
            player->respawn(checkpoint);
            particles.spawn(player->centre(), {.count = 12, .speed = 30.f, .life = 0.4f, .gravity = 0.f,
                                               .colors = {sf::Color(255, 255, 255, 200), sf::Color(200, 220, 255, 200)}});
        }
        updateCamera(dt);
        return;
    }

    player->update(dt);
    showEffects(player->takeEvents());

    const bool fellOut = player->getPosition().y < level->bounds().top - FALL_OUT_MARGIN;
    if (player->isTouchingDanger() || fellOut) {
        killPlayer();
        updateCamera(dt);
        return;
    }

    // Walking off the edge of the room, like in Celeste, takes you into the room on the other side.
    if (auto side = level->exitThrough(player->centre())) {
        if (auto next = roomAt(player->centre())) {
            enterRoom(*next, *side);
            return;
        }
        // An opening to nowhere is the end of the game. Except at the bottom: that's a pit.
        if (*side != Level::Side::Bottom) {
            finishGame();
            return;
        }
    }
    if (level->isGoal(player->getPosition(), Player::HEIGHT, Player::WIDTH)) {
        finishGame();
        return;
    }

    if (needsSpawnPoint && player->isGrounded()) {
        checkpoint = player->getPosition();
        needsSpawnPoint = false;
    }
    if (const Pickup* pickup = level->pickupAt(player->getPosition(), Player::HEIGHT, Player::WIDTH); pickup && !hasAbility(pickup->ability)) {
        collect(*pickup);
    }
    if (auto gem = level->gemAt(player->getPosition(), Player::HEIGHT, Player::WIDTH); gem && !gemFound(levelIndex, *gem)) {
        gems.insert(gemKey(levelIndex, *gem));
        if (!testing) {
            save.gems.insert(gemKey(levelIndex, *gem));
            writeSave();
        }
        gemCounterTimer = GEM_COUNTER_TIME;
        particles.spawn(level->gems()[*gem] + sf::Vector2f(4.f, 4.f), {.count = 20, .speed = 55.f, .life = 0.6f, .gravity = 60.f,
                                                                       .colors = {CYAN, sf::Color(234, 255, 255), sf::Color(42, 143, 176)}});
    }
    if (auto cp = level->checkpointAt(player->getPosition(), Player::HEIGHT, Player::WIDTH); cp && *cp != checkpoint) {
        checkpoint = *cp;
        needsSpawnPoint = false;
        particles.spawn(checkpoint + sf::Vector2f(4.f, 16.f), {.count = 16, .speed = 50.f, .spread = 2.f, .life = 0.6f, .gravity = 60.f,
                                                              .colors = {sf::Color(126, 240, 142), sf::Color(220, 255, 220)}});
    }

    updateCamera(dt);
}

void Game::killPlayer()
{
    ++deaths;
    deathTimer = DEATH_TIME;
    deathFlash = DEATH_TIME;
    shake = SHAKE_TIME;
    particles.spawn(player->centre(), {.count = 36, .speed = 100.f, .life = 0.7f, .gravity = 150.f, .size = 2.f,
                                       .colors = {sf::Color(251, 242, 54), sf::Color(153, 229, 80), sf::Color(215, 123, 186)}});
}

void Game::showEffects(const std::vector<PlayerEvent>& events)
{
    const std::vector<sf::Color> dust = {sf::Color(210, 200, 185, 220), sf::Color(160, 150, 140, 200)};
    const std::vector<sf::Color> sparks = {sf::Color(255, 255, 255), sf::Color(255, 230, 140)};
    for (const auto& e : events) {
        switch (e.type) {
            case PlayerEvent::Type::Jumped:
                particles.spawn(e.position, {.count = 6, .speed = 30.f, .spread = 2.5f, .life = 0.35f, .gravity = 20.f, .colors = dust});
                break;
            case PlayerEvent::Type::Landed: {
                int count = 3 + static_cast<int>(e.strength / 60.f);
                float speed = 20.f + e.strength * 0.1f;
                particles.spawn(e.position, {.count = count, .speed = speed, .spread = 0.6f, .direction = 0.f, .life = 0.4f, .gravity = 10.f, .colors = dust});
                particles.spawn(e.position, {.count = count, .speed = speed, .spread = 0.6f, .direction = 3.1416f, .life = 0.4f, .gravity = 10.f, .colors = dust});
                break;
            }
            case PlayerEvent::Type::Stepped:
                particles.spawn(e.position, {.count = 2, .speed = 12.f, .spread = 1.5f, .life = 0.3f, .gravity = 10.f, .colors = dust});
                break;
            case PlayerEvent::Type::RopeAttached:
                particles.spawn(e.position, {.count = 8, .speed = 50.f, .life = 0.25f, .gravity = 100.f, .colors = sparks});
                break;
            case PlayerEvent::Type::RopeWrapped:
                particles.spawn(e.position, {.count = 4, .speed = 30.f, .life = 0.2f, .gravity = 100.f, .colors = sparks});
                break;
            case PlayerEvent::Type::RopeBounced:
                particles.spawn(e.position, {.count = 5, .speed = 40.f, .life = 0.25f, .gravity = 200.f, .colors = dust});
                // The first time, say why.
                if (!shownRockHint) {
                    shownRockHint = true;
                    showMessage("CLINK!", "THE ROPE ONLY STICKS TO METAL BLOCKS");
                }
                break;
        }
    }
}

void Game::updateCamera(float dt, bool snap)
{
    sf::Vector2f target = player->centre();
    if (snap) camera = target;
    else camera += (target - camera) * (1.f - std::exp(-CAMERA_FOLLOW * dt));
    camera = clampToLevel(camera);
}

sf::Vector2f Game::clampToLevel(sf::Vector2f cam) const
{
    // Never show what's outside the level, unless the level is smaller than the screen.
    const sf::FloatRect b = level->bounds();
    if (b.width > 2.f * HALF_W) cam.x = std::clamp(cam.x, b.left + HALF_W, b.left + b.width - HALF_W);
    else cam.x = b.left + b.width * 0.5f;
    if (b.height > 2.f * HALF_H) cam.y = std::clamp(cam.y, b.top + HALF_H, b.top + b.height - HALF_H);
    else cam.y = b.top + b.height * 0.5f;
    return cam;
}

sf::Vector2f Game::drawCamera() const
{
    // Rounded so tiles and player move in whole pixels together and nothing shimmers.
    return {std::round(camera.x), std::round(camera.y)};
}

sf::Vector2f Game::titleCamera() const
{
    // Drifts slowly back and forth along the level, at the height of the start.
    const sf::FloatRect b = level->bounds();
    const float range = std::max(0.f, b.width - 2.f * HALF_W);
    const float along = 0.5f - 0.5f * std::cos(clockTime * 0.06f);
    const sf::Vector2f cam = clampToLevel({b.left + HALF_W + range * along, level->getStartPos().y});
    return {std::round(cam.x), std::round(cam.y)};
}

// ---------------------------------------------------------------------------------------------
// Drawing

void Game::render()
{
    canvas.clear();
    if (!inGame()) {
        drawWorld(titleCamera(), false);
    } else if (transition) {
        drawTransition();
    } else {
        sf::Vector2f cam = drawCamera();
        if (shake > 0.f && save.settings.screenShake && screen == Screen::Playing) {
            std::uniform_real_distribution<float> offset(-1.f, 1.f);
            float amount = SHAKE_PIXELS * shake / SHAKE_TIME;
            cam += sf::Vector2f(std::round(offset(rng) * amount), std::round(offset(rng) * amount));
        }
        drawWorld(cam);
    }

    if (screen == Screen::Playing) {
        if (!transition && deathTimer <= 0.f) drawAim();
        drawHud();
    } else {
        if (screen == Screen::Paused) drawHud();
        drawMenuScreen();
        drawCrosshair(GREY);
    }
    canvas.display();

    window->clear();
    const sf::FloatRect area = canvasRect(window->getSize());
    sf::Sprite scaledSprite(canvas.getTexture());
    scaledSprite.setPosition(area.left, area.top);
    scaledSprite.setScale(area.width / config::GAME_WIDTH, area.height / config::GAME_HEIGHT);
    window->draw(scaledSprite);
    window->display();
}

void Game::drawWorld(sf::Vector2f cam, bool withPlayer)
{
    drawRooms(cam);
    if (withPlayer && deathTimer <= 0.f) player->draw(canvas, cam);
    if (screen != Screen::Complete) particles.draw(canvas, cam); // the confetti goes over the dimmed screen
}

void Game::drawRooms(sf::Vector2f cam)
{
    // Rooms narrower or lower than the screen show what's around them, and that's the rooms next
    // door, so a gap in the wall leads somewhere. Everything is drawn in the current room's
    // coordinates; another room is just offset by where it is on the map.
    const sf::Vector2f origin = roomOrigin(levelIndex);
    const sf::FloatRect screen(0.f, 0.f, config::GAME_WIDTH, config::GAME_HEIGHT);
    const float scroll = cam.x + origin.x;
    struct Visible
    {
        size_t index;
        sf::Vector2f cam; // in that room's coordinates
        sf::FloatRect area; // of the screen
    };
    std::vector<Visible> visible;
    for (size_t ii = 0; ii < rooms.size(); ++ii) {
        const sf::Vector2f roomCam = cam + origin - roomOrigin(ii);
        const sf::FloatRect b = rooms[ii]->bounds();
        sf::FloatRect area;
        if (screen.intersects({viewCoordinates({b.left, b.top + b.height}, roomCam), {b.width, b.height}}, area)) {
            visible.push_back({ii, roomCam, area});
        }
    }

    // Each room has its own scenery behind it, so going into another theme the new scenery comes
    // into view with the room while the parallax carries on across the seam.
    drawBackground(level->theme(), scroll);
    for (const Visible& v : visible) {
        if (&rooms[v.index]->theme() == &level->theme()) continue;
        canvas.setView(clipView(v.area));
        drawBackground(rooms[v.index]->theme(), scroll);
        canvas.setView(canvas.getDefaultView());
    }
    auto owned = [this](const std::string& ability) { return hasAbility(ability); };
    for (const Visible& v : visible) {
        std::optional<sf::Vector2f> flag;
        if (v.index == levelIndex) flag = checkpoint;
        else if (transition && v.index == transition->from) flag = transition->fromCheckpoint;
        rooms[v.index]->draw(canvas, v.cam);
        rooms[v.index]->drawMarkers(canvas, v.cam, flag, clockTime);
        rooms[v.index]->drawPickups(canvas, v.cam, clockTime, owned);
        rooms[v.index]->drawGems(canvas, v.cam, clockTime, [&](size_t gem) { return gemFound(v.index, gem); });
    }
}

void Game::drawTransition()
{
    // The camera pans from where it was to where it'll be in the new room.
    const Transition& t = *transition;
    const float p = easeInOut(t.time / TRANSITION_TIME);
    const sf::Vector2f to = drawCamera();
    const sf::Vector2f cam(std::round(t.fromCamera.x + (to.x - t.fromCamera.x) * p), std::round(t.fromCamera.y + (to.y - t.fromCamera.y) * p));
    drawRooms(cam);
    player->draw(canvas, cam);
}

void Game::drawBackground(const Theme& theme, float scrollX)
{
    // The sky, then two layers of scenery, each scrolling slower than the level the further back
    // it is, for depth.
    const float parallax[] = {0.05f, 0.15f, 0.35f};
    for (size_t i = 0; i < theme.layers.size(); ++i) {
        sf::Sprite sprite(theme.layers[i]);
        int offset = static_cast<int>(std::round(scrollX * parallax[i]));
        sprite.setTextureRect(sf::IntRect(offset, 0, config::GAME_WIDTH, config::GAME_HEIGHT));
        canvas.draw(sprite);
    }
}

void Game::drawAim()
{
    const sf::Vector2f mouse = input.mousePosition();
    if (pauseButton().contains(mouse)) {
        drawCrosshair(GREY);
        return;
    }
    const sf::Vector2f mouseWorld = worldCoordinates(mouse, drawCamera());

    // Show where the rope would stick, so you know before you click.
    bool canHook = false;
    if (!player->getRope() && player->canShootRope()) {
        if (auto target = player->ropeTarget(mouseWorld)) {
            canHook = true;
            sf::Vector2f p = viewCoordinates(*target, drawCamera());
            drawRect(canvas, {std::floor(p.x) - 1.f, std::floor(p.y) - 1.f}, {2.f, 2.f}, sf::Color(255, 255, 255, 200));
        } else if (auto bounce = player->ropeBounce(mouseWorld)) {
            // Rock in range, but no pick yet: a dim red dot where the rope would bounce off.
            sf::Vector2f p = viewCoordinates(*bounce, drawCamera());
            drawRect(canvas, {std::floor(p.x) - 1.f, std::floor(p.y) - 1.f}, {2.f, 2.f}, sf::Color(230, 90, 90, 150));
        }
    }
    drawCrosshair(canHook ? sf::Color(255, 255, 255) : sf::Color(150, 150, 170, 180));
}

void Game::drawCrosshair(sf::Color color)
{
    const sf::Vector2f mouse = input.mousePosition();
    const float mx = std::floor(mouse.x), my = std::floor(mouse.y);
    // With a dark shadow, so it shows up on the bright daylight sky too.
    for (auto [offset, c] : {std::pair{1.f, sf::Color(0, 0, 0, 140)}, std::pair{0.f, color}}) {
        drawRect(canvas, {mx - 3.f + offset, my + offset}, {2.f, 1.f}, c);
        drawRect(canvas, {mx + 2.f + offset, my + offset}, {2.f, 1.f}, c);
        drawRect(canvas, {mx + offset, my - 3.f + offset}, {1.f, 2.f}, c);
        drawRect(canvas, {mx + offset, my + 2.f + offset}, {1.f, 2.f}, c);
    }
}

void Game::drawHud()
{
    if (deathFlash > 0.f) {
        auto alpha = static_cast<sf::Uint8>(110.f * deathFlash / DEATH_TIME);
        drawRect(canvas, {0.f, 0.f}, {float(config::GAME_WIDTH), float(config::GAME_HEIGHT)}, sf::Color(200, 30, 40, alpha));
    }

    if (save.settings.showTimer) {
        hud::drawText(canvas, hud::formatTime(runTime), {4.f, 4.f}, WHITE);
        hud::drawText(canvas, "x" + std::to_string(deaths), {4.f, 11.f}, RED);
    }

    // How many gems you've found, for a moment after finding one, and while paused.
    if (gemCount() > 0 && (gemCounterTimer > 0.f || screen == Screen::Paused)) {
        const float alpha = screen == Screen::Paused ? 1.f : std::min({1.f, (GEM_COUNTER_TIME - gemCounterTimer) * 6.f, gemCounterTimer});
        hud::drawText(canvas, gemTotal(), {4.f, save.settings.showTimer ? 18.f : 4.f}, withAlpha(CYAN, static_cast<sf::Uint8>(255.f * alpha)));
    }

    // Pause button in the top right corner, with the level number next to it.
    const sf::FloatRect button = pauseButton();
    const bool hovered = screen == Screen::Playing && button.contains(input.mousePosition());
    const sf::Color barColor = hovered ? WHITE : GREY;
    for (float x : {button.left + 3.f, button.left + 7.f}) {
        drawRect(canvas, {x + 1.f, button.top + 4.f}, {2.f, 6.f}, sf::Color(0, 0, 0, 160));
        drawRect(canvas, {x, button.top + 3.f}, {2.f, 6.f}, barColor);
    }
    const std::string levelNumber = std::to_string(levelIndex + 1) + "/" + std::to_string(levels.size());
    hud::drawText(canvas, levelNumber, {button.left - 2.f - hud::textWidth(levelNumber), 4.f}, GREY);

    if (titleTimer > 0.f && messageTimer <= 0.f && screen == Screen::Playing) {
        // Fade in quickly, hold, fade out over the last second.
        float alpha = std::min({1.f, (TITLE_TIME - titleTimer) * 4.f, titleTimer});
        auto a = static_cast<sf::Uint8>(255.f * alpha);
        hud::drawCentred(canvas, "LEVEL " + std::to_string(levelIndex + 1), HALF_W, 30.f, withAlpha(GREY, a));
        hud::drawCentred(canvas, upper(levels[levelIndex].title), HALF_W, 40.f, withAlpha(WHITE, a), 2.f);
        if (!lastResult.empty()) hud::drawCentred(canvas, lastResult, HALF_W, 58.f, withAlpha(GREY, a));
    }
    if (messageTimer > 0.f && screen == Screen::Playing) {
        float alpha = std::min({1.f, (MESSAGE_TIME - messageTimer) * 4.f, messageTimer});
        auto a = static_cast<sf::Uint8>(255.f * alpha);
        hud::drawCentred(canvas, messageTitle, HALF_W, 34.f, withAlpha(GOLD, a), 2.f);
        hud::drawCentred(canvas, messageText, HALF_W, 52.f, withAlpha(WHITE, a));
    }
}

void Game::drawMenuScreen()
{
    switch (screen) {
        case Screen::Title:
            dimScreen(canvas, 110);
            hud::drawCentred(canvas, upper(TITLE), HALF_W, 36.f, GOLD, 3.f);
            hud::drawCentred(canvas, "SWING. CLIMB. DON'T LOOK DOWN.", HALF_W, 60.f, GREY);
            hud::drawCentred(canvas, "ARROWS / MOUSE - CHOOSE    ENTER - OK", HALF_W, 168.f, GREY);
            break;
        case Screen::LevelSelect:
            dimScreen(canvas, 170);
            hud::drawCentred(canvas, "LEVELS", HALF_W, 12.f, GOLD, 2.f);
            break;
        case Screen::Options:
            dimScreen(canvas, 170);
            hud::drawCentred(canvas, "OPTIONS", HALF_W, 24.f, GOLD, 2.f);
            player->drawPreview(canvas, {HALF_W, 158.f}, 2.f, clockTime);
            hud::drawCentred(canvas, "LEFT / RIGHT - CHANGE    ESC - BACK", HALF_W, 168.f, GREY);
            break;
        case Screen::Paused:
            dimScreen(canvas, 140);
            hud::drawCentred(canvas, "PAUSED", HALF_W, 36.f, WHITE, 3.f);
            hud::drawCentred(canvas, "LEVEL " + std::to_string(levelIndex + 1) + " - " + upper(levels[levelIndex].title), HALF_W, 60.f, GREY);
            break;
        case Screen::Complete: {
            dimScreen(canvas, 170);
            const size_t played = levels.size() - firstLevelPlayed;
            hud::drawCentred(canvas, levels.size() == 1 ? "LEVEL COMPLETE" : "YOU DID IT!", HALF_W, 26.f, GOLD, 3.f);
            const std::string what = firstLevelPlayed == 0
                ? "ALL " + std::to_string(levels.size()) + " LEVELS"
                : "LEVELS " + std::to_string(firstLevelPlayed + 1) + " TO " + std::to_string(levels.size());
            if (played > 1) hud::drawCentred(canvas, what, HALF_W, 52.f, WHITE);
            hud::drawCentred(canvas, hud::formatTime(totalTime), HALF_W, 70.f, WHITE, 4.f);
            hud::drawCentred(canvas, "x" + std::to_string(totalDeaths), HALF_W, 100.f, RED, 2.f);
            if (gemCount() > 0) hud::drawCentred(canvas, gemTotal(), HALF_W, 118.f, CYAN);
            particles.draw(canvas, drawCamera());
            break;
        }
        case Screen::Map:
            drawMap();
            break;
        case Screen::Playing:
            break;
    }
    menu.draw(canvas, clockTime);
}

void Game::drawMap()
{
    dimScreen(canvas, 215);
    hud::drawCentred(canvas, "MAP", HALF_W, 4.f, GOLD, 2.f);
    const WorldMap::View view = mapView();
    const sf::FloatRect& a = view.area;
    for (const sf::FloatRect& line : {sf::FloatRect(a.left - 1.f, a.top - 1.f, a.width + 2.f, 1.f), sf::FloatRect(a.left - 1.f, a.top + a.height, a.width + 2.f, 1.f),
                                      sf::FloatRect(a.left - 1.f, a.top, 1.f, a.height), sf::FloatRect(a.left + a.width, a.top, 1.f, a.height)}) {
        drawRect(canvas, {line.left, line.top}, {line.width, line.height}, withAlpha(GREY, 90));
    }
    worldMap->draw(canvas, view);

    const std::vector<bool> explored = exploredRooms();
    canvas.setView(clipView(a));
    // Things still to pick up in the rooms you've seen, glinting.
    for (size_t ii = 0; ii < rooms.size(); ++ii) {
        if (!explored[ii]) continue;
        for (const Pickup& pickup : rooms[ii]->pickups()) {
            if (hasAbility(pickup.ability)) continue;
            const sf::Vector2f p = worldMap->toScreen(sf::Vector2f(levels[ii].position) + pickup.position / Level::TILE_SIZE + sf::Vector2f(0.5f, 0.5f), view);
            const bool bright = std::fmod(clockTime, 0.8f) < 0.4f;
            drawRect(canvas, {std::round(p.x) - 1.f, std::round(p.y) - 1.f}, {3.f, 3.f}, bright ? sf::Color(255, 246, 200) : GOLD);
        }
    }

    // The room you're in, outlined, and you in it, blinking.
    const sf::FloatRect room = worldMap->roomRect(levelIndex);
    const sf::Vector2f corner = worldMap->toScreen({room.left, room.top + room.height}, view);
    sf::RectangleShape outline({std::round(room.width / mapZoom) + 1.f, std::round(room.height / mapZoom) + 1.f});
    outline.setPosition(std::round(corner.x) - 1.f, std::round(corner.y) - 1.f);
    outline.setFillColor(sf::Color::Transparent);
    outline.setOutlineColor(withAlpha(WHITE, 110));
    outline.setOutlineThickness(1.f);
    canvas.draw(outline);
    if (std::fmod(clockTime, 0.6f) < 0.4f) {
        const sf::Vector2f p = worldMap->toScreen(playerTile(), view);
        const sf::Vector2f dot(std::round(p.x) - 1.f, std::round(p.y) - 1.f);
        drawRect(canvas, dot - sf::Vector2f(1.f, 1.f), {5.f, 5.f}, sf::Color(0, 0, 0, 200));
        drawRect(canvas, dot, {3.f, 3.f}, GOLD);
    }
    canvas.setView(canvas.getDefaultView());

    // The name of the explored room under the mouse, or else the one you're in.
    size_t shownRoom = levelIndex;
    const sf::Vector2f mouse = input.mousePosition();
    if (a.contains(mouse)) {
        if (auto hovered = worldMap->roomAt(worldMap->toWorld(mouse, view)); hovered && explored[*hovered]) shownRoom = *hovered;
    }
    std::string name = "LEVEL " + std::to_string(shownRoom + 1) + " - " + upper(levels[shownRoom].title);
    if (!gemTally(shownRoom).empty()) name += "  GEMS " + gemTally(shownRoom);
    hud::drawCentred(canvas, shownRoom == levelIndex ? name + "  (YOU ARE HERE)" : name, HALF_W, a.top + a.height + 4.f, WHITE);
    hud::drawCentred(canvas, "ARROWS / DRAG - MOVE    Z / WHEEL - ZOOM    M - CLOSE", HALF_W, 171.f, GREY);
}
