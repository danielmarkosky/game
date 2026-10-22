#include <gtest/gtest.h>
#include "src/Catalog.h"
#include "src/Player.h"
#include "src/Props.h"
#include <algorithm>

// assets/quests.txt and assets/outfits.txt: everything they name has to exist.
TEST(Catalog, EveryOutfitIsALookAndEveryLookCanBeWorn) {
    ASSERT_FALSE(catalog::outfits().empty());
    for (const catalog::Outfit& o : catalog::outfits()) {
        EXPECT_NE(std::find(Player::SKINS.begin(), Player::SKINS.end(), o.look), Player::SKINS.end()) << o.look;
    }
    for (const char* skin : Player::SKINS) EXPECT_TRUE(catalog::outfit(skin)) << skin << " isn't in outfits.txt";
    EXPECT_EQ(catalog::outfit("classic")->price, 0);
}

TEST(Catalog, ItemsHavePicturesAndQuestsAndRacesAreThere) {
    ASSERT_FALSE(catalog::items().empty());
    for (const catalog::Item& item : catalog::items()) EXPECT_TRUE(props::find(item.sprite)) << item.id << ": " << item.sprite;
    ASSERT_FALSE(catalog::quests().empty());
    for (const catalog::Quest& q : catalog::quests()) {
        EXPECT_FALSE(q.start.empty()) << q.id;
        EXPECT_FALSE(q.done.empty()) << q.id;
    }
    ASSERT_TRUE(catalog::race("garden"));
    EXPECT_GT(catalog::race("garden")->seconds, 0.f);
}
