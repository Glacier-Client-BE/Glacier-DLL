#pragma once

#include "Module.h"
#include "../ui/HudEditor.h"
#include "../ui/Renderer.h"

// Base for modules that draw a persistent on-screen widget.
//
// Position, anchor, scale, colors, and drag-to-move are baked in here rather
// than reimplemented by every widget: a HUD module supplies only its content
// via writeHudBody(), and gets placement, scaling, a background, a text color,
// and repositioning for free. Positions are stored normalized (0..1 of the
// screen) so a widget stays where the user put it across resolution changes
// instead of drifting off-screen.
//
// ── The shared visual language ──
//
// Everything a widget needs to look like the rest of the HUD is defined here,
// because the alternative was tried and it does not work: each widget used to
// pick its own font size (15, 16, 14), its own line height (`size * 1.4f` in
// some, `* 1.35f` in others), and its own weight (all of them semi-bold), while
// the background was nudged around the text by hardcoded offsets that belonged
// to no scale at all. The result read as a pile of unrelated boxes.
//
// So the numbers live in one place. A widget asks for `bodyFontSize(scale)` and
// `lineHeight(scale)` and inherits the rhythm; it does not invent one. If the
// HUD should look different, it should change here, once.
namespace glacier {

class HudModule : public Module {
public:
    // Category is a parameter, not a constant: a HUD widget is not inherently
    // "Visual" — a reach readout is a combat tool and a keystroke display is a
    // movement one. Hardcoding it piles every widget into one menu tab and
    // leaves the rest empty.
    // `defaultBackground` exists because not every widget wants the box.
    // Latite's own HUDModule never draws a background during normal play —
    // renderFrame() (a dark fill + border) is reserved for the editor's
    // "this one is selected" highlight, not gameplay. A plain readout backed
    // by nothing but shadowed text is the reference look, not a fallback; a
    // widget that genuinely benefits from a backing plate (none currently do)
    // can still opt in by passing true.
    HudModule(std::string name, std::string description, Category category,
              int keybind = 0,
              float defaultX = 0.02f, float defaultY = 0.02f,
              std::uint32_t defaultColor = 0xFFFFFFFF,
              bool defaultBackground = false)
        : Module(std::move(name), std::move(description), category, keybind) {
        addSetting(Setting{ "hud.x", "X", defaultX, 0.0f, 1.0f, 0.001f });
        addSetting(Setting{ "hud.y", "Y", defaultY, 0.0f, 1.0f, 0.001f });
        addSetting(Setting{ "hud.scale", "Scale", 1.0f, 0.5f, 3.0f, 0.05f });
        addSetting(Setting{ "hud.color", "Color", Setting::ColorTag{}, defaultColor });
        addSetting(Setting{ "hud.background", "Background", defaultBackground });
        addSetting(Setting{ "hud.bgcolor", "Background color",
                            Setting::ColorTag{}, kDefaultBackground });

        // Padding is a setting rather than a constant because it is the main
        // lever on how dense the HUD feels, and taste on that varies. The
        // defaults are deliberately generous: cramped padding is the single
        // biggest reason an overlay looks unfinished.
        addSetting(Setting{ "hud.padx", "Padding X", 7.0f, 0.0f, 24.0f, 0.5f });
        addSetting(Setting{ "hud.pady", "Padding Y", 4.0f, 0.0f, 24.0f, 0.5f });

        // Proportional, not absolute — this is Latite's scheme. 0 is square and
        // 10 is a full pill, with everything between scaling to the widget's
        // own height. An absolute radius (Glacier used a flat 4px) stays tiny
        // when the widget is scaled up, which is exactly when it looks wrong.
        addSetting(Setting{ "hud.radius", "Corner radius", 4.0f, 0.0f, 10.0f, 0.5f });

        addSetting(Setting{ "hud.shadow", "Text shadow", true });
        addSetting(Setting{ "hud.outline", "Outline", false });
        addSetting(Setting{ "hud.outlinecolor", "Outline color",
                            Setting::ColorTag{}, 0x40FFFFFF });
    }

    // Concrete widgets implement this. `origin` is the top-left of the CONTENT
    // area — padding is already applied, so a widget draws from `origin` and
    // never has to know the background exists. Return the size actually drawn.
    virtual ui::Rect writeHudBody(const ui::Rect& origin, float scale) = 0;

    // Exact content size for this frame, if the widget can say cheaply.
    //
    // The background has to be drawn BEFORE the content (it sits behind it),
    // but its size depends on the content — so something has to know the size
    // in advance. Widgets that can compute it from settings alone (a grid of
    // cells) or that build their text up front should override this.
    //
    // Returning a zero size means "ask me later", and the base falls back to
    // the previous frame's measurement. That fallback is what the whole HUD
    // used to run on, and it visibly lags by a frame whenever content changes
    // width — a counter ticking 99 -> 100 showed a box that was briefly too
    // small. It stays only as a safety net for widgets that genuinely cannot
    // measure ahead.
    virtual ui::Rect measureHudBody(float /*scale*/) { return ui::Rect{}; }

    void onRender() final {
        auto& r = ui::Renderer::get();
        if (!r.ready()) return;

        const float scale = settingFloat("hud.scale", 1.0f);
        const float padX  = settingFloat("hud.padx", 7.0f) * scale;
        const float padY  = settingFloat("hud.pady", 4.0f) * scale;

        // Anchor is the top-left of the FRAME, so a widget does not shift when
        // its padding changes. Previously the stored position was the text
        // origin and the background was drawn at a negative offset from it,
        // which meant increasing the padding moved the widget.
        const float frameX = settingFloat("hud.x", 0.0f) * r.width();
        const float frameY = settingFloat("hud.y", 0.0f) * r.height();

        ui::Rect content = measureHudBody(scale);
        if (content.w <= 0.0f || content.h <= 0.0f) content = m_lastContent;

        const ui::Rect frame{
            frameX, frameY,
            content.w + padX * 2.0f,
            content.h + padY * 2.0f
        };

        if (content.w > 0.0f && content.h > 0.0f) {
            const float radius = cornerRadius(frame);
            if (settingBool("hud.background", false)) {
                r.fillRoundedRect(frame, radius,
                                  ui::Color::rgba(settingColor("hud.bgcolor", kDefaultBackground)));
            }
            if (settingBool("hud.outline", false)) {
                // Inset by half the stroke: Direct2D centres a stroke on the
                // path, so an un-inset outline is half-clipped by the frame.
                r.strokeRoundedRect(frame.inset(0.5f), radius,
                                    ui::Color::rgba(settingColor("hud.outlinecolor", 0x40FFFFFF)),
                                    1.0f);
            }
        }

        m_lastContent = writeHudBody(ui::Rect{ frameX + padX, frameY + padY, 0, 0 }, scale);

        // Drag-to-move, only while the menu is open. Hit-tested against the
        // whole frame rather than the text, so the padding and background are
        // grabbable — a widget you can only catch by its glyphs is fiddly.
        if (ui::HudEditor::get().active()) {
            const ui::Rect dragRect{
                frameX, frameY,
                m_lastContent.w + padX * 2.0f,
                m_lastContent.h + padY * 2.0f
            };
            float nx = settingFloat("hud.x", 0.0f);
            float ny = settingFloat("hud.y", 0.0f);
            if (ui::HudEditor::get().update(name(), dragRect, nx, ny)) {
                if (auto* sx = setting("hud.x")) sx->set(nx);
                if (auto* sy = setting("hud.y")) sy->set(ny);
            }
        }
    }

protected:
    // ── Design tokens ──
    // The one place HUD typography is defined. See the note at the top of the
    // file for why these are not per-widget constants.

    static constexpr float kBaseFontSize = 15.0f;

    // Secondary text — units, labels, the dimmer half of a readout. A single
    // ratio and a single alpha, so "less important" looks the same everywhere.
    static constexpr float kSecondaryScale = 0.85f;
    static constexpr float kSecondaryAlpha = 0.62f;

    static constexpr std::uint32_t kDefaultBackground = 0xA612151C;

    float bodyFontSize(float scale) const { return kBaseFontSize * scale; }

    // Real font metrics, not a guessed multiplier — so two widgets at the same
    // size always produce the same line rhythm.
    float lineHeight(float scale) const {
        return ui::Renderer::get().lineHeight(bodyFontSize(scale), bodyWeight());
    }

    // Normal rather than semi-bold. Every widget used to force bold, which is
    // what made the HUD read as heavy and undifferentiated; keeping the body
    // at Normal leaves SemiBold available to actually mean emphasis.
    static constexpr ui::FontWeight bodyWeight() { return ui::FontWeight::Normal; }

    bool textShadow() const { return settingBool("hud.shadow", true); }

    // Draws one line of body text at `origin` and returns its width. Widgets
    // use this instead of calling the renderer directly so weight, shadow and
    // colour stay consistent without each of them remembering to pass the same
    // three arguments.
    float drawLine(const ui::Rect& origin, std::string_view text, float scale,
                   bool secondary = false) const {
        auto& r = ui::Renderer::get();
        const float size = bodyFontSize(scale) * (secondary ? kSecondaryScale : 1.0f);
        const ui::Color color = secondary ? textColor().withAlpha(kSecondaryAlpha) : textColor();
        const float w = r.measureText(text, size, bodyWeight());
        r.drawText(text, ui::Rect{ origin.x, origin.y, w + 2.0f, lineHeight(scale) },
                   color, size, ui::TextAlign::Left, bodyWeight(), textShadow());
        return w;
    }

    float settingFloat(std::string_view id, float fallback) const {
        const auto* s = setting(id);
        return s ? s->asFloat() : fallback;
    }
    bool settingBool(std::string_view id, bool fallback) const {
        const auto* s = setting(id);
        return s ? s->asBool() : fallback;
    }
    std::uint32_t settingColor(std::string_view id, std::uint32_t fallback) const {
        const auto* s = setting(id);
        return s ? s->asColor() : fallback;
    }

    // The widget's own text color, resolved from the shared "hud.color" setting.
    ui::Color textColor() const {
        return ui::Color::rgba(settingColor("hud.color", 0xFFFFFFFF));
    }

private:
    // 0..10 mapped onto "square" .. "pill", against the frame's own height.
    float cornerRadius(const ui::Rect& frame) const {
        const float t = settingFloat("hud.radius", 4.0f) / 10.0f;
        return t * (frame.h * 0.5f);
    }

    ui::Rect m_lastContent{};
};

} // namespace glacier
