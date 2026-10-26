#pragma once

#include <string>
#include <vector>

// What you can wear: read from assets/outfits.txt (see the note at its top). Mags sells them at
// her stall by the campfire, for gems. Each is drawn as one of Player::LOOKS, by its id.
namespace outfits
{
    struct Outfit
    {
        std::string id;   // its look: "diver"
        int price = 0;    // in gems; 0: yours from the start
        std::string name; // "DIVING SUIT"
        std::string text; // what it's like, at the stall
    };

    // The one you woke up in.
    inline const std::string FIRST = "heroine";
    // The one that lets you hold your breath longer (Physics::divingAir), and the one with a lamp.
    inline const std::string DIVER = "diver";
    inline const std::string MINER = "miner";

    const std::vector<Outfit>& all();
    const Outfit* find(const std::string& id);
}
