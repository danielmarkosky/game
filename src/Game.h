#pragma once

#include "src/InputManager.h"
#include "src/Level.h"
#include "src/Particles.h"
#include "src/Player.h"
#include <SFML/Graphics.hpp>
#include <memory>
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
    explicit Game(std::vector<LevelInfo> levels);
    void run();

private:
    enum class State { Playing, LevelComplete, GameComplete };

    void loadLevel(size_t index);
    void restartLevel();
    void handleKey(sf::Keyboard::Key key);
    PlayerInput readInput() const;
    void step(float dt);
    void killPlayer();
    void showEffects(const std::vector<PlayerEvent>& events);
    void updateCamera(float dt, bool snap = false);
    sf::Vector2f drawCamera() const;
    void render();
    void drawBackground(sf::Vector2f cam);
    void drawAim();
    void drawHud();

    std::shared_ptr<sf::RenderWindow> window;
    sf::RenderTexture canvas;
    InputManager input;
    sf::Texture bgFar;
    sf::Texture bgNear;

    std::vector<LevelInfo> levels;
    size_t levelIndex = 0;
    std::shared_ptr<Level> level;
    std::unique_ptr<Player> player;
    Particles particles;
    std::mt19937 rng{7};

    State state = State::Playing;
    sf::Vector2f camera;
    sf::Vector2f checkpoint;
    float runTime = 0.f;
    float clockTime = 0.f;   // never stops, drives animations
    float deathTimer = 0.f;  // > 0 while dead, counting down to the respawn
    float deathFlash = 0.f;
    float shake = 0.f;
    float titleTimer = 0.f;
    int deaths = 0;
    float totalTime = 0.f;   // over the levels finished and moved on from
    int totalDeaths = 0;
    bool ignoreInputThisFrame = false;
};
