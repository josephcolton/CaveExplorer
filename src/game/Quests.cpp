#include "game/Quests.h"

#include "game/Inventory.h"

#include <algorithm>

using nlohmann::json;

QuestDef QuestDef::fromJson(const std::string& id, const json& j) {
    QuestDef q;
    q.id = id;
    q.title = j.value("title", id);
    q.intro = j.value("intro", "");
    q.accepted = j.value("accepted", "Thank you.");
    q.progress = j.value("progress", "{n}/{count}");
    q.complete = j.value("complete", "Well done.");
    const json obj = j.value("objective", json::object());
    q.type = obj.value("type", "kill") == "fetch" ? Type::Fetch : Type::Kill;
    q.target = obj.value("target", "");
    q.count = std::max(1, obj.value("count", 1));
    const json reward = j.value("reward", json::object());
    q.rewardItem = reward.value("item", "");
    q.rewardItemCount = std::max(1, reward.value("count", 1));
    q.rewardCoins = reward.value("coins", 0);
    q.rewardAbility = reward.value("ability", "");
    return q;
}

QuestState::Status QuestLog::status(const std::string& id) const {
    auto it = states_.find(id);
    return it == states_.end() ? QuestState::Status::NotStarted : it->second.status;
}

int QuestLog::progress(const QuestDef& q, const Inventory& inv) const {
    if (q.type == QuestDef::Type::Fetch) return std::min(q.count, inv.backpack.countOf(q.target));
    auto it = states_.find(q.id);
    return it == states_.end() ? 0 : std::min(q.count, it->second.kills);
}

std::vector<std::string> QuestLog::onKill(const std::string& enemyId, const std::map<std::string, QuestDef>& quests) {
    std::vector<std::string> advanced;
    for (auto& [id, st] : states_) {
        if (st.status != QuestState::Status::Active) continue;
        auto q = quests.find(id);
        if (q == quests.end() || q->second.type != QuestDef::Type::Kill || q->second.target != enemyId) continue;
        if (st.kills < q->second.count) {
            ++st.kills;
            advanced.push_back(id);
        }
    }
    return advanced;
}

bool QuestLog::turnIn(const QuestDef& q, Inventory& inv, const ItemDb& db, Reward& out) {
    if (status(q.id) != QuestState::Status::Active || !ready(q, inv)) return false;
    if (q.type == QuestDef::Type::Fetch) {
        int need = q.count;
        for (std::size_t i = 0; i < inv.backpack.size() && need > 0; ++i) {
            if (inv.backpack.at(i).id == q.target) need -= inv.backpack.takeFrom(i, need);
        }
    }
    out = {};
    if (!q.rewardItem.empty()) {
        out.item = q.rewardItem;
        out.itemCount = q.rewardItemCount;
        out.itemsThatDidNotFit = inv.add(db, q.rewardItem, q.rewardItemCount);
    }
    out.coins = q.rewardCoins;
    inv.coins += q.rewardCoins;
    out.ability = q.rewardAbility;
    states_[q.id].status = QuestState::Status::Done;
    return true;
}

std::vector<std::string> QuestLog::active() const {
    std::vector<std::string> ids;
    for (const auto& [id, st] : states_) {
        if (st.status == QuestState::Status::Active) ids.push_back(id);
    }
    return ids;
}

json QuestLog::toJson() const {
    json j = json::object();
    for (const auto& [id, st] : states_) {
        j[id] = {{"status", static_cast<int>(st.status)}, {"kills", st.kills}};
    }
    return j;
}

void QuestLog::fromJson(const json& j) {
    states_.clear();
    if (!j.is_object()) return;
    for (auto& [id, v] : j.items()) {
        QuestState st;
        st.status = static_cast<QuestState::Status>(std::clamp(v.value("status", 0), 0, 2));
        st.kills = v.value("kills", 0);
        states_[id] = st;
    }
}

std::string fillQuestText(std::string text, int n, int count) {
    auto replace = [&](const std::string& key, const std::string& value) {
        for (std::size_t p = text.find(key); p != std::string::npos; p = text.find(key, p + value.size())) {
            text.replace(p, key.size(), value);
        }
    };
    replace("{n}", std::to_string(n));
    replace("{count}", std::to_string(count));
    return text;
}
