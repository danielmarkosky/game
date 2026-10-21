#pragma once

#include "src/InputManager.h"
#include "src/Level.h"
#include "src/Menu.h"
#include "src/Particles.h"
#include "src/Player.h"
#include "src/Save.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

struct LevelInfo
{
    std::string path;
    std::string title;
};

// Reads assets/levels/levels.txt style lists: one level per line, "file.txt Title of the level".
// Paths are relative to the list's folder. Blank lines and lines starting with '#' are skipped.
std::vector<LevelInfo> loadLevelList(const std::string& listPath);

// Owns the window and runs the main loop: input once per frame, physics at a fixed rate,
// then rendering at the game's low resolution scaled up to the window.
class Game
{
public:
    // With `testing` set (playing level files given on the command line) the game skips the title
    // screen and doesn't record results.
    explicit Game(std::vector<LevelInfo> levels, bool testing = false);
    void run();

private:
    enum class Screen { Title, LevelSelect, Options, Playing, Paused, Complete };

    // Moving from one level to the next: the old level slides out while the new one slides in.
    struct Transition
    {
        std::shared_ptr<Level> from;
        sf::Vector2f fromCamera;
        sf::Vector2f fromCheckpoint;
        std::optional<Level::Side> side; // none: a fade, for levels finished at a goal flag
        float time = 0.f;
    };

    // Screens
    void openScreen(Screen next);
    void refreshMenu();
    void onMenuChoice(MenuChoice choice);
    void back();
    void changeSetting(int id, int direction);
    void applyWindowSettings();
    int maxWindowScale() const;
    bool inGame() const; // a level is being played, possibly paused
    std::optional<size_t> continueIndex() const;
    std::string levelKey(size_t index) const;
    size_t skinIndex() const; // of the saved skin; the first one if the name is unknown
    void writeSave();

    // Playing
    void startGame(size_t index);
    void loadLevel(size_t index);
    void restartLevel();
    void finishLevel(std::optional<Level::Side> side);
    void handleKey(sf::Keyboard::Key key);
    PlayerInput readInput() const;
    sf::FloatRect pauseButton() const;
    void step(float dt);
    void killPlayer();
    void showEffects(const std::vector<PlayerEvent>& events);
    void updateCamera(float dt, bool snap = false);
    sf::Vector2f clampToLevel(sf::Vector2f cam) const;
    sf::Vector2f drawCamera() const;
    sf::Vector2f titleCamera() const;

    // Drawing
    void render();
    void drawWorld(sf::Vector2f cam, bool withPlayer = true);
    void drawTransition();
    void drawBackground(float scrollX);
    void drawAim();
    void drawCrosshair(sf::Color color);
    void drawHud();
    void drawMenuScreen();

    std::vector<LevelInfo> levels;
    bool testing;
    std::string savePath;
    SaveData save;
    bool reapplyWindow = false; // window settings changed, recreate it once the events are handled

    std::shared_ptr<sf::RenderWindow> window;
    sf::RenderTexture canvas;
    InputManager input;
    sf::Texture bgFar;
    sf::Texture bgNear;

    Screen screen = Screen::Title;
    Screen returnScreen = Screen::Title; // where Back goes from the level select and the options
    Menu menu;
    sf::Vector2f lastMouse;

    size_t levelIndex = 0;
    std::shared_ptr<Level> level;
    std::unique_ptr<Player> player;
    Particles particles;
    std::mt19937 rng{7};
    std::optional<Transition> transition;

    sf::Vector2f camera;
    // Where the current level sits in one imaginary long world, so the background scrolls on
    // smoothly from level to level instead of jumping.
    float worldOffsetX = 0.f;
    sf::Vector2f checkpoint;
    float runTime = 0.f;
    float clockTime = 0.f;   // never stops, drives animations
    float deathTimer = 0.f;  // > 0 while dead, counting down to the respawn
    float deathFlash = 0.f;
    float shake = 0.f;
    float titleTimer = 0.f;
    int deaths = 0;
    float totalTime = 0.f;   // over the levels finished since the game was started
    int totalDeaths = 0;
    size_t firstLevelPlayed = 0;
    std::string lastResult;  // shown under the next level's title
    bool ignoreInputThisFrame = false;   // right after a (re)start, so the key that started it doesn't jump
    bool ignorePressesThisFrame = false; // right after unpausing: held buttons count, new presses don't
};
