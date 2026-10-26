// Game: the screens and menus, and the settings they change.
#include "Game.h"
#include "src/GameShared.h"
#include "src/Hud.h"
#include "src/Outfits.h"
#include "src/Regions.h"
#include "src/Text.h"
#include "src/utils.h"
#include <algorithm>
#include <cmath>

using namespace game_detail;

namespace
{
    // Menu item ids.
    enum Choice
    {
        PLAY, OPTIONS, QUIT, ERASE, ERASE_YES, ERASE_NO,
        RESUME, RETRY, MAIN_MENU, BACK, MAP, RECORDS,
        WINDOW_SIZE, FULLSCREEN, SCREEN_SHAKE, TIMER, MUSIC, SOUND, CONTROLS, RESET_CONTROLS,
        FIRST_SLOT = 1000,   // + slot number
        FIRST_ACTION = 2000, // + controls::Action, in the controls
        FIRST_OUTFIT = 3000, // + its number in outfits::all(), at the stall
        FIRST_ROUTE = 4000,  // + its number in trialRoutes(), in the records
    };

    MenuItem button(int id, std::string label)
    {
        return {id, std::move(label), "", false};
    }

    std::string onOff(bool on)
    {
        return on ? "ON" : "OFF";
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
    menu.offsetX = next == Screen::Shop ? -45.f : 0.f; // the mirror's beside it

    switch (next) {
        case Screen::Title: menu.top = 92.f; menu.maxVisible = 9; break;
        case Screen::Slots: menu.top = 54.f; menu.maxVisible = 9; break;
        case Screen::ConfirmErase: menu.top = 92.f; menu.maxVisible = 9; break;
        case Screen::Options: menu.top = 54.f; menu.maxVisible = 9; break;
        case Screen::Controls: menu.top = 44.f; menu.maxVisible = 10; break;
        case Screen::Paused: menu.top = 78.f; menu.maxVisible = 9; break;
        case Screen::Complete: menu.top = 132.f; menu.maxVisible = 9; break;
        case Screen::Shop: menu.top = 40.f; menu.maxVisible = 10; break;
        case Screen::Records: menu.top = 50.f; menu.maxVisible = 9; break;
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
            items.push_back({MUSIC, "MUSIC", std::to_string(s.musicVolume), true});
            items.push_back({SOUND, "SOUNDS", std::to_string(s.soundVolume), true});
            items.push_back({CONTROLS, "CONTROLS", " ", false});
            items.push_back({BACK, "BACK", " ", false});
            break;
        }
        case Screen::Controls: {
            // Each action's keys, and its button on the gamepad (in the layout of the one used last).
            const controls::PadLayout layout = input.padLayout();
            for (size_t a = 0; a < controls::BINDABLE; ++a) {
                const auto action = static_cast<controls::Action>(a);
                const int button = settings.controls.button(action, layout);
                const std::string pad = button >= 0 ? controls::buttonName(button, layout) : "STICK";
                const bool waiting = rebinding == action;
                items.push_back({FIRST_ACTION + static_cast<int>(a), controls::actionLabel(action), waiting ? "PRESS A KEY" : settings.controls.keysName(action),
                                 false, waiting ? "OR BUTTON" : pad});
            }
            items.push_back({RESET_CONTROLS, "RESET ALL", " ", false});
            items.push_back({BACK, "BACK", " ", false});
            break;
        }
        case Screen::Paused:
            items.push_back(button(RESUME, "RESUME"));
            items.push_back(button(MAP, "MAP"));
            if (hasAbility("stopwatch")) items.push_back(button(RECORDS, "RECORDS"));
            items.push_back(button(RETRY, "RETRY"));
            items.push_back(button(OPTIONS, "OPTIONS"));
            items.push_back(button(MAIN_MENU, "MAIN MENU"));
            break;
        case Screen::Complete:
            items.push_back(button(MAIN_MENU, "MAIN MENU"));
            break;
        case Screen::Shop: {
            // Each outfit, and what it costs, or that it's yours, or that you've got it on. The
            // values sit left of the mirror.
            const std::string wearing = progress.outfit.empty() ? outfits::FIRST : progress.outfit;
            const auto& all = outfits::all();
            for (size_t ii = 0; ii < all.size(); ++ii) {
                const outfits::Outfit& o = all[ii];
                const bool owned = o.price == 0 || progress.outfits.count(o.id) > 0;
                const std::string value = o.id == wearing ? "WEARING" : owned ? "YOURS" : std::to_string(o.price) + " GEMS";
                items.push_back({FIRST_OUTFIT + static_cast<int>(ii), o.name, value, false});
            }
            break;
        }
        case Screen::Records: {
            const std::vector<std::string> routes = trialRoutes();
            for (size_t ii = 0; ii < routes.size(); ++ii) {
                const auto best = progress.records.find(routes[ii]);
                items.push_back({FIRST_ROUTE + static_cast<int>(ii), regions::name(routes[ii]),
                                 best == progress.records.end() ? "-:--.-" : hud::formatTime(best->second), false});
            }
            items.push_back({BACK, "BACK", " ", false});
            break;
        }
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
        case CONTROLS: openScreen(Screen::Controls); break;
        case RESET_CONTROLS:
            settings.controls = {};
            input.setBindings(settings.controls);
            saveSettings();
            refreshMenu();
            break;
        case QUIT: window->close(); break;
        case RESUME: openScreen(Screen::Playing); break;
        case MAP: openMap(); break;
        case RECORDS: openScreen(Screen::Records); break;
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
        case MUSIC:
        case SOUND:
            // Clicking or pressing Enter on a setting steps it forwards.
            changeSetting(choice.id, choice.direction == 0 ? 1 : choice.direction);
            break;
        default:
            if (choice.id >= FIRST_ROUTE) {
                // Only to look at.
            } else if (choice.id >= FIRST_OUTFIT) {
                shopChoice(static_cast<size_t>(choice.id - FIRST_OUTFIT));
            } else if (choice.id >= FIRST_ACTION) {
                rebinding = static_cast<controls::Action>(choice.id - FIRST_ACTION);
                refreshMenu();
            } else if (choice.id >= FIRST_SLOT) {
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
        case Screen::Controls: openScreen(Screen::Options); break;
        case Screen::Slots: openScreen(Screen::Title); break;
        case Screen::ConfirmErase: openScreen(Screen::Slots); break;
        case Screen::Paused: openScreen(Screen::Playing); break;
        case Screen::Complete: openScreen(Screen::Title); break;
        case Screen::Map: openScreen(mapReturn); break;
        case Screen::Talking: endTalk(); break;
        case Screen::Shop: openScreen(Screen::Playing); break;
        case Screen::Records: openScreen(Screen::Paused); break;
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
        case MUSIC:
        case SOUND: {
            int& level = id == MUSIC ? s.musicVolume : s.soundVolume;
            level = (level + direction + 11) % 11;
            audio.setVolumes(s.musicVolume, s.soundVolume);
            break;
        }
        default: break;
    }
    saveSettings();
    refreshMenu();
}

void Game::rebind(const InputManager::Press& press)
{
    const controls::Action action = *rebinding;
    // Escape or Start: never mind. The stick and d-pad stay the directions.
    if (press.key != sf::Keyboard::Unknown && press.key != sf::Keyboard::Escape) {
        if (!settings.controls.bindKey(action, press.key)) return; // Enter: works the menus, try another
    } else if (press.button >= 0) {
        settings.controls.bindButton(action, press.button, input.padLayout());
    } else if (press.key != sf::Keyboard::Escape) {
        return;
    }
    rebinding.reset();
    input.setBindings(settings.controls);
    saveSettings();
    refreshMenu();
    audio.play(Audio::Sfx::MenuPick);
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
        case Screen::Talking:
        case Screen::Shop:
        case Screen::Records: return true;
        case Screen::Options:
        case Screen::Controls: return returnScreen == Screen::Paused;
        case Screen::Title:
        case Screen::Slots:
        case Screen::ConfirmErase: return false;
    }
    return false;
}

std::string Game::slotSummary(const Progress& p) const
{
    // Where it would continue: the room of the bench, or the start.
    const auto bench = findBench(p.bench);
    return hud::upper(levels[bench ? bench->first : 0].title);
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

void Game::drawMenuScreen()
{
    const auto chooseHint = [this] { return std::string(input.usingPad() ? "STICK" : "ARROWS / MOUSE") + " - CHOOSE"; };
    switch (screen) {
        case Screen::Title: {
            hud::dim(canvas, 110);
            // You, swinging on a rope from the top of the screen, above the name.
            const float angle = std::sin(clockTime * 1.7f) * 0.75f;
            const sf::Vector2f anchor(HALF_W, -4.f), feet(anchor.x + std::sin(angle) * 38.f, anchor.y + std::cos(angle) * 38.f + 6.f);
            const sf::Vertex rope[] = {{anchor, sf::Color(200, 190, 170)}, {feet - sf::Vector2f(0.f, 8.f), sf::Color(200, 190, 170)}};
            canvas.draw(rope, 2, sf::Lines);
            player->drawFigure(canvas, 0, {std::round(feet.x), std::round(feet.y)}, std::cos(clockTime * 1.7f) < 0.f, clockTime);
            hud::drawCentred(canvas, hud::upper(TITLE), HALF_W, 46.f, hud::GOLD, 4.f);
            hud::drawCentred(canvas, "SWING. CLIMB. DON'T LOOK DOWN.", HALF_W, 72.f, hud::GREY);
            hud::drawCentred(canvas, chooseHint() + "    " + hint(controls::Action::Confirm) + " - OK", HALF_W, 168.f, hud::GREY);
            break;
        }
        case Screen::Slots:
            hud::dim(canvas, 170);
            hud::drawCentred(canvas, erasing ? "ERASE WHICH?" : "CHOOSE A SLOT", HALF_W, 24.f, erasing ? hud::RED : hud::GOLD, 2.f);
            hud::drawCentred(canvas, chooseHint() + "    " + hint(controls::Action::Confirm) + " - OK    " + hint(controls::Action::Back) + " - BACK", HALF_W, 168.f, hud::GREY);
            break;
        case Screen::ConfirmErase:
            hud::dim(canvas, 190);
            hud::drawCentred(canvas, "ERASE SLOT " + std::to_string(slotToErase + 1) + "?", HALF_W, 40.f, hud::RED, 2.f);
            hud::drawCentred(canvas, "IT'S GONE FOR GOOD.", HALF_W, 62.f, hud::GREY);
            break;
        case Screen::Options:
            hud::dim(canvas, 170);
            hud::drawCentred(canvas, "OPTIONS", HALF_W, 20.f, hud::GOLD, 2.f);
            hud::drawCentred(canvas, std::string(input.usingPad() ? "STICK" : "LEFT / RIGHT") + " - CHANGE    " + hint(controls::Action::Back) + " - BACK", HALF_W, 168.f, hud::GREY);
            break;
        case Screen::Controls:
            hud::dim(canvas, 170);
            hud::drawCentred(canvas, "CONTROLS", HALF_W, 14.f, hud::GOLD, 2.f);
            {
                const float right = Menu::RIGHT - Menu::EXTRA_WIDTH * 0.5f;
                hud::drawText(canvas, "KEYS", {right - hud::textWidth("KEYS"), menu.top - 12.f}, hud::GREY);
                hud::drawText(canvas, "PAD", {right + 12.f, menu.top - 12.f}, hud::GREY);
            }
            hud::drawCentred(canvas, rebinding ? "PRESS THE NEW ONE    ESC - KEEP THE OLD ONE"
                                               : hint(controls::Action::Confirm) + " - CHANGE    " + hint(controls::Action::Back) + " - BACK",
                             HALF_W, 168.f, hud::GREY);
            break;
        case Screen::Paused:
            hud::dim(canvas, 140);
            hud::drawCentred(canvas, "PAUSED", HALF_W, 36.f, hud::WHITE, 3.f);
            hud::drawCentred(canvas, hud::upper(levels[levelIndex].title), HALF_W, 60.f, hud::GREY);
            break;
        case Screen::Complete: {
            hud::dim(canvas, 170);
            hud::drawCentred(canvas, "THE END", HALF_W, 12.f, hud::GOLD, 3.f);
            // How the story ended, what became of everyone, then the numbers: rolling up slowly once
            // the first lines have been read, if there's more than fits, until the last is in view.
            const sf::FloatRect area(0.f, 36.f, config::GAME_WIDTH, 90.f);
            std::vector<std::pair<std::string, sf::Color>> lines;
            for (const auto& [paragraph, color] : credits) {
                if (paragraph.empty()) lines.emplace_back("", color);
                for (const std::string& line : text::wrap(paragraph, 280.f)) lines.emplace_back(line, color);
            }
            const float height = static_cast<float>(lines.size()) * text::LINE_HEIGHT;
            const float scroll = std::clamp((creditsTime - 6.f) * 7.f, 0.f, std::max(0.f, height - area.height));
            sf::View clip(area);
            clip.setViewport({0.f, area.top / config::GAME_HEIGHT, 1.f, area.height / config::GAME_HEIGHT});
            canvas.setView(clip);
            float y = area.top - scroll;
            for (size_t ii = 0; ii < lines.size(); ++ii, y += text::LINE_HEIGHT) {
                // Each line fades in in turn, and out at the edges as it rolls past them.
                const float bottom = area.top + area.height - y - text::LINE_HEIGHT, top = y - area.top;
                const float edges = std::clamp(std::min(bottom, scroll > 0.f ? top : 0.f) / 6.f + 1.f, 0.f, 1.f);
                const float shown = edges * std::clamp((creditsTime - static_cast<float>(ii) * 0.25f) * 2.f, 0.f, 1.f);
                if (lines[ii].first.empty() || shown <= 0.f) continue;
                const std::string& line = lines[ii].first;
                text::draw(canvas, line, {std::floor(HALF_W - text::width(line) * 0.5f), y}, hud::withAlpha(lines[ii].second, static_cast<sf::Uint8>(255.f * shown)));
            }
            canvas.setView(canvas.getDefaultView());
            particles.draw(canvas, drawCamera());
            break;
        }
        case Screen::Map:
            mapScreen->draw(canvas, mapMarks(), input.mousePosition(), clockTime);
            break;
        case Screen::Talking:
            talk->draw(canvas, *player, clockTime);
            break;
        case Screen::Shop: drawShop(); break;
        case Screen::Records: drawRecords(); break;
        case Screen::Playing:
            break;
    }
    menu.draw(canvas, clockTime);
}
