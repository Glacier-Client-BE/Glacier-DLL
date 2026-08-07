#include "../HudModule.h"
#include "../ModuleManager.h"
#include "../ModuleRegistry.h"

#include <algorithm>
#include <string>
#include <vector>

namespace glacier {

// Lists every currently-enabled module. Needs no game memory at all.
//
// It deliberately skips itself and any module whose name would be redundant
// on screen — a list that includes "Module List" is noise.
class ModuleList final : public HudModule {
public:
    ModuleList()
        : HudModule("Module List", "Lists your enabled modules",
                    Category::Visual, 0,
                    0.86f, 0.02f, 0xFF4C9AFF) {
        addSetting(Setting{ "sort", "Sort by name length", true });
        addSetting(Setting{ "right", "Right-align", true });
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        auto& r = ui::Renderer::get();

        std::vector<const std::string*> names;
        for (const auto& m : ModuleManager::get().modules()) {
            if (!m->enabled() || m.get() == this) continue;
            names.push_back(&m->name());
        }

        if (settingBool("sort", true)) {
            // Longest first: the classic stepped-edge look, and it also stops
            // the block's width jittering as modules toggle.
            std::sort(names.begin(), names.end(),
                      [](const std::string* a, const std::string* b) {
                          if (a->size() != b->size()) return a->size() > b->size();
                          return *a < *b;
                      });
        }

        const float size = 14.0f * scale;
        const float lineH = size * 1.35f;
        const bool rightAlign = settingBool("right", true);

        // Widest entry defines the block, so right-aligned text lines up.
        float widest = 0.0f;
        for (const auto* n : names) {
            widest = std::max(widest, r.measureText(*n, size, true));
        }

        float y = origin.y;
        for (const auto* n : names) {
            const float w = r.measureText(*n, size, true);
            const float x = rightAlign ? (origin.x + widest - w) : origin.x;
            r.drawText(*n, ui::Rect{ x, y, w + 4.0f, lineH },
                       textColor(), size, ui::TextAlign::Left, true);
            y += lineH;
        }

        return ui::Rect{ origin.x, origin.y, widest, y - origin.y };
    }
};

GLACIER_MODULE(ModuleList);

} // namespace glacier
