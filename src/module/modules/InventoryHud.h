#pragma once

#include <vector>

#include "../HudModule.h"
#include "../../sdk/GameSDK.h"

namespace glacier {

// Renders the player's main inventory (27 slots above the hotbar) as a 9x3 grid
// so it's readable without opening the inventory. Same snapshot pattern as
// ArmorHud.
class InventoryHud final : public HudModule {
public:
    InventoryHud()
        : HudModule("Inventory HUD", "Shows your inventory as an on-screen grid",
                    Category::Visual, /*positioned*/ true, "", /*defX*/ 8, /*defY*/ 220) {
        addSetting(Setting{ "hotbar", "Include hotbar", false });
        addSetting(Setting{ "counts", "Show stack counts", true });
        addSetting(Setting{ "bg",     "Background opacity", 0.5f, 0.0f, 1.0f, 0.05f });
    }

    void onRender() override {
        auto all = sdk::GameSDK::get().inventory();   // 0-8 hotbar, 9-35 main

        m_grid.clear();
        const int start = hotbar() ? 0 : 9;
        m_grid.reserve(all.size());
        for (int i = start; i < static_cast<int>(all.size()); ++i) {
            m_grid.push_back(std::move(all[i]));
        }
    }

    static constexpr int columns() { return 9; }

protected:
    void writeHudBody(json::Object& o) const override {
        const auto* counts = setting("counts");
        const auto* bg = setting("bg");

        json::Array slots;
        for (const auto& it : m_grid) {
            json::Object s;
            s.set("valid", it.valid);
            if (it.valid) s.set("name", it.name).set("count", it.count);
            slots.push(s);
        }

        o.set("type", "inventory")
         .set("columns", columns())
         .set("counts", counts && counts->asBool())
         .set("bg", bg ? bg->asFloat() : 0.5f)
         .raw("slots", slots.str());
    }

private:
    bool hotbar() const { const auto* s = setting("hotbar"); return s && s->asBool(); }

    std::vector<sdk::ItemStack> m_grid;
};

} // namespace glacier
