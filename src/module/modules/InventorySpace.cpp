#include "../TextHudModule.h"
#include "../ModuleRegistry.h"
#include "../../sdk/GameSDK.h"

#include <string>

namespace glacier {

// How full the inventory is, in slots.
//
// Flarial's ItemCounter counts a *specific* item, which needs item identity —
// `ItemStack::getItem` and a name lookup, neither of which has a signature
// here. Counting occupied slots needs nothing beyond `GameSDK::inventory()`,
// which Inventory HUD already relies on, and answers the question people
// actually check mid-run: "do I need to go home yet".
//
// Deliberately counts SLOTS, not items. A stack of 64 and a stack of 1 take
// the same room, so "1287 items" tells you nothing about whether the next
// pickup fits.
class InventorySpace final : public TextHudModule {
public:
    InventorySpace()
        : TextHudModule("Inventory Space", "Shows how many inventory slots are used",
                    Category::Player, 0,
                    0.01f, 0.53f, 0xFFFFFFFF) {
        setIcon(0xF009);   // fa-table-cells-large
        addSetting(Setting{ "free", "Count free instead of used", false });
        addSetting(Setting{ "hotbar", "Include the hotbar", true });
        addSetting(Setting{ "warn", "Turn red when nearly full", true });
        addSetting(Setting{ "threshold", "Warn with N slots left", 3.0f, 0.0f, 18.0f, 1.0f });
    }

    void buildLines(std::vector<Line>& out) override {
        const auto slots = sdk::GameSDK::get().inventory();

        // Slots 0..8 are the hotbar, 9..35 the main grid — see the comment on
        // GameSDK::inventory(). Skipping the hotbar is the more useful default
        // for some players and not for others, hence the setting rather than a
        // decision made here.
        const std::size_t first = settingBool("hotbar", true) ? 0u : 9u;
        const std::size_t total = slots.size() - first;

        std::size_t used = 0;
        for (std::size_t i = first; i < slots.size(); ++i) {
            if (slots[i].valid) ++used;
        }
        const std::size_t free = total - used;

        const bool showFree = settingBool("free", false);
        const std::string body = showFree
            ? std::to_string(free) + " / " + std::to_string(total) + " free"
            : std::to_string(used) + " / " + std::to_string(total) + " slots";

        std::optional<ui::Color> color;
        if (settingBool("warn", true)) {
            const auto limit = static_cast<std::size_t>(settingFloat("threshold", 3.0f));
            if (free <= limit) color = ui::Color::rgba(0xFFF87171);
        }

        out.push_back(Line{ body, false, color });
    }
};

GLACIER_MODULE(InventorySpace);

} // namespace glacier
