#include "Audio.h"
#include "Config.h"
#include "Synth.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    constexpr float FADE_TIME = 1.5f; // seconds to cross over from one song to the next

    using synth::Effect;
    using synth::Wave;

    Effect tone(Wave wave, float fromHz, float toHz, float seconds, float attack, float release, float volume,
                float noise = 0.f, std::vector<int> arpeggio = {})
    {
        return {wave, fromHz, toHz, seconds, attack, release, volume, noise, std::move(arpeggio)};
    }

    // What every sound is. Tweak and listen: they're made fresh every time the game starts.
    Effect effect(Audio::Sfx sound)
    {
        switch (sound) {
            case Audio::Sfx::Jump: return tone(Wave::Square, 260.f, 520.f, 0.12f, 0.002f, 0.08f, 0.22f);
            case Audio::Sfx::AirJump: return tone(Wave::Pulse, 420.f, 900.f, 0.18f, 0.002f, 0.12f, 0.2f, 0.35f);
            case Audio::Sfx::Land: return tone(Wave::Noise, 300.f, 120.f, 0.09f, 0.001f, 0.07f, 0.3f);
            case Audio::Sfx::RopeShot: return tone(Wave::Noise, 2400.f, 900.f, 0.08f, 0.001f, 0.06f, 0.18f);
            case Audio::Sfx::RopeClink: return tone(Wave::Square, 2300.f, 1900.f, 0.07f, 0.001f, 0.06f, 0.16f);
            case Audio::Sfx::Death: return tone(Wave::Saw, 420.f, 60.f, 0.45f, 0.002f, 0.3f, 0.32f, 0.5f);
            case Audio::Sfx::Respawn: return tone(Wave::Sine, 520.f, 520.f, 0.32f, 0.01f, 0.15f, 0.3f, 0.f, {0, 4, 7, 12});
            case Audio::Sfx::Gem: return tone(Wave::Sine, 1046.f, 1046.f, 0.28f, 0.002f, 0.18f, 0.32f, 0.f, {0, 7, 12});
            case Audio::Sfx::Pickup: return tone(Wave::Square, 392.f, 392.f, 0.9f, 0.005f, 0.4f, 0.22f, 0.f, {0, 4, 7, 12, 7, 12, 16, 19});
            case Audio::Sfx::Checkpoint: return tone(Wave::Triangle, 660.f, 660.f, 0.3f, 0.003f, 0.15f, 0.3f, 0.f, {0, 4, 7});
            case Audio::Sfx::Bench: return tone(Wave::Sine, 392.f, 392.f, 0.8f, 0.02f, 0.4f, 0.3f, 0.f, {0, 4, 7, 11});
            case Audio::Sfx::Travel: return tone(Wave::Sine, 180.f, 900.f, 0.7f, 0.1f, 0.3f, 0.25f, 0.4f);
            case Audio::Sfx::MenuMove: return tone(Wave::Square, 880.f, 880.f, 0.035f, 0.001f, 0.025f, 0.1f);
            case Audio::Sfx::MenuPick: return tone(Wave::Square, 600.f, 900.f, 0.09f, 0.001f, 0.05f, 0.14f);
            case Audio::Sfx::Talk: return tone(Wave::Square, 330.f, 330.f, 0.035f, 0.001f, 0.02f, 0.08f);
            case Audio::Sfx::Door: return tone(Wave::Noise, 160.f, 70.f, 0.5f, 0.02f, 0.3f, 0.3f);
        }
        return {};
    }

    sf::SoundBuffer buffer(const std::vector<sf::Int16>& samples)
    {
        sf::SoundBuffer b;
        if (!samples.empty()) b.loadFromSamples(samples.data(), samples.size(), 1, synth::RATE);
        return b;
    }
}

std::vector<std::string> Audio::songNames()
{
    std::vector<std::string> names;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(config::assetPath("music"), error)) {
        if (entry.path().extension() == ".txt") names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

Audio::Audio()
{
    for (int s = 0; s <= static_cast<int>(Sfx::Door); ++s) effects[static_cast<Sfx>(s)] = buffer(synth::render(effect(static_cast<Sfx>(s))));
    music.setLoop(true);
    // The songs: a few seconds of work, so not holding up the start.
    maker = std::thread([this] {
        for (const std::string& name : songNames()) {
            if (stopMaking) return;
            std::ifstream file(config::assetPath("music/" + name + ".txt"));
            std::stringstream text;
            text << file.rdbuf();
            try {
                auto rendered = synth::render(synth::parseSong(text.str()));
                std::lock_guard<std::mutex> lock(made);
                samples[name] = std::move(rendered);
            } catch (const std::exception&) {
                // A broken song is skipped; game_tests checks they all read.
            }
        }
    });
}

Audio::~Audio()
{
    stopMaking = true;
    if (maker.joinable()) maker.join();
    music.stop();
    fadingOut.stop();
    for (auto& voice : voices) voice.stop();
}

void Audio::setVolumes(int musicLevel, int soundLevel)
{
    musicVolume = 10.f * static_cast<float>(std::clamp(musicLevel, 0, 10));
    soundVolume = 10.f * static_cast<float>(std::clamp(soundLevel, 0, 10));
}

void Audio::play(Sfx sound, float pitch)
{
    if (soundVolume <= 0.f) return;
    sf::Sound& voice = voices[nextVoice];
    nextVoice = (nextVoice + 1) % voices.size();
    voice.setBuffer(effects[sound]);
    voice.setPitch(pitch);
    voice.setVolume(soundVolume);
    voice.play();
}

void Audio::playMusic(const std::string& name)
{
    if (name == wanted) return;
    wanted = name;
    // Whatever's playing fades out; the new song fades in once it's ready (see update).
    if (music.getStatus() == sf::Sound::Playing) {
        fadingOut.stop();
        fadingOut.setBuffer(*music.getBuffer());
        fadingOut.setPlayingOffset(music.getPlayingOffset());
        fadingOut.setLoop(true);
        fadingOut.setVolume(music.getVolume());
        fadingOut.play();
        music.stop();
    }
    fade = 0.f;
}

void Audio::update(float dt)
{
    {
        std::lock_guard<std::mutex> lock(made);
        for (auto& [name, rendered] : samples) songs[name] = std::make_unique<sf::SoundBuffer>(buffer(rendered));
        samples.clear();
    }
    if (!wanted.empty() && music.getStatus() != sf::Sound::Playing) {
        if (auto song = songs.find(wanted); song != songs.end()) {
            music.setBuffer(*song->second);
            music.play();
        }
    }
    fade = std::min(1.f, fade + dt / FADE_TIME);
    music.setVolume(musicVolume * fade);
    fadingOut.setVolume(musicVolume * (1.f - fade) * (fadingOut.getStatus() == sf::Sound::Playing ? 1.f : 0.f));
    if (fade >= 1.f) fadingOut.stop();
}
