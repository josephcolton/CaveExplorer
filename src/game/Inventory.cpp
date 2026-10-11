#include "game/Inventory.h"

#include <algorithm>

using nlohmann::json;

Container::Container(std::size_t size, EquipSlot kind) : slots_(size), kinds_(size, kind) {}

Container::Container(std::vector<EquipSlot> kinds) : slots_(kinds.size()), kinds_(std::move(kinds)) {}

bool Container::accepts(std::size_t i, const ItemDef& def) const {
    return kinds_[i] == EquipSlot::None || kinds_[i] == def.slot;
}

int Container::add(const ItemDb& db, const std::string& id, int count) {
    const ItemDef& def = db.get(id);
    // Top up existing stacks first, then fill empty slots.
    for (std::size_t i = 0; i < slots_.size() && count > 0; ++i) {
        ItemStack& s = slots_[i];
        if (!s.empty() && s.id == id && s.count < def.maxStack) {
            const int n = std::min(count, def.maxStack - s.count);
            s.count += n;
            count -= n;
        }
    }
    for (std::size_t i = 0; i < slots_.size() && count > 0; ++i) {
        ItemStack& s = slots_[i];
        if (s.empty() && accepts(i, def)) {
            const int n = std::min(count, def.maxStack);
            s.id = id;
            s.count = n;
            count -= n;
        }
    }
    return count;
}

int Container::roomFor(const ItemDb& db, const std::string& id) const {
    const ItemDef& def = db.get(id);
    int room = 0;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        const ItemStack& s = slots_[i];
        if (s.empty() && accepts(i, def)) room += def.maxStack;
        else if (s.id == id) room += std::max(0, def.maxStack - s.count);
    }
    return room;
}

int Container::countOf(const std::string& id) const {
    int n = 0;
    for (const ItemStack& s : slots_) {
        if (!s.empty() && s.id == id) n += s.count;
    }
    return n;
}

int Container::takeFrom(std::size_t i, int count) {
    ItemStack& s = slots_[i];
    const int n = std::min(count, s.count);
    s.count -= n;
    if (s.count <= 0) s.clear();
    return n;
}

bool Container::isEmpty() const {
    return std::all_of(slots_.begin(), slots_.end(), [](const ItemStack& s) { return s.empty(); });
}

json Container::toJson() const {
    json arr = json::array();
    for (const ItemStack& s : slots_) {
        arr.push_back(s.empty() ? json(nullptr) : json{{"id", s.id}, {"count", s.count}});
    }
    return arr;
}

void Container::fromJson(const json& j) {
    for (ItemStack& s : slots_) s.clear();
    if (!j.is_array()) return;
    for (std::size_t i = 0; i < j.size() && i < slots_.size(); ++i) {
        if (j[i].is_object()) {
            slots_[i].id = j[i].value("id", "");
            slots_[i].count = j[i].value("count", 0);
        }
    }
}

bool transferStack(const ItemDb& db, Container& from, std::size_t i, Container& to, std::size_t j) {
    if (&from == &to && i == j) return false;
    ItemStack& src = from.at(i);
    ItemStack& dst = to.at(j);
    if (src.empty()) return false;
    const ItemDef& srcDef = db.get(src.id);
    if (!to.accepts(j, srcDef)) return false;

    if (dst.empty()) {
        dst = src;
        src.clear();
        return true;
    }
    if (dst.id == src.id) {
        const int n = std::min(src.count, srcDef.maxStack - dst.count);
        if (n <= 0) return false;
        dst.count += n;
        from.takeFrom(i, n);
        return true;
    }
    if (!from.accepts(i, db.get(dst.id))) return false;
    std::swap(src, dst);
    return true;
}

Inventory::Inventory()
    : backpack(kBackpackSlots),
      equipment(std::vector<EquipSlot>{EquipSlot::Hand, EquipSlot::Hand, EquipSlot::Feet, EquipSlot::Body,
                                       EquipSlot::Head}) {}

json Inventory::toJson() const {
    return {{"backpack", backpack.toJson()}, {"equipment", equipment.toJson()}, {"coins", coins}};
}

void Inventory::fromJson(const json& j) {
    backpack.fromJson(j.value("backpack", json::array()));
    equipment.fromJson(j.value("equipment", json::array()));
    coins = j.value("coins", 0);
}
