#include "Game.h"
#include "src/GameShared.h"
#include "src/Catalog.h"
#include "src/Hud.h"
#include "src/Physics.h"
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
        {"rope", "ASH'S ROPE", "HOLD TO THROW IT, STANDING. KEEP HOLDING TO SWING"},
        {"long_rope", "LONG ROPE", "YOUR ROPE REACHES FURTHER"},
        {"wall_climb", "ASH'S CHALK BAG", "PUSH INTO A WALL TO GRAB IT. UP CLIMBS, JUMP LEAPS OFF"},
        {"air_throw", "THROWING GLOVE", "THROW THE ROPE IN MID-AIR TOO"},
        {"double_jump", "UPDRAFT FEATHER", "JUMP AGAIN IN THE AIR"},
    };
}

Game::Game(std::vector<LevelInfo> levelList, Options options)
    : levels(std::move(levelList)),
      testing(options.testing),
      headless(options.headless),
      files(options.saveFolder.empty() ? SaveFiles::defaultFolder() : options.saveFolder),
      settings(files.loadSettings()),
      window(std::make_shared<sf::RenderWindow>()),
      input(window)
{
    const size_t startRoom = options.startRoom;
    input.setBindings(settings.controls);
    if (!headless) applyWindowSettings();
    audio.setVolumes(headless ? 0 : settings.musicVolume, headless ? 0 : settings.soundVolume);
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
    // From before the rope was found at the bottom of the crevasse: past it, you have it.
    if (progress.explored.count("level1")) progress.abilities.insert("rope");
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
    waking = false;
    level->reset(); // icicles back, crumbled blocks whole again
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
    level->reset();
    player->enterLevel(level, shift, side == Level::Side::Top);
    particles.clear();

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
    saveProgress();
}

void Game::finishGame()
{
    // The ending that fits the story so far: assets/dialogue/endings.txt.
    ending.clear();
    try {
        const Dialogue endings = Dialogue::load("endings");
        for (const Conversation& c : endings.conversations()) {
            if (!Dialogue::allowed(c, [this](const std::string& f) { return hasFact(f); })) continue;
            for (const DialogueStep& step : c.steps) {
                if (step.kind == DialogueStep::Kind::Line) ending.push_back(step.text);
            }
            break;
        }
    } catch (const std::exception&) {
        // No endings: just the time.
    }
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
    if (progress.items.count(fact)) return true;
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
    if (transition || deathTimer > 0.f || player->getRope() || !player->isGrounded()) return nullptr;
    for (const Npc& npc : level->npcs()) {
        const sf::Vector2f d = player->centre() - (npc.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, 6.f));
        if (!npc.dialogue.empty() && std::abs(d.x) <= TALK_REACH && std::abs(d.y) <= TALK_REACH) return &npc;
    }
    return nullptr;
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
    } catch (const std::exception&) {
        return; // nothing to say; game_tests checks every character's file is there
    }
    openScreen(Screen::Talking);
}

void Game::endTalk()
{
    progress.conversations.insert(talk->key());
    saveProgress();
    talk.reset();
    openScreen(Screen::Playing);
    // What the conversation asked for, now it's over.
    if (pendingShop) {
        pendingShop = false;
        openScreen(Screen::Shop);
    }
    if (pendingRace) {
        startRace(*pendingRace);
        pendingRace.reset();
    }
}

void Game::act(const std::string& command)
{
    std::istringstream words(command);
    std::string verb, what, name;
    words >> verb >> what >> name;
    if (verb == "shop") {
        pendingShop = true;
    } else if (verb == "race") {
        pendingRace = what;
    } else if (verb == "give" && what == "gems") {
        const int gems = std::atoi(name.c_str());
        progress.spent -= gems;
        showNotice("+" + std::to_string(gems) + " GEMS");
        audio.play(Audio::Sfx::Gem);
    } else if (verb == "give" && what == "outfit") {
        progress.outfits.insert(name);
        const catalog::Outfit* outfit = catalog::outfit(name);
        showMessage("A NEW OUTFIT", (outfit ? outfit->name : hud::upper(name)) + ": WEAR IT FROM THE PAUSE MENU, OPTIONS");
        audio.play(Audio::Sfx::Pickup);
    } else if (verb == "give" && what == "item") {
        progress.items.insert(name);
    } else if (verb == "take" && what == "item") {
        progress.items.erase(name);
    }
    saveProgress();
}

int Game::gemsFound() const
{
    int found = 0;
    for (size_t room = 0; room < rooms.size(); ++room) {
        for (size_t gem = 0; gem < rooms[room]->gems().size(); ++gem) found += gemFound(room, gem) ? 1 : 0;
    }
    return found;
}

int Game::money() const
{
    return gemsFound() - progress.spent;
}

std::vector<std::string> Game::ownedOutfits() const
{
    std::vector<std::string> owned = {"classic"};
    for (const catalog::Outfit& outfit : catalog::outfits()) {
        if (outfit.look != "classic" && progress.outfits.count(outfit.look)) owned.push_back(outfit.look);
    }
    return owned;
}

void Game::wear(const std::string& look)
{
    progress.wearing = look;
    settings.skin = look; // and on the title screen
    player->setSkin(Player::lookNamed(look));
    saveProgress();
    saveSettings();
}

void Game::startRace(const std::string& id)
{
    const catalog::Race* info = catalog::race(id);
    if (!info) return;
    race = Race{id, info->seconds};
    showMessage("GO!", "GET TO THE FINISH IN " + std::to_string(static_cast<int>(info->seconds)) + " SECONDS");
    audio.play(Audio::Sfx::Checkpoint);
}

void Game::checkQuestItemsAndRaces(float dt)
{
    const sf::FloatRect box = player->hitbox();
    if (auto found = level->questItemAt(box)) {
        const std::string& id = level->questItems()[*found].item;
        if (!progress.flags.count("got_" + id)) {
            progress.flags.insert("got_" + id);
            progress.items.insert(id);
            const catalog::Item* info = catalog::item(id);
            showMessage(info ? info->name : hud::upper(id), info ? info->text : "");
            audio.play(Audio::Sfx::Pickup);
            saveProgress();
        }
    }
    if (!race) return;
    race->left -= dt;
    if (auto finish = level->finishAt(box); finish && level->finishes()[*finish].race == race->id) {
        progress.flags.insert("won_" + race->id);
        showMessage("YOU WON!", "WITH " + std::to_string(static_cast<int>(std::ceil(race->left))) + " SECONDS TO SPARE");
        audio.play(Audio::Sfx::Pickup);
        race.reset();
        saveProgress();
    } else if (race->left <= 0.f) {
        showMessage("TOO SLOW!", "TALK TO THEM AGAIN TO HAVE ANOTHER GO");
        audio.play(Audio::Sfx::Death);
        race.reset();
    }
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
    level->reset(); // icicles back, crumbled blocks whole again
    player->respawn(checkpoint);
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
        for (const Pickup& pickup : rooms[room]->pickups()) {
            if (!hasAbility(pickup.ability)) marks.pickups.push_back(sf::Vector2f(levels[room].position) + pickup.position / Level::TILE_SIZE + sf::Vector2f(0.5f, 0.5f));
        }
    }
    for (const std::string& key : travelDestinations()) {
        const auto [room, bench] = *findBench(key);
        if (key == progress.bench) marks.benchHere = marks.benches.size();
        marks.benches.push_back(sf::Vector2f(levels[room].position) + rooms[room]->benches()[bench] / Level::TILE_SIZE + sf::Vector2f(1.f, 0.5f));
        marks.benchRooms.push_back(room);
    }
    marks.describe = [this](size_t room) {
        const std::string tally = gemTally(room);
        return hud::upper(levels[room].title) + (tally.empty() ? "" : "  GEMS " + tally);
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

std::string Game::screenName() const
{
    switch (screen) {
        case Screen::Title: return "title";
        case Screen::Slots: return "slots";
        case Screen::ConfirmErase: return "erase";
        case Screen::Options: return "options";
        case Screen::Controls: return "controls";
        case Screen::Shop: return "shop";
        case Screen::Quests: return "quests";
        case Screen::Playing: return "playing";
        case Screen::Paused: return "paused";
        case Screen::Complete: return "complete";
        case Screen::Map: return "map";
        case Screen::Talking: return "talking";
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
            audio.play(Audio::Sfx::Respawn);
            particles.spawn(player->centre(), {.count = 12, .speed = 30.f, .life = 0.4f, .gravity = 0.f,
                                               .colors = {sf::Color(255, 255, 255, 200), sf::Color(200, 220, 255, 200)}});
        }
        updateCamera(dt);
        return;
    }

    level->update(dt, player->hitbox());
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
    checkQuestItemsAndRaces(dt);
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
                    audio.play(Audio::Sfx::Checkpoint);
                    particles.spawn(e.position, {.count = 14, .speed = 40.f, .life = 0.5f, .gravity = -20.f, .colors = sparks});
                }
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
