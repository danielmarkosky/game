#include "Game.h"
#include "src/GameShared.h"
#include "src/Outfits.h"
#include "src/Regions.h"
#include "src/Hud.h"
#include "src/Physics.h"
#include "src/Props.h"
#include "src/Text.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace game_detail;

namespace
{
    constexpr float CAMERA_FOLLOW = 6.f;    // higher = camera catches up faster
    // The camera looks ahead of where you're going: this many seconds of your speed, up to LOOK_AHEAD
    // pixels, easing over at LOOK_AHEAD_FOLLOW.
    constexpr float LOOK_AHEAD_TIME = 0.5f;
    constexpr float LOOK_AHEAD = 56.f;
    constexpr float LOOK_AHEAD_FOLLOW = 2.5f;
    // Up and down it stays at the height you last stood (or held on to a wall) at, unless you go
    // further above it than a jump or further below it than this: jumps don't bob the screen.
    constexpr float RISE_ROOM = 34.f;
    constexpr float DROP_ROOM = 20.f;
    constexpr float FALL_OUT_MARGIN = 64.f; // how far below the level counts as falling out
    constexpr float TALK_REACH = 12.f;      // how close to someone the player has to be to talk to them
    constexpr float BENCH_REACH = 12.f;     // the same for benches and levers

    // What picking each ability up tells you.
    struct AbilityInfo
    {
        const char* id;
        const char* name;
        const char* text;
    };
    // In the order they're found: in a test room, the number keys switch them.
    const AbilityInfo ABILITIES[] = {
        {"rope", "ROPE AND HOOK", "HOLD TO THROW IT, STANDING. KEEP HOLDING TO SWING"},
        {"long_rope", "LONG ROPE", "YOUR ROPE REACHES FURTHER"},
        {"wall_climb", "CHALK BAG", "PUSH INTO A WALL TO GRAB IT. UP CLIMBS, JUMP LEAPS OFF"},
        {"air_throw", "THROWING GLOVE", "THROW THE ROPE IN MID-AIR TOO"},
        {"double_jump", "UPDRAFT FEATHER", "JUMP AGAIN IN THE AIR"},
        {"stopwatch", "GRAN'S STOPWATCH", "TIME YOUR RUNS: FROM A GREEN POST TO THE FLAG"},
    };
}

Game::Game(std::vector<LevelInfo> levelList, Options options)
    : levels(std::move(levelList)),
      testing(options.testing),
      headless(options.headless),
      startAt(options.startAt),
      files(options.saveFolder.empty() ? SaveFiles::defaultFolder() : options.saveFolder),
      settings(files.loadSettings()),
      window(std::make_shared<sf::RenderWindow>()),
      input(window)
{
    const size_t startRoom = options.startRoom;
    input.setBindings(settings.controls);
    if (!headless) applyWindowSettings();
    const bool heard = !headless || options.sounds;
    audio.setVolumes(heard ? settings.musicVolume : 0, heard ? settings.soundVolume : 0);
    canvas.create(config::GAME_WIDTH, config::GAME_HEIGHT);
    canvas.setSmooth(false);
    for (const auto& info : levels) {
        info.level->setTheme(Theme::get(info.theme));
        rooms.push_back(info.level);
    }
    std::vector<WorldMap::Room> mapRooms;
    for (size_t ii = 0; ii < rooms.size(); ++ii) mapRooms.push_back({rooms[ii], levels[ii].position});
    mapScreen = std::make_unique<MapScreen>(std::move(mapRooms));

    applyGates();
    if (testing) {
        for (const AbilityInfo& info : ABILITIES) progress.abilities.insert(info.id);
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
// Playing

void Game::startGame(int which)
{
    slot = which;
    settings.lastSlot = which;
    saveSettings();
    progress = files.loadSlot(which);
    messageTimer = 0.f;
    visited.assign(levels.size(), false);
    applyGates();
    // On the bench sat on last, or at the very start.
    const auto bench = findBench(progress.bench);
    loadLevel(bench ? bench->first : 0);
    if (bench) sitDown(bench->second);
    // A new game: you come to on the floor where you fell.
    waking = progress.isEmpty();
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
    applyAbilities();
    applyOutfit();
    spawnAtStart();
    titleTimer = TITLE_TIME;
}

void Game::spawnAtStart()
{
    checkpoint = startAt.value_or(level->getStartPos());
    startAt.reset();
    checkpointRiding = 0;
    needsSpawnPoint = false;
    sitting.reset();
    waking = false;
    level->reset(); // icicles back, crumbled blocks whole again
    player->respawn(checkpoint);
    dog.reset(player->getPosition() + sf::Vector2f(Player::WIDTH * 0.5f, 0.f));
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
    level->reset();
    player->enterLevel(level, shift, side == Level::Side::Top);
    dog.shift(shift);
    particles.clear();
    cartRefused = false;

    // Until the player lands somewhere in the new room, dying puts them back where they came in.
    checkpoint = player->getPosition();
    needsSpawnPoint = true;
    updateCamera(0.f, true);
    // The title only shows the first time in, and only coming into another place: a place can be
    // several rooms with the same title.
    if (!visited[index]) {
        visited[index] = true;
        if (levels[index].title != levels[transition->from].title) titleTimer = TITLE_TIME;
    }
    progress.explored.insert(levelKey(index));
    // The first time with no gravity: how it works.
    if (level->isSpace() && progress.flags.insert("seen_space").second) showMessage("NOTHING FALLS HERE", "THE ARROWS PUSH YOU ANY WAY");
    saveProgress();
}

void Game::finishGame()
{
    // How the climb went.
    // How the story ends, then how the climb went, and thanks.
    credits.clear();
    creditsTime = 0.f;
    const sf::Color story(225, 220, 235), soft(170, 170, 190);
    credits.emplace_back("The spark caught. The Lantern burned on the mountain again, and one by one the stars "
                         "drifted back up to where they belong.", story);
    credits.emplace_back("", story);
    credits.emplace_back("Odo mended the bridge by the end of the summer. Nobody has fallen from it since.", soft);
    credits.emplace_back("Fen counted the stars every night until they were all home. It took a long time. Fen didn't mind.", soft);
    credits.emplace_back("Mags sold a great many hats with brims, for the glare.", soft);
    if (hasDog()) credits.emplace_back("Pip sleeps by the Lantern, where it's warm.", soft);
    else credits.emplace_back("Down in the Hollows, a dog is still waiting on a ledge. Someone should go and get him.", soft);
    credits.emplace_back("And the new keeper climbs every morning, just to see the light. Gran would have timed it.", soft);
    credits.emplace_back("", story);
    credits.emplace_back(hud::formatTime(progress.playTime) + "   x" + std::to_string(progress.deaths), hud::GREY);
    if (gemCount() > 0) credits.emplace_back(gemTotal(), hud::GREY);
    for (const auto& [route, seconds] : progress.records) credits.emplace_back(regions::name(route) + "  " + hud::formatTime(seconds), hud::GREY);
    credits.emplace_back("", hud::WHITE);
    credits.emplace_back("TETHER", hud::GOLD);
    credits.emplace_back("THANK YOU FOR CLIMBING", hud::WHITE);
    // The slot carries on from its last bench afterwards, for whatever's left to find.
    progress.finished = true;
    saveProgress();
    particles.clear();
    openScreen(Screen::Complete);
}

std::optional<size_t> Game::roomAt(sf::Vector2f point) const
{
    const auto room = mapScreen->roomAt((point + roomOrigin(levelIndex)) / Level::TILE_SIZE);
    if (room == levelIndex) return std::nullopt;
    return room;
}

sf::Vector2f Game::roomOrigin(size_t index) const
{
    return sf::Vector2f(levels[index].position) * Level::TILE_SIZE;
}

bool Game::hasAbility(const std::string& ability) const
{
    // Picked up. (In a test room it's all given at the start, and the number keys switch it.)
    return progress.abilities.count(ability) > 0;
}

bool Game::hasFact(const std::string& fact) const
{
    for (const AbilityInfo& info : ABILITIES) {
        if (fact == info.id) return hasAbility(fact);
    }
    return progress.flags.count(fact) > 0;
}

void Game::applyAbilities()
{
    player->setHasRope(hasAbility("rope"));
    player->setRopeRange(hasAbility("long_rope") ? physics().longRopeRange : physics().ropeRange);
    player->setCanClimb(hasAbility("wall_climb"));
    player->setCanThrowInAir(hasAbility("air_throw"));
    player->setCanDoubleJump(hasAbility("double_jump"));
}

bool Game::touchesSlick(sf::Vector2f point) const
{
    for (float dx : {-1.f, 1.f}) {
        for (float dy : {-1.f, 1.f}) {
            const auto [x, y] = tailNumbers(point + sf::Vector2f(dx, dy));
            if (level->isSlick(x, y)) return true;
        }
    }
    return false;
}

void Game::toggleAbility(size_t index)
{
    if (index >= std::size(ABILITIES)) return;
    const AbilityInfo& info = ABILITIES[index];
    const bool had = progress.abilities.erase(info.id) > 0;
    if (!had) progress.abilities.insert(info.id);
    applyAbilities();
    showNotice(std::string(info.name) + (had ? " OFF" : " ON"));
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
                                                                .colors = {hud::GOLD, sf::Color(255, 246, 200), sf::Color(232, 238, 248)}});
    for (const AbilityInfo& info : ABILITIES) {
        if (pickup.ability == info.id) showMessage(info.name, info.text);
    }
    audio.play(Audio::Sfx::Pickup);
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
    if (transition || deathTimer > 0.f || player->getRope() || player->isRiding() || !player->isGrounded()) return nullptr;
    for (const Npc& npc : level->npcs()) {
        const sf::Vector2f d = player->centre() - (npc.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, 6.f));
        if (!npc.dialogue.empty() && isAround(npc) && std::abs(d.x) <= TALK_REACH && std::abs(d.y) <= TALK_REACH) return &npc;
    }
    return nullptr;
}

bool Game::isAround(const Npc& npc) const
{
    return (npc.needs.empty() || hasFact(npc.needs)) && (npc.until.empty() || !hasFact(npc.until));
}

void Game::startTalk(const Npc& npc)
{
    try {
        Dialogue dialogue = Dialogue::load(npc.dialogue);
        auto had = [&](size_t n) { return progress.conversations.count(npc.dialogue + " " + std::to_string(n)) > 0; };
        const std::optional<size_t> conversation = dialogue.next(had, [this](const std::string& f) { return hasFact(f); });
        if (!conversation) return;
        talk.emplace(npc, std::move(dialogue), *conversation, [this](const std::string& flag) { progress.flags.insert(flag); },
                     [this](const std::string& command) { act(command); });
        talkedTo = npc.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, 0.f);
    } catch (const std::exception&) {
        return; // nothing to say; game_tests checks every character's file is there
    }
    openScreen(Screen::Talking);
}

void Game::endTalk()
{
    const bool dogBefore = talk->key().rfind("pip ", 0) == 0 && !hasDog();
    progress.conversations.insert(talk->key());
    saveProgress();
    talk.reset();
    if (dogBefore && hasDog()) {
        // Pip's yours again: he jumps down to you.
        dog.reset(talkedTo);
        showMessage("PIP", "HE'LL SNIFF OUT GEMS FOR YOU. WATCH WHERE HE POINTS");
        audio.play(Audio::Sfx::Bark);
    }
    openScreen(shopping ? Screen::Shop : Screen::Playing);
    shopping = false;
}

void Game::act(const std::string& command)
{
    std::istringstream words(command);
    std::string verb, what, name;
    words >> verb >> what >> name;
    if (verb == "give" && what == "ability") {
        // Handed over in a conversation, like picking it up: what it is shows once the talking's done.
        if (!hasAbility(name)) {
            progress.abilities.insert(name);
            applyAbilities();
            for (const AbilityInfo& info : ABILITIES) {
                if (name == info.id) showMessage(info.name, info.text);
            }
            particles.spawn(player->centre(), {.count = 30, .speed = 60.f, .life = 0.8f, .gravity = 40.f, .size = 2.f,
                                               .colors = {hud::GOLD, sf::Color(255, 246, 200), sf::Color(232, 238, 248)}});
            audio.play(Audio::Sfx::Pickup);
        }
    } else if (verb == "open" && what == "shop") {
        // Once the talking's done, the stall.
        shopping = true;
    }
    saveProgress();
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
    checkpointRiding = 0;
    level->reset(); // icicles back, crumbled blocks whole again
    player->respawn(checkpoint);
    dog.reset(checkpoint + sf::Vector2f(-6.f, 0.f));
    updateCamera(0.f, true);
    sitting = bench;

    progress.bench = benchKey(levelIndex, bench);
    progress.benches.insert(progress.bench);
    saveProgress();
    showNotice("SAVED");
    audio.play(Audio::Sfx::Bench);
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
    timedRun.reset(); // a timed run doesn't survive travelling
    visited[levelIndex] = true;
    progress.explored.insert(levelKey(levelIndex));
    fade = FADE_TIME;
    audio.play(Audio::Sfx::Travel);
    openScreen(Screen::Playing);
}

// ---------------------------------------------------------------------------------------------
// Levers

std::optional<size_t> Game::leverInReach() const
{
    if (level->gatesOpen() || transition || deathTimer > 0.f || player->getRope() || !player->isGrounded()) return std::nullopt;
    const auto& levers = level->levers();
    for (size_t ii = 0; ii < levers.size(); ++ii) {
        const sf::Vector2f d = player->centre() - (levers[ii] + sf::Vector2f(4.f, 6.f));
        if (std::abs(d.x) <= BENCH_REACH && std::abs(d.y) <= BENCH_REACH) return ii;
    }
    return std::nullopt;
}

void Game::pullLever()
{
    level->setGatesOpen(true);
    progress.flags.insert("open_" + levelKey(levelIndex));
    saveProgress();
    audio.play(Audio::Sfx::Door);
    shake = SHAKE_TIME;
    showNotice("A GATE OPENED");
}

void Game::applyGates()
{
    for (size_t room = 0; room < rooms.size(); ++room) {
        rooms[room]->setGatesOpen(progress.flags.count("open_" + levelKey(room)) > 0);
        for (size_t lamp = 0; lamp < rooms[room]->lamps().size(); ++lamp) {
            rooms[room]->setLampLit(lamp, progress.flags.count(lampKey(room, lamp)) > 0);
        }
    }
}

std::string Game::lampKey(size_t room, size_t lamp) const
{
    return "lamp_" + levelKey(room) + "_" + std::to_string(lamp);
}

// ---------------------------------------------------------------------------------------------
// Map

std::string Game::songNow() const
{
    // Each part of the world has its own; the menus have the title's.
    const bool playing = inGame() && screen != Screen::Complete;
    return playing ? levels[levelIndex].theme : "title";
}

void Game::openMap(bool travel)
{
    mapScreen->open(mapMarks(), travel);
    mapReturn = screen;
    openScreen(Screen::Map);
}

MapScreen::Marks Game::mapMarks() const
{
    MapScreen::Marks marks;
    marks.currentRoom = levelIndex;
    marks.player = (player->centre() + roomOrigin(levelIndex)) / Level::TILE_SIZE;
    marks.explored.resize(levels.size());
    for (size_t room = 0; room < levels.size(); ++room) {
        marks.explored[room] = room == levelIndex || progress.explored.count(levelKey(room)) > 0;
        if (!marks.explored[room]) continue;
        const sf::Vector2f origin(levels[room].position);
        for (const Pickup& pickup : rooms[room]->pickups()) {
            if (!hasAbility(pickup.ability)) marks.pickups.push_back(origin + pickup.position / Level::TILE_SIZE + sf::Vector2f(0.5f, 0.5f));
        }
        for (const Npc& npc : rooms[room]->npcs()) {
            if (isAround(npc)) marks.people.push_back(origin + npc.position / Level::TILE_SIZE + sf::Vector2f(0.5f, 0.5f));
        }
        // With Pip: the rooms with gems still in them, a paw print on each.
        if (hasDog()) {
            for (size_t gem = 0; gem < rooms[room]->gems().size(); ++gem) {
                if (gemFound(room, gem)) continue;
                const sf::FloatRect b = rooms[room]->bounds();
                marks.gemRooms.push_back(origin + sf::Vector2f(b.width, b.height) / Level::TILE_SIZE * 0.5f);
                break;
            }
        }
    }
    for (const LevelInfo& info : levels) marks.regions.push_back(regions::name(info.region));
    for (const std::string& key : travelDestinations()) {
        const auto [room, bench] = *findBench(key);
        if (key == progress.bench) marks.benchHere = marks.benches.size();
        marks.benches.push_back(sf::Vector2f(levels[room].position) + rooms[room]->benches()[bench] / Level::TILE_SIZE + sf::Vector2f(1.f, 0.5f));
        marks.benchRooms.push_back(room);
    }
    marks.describe = [this](size_t room) {
        const std::string tally = gemTally(room);
        const std::string region = regions::name(levels[room].region);
        const std::string title = hud::upper(levels[room].title);
        return (region.empty() || region == title ? "" : region + ": ") + title + (tally.empty() ? "" : "  GEMS " + tally);
    };
    using A = controls::Action;
    const bool pad = input.usingPad();
    if (mapScreen && mapScreen->travelling()) {
        marks.hint = std::string(pad ? "STICK" : "LEFT / RIGHT / CLICK") + " - CHOOSE    " + hint(A::Confirm) + " - GO    " + hint(A::Back) + " - STAY";
    } else {
        marks.hint = std::string(pad ? "STICK" : "ARROWS / DRAG") + " - MOVE    " + hint(A::Zoom) + (pad ? "" : " / WHEEL") + " - ZOOM    " +
                     hint(A::Map) + " - CLOSE";
    }
    return marks;
}

void Game::run()
{
    sf::Clock clock;
    while (window->isOpen()) {
        input.newFrame();
        sf::Event event;
        while (window->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                if (inGame()) saveProgress();
                window->close();
            }
            if (event.type == sf::Event::LostFocus && screen == Screen::Playing) openScreen(Screen::Paused);
            if (auto press = input.handleEvent(event)) handlePress(*press);
        }
        if (reapplyWindow) {
            reapplyWindow = false;
            applyWindowSettings();
        }
        if (!window->isOpen()) break;
        // Cap the frame time so a hitch (e.g. dragging the window) doesn't fast-forward the game.
        frame(std::min(clock.restart().asSeconds(), 0.25f));
        if (window->isOpen()) render();
    }
}

void Game::frame(float frameTime, std::optional<PlayerInput> scripted)
{
    audio.playMusic(songNow());
    updateLoops();
    audio.update(frameTime);
    // The air: what drifts in it moves along with the camera, at its own depth.
    ambience.setTheme(levels[levelIndex].theme);
    ambience.setDark(level->isDark());
    const sf::Vector2f cameraWorld = drawCamera() + roomOrigin(levelIndex);
    const sf::Vector2f moved = cameraWorld - lastCamera;
    lastCamera = cameraWorld;
    ambience.update(frameTime, std::abs(moved.x) + std::abs(moved.y) < 100.f ? sf::Vector2f(moved.x, -moved.y) : sf::Vector2f());

    const sf::Vector2f mouse = input.mousePosition();
    const sf::Vector2f mouseMove = mouse - lastMouse;
    const bool mouseMoved = mouse != lastMouse;
    lastMouse = mouse;
    if (!scripted) {
        if (screen == Screen::Playing) {
            if (input.wasMouseClicked() && pauseButton().contains(mouse)) openScreen(Screen::Paused);
        } else if (screen == Screen::Map) {
            const MapScreen::Marks marks = mapMarks();
            if (mapScreen->update(frameTime, input, mouseMove, marks) == MapScreen::Action::Travel) {
                travelTo(travelDestinations()[mapScreen->destination()]);
            }
        } else if (screen == Screen::Talking) {
            talk->pointAt(mouse, input.wasMouseClicked());
            if (talk->finished()) endTalk();
        } else if (rebinding) {
            if (input.wasMouseClicked()) rebinding.reset(); // clicking away: never mind
        } else {
            const size_t before = menu.selected();
            if (auto choice = menu.handleMouse(mouse, mouseMoved, input.wasMouseClicked())) {
                audio.play(Audio::Sfx::MenuPick);
                onMenuChoice(*choice);
            } else if (menu.selected() != before) {
                audio.play(Audio::Sfx::MenuMove);
            }
        }
    }

    // Menus leave the player alone, so a rope held when pausing is still held after.
    if (screen == Screen::Playing) {
        PlayerInput in;
        if (!transition && deathTimer <= 0.f && !ignoreInputThisFrame) {
            in = scripted ? *scripted : playerInput();
            if (ignorePressesThisFrame) in.jumpPressed = in.ropePressed = false;
            // On a bench (or coming to), moving gets you up; anything else leaves you sitting.
            if (sitting || waking) {
                if (in.moveX != 0.f || in.jumpPressed || in.ropePressed) {
                    sitting.reset();
                    waking = false;
                } else {
                    in = {};
                }
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
}

void Game::pressKey(sf::Keyboard::Key key)
{
    sf::Event event{};
    event.type = sf::Event::KeyPressed;
    event.key.code = key;
    if (auto press = input.handleEvent(event)) handlePress(*press);
    event.type = sf::Event::KeyReleased;
    input.handleEvent(event);
}

void Game::pressButton(int button)
{
    sf::Event event{};
    event.type = sf::Event::JoystickButtonPressed;
    event.joystickButton.button = static_cast<unsigned>(button);
    if (auto press = input.handleEvent(event)) handlePress(*press);
    event.type = sf::Event::JoystickButtonReleased;
    input.handleEvent(event);
}

void Game::advance(float seconds, const PlayerInput& held)
{
    // In frames of a 60th of a second, like the real thing. Presses only count once.
    PlayerInput in = held;
    for (float t = 0.f; t < seconds; t += 1.f / 60.f) {
        frame(1.f / 60.f, in);
        in.jumpPressed = in.ropePressed = false;
    }
}

sf::Image Game::screenshot()
{
    renderCanvas();
    return canvas.getTexture().copyToImage();
}

const sf::Texture& Game::play(float frameTime, PlayerInput input)
{
    playedMouse = input.aim;
    input.aim = worldCoordinates(input.aim, drawCamera());
    frame(frameTime, input);
    renderCanvas();
    return canvas.getTexture();
}

std::string Game::screenName() const
{
    switch (screen) {
        case Screen::Title: return "title";
        case Screen::Slots: return "slots";
        case Screen::ConfirmErase: return "erase";
        case Screen::Options: return "options";
        case Screen::Controls: return "controls";
        case Screen::Playing: return "playing";
        case Screen::Paused: return "paused";
        case Screen::Complete: return "complete";
        case Screen::Map: return "map";
        case Screen::Talking: return "talking";
        case Screen::Shop: return "shop";
        case Screen::Records: return "records";
    }
    return "";
}

std::string Game::roomName() const
{
    return levelKey(levelIndex);
}

const Progress& Game::state() const
{
    return progress;
}

bool Game::isSitting() const
{
    return sitting.has_value();
}

sf::Vector2f Game::playerPosition() const
{
    return player->getPosition();
}

void Game::handlePress(const InputManager::Press& press)
{
    using A = controls::Action;
    const auto has = [&](A action) { return press.actions[controls::index(action)]; };
    if (rebinding) {
        rebind(press);
        return;
    }
    if (press.key == sf::Keyboard::F5) {
        reloadPhysics();
        return;
    }
    if (screen == Screen::Playing) {
        const bool interact = has(A::Interact) || has(A::Up);
        if (has(A::Pause)) openScreen(Screen::Paused);
        else if (has(A::Map)) openMap(sitting && travelDestinations().size() > 1);
        else if (interact && npcInReach()) startTalk(*npcInReach());
        else if (interact && !sitting && benchInReach()) sitDown(*benchInReach());
        else if (interact && leverInReach()) pullLever();
        else if (has(A::Restart) && !transition && deathTimer <= 0.f) killPlayer(); // back to the checkpoint
        else if (testing && press.key >= sf::Keyboard::Num1 && press.key <= sf::Keyboard::Num9) toggleAbility(size_t(press.key - sf::Keyboard::Num1));
        return;
    }
    if (screen == Screen::Talking) {
        if (has(A::Pause)) back();
        else if (has(A::Up)) talk->moveChoice(-1);
        else if (has(A::Down)) talk->moveChoice(1);
        else if (has(A::Confirm) || has(A::Jump) || has(A::Interact)) talk->advance();
        if (talk && talk->finished()) endTalk();
        return;
    }
    if (screen == Screen::Map) {
        switch (mapScreen->handle(press.actions, mapMarks())) {
            case MapScreen::Action::Close: back(); break;
            case MapScreen::Action::Travel: travelTo(travelDestinations()[mapScreen->destination()]); break;
            case MapScreen::Action::None: break;
        }
        return;
    }
    if (has(A::Back) || (has(A::Pause) && screen == Screen::Paused)) {
        back();
        return;
    }
    const size_t before = menu.selected();
    if (auto choice = menu.handle(press.actions)) {
        audio.play(Audio::Sfx::MenuPick);
        onMenuChoice(*choice);
    } else if (menu.selected() != before) {
        audio.play(Audio::Sfx::MenuMove);
    }
}

PlayerInput Game::playerInput()
{
    using A = controls::Action;
    const auto held = [this](A action) { return input.isHeld(action) ? 1.f : 0.f; };

    PlayerInput in;
    in.moveX = held(A::Right) - held(A::Left);
    in.moveY = held(A::Up) - held(A::Down);
    in.jumpPressed = input.wasPressed(A::Jump);
    in.jumpHeld = input.isHeld(A::Jump);
    // The rope's key works like the mouse button, for trackpads that can't hold a click.
    in.ropePressed = input.wasMouseClicked() || input.wasPressed(A::Rope);
    in.ropeHeld = input.isMousePressed() || input.isHeld(A::Rope);
    in.aim = aimPoint();
    return in;
}

void Game::reloadPhysics()
{
    const std::vector<std::string> problems = loadPhysics(config::assetPath("physics.txt"));
    for (const std::string& problem : problems) std::cerr << "physics.txt: " << problem << std::endl;
    applyAbilities(); // the rope's reach
    showNotice(problems.empty() ? "PHYSICS RELOADED" : "PHYSICS.TXT: " + hud::upper(problems.front()));
}

sf::Vector2f Game::aimPoint() const
{
    if (playedMouse) return worldCoordinates(*playedMouse, drawCamera());
    if (!input.usingPad()) return worldCoordinates(input.mousePosition(), drawCamera());
    // Where the stick points, or with it let go, up and ahead.
    sf::Vector2f direction = input.padDirection();
    if (direction == sf::Vector2f()) direction = {player->isFacingLeft() ? -1.f : 1.f, 1.f};
    return player->aimAlong(direction);
}

std::string Game::hint(controls::Action action) const
{
    using A = controls::Action;
    using controls::PadButton;
    const controls::Bindings& b = settings.controls;
    if (input.usingPad()) {
        const controls::PadLayout layout = input.padLayout();
        switch (action) {
            case A::Left:
            case A::Right:
            case A::Up:
            case A::Down: return "STICK";
            case A::Pause: return controls::buttonName(controls::buttonNumber(PadButton::Start, layout), layout);
            case A::Confirm: return controls::buttonName(controls::buttonNumber(PadButton::South, layout), layout);
            case A::Back: return controls::buttonName(controls::buttonNumber(PadButton::East, layout), layout);
            case A::Zoom:
                return controls::buttonName(controls::buttonNumber(PadButton::LeftShoulder, layout), layout) + " / " +
                       controls::buttonName(controls::buttonNumber(PadButton::RightShoulder, layout), layout);
            default: return controls::buttonName(b.button(action, layout), layout);
        }
    }
    switch (action) {
        case A::Pause:
        case A::Back: return "ESC";
        case A::Confirm: return "ENTER";
        case A::Zoom: return "Z";
        default: return controls::keyName(b.keys[controls::index(action)][0]);
    }
}

sf::FloatRect Game::pauseButton() const
{
    return {config::GAME_WIDTH - 13.f, 1.f, 12.f, 11.f};
}

void Game::step(float dt)
{
    clockTime += dt;
    if (screen == Screen::Complete) {
        creditsTime += dt;
        // Confetti rains down while the results are up, about 40 bits a second.
        std::uniform_real_distribution<float> unit(0.f, 1.f);
        if (unit(rng) < dt * 40.f) {
            const sf::Vector2f top = worldCoordinates({unit(rng) * config::GAME_WIDTH, -2.f}, drawCamera());
            particles.spawn(top, {.count = 1, .speed = 20.f, .spread = 1.f, .direction = -1.5708f, .life = 3.5f, .gravity = 500.f, .size = 2.f,
                                  .colors = CONFETTI});
        }
        particles.update(dt);
    }
    if (screen == Screen::Talking) {
        talk->update(dt);
        // A blip every couple of letters, in the speaker's own voice.
        talkBlips += static_cast<float>(talk->lettersTyped());
        if (talkBlips >= 2.f) {
            talkBlips = 0.f;
            const size_t voice = std::hash<std::string>{}(talk->speaker()) % 7;
            audio.play(Audio::Sfx::Talk, 0.75f + 0.12f * static_cast<float>(voice));
        }
    }
    if (screen != Screen::Playing) return;

    deathFlash = std::max(0.f, deathFlash - dt);
    shake = std::max(0.f, shake - dt);
    titleTimer = std::max(0.f, titleTimer - dt);
    messageTimer = std::max(0.f, messageTimer - dt);
    noticeTimer = std::max(0.f, noticeTimer - dt);
    fade = std::max(0.f, fade - dt);
    progress.playTime += dt;
    if (timedRun) timedRun->time += dt; // dying and all
    runShown = std::max(0.f, runShown - dt);
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
            level->reset(); // icicles back, crumbled blocks whole again
            player->respawn(checkpoint);
            if (checkpointRiding) player->startRiding(checkpointRiding);
            dog.reset(checkpoint + sf::Vector2f(Player::WIDTH * 0.5f, 0.f));
            audio.play(Audio::Sfx::Respawn);
            particles.spawn(player->centre(), {.count = 12, .speed = 30.f, .life = 0.4f, .gravity = 0.f,
                                               .colors = {sf::Color(255, 255, 255, 200), sf::Color(200, 220, 255, 200)}});
        }
        updateCamera(dt);
        return;
    }

    level->update(dt, player->hitbox());
    for (const Prop& prop : level->props()) {
        // Spray drifting up where a waterfall lands.
        if (prop.kind != props::WATERFALL || std::uniform_real_distribution<float>(0.f, 1.f)(rng) > dt * 30.f) continue;
        const int x = static_cast<int>(prop.position.x / Level::TILE_SIZE), y = static_cast<int>(prop.position.y / Level::TILE_SIZE);
        const sf::Vector2f foot(prop.position.x + Level::TILE_SIZE + std::uniform_real_distribution<float>(-8.f, 8.f)(rng),
                                static_cast<float>(fallFloor(*level, x, y, true)) * Level::TILE_SIZE);
        particles.spawn(foot, {.count = 1, .speed = 22.f, .spread = 1.8f, .life = 1.1f, .gravity = -6.f,
                               .colors = {sf::Color(220, 245, 255, 140), sf::Color(150, 225, 245, 120)}});
    }
    playLevelSounds();
    player->update(dt);
    showEffects(player->takeEvents());

    const bool fellOut = player->getPosition().y < level->bounds().top - FALL_OUT_MARGIN;
    if (player->isOutOfAir()) {
        // Drowned: the last of your breath bubbling up.
        particles.spawn(player->centre(), {.count = 14, .speed = 25.f, .spread = 1.2f, .direction = 1.5708f, .life = 0.9f, .gravity = -60.f,
                                           .colors = {sf::Color(220, 245, 255, 220), sf::Color(150, 220, 245, 200)}});
    }
    if (player->hasCrashed()) {
        // A cart into a wall: splinters.
        particles.spawn(player->centre(), {.count = 20, .speed = 80.f, .life = 0.6f, .gravity = 300.f, .size = 2.f,
                                           .colors = {sf::Color(140, 100, 70), sf::Color(90, 70, 60), sf::Color(170, 170, 180)}});
        audio.play(Audio::Sfx::Crash);
    }
    if (player->isTouchingDanger() || player->isOutOfAir() || fellOut || player->hasCrashed()) {
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
        checkpointRiding = player->ridingDir();
        needsSpawnPoint = false;
    }
    updateCart();
    updateTrial();
    updateDog(dt);
    if (const Pickup* pickup = level->pickupAt(player->getPosition(), Player::HEIGHT, Player::WIDTH); pickup && !hasAbility(pickup->ability)) {
        collect(*pickup);
    }
    if (auto gem = level->gemAt(player->getPosition(), Player::HEIGHT, Player::WIDTH); gem && !gemFound(levelIndex, *gem)) {
        progress.gems.insert(gemKey(levelIndex, *gem));
        saveProgress();
        showNotice(gemTotal());
        audio.play(Audio::Sfx::Gem);
        particles.spawn(level->gems()[*gem] + sf::Vector2f(4.f, 4.f), {.count = 20, .speed = 55.f, .life = 0.6f, .gravity = 60.f,
                                                                       .colors = {hud::CYAN, sf::Color(234, 255, 255), sf::Color(42, 143, 176)}});
    }
    if (auto cp = level->checkpointAt(player->getPosition(), Player::HEIGHT, Player::WIDTH); cp && *cp != checkpoint) {
        checkpoint = *cp;
        checkpointRiding = player->ridingDir();
        audio.play(Audio::Sfx::Checkpoint);
        needsSpawnPoint = false;
        particles.spawn(checkpoint + sf::Vector2f(4.f, 16.f), {.count = 16, .speed = 50.f, .spread = 2.f, .life = 0.6f, .gravity = 60.f,
                                                              .colors = {sf::Color(126, 240, 142), sf::Color(220, 255, 220)}});
    }

    updateCamera(dt);
}

void Game::killPlayer()
{
    ++progress.deaths;
    audio.play(Audio::Sfx::Death);
    sitting.reset();
    deathTimer = DEATH_TIME;
    deathFlash = DEATH_TIME;
    shake = SHAKE_TIME;
    particles.spawn(player->centre(), {.count = 36, .speed = 100.f, .life = 0.7f, .gravity = 150.f, .size = 2.f,
                                       .colors = {sf::Color(251, 242, 54), sf::Color(153, 229, 80), sf::Color(215, 123, 186)}});
}

void Game::playLevelSounds()
{
    constexpr float HEARING = 24.f * Level::TILE_SIZE; // further than this, nothing
    for (const LevelSound& sound : level->takeSounds()) {
        const float volume = 1.f - distance(sound.position, player->centre()) / HEARING;
        if (volume <= 0.f) continue;
        const float pitch = 0.9f + 0.2f * static_cast<float>(std::rand() % 100) / 100.f;
        switch (sound.kind) {
            case LevelSound::Kind::Crack: audio.play(Audio::Sfx::Crack, pitch, volume); break;
            case LevelSound::Kind::Crumble:
                audio.play(Audio::Sfx::Crumble, pitch, volume);
                particles.spawn(sound.position, {.count = 8, .speed = 25.f, .life = 0.6f, .gravity = 200.f,
                                                 .colors = {sf::Color(120, 100, 90), sf::Color(80, 70, 65)}});
                break;
            case LevelSound::Kind::CloudFade: audio.play(Audio::Sfx::CloudPuff, pitch, volume); break;
            case LevelSound::Kind::IcicleShake: audio.play(Audio::Sfx::IcicleShake, pitch, volume); break;
            case LevelSound::Kind::IcicleShatter:
                audio.play(Audio::Sfx::Shatter, pitch, volume);
                particles.spawn(sound.position, {.count = 10, .speed = 50.f, .life = 0.4f, .gravity = 250.f,
                                                 .colors = {sf::Color(220, 240, 255), sf::Color(160, 200, 240)}});
                break;
        }
    }
}

void Game::updateLoops()
{
    const bool playing = screen == Screen::Playing || screen == Screen::Talking;
    // The wind: as strong as the room's, and a breeze anywhere up in the sky.
    float wind = std::min(1.f, std::abs(level->wind()) / 100.f);
    if (levels[levelIndex].theme == "sky") wind = std::max(wind, 0.2f);
    audio.setLoop(Audio::Loop::Wind, playing ? wind : 0.f);
    // The heat: loud in an updraft, fading over a few tiles away from one.
    float heat = 0.f;
    const sf::Vector2f you = player->centre();
    for (const sf::FloatRect& area : level->updrafts()) {
        const float dx = std::max({0.f, area.left - you.x, you.x - (area.left + area.width)});
        const float dy = std::max({0.f, area.top - you.y, you.y - (area.top + area.height)});
        heat = std::max(heat, 1.f - std::hypot(dx, dy) / (8.f * Level::TILE_SIZE));
    }
    audio.setLoop(Audio::Loop::Heat, playing ? heat * 0.8f : 0.f);
    // Falling water: loud close by, heard across the room.
    float water = 0.f;
    for (const Prop& prop : level->props()) {
        if (prop.kind != props::WATERFALL) continue;
        const int x = static_cast<int>(prop.position.x / Level::TILE_SIZE), y = static_cast<int>(prop.position.y / Level::TILE_SIZE);
        const float bottom = static_cast<float>(fallFloor(*level, x, y, true)) * Level::TILE_SIZE;
        const sf::Vector2f nearest(prop.position.x + Level::TILE_SIZE, std::clamp(you.y, bottom, prop.position.y + Level::TILE_SIZE));
        water = std::max(water, 1.f - distance(nearest, you) / (30.f * Level::TILE_SIZE));
    }
    audio.setLoop(Audio::Loop::Water, playing ? water * 0.7f : 0.f);
    // A cart rolling along.
    audio.setLoop(Audio::Loop::Rumble, playing && player->isRiding() && player->isGrounded() ? 0.6f : 0.f);
}

void Game::showEffects(const std::vector<PlayerEvent>& events)
{
    const std::vector<sf::Color> dust = {sf::Color(210, 200, 185, 220), sf::Color(160, 150, 140, 200)};
    const std::vector<sf::Color> sparks = {sf::Color(255, 255, 255), sf::Color(255, 230, 140)};
    for (const auto& e : events) {
        switch (e.type) {
            case PlayerEvent::Type::Jumped:
                audio.play(Audio::Sfx::Jump);
                particles.spawn(e.position, {.count = 6, .speed = 30.f, .spread = 2.5f, .life = 0.35f, .gravity = 20.f, .colors = dust});
                break;
            case PlayerEvent::Type::AirJumped:
                audio.play(Audio::Sfx::AirJump);
                // A puff of air pushed down under the feet.
                particles.spawn(e.position, {.count = 10, .speed = 45.f, .spread = 1.2f, .direction = -1.5708f, .life = 0.4f, .gravity = -20.f,
                                             .colors = {sf::Color(255, 255, 255, 220), sf::Color(200, 230, 255, 200)}});
                break;
            case PlayerEvent::Type::Landed: {
                if (e.strength > 120.f) audio.play(Audio::Sfx::Land, 0.8f + std::min(0.4f, e.strength / 1000.f));
                int count = 3 + static_cast<int>(e.strength / 60.f);
                float speed = 20.f + e.strength * 0.1f;
                particles.spawn(e.position, {.count = count, .speed = speed, .spread = 0.6f, .direction = 0.f, .life = 0.4f, .gravity = 10.f, .colors = dust});
                particles.spawn(e.position, {.count = count, .speed = speed, .spread = 0.6f, .direction = 3.1416f, .life = 0.4f, .gravity = 10.f, .colors = dust});
                break;
            }
            case PlayerEvent::Type::Stepped:
                particles.spawn(e.position, {.count = 2, .speed = 12.f, .spread = 1.5f, .life = 0.3f, .gravity = 10.f, .colors = dust});
                break;
            case PlayerEvent::Type::WallJumped:
                audio.play(Audio::Sfx::Jump, 1.1f);
                particles.spawn(e.position, {.count = 6, .speed = 30.f, .spread = 2.5f, .life = 0.3f, .gravity = 20.f, .colors = dust});
                break;
            case PlayerEvent::Type::Grabbed:
                audio.play(Audio::Sfx::Grab);
                particles.spawn(e.position, {.count = 3, .speed = 15.f, .spread = 3.f, .life = 0.25f, .gravity = 40.f, .colors = dust});
                break;
            case PlayerEvent::Type::RopeThrown:
                audio.play(Audio::Sfx::Throw);
                break;
            case PlayerEvent::Type::RopeAttached:
                audio.play(Audio::Sfx::Hooked);
                particles.spawn(e.position, {.count = 8, .speed = 50.f, .life = 0.25f, .gravity = 100.f, .colors = sparks});
                break;
            case PlayerEvent::Type::RopeWrapped:
                particles.spawn(e.position, {.count = 4, .speed = 30.f, .life = 0.2f, .gravity = 100.f, .colors = sparks});
                break;
            case PlayerEvent::Type::LeverTugged:
                audio.play(Audio::Sfx::RopeClink, 0.7f);
                if (!level->gatesOpen()) pullLever();
                break;
            case PlayerEvent::Type::LampTugged:
                if (e.index >= 0 && !level->lamps()[static_cast<size_t>(e.index)].lit) {
                    level->setLampLit(static_cast<size_t>(e.index), true);
                    progress.flags.insert(lampKey(levelIndex, static_cast<size_t>(e.index)));
                    saveProgress();
                    audio.play(Audio::Sfx::LampLit);
                    particles.spawn(e.position, {.count = 14, .speed = 40.f, .life = 0.5f, .gravity = -20.f, .colors = sparks});
                }
                break;
            case PlayerEvent::Type::Splashed: {
                // Into the water, or leaping out of it: a spray, bigger the harder.
                const float hard = std::min(1.f, e.strength / 300.f);
                audio.play(Audio::Sfx::Splash, 1.1f - 0.3f * hard, 0.4f + 0.6f * hard);
                const std::vector<sf::Color> spray = {sf::Color(200, 245, 255, 230), sf::Color(110, 210, 240, 210), sf::Color(255, 255, 255, 240)};
                particles.spawn(e.position, {.count = 6 + static_cast<int>(16.f * hard), .speed = 40.f + 60.f * hard, .spread = 1.4f, .life = 0.5f,
                                             .gravity = 300.f, .colors = spray});
                break;
            }
            case PlayerEvent::Type::Stroked:
                audio.play(Audio::Sfx::Stroke, 0.9f + 0.2f * static_cast<float>(std::rand() % 100) / 100.f);
                particles.spawn(e.position, {.count = 3, .speed = 18.f, .spread = 2.f, .life = 0.35f, .gravity = 120.f,
                                             .colors = {sf::Color(200, 245, 255, 200), sf::Color(120, 210, 240, 180)}});
                break;
            case PlayerEvent::Type::Bubbled:
                // Breath escaping, rising to the surface.
                audio.play(Audio::Sfx::Bubble, 0.8f + 0.4f * static_cast<float>(std::rand() % 100) / 100.f, 0.6f);
                particles.spawn(e.position, {.count = 2, .speed = 10.f, .spread = 0.8f, .direction = 1.5708f, .life = 0.8f, .gravity = -50.f,
                                             .colors = {sf::Color(220, 245, 255, 210), sf::Color(160, 225, 250, 190)}});
                break;
            case PlayerEvent::Type::Gasped:
                audio.play(Audio::Sfx::Gasp);
                break;
            case PlayerEvent::Type::Bounced:
                audio.play(Audio::Sfx::Bounce, 0.9f + 0.2f * static_cast<float>(std::rand() % 100) / 100.f);
                particles.spawn(e.position, {.count = 8, .speed = 40.f, .spread = 1.4f, .life = 0.4f, .gravity = 60.f,
                                             .colors = {sf::Color(255, 190, 220, 220), sf::Color(240, 140, 190, 200)}});
                break;
            case PlayerEvent::Type::Pushed:
                // Floating: a little puff of starlight behind you.
                particles.spawn(e.position, {.count = 2, .speed = 10.f, .life = 0.6f, .gravity = 0.f,
                                             .colors = {sf::Color(200, 210, 255, 160), sf::Color(255, 255, 255, 120)}});
                break;
            case PlayerEvent::Type::RopeBounced:
                audio.play(Audio::Sfx::RopeClink);
                particles.spawn(e.position, {.count = 5, .speed = 40.f, .life = 0.25f, .gravity = 200.f, .colors = dust});
                // The first time off slick stone, say why.
                if (!shownRockHint && touchesSlick(e.position)) {
                    shownRockHint = true;
                    showMessage("CLINK!", "NOTHING HOLDS ON SLICK STONE");
                }
                break;
        }
    }
}

void Game::updateCamera(float dt, bool snap)
{
    const sf::Vector2f you = player->centre();
    if (snap) {
        lookAhead = 0.f;
        standingY = you.y;
    }
    const float ahead = std::clamp(player->getVelocity().x * LOOK_AHEAD_TIME, -LOOK_AHEAD, LOOK_AHEAD);
    lookAhead += (ahead - lookAhead) * (1.f - std::exp(-LOOK_AHEAD_FOLLOW * dt));
    if (player->isGrounded() || player->isClinging()) standingY = you.y;
    const float y = std::clamp(standingY, you.y - RISE_ROOM, you.y + DROP_ROOM);

    const sf::Vector2f target(you.x + lookAhead, y);
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
