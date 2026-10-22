// Game: drawing it all.
#include "Game.h"
#include "src/GameShared.h"
#include "src/Hud.h"
#include "src/Props.h"
#include "src/Text.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>

using namespace game_detail;

// ---------------------------------------------------------------------------------------------
// Drawing

void Game::renderCanvas()
{
    player->setSitting(sitting.has_value() || waking);
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
        if (!input.usingPad()) drawCrosshair(input.mousePosition(), hud::GREY);
    }
    if (fade > 0.f) hud::dim(canvas, static_cast<sf::Uint8>(255.f * fade / FADE_TIME));
    canvas.display();
}

void Game::render()
{
    renderCanvas();
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
    drawAir(cam);
}

void Game::drawAir(sf::Vector2f cam)
{
    ambience.drawNear(canvas);
    // In dark places the player carries a little light.
    if (deathTimer <= 0.f && ambience.isDark()) {
        lights.push_back({viewCoordinates(player->centre(), cam), 34.f, sf::Color(150, 140, 170, 255)});
    } else if (deathTimer <= 0.f && ambience.darkness() > 0.f) {
        lights.push_back({viewCoordinates(player->centre(), cam), 46.f, sf::Color(90, 85, 110, static_cast<sf::Uint8>(160.f * ambience.darkness()))});
    }
    ambience.drawLight(canvas, lights);
    drawBeams();
}

void Game::drawBeams()
{
    // Soft light falling through a crack: widening as it goes down, fading out, swaying a little,
    // with dust drifting in it. Added on, so it brightens what it falls on.
    const sf::RenderStates add(sf::BlendAdd);
    for (const Beam& beam : beams) {
        const float sway = std::sin(clockTime * 0.4f + beam.seed) * 3.f;
        const sf::Vector2f top = beam.top, bottom = beam.top + sf::Vector2f(sway, beam.length);
        const float topHalf = 5.f, bottomHalf = 14.f;
        const sf::Color bright(255, 238, 200, 70), none(255, 238, 200, 0);
        const sf::Vertex quad[] = {{top + sf::Vector2f(-topHalf, 0.f), bright}, {top + sf::Vector2f(topHalf, 0.f), bright},
                                   {bottom + sf::Vector2f(bottomHalf, 0.f), none}, {bottom + sf::Vector2f(-bottomHalf, 0.f), none}};
        canvas.draw(quad, 4, sf::Quads, add);
        // A narrower, brighter core.
        const sf::Vertex core[] = {{top + sf::Vector2f(-2.f, 0.f), bright}, {top + sf::Vector2f(2.f, 0.f), bright},
                                   {bottom + sf::Vector2f(6.f, 0.f), none}, {bottom + sf::Vector2f(-6.f, 0.f), none}};
        canvas.draw(core, 4, sf::Quads, add);
        for (int i = 0; i < 12; ++i) {
            const float along = std::fmod(clockTime * (4.f + i % 3) + i * 23.7f + beam.seed, beam.length);
            const float t = along / beam.length;
            const float half = topHalf + (bottomHalf - topHalf) * t;
            const float x = top.x + sway * t + std::sin(clockTime * 0.6f + i * 1.7f) * half * 0.8f;
            const auto alpha = static_cast<sf::Uint8>(150.f * (1.f - t) * (0.5f + 0.5f * std::sin(clockTime * 1.3f + i)));
            hud::fillRect(canvas, {std::floor(x), std::floor(top.y + along), 1.f, 1.f}, sf::Color(255, 240, 210, alpha));
        }
    }
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
    ambience.drawFar(canvas);
    lights.clear();
    beams.clear();
    auto owned = [this](const std::string& ability) { return hasAbility(ability); };
    for (const Visible& v : visible) {
        std::optional<sf::Vector2f> flag;
        if (v.index == levelIndex) flag = checkpoint;
        else if (transition && v.index == transition->from) flag = transition->fromCheckpoint;
        rooms[v.index]->draw(canvas, v.cam);
        rooms[v.index]->drawProps(canvas, v.cam, clockTime);
        rooms[v.index]->drawMoving(canvas, v.cam, clockTime);
        rooms[v.index]->drawMarkers(canvas, v.cam, flag, clockTime);
        std::optional<size_t> saved;
        for (size_t bench = 0; bench < rooms[v.index]->benches().size(); ++bench) {
            if (benchKey(v.index, bench) == progress.bench) saved = bench;
        }
        rooms[v.index]->drawBenches(canvas, v.cam, saved, clockTime);
        rooms[v.index]->drawLevers(canvas, v.cam);
        rooms[v.index]->drawPickups(canvas, v.cam, clockTime, owned);
        rooms[v.index]->drawGems(canvas, v.cam, clockTime, [&](size_t gem) { return gemFound(v.index, gem); });
        // What shines: lamps and fires, gems still to find, things to pick up, benches.
        const Level& room = *rooms[v.index];
        for (const Prop& prop : room.props()) {
            if (prop.kind == props::LIGHT_SHAFT) {
                // Down from its tile to the first rock under it.
                int x = static_cast<int>(prop.position.x / Level::TILE_SIZE), y = static_cast<int>(prop.position.y / Level::TILE_SIZE);
                int floor = y;
                while (floor > y - 40 && !room.isSolid(x, floor - 1)) --floor;
                const sf::Vector2f top = viewCoordinates(prop.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, Level::TILE_SIZE), v.cam);
                beams.push_back({top, static_cast<float>(y + 1 - floor) * Level::TILE_SIZE, prop.position.x});
                continue;
            }
            const PropInfo* info = props::find(prop.kind);
            if (!info || info->lightRadius <= 0.f) continue;
            if (room.isDark() && prop.kind == "lantern_post") continue; // a dark room's lamps are lit with the rope
            const float h = static_cast<float>(info->rect.height);
            const sf::Vector2f middle = prop.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, info->hangs ? Level::TILE_SIZE - h * 0.6f : h * 0.5f);
            const float flicker = info->fps > 0.f ? 1.f + 0.07f * std::sin(clockTime * 11.f + prop.position.x) : 1.f;
            sf::Color colour = info->light;
            colour.a = 150;
            lights.push_back({viewCoordinates(middle, v.cam), info->lightRadius * flicker, colour});
        }
        for (size_t gem = 0; gem < room.gems().size(); ++gem) {
            if (!gemFound(v.index, gem)) lights.push_back({viewCoordinates(room.gems()[gem] + sf::Vector2f(4.f, 4.f), v.cam), 16.f, sf::Color(60, 150, 170, 150)});
        }
        for (const Lamp& lamp : room.lamps()) {
            const sf::Vector2f middle = lamp.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, Level::TILE_SIZE * 0.4f);
            if (lamp.lit) lights.push_back({viewCoordinates(middle, v.cam), 64.f * (1.f + 0.04f * std::sin(clockTime * 9.f + middle.x)), sf::Color(255, 200, 120, 170)});
            else lights.push_back({viewCoordinates(middle, v.cam), 7.f, sf::Color(120, 120, 150, 120)}); // a glint, to find it by
        }
        for (const Pickup& pickup : room.pickups()) {
            if (!hasAbility(pickup.ability)) lights.push_back({viewCoordinates(pickup.position + sf::Vector2f(4.f, 6.f), v.cam), 30.f, sf::Color(170, 140, 60, 170)});
        }
        for (const sf::Vector2f& bench : room.benches()) {
            lights.push_back({viewCoordinates(bench + sf::Vector2f(Level::BENCH_WIDTH * 0.5f, 6.f), v.cam), 34.f, sf::Color(150, 110, 60, 140)});
        }
        for (const Npc& npc : rooms[v.index]->npcs()) {
            const sf::Vector2f feet = npc.position + sf::Vector2f(Level::TILE_SIZE * 0.5f, 0.f);
            if (npc.look == "note") {
                // One of Ash's notes: a page weighed down with a stone, glinting until it's read.
                const PropInfo* note = props::find("note");
                if (note) {
                    sf::Sprite sprite(props::texture(), note->rect);
                    sprite.setOrigin(std::floor(note->rect.width * 0.5f), note->rect.height - 1.f);
                    const sf::Vector2f at = viewCoordinates(feet, v.cam);
                    sprite.setPosition(std::floor(at.x), std::floor(at.y));
                    canvas.draw(sprite);
                }
                continue;
            }
            // Turned towards the player, and not all breathing in step.
            const bool facingLeft = v.index == levelIndex && player->centre().x < feet.x;
            player->drawFigure(canvas, Player::lookNamed(npc.look), viewCoordinates(feet, v.cam), facingLeft, clockTime + feet.x * 0.37f);
        }
        rooms[v.index]->drawQuestItems(canvas, v.cam, clockTime, [&](size_t item) { return progress.flags.count("got_" + room.questItems()[item].item) > 0; });
        for (const QuestItem& item : room.questItems()) {
            if (!progress.flags.count("got_" + item.item)) lights.push_back({viewCoordinates(item.position + sf::Vector2f(4.f, 5.f), v.cam), 22.f, sf::Color(230, 200, 110, 150)});
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
    drawAir(cam);
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
    const bool pad = input.usingPad();
    const sf::Vector2f mouse = input.mousePosition();
    if (!pad && pauseButton().contains(mouse)) {
        drawCrosshair(mouse, hud::GREY);
        return;
    }
    const sf::Vector2f aim = aimPoint();

    // Show where the rope would stick, so you know before you shoot. With the stick the crosshair
    // goes there too, or as far as the rope reaches.
    std::optional<sf::Vector2f> hook;
    if (!player->getRope() && player->canShootRope()) {
        if ((hook = player->ropeTarget(aim))) {
            sf::Vector2f p = viewCoordinates(*hook, drawCamera());
            hud::fillRect(canvas, {std::floor(p.x) - 1.f, std::floor(p.y) - 1.f, 2.f, 2.f}, sf::Color(255, 255, 255, 200));
        } else if (auto hit = player->aimHit(aim); hit && (hit->kind == RopeHit::Kind::Lever || hit->kind == RopeHit::Kind::Lamp)) {
            // Something it tugs: a lever, a lamp.
            sf::Vector2f p = viewCoordinates(hit->point, drawCamera());
            hud::fillRect(canvas, {std::floor(p.x) - 1.f, std::floor(p.y) - 1.f, 2.f, 2.f}, hud::GOLD);
        } else if (auto bounce = player->ropeBounce(aim)) {
            // Rock in range, but no pick yet: a dim red dot where the rope would bounce off.
            sf::Vector2f p = viewCoordinates(*bounce, drawCamera());
            hud::fillRect(canvas, {std::floor(p.x) - 1.f, std::floor(p.y) - 1.f, 2.f, 2.f}, sf::Color(230, 90, 90, 150));
        }
    }
    if (pad && player->getRope()) return; // swinging: the stick reels, it doesn't aim
    const sf::Vector2f at = pad ? viewCoordinates(hook ? *hook : aim, drawCamera()) : mouse;
    drawCrosshair(at, hook ? sf::Color(255, 255, 255) : sf::Color(150, 150, 170, 180));
}

void Game::drawCrosshair(sf::Vector2f at, sf::Color color)
{
    const float mx = std::floor(at.x), my = std::floor(at.y);
    // With a dark shadow, so it shows up on the bright daylight sky too.
    for (auto [offset, c] : {std::pair{1.f, sf::Color(0, 0, 0, 140)}, std::pair{0.f, color}}) {
        hud::fillRect(canvas, {mx - 3.f + offset, my + offset, 2.f, 1.f}, c);
        hud::fillRect(canvas, {mx + 2.f + offset, my + offset, 2.f, 1.f}, c);
        hud::fillRect(canvas, {mx + offset, my - 3.f + offset, 1.f, 2.f}, c);
        hud::fillRect(canvas, {mx + offset, my + 2.f + offset, 1.f, 2.f}, c);
    }
}

void Game::drawHud()
{
    if (deathFlash > 0.f) {
        auto alpha = static_cast<sf::Uint8>(110.f * deathFlash / DEATH_TIME);
        hud::fillRect(canvas, {0.f, 0.f, float(config::GAME_WIDTH), float(config::GAME_HEIGHT)}, sf::Color(200, 30, 40, alpha));
    }

    if (settings.showTimer) {
        hud::drawText(canvas, hud::formatTime(progress.playTime), {4.f, 4.f}, hud::WHITE);
        hud::drawText(canvas, "x" + std::to_string(progress.deaths), {4.f, 11.f}, hud::RED);
    }
    // A race: the time left, big, red at the end.
    if (race && screen == Screen::Playing) {
        const int left = static_cast<int>(std::ceil(race->left));
        const bool hurry = race->left < 10.f;
        hud::drawCentred(canvas, std::to_string(left), HALF_W, 4.f, hurry && std::fmod(race->left, 0.5f) < 0.25f ? hud::RED : hud::WHITE, 2.f);
    }

    // Holding on to a wall: how much strength is left, a bar over your head that goes red at the end.
    if (screen == Screen::Playing && (player->isClinging() || (player->staminaLeft() < 1.f && !player->isGrounded()))) {
        const float left = player->staminaLeft();
        const sf::Vector2f p = viewCoordinates(player->centre() + sf::Vector2f(0.f, 10.f), drawCamera());
        const sf::FloatRect bar(std::floor(p.x) - 5.f, std::floor(p.y), 10.f, 2.f);
        hud::fillRect(canvas, {bar.left - 1.f, bar.top - 1.f, bar.width + 2.f, bar.height + 2.f}, sf::Color(14, 14, 22, 200));
        const bool flash = left < 0.25f && std::fmod(clockTime, 0.3f) < 0.15f;
        hud::fillRect(canvas, {bar.left, bar.top, std::round(bar.width * left), bar.height}, left < 0.25f ? (flash ? hud::WHITE : hud::RED) : hud::GOLD);
    }

    // Someone to talk to, or a bench to sit on: a little bubble above saying which key.
    std::optional<sf::Vector2f> bubbleAt;
    if (screen == Screen::Playing) {
        if (const Npc* npc = npcInReach()) bubbleAt = npc->position + sf::Vector2f(Level::TILE_SIZE * 0.5f, 24.f);
        else if (auto bench = sitting ? std::nullopt : benchInReach()) bubbleAt = level->benches()[*bench] + sf::Vector2f(Level::BENCH_WIDTH * 0.5f, 20.f);
        else if (auto lever = leverInReach()) bubbleAt = level->levers()[*lever] + sf::Vector2f(4.f, 20.f);
    }
    if (bubbleAt) {
        const float bob = std::round(std::sin(clockTime * 4.f));
        const sf::Vector2f p = viewCoordinates(*bubbleAt + sf::Vector2f(0.f, bob), drawCamera());
        // Up works too, but on the gamepad the button is easier.
        const std::string key = input.usingPad() ? hint(controls::Action::Interact) : hint(controls::Action::Up);
        const float width = hud::textWidth(key) + 8.f;
        const sf::FloatRect bubble(std::floor(p.x - width * 0.5f), std::floor(p.y), width, 9.f);
        hud::fillRect(canvas, {bubble.left, bubble.top, bubble.width, bubble.height}, sf::Color(14, 14, 22, 220));
        hud::frame(canvas, bubble, hud::withAlpha(hud::WHITE, 160));
        hud::drawText(canvas, key, {bubble.left + 4.f, bubble.top + 2.f}, hud::WHITE);
    }
    if (waking && screen == Screen::Playing && titleTimer <= 0.f) {
        const std::string move = input.usingPad() ? "STICK" : hint(controls::Action::Left) + " / " + hint(controls::Action::Right);
        hud::drawCentred(canvas, move + " - GET UP", HALF_W, 168.f, hud::withAlpha(hud::GREY, 200));
    }
    if (sitting && screen == Screen::Playing) {
        const std::string getUp = "MOVE - GET UP";
        const std::string text = travelDestinations().size() > 1 ? hint(controls::Action::Map) + " - TRAVEL    " + getUp : getUp;
        hud::drawCentred(canvas, text, HALF_W, 168.f, hud::GREY);
    }

    // A note for a moment (saved, or how many gems now), and the gem count while paused.
    const float noteY = settings.showTimer ? 18.f : 4.f;
    if (screen == Screen::Paused && gemCount() > 0) {
        hud::drawText(canvas, gemTotal(), {4.f, noteY}, hud::CYAN);
    } else if (noticeTimer > 0.f) {
        const float alpha = std::min({1.f, (NOTICE_TIME - noticeTimer) * 6.f, noticeTimer});
        hud::drawText(canvas, notice, {4.f, noteY}, hud::withAlpha(hud::CYAN, static_cast<sf::Uint8>(255.f * alpha)));
    }

    // Pause button in the top right corner.
    const sf::FloatRect button = pauseButton();
    const bool hovered = screen == Screen::Playing && !input.usingPad() && button.contains(input.mousePosition());
    const sf::Color barColor = hovered ? hud::WHITE : hud::GREY;
    for (float x : {button.left + 3.f, button.left + 7.f}) {
        hud::fillRect(canvas, {x + 1.f, button.top + 4.f, 2.f, 6.f}, sf::Color(0, 0, 0, 160));
        hud::fillRect(canvas, {x, button.top + 3.f, 2.f, 6.f}, barColor);
    }

    if (titleTimer > 0.f && messageTimer <= 0.f && screen == Screen::Playing) {
        // Fade in quickly, hold, fade out over the last second.
        float alpha = std::min({1.f, (TITLE_TIME - titleTimer) * 4.f, titleTimer});
        auto a = static_cast<sf::Uint8>(255.f * alpha);
        hud::drawCentred(canvas, hud::upper(levels[levelIndex].title), HALF_W, 40.f, hud::withAlpha(hud::WHITE, a), 2.f);
    }
    if (messageTimer > 0.f && screen == Screen::Playing) {
        float alpha = std::min({1.f, (MESSAGE_TIME - messageTimer) * 4.f, messageTimer});
        auto a = static_cast<sf::Uint8>(255.f * alpha);
        hud::drawCentred(canvas, messageTitle, HALF_W, 34.f, hud::withAlpha(hud::GOLD, a), 2.f);
        hud::drawCentred(canvas, messageText, HALF_W, 52.f, hud::withAlpha(hud::WHITE, a));
    }
}
