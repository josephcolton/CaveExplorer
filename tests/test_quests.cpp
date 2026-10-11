#include <doctest.h>

#include "game/Inventory.h"
#include "game/Modifiers.h"
#include "game/Quests.h"

#include <algorithm>

using nlohmann::json;

namespace {
ItemDb makeDb() {
    ItemDb db;
    const bool ok = db.loadFromJson(R"({"items":[
        {"id":"charm", "carried": {"jump": 1.2}},
        {"id":"boots", "slot":"feet", "equipped": {"speed": 1.1, "flags": ["nonslip"]}},
        {"id":"helmet", "slot":"head", "equipped": {"defense": 0.8, "max_health": 10}},
        {"id":"bone", "stack": 10},
        {"id":"prize", "slot":"feet"}
    ]})");
    REQUIRE(ok);
    return db;
}
}  // namespace

TEST_CASE("carried effects work from the backpack; worn effects only when equipped") {
    ItemDb db = makeDb();
    Inventory inv;
    inv.add(db, "charm", 1);
    inv.add(db, "boots", 1);
    Modifiers m = gearModifiers(inv, db);
    CHECK(m.jump == doctest::Approx(1.2f));
    CHECK(m.runSpeed == doctest::Approx(1.0f));  // boots only in the pack
    CHECK_FALSE(m.has("nonslip"));

    REQUIRE(transferStack(db, inv.backpack, 1, inv.equipment, Inventory::Feet));
    m = gearModifiers(inv, db);
    CHECK(m.runSpeed == doctest::Approx(1.1f));
    CHECK(m.has("nonslip"));
    CHECK(m.jump == doctest::Approx(1.2f));  // the charm still counts

    // Drop the charm: its effect goes away.
    inv.backpack.takeFrom(0, 1);
    CHECK(gearModifiers(inv, db).jump == doctest::Approx(1.0f));
}

TEST_CASE("modifiers stack and describe themselves") {
    Modifiers a = Modifiers::fromJson(json::parse(R"({"defense": 0.8, "max_health": 10})"));
    Modifiers b = Modifiers::fromJson(json::parse(R"({"defense": 0.5, "max_health": 5, "flags": ["warm"]})"));
    a.combine(b);
    CHECK(a.defense == doctest::Approx(0.4f));
    CHECK(a.maxHealth == doctest::Approx(15.0f));
    const auto text = a.describe();
    CHECK(std::find(text.begin(), text.end(), "Max health +15") != text.end());
    CHECK(std::find(text.begin(), text.end(), "Damage taken -60%") != text.end());
    CHECK(std::find(text.begin(), text.end(), "Keeps out the cold") != text.end());
    CHECK(Modifiers{}.empty());
}

TEST_CASE("kill quests count matching kills only while active") {
    ItemDb db = makeDb();
    std::map<std::string, QuestDef> quests;
    quests["rats"] = QuestDef::fromJson("rats", json::parse(R"({
        "objective": {"type": "kill", "target": "rat", "count": 2},
        "reward": {"item": "prize", "coins": 10}})"));
    QuestLog log;
    Inventory inv;

    CHECK(log.onKill("rat", quests).empty());  // not started yet
    log.start("rats");
    CHECK(log.onKill("bat", quests).empty());
    CHECK(log.onKill("rat", quests).size() == 1);
    CHECK_FALSE(log.ready(quests["rats"], inv));
    log.onKill("rat", quests);
    log.onKill("rat", quests);  // extra kills don't overflow
    CHECK(log.progress(quests["rats"], inv) == 2);

    QuestLog::Reward reward;
    REQUIRE(log.turnIn(quests["rats"], inv, db, reward));
    CHECK(reward.coins == 10);
    CHECK(inv.coins == 10);
    CHECK(inv.backpack.countOf("prize") == 1);
    CHECK(log.status("rats") == QuestState::Status::Done);
    CHECK_FALSE(log.turnIn(quests["rats"], inv, db, reward));  // only once
}

TEST_CASE("fetch quests take the items on turn-in and can grant an ability") {
    ItemDb db = makeDb();
    const QuestDef q = QuestDef::fromJson("bones", json::parse(R"({
        "objective": {"type": "fetch", "target": "bone", "count": 3},
        "reward": {"ability": "grit"}})"));
    QuestLog log;
    Inventory inv;
    log.start("bones");
    inv.add(db, "bone", 2);
    CHECK_FALSE(log.ready(q, inv));
    inv.add(db, "bone", 2);
    CHECK(log.ready(q, inv));
    QuestLog::Reward reward;
    REQUIRE(log.turnIn(q, inv, db, reward));
    CHECK(inv.backpack.countOf("bone") == 1);
    CHECK(reward.ability == "grit");
}

TEST_CASE("quest logs round-trip and text is filled in") {
    QuestLog log;
    log.start("a");
    QuestLog copy;
    copy.fromJson(json::parse(log.toJson().dump()));
    CHECK(copy.status("a") == QuestState::Status::Active);
    CHECK(copy.status("b") == QuestState::Status::NotStarted);
    CHECK(fillQuestText("Rats: {n} of {count}.", 1, 3) == "Rats: 1 of 3.");
}
