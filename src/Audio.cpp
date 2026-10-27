#include "Audio.h"
#include "Config.h"
#include "Synth.h"
#include <algorithm>
#include <cmath>
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
            case Audio::Sfx::Throw: return tone(Wave::Noise, 900.f, 2600.f, 0.14f, 0.03f, 0.08f, 0.12f);  // a whoosh
            case Audio::Sfx::Hooked: return tone(Wave::Square, 1500.f, 700.f, 0.06f, 0.001f, 0.05f, 0.14f, 0.3f); // a bite into the wall
            case Audio::Sfx::Grab: return tone(Wave::Noise, 500.f, 250.f, 0.06f, 0.002f, 0.05f, 0.18f);    // a hand slapping rock
            case Audio::Sfx::Crack: return tone(Wave::Noise, 2400.f, 900.f, 0.1f, 0.001f, 0.08f, 0.22f);  // stone starting to give
            case Audio::Sfx::Crumble: return tone(Wave::Noise, 260.f, 60.f, 0.45f, 0.005f, 0.35f, 0.3f);  // and going
            case Audio::Sfx::CloudPuff: return tone(Wave::Noise, 700.f, 300.f, 0.35f, 0.08f, 0.25f, 0.1f); // a cloud thinning
            case Audio::Sfx::IcicleShake: return tone(Wave::Sine, 2600.f, 2500.f, 0.3f, 0.002f, 0.2f, 0.12f, 0.f, {0, 3, 0, 3, 0, 3});
            case Audio::Sfx::Shatter: return tone(Wave::Noise, 5000.f, 1800.f, 0.3f, 0.001f, 0.25f, 0.24f, 0.f, {0, 7, 0, 5});
            case Audio::Sfx::LampLit: return tone(Wave::Triangle, 330.f, 660.f, 0.5f, 0.01f, 0.35f, 0.26f, 0.25f, {0, 7, 12});
            case Audio::Sfx::Splash: return tone(Wave::Noise, 1800.f, 300.f, 0.4f, 0.002f, 0.3f, 0.26f);   // into the water
            case Audio::Sfx::Stroke: return tone(Wave::Noise, 700.f, 400.f, 0.18f, 0.03f, 0.12f, 0.08f);   // a swimming stroke
            case Audio::Sfx::Bubble: return tone(Wave::Sine, 500.f, 1100.f, 0.07f, 0.002f, 0.05f, 0.12f);   // a bubble rising
            case Audio::Sfx::Gasp: return tone(Wave::Noise, 1400.f, 2600.f, 0.3f, 0.04f, 0.2f, 0.16f);    // a breath, coming up
            case Audio::Sfx::Bounce: return tone(Wave::Sine, 180.f, 620.f, 0.22f, 0.002f, 0.15f, 0.3f);      // boing
            case Audio::Sfx::Bark: return tone(Wave::Saw, 520.f, 330.f, 0.09f, 0.004f, 0.06f, 0.2f, 0.3f, {0, 0, 5}); // woof
            case Audio::Sfx::Buy: return tone(Wave::Square, 988.f, 988.f, 0.35f, 0.002f, 0.2f, 0.18f, 0.f, {0, 5, 12});  // coins
            case Audio::Sfx::CartStart: return tone(Wave::Noise, 120.f, 380.f, 0.5f, 0.05f, 0.3f, 0.3f);  // off it rolls
            case Audio::Sfx::Crash: return tone(Wave::Noise, 900.f, 80.f, 0.6f, 0.001f, 0.45f, 0.4f);     // into the wall
            case Audio::Sfx::Watch: return tone(Wave::Square, 1760.f, 1760.f, 0.08f, 0.001f, 0.04f, 0.14f); // a beep
            case Audio::Sfx::Record: return tone(Wave::Triangle, 784.f, 784.f, 0.7f, 0.003f, 0.35f, 0.3f, 0.f, {0, 4, 7, 12, 16});
            case Audio::Sfx::Scrape: return tone(Wave::Noise, 220.f, 160.f, 0.22f, 0.04f, 0.12f, 0.14f);   // stone dragged over stone
            case Audio::Sfx::Thud: return tone(Wave::Noise, 140.f, 50.f, 0.3f, 0.001f, 0.25f, 0.4f);      // and dropped
            case Audio::Sfx::PlateClick: return tone(Wave::Square, 220.f, 110.f, 0.12f, 0.001f, 0.08f, 0.2f, 0.f, {0, 7}); // a plate going down
            case Audio::Sfx::Burn: return tone(Wave::Noise, 3000.f, 700.f, 0.4f, 0.005f, 0.3f, 0.22f);     // the rope burning through
            case Audio::Sfx::Count: break;
        }
        return {};
    }

    // A loop that goes on: noise, smoothed until it's the wind, the roar of heat or the rush of
    // falling water, its loudness swelling and falling, so the end runs back into the start
    // without a seam.
    std::vector<sf::Int16> loopSamples(Audio::Loop loop)
    {
        const bool wind = loop == Audio::Loop::Wind, water = loop == Audio::Loop::Water, rumble = loop == Audio::Loop::Rumble;
        const float seconds = wind ? 6.f : rumble ? 2.f : 3.f;
        const size_t count = static_cast<size_t>(seconds * synth::RATE);
        std::vector<sf::Int16> out(count);
        unsigned seed = wind ? 12345u : water ? 4242u : rumble ? 9090u : 777u;
        float low = 0.f, lower = 0.f;
        for (size_t i = 0; i < count; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const float white = static_cast<float>(seed >> 8) / 8388608.f - 1.f;
            const float t = static_cast<float>(i) / count;  // 0 to 1, round the loop
            const float pi2 = 6.2831853f;
            float v;
            if (rumble) {
                // Wheels on rails: a low rattle, clacking over the joins eight times round the loop.
                low += (white - low) * 0.04f;
                lower += (low - lower) * 0.2f;
                const float join = std::fmod(t * 8.f, 1.f);
                v = lower * 2.f + (join < 0.03f ? white * 0.5f * (1.f - join / 0.03f) : 0.f);
            } else if (water) {
                // A steady hiss with a rumble under it.
                low += (white - low) * 0.18f;
                lower += (white - lower) * 0.02f;
                v = (low * 0.55f + lower * 1.6f) * (0.9f + 0.1f * std::sin(pi2 * 3.f * t));
            } else if (wind) {
                // How bright the wind is rises and falls with its gusts.
                const float gust = 0.55f + 0.3f * std::sin(pi2 * t) + 0.15f * std::sin(pi2 * 3.f * t + 1.f);
                low += (white - low) * (0.02f + 0.05f * gust);
                lower += (low - lower) * 0.3f;
                v = lower * 1.1f * gust;
            } else {
                low += (white - low) * 0.012f;
                lower += (low - lower) * 0.05f;
                // A low roar, and now and then a crackle.
                v = lower * 2.2f * (0.8f + 0.2f * std::sin(pi2 * 2.f * t));
                if ((seed & 0xfff) < 3) v += white * 0.25f;
            }
            out[i] = static_cast<sf::Int16>(std::clamp(v, -1.f, 1.f) * 20000.f);
        }
        return out;
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
    for (int s = 0; s < static_cast<int>(Sfx::Count); ++s) effects[static_cast<Sfx>(s)] = buffer(synth::render(effect(static_cast<Sfx>(s))));
    for (size_t l = 0; l < loops.size(); ++l) {
        loops[l].buffer = buffer(loopSamples(static_cast<Loop>(l)));
        loops[l].sound.setBuffer(loops[l].buffer);
        loops[l].sound.setLoop(true);
        loops[l].sound.setVolume(0.f);
    }
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
    for (auto& loop : loops) loop.sound.stop();
}

void Audio::setVolumes(int musicLevel, int soundLevel)
{
    musicVolume = 10.f * static_cast<float>(std::clamp(musicLevel, 0, 10));
    soundVolume = 10.f * static_cast<float>(std::clamp(soundLevel, 0, 10));
}

void Audio::play(Sfx sound, float pitch, float volume)
{
    if (soundVolume <= 0.f || volume <= 0.01f) return;
    sf::Sound& voice = voices[nextVoice];
    nextVoice = (nextVoice + 1) % voices.size();
    voice.setBuffer(effects[sound]);
    voice.setPitch(pitch);
    voice.setVolume(soundVolume * std::min(volume, 1.f));
    voice.play();
}

void Audio::setLoop(Loop loop, float level)
{
    loops[static_cast<size_t>(loop)].wanted = std::clamp(level, 0.f, 1.f);
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
    // The loops ease towards how loud they're wanted, and only play while they can be heard.
    for (Looping& loop : loops) {
        loop.level += (loop.wanted - loop.level) * std::min(1.f, dt * 3.f);
        const float volume = soundVolume * loop.level * 0.6f;
        if (volume > 0.5f && loop.sound.getStatus() != sf::Sound::Playing) loop.sound.play();
        if (volume <= 0.5f && loop.sound.getStatus() == sf::Sound::Playing) loop.sound.pause();
        loop.sound.setVolume(volume);
    }
}
