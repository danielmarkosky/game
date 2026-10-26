// Game: what there is along the routes besides the climbing. Mine carts, the stopwatch's timed
// runs, and Pip.
#include "Game.h"
#include "src/GameShared.h"
#include "src/Hud.h"
#include "src/Regions.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>

using namespace game_detail;

namespace
{
    constexpr float SNIFF_TIME = 7.f;                       // Pip points out a gem this often, while there's one in the room
    constexpr float SNIFF_RANGE = 26.f * Level::TILE_SIZE;  // as far as he can smell one
    constexpr float RUN_SHOWN = 4.f;                        // a finished run's time stays up this long
}

// ---------------------------------------------------------------------------------------------
// Mine carts

bool Game::cartReady(const Cart& cart) const
{
    std::istringstream facts(cart.needs);
    for (std::string fact; std::getline(facts, fact, ',');) {
        if (!fact.empty() && !hasFact(fact)) return false;
    }
    return true;
}

void Game::updateCart()
{
    if (player->isRiding()) {
        if (level->isAtCartStop(player->getPosition(), Player::HEIGHT, Player::WIDTH) && player->isGrounded()) {
            // The end of the line: out you get, and the cart stays where it stopped.
            parked = {levelIndex, player->getPosition() + sf::Vector2f(Player::WIDTH * 0.5f, 0.f)};
            player->stopRiding();
            checkpoint = player->getPosition();
            checkpointRiding = 0;
            audio.play(Audio::Sfx::Land, 0.7f);
            shake = SHAKE_TIME * 0.5f;
        }
        return;
    }
    const auto cart = level->cartAt(player->getPosition(), Player::HEIGHT, Player::WIDTH);
    if (!cart) {
        cartRefused = false;
        return;
    }
    const Cart& c = level->carts()[*cart];
    if (!cartReady(c)) {
        if (!cartRefused) showMessage("IT WON'T BUDGE", "THE LINE IS ASLEEP. FOUR BEACONS WAKE IT");
        cartRefused = true;
        return;
    }
    if (!player->isGrounded() || player->getRope()) return;
    // In, standing in the middle of it, and off it goes.
    player->respawn({std::round(c.position.x + (Level::TILE_SIZE - Player::WIDTH) * 0.5f), c.position.y});
    player->startRiding(c.dir);
    checkpoint = player->getPosition();
    checkpointRiding = c.dir;
    needsSpawnPoint = false;
    parked.reset();
    audio.play(Audio::Sfx::CartStart);
    if (progress.flags.insert("seen_cart").second) showMessage("HOLD ON", "IT GOES WHERE THE RAILS GO. JUMP TO JUMP IT");
}

// ---------------------------------------------------------------------------------------------
// The stopwatch

std::vector<std::string> Game::trialRoutes() const
{
    std::vector<std::string> routes;
    for (const auto& room : rooms) {
        for (const Trial& trial : room->trials()) {
            if (!trial.finish && std::find(routes.begin(), routes.end(), trial.route) == routes.end()) routes.push_back(trial.route);
        }
    }
    return routes;
}

void Game::updateTrial()
{
    if (!hasAbility("stopwatch")) return;
    const Trial* trial = level->trialAt(player->getPosition(), Player::HEIGHT, Player::WIDTH);
    if (!trial) return;
    if (!trial->finish) {
        // Through the start: from nothing, again.
        if (!timedRun || timedRun->route != trial->route || timedRun->time > 0.5f) audio.play(Audio::Sfx::Watch);
        if (!timedRun || timedRun->route != trial->route) showNotice("TIMING: " + regions::name(trial->route));
        timedRun = Run{trial->route, 0.f};
        return;
    }
    if (!timedRun || timedRun->route != trial->route) return;
    // Over the line.
    const float time = timedRun->time;
    const auto best = progress.records.find(timedRun->route);
    const bool record = best == progress.records.end() || time < best->second;
    runText = hud::formatTime(time) + (record ? "  NEW RECORD!" : "  BEST " + hud::formatTime(best->second));
    if (record) progress.records[timedRun->route] = time;
    runShown = RUN_SHOWN;
    timedRun.reset();
    saveProgress();
    audio.play(record ? Audio::Sfx::Record : Audio::Sfx::Watch);
    if (record) {
        particles.spawn(player->centre(), {.count = 30, .speed = 70.f, .life = 0.9f, .gravity = 80.f, .size = 2.f, .colors = CONFETTI});
    }
}

// ---------------------------------------------------------------------------------------------
// Pip

bool Game::hasDog() const
{
    return hasFact("dog");
}

void Game::updateDog(float dt)
{
    if (!hasDog()) return;
    dog.follow(player->getPosition() + sf::Vector2f(Player::WIDTH * 0.5f, 0.f), dt);
    // Now and then, standing about, he smells the nearest gem still to find in the room and points.
    sniffTimer -= dt;
    if (sniffTimer > 0.f || !player->isGrounded()) return;
    sniffTimer = SNIFF_TIME;
    std::optional<sf::Vector2f> nearest;
    for (size_t gem = 0; gem < level->gems().size(); ++gem) {
        const sf::Vector2f at = level->gems()[gem] + sf::Vector2f(4.f, 4.f);
        if (gemFound(levelIndex, gem) || distance(at, dog.feet()) > SNIFF_RANGE) continue;
        if (!nearest || distance(at, dog.feet()) < distance(*nearest, dog.feet())) nearest = at;
    }
    if (!nearest) return;
    dog.point(*nearest);
    audio.play(Audio::Sfx::Bark, 0.95f + 0.1f * static_cast<float>(std::rand() % 100) / 100.f);
    // A trail of sparkles off his nose towards it.
    const sf::Vector2f nose = dog.feet() + sf::Vector2f(0.f, 6.f);
    const sf::Vector2f way = *nearest - nose;
    particles.spawn(nose, {.count = 10, .speed = 45.f, .spread = 0.25f, .direction = std::atan2(way.y, way.x), .life = 1.f, .gravity = 0.f,
                           .colors = {hud::withAlpha(hud::CYAN, 200), sf::Color(220, 255, 255, 180)}});
}
