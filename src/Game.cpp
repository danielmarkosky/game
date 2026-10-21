#include "Game.h"
#include "src/Config.h"
#include "src/Hud.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>
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
    constexpr float FALL_OUT_MARGIN = 64.f; // how far below the level counts as falling out
    const char* const TITLE = "Celeste-like Game";

    const sf::Color WHITE(235, 235, 245);
    const sf::Color GOLD(255, 210, 60);
    const sf::Color RED(255, 90, 90);
    const sf::Color GREY(150, 150, 170);

    std::shared_ptr<sf::RenderWindow> makeWindow()
    {
        auto window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode(config::GAME_WIDTH * config::WINDOW_SCALE, config::GAME_HEIGHT * config::WINDOW_SCALE),
            TITLE,
            sf::Style::Close
        );
        window->setFramerateLimit(60);
        window->setMouseCursorVisible(false); // we draw our own crosshair
        return window;
    }

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

Game::Game(std::vector<LevelInfo> levelList)
    : window(makeWindow()),
      input(window),
      levels(std::move(levelList))
{
    canvas.create(config::GAME_WIDTH, config::GAME_HEIGHT);
    canvas.setSmooth(false);
    bgFar.loadFromFile(config::assetPath("gfx/bg_far.png"));
    bgNear.loadFromFile(config::assetPath("gfx/bg_near.png"));
    bgFar.setRepeated(true);
    bgNear.setRepeated(true);
    loadLevel(0);
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
    runTime = 0.f;
    deaths = 0;
    deathTimer = 0.f;
    deathFlash = 0.f;
    shake = 0.f;
    state = State::Playing;
    ignoreInputThisFrame = true;
    updateCamera(0.f, true);
}

void Game::run()
{
    sf::Clock clock;
    float accumulator = 0.f;

    while (window->isOpen()) {
        sf::Event event;
        while (window->pollEvent(event)) {
            if (event.type == sf::Event::Closed) window->close();
            if (event.type == sf::Event::KeyPressed) handleKey(event.key.code);
        }

        // Cap the frame time so a hitch (e.g. dragging the window) doesn't fast-forward the game.
        float frameTime = std::min(clock.restart().asSeconds(), 0.25f);

        input.update();
        // Right after a level (re)starts, the key that started it shouldn't also make you jump.
        player->handleInput(deathTimer > 0.f || ignoreInputThisFrame ? PlayerInput{} : readInput());
        ignoreInputThisFrame = false;

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
    if (key == sf::Keyboard::Escape) window->close();

    if (key == sf::Keyboard::R) {
        if (state == State::GameComplete) {
            totalTime = 0.f;
            totalDeaths = 0;
            loadLevel(0);
        } else {
            restartLevel();
        }
    }

    const bool next = key == sf::Keyboard::Space || key == sf::Keyboard::Enter;
    if (next && state == State::LevelComplete) {
        totalTime += runTime;
        totalDeaths += deaths;
        if (levelIndex + 1 < levels.size()) loadLevel(levelIndex + 1);
        else state = State::GameComplete;
    }
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
    in.ropePressed = input.wasMouseClicked();
    in.ropeHeld = input.isMousePressed();
    in.aim = worldCoordinates(input.mousePosition(), drawCamera());
    return in;
}

void Game::step(float dt)
{
    clockTime += dt;
    if (state == State::Playing) runTime += dt;
    deathFlash = std::max(0.f, deathFlash - dt);
    shake = std::max(0.f, shake - dt);
    titleTimer = std::max(0.f, titleTimer - dt);
    particles.update(dt);

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

    if (auto cp = level->checkpointAt(player->getPosition(), Player::HEIGHT, Player::WIDTH); cp && *cp != checkpoint) {
        checkpoint = *cp;
        particles.spawn(checkpoint + sf::Vector2f(4.f, 16.f), {.count = 16, .speed = 50.f, .spread = 2.f, .life = 0.6f, .gravity = 60.f,
                                                              .colors = {sf::Color(126, 240, 142), sf::Color(220, 255, 220)}});
    }

    if (state == State::Playing && level->isGoal(player->getPosition(), Player::HEIGHT, Player::WIDTH)) {
        state = State::LevelComplete;
        particles.spawn(player->centre(), {.count = 70, .speed = 140.f, .spread = 2.2f, .life = 1.6f, .gravity = 150.f, .size = 2.f,
                                           .colors = {GOLD, sf::Color(255, 120, 160), sf::Color(120, 220, 255), sf::Color(160, 240, 120)}});
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

    // Never show what's outside the level, unless the level is smaller than the screen.
    const sf::FloatRect b = level->bounds();
    if (b.width > 2.f * HALF_W) camera.x = std::clamp(camera.x, b.left + HALF_W, b.left + b.width - HALF_W);
    else camera.x = b.left + b.width * 0.5f;
    if (b.height > 2.f * HALF_H) camera.y = std::clamp(camera.y, b.top + HALF_H, b.top + b.height - HALF_H);
    else camera.y = b.top + b.height * 0.5f;
}

sf::Vector2f Game::drawCamera() const
{
    // Rounded so tiles and player move in whole pixels together and nothing shimmers.
    return {std::round(camera.x), std::round(camera.y)};
}

void Game::render()
{
    sf::Vector2f cam = drawCamera();
    if (shake > 0.f) {
        std::uniform_real_distribution<float> offset(-1.f, 1.f);
        float amount = SHAKE_PIXELS * shake / SHAKE_TIME;
        cam += sf::Vector2f(std::round(offset(rng) * amount), std::round(offset(rng) * amount));
    }

    drawBackground(cam);
    level->draw(canvas, cam);
    level->drawMarkers(canvas, cam, checkpoint, clockTime);
    if (deathTimer <= 0.f) player->draw(canvas, cam);
    particles.draw(canvas, cam);
    if (state == State::Playing && deathTimer <= 0.f) drawAim();
    drawHud();
    canvas.display();

    window->clear();
    sf::Sprite scaledSprite(canvas.getTexture());
    scaledSprite.setScale(static_cast<float>(config::WINDOW_SCALE), static_cast<float>(config::WINDOW_SCALE));
    window->draw(scaledSprite);
    window->display();
}

void Game::drawBackground(sf::Vector2f cam)
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
        int offset = static_cast<int>(std::round(cam.x * parallax));
        sprite.setTextureRect(sf::IntRect(offset, 0, static_cast<int>(w), static_cast<int>(h)));
        canvas.draw(sprite);
    };
    layer(bgFar, 0.15f);
    layer(bgNear, 0.35f);
}

void Game::drawAim()
{
    const sf::Vector2f mouse = input.mousePosition();
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

    const sf::Color color = canHook ? sf::Color(255, 255, 255) : sf::Color(150, 150, 170, 180);
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

    const std::string levelNumber = std::to_string(levelIndex + 1) + "/" + std::to_string(levels.size());

    if (state == State::GameComplete) {
        dimScreen(canvas, 170);
        hud::drawCentred(canvas, "YOU DID IT!", HALF_W, 30.f, GOLD, 3.f);
        hud::drawCentred(canvas, "ALL " + std::to_string(levels.size()) + " LEVELS", HALF_W, 56.f, WHITE);
        hud::drawCentred(canvas, hud::formatTime(totalTime), HALF_W, 74.f, WHITE, 4.f);
        hud::drawCentred(canvas, "x" + std::to_string(totalDeaths), HALF_W, 104.f, RED, 2.f);
        hud::drawCentred(canvas, "R - PLAY AGAIN", HALF_W, 150.f, GREY);
        return;
    }

    if (state == State::LevelComplete) {
        dimScreen(canvas, 120);
        hud::drawCentred(canvas, "LEVEL COMPLETE", HALF_W, 34.f, GOLD, 2.f);
        hud::drawCentred(canvas, hud::formatTime(runTime), HALF_W, 60.f, WHITE, 4.f);
        hud::drawCentred(canvas, "x" + std::to_string(deaths), HALF_W, 90.f, RED, 2.f);
        const bool last = levelIndex + 1 >= levels.size();
        hud::drawCentred(canvas, last ? "SPACE - FINISH" : "SPACE - NEXT LEVEL", HALF_W, 140.f, WHITE);
        hud::drawCentred(canvas, "R - TRY AGAIN", HALF_W, 150.f, GREY);
        return;
    }

    hud::drawText(canvas, hud::formatTime(runTime), {4.f, 4.f}, WHITE);
    hud::drawText(canvas, "x" + std::to_string(deaths), {4.f, 11.f}, RED);
    hud::drawText(canvas, levelNumber, {config::GAME_WIDTH - 4.f - hud::textWidth(levelNumber), 4.f}, GREY);

    if (titleTimer > 0.f) {
        // Fade in quickly, hold, fade out over the last second.
        float alpha = std::min({1.f, (TITLE_TIME - titleTimer) * 4.f, titleTimer});
        auto a = static_cast<sf::Uint8>(255.f * alpha);
        hud::drawCentred(canvas, "LEVEL " + std::to_string(levelIndex + 1), HALF_W, 30.f, sf::Color(GREY.r, GREY.g, GREY.b, a));
        hud::drawCentred(canvas, levels[levelIndex].title, HALF_W, 40.f, sf::Color(WHITE.r, WHITE.g, WHITE.b, a), 2.f);
    }
}
