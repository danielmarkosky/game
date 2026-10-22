#pragma once

#include <string>
#include <vector>

// What the world holds besides its rooms: things to find for people, quests, races and outfits.
// Read from assets/quests.txt and assets/outfits.txt (see the notes at the top of each).
namespace catalog
{
    struct Item
    {
        std::string id;     // what conversations and the Item entity call it: "moss_page"
        std::string sprite; // its picture in props.png
        std::string name;   // "A TORN PAGE"
        std::string text;   // shown when it's found
    };
    struct Quest
    {
        std::string id;
        std::string title;
        std::string text;  // what to do, in the quest list
        std::string start; // the story flag that gives it
        std::string done;  // and the one that ends it
    };
    struct Race
    {
        std::string id;    // a conversation's "race <id>", and the Finish entity's `race`
        float seconds;     // to get to the finish in
    };
    struct Outfit
    {
        std::string look;  // one of Player::SKINS
        int price;         // in gems at the shop; 0: not for sale, only won
        std::string name;
    };

    const std::vector<Item>& items();
    const std::vector<Quest>& quests();
    const std::vector<Race>& races();
    const std::vector<Outfit>& outfits();
    const Item* item(const std::string& id);
    const Race* race(const std::string& id);
    const Outfit* outfit(const std::string& look);
}
