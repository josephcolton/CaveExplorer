#include <doctest.h>

#include "game/Character.h"
#include "game/SaveGame.h"

using nlohmann::json;

TEST_CASE("explorers roll stats inside the configured ranges") {
    const json config = json::parse(R"({
        "first_names": ["Ada"], "last_names": ["Stone"],
        "stats": {"max_health": [70, 120], "strength": [0.8, 1.2], "speed": [0.9, 1.1], "jump": [0.95, 1.05]},
        "starting_coins": [20, 60]})");
    std::mt19937 rng(1234);
    for (int i = 0; i < 200; ++i) {
        const Character c = Character::roll(config, rng, i + 1);
        CHECK(c.name == "Ada Stone");
        CHECK(c.number == i + 1);
        CHECK(c.maxHealth >= 70);
        CHECK(c.maxHealth <= 120);
        CHECK(c.strength >= doctest::Approx(0.8));
        CHECK(c.strength <= doctest::Approx(1.2));
        CHECK(c.startingCoins >= 20);
        CHECK(c.startingCoins <= 60);
    }
}

TEST_CASE("explorers differ from one another") {
    std::mt19937 rng(99);
    const Character a = Character::roll(json::object(), rng, 1);
    const Character b = Character::roll(json::object(), rng, 2);
    CHECK((a.maxHealth != b.maxHealth || a.strength != b.strength || a.speed != b.speed));
}

TEST_CASE("a save slot round-trips the world, explorer, and position") {
    ItemDb db;
    REQUIRE(db.loadFromJson(R"({"items":[{"id":"apple","stack":5},{"id":"sword","slot":"hand"}]})"));

    SaveGame s;
    s.character.name = "Ada Stone";
    s.character.number = 3;
    s.character.maxHealth = 95;
    s.inventory.coins = 17;
    s.inventory.add(db, "apple", 2);
    s.inventory.equipment.at(Inventory::RightHand) = {"sword", 1};
    s.world.chest.add(db, "apple", 5);
    s.world.explorersLost = 2;
    s.world.takenUniques.insert("cave2_machete");
    CorpsePile pile;
    pile.area = "cave_2";
    pile.x = 400;
    pile.y = 544;
    pile.explorer = "Bram Pike";
    pile.coins = 12;
    pile.items = {{"sword", 1}, {"apple", 3}};
    s.world.corpses.push_back(pile);
    s.takenPickups = {"beach#7", "forest#12"};
    s.health = 40;
    s.area = "cave_1";
    s.areaName = "Upper Caves";
    s.x = 123;
    s.y = 456;
    s.hasPosition = true;
    s.playSeconds = 3725;

    SaveGame t;
    REQUIRE(t.fromJson(json::parse(s.toJson().dump())));
    CHECK(t.character.name == "Ada Stone");
    CHECK(t.character.number == 3);
    CHECK(t.inventory.coins == 17);
    CHECK(t.inventory.backpack.countOf("apple") == 2);
    CHECK(t.inventory.equipment.at(Inventory::RightHand).id == "sword");
    CHECK(t.world.chest.countOf("apple") == 5);
    CHECK(t.world.explorersLost == 2);
    CHECK(t.world.takenUniques.count("cave2_machete") == 1);
    REQUIRE(t.world.corpses.size() == 1);
    CHECK(t.world.corpses[0].explorer == "Bram Pike");
    CHECK(t.world.corpses[0].coins == 12);
    CHECK(t.world.corpses[0].items.size() == 2);
    CHECK(t.takenPickups.count("forest#12") == 1);
    CHECK(t.health == doctest::Approx(40));
    CHECK(t.area == "cave_1");
    CHECK(t.hasPosition);
    CHECK(t.x == doctest::Approx(123));
    CHECK(t.playSeconds == doctest::Approx(3725));
}

TEST_CASE("unknown or future save versions are rejected") {
    SaveGame s;
    CHECK_FALSE(s.fromJson(json::parse(R"({"version": 999})")));
    CHECK_FALSE(s.fromJson(json::parse(R"([1,2,3])")));
}

TEST_CASE("empty corpse piles are dropped on load") {
    WorldSave w;
    w.fromJson(json::parse(R"({"corpses":[{"area":"a","explorer":"X","coins":0,"items":[]},
                                          {"area":"a","explorer":"Y","coins":5,"items":[]}]})"));
    REQUIRE(w.corpses.size() == 1);
    CHECK(w.corpses[0].explorer == "Y");
}
