#pragma once

#include <array>

#include "../HudModule.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Shows the player's four equipped armor pieces with count and durability.
// onRender snapshots the SDK on the render thread (pointers live that frame);
// writeHudBody serializes the snapshot for the web HUD.
class ArmorHud final : public HudModule {
public:
    ArmorHud()
        : HudModule("Armor HUD", "Displays your equipped armor and durability",
                    Category::Visual, /*positioned*/ true, "", /*defX*/ 8, /*defY*/ 8) {
        addSetting(Setting{ "vertical",   "Vertical layout", true });
        addSetting(Setting{ "durability", "Show durability", true });
    }

    void onRender() override {
        m_slots = sdk::GameSDK::get().armor();
    }

protected:
    void writeHudBody(json::Object& o) const override {
        const auto* vert = setting("vertical");
        const auto* dura = setting("durability");

        json::Array slots;
        for (const auto& it : m_slots) slots.push(serializeSlot(it));

        o.set("type", "armor")
         .set("vertical", vert && vert->asBool())
         .set("durability", dura && dura->asBool())
         .raw("slots", slots.str());
    }

private:
    static json::Object serializeSlot(const sdk::ItemStack& it) {
        json::Object o;
        o.set("valid", it.valid);
        if (it.valid) {
            o.set("name", it.name)
             .set("count", it.count)
             .set("dura", it.durability)
             .set("max", it.maxDurability)
             .set("ench", it.enchanted);
        }
        return o;
    }

    std::array<sdk::ItemStack, 4> m_slots{};
};

} // namespace glacier
