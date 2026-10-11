#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "game/Inventory.h"

namespace {
ItemDb makeDb() {
    ItemDb db;
    const bool ok = db.loadFromJson(R"({"items":[
        {"id":"apple","stack":5,"value":3},
        {"id":"dagger","slot":"hand","value":15},
        {"id":"boots","slot":"feet","value":10},
        {"id":"cap","slot":"head"}
    ]})");
    REQUIRE(ok);
    return db;
}
}  // namespace

TEST_CASE("items load from JSON") {
    ItemDb db = makeDb();
    CHECK(db.size() == 4);
    CHECK(db.get("apple").maxStack == 5);
    CHECK(db.get("dagger").slot == EquipSlot::Hand);
    CHECK(db.get("dagger").sellPrice() == 7);
    CHECK(db.find("nope") == nullptr);
    CHECK_FALSE(ItemDb().loadFromJson("{not json"));
}

TEST_CASE("adding stacks fills existing stacks first, then empty slots") {
    ItemDb db = makeDb();
    Inventory inv;
    CHECK(inv.add(db, "apple", 3) == 0);
    CHECK(inv.add(db, "apple", 4) == 0);
    CHECK(inv.backpack.at(0).count == 5);
    CHECK(inv.backpack.at(1).count == 2);
    CHECK(inv.backpack.countOf("apple") == 7);
}

TEST_CASE("a full backpack reports leftovers") {
    ItemDb db = makeDb();
    Inventory inv;
    for (int i = 0; i < 9; ++i) CHECK(inv.add(db, "dagger", 1) == 0);
    CHECK(inv.add(db, "dagger", 1) == 1);
    CHECK(inv.backpack.roomFor(db, "apple") == 0);
}

TEST_CASE("equipment slots only accept matching items") {
    ItemDb db = makeDb();
    Inventory inv;
    inv.add(db, "boots", 1);
    inv.add(db, "dagger", 1);
    // boots -> right hand: rejected
    CHECK_FALSE(transferStack(db, inv.backpack, 0, inv.equipment, Inventory::RightHand));
    // boots -> feet: ok
    CHECK(transferStack(db, inv.backpack, 0, inv.equipment, Inventory::Feet));
    CHECK(inv.equipment.at(Inventory::Feet).id == "boots");
    CHECK(inv.backpack.at(0).empty());
    CHECK(inv.carries("boots"));
}

TEST_CASE("swapping is refused when the other item cannot go back") {
    ItemDb db = makeDb();
    Inventory inv;
    inv.add(db, "dagger", 1);  // slot 0
    inv.add(db, "apple", 1);   // slot 1
    CHECK(transferStack(db, inv.backpack, 0, inv.equipment, Inventory::RightHand));
    // apple onto the hand holding the dagger: apple can't be held -> refused
    CHECK_FALSE(transferStack(db, inv.backpack, 1, inv.equipment, Inventory::RightHand));
    // dagger onto the apple's backpack slot would put the apple in the hand -> also refused
    CHECK_FALSE(transferStack(db, inv.equipment, Inventory::RightHand, inv.backpack, 1));
    CHECK(inv.equipment.at(Inventory::RightHand).id == "dagger");
    CHECK(inv.backpack.at(1).id == "apple");
    // dagger into an empty backpack slot is fine
    CHECK(transferStack(db, inv.equipment, Inventory::RightHand, inv.backpack, 4));
    CHECK(inv.equipment.at(Inventory::RightHand).empty());
}

TEST_CASE("merging same items respects the stack limit") {
    ItemDb db = makeDb();
    Container a(2), b(2);
    a.add(db, "apple", 4);
    b.add(db, "apple", 3);
    CHECK(transferStack(db, a, 0, b, 0));
    CHECK(b.at(0).count == 5);
    CHECK(a.at(0).count == 2);
}

TEST_CASE("containers round-trip through JSON") {
    ItemDb db = makeDb();
    Inventory inv;
    inv.coins = 42;
    inv.add(db, "apple", 3);
    inv.equipment.add(db, "cap", 1);
    Inventory copy;
    copy.fromJson(inv.toJson());
    CHECK(copy.coins == 42);
    CHECK(copy.backpack.countOf("apple") == 3);
    CHECK(copy.equipment.at(Inventory::Head).id == "cap");
}

#include "game/WorldSave.h"

TEST_CASE("world save keeps the chest and taken uniques") {
    ItemDb db = makeDb();
    WorldSave w;
    w.chest.add(db, "dagger", 1);
    w.chest.add(db, "apple", 4);
    w.takenUniques.insert("cave2_machete");
    WorldSave copy;
    copy.fromJson(nlohmann::json::parse(w.toJson().dump()));
    CHECK(copy.chest.size() == WorldSave::kChestSlots);
    CHECK(copy.chest.countOf("apple") == 4);
    CHECK(copy.chest.countOf("dagger") == 1);
    CHECK(copy.takenUniques.count("cave2_machete") == 1);
}
