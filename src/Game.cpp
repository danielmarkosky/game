#include "Game.h"
#include "src/Config.h"
#include "src/Hud.h"
#include "src/Text.h"
#include "src/utils.h"
#include <algorithm>
#include <cctype>
#include <cmath>

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
    constexpr float TALK_REACH = 12.f;      // how close to someone the player has to be to talk to them
    constexpr float TYPING_SPEED = 45.f;    // characters a second
    const sf::FloatRect TALK_BOX(8.f, 6.f, 304.f, 52.f); // at the top, like Celeste: the talkers are usually lower down
    constexpr float TALK_TEXT_LEFT = 46.f;  // from the left of the box, after the face
    constexpr size_t LINES_PER_PAGE = 3;
    const char* const TITLE = "Celeste-like Game";

    const sf::Color WHITE(235, 235, 245);
    const sf::Color GOLD(255, 210, 60);
    const sf::Color RED(255, 90, 90);
    const sf::Color GREY(150, 150, 170);
    const sf::Color CYAN(110, 220, 240);
    constexpr float NOTICE_TIME = 3.f;
    constexpr float FADE_TIME = 0.6f;       // fading in after travelling
    constexpr float BENCH_REACH = 12.f;
    const std::vector<sf::Color> CONFETTI = {GOLD, sf::Color(255, 120, 160), sf::Color(120, 220, 255), sf::Color(160, 240, 120)};

    // Menu item ids.
    enum Choice
    {
        PLAY, OPTIONS, QUIT, ERASE, ERASE_YES, ERASE_NO,
        RESUME, RETRY, MAIN_MENU, BACK, MAP,
        WINDOW_SIZE, FULLSCREEN, SCREEN_SHAKE, TIMER, SKIN,
        FIRST_SLOT = 1000, // + slot number
    };

    void drawRect(sf::RenderTarget& target, sf::Vector2f pos, sf::Vector2f size, sf::Color color)
    {
        sf::RectangleShape rect(size);
        rect.setPosition(pos);
        rect.setFillColor(color);
        target.draw(rect);
    }

    // A 1 px border just outside `area`.
    void drawFrame(sf::RenderTarget& target, sf::FloatRect area, sf::Color color)
    {
        drawRect(target, {area.left - 1.f, area.top - 1.f}, {area.width + 2.f, 1.f}, color);
        drawRect(target, {area.left - 1.f, area.top + area.height}, {area.width + 2.f, 1.f}, color);
        drawRect(target, {area.left - 1.f, area.top}, {1.f, area.height}, color);
        drawRect(target, {area.left + area.width, area.top}, {1.f, area.height}, color);
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

    // The HUD font only has capitals (its lowercase 'x' is a cross), so titles are shown in them.
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
        {"climbing_pick", "CLIMBING PICK", "YOUR ROPE STICKS TO ROCK NOW, NOT JUST METAL"},
        {"long_rope", "LONG ROPE", "YOUR ROPE REACHES HALF AS FAR AGAIN"},
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

Game::Game(std::vector<LevelInfo> levelList, bool testing, size_t startRoom)
    : levels(std::move(levelList)),
      testing(testing),
      files(SaveFiles::defaultFolder()),
      settings(files.loadSettings()),
      window(std::make_shared<sf::RenderWindow>()),
      input(window)
{
    applyWindowSettings();
    canvas.create(config::GAME_WIDTH, config::GAME_HEIGHT);
    canvas.setSmooth(false);
    for (const auto& info : levels) {
        info.level->setTheme(Theme::get(info.theme));
        rooms.push_back(info.level);
    }
    std::vector<WorldMap::Room> mapRooms;
    for (size_t ii = 0; ii < rooms.size(); ++ii) mapRooms.push_back({rooms[ii], levels[ii].position});
    worldMap = std::make_unique<WorldMap>(std::move(mapRooms));

    if (testing) {
        loadLevel(std::min(startRoom, levels.size() - 1));
        visited.assign(levels.size(), false);
        visited[levelIndex] = true;
        openScreen(Screen::Playing);
    } else {
        // Where the last slot played would continue drifts by behind the title screen.
        const auto bench = findBench(files.loadSlot(settings.lastSlot).bench);
        loadLevel(bench ? bench->first : 0);
        openScreen(Screen::Title);
    }
}

// ---------------------------------------------------------------------------------------------
// Screens and menus

void Game::openScreen(Screen next)
{
    if (next == Screen::Options && (screen == Screen::Title || screen == Screen::Paused)) returnScreen = screen;
    if (next == Screen::Slots && screen == Screen::Title) erasing = false;
    if (next == Screen::Playing && screen != Screen::Playing) ignorePressesThisFrame = true;
    screen = next;

    switch (next) {
        case Screen::Title: menu.top = 92.f; menu.maxVisible = 9; break;
        case Screen::Slots: menu.top = 54.f; menu.maxVisible = 9; break;
        case Screen::ConfirmErase: menu.top = 92.f; menu.maxVisible = 9; break;
        case Screen::Options: menu.top = 60.f; menu.maxVisible = 9; break;
        case Screen::Paused: menu.top = 78.f; menu.maxVisible = 9; break;
        case Screen::Complete: menu.top = 132.f; menu.maxVisible = 9; break;
        case Screen::Playing:
        case Screen::Map:
        case Screen::Talking: break;
    }
    refreshMenu();
    menu.select(next == Screen::Slots ? static_cast<size_t>(settings.lastSlot) : 0);
}

void Game::refreshMenu()
{
    std::vector<MenuItem> items;
    switch (screen) {
        case Screen::Title:
            items.push_back(button(PLAY, "PLAY"));
            items.push_back(button(OPTIONS, "OPTIONS"));
            items.push_back(button(QUIT, "QUIT"));
            break;
        case Screen::Slots:
            for (int ii = 0; ii < SaveFiles::SLOTS; ++ii) {
                const Progress p = files.loadSlot(ii);
                const std::string label = std::to_string(ii + 1) + "  " + (p.isEmpty() ? "NEW GAME" : slotSummary(p));
                std::string value = " ";
                if (!p.isEmpty()) {
                    int found = 0;
                    for (size_t room = 0; room < rooms.size(); ++room) {
                        for (size_t gem = 0; gem < rooms[room]->gems().size(); ++gem) found += p.gems.count(gemKey(room, gem)) > 0;
                    }
                    value = hud::formatTime(p.playTime) + "  " + std::to_string(found) + "/" + std::to_string(gemCount()) + (p.finished ? "  *" : "");
                }
                items.push_back({FIRST_SLOT + ii, label, value, false});
            }
            items.push_back({ERASE, erasing ? "DON'T ERASE" : "ERASE A SLOT", " ", false});
            items.push_back({BACK, "BACK", " ", false});
            break;
        case Screen::ConfirmErase:
            items.push_back(button(ERASE_NO, "NO, KEEP IT"));
            items.push_back(button(ERASE_YES, "YES, ERASE IT"));
            break;
        case Screen::Options: {
            const Settings& s = settings;
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
            items.push_back(button(OPTIONS, "OPTIONS"));
            items.push_back(button(MAIN_MENU, "MAIN MENU"));
            break;
        case Screen::Complete:
            items.push_back(button(MAIN_MENU, "MAIN MENU"));
            break;
        case Screen::Playing:
        case Screen::Map:
        case Screen::Talking:
            break;
    }
    menu.setItems(std::move(items));
}

void Game::onMenuChoice(MenuChoice choice)
{
    switch (choice.id) {
        case PLAY: openScreen(Screen::Slots); break;
        case ERASE:
            erasing = !erasing;
            refreshMenu();
            break;
        case ERASE_YES:
            files.eraseSlot(slotToErase);
            erasing = false;
            openScreen(Screen::Slots);
            break;
        case ERASE_NO: back(); break;
        case OPTIONS: openScreen(Screen::Options); break;
        case QUIT: window->close(); break;
        case RESUME: openScreen(Screen::Playing); break;
        case MAP: openMap(); break;
        case RETRY:
            if (deathTimer <= 0.f && !transition) killPlayer();
            openScreen(Screen::Playing);
            break;
        case MAIN_MENU:
            saveProgress();
            openScreen(Screen::Title);
            break;
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
            if (choice.id >= FIRST_SLOT) {
                const int chosen = choice.id - FIRST_SLOT;
                if (!erasing) {
                    startGame(chosen);
                } else if (!files.loadSlot(chosen).isEmpty()) {
                    slotToErase = chosen;
                    openScreen(Screen::ConfirmErase);
                }
            }
            break;
    }
}

void Game::back()
{
    switch (screen) {
        case Screen::Options: openScreen(returnScreen); break;
        case Screen::Slots: openScreen(Screen::Title); break;
        case Screen::ConfirmErase: openScreen(Screen::Slots); break;
        case Screen::Paused: openScreen(Screen::Playing); break;
        case Screen::Complete: openScreen(Screen::Title); break;
        case Screen::Map: openScreen(mapReturn); break;
        case Screen::Talking: endTalk(); break;
        case Screen::Title:
        case Screen::Playing: break;
    }
}

void Game::changeSetting(int id, int direction)
{
    Settings& s = settings;
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
    saveSettings();
    refreshMenu();
}

void Game::applyWindowSettings()
{
    const Settings& s = settings;
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
        case Screen::Map:
        case Screen::Talking: return true;
        case Screen::Options: return returnScreen == Screen::Paused;
        case Screen::Title:
        case Screen::Slots:
        case Screen::ConfirmErase: return false;
    }
    return false;
}

size_t Game::skinIndex() const
{
    return Player::skinNamed(settings.skin);
}

std::string Game::slotSummary(const Progress& p) const
{
    // Where it would continue: the room of the bench, or the start.
    const auto bench = findBench(p.bench);
    return upper(levels[bench ? bench->first : 0].title);
}

std::string Game::levelKey(size_t index) const
{
    return levels[index].name;
}

void Game::saveSettings()
{
    files.saveSettings(settings);
}

void Game::saveProgress()
{
    if (!testing && slot >= 0) files.saveSlot(slot, progress);
}

// ---------------------------------------------------------------------------------------------
// Playing

void Game::startGame(int which)
{
    slot = which;
    settings.lastSlot = which;
    saveSettings();
    progress = files.loadSlot(which);
    messageTimer = 0.f;
    visited.assign(levels.size(), false);
    // On the bench sat on last, or at the very start.
    const auto bench = findBench(progress.bench);
    loadLevel(bench ? bench->first : 0);
    if (bench) sitDown(bench->second);
    visited[levelIndex] = true;
    progress.explored.insert(levelKey(levelIndex));
    saveProgress();
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
    sitting.reset();
    player->respawn(checkpoint);
    particles.clear();
    transition.reset();
    deathTimer = 0.f;
    deathFlash = 0.f;
    shake = 0.f;
    ignoreInputThisFrame = true;
    updateCamera(0.f, true);
}

void Game::enterRoom(size_t index, Level::Side side)
{
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
    updateCamera(0.f, true);
    // The title only shows the first time in.
    if (!visited[index]) {
        visited[index] = true;
        titleTimer = TITLE_TIME;
    }
    progress.explored.insert(levelKey(index));
    saveProgress();
}

void Game::finishGame()
{
    // The slot carries on from its last bench afterwards, for whatever's left to find.
    progress.finished = true;
    saveProgress();
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
    return testing || progress.abilities.count(ability) > 0;
}

void Game::applyAbilities()
{
    player->setCanHookRock(hasAbility("climbing_pick"));
    player->setRopeRange(hasAbility("long_rope") ? Player::LONG_ROPE_RANGE : Player::ROPE_RANGE);
}

void Game::collect(const Pickup& pickup)
{
    progress.abilities.insert(pickup.ability);
    saveProgress();
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

void Game::showNotice(std::string text)
{
    notice = std::move(text);
    noticeTimer = NOTICE_TIME;
}

std::string Game::gemKey(size_t room, size_t gem) const
{
    const sf::Vector2f tile = rooms[room]->gems()[gem] / Level::TILE_SIZE;
    return levelKey(room) + " " + std::to_string(static_cast<int>(tile.x)) + " " + std::to_string(static_cast<int>(tile.y));
}

bool Game::gemFound(size_t room, size_t gem) const
{
    return progress.gems.count(gemKey(room, gem)) > 0;
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
// Talking

const Npc* Game::npcInReach() const
{
    if (transition || deathTimer > 0.f || player->getRope() || !player->isGrounded()) return nullptr;
    for (const Npc& npc : level->npcs()) {
        const sf::Vector2f d = player->centre() - (npc.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, 6.f));
        if (!npc.dialogue.empty() && std::abs(d.x) <= TALK_REACH && std::abs(d.y) <= TALK_REACH) return &npc;
    }
    return nullptr;
}

void Game::startTalk(const Npc& npc)
{
    Dialogue dialogue;
    try {
        dialogue = Dialogue::load(npc.dialogue);
    } catch (const std::exception&) {
        return; // nothing to say; game_tests checks every character's file is there
    }
    auto had = [&](size_t n) { return progress.conversations.count(npc.dialogue + " " + std::to_string(n)) > 0; };
    auto has = [this](const std::string& ability) { return hasAbility(ability); };
    const std::optional<size_t> conversation = dialogue.next(had, has);
    if (!conversation) return;

    // Each line, wrapped to the box, a page at a time.
    Talk t;
    t.npc = npc;
    t.conversation = *conversation;
    const float width = TALK_BOX.width - TALK_TEXT_LEFT - 8.f;
    for (const DialogueLine& line : dialogue.conversations()[*conversation].lines) {
        const std::vector<std::string> wrapped = text::wrap(line.text, width);
        for (size_t first = 0; first < wrapped.size(); first += LINES_PER_PAGE) {
            const auto end = wrapped.begin() + static_cast<long>(std::min(first + LINES_PER_PAGE, wrapped.size()));
            t.pages.push_back({line.speaker, {wrapped.begin() + static_cast<long>(first), end}});
        }
    }
    talk = t;
    openScreen(Screen::Talking);
}

void Game::advanceTalk()
{
    // First show the rest of the page, then turn it.
    size_t length = 0;
    for (const std::string& line : talk->pages[talk->page].lines) length += line.size();
    if (talk->typed < static_cast<float>(length)) {
        talk->typed = static_cast<float>(length);
    } else if (++talk->page < talk->pages.size()) {
        talk->typed = 0.f;
    } else {
        endTalk();
    }
}

void Game::endTalk()
{
    progress.conversations.insert(talk->npc.dialogue + " " + std::to_string(talk->conversation));
    saveProgress();
    talk.reset();
    openScreen(Screen::Playing);
}

// ---------------------------------------------------------------------------------------------
// Benches

std::optional<size_t> Game::benchInReach() const
{
    if (transition || deathTimer > 0.f || player->getRope() || !player->isGrounded()) return std::nullopt;
    const auto& benches = level->benches();
    for (size_t ii = 0; ii < benches.size(); ++ii) {
        const sf::Vector2f d = player->centre() - (benches[ii] + sf::Vector2f(Level::BENCH_WIDTH * 0.5f, 6.f));
        if (std::abs(d.x) <= BENCH_REACH && std::abs(d.y) <= BENCH_REACH) return ii;
    }
    return std::nullopt;
}

std::string Game::benchKey(size_t room, size_t bench) const
{
    return levelKey(room) + " " + std::to_string(bench);
}

std::optional<std::pair<size_t, size_t>> Game::findBench(const std::string& key) const
{
    for (size_t room = 0; room < levels.size(); ++room) {
        for (size_t bench = 0; bench < rooms[room]->benches().size(); ++bench) {
            if (benchKey(room, bench) == key) return std::pair{room, bench};
        }
    }
    return std::nullopt;
}

void Game::sitDown(size_t bench)
{
    // In the middle of the bench, which then is where dying brings you back to.
    const sf::Vector2f seat = level->benches()[bench];
    checkpoint = {std::round(seat.x + (Level::BENCH_WIDTH - Player::WIDTH) * 0.5f), seat.y};
    needsSpawnPoint = false;
    player->respawn(checkpoint);
    updateCamera(0.f, true);
    sitting = bench;

    progress.bench = benchKey(levelIndex, bench);
    progress.benches.insert(progress.bench);
    saveProgress();
    showNotice("SAVED");
}

std::vector<std::string> Game::travelDestinations() const
{
    std::vector<std::string> destinations;
    for (size_t room = 0; room < levels.size(); ++room) {
        for (size_t bench = 0; bench < rooms[room]->benches().size(); ++bench) {
            if (progress.benches.count(benchKey(room, bench))) destinations.push_back(benchKey(room, bench));
        }
    }
    return destinations;
}

void Game::travelTo(const std::string& key)
{
    const auto bench = findBench(key);
    if (!bench) return;
    loadLevel(bench->first); // shows the room's name, saying where you've arrived
    sitDown(bench->second);
    visited[levelIndex] = true;
    progress.explored.insert(levelKey(levelIndex));
    fade = FADE_TIME;
    openScreen(Screen::Playing);
}

// ---------------------------------------------------------------------------------------------
// Map

void Game::openMap(bool travel)
{
    worldMap->reveal(exploredRooms());
    mapReturn = screen;
    travelling = travel;
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
    if (travelling) {
        // Starting from the bench you're on.
        const auto destinations = travelDestinations();
        const auto here = std::find(destinations.begin(), destinations.end(), progress.bench);
        travelChoice = here == destinations.end() ? 0 : static_cast<size_t>(here - destinations.begin());
    }
    openScreen(Screen::Map);
}

void Game::chooseDestination(int step)
{
    const auto destinations = travelDestinations();
    if (destinations.empty()) return;
    const int count = static_cast<int>(destinations.size());
    travelChoice = static_cast<size_t>((static_cast<int>(travelChoice) + step + count) % count);
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

    // Travelling: a click on a bench chooses it, a second one goes there.
    if (travelling && input.wasMouseClicked()) {
        const auto destinations = travelDestinations();
        for (size_t ii = 0; ii < destinations.size(); ++ii) {
            const auto [room, bench] = *findBench(destinations[ii]);
            const sf::Vector2f tile = sf::Vector2f(levels[room].position) + rooms[room]->benches()[bench] / Level::TILE_SIZE + sf::Vector2f(1.f, 0.5f);
            const sf::Vector2f d = worldMap->toScreen(tile, mapView()) - input.mousePosition();
            if (std::abs(d.x) > 5.f || std::abs(d.y) > 5.f) continue;
            if (ii == travelChoice) {
                travelTo(destinations[ii]);
                return;
            }
            travelChoice = ii;
        }
    }

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
    std::vector<bool> explored(levels.size());
    for (size_t ii = 0; ii < levels.size(); ++ii) explored[ii] = ii == levelIndex || progress.explored.count(levelKey(ii)) > 0;
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
            if (event.type == sf::Event::Closed) {
                if (inGame()) saveProgress();
                window->close();
            }
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
        } else if (screen == Screen::Talking) {
            if (input.wasMouseClicked()) advanceTalk();
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
                // On a bench, moving gets you up; anything else leaves you sitting.
                if (sitting) {
                    if (in.moveX != 0.f || in.jumpPressed || in.ropePressed) sitting.reset();
                    else in = {};
                }
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
        else if (key == K::M || key == K::Tab) openMap(sitting && travelDestinations().size() > 1);
        else if ((key == K::W || key == K::Up || key == K::E) && npcInReach()) startTalk(*npcInReach());
        else if ((key == K::W || key == K::Up || key == K::E) && !sitting && benchInReach()) sitDown(*benchInReach());
        else if (key == K::R && !transition && deathTimer <= 0.f) killPlayer(); // back to the checkpoint
        return;
    }
    if (screen == Screen::Talking) {
        if (key == K::Escape) back();
        else if (key == K::Space || key == K::Enter || key == K::E || key == K::W || key == K::Up) advanceTalk();
        return;
    }
    if (screen == Screen::Map && travelling) {
        if (key == K::Left || key == K::A) chooseDestination(-1);
        else if (key == K::Right || key == K::D) chooseDestination(1);
        else if (key == K::Enter || key == K::Space || key == K::E) travelTo(travelDestinations()[travelChoice]);
        else if (key == K::Escape || key == K::M || key == K::Tab) back();
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
    if (screen == Screen::Talking) talk->typed += dt * TYPING_SPEED;
    if (screen != Screen::Playing) return;

    deathFlash = std::max(0.f, deathFlash - dt);
    shake = std::max(0.f, shake - dt);
    titleTimer = std::max(0.f, titleTimer - dt);
    messageTimer = std::max(0.f, messageTimer - dt);
    noticeTimer = std::max(0.f, noticeTimer - dt);
    fade = std::max(0.f, fade - dt);
    progress.playTime += dt;
    particles.update(dt);

    if (transition) {
        transition->time += dt;
        if (transition->time >= TRANSITION_TIME) transition.reset();
        return;
    }

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
        progress.gems.insert(gemKey(levelIndex, *gem));
        saveProgress();
        showNotice(gemTotal());
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
    ++progress.deaths;
    sitting.reset();
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
        if (shake > 0.f && settings.screenShake && screen == Screen::Playing) {
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
    if (fade > 0.f) dimScreen(canvas, static_cast<sf::Uint8>(255.f * fade / FADE_TIME));
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
        std::optional<size_t> saved;
        for (size_t bench = 0; bench < rooms[v.index]->benches().size(); ++bench) {
            if (benchKey(v.index, bench) == progress.bench) saved = bench;
        }
        rooms[v.index]->drawBenches(canvas, v.cam, saved, clockTime);
        rooms[v.index]->drawPickups(canvas, v.cam, clockTime, owned);
        rooms[v.index]->drawGems(canvas, v.cam, clockTime, [&](size_t gem) { return gemFound(v.index, gem); });
        for (const Npc& npc : rooms[v.index]->npcs()) {
            // Turned towards the player, and not all breathing in step.
            const sf::Vector2f feet = npc.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, 0.f);
            const bool facingLeft = v.index == levelIndex && player->centre().x < feet.x;
            player->drawFigure(canvas, Player::skinNamed(npc.look), viewCoordinates(feet, v.cam), facingLeft, clockTime + feet.x * 0.37f);
        }
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

    if (settings.showTimer) {
        hud::drawText(canvas, hud::formatTime(progress.playTime), {4.f, 4.f}, WHITE);
        hud::drawText(canvas, "x" + std::to_string(progress.deaths), {4.f, 11.f}, RED);
    }

    // Someone to talk to, or a bench to sit on: a little bubble above saying which key.
    std::optional<sf::Vector2f> bubbleAt;
    if (screen == Screen::Playing) {
        if (const Npc* npc = npcInReach()) bubbleAt = npc->position + sf::Vector2f(Level::TILE_SIZE * 0.5f, 24.f);
        else if (auto bench = sitting ? std::nullopt : benchInReach()) bubbleAt = level->benches()[*bench] + sf::Vector2f(Level::BENCH_WIDTH * 0.5f, 20.f);
    }
    if (bubbleAt) {
        const float bob = std::round(std::sin(clockTime * 4.f));
        const sf::Vector2f p = viewCoordinates(*bubbleAt + sf::Vector2f(0.f, bob), drawCamera());
        const sf::FloatRect bubble(std::floor(p.x) - 5.f, std::floor(p.y), 11.f, 9.f);
        drawRect(canvas, {bubble.left, bubble.top}, {bubble.width, bubble.height}, sf::Color(14, 14, 22, 220));
        drawFrame(canvas, bubble, withAlpha(WHITE, 160));
        hud::drawText(canvas, "W", {bubble.left + 4.f, bubble.top + 2.f}, WHITE);
    }
    if (sitting && screen == Screen::Playing) {
        const std::string hint = travelDestinations().size() > 1 ? "M - TRAVEL    MOVE - GET UP" : "MOVE - GET UP";
        hud::drawCentred(canvas, hint, HALF_W, 168.f, GREY);
    }

    // A note for a moment (saved, or how many gems now), and the gem count while paused.
    const float noteY = settings.showTimer ? 18.f : 4.f;
    if (screen == Screen::Paused && gemCount() > 0) {
        hud::drawText(canvas, gemTotal(), {4.f, noteY}, CYAN);
    } else if (noticeTimer > 0.f) {
        const float alpha = std::min({1.f, (NOTICE_TIME - noticeTimer) * 6.f, noticeTimer});
        hud::drawText(canvas, notice, {4.f, noteY}, withAlpha(CYAN, static_cast<sf::Uint8>(255.f * alpha)));
    }

    // Pause button in the top right corner.
    const sf::FloatRect button = pauseButton();
    const bool hovered = screen == Screen::Playing && button.contains(input.mousePosition());
    const sf::Color barColor = hovered ? WHITE : GREY;
    for (float x : {button.left + 3.f, button.left + 7.f}) {
        drawRect(canvas, {x + 1.f, button.top + 4.f}, {2.f, 6.f}, sf::Color(0, 0, 0, 160));
        drawRect(canvas, {x, button.top + 3.f}, {2.f, 6.f}, barColor);
    }

    if (titleTimer > 0.f && messageTimer <= 0.f && screen == Screen::Playing) {
        // Fade in quickly, hold, fade out over the last second.
        float alpha = std::min({1.f, (TITLE_TIME - titleTimer) * 4.f, titleTimer});
        auto a = static_cast<sf::Uint8>(255.f * alpha);
        hud::drawCentred(canvas, upper(levels[levelIndex].title), HALF_W, 40.f, withAlpha(WHITE, a), 2.f);
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
        case Screen::Slots:
            dimScreen(canvas, 170);
            hud::drawCentred(canvas, erasing ? "ERASE WHICH?" : "CHOOSE A SLOT", HALF_W, 24.f, erasing ? RED : GOLD, 2.f);
            hud::drawCentred(canvas, "ARROWS / MOUSE - CHOOSE    ENTER - OK    ESC - BACK", HALF_W, 168.f, GREY);
            break;
        case Screen::ConfirmErase:
            dimScreen(canvas, 190);
            hud::drawCentred(canvas, "ERASE SLOT " + std::to_string(slotToErase + 1) + "?", HALF_W, 40.f, RED, 2.f);
            hud::drawCentred(canvas, "IT'S GONE FOR GOOD.", HALF_W, 62.f, GREY);
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
            hud::drawCentred(canvas, upper(levels[levelIndex].title), HALF_W, 60.f, GREY);
            break;
        case Screen::Complete: {
            dimScreen(canvas, 170);
            hud::drawCentred(canvas, "YOU DID IT!", HALF_W, 26.f, GOLD, 3.f);
            hud::drawCentred(canvas, "YOUR TIME", HALF_W, 56.f, GREY);
            hud::drawCentred(canvas, hud::formatTime(progress.playTime), HALF_W, 70.f, WHITE, 4.f);
            hud::drawCentred(canvas, "x" + std::to_string(progress.deaths), HALF_W, 100.f, RED, 2.f);
            if (gemCount() > 0) hud::drawCentred(canvas, gemTotal(), HALF_W, 118.f, CYAN);
            particles.draw(canvas, drawCamera());
            break;
        }
        case Screen::Map:
            drawMap();
            break;
        case Screen::Talking:
            drawTalk();
            break;
        case Screen::Playing:
            break;
    }
    menu.draw(canvas, clockTime);
}

void Game::drawTalk()
{
    const Page& page = talk->pages[talk->page];
    const sf::FloatRect& box = TALK_BOX;
    drawRect(canvas, {box.left, box.top}, {box.width, box.height}, sf::Color(14, 14, 22, 235));
    drawFrame(canvas, box, withAlpha(GREY, 180));

    // The face of whoever's speaking: you, or the one you're talking to.
    const bool you = page.speaker == "You";
    const sf::FloatRect face(box.left + 6.f, box.top + 8.f, 34.f, 36.f);
    drawRect(canvas, {face.left, face.top}, {face.width, face.height}, sf::Color(34, 38, 58));
    drawFrame(canvas, face, withAlpha(GREY, 120));
    player->drawFigure(canvas, you ? skinIndex() : Player::skinNamed(talk->npc.look), {face.left + 17.f, face.top + 33.f}, false, clockTime, 2.f);

    // The name, then the words typing themselves out.
    const float left = box.left + TALK_TEXT_LEFT;
    if (!page.speaker.empty()) text::draw(canvas, page.speaker, {left, box.top + 4.f}, GOLD);
    size_t typed = static_cast<size_t>(talk->typed), length = 0;
    for (size_t i = 0; i < page.lines.size(); ++i) {
        text::draw(canvas, page.lines[i], {left, box.top + 16.f + text::LINE_HEIGHT * static_cast<float>(i)}, WHITE, typed);
        typed -= std::min(typed, page.lines[i].size());
        length += page.lines[i].size();
    }
    // All shown: a blinking arrow says there's more, or that it's the end.
    if (talk->typed >= static_cast<float>(length) && std::fmod(clockTime, 0.8f) < 0.5f) {
        const sf::Vector2f arrow(box.left + box.width - 10.f, box.top + box.height - 7.f);
        for (float row = 0.f; row < 3.f; ++row) drawRect(canvas, {arrow.x + row, arrow.y + row}, {5.f - 2.f * row, 1.f}, GOLD);
    }
}

void Game::drawMap()
{
    dimScreen(canvas, 215);
    hud::drawCentred(canvas, travelling ? "TRAVEL" : "MAP", HALF_W, 4.f, GOLD, 2.f);
    const WorldMap::View view = mapView();
    const sf::FloatRect& a = view.area;
    drawFrame(canvas, a, withAlpha(GREY, 90));
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

    // The benches you've sat on, and in travel mode the one you'd go to, framed.
    const auto destinations = travelDestinations();
    for (size_t ii = 0; ii < destinations.size(); ++ii) {
        const auto [room, bench] = *findBench(destinations[ii]);
        const sf::Vector2f tile = sf::Vector2f(levels[room].position) + rooms[room]->benches()[bench] / Level::TILE_SIZE + sf::Vector2f(1.f, 0.5f);
        const sf::Vector2f p = worldMap->toScreen(tile, view);
        const sf::Vector2f mark(std::round(p.x) - 2.f, std::round(p.y) - 1.f);
        drawRect(canvas, mark - sf::Vector2f(1.f, 1.f), {6.f, 4.f}, sf::Color(0, 0, 0, 200));
        drawRect(canvas, mark, {4.f, 2.f}, sf::Color(220, 170, 108));
        if (travelling && ii == travelChoice && std::fmod(clockTime, 0.6f) < 0.45f) drawFrame(canvas, {mark.x - 2.f, mark.y - 2.f, 8.f, 6.f}, WHITE);
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

    if (travelling) {
        const size_t room = findBench(destinations[travelChoice])->first;
        hud::drawCentred(canvas, "TRAVEL TO " + upper(levels[room].title) + (room == levelIndex ? "  (YOU ARE HERE)" : ""), HALF_W, a.top + a.height + 4.f, WHITE);
        hud::drawCentred(canvas, "LEFT / RIGHT / CLICK - CHOOSE    ENTER - GO    ESC - STAY", HALF_W, 171.f, GREY);
        return;
    }

    // The name of the explored room under the mouse, or else the one you're in.
    size_t shownRoom = levelIndex;
    const sf::Vector2f mouse = input.mousePosition();
    if (a.contains(mouse)) {
        if (auto hovered = worldMap->roomAt(worldMap->toWorld(mouse, view)); hovered && explored[*hovered]) shownRoom = *hovered;
    }
    std::string name = upper(levels[shownRoom].title);
    if (!gemTally(shownRoom).empty()) name += "  GEMS " + gemTally(shownRoom);
    hud::drawCentred(canvas, shownRoom == levelIndex ? name + "  (YOU ARE HERE)" : name, HALF_W, a.top + a.height + 4.f, WHITE);
    hud::drawCentred(canvas, "ARROWS / DRAG - MOVE    Z / WHEEL - ZOOM    M - CLOSE", HALF_W, 171.f, GREY);
}
