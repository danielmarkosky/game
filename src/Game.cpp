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
    constexpr float TRANSITION_TIME = 0.5f; // sliding from one level to the next
    constexpr float FALL_OUT_MARGIN = 64.f; // how far below the level counts as falling out
    const char* const TITLE = "Celeste-like Game";

    const sf::Color WHITE(235, 235, 245);
    const sf::Color GOLD(255, 210, 60);
    const sf::Color RED(255, 90, 90);
    const sf::Color GREY(150, 150, 170);
    const std::vector<sf::Color> CONFETTI = {GOLD, sf::Color(255, 120, 160), sf::Color(120, 220, 255), sf::Color(160, 240, 120)};

    // Menu item ids.
    enum Choice
    {
        CONTINUE, NEW_GAME, LEVELS, OPTIONS, QUIT,
        RESUME, RESTART, MAIN_MENU, PLAY_AGAIN, BACK,
        WINDOW_SIZE, FULLSCREEN, SCREEN_SHAKE, TIMER,
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
    for (std::string line; std::getline(file, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream words(line);
        LevelInfo info;
        words >> info.path;
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
    bgFar.loadFromFile(config::assetPath("gfx/bg_far.png"));
    bgNear.loadFromFile(config::assetPath("gfx/bg_near.png"));
    bgFar.setRepeated(true);
    bgNear.setRepeated(true);

    if (testing) {
        startGame(0);
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
    if (next == Screen::Playing && screen == Screen::Paused) ignorePressesThisFrame = true;
    screen = next;

    switch (next) {
        case Screen::Title: menu.top = 92.f; menu.maxVisible = 9; break;
        case Screen::LevelSelect: menu.top = 36.f; menu.maxVisible = 12; break;
        case Screen::Options: menu.top = 60.f; menu.maxVisible = 9; break;
        case Screen::Paused: menu.top = 78.f; menu.maxVisible = 9; break;
        case Screen::Complete: menu.top = 132.f; menu.maxVisible = 9; break;
        case Screen::Playing: break;
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
                const std::string best = record ? hud::formatTime(record->bestTime) + "  x" + std::to_string(record->fewestDeaths) : "-";
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
            items.push_back({BACK, "BACK", " ", false});
            break;
        }
        case Screen::Paused:
            items.push_back(button(RESUME, "RESUME"));
            items.push_back(button(RESTART, "RESTART LEVEL"));
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
            break;
    }
    menu.setItems(std::move(items));
}

void Game::onMenuChoice(MenuChoice choice)
{
    switch (choice.id) {
        case CONTINUE: startGame(continueIndex().value_or(0)); break;
        case NEW_GAME: startGame(0); break;
        case LEVELS: openScreen(Screen::LevelSelect); break;
        case OPTIONS: openScreen(Screen::Options); break;
        case QUIT: window->close(); break;
        case RESUME: openScreen(Screen::Playing); break;
        case RESTART:
            restartLevel();
            openScreen(Screen::Playing);
            break;
        case MAIN_MENU: openScreen(Screen::Title); break;
        case PLAY_AGAIN: startGame(firstLevelPlayed); break;
        case BACK: back(); break;
        case WINDOW_SIZE:
        case FULLSCREEN:
        case SCREEN_SHAKE:
        case TIMER:
            // Clicking or pressing Enter on a setting steps it forwards.
            changeSetting(choice.id, choice.direction == 0 ? 1 : choice.direction);
            break;
        default:
            if (choice.id >= FIRST_LEVEL) startGame(static_cast<size_t>(choice.id - FIRST_LEVEL));
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
        case Screen::Complete: return true;
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

void Game::startGame(size_t index)
{
    totalTime = 0.f;
    totalDeaths = 0;
    firstLevelPlayed = index;
    lastResult.clear();
    worldOffsetX = 0.f;
    loadLevel(index);
    if (!testing) {
        save.continueLevel = levelKey(index);
        writeSave();
    }
    openScreen(Screen::Playing);
}

void Game::loadLevel(size_t index)
{
    levelIndex = index;
    level = Level::loadFromFile(levels[index].path);
    player = std::make_unique<Player>(level);
    restartLevel();
    titleTimer = TITLE_TIME;
}

void Game::restartLevel()
{
    checkpoint = level->getStartPos();
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

void Game::finishLevel(std::optional<Level::Side> side)
{
    totalTime += runTime;
    totalDeaths += deaths;
    if (!testing) {
        const LevelRecord* old = save.record(levelKey(levelIndex));
        const bool best = old && (runTime < old->bestTime || deaths < old->fewestDeaths);
        lastResult = hud::formatTime(runTime) + "  x" + std::to_string(deaths) + (best ? "  NEW BEST!" : "");
        save.addResult(levelKey(levelIndex), runTime, deaths);
    }

    if (levelIndex + 1 >= levels.size()) {
        if (!testing) save.continueLevel.clear();
        writeSave();
        particles.clear();
        openScreen(Screen::Complete);
        return;
    }

    // Keep the old level around to slide it out of view, and put the new one next to it.
    Transition next{level, drawCamera(), checkpoint, side};
    const float oldScroll = drawCamera().x + worldOffsetX;
    loadLevel(levelIndex + 1);
    const float slide = side == Level::Side::Right ? config::GAME_WIDTH : side == Level::Side::Left ? -float(config::GAME_WIDTH) : 0.f;
    worldOffsetX = oldScroll + slide - drawCamera().x;
    transition = next;

    if (!testing) save.continueLevel = levelKey(levelIndex);
    writeSave();
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
        const bool mouseMoved = mouse != lastMouse;
        lastMouse = mouse;
        if (screen == Screen::Playing) {
            if (input.wasMouseClicked() && pauseButton().contains(mouse)) openScreen(Screen::Paused);
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
        else if (key == K::R && !transition) restartLevel();
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

    // Walking off the edge of the screen, like in Celeste.
    if (auto side = level->exitThrough(player->centre())) {
        finishLevel(side);
        return;
    }
    if (level->isGoal(player->getPosition(), Player::HEIGHT, Player::WIDTH)) {
        finishLevel(std::nullopt);
        return;
    }

    if (auto cp = level->checkpointAt(player->getPosition(), Player::HEIGHT, Player::WIDTH); cp && *cp != checkpoint) {
        checkpoint = *cp;
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
    drawBackground(cam.x + worldOffsetX);
    level->draw(canvas, cam);
    level->drawMarkers(canvas, cam, checkpoint, clockTime);
    if (withPlayer && deathTimer <= 0.f) player->draw(canvas, cam);
    if (screen != Screen::Complete) particles.draw(canvas, cam); // the confetti goes over the dimmed screen
}

void Game::drawTransition()
{
    const Transition& t = *transition;
    const float p = easeInOut(t.time / TRANSITION_TIME);

    if (!t.side) {
        // Finished at a goal flag: fade to black and back.
        if (p < 0.5f) {
            drawBackground(drawCamera().x + worldOffsetX); // same as the old level's, nothing slides
            t.from->draw(canvas, t.fromCamera);
            t.from->drawMarkers(canvas, t.fromCamera, t.fromCheckpoint, clockTime);
            dimScreen(canvas, static_cast<sf::Uint8>(255.f * p * 2.f));
        } else {
            drawWorld(drawCamera());
            dimScreen(canvas, static_cast<sf::Uint8>(255.f * (1.f - p) * 2.f));
        }
        return;
    }

    // The new level comes in from the side the player left through, pushing the old one out.
    const sf::Vector2f screenSize(config::GAME_WIDTH, config::GAME_HEIGHT);
    const sf::Vector2f dir = *t.side == Level::Side::Right ? sf::Vector2f(1.f, 0.f)
                           : *t.side == Level::Side::Left ? sf::Vector2f(-1.f, 0.f)
                           : sf::Vector2f(0.f, -1.f); // screen y points down
    auto shiftBy = [&](float amount) {
        return sf::Vector2f(std::round(dir.x * amount * screenSize.x), std::round(dir.y * amount * screenSize.y));
    };
    const sf::Vector2f oldShift = shiftBy(-p), newShift = shiftBy(1.f - p);
    // A camera that draws everything `shift` screen pixels over.
    auto shifted = [](sf::Vector2f cam, sf::Vector2f shift) { return sf::Vector2f(cam.x - shift.x, cam.y + shift.y); };
    const sf::Vector2f oldCam = shifted(t.fromCamera, oldShift), newCam = shifted(drawCamera(), newShift);

    drawBackground(newCam.x + worldOffsetX);

    // Each level only draws inside its own part of the screen, so their edges don't overlap.
    auto clipTo = [&](sf::Vector2f shift) {
        sf::FloatRect area(std::max(0.f, shift.x), std::max(0.f, shift.y),
                           screenSize.x - std::abs(shift.x), screenSize.y - std::abs(shift.y));
        if (area.width <= 0.f || area.height <= 0.f) return false;
        sf::View view(area);
        view.setViewport({area.left / screenSize.x, area.top / screenSize.y, area.width / screenSize.x, area.height / screenSize.y});
        canvas.setView(view);
        return true;
    };
    if (clipTo(oldShift)) {
        t.from->draw(canvas, oldCam);
        t.from->drawMarkers(canvas, oldCam, t.fromCheckpoint, clockTime);
    }
    if (clipTo(newShift)) {
        level->draw(canvas, newCam);
        level->drawMarkers(canvas, newCam, checkpoint, clockTime);
        player->draw(canvas, newCam);
    }
    canvas.setView(canvas.getDefaultView());
}

void Game::drawBackground(float scrollX)
{
    sf::VertexArray sky(sf::Quads, 4);
    const sf::Color top(34, 38, 58), bottom(16, 18, 26);
    const float w = config::GAME_WIDTH, h = config::GAME_HEIGHT;
    sky[0] = sf::Vertex({0.f, 0.f}, top);
    sky[1] = sf::Vertex({w, 0.f}, top);
    sky[2] = sf::Vertex({w, h}, bottom);
    sky[3] = sf::Vertex({0.f, h}, bottom);
    canvas.draw(sky);

    // Two layers of cave silhouettes that scroll slower than the level, for depth.
    auto layer = [&](const sf::Texture& texture, float parallax) {
        sf::Sprite sprite(texture);
        int offset = static_cast<int>(std::round(scrollX * parallax));
        sprite.setTextureRect(sf::IntRect(offset, 0, static_cast<int>(w), static_cast<int>(h)));
        canvas.draw(sprite);
    };
    layer(bgFar, 0.15f);
    layer(bgNear, 0.35f);
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
    if (!player->getRope()) {
        if (auto target = player->ropeTarget(mouseWorld)) {
            canHook = true;
            sf::Vector2f p = viewCoordinates(*target, drawCamera());
            drawRect(canvas, {std::floor(p.x) - 1.f, std::floor(p.y) - 1.f}, {2.f, 2.f}, sf::Color(255, 255, 255, 200));
        }
    }
    drawCrosshair(canHook ? sf::Color(255, 255, 255) : sf::Color(150, 150, 170, 180));
}

void Game::drawCrosshair(sf::Color color)
{
    const sf::Vector2f mouse = input.mousePosition();
    const float mx = std::floor(mouse.x), my = std::floor(mouse.y);
    drawRect(canvas, {mx - 3.f, my}, {2.f, 1.f}, color);
    drawRect(canvas, {mx + 2.f, my}, {2.f, 1.f}, color);
    drawRect(canvas, {mx, my - 3.f}, {1.f, 2.f}, color);
    drawRect(canvas, {mx, my + 2.f}, {1.f, 2.f}, color);
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

    if (titleTimer > 0.f && screen == Screen::Playing) {
        // Fade in quickly, hold, fade out over the last second.
        float alpha = std::min({1.f, (TITLE_TIME - titleTimer) * 4.f, titleTimer});
        auto a = static_cast<sf::Uint8>(255.f * alpha);
        hud::drawCentred(canvas, "LEVEL " + std::to_string(levelIndex + 1), HALF_W, 30.f, withAlpha(GREY, a));
        hud::drawCentred(canvas, upper(levels[levelIndex].title), HALF_W, 40.f, withAlpha(WHITE, a), 2.f);
        if (!lastResult.empty()) hud::drawCentred(canvas, lastResult, HALF_W, 58.f, withAlpha(GREY, a));
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
            particles.draw(canvas, drawCamera());
            break;
        }
        case Screen::Playing:
            break;
    }
    menu.draw(canvas, clockTime);
}
