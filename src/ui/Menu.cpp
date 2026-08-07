#include "Menu.h"

#include "HudEditor.h"
#include "KeyNames.h"
#include "Input.h"
#include "../core/Config.h"
#include "../module/ModuleManager.h"
#include "../util/Logger.h"

#include <algorithm>
#include <array>

namespace glacier::ui {

namespace {

// Phase 2 theme: flat, legible, no gradients or animation yet (Phase 4).
constexpr auto kBackdrop   = Color::rgba(0x99000000);
constexpr auto kPanel      = Color::rgba(0xF01A1D23);
constexpr auto kSidebar    = Color::rgba(0xF0141619);
constexpr auto kCard       = Color::rgba(0xFF23272F);
constexpr auto kCardHover  = Color::rgba(0xFF2B3038);
constexpr auto kAccent     = Color::rgba(0xFF4C9AFF);
constexpr auto kText       = Color::rgba(0xFFE6E9EF);
constexpr auto kTextDim    = Color::rgba(0xFF8A93A3);
constexpr auto kTrack      = Color::rgba(0xFF3A404A);

constexpr float kPanelW   = 620.0f;
constexpr float kPanelH   = 440.0f;
constexpr float kSidebarW = 150.0f;
constexpr float kRowH     = 40.0f;
constexpr float kPad      = 12.0f;

constexpr std::array kCategories{
    Category::Combat, Category::Movement, Category::Visual,
    Category::Player, Category::World,    Category::Misc,
};

} // namespace

void Menu::setOpen(bool open) {
    m_open = open;
    // HUD widgets are only draggable while the menu is up — otherwise a click
    // during normal play would move them.
    HudEditor::get().setActive(open);
    if (!open) {
        // Drop any in-flight interaction, or it resumes when the menu reopens.
        m_capturingModule = nullptr;
        m_draggingSetting = nullptr;
        m_draggingChannel = -1;

        // Closing the menu is the natural commit point: everything the user
        // just changed gets persisted, without writing a file on every slider
        // pixel. The write itself happens on the logic thread.
        Config::get().markDirty();
    }
    Input::get().reset();
}

bool Menu::hovered(const Rect& r) const {
    auto& in = Input::get();
    return r.contains(in.mouseX(), in.mouseY());
}

bool Menu::clicked(const Rect& r) const {
    return Input::get().leftPressed() && hovered(r);
}

void Menu::render() {
    if (!m_open) return;

    auto& r = Renderer::get();
    auto& in = Input::get();

    // A drag that ended anywhere (including outside the menu) must release.
    if (!in.leftDown()) m_draggingSetting = nullptr;

    const Rect screen{ 0, 0, r.width(), r.height() };
    r.fillRect(screen, kBackdrop);

    const Rect panel{
        (r.width() - kPanelW) * 0.5f,
        (r.height() - kPanelH) * 0.5f,
        kPanelW, kPanelH
    };
    r.fillRoundedRect(panel, 10.0f, kPanel);

    const Rect sidebar{ panel.x, panel.y, kSidebarW, panel.h };
    drawSidebar(sidebar);

    const Rect list{
        panel.x + kSidebarW, panel.y + kPad,
        panel.w - kSidebarW - kPad, panel.h - 2 * kPad
    };
    drawModuleList(list);

    in.newFrame();
}

void Menu::drawSidebar(const Rect& area) {
    auto& r = Renderer::get();

    r.fillRoundedRect(area, 10.0f, kSidebar);
    // Square off the inner edge so the sidebar meets the panel flush.
    r.fillRect(Rect{ area.right() - 10.0f, area.y, 10.0f, area.h }, kSidebar);

    r.drawText("GLACIER", Rect{ area.x + kPad, area.y + kPad, area.w - 2 * kPad, 28.0f },
               kAccent, 17.0f, TextAlign::Left, true);

    float y = area.y + 52.0f;
    for (const auto cat : kCategories) {
        const Rect row{ area.x + 8.0f, y, area.w - 16.0f, 32.0f };
        const bool selected = (cat == m_category);

        if (selected) {
            r.fillRoundedRect(row, 6.0f, kAccent.withAlpha(0.16f));
            r.fillRoundedRect(Rect{ row.x, row.y + 6.0f, 3.0f, row.h - 12.0f }, 1.5f, kAccent);
        } else if (hovered(row)) {
            r.fillRoundedRect(row, 6.0f, kCardHover.withAlpha(0.5f));
        }

        if (clicked(row)) {
            m_category = cat;
            m_scroll = 0.0f;
        }

        r.drawText(categoryName(cat), Rect{ row.x + 14.0f, row.y, row.w - 14.0f, row.h },
                   selected ? kText : kTextDim, 14.0f);
        y += 36.0f;
    }

    // Footer: settings save automatically when the menu closes, which is worth
    // stating — an autosave the user can't see is indistinguishable from a
    // client that forgets everything.
    r.drawText("Saves on close", Rect{ area.x + kPad, area.bottom() - 46.0f, area.w, 18.0f },
               kTextDim.withAlpha(0.55f), 10.5f);
    r.drawText("G / M toggles  ·  END unloads",
               Rect{ area.x + kPad, area.bottom() - 30.0f, area.w, 18.0f },
               kTextDim.withAlpha(0.55f), 10.5f);
}

void Menu::drawModuleList(const Rect& area) {
    auto& r = Renderer::get();
    auto& in = Input::get();

    if (hovered(area)) {
        m_scroll -= in.scroll() * 40.0f;
    }

    r.pushClip(area);

    Rect cursor{ area.x, area.y - m_scroll, area.w, 0 };
    int shown = 0;

    for (const auto& module : ModuleManager::get().modules()) {
        if (module->category() != m_category) continue;
        drawModuleCard(*module, cursor, area.w);
        ++shown;
    }

    const float contentH = (cursor.y + m_scroll) - area.y;

    r.popClip();

    if (shown == 0) {
        r.drawText("No modules in this category yet",
                   Rect{ area.x, area.y + 20.0f, area.w, 24.0f },
                   kTextDim, 13.0f, TextAlign::Center);
        m_scroll = 0.0f;
        return;
    }

    // Clamp after drawing: content height isn't known until the walk is done,
    // and clamping to a stale height causes a visible jump on the next frame.
    m_scroll = std::clamp(m_scroll, 0.0f, std::max(0.0f, contentH - area.h));
}

void Menu::drawModuleCard(Module& module, Rect& cursor, float width) {
    auto& r = Renderer::get();

    const Rect card{ cursor.x, cursor.y, width, kRowH };
    const bool isHovered = hovered(card);
    r.fillRoundedRect(card, 6.0f, isHovered ? kCardHover : kCard);

    // Left region expands/collapses settings; the toggle on the right is a
    // separate hit target so opening settings never flips the module.
    const Rect toggleRect{ card.right() - 52.0f, card.y + 10.0f, 40.0f, 20.0f };
    const Rect labelRect{ card.x + 12.0f, card.y, card.w - 76.0f, card.h };

    if (Input::get().leftPressed() && hovered(labelRect)) {
        m_expanded = (m_expanded == module.name()) ? std::string{} : module.name();
    }

    r.drawText(module.name(), Rect{ labelRect.x, card.y + 4.0f, labelRect.w, 20.0f },
               module.enabled() ? kText : kTextDim, 14.0f, TextAlign::Left, true);
    r.drawText(module.description(),
               Rect{ labelRect.x, card.y + 20.0f, labelRect.w, 16.0f },
               kTextDim.withAlpha(0.7f), 11.0f);

    if (widgetToggle(toggleRect, module.enabled())) {
        module.toggle();
        LOG_INFO("{} {}", module.name(), module.enabled() ? "enabled" : "disabled");
    }

    cursor.y += kRowH + 4.0f;

    if (m_expanded == module.name()) {
        drawSettings(module, cursor, width);
    }
}

void Menu::drawSettings(Module& module, Rect& cursor, float width) {
    auto& r = Renderer::get();

    const Rect keyRow{ cursor.x + 16.0f, cursor.y, width - 16.0f, 28.0f };
    r.drawText("Keybind", Rect{ keyRow.x, keyRow.y, 140.0f, keyRow.h }, kTextDim, 12.0f);
    widgetKeybind(Rect{ keyRow.right() - 110.0f, keyRow.y + 3.0f, 100.0f, 22.0f }, module);
    cursor.y += 30.0f;

    for (auto& setting : module.settings()) {
        const Rect row{ cursor.x + 16.0f, cursor.y, width - 16.0f, 28.0f };
        r.drawText(setting.label(), Rect{ row.x, row.y, 140.0f, row.h }, kTextDim, 12.0f);

        switch (setting.type()) {
            case SettingType::Bool: {
                const Rect box{ row.right() - 50.0f, row.y + 4.0f, 40.0f, 20.0f };
                if (widgetToggle(box, setting.asBool())) {
                    setting.set(!setting.asBool());
                }
                break;
            }
            case SettingType::Float:
            case SettingType::Int: {
                const Rect track{ row.right() - 210.0f, row.y + 11.0f, 160.0f, 6.0f };
                widgetSlider(track, setting);

                const bool isInt = setting.type() == SettingType::Int;
                const std::string value = isInt
                    ? std::to_string(setting.asInt())
                    : std::to_string(setting.asFloat()).substr(0, 4);
                r.drawText(value, Rect{ row.right() - 44.0f, row.y, 40.0f, row.h },
                           kText, 12.0f, TextAlign::Right);
                break;
            }
            case SettingType::Color: {
                const Rect swatch{ row.right() - 54.0f, row.y + 5.0f, 44.0f, 18.0f };
                cursor.y += widgetColor(swatch, setting, width);
                break;
            }
            case SettingType::Key:
                // Per-setting keybinds aren't used yet; the module-level bind
                // above covers this. Drawn as a plain value so a module that
                // declares one isn't silently ignored.
                r.drawText(keyDisplayName(setting.asInt()),
                           Rect{ row.right() - 110.0f, row.y, 100.0f, row.h },
                           kText, 12.0f, TextAlign::Right);
                break;
        }
        cursor.y += 30.0f;
    }

    cursor.y += 6.0f;
}

bool Menu::widgetToggle(const Rect& r, bool value) {
    auto& gfx = Renderer::get();

    const Color track = value ? kAccent : kTrack;
    gfx.fillRoundedRect(r, r.h * 0.5f, track);

    const float knobR = r.h - 6.0f;
    const float knobX = value ? (r.right() - knobR - 3.0f) : (r.x + 3.0f);
    gfx.fillRoundedRect(Rect{ knobX, r.y + 3.0f, knobR, knobR }, knobR * 0.5f,
                        Color::rgba(0xFFFFFFFF));

    if (hovered(r)) {
        gfx.strokeRoundedRect(r, r.h * 0.5f, kAccent.withAlpha(0.5f), 1.5f);
    }
    return clicked(r);
}

bool Menu::widgetSlider(const Rect& r, Setting& setting) {
    auto& gfx = Renderer::get();
    auto& in  = Input::get();

    // Grabbing anywhere on a padded band around the track starts a drag — a
    // 6px-tall target is unusable otherwise.
    const Rect grab = Rect{ r.x, r.y - 8.0f, r.w, r.h + 16.0f };
    if (in.leftPressed() && hovered(grab)) {
        m_draggingSetting = &setting;
    }

    const bool active = (m_draggingSetting == &setting);
    if (active && r.w > 0.0f) {
        setting.setNormalized((in.mouseX() - r.x) / r.w);
    }

    const float t = setting.normalized();
    gfx.fillRoundedRect(r, r.h * 0.5f, kTrack);
    gfx.fillRoundedRect(Rect{ r.x, r.y, r.w * t, r.h }, r.h * 0.5f, kAccent);

    const float knob = 14.0f;
    gfx.fillRoundedRect(
        Rect{ r.x + r.w * t - knob * 0.5f, r.y + r.h * 0.5f - knob * 0.5f, knob, knob },
        knob * 0.5f,
        (active || hovered(grab)) ? Color::rgba(0xFFFFFFFF) : Color::rgba(0xFFD8DCE4));

    return active;
}

float Menu::widgetColor(const Rect& swatch, Setting& setting, float rowWidth) {
    auto& gfx = Renderer::get();
    auto& in  = Input::get();

    // Checkerboard behind the swatch so a low-alpha color reads as translucent
    // rather than as a dark color.
    constexpr float kCheck = 4.5f;
    for (int i = 0; i * kCheck < swatch.w; ++i) {
        for (int j = 0; j * kCheck < swatch.h; ++j) {
            if (((i + j) & 1) == 0) continue;
            const float cw = std::min(kCheck, swatch.w - i * kCheck);
            const float ch = std::min(kCheck, swatch.h - j * kCheck);
            gfx.fillRect(Rect{ swatch.x + i * kCheck, swatch.y + j * kCheck, cw, ch },
                         Color::rgba(0xFF4A4F58));
        }
    }
    gfx.fillRoundedRect(swatch, 3.0f, Color::rgba(setting.asColor()));
    gfx.strokeRoundedRect(swatch, 3.0f,
                          hovered(swatch) ? kAccent : Color::rgba(0xFF555C68), 1.0f);

    if (clicked(swatch)) {
        m_expandedColor = (m_expandedColor == &setting) ? nullptr : &setting;
    }
    if (m_expandedColor != &setting) return 0.0f;

    // Expanded: one slider per channel. Sliders are drawn below the swatch row,
    // so the caller must be told how much vertical space they consumed.
    static constexpr const char* kNames[4] = { "A", "R", "G", "B" };
    float consumed = 0.0f;

    for (int ch = 0; ch < 4; ++ch) {
        const float y = swatch.bottom() + 8.0f + static_cast<float>(ch) * 20.0f;
        const Rect track{ swatch.x - 150.0f, y, 160.0f, 5.0f };

        gfx.drawText(kNames[ch], Rect{ track.x - 16.0f, y - 8.0f, 14.0f, 20.0f },
                     kTextDim, 11.0f);

        const Rect grab{ track.x, track.y - 8.0f, track.w, track.h + 16.0f };
        if (in.leftPressed() && hovered(grab) && m_draggingChannel < 0) {
            m_draggingChannel = ch;
            m_expandedColor = &setting;
        }
        if (!in.leftDown() && m_draggingChannel == ch) {
            m_draggingChannel = -1;
        }
        if (m_draggingChannel == ch && track.w > 0.0f) {
            const float t = std::clamp((in.mouseX() - track.x) / track.w, 0.0f, 1.0f);
            setting.setChannel(ch, static_cast<int>(t * 255.0f + 0.5f));
        }

        const float t = static_cast<float>(setting.channel(ch)) / 255.0f;
        gfx.fillRoundedRect(track, track.h * 0.5f, kTrack);
        gfx.fillRoundedRect(Rect{ track.x, track.y, track.w * t, track.h },
                            track.h * 0.5f, kAccent);
        gfx.fillRoundedRect(
            Rect{ track.x + track.w * t - 5.0f, track.y - 3.5f, 10.0f, 12.0f },
            5.0f, Color::rgba(0xFFFFFFFF));

        gfx.drawText(std::to_string(setting.channel(ch)),
                     Rect{ track.right() + 8.0f, y - 8.0f, 34.0f, 20.0f },
                     kText, 11.0f, TextAlign::Right);

        consumed += 20.0f;
    }

    (void)rowWidth;
    return consumed + 12.0f;
}

bool Menu::widgetKeybind(const Rect& r, Module& module) {
    auto& gfx = Renderer::get();
    auto& in  = Input::get();

    const bool capturing = (m_capturingModule == &module);

    gfx.fillRoundedRect(r, 4.0f, capturing ? kAccent.withAlpha(0.25f) : kTrack);
    if (hovered(r) || capturing) {
        gfx.strokeRoundedRect(r, 4.0f, kAccent, 1.0f);
    }

    gfx.drawText(capturing ? "Press a key..." : keyDisplayName(module.keybind()),
                 r, capturing ? kAccent : kText, 11.5f, TextAlign::Center);

    if (clicked(r)) {
        m_capturingModule = capturing ? nullptr : &module;
        in.takeKey();   // discard the click-frame key so it can't bind instantly
        return false;
    }

    if (capturing) {
        if (const int vk = in.takeKey()) {
            // Escape cancels rather than binding — otherwise there is no way to
            // back out of a capture without binding something.
            if (vk != VK_ESCAPE) {
                module.setKeybind(vk);
                LOG_INFO("{} bound to {}", module.name(), keyDisplayName(vk));
            }
            m_capturingModule = nullptr;
            return vk != VK_ESCAPE;
        }
    }
    return false;
}

} // namespace glacier::ui
