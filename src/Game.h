#pragma once

#include "src/InputManager.h"
#include "src/Level.h"
#include "src/Menu.h"
#include "src/Particles.h"
#include "src/Player.h"
#include "src/Dialogue.h"
#include "src/Save.h"
#include "src/Theme.h"
#include "src/World.h"
#include "src/WorldMap.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

// Owns the window and runs the main loop: input once per frame, physics at a fixed rate,
// then rendering at the game's low resolution scaled up to the window.
class Game
{
public:
    // With `testing` set (a level file or a room given on the command line), the game starts
    // straight away in room `startRoom`, with every ability, and saves nothing but the settings.
    explicit Game(std::vector<LevelInfo> levels, bool testing = false, size_t startRoom = 0);
    void run();

private:
    enum class Screen { Title, Slots, ConfirmErase, Options, Playing, Paused, Complete, Map, Talking };

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
    std::string levelKey(size_t index) const;
    size_t skinIndex() const; // of the saved skin; the first one if the name is unknown
    std::string slotSummary(const Progress& p) const; // "THE CROSSROADS" and "1:02:03  5/22"
    void saveSettings();
    void saveProgress(); // to the slot being played, unless testing

    // Playing
    void startGame(int slot); // where that slot left off, or from the start if it's empty
    void loadLevel(size_t index);
    void spawnAtStart();
    void enterRoom(size_t index, Level::Side side);
    void finishGame();
    // The room other than the current one that a point (in the current room's coordinates) is in.
    std::optional<size_t> roomAt(sf::Vector2f point) const;
    sf::Vector2f roomOrigin(size_t index) const; // world position of the room's (0, 0)

    // Abilities and gems
    bool hasAbility(const std::string& ability) const;
    void applyAbilities(); // to the player
    void collect(const Pickup& pickup);
    void showMessage(std::string title, std::string text);
    void showNotice(std::string text); // a small line in the corner for a moment
    std::string gemKey(size_t room, size_t gem) const;
    bool gemFound(size_t room, size_t gem) const;
    int gemCount() const;     // in the whole world
    std::string gemTotal() const;            // "GEMS 5/20"
    std::string gemTally(size_t room) const; // "1/2", or empty if the room has none

    // Talking
    struct Page
    {
        std::string speaker;
        std::vector<std::string> lines; // wrapped to fit the box
    };
    struct Talk
    {
        Npc npc;
        size_t conversation = 0;
        std::vector<Page> pages;
        size_t page = 0;
        float typed = 0.f; // characters of the page shown so far
    };
    void startTalk(const Npc& npc);
    void advanceTalk();
    void endTalk();
    // Someone in the room close enough to talk to.
    const Npc* npcInReach() const;

    // Benches: sitting on one saves, and from one you can travel to any other you've sat on.
    std::optional<size_t> benchInReach() const;
    std::string benchKey(size_t room, size_t bench) const; // as in Progress
    std::optional<std::pair<size_t, size_t>> findBench(const std::string& key) const; // room, bench
    void sitDown(size_t bench); // on that bench of the current room
    std::vector<std::string> travelDestinations() const; // the benches sat on, in room order
    void travelTo(const std::string& key);

    // Map
    void openMap(bool travel = false);
    void chooseDestination(int step);
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
    void drawTalk();

    std::vector<LevelInfo> levels;
    bool testing;
    SaveFiles files;
    Settings settings;
    Progress progress;      // of the slot being played
    int slot = -1;          // being played, counting from 0; -1 when testing
    bool erasing = false;   // on the slot screen, choosing one to erase
    int slotToErase = -1;
    bool reapplyWindow = false; // window settings changed, recreate it once the events are handled

    std::shared_ptr<sf::RenderWindow> window;
    sf::RenderTexture canvas;
    InputManager input;

    Screen screen = Screen::Title;
    Screen returnScreen = Screen::Title; // where Back goes from the options
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
    bool travelling = false;                   // the map is open to choose a bench to travel to
    size_t travelChoice = 0;                   // in travelDestinations()
    std::unique_ptr<Player> player;
    Particles particles;
    std::mt19937 rng{7};
    std::optional<Transition> transition;

    sf::Vector2f camera;
    sf::Vector2f checkpoint;
    bool needsSpawnPoint = false; // just came into the room: where the player first lands becomes the checkpoint
    std::optional<size_t> sitting; // the bench in this room the player is sitting on
    float clockTime = 0.f;   // never stops, drives animations
    float deathTimer = 0.f;  // > 0 while dead, counting down to the respawn
    float deathFlash = 0.f;
    float shake = 0.f;
    float titleTimer = 0.f;
    std::string messageTitle, messageText; // shown in the middle of the screen for a while, instead of the level's title
    float messageTimer = 0.f;
    bool shownRockHint = false;
    std::optional<Talk> talk;
    std::string notice;      // see showNotice
    float noticeTimer = 0.f;
    float fade = 0.f;        // > 0 while fading in after travelling
    bool ignoreInputThisFrame = false;   // right after a (re)start, so the key that started it doesn't jump
    bool ignorePressesThisFrame = false; // right after unpausing: held buttons count, new presses don't
};
