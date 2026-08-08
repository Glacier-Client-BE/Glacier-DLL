#pragma once

#include "HudModule.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// Base for HUD widgets whose content is just lines of text — which is most of
// them: coordinates, clock, FPS, ping, reach, CPS, day, watermark.
//
// This exists for the same reason Latite splits TextModule out of HUDModule.
// Left to themselves, ten text widgets produce ten slightly different answers
// to the same questions — what size, what weight, how far apart the lines sit,
// how a secondary line is dimmed — and the HUD stops looking like one product.
// Here a widget answers only "what does it say", and layout is not its problem.
//
// It also fixes the background sizing honestly. buildLines() runs during the
// measure pass and the result is kept, so the frame is sized from THIS frame's
// text and drawn once — no second layout pass, and none of the one-frame lag
// the shared last-bounds fallback has.
namespace glacier {

class TextHudModule : public HudModule {
public:
    using HudModule::HudModule;

    struct Line {
        std::string text;
        // Secondary lines are smaller and dimmer — units, qualifiers, the
        // second half of a readout. One flag rather than per-widget alpha and
        // size arithmetic, so "less important" reads the same everywhere.
        bool secondary = false;
        // Overrides the widget's colour for this line only. Used where the
        // colour carries meaning (a module list tinting by category, a ping
        // readout going red), not for decoration.
        std::optional<ui::Color> color;
    };

    // Widgets implement this. Append nothing to draw nothing — a widget with no
    // lines is skipped entirely rather than leaving an empty box on screen.
    virtual void buildLines(std::vector<Line>& out) = 0;

    ui::Rect measureHudBody(float scale) final {
        auto& r = ui::Renderer::get();

        m_lines.clear();
        buildLines(m_lines);

        const float lh = lineHeight(scale);
        float width = 0.0f;
        for (const Line& line : m_lines) {
            const float size = bodyFontSize(scale) * (line.secondary ? kSecondaryScale : 1.0f);
            const float w = r.measureText(line.text, size, bodyWeight());
            if (w > width) width = w;
        }
        return ui::Rect{ 0, 0, width, lh * static_cast<float>(m_lines.size()) };
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) final {
        auto& r = ui::Renderer::get();

        const float lh = lineHeight(scale);
        float width = 0.0f;
        float y = origin.y;

        for (const Line& line : m_lines) {
            const float size = bodyFontSize(scale) * (line.secondary ? kSecondaryScale : 1.0f);
            const ui::Color color =
                line.color ? *line.color
                           : (line.secondary ? textColor().withAlpha(kSecondaryAlpha) : textColor());

            const float w = r.measureText(line.text, size, bodyWeight());
            r.drawText(line.text, ui::Rect{ origin.x, y, w + 2.0f, lh },
                       color, size, ui::TextAlign::Left, bodyWeight(), textShadow());

            if (w > width) width = w;
            y += lh;
        }

        return ui::Rect{ origin.x, origin.y, width, lh * static_cast<float>(m_lines.size()) };
    }

protected:
    // Convenience for the common one-and-two-line cases, so widgets read as a
    // list of strings rather than a list of struct initialisations.
    static void push(std::vector<Line>& out, std::string text) {
        out.push_back(Line{ std::move(text), false, std::nullopt });
    }
    static void pushSecondary(std::vector<Line>& out, std::string text) {
        out.push_back(Line{ std::move(text), true, std::nullopt });
    }

private:
    // Built in measureHudBody, consumed by writeHudBody in the same frame.
    std::vector<Line> m_lines;
};

} // namespace glacier
