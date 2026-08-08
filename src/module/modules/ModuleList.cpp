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

    // Right-alignment is why this is a custom painter rather than a
    // TextHudModule: the entries have to line up against the block's right
    // edge, which means every line's x depends on the widest line.
    ui::Rect measureHudBody(float scale) override {
        auto& r = ui::Renderer::get();

        m_names.clear();
        for (const auto& m : ModuleManager::get().modules()) {
            if (!m->enabled() || m.get() == this) continue;
            m_names.push_back(&m->name());
        }

        if (settingBool("sort", true)) {
            // Longest first: the classic stepped-edge look, and it also stops
            // the block's width jittering as modules toggle.
            std::sort(m_names.begin(), m_names.end(),
                      [](const std::string* a, const std::string* b) {
                          if (a->size() != b->size()) return a->size() > b->size();
                          return *a < *b;
                      });
        }

        const float size = bodyFontSize(scale);
        m_widest = 0.0f;
        for (const auto* n : m_names) {
            m_widest = std::max(m_widest, r.measureText(*n, size, bodyWeight()));
        }

        return ui::Rect{ 0, 0, m_widest,
                         lineHeight(scale) * static_cast<float>(m_names.size()) };
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        auto& r = ui::Renderer::get();

        const float size = bodyFontSize(scale);
        const float lineH = lineHeight(scale);
        const bool rightAlign = settingBool("right", true);

        float y = origin.y;
        for (const auto* n : m_names) {
            const float w = r.measureText(*n, size, bodyWeight());
            const float x = rightAlign ? (origin.x + m_widest - w) : origin.x;
            r.drawText(*n, ui::Rect{ x, y, w + 2.0f, lineH },
                       textColor(), size, ui::TextAlign::Left, bodyWeight(), textShadow());
            y += lineH;
        }

        return ui::Rect{ origin.x, origin.y, m_widest, y - origin.y };
    }

private:
    // Built in the measure pass, drawn in the same frame's write pass.
    std::vector<const std::string*> m_names;
    float m_widest = 0.0f;
};

GLACIER_MODULE(ModuleList);

} // namespace glacier
