#pragma once

#include "src/Ambience.h"
#include "src/Audio.h"
#include "src/Dog.h"
#include "src/InputManager.h"
#include "src/Level.h"
#include "src/Menu.h"
#include "src/Particles.h"
#include "src/Player.h"
#include "src/Save.h"
#include "src/Talk.h"
#include "src/Theme.h"
#include "src/World.h"
#include "src/MapScreen.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

// Owns the window and runs the main loop: input once per frame, physics at a fixed rate,
// then rendering at the game's low resolution scaled up to the window.
class Game
{
public:
    struct Options
    {
        // A level file or a room given on the command line: the game starts straight away in
        // `startRoom`, with every ability, and saves nothing but the settings.
        bool testing = false;
        size_t startRoom = 0;
        // No window, nothing heard, saves in `saveFolder`: for tests and pictures, driven with
        // the functions below instead of run().
        bool headless = false;
        std::string saveFolder; // empty: the usual one
        // Where to start in `startRoom`, in its pixels, instead of at its Start (the room editor's
        // "play from here").
        std::optional<sf::Vector2f> startAt;
        bool sounds = false; // headless, but heard all the same (the room editor plays it so)
    };
    Game(std::vector<LevelInfo> levels, Options options);
    void run();

    // Driving the game without a window: keys and gamepad buttons pressed and let go, time
    // passing (with `held` as the player's input the whole while), and pictures of what it looks like.
    void pressKey(sf::Keyboard::Key key);
    void pressButton(int button);
    void advance(float seconds, const PlayerInput& held = {});
    sf::Image screenshot();
    // Driven by someone else's window (the room editor's): one frame of `frameTime` seconds with
    // that input, and the picture of it. `aim` in the input is in the picture's pixels.
    const sf::Texture& play(float frameTime, PlayerInput input);
    // What tests look at.
    std::string screenName() const;
    std::string roomName() const;
    const Progress& state() const;
    bool isSitting() const;
    sf::Vector2f playerPosition() const;

private:
    enum class Screen { Title, Slots, ConfirmErase, Options, Controls, Playing, Paused, Complete, Map, Talking, Shop, Records };

    // Moving from one room into the next: the camera eases across from where it was in the old one,
    // while you carry on moving.
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
    std::string slotSummary(const Progress& p) const; // where it'd carry on: "THE HIDDEN LAKE"
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
    // A way out of this room into one that looks different: as you near it, the look of the room
    // beyond comes in, `amount` of it, up to half and half in the doorway, where the room beyond, from
    // its side, has this one's half in. So places run into each other instead of stopping at the edge.
    struct NextDoor
    {
        size_t room;
        float amount; // 0 to 0.5
    };
    std::optional<NextDoor> nextDoorLook() const;
    sf::Vector2f roomOrigin(size_t index) const; // world position of the room's (0, 0)

    // Abilities, story flags and gems
    bool hasAbility(const std::string& ability) const;
    bool hasFact(const std::string& fact) const; // an ability, or a flag set in a conversation
    void applyAbilities(); // to the player
    void toggleAbility(size_t index); // in a test room: on or off, by its number in the list
    bool touchesSlick(sf::Vector2f point) const; // a point on a surface of the room: is it slick stone?
    void collect(const Pickup& pickup);
    void showMessage(std::string title, std::string text);
    void showNotice(std::string text); // a small line in the corner for a moment
    std::string gemKey(size_t room, size_t gem) const;
    bool gemFound(size_t room, size_t gem) const;
    int gemCount() const;     // in the whole world
    std::string gemTotal() const;            // "GEMS 5/20"
    std::string gemTally(size_t room) const; // "1/2", or empty if the room has none

    // What a conversation does (see Dialogue::isCommand): "give ability rope", "open shop".
    void act(const std::string& command);
    // Whether someone is there now: their `needs` holds and their `until` doesn't.
    bool isAround(const Npc& npc) const;

    // Mags's stall: outfits for gems. What you're wearing is put on the player.
    int gemsFound() const;
    int gemsToSpend() const;
    void applyOutfit();
    void shopChoice(size_t outfit); // buy it, or put it on
    void stallSays(std::string text); // under the list, for a moment

    // Mine carts: getting in, riding, getting out at the end of the line.
    bool cartReady(const Cart& cart) const; // its `needs` all hold
    void updateCart();

    // The stopwatch: runs of a route timed from its start post to its finish, and the best kept.
    void updateTrial(); // starting or finishing one, at a post
    std::vector<std::string> trialRoutes() const; // every route with a start post, in the world's order

    // Pip, once found.
    bool hasDog() const;
    void updateDog(float dt);

    // Talking
    void startTalk(const Npc& npc);
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

    // Levers: pulling one opens the gates in its room, from then on.
    std::optional<size_t> leverInReach() const;
    void pullLever();
    void applyGates(); // every room's gates, open or shut, and lamps, lit or not, as the slot says
    std::string lampKey(size_t room, size_t lamp) const; // the flag that it's lit

    // Map
    void openMap(bool travel = false);
    std::string songNow() const; // what should be playing
    MapScreen::Marks mapMarks() const; // what to show on it
    void handlePress(const InputManager::Press& press);
    // F5: assets/physics.txt again, for tuning how it moves while playing.
    void reloadPhysics();
    // Controls: choosing an action in the menu waits for its new key or button, the next press.
    void rebind(const InputManager::Press& press);
    // What to press for an action, on the keyboard or the gamepad, whichever was used last: "W", "A".
    std::string hint(controls::Action action) const;

    sf::FloatRect pauseButton() const;
    void step(float dt);
    void killPlayer();
    void showEffects(const std::vector<PlayerEvent>& events);
    // The room's own sounds, louder the nearer they are: cracking stone, icicles.
    void playLevelSounds();
    // The wind and the heat of updrafts, by where you are.
    void updateLoops();
    void updateCamera(float dt, bool snap = false);
    sf::Vector2f clampToLevel(sf::Vector2f cam) const;
    sf::Vector2f drawCamera() const;
    sf::Vector2f titleCamera() const;

    // One frame: input, the time passing in fixed steps, sounds and the air.
    void frame(float frameTime, std::optional<PlayerInput> scripted = std::nullopt);
    PlayerInput playerInput();

    // Drawing
    void renderCanvas();
    void render(); // the canvas, scaled up to the window
    void drawWorld(sf::Vector2f cam, bool withPlayer = true);
    void drawRooms(sf::Vector2f cam); // the current room and any others on screen
    void drawAir(sf::Vector2f cam);   // what's in front: near motes, the dark and the lights
    void drawBeams();                 // light shafts (see props::LIGHT_SHAFT)
    void drawFront();                 // in front of the player: the water, and waterfalls (props::WATERFALL)
    void drawBackground(sf::RenderTarget& target, const Theme& theme, float scrollX);
    // Another room's scenery, over `area` of the screen (where that room is), fading in across the
    // seam with the current room's instead of starting at a hard line.
    void drawBackdrop(const Theme& theme, float scrollX, sf::FloatRect area);
    // Where a rope shot now would go (in the world): at the mouse, or along the stick.
    sf::Vector2f aimPoint() const;
    void drawAim();
    void drawCrosshair(sf::Vector2f at, sf::Color color); // at a screen position
    void drawHud();
    void drawMenuScreen();
    void drawShop();
    void drawRecords();

    std::vector<LevelInfo> levels;
    bool testing;
    bool headless;
    std::optional<sf::Vector2f> startAt;
    float accumulator = 0.f; // time not yet stepped through
    SaveFiles files;
    Settings settings;
    Progress progress;      // of the slot being played
    int slot = -1;          // being played, counting from 0; -1 when testing
    bool erasing = false;   // on the slot screen, choosing one to erase
    int slotToErase = -1;
    bool reapplyWindow = false; // window settings changed, recreate it once the events are handled

    std::shared_ptr<sf::RenderWindow> window;
    sf::RenderTexture canvas;
    sf::RenderTexture backdrop; // another room's scenery, before it's faded in over the canvas
    InputManager input;
    Audio audio;
    Ambience ambience;
    std::vector<Ambience::Light> lights; // gathered while drawing the rooms
    struct Beam
    {
        sf::Vector2f top; // screen pixels
        float length;
        float seed;       // so they don't all sway together
    };
    std::vector<Beam> beams;             // the same
    struct Fall
    {
        sf::Vector2f top; // screen pixels, the middle of it
        float length;
        float seed;
    };
    std::vector<Fall> falls;             // the same
    std::vector<std::pair<size_t, sf::Vector2f>> roomsDrawn; // and the rooms on screen, with the camera in each one's coordinates
    sf::Vector2f lastCamera;             // world position, to move the motes along with it
    float talkBlips = 0.f; // letters typed since the last talking sound

    Screen screen = Screen::Title;
    Screen returnScreen = Screen::Title; // where Back goes from the options
    std::optional<controls::Action> rebinding; // in the controls, waiting for this one's new key
    Menu menu;
    sf::Vector2f lastMouse;

    std::vector<std::shared_ptr<Level>> rooms; // one per entry in `levels`
    size_t levelIndex = 0;
    std::shared_ptr<Level> level;              // rooms[levelIndex]
    std::vector<bool> visited;                 // rooms entered since the game was started
    std::unique_ptr<MapScreen> mapScreen;
    Screen mapReturn = Screen::Playing;        // where closing the map goes
    std::unique_ptr<Player> player;
    Particles particles;
    std::mt19937 rng{7};
    std::optional<Transition> transition;
    Dog dog;
    float sniffTimer = 0.f;   // until Pip next points out a gem
    struct Run
    {
        std::string route;
        float time = 0.f;
    };
    std::optional<Run> timedRun; // going on
    float runShown = 0.f;     // > 0: a finished run's time stays up this long
    std::string runText;      // and what it says
    bool cartRefused = false; // said already that this cart won't go, until you step away
    std::optional<std::pair<size_t, sf::Vector2f>> parked; // where a cart was left at the end of the line: room, its bottom middle

    sf::Vector2f camera;
    float lookAhead = 0.f; // how far ahead of you the camera is looking, see updateCamera
    float standingY = 0.f; // the height you last stood at
    sf::Vector2f checkpoint;
    int checkpointRiding = 0;     // riding a cart that way when the checkpoint was set: back in it
    bool needsSpawnPoint = false; // just came into the room: where the player first lands becomes the checkpoint
    std::optional<size_t> sitting; // the bench in this room the player is sitting on
    bool waking = false;           // a new game: sitting up on the floor until moving
    float clockTime = 0.f;   // never stops, drives animations
    float deathTimer = 0.f;  // > 0 while dead, counting down to the respawn
    float deathFlash = 0.f;
    float shake = 0.f;
    float titleTimer = 0.f;
    std::string messageTitle, messageText; // shown in the middle of the screen for a while, instead of the level's title
    float messageTimer = 0.f;
    bool shownRockHint = false;
    std::optional<Talk> talk;
    bool shopping = false;   // the conversation going on opens the stall when it's done
    std::string stallNote;   // what Mags says about the last choice (see stallSays)
    float stallNoteUntil = 0.f;
    sf::Vector2f talkedTo;   // the feet of whoever that is
    std::string notice;      // see showNotice
    float noticeTimer = 0.f;
    float fade = 0.f;        // > 0 while fading in after travelling
    // What the end screen rolls up: how the climb went, and thanks. A colour for each line, "" a gap.
    std::vector<std::pair<std::string, sf::Color>> credits;
    float creditsTime = 0.f; // seconds the end screen's been up
    std::optional<sf::Vector2f> playedMouse; // driven by play(): where its mouse is, in the picture's pixels
    bool ignoreInputThisFrame = false;   // right after a (re)start, so the key that started it doesn't jump
    bool ignorePressesThisFrame = false; // right after unpausing: held buttons count, new presses don't
};
