#pragma once

#include <SFML/Audio.hpp>
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Plays the sound effects and the music, all made by Synth: the effects from the table in
// Audio.cpp, the songs from assets/music/*.txt. The songs take a moment to make, so that happens
// in the background when the game starts; a song asked for before it's ready starts once it is.
class Audio
{
public:
    enum class Sfx
    {
        Jump, AirJump, Land, RopeClink, Death, Respawn, Gem, Pickup, Checkpoint,
        Bench, Travel, MenuMove, MenuPick, Talk, Door, Throw, Hooked, Grab,
        Count // not a sound
    };

    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // `pitch` 1 is as made; 2 an octave up.
    void play(Sfx sound, float pitch = 1.f);
    // Fades over to a song, by its file name without ".txt". Empty for none.
    void playMusic(const std::string& name);
    // Each frame: fades the music, and starts songs once they're made.
    void update(float dt);
    // 0 to 10.
    void setVolumes(int music, int sound);

    // The songs in assets/music/.
    static std::vector<std::string> songNames();

private:
    std::map<Sfx, sf::SoundBuffer> effects;
    std::array<sf::Sound, 12> voices;
    size_t nextVoice = 0;
    float soundVolume = 80.f;
    float musicVolume = 60.f;

    // Made in the background, turned into buffers on the main thread.
    std::thread maker;
    std::atomic<bool> stopMaking{false};
    std::mutex made;
    std::map<std::string, std::vector<sf::Int16>> samples;
    std::map<std::string, std::unique_ptr<sf::SoundBuffer>> songs;

    std::string wanted;          // the song asked for
    sf::Sound music, fadingOut;  // playing, and the one before it fading away
    float fade = 1.f;            // 0 to 1: how far into the song playing it has faded in
};
