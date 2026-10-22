#pragma once

#include "src/InputManager.h"
#include "src/Level.h"
#include "src/Menu.h"
#include "src/Particles.h"
#include "src/Player.h"
#include "src/Save.h"
#include "src/Theme.h"
#include "src/WorldMap.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

struct LevelInfo
{
    std::string path;
    std::string title;
    std::string theme = Theme::DEFAULT;
    sf::Vector2i position{}; // on the world map, in tiles: where the level's bottom-left tile is
};

// Reads assets/levels/levels.txt style lists: one level per line, "file.txt x y Title of the level".
// A "theme <name>" line sets the theme of the levels after it. Paths are relative to the list's
// folder. Blank lines and lines starting with '#' are skipped.
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
    enum class Screen { Title, LevelSelect, Options, Playing, Paused, Complete, Map };

    // Moving from one room into the next: the camera pans across from the old one.
    struct Transition
    {
        size_t from;                 // the room left
        sf::Vector2f fromCamera;     // where the camera was, in the new room's coordinates
        sf::Vector2f fromCheckpoint; // its flag stays up while it's in view
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
    enum class Start
    {
        New,      // from scratch: no abilities, nothing on the map
        Continue, // with what the save has
        From,     // from a room picked in the level select, with what the rooms before it give
    };
    void startGame(size_t index, Start how);
    void loadLevel(size_t index);
    void spawnAtStart();
    void enterRoom(size_t index, Level::Side side);
    void recordResult();
    void finishGame();
    // The room other than the current one that a point (in the current room's coordinates) is in.
    std::optional<size_t> roomAt(sf::Vector2f point) const;
    sf::Vector2f roomOrigin(size_t index) const; // world position of the room's (0, 0)

    // Abilities and gems
    bool hasAbility(const std::string& ability) const;
    void applyAbilities(); // to the player
    void collect(const Pickup& pickup);
    void showMessage(std::string title, std::string text);
    std::string gemKey(size_t room, size_t gem) const;
    bool gemFound(size_t room, size_t gem) const;
    int gemCount() const;     // in the whole world
    std::string gemTotal() const;            // "GEMS 5/20"
    std::string gemTally(size_t room) const; // "1/2", or empty if the room has none

    // Map
    void openMap();
    void updateMap(float dt, sf::Vector2f mouseMove);
    void zoomMap(int steps); // + is closer
    std::vector<bool> exploredRooms() const;
    sf::Vector2f playerTile() const; // where the player is on the map
    WorldMap::View mapView() const;
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
    void drawRooms(sf::Vector2f cam); // the current room and any others on screen
    void drawTransition();
    void drawBackground(const Theme& theme, float scrollX);
    void drawAim();
    void drawCrosshair(sf::Color color);
    void drawHud();
    void drawMenuScreen();
    void drawMap();

    std::vector<LevelInfo> levels;
    bool testing;
    std::string savePath;
    SaveData save;
    bool reapplyWindow = false; // window settings changed, recreate it once the events are handled

    std::shared_ptr<sf::RenderWindow> window;
    sf::RenderTexture canvas;
    InputManager input;

    Screen screen = Screen::Title;
    Screen returnScreen = Screen::Title; // where Back goes from the level select and the options
    Menu menu;
    sf::Vector2f lastMouse;

    std::vector<std::shared_ptr<Level>> rooms; // one per entry in `levels`
    size_t levelIndex = 0;
    std::shared_ptr<Level> level;              // rooms[levelIndex]
    std::vector<bool> visited;                 // rooms entered since the game was started
    std::unique_ptr<WorldMap> worldMap;
    sf::Vector2f mapCentre;                    // in world tiles
    int mapZoom = 1;                           // one of WorldMap::ZOOMS
    Screen mapReturn = Screen::Playing;        // where closing the map goes
    std::unique_ptr<Player> player;
    Particles particles;
    std::mt19937 rng{7};
    std::optional<Transition> transition;

    sf::Vector2f camera;
    sf::Vector2f checkpoint;
    bool needsSpawnPoint = false; // just came into the room: where the player first lands becomes the checkpoint
    float runTime = 0.f;
    float clockTime = 0.f;   // never stops, drives animations
    float deathTimer = 0.f;  // > 0 while dead, counting down to the respawn
    float deathFlash = 0.f;
    float shake = 0.f;
    float titleTimer = 0.f;
    std::string messageTitle, messageText; // shown in the middle of the screen for a while, instead of the level's title
    float messageTimer = 0.f;
    bool shownRockHint = false;
    std::set<std::string> gems;  // found, by gemKey: the save's, except when testing a level
    float gemCounterTimer = 0.f; // the count shows for a moment after finding one
    int deaths = 0;
    float totalTime = 0.f;   // over the rooms played since the game was started
    int totalDeaths = 0;
    size_t firstLevelPlayed = 0;
    std::string lastResult;  // shown under the next level's title
    bool ignoreInputThisFrame = false;   // right after a (re)start, so the key that started it doesn't jump
    bool ignorePressesThisFrame = false; // right after unpausing: held buttons count, new presses don't
};
