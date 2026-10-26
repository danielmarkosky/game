// Game: Mags's stall, where gems buy outfits, and what you're wearing.
#include "Game.h"
#include "src/GameShared.h"
#include "src/Hud.h"
#include "src/Outfits.h"
#include "src/Physics.h"
#include "src/Text.h"
#include <cmath>

using namespace game_detail;

int Game::gemsFound() const
{
    return static_cast<int>(progress.gems.size());
}

int Game::gemsToSpend() const
{
    return std::max(0, gemsFound() - progress.spent);
}

void Game::applyOutfit()
{
    const std::string wearing = progress.outfit.empty() ? outfits::FIRST : progress.outfit;
    player->setLook(Player::lookNamed(wearing));
    player->setBreathScale(wearing == outfits::DIVER ? physics().divingAir : 1.f);
}

void Game::shopChoice(size_t index)
{
    const auto& all = outfits::all();
    if (index >= all.size()) return;
    const outfits::Outfit& outfit = all[index];
    const bool owned = outfit.price == 0 || progress.outfits.count(outfit.id) > 0;
    if (!owned) {
        if (gemsToSpend() < outfit.price) {
            stallSays("NOT ENOUGH GEMS. COME BACK WITH " + std::to_string(outfit.price - gemsToSpend()) + " MORE");
            audio.play(Audio::Sfx::RopeClink, 0.6f);
            return;
        }
        progress.spent += outfit.price;
        progress.outfits.insert(outfit.id);
        stallSays("SOLD! IT SUITS YOU");
        audio.play(Audio::Sfx::Buy);
    } else {
        audio.play(Audio::Sfx::MenuPick);
    }
    progress.outfit = outfit.id == outfits::FIRST ? "" : outfit.id;
    applyOutfit();
    saveProgress();
    refreshMenu();
}

void Game::drawShop()
{
    hud::dim(canvas, 180);
    hud::drawCentred(canvas, "MAGS'S STALL", HALF_W, 12.f, hud::GOLD, 2.f);
    hud::drawText(canvas, "GEMS " + std::to_string(gemsToSpend()), {config::GAME_WIDTH - 8.f - hud::textWidth("GEMS " + std::to_string(gemsToSpend())), 8.f},
                  hud::CYAN);
    // You, in the one chosen, turning to look at yourself.
    const auto& all = outfits::all();
    if (menu.selected() < all.size()) {
        const outfits::Outfit& outfit = all[menu.selected()];
        const sf::Vector2f feet(250.f, 104.f);
        hud::fillRect(canvas, {feet.x - 26.f, feet.y + 1.f, 52.f, 2.f}, sf::Color(255, 255, 255, 40));
        player->drawPose(canvas, Player::lookNamed(outfit.id), Player::Pose::Idle, feet, std::fmod(clockTime, 6.f) > 3.f, clockTime, 3.f);
        float y = 112.f;
        for (const std::string& line : text::wrap(outfit.text, 120.f)) {
            text::draw(canvas, line, {std::floor(feet.x - text::width(line) * 0.5f), y}, hud::GREY);
            y += text::LINE_HEIGHT;
        }
    }
    if (clockTime < stallNoteUntil) hud::drawCentred(canvas, stallNote, HALF_W, 156.f, hud::GOLD);
    hud::drawCentred(canvas, hint(controls::Action::Confirm) + " - BUY / WEAR    " + hint(controls::Action::Back) + " - LEAVE", HALF_W, 168.f, hud::GREY);
}

void Game::stallSays(std::string text)
{
    stallNote = std::move(text);
    stallNoteUntil = clockTime + 2.5f;
}

void Game::drawRecords()
{
    hud::dim(canvas, 180);
    hud::drawCentred(canvas, "RECORDS", HALF_W, 14.f, hud::GOLD, 2.f);
    hud::drawCentred(canvas, "EACH ROUTE, FROM ITS GREEN POST TO ITS FLAG", HALF_W, 34.f, hud::GREY);
    hud::drawCentred(canvas, hint(controls::Action::Back) + " - BACK", HALF_W, 168.f, hud::GREY);
}
