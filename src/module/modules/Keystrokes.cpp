#include "../HudModule.h"
#include "../ModuleRegistry.h"
#include "../../ui/Input.h"

#include <array>
#include <string>

namespace glacier {

// WASD + sprint/jump + mouse-button display.
//
// Reads key state directly with GetAsyncKeyState rather than tracking WM_KEYDOWN
// in the WndProc hook. That matters: Bedrock consumes raw input, so a key held
// during gameplay does not generate repeated window messages, and a
// message-driven implementation would show a key as released while it is still
// physically down. Polling reflects true physical state every frame.
//
// This module touches no game memory whatsoever.
class Keystrokes final : public HudModule {
public:
    Keystrokes()
        : HudModule("Keystrokes", "Displays movement keys and mouse buttons",
                    Category::Movement, 0,
                    0.02f, 0.55f, 0xFFFFFFFF) {
        addSetting(Setting{ "size", "Key size", 26.0f, 16.0f, 48.0f, 1.0f });
        addSetting(Setting{ "gap", "Spacing", 3.0f, 0.0f, 12.0f, 1.0f });
        addSetting(Setting{ "mouse", "Show mouse buttons", true });
        addSetting(Setting{ "space", "Show spacebar", true });
        addSetting(Setting{ "active", "Pressed color",
                            Setting::ColorTag{}, 0xFF4C9AFF });
        addSetting(Setting{ "idle", "Idle color", Setting::ColorTag{}, 0x66222222 });
    }

    // Exact, and cheap: the layout is a fixed grid, so it follows from the two
    // size settings without measuring anything.
    ui::Rect measureHudBody(float scale) override {
        const float cell = settingFloat("size", 26.0f) * scale;
        const float gap  = settingFloat("gap", 3.0f) * scale;
        const float step = cell + gap;

        float h = step * 2.0f;                                  // W row + ASD row
        if (settingBool("mouse", true)) h += step;
        if (settingBool("space", true)) h += cell * 0.6f + gap;

        return ui::Rect{ 0, 0, 3.0f * cell + 2.0f * gap, h - gap };
    }

    ui::Rect writeHudBody(const ui::Rect& origin, float scale) override {
        const float cell = settingFloat("size", 26.0f) * scale;
        const float gap  = settingFloat("gap", 3.0f) * scale;
        const float step = cell + gap;

        float y = origin.y;

        // Row 1: W centered over the ASD row.
        drawKey(origin.x + step, y, cell, "W", 'W');
        y += step;

        // Row 2: A S D
        drawKey(origin.x,            y, cell, "A", 'A');
        drawKey(origin.x + step,     y, cell, "S", 'S');
        drawKey(origin.x + 2 * step, y, cell, "D", 'D');
        y += step;

        float width = 3 * cell + 2 * gap;

        if (settingBool("mouse", true)) {
            const float half = (width - gap) * 0.5f;
            drawWide(origin.x,               y, half, cell, "LMB", VK_LBUTTON);
            drawWide(origin.x + half + gap,  y, half, cell, "RMB", VK_RBUTTON);
            y += step;
        }

        if (settingBool("space", true)) {
            drawWide(origin.x, y, width, cell * 0.6f, "", VK_SPACE);
            y += cell * 0.6f + gap;
        }

        return ui::Rect{ origin.x, origin.y, width, y - origin.y - gap };
    }

private:
    static bool isDown(int vk) {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    void drawKey(float x, float y, float size, const char* label, int vk) {
        drawWide(x, y, size, size, label, vk);
    }

    void drawWide(float x, float y, float w, float h, const char* label, int vk) {
        auto& r = ui::Renderer::get();
        const bool down = isDown(vk);

        const ui::Rect box{ x, y, w, h };
        // Radius follows the key's own height rather than a flat 3px, so a key
        // keeps its shape when the widget is scaled up — a fixed radius on a
        // scaled box is what makes an overlay look stretched.
        r.fillRoundedRect(box, h * 0.18f,
                          ui::Color::rgba(down ? settingColor("active", 0xFF4C9AFF)
                                               : settingColor("idle", 0x66222222)));

        if (label && *label) {
            // Invert the label against a lit key so it stays readable whatever
            // the user picks for the pressed color.
            const ui::Color fg = down ? ui::Color::rgba(0xFF101418) : textColor();
            r.drawText(label, box, fg, h * 0.42f, ui::TextAlign::Center, ui::FontWeight::SemiBold);
        }
    }
};

GLACIER_MODULE(Keystrokes);

} // namespace glacier
