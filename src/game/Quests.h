#pragma once

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

class ItemDb;
struct Inventory;

// A simple task given by an NPC (data/quests.json).
struct QuestDef {
    enum class Type { Kill, Fetch };

    std::string id;
    std::string title;
    std::string intro;     // what the giver says when offering it
    std::string accepted;  // reply when you accept
    std::string progress;  // reminder; "{n}" and "{count}" are replaced
    std::string complete;  // reply when you turn it in
    Type type = Type::Kill;
    std::string target;    // enemy id (kill) or item id (fetch)
    int count = 1;
    // Rewards
    std::string rewardItem;
    int rewardItemCount = 1;
    int rewardCoins = 0;
    std::string rewardAbility;

    static QuestDef fromJson(const std::string& id, const nlohmann::json& j);
};

struct QuestState {
    enum class Status { NotStarted, Active, Done };
    Status status = Status::NotStarted;
    int kills = 0;  // kill quests count as they happen; fetch quests check the backpack
};

// The current explorer's quests. Cleared when the explorer dies.
class QuestLog {
public:
    QuestState::Status status(const std::string& id) const;
    void start(const std::string& id) { states_[id].status = QuestState::Status::Active; }
    // Progress toward the objective (kills so far, or matching items carried).
    int progress(const QuestDef& q, const Inventory& inv) const;
    bool ready(const QuestDef& q, const Inventory& inv) const { return progress(q, inv) >= q.count; }

    // Records a kill. Returns the ids of active kill quests that advanced.
    std::vector<std::string> onKill(const std::string& enemyId, const std::map<std::string, QuestDef>& quests);

    struct Reward {
        std::string item;
        int itemCount = 0;
        int itemsThatDidNotFit = 0;
        int coins = 0;
        std::string ability;
    };
    // Completes a ready quest: takes fetched items, adds reward items/coins to the inventory.
    // Returns false if the quest isn't active and ready.
    bool turnIn(const QuestDef& q, Inventory& inv, const ItemDb& db, Reward& out);

    std::vector<std::string> active() const;
    void clear() { states_.clear(); }

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::map<std::string, QuestState> states_;
};

// Pretty "{n}/{count}" substitution for quest text.
std::string fillQuestText(std::string text, int n, int count);
