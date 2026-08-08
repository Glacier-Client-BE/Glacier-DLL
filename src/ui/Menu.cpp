#include "Menu.h"

#include "HudEditor.h"
#include "KeyNames.h"
#include "Input.h"
#include "../Glacier.h"
#include "../core/Config.h"
#include "../module/ModuleManager.h"
#include "../util/CrashHandler.h"
#include "../util/FrameStats.h"
#include "../util/Logger.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <utility>

namespace glacier::ui {

namespace {

// ── Palette ──
//
// One accent, one positive, three text weights, and everything else expressed
// as a white or accent overlay at some alpha. Overlays rather than opaque
// greys because the panel is translucent over a blurred game frame: a fixed
// dark grey would read as a different colour depending on what was behind it,
// which is exactly what made the old flat panel look pasted on.
constexpr auto kScrim      = Color::rgba(0xA6070A12);
constexpr auto kPanel      = Color::rgba(0xF20E1220);
constexpr auto kPanelEdge  = Color::rgba(0x24FFFFFF);
constexpr auto kHair       = Color::rgba(0x14FFFFFF);
constexpr auto kAccent     = Color::rgba(0xFF5B8CFF);
constexpr auto kOn         = Color::rgba(0xFF3FD08A);
constexpr auto kText       = Color::rgba(0xFFEAEEF7);
constexpr auto kTextDim    = Color::rgba(0xFF8892A6);
constexpr auto kTextFaint  = Color::rgba(0xFF5B6478);
constexpr auto kField      = Color::rgba(0x12FFFFFF);
constexpr auto kCard       = Color::rgba(0x0AFFFFFF);
constexpr auto kCardHover  = Color::rgba(0x1AFFFFFF);
constexpr auto kTrack      = Color::rgba(0x1FFFFFFF);

// ── Metrics ──
constexpr float kPanelW    = 980.0f;
constexpr float kPanelH    = 640.0f;
constexpr float kRadius    = 18.0f;
constexpr float kHeaderH   = 62.0f;
constexpr float kToolbarH  = 58.0f;
constexpr float kPad       = 22.0f;
constexpr float kCardH     = 122.0f;
constexpr float kCardGap   = 14.0f;
constexpr int   kColumns   = 3;
constexpr float kRowH      = 34.0f;   // one settings row

// ── Font Awesome codepoints (all verified present in the embedded font) ──
constexpr wchar_t kIconLogo    = 0xF2DC;   // fa-snowflake
constexpr wchar_t kIconSearch  = 0xF002;   // fa-magnifying-glass
constexpr wchar_t kIconGear    = 0xF013;   // fa-gear
constexpr wchar_t kIconClose   = 0xF00D;   // fa-xmark
constexpr wchar_t kIconBack    = 0xF053;   // fa-chevron-left

constexpr std::array kCategories{
    Category::Combat, Category::Movement, Category::Visual,
    Category::Player, Category::World,    Category::Misc,
};

// Fallback mark for a module that never called setIcon(). Modules are not
// required to have an opinion about their own icon, and a category is always
// a truthful answer where a guess from the name would not be.
wchar_t categoryIcon(Category c) {
    switch (c) {
        case Category::Combat:   return 0xF6DE;   // fa-hand-fist
        case Category::Movement: return 0xF70C;   // fa-person-running
        case Category::Visual:   return 0xF06E;   // fa-eye
        case Category::Player:   return 0xF007;   // fa-user
        case Category::World:    return 0xF57D;   // fa-globe
        case Category::Misc:     return 0xF1DE;   // fa-sliders
    }
    return 0xF1DE;
}

// "Frame Time" -> "FT", "Fullbright" -> "FU". Only used when the icon font
// could not be loaded at all — see Renderer::hasGlyph.
std::string initials(const std::string& name) {
    std::string out;
    bool boundary = true;
    for (const char c : name) {
        if (c == ' ') { boundary = true; continue; }
        if (boundary && out.size() < 2) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            boundary = false;
        }
    }
    if (out.size() < 2 && name.size() > 1) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(name[1]))));
    }
    return out;
}

bool containsCaseInsensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;
    const auto equalFold = [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a))
             == std::tolower(static_cast<unsigned char>(b));
    };
    return std::search(haystack.begin(), haystack.end(),
                       needle.begin(), needle.end(), equalFold) != haystack.end();
}

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
        m_searchFocused = false;

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

bool Menu::matchesFilter(const Module& module) const {
    if (m_categoryFilter >= 0 && static_cast<int>(module.category()) != m_categoryFilter) {
        return false;
    }
    if (m_search.empty()) return true;
    return containsCaseInsensitive(module.name(), m_search)
        || containsCaseInsensitive(module.description(), m_search);
}

void Menu::render() {
    if (!m_open) return;

    // So a fault in here is reported as the menu rather than as "(idle)" —
    // this runs on the game's render thread, where nothing else would name it.
    GLACIER_ACTIVITY("drawing and hit-testing the menu");

    auto& r  = Renderer::get();
    auto& in = Input::get();

    // A drag that ended anywhere (including outside the menu) must release.
    if (!in.leftDown()) m_draggingSetting = nullptr;

    // Drained every frame, not just when focused — otherwise characters typed
    // while unfocused sit in the buffer and all appear at once the moment the
    // search box is clicked.
    const std::wstring typed = in.takeChars();
    if (m_searchFocused) {
        for (const wchar_t c : typed) {
            if (c == L'\b') {
                if (!m_search.empty()) m_search.pop_back();
            } else if (c >= L' ' && c < 0x7F && m_search.size() < 40) {
                m_search.push_back(static_cast<char>(c));
            }
        }
    }

    const Rect screen{ 0, 0, r.width(), r.height() };

    // Panel size adapts down on small or scaled displays rather than running
    // off the edge. Nothing inside it is positioned absolutely, so it just
    // gets tighter.
    const float panelW = std::min(kPanelW, std::max(520.0f, screen.w - 80.0f));
    const float panelH = std::min(kPanelH, std::max(380.0f, screen.h - 80.0f));
    const Rect panel{
        std::floor((screen.w - panelW) * 0.5f),
        std::floor((screen.h - panelH) * 0.5f),
        panelW, panelH
    };
    m_panelRect = panel;

    // Blurs the game frame behind the panel before dimming it. Must run first,
    // and must be the panel's own rect rather than the whole screen: blurring
    // everything costs a full-screen Gaussian per frame to produce something
    // the scrim then covers anyway.
    r.blurBackdrop(screen, 18.0f);
    r.fillRect(screen, kScrim);

    r.fillRoundedRect(panel, kRadius, kPanel);
    r.strokeRoundedRect(panel, kRadius, kPanelEdge, 1.0f);

    const Rect header{ panel.x, panel.y, panel.w, kHeaderH };
    drawHeader(header);
    r.drawLine(panel.x + 1.0f, header.bottom(), panel.right() - 1.0f, header.bottom(), kHair, 1.0f);

    if (m_tab == Tab::Modules) {
        const Rect toolbar{ panel.x, header.bottom(), panel.w, kToolbarH };
        drawToolbar(toolbar);

        const Rect content{
            panel.x + kPad, toolbar.bottom(),
            panel.w - 2 * kPad, panel.bottom() - toolbar.bottom() - kPad
        };
        drawModulesTab(content);
    } else {
        const Rect content{
            panel.x + kPad, header.bottom() + kPad,
            panel.w - 2 * kPad, panel.bottom() - header.bottom() - 2 * kPad
        };
        drawPerformanceTab(content);
    }

    // Clicking anywhere that isn't the search field drops its focus, which is
    // the only way to get G/M back as menu keys without a dedicated control.
    if (in.leftPressed() && m_searchFocused && !hovered(m_searchRect)) {
        m_searchFocused = false;
    }

    in.newFrame();
}

void Menu::drawHeader(const Rect& area) {
    auto& r = Renderer::get();

    // ── Brand ──
    const Rect logo{ area.x + kPad, area.y + (area.h - 32.0f) * 0.5f, 32.0f, 32.0f };
    r.fillRoundedRect(logo, 9.0f, kAccent.withAlpha(0.20f));
    r.strokeRoundedRect(logo, 9.0f, kAccent.withAlpha(0.45f), 1.0f);
    if (!r.drawGlyph(kIconLogo, logo, kAccent, 15.0f)) {
        r.drawText("G", logo, kAccent, 16.0f, TextAlign::Center, FontWeight::SemiBold);
    }

    const float nameW = r.measureText("Glacier", 19.0f, FontWeight::SemiBold);
    const Rect name{ logo.right() + 12.0f, area.y, nameW + 4.0f, area.h };
    r.drawText("Glacier", name, kText, 19.0f, TextAlign::Left, FontWeight::SemiBold);

    const std::string version = std::string("v") + Glacier::kVersion;
    const Rect pill{ name.right() + 8.0f, area.y + (area.h - 20.0f) * 0.5f,
                     r.measureText(version, 11.0f, FontWeight::Medium) + 16.0f, 20.0f };
    r.fillRoundedRect(pill, 10.0f, kAccent.withAlpha(0.18f));
    r.drawText(version, pill, kAccent, 11.0f, TextAlign::Center, FontWeight::Medium);

    // ── Close ──
    const Rect close{ area.right() - kPad - 32.0f, area.y + (area.h - 32.0f) * 0.5f, 32.0f, 32.0f };
    if (widgetIconButton(close, kIconClose)) {
        // Routed through the same path the G key uses so the cursor handoff
        // and the config write happen exactly once, in one place.
        Glacier::get().closeMenu();
    }

    // ── Tabs ──
    //
    // Centred on the panel, not on the space between the brand and the close
    // button: the brand's width depends on the version string, and a tab strip
    // that shifts when the version number gets longer is the kind of detail
    // that reads as unfinished.
    static constexpr std::array<std::pair<Tab, const char*>, 2> kTabs{ {
        { Tab::Modules,     "Modules" },
        { Tab::Performance, "Performance" },
    } };

    float stripW = 8.0f;
    std::array<float, kTabs.size()> widths{};
    for (std::size_t i = 0; i < kTabs.size(); ++i) {
        widths[i] = r.measureText(kTabs[i].second, 12.5f, FontWeight::Medium) + 34.0f;
        stripW += widths[i];
    }

    const Rect strip{ area.x + (area.w - stripW) * 0.5f, area.y + (area.h - 34.0f) * 0.5f,
                      stripW, 34.0f };
    r.fillRoundedRect(strip, 17.0f, kField);

    float x = strip.x + 4.0f;
    for (std::size_t i = 0; i < kTabs.size(); ++i) {
        const Rect tab{ x, strip.y + 4.0f, widths[i], strip.h - 8.0f };
        if (widgetChip(tab, kTabs[i].second, m_tab == kTabs[i].first)) {
            m_tab = kTabs[i].first;
            m_scroll = 0.0f;
            // The search field only exists on the Modules tab; leaving it
            // focused would silently keep swallowing G and M elsewhere.
            m_searchFocused = false;
        }
        x += widths[i];
    }
}

void Menu::drawToolbar(const Rect& area) {
    auto& r = Renderer::get();

    const float mid = area.y + area.h * 0.5f;

    // ── Search ──
    const Rect search{ area.x + kPad, mid - 17.0f, 236.0f, 34.0f };
    m_searchRect = search;

    r.fillRoundedRect(search, 10.0f, kField);
    if (m_searchFocused) {
        r.strokeRoundedRect(search, 10.0f, kAccent.withAlpha(0.7f), 1.0f);
    } else if (hovered(search)) {
        r.strokeRoundedRect(search, 10.0f, kPanelEdge, 1.0f);
    }
    if (clicked(search)) m_searchFocused = true;

    const Rect icon{ search.x + 8.0f, search.y, 22.0f, search.h };
    if (!r.drawGlyph(kIconSearch, icon, kTextFaint, 11.0f)) {
        // A ring and a tail, drawn rather than dropped: an empty box where an
        // icon should be looks broken, an absent one looks unfinished.
        r.strokeRoundedRect(Rect{ icon.x + 5.0f, icon.y + 11.0f, 9.0f, 9.0f }, 4.5f, kTextFaint, 1.3f);
        r.drawLine(icon.x + 13.0f, icon.y + 20.0f, icon.x + 17.0f, icon.y + 24.0f, kTextFaint, 1.3f);
    }

    const Rect searchText{ icon.right() + 4.0f, search.y, search.w - 44.0f, search.h };
    if (m_search.empty()) {
        r.drawText("Search modules", searchText, kTextFaint, 12.5f);
    } else {
        r.drawText(m_search, searchText, kText, 12.5f);
    }

    // Caret. Blinks off the wall clock rather than a frame counter so its
    // rhythm doesn't depend on the frame rate.
    if (m_searchFocused && (GetTickCount64() / 500) % 2 == 0) {
        const float caretX = searchText.x + std::min(r.measureText(m_search, 12.5f),
                                                     searchText.w - 6.0f) + 1.5f;
        r.fillRect(Rect{ caretX, mid - 8.0f, 1.5f, 16.0f }, kText.withAlpha(0.8f));
    }

    // ── Enabled count, right-aligned ──
    int enabled = 0;
    for (const auto& module : ModuleManager::get().modules()) {
        if (module->enabled()) ++enabled;
    }
    const std::string count = std::to_string(enabled) + " enabled";
    const float countW = r.measureText(count, 12.0f, FontWeight::Medium) + 4.0f;
    const Rect countRect{ area.right() - kPad - countW, area.y, countW, area.h };
    r.drawText(count, countRect, enabled > 0 ? kText : kTextDim, 12.0f,
               TextAlign::Right, FontWeight::Medium);

    // ── Category chips ──
    //
    // Laid out between the search field and the count, and simply clipped if
    // the panel is too narrow for all seven — better a chip cut off at a hard
    // edge than a row that overlaps the count text.
    const Rect chipArea{ search.right() + 14.0f, area.y,
                         std::max(0.0f, countRect.x - 14.0f - (search.right() + 14.0f)), area.h };
    r.pushClip(chipArea);

    float x = chipArea.x;
    const auto chip = [&](const char* label, int filter) {
        const float w = r.measureText(label, 11.5f, FontWeight::Medium) + 26.0f;
        const Rect box{ x, mid - 14.0f, w, 28.0f };
        if (widgetChip(box, label, m_categoryFilter == filter)) {
            m_categoryFilter = filter;
            m_scroll = 0.0f;
            m_detail.clear();
        }
        x += w + 6.0f;
    };

    chip("All", -1);
    for (const auto category : kCategories) {
        chip(categoryName(category), static_cast<int>(category));
    }

    r.popClip();
}

void Menu::drawModulesTab(const Rect& area) {
    if (!m_detail.empty()) {
        for (const auto& module : ModuleManager::get().modules()) {
            if (module->name() == m_detail) {
                drawDetail(area, *module);
                return;
            }
        }
        // The named module no longer exists (only possible if the list were
        // ever rebuilt). Fall back to the grid rather than showing nothing.
        m_detail.clear();
    }
    drawGrid(area);
}

void Menu::drawGrid(const Rect& area) {
    auto& r  = Renderer::get();
    auto& in = Input::get();

    if (hovered(area)) m_scroll -= in.scroll() * 60.0f;

    // Collected first so the empty case can be answered before any clipping or
    // scroll maths, and so the column width is known independently of it.
    std::vector<Module*> shown;
    for (const auto& module : ModuleManager::get().modules()) {
        if (matchesFilter(*module)) shown.push_back(module.get());
    }

    if (shown.empty()) {
        m_scroll = 0.0f;
        const std::string empty = m_search.empty()
            ? std::string("No modules in this category")
            : "Nothing matches \"" + m_search + "\"";
        r.drawText(empty, Rect{ area.x, area.y + area.h * 0.42f, area.w, 24.0f },
                   kTextDim, 13.0f, TextAlign::Center);
        return;
    }

    const int rows = (static_cast<int>(shown.size()) + kColumns - 1) / kColumns;
    const float contentH = rows * (kCardH + kCardGap) - kCardGap;
    const float maxScroll = std::max(0.0f, contentH - area.h);
    m_scroll = std::clamp(m_scroll, 0.0f, maxScroll);

    // The scrollbar lives outside the clip so it isn't scrolled with the
    // content; the grid gets the remaining width.
    const bool scrollable = maxScroll > 0.0f;
    const float gutter = scrollable ? 12.0f : 0.0f;
    const float gridW = area.w - gutter;
    const float cardW = (gridW - (kColumns - 1) * kCardGap) / kColumns;

    r.pushClip(Rect{ area.x, area.y, gridW, area.h });
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const int column = static_cast<int>(i) % kColumns;
        const int row    = static_cast<int>(i) / kColumns;
        const Rect card{
            area.x + column * (cardW + kCardGap),
            area.y + row * (kCardH + kCardGap) - m_scroll,
            cardW, kCardH
        };
        // Cheap reject: a card scrolled fully out of view still hit-tests, and
        // a click landing on one the user cannot see would be a real bug.
        if (card.bottom() < area.y || card.y > area.bottom()) continue;
        drawCard(*shown[i], card);
    }
    r.popClip();

    if (scrollable) {
        drawScrollbar(Rect{ area.right() - 5.0f, area.y, 4.0f, area.h },
                      m_scroll, contentH, area.h);
    }
}

void Menu::drawCard(Module& module, const Rect& card) {
    auto& r = Renderer::get();

    const bool on = module.enabled();
    const Rect gear{ card.right() - 32.0f, card.y + 8.0f, 24.0f, 24.0f };
    const bool overGear = hovered(gear);
    const bool overCard = hovered(card) && !overGear;

    r.fillRoundedRect(card, 12.0f, on ? kAccent.withAlpha(overCard ? 0.20f : 0.13f)
                                      : (overCard ? kCardHover : kCard));
    r.strokeRoundedRect(card, 12.0f,
                        on ? kAccent.withAlpha(0.55f) : kPanelEdge.withAlpha(0.55f), 1.0f);

    // Status dot, top-left. The one piece of state readable at a glance from
    // across the grid, which is why it is a colour and not more text.
    r.fillEllipse(card.x + 16.0f, card.y + 18.0f, 3.5f, 3.5f, on ? kOn : kTextFaint.withAlpha(0.5f));

    // Gear opens the detail page. A separate hit target from the card so
    // configuring a module never also toggles it.
    if (!r.drawGlyph(kIconGear, gear, overGear ? kText : kTextFaint, 12.0f)) {
        r.strokeRoundedRect(gear.inset(6.0f), 6.0f, overGear ? kText : kTextFaint, 1.3f);
    }
    if (clicked(gear)) {
        m_detail = module.name();
        m_scroll = 0.0f;
        return;
    }

    drawModuleMark(module, Rect{ card.x, card.y + 30.0f, card.w, 34.0f },
                   on ? kAccent : kTextDim, 24.0f);

    r.drawText(module.name(), Rect{ card.x + 8.0f, card.y + 66.0f, card.w - 16.0f, 20.0f },
               on ? kText : kTextDim, 13.5f, TextAlign::Center, FontWeight::Medium);

    r.drawText(on ? "ENABLED" : "DISABLED",
               Rect{ card.x + 8.0f, card.y + 88.0f, card.w - 16.0f, 16.0f },
               on ? kAccent : kTextFaint, 9.5f, TextAlign::Center, FontWeight::SemiBold);

    if (overCard && Input::get().leftPressed()) {
        // Queued, not applied here — this runs on the game's render thread and
        // onEnable/onDisable are not safe there. See
        // ModuleManager::requestToggle.
        ModuleManager::get().requestToggle(&module);
    }
}

void Menu::drawDetail(const Rect& area, Module& module) {
    auto& r  = Renderer::get();
    auto& in = Input::get();

    // ── Back bar ──
    const Rect back{ area.x, area.y + 4.0f, 78.0f, 28.0f };
    const bool overBack = hovered(back);
    if (!r.drawGlyph(kIconBack, Rect{ back.x, back.y, 18.0f, back.h },
                     overBack ? kText : kTextDim, 11.0f)) {
        r.drawText("<", Rect{ back.x, back.y, 18.0f, back.h },
                   overBack ? kText : kTextDim, 13.0f, TextAlign::Center);
    }
    r.drawText("Back", Rect{ back.x + 18.0f, back.y, 60.0f, back.h },
               overBack ? kText : kTextDim, 12.5f, TextAlign::Left, FontWeight::Medium);
    if (clicked(back)) {
        m_detail.clear();
        m_capturingModule = nullptr;
        m_scroll = 0.0f;
        return;
    }

    // ── Title block ──
    const Rect head{ area.x, back.bottom() + 10.0f, area.w, 58.0f };
    const Rect mark{ head.x, head.y + (head.h - 46.0f) * 0.5f, 46.0f, 46.0f };
    r.fillRoundedRect(mark, 12.0f, module.enabled() ? kAccent.withAlpha(0.18f) : kField);
    drawModuleMark(module, mark, module.enabled() ? kAccent : kTextDim, 20.0f);

    r.drawText(module.name(), Rect{ mark.right() + 14.0f, head.y + 8.0f, head.w - 200.0f, 22.0f },
               kText, 16.5f, TextAlign::Left, FontWeight::SemiBold);
    r.drawText(module.description(),
               Rect{ mark.right() + 14.0f, head.y + 30.0f, head.w - 200.0f, 20.0f },
               kTextDim, 12.0f);

    const Rect masterToggle{ head.right() - 46.0f, head.y + (head.h - 24.0f) * 0.5f, 46.0f, 24.0f };
    if (widgetToggle(masterToggle, module.enabled())) {
        ModuleManager::get().requestToggle(&module);
    }

    r.drawLine(area.x, head.bottom() + 6.0f, area.right(), head.bottom() + 6.0f, kHair, 1.0f);

    // ── Settings ──
    const Rect list{ area.x, head.bottom() + 14.0f, area.w,
                     area.bottom() - (head.bottom() + 14.0f) };
    if (hovered(list)) m_scroll -= in.scroll() * 40.0f;

    r.pushClip(list);
    Rect cursor{ list.x, list.y - m_scroll, list.w, 0.0f };

    const Rect keyRow{ cursor.x, cursor.y, list.w, kRowH };
    r.drawText("Keybind", Rect{ keyRow.x + 2.0f, keyRow.y, 200.0f, keyRow.h }, kTextDim, 12.5f);
    widgetKeybind(Rect{ keyRow.right() - 116.0f, keyRow.y + 5.0f, 116.0f, 24.0f }, module);
    cursor.y += kRowH + 4.0f;

    for (auto& setting : module.settings()) {
        drawSettingRow(module, setting, cursor, list.w);
    }

    const float contentH = (cursor.y + m_scroll) - list.y;
    r.popClip();

    m_scroll = std::clamp(m_scroll, 0.0f, std::max(0.0f, contentH - list.h));
    if (contentH > list.h) {
        drawScrollbar(Rect{ area.right() - 4.0f, list.y, 4.0f, list.h },
                      m_scroll, contentH, list.h);
    }
}

void Menu::drawSettingRow(Module& /*module*/, Setting& setting, Rect& cursor, float width) {
    auto& r = Renderer::get();

    const Rect row{ cursor.x, cursor.y, width, kRowH };
    if (hovered(row)) {
        r.fillRoundedRect(row, 8.0f, Color::rgba(0x08FFFFFF));
    }
    r.drawText(setting.label(), Rect{ row.x + 2.0f, row.y, 220.0f, row.h }, kTextDim, 12.5f);

    switch (setting.type()) {
        case SettingType::Bool: {
            const Rect box{ row.right() - 46.0f, row.y + 5.0f, 46.0f, 24.0f };
            if (widgetToggle(box, setting.asBool())) setting.set(!setting.asBool());
            break;
        }
        case SettingType::Float:
        case SettingType::Int: {
            const Rect track{ row.right() - 232.0f, row.y + row.h * 0.5f - 3.0f, 176.0f, 6.0f };
            widgetSlider(track, setting);

            const bool isInt = setting.type() == SettingType::Int;
            char value[24];
            if (isInt) {
                std::snprintf(value, sizeof(value), "%d", setting.asInt());
            } else {
                std::snprintf(value, sizeof(value), "%.2f", setting.asFloat());
            }
            r.drawText(value, Rect{ row.right() - 48.0f, row.y, 48.0f, row.h },
                       kText, 12.0f, TextAlign::Right, FontWeight::Medium);
            break;
        }
        case SettingType::Color: {
            const Rect swatch{ row.right() - 52.0f, row.y + 7.0f, 52.0f, 20.0f };
            cursor.y += widgetColor(swatch, setting);
            break;
        }
        case SettingType::Key:
            // Per-setting keybinds aren't editable yet; the module-level bind
            // above covers this. Drawn as a plain value so a module that
            // declares one isn't silently ignored.
            r.drawText(keyDisplayName(setting.asInt()),
                       Rect{ row.right() - 116.0f, row.y, 116.0f, row.h },
                       kText, 12.0f, TextAlign::Right);
            break;
    }

    cursor.y += kRowH + 2.0f;
}

void Menu::drawPerformanceTab(const Rect& area) {
    auto& r = Renderer::get();
    auto& stats = FrameStats::get();

    // Sampled on a timer rather than per frame: 120 per-frame samples cover
    // two seconds, which tells you nothing you can't already see.
    const ULONGLONG now = GetTickCount64();
    if (now - m_lastSample >= 100) {
        m_lastSample = now;
        if (m_fpsCount < kGraphSamples) {
            m_fpsHistory[m_fpsCount++] = stats.fps();
        } else {
            std::move(m_fpsHistory + 1, m_fpsHistory + kGraphSamples, m_fpsHistory);
            m_fpsHistory[kGraphSamples - 1] = stats.fps();
        }
    }

    // ── Stat tiles ──
    const int seconds = stats.sessionSeconds();
    char session[16];
    std::snprintf(session, sizeof(session), "%d:%02d:%02d",
                  seconds / 3600, (seconds / 60) % 60, seconds % 60);
    char frameTime[16];
    std::snprintf(frameTime, sizeof(frameTime), "%.1f", stats.frameTimeMs());

    int enabled = 0;
    for (const auto& module : ModuleManager::get().modules()) {
        if (module->enabled()) ++enabled;
    }
    const std::string modules = std::to_string(enabled) + " / "
                              + std::to_string(ModuleManager::get().modules().size());

    const std::array<std::pair<const char*, std::string>, 4> tiles{ {
        { "FRAMES PER SECOND", std::to_string(stats.fps()) },
        { "FRAME TIME (MS)",   frameTime },
        { "SESSION",           session },
        { "MODULES ON",        modules },
    } };

    const float tileW = (area.w - 3 * kCardGap) / 4.0f;
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const Rect tile{ area.x + i * (tileW + kCardGap), area.y, tileW, 88.0f };
        r.fillRoundedRect(tile, 12.0f, kCard);
        r.strokeRoundedRect(tile, 12.0f, kPanelEdge.withAlpha(0.55f), 1.0f);
        r.drawText(tiles[i].first, Rect{ tile.x + 16.0f, tile.y + 14.0f, tile.w - 32.0f, 14.0f },
                   kTextFaint, 9.5f, TextAlign::Left, FontWeight::SemiBold);
        r.drawText(tiles[i].second, Rect{ tile.x + 16.0f, tile.y + 38.0f, tile.w - 32.0f, 34.0f },
                   i == 0 ? kAccent : kText, 26.0f, TextAlign::Left, FontWeight::Light);
    }

    // ── Graph ──
    const Rect graph{ area.x, area.y + 104.0f, area.w, std::max(120.0f, area.h - 118.0f) };
    r.fillRoundedRect(graph, 12.0f, kCard);
    r.strokeRoundedRect(graph, 12.0f, kPanelEdge.withAlpha(0.55f), 1.0f);
    r.drawText("FRAME RATE — LAST 12 SECONDS",
               Rect{ graph.x + 16.0f, graph.y + 14.0f, graph.w - 32.0f, 14.0f },
               kTextFaint, 9.5f, TextAlign::Left, FontWeight::SemiBold);

    const Rect plot = Rect{ graph.x + 16.0f, graph.y + 36.0f, graph.w - 32.0f, graph.h - 52.0f };

    // Scaled to the window's own peak, rounded up to the next 30fps step, so
    // the line uses the full height at any frame rate instead of hugging the
    // floor on a 60fps cap.
    int peak = 30;
    for (int i = 0; i < m_fpsCount; ++i) peak = std::max(peak, m_fpsHistory[i]);
    const float ceiling = static_cast<float>(((peak + 29) / 30) * 30);

    for (int step = 1; step < 4; ++step) {
        const float y = plot.bottom() - plot.h * (static_cast<float>(step) / 4.0f);
        r.drawLine(plot.x, y, plot.right(), y, kHair.withAlpha(0.35f), 1.0f);
    }
    r.drawText(std::to_string(static_cast<int>(ceiling)),
               Rect{ plot.right() - 40.0f, plot.y - 2.0f, 40.0f, 14.0f },
               kTextFaint, 9.5f, TextAlign::Right);

    if (m_fpsCount >= 2) {
        const float stride = plot.w / static_cast<float>(kGraphSamples - 1);
        for (int i = 1; i < m_fpsCount; ++i) {
            const float x0 = plot.x + (i - 1) * stride;
            const float x1 = plot.x + i * stride;
            const float y0 = plot.bottom() - plot.h * (m_fpsHistory[i - 1] / ceiling);
            const float y1 = plot.bottom() - plot.h * (m_fpsHistory[i]     / ceiling);
            r.drawLine(x0, y0, x1, y1, kAccent, 1.6f);
        }
    } else {
        r.drawText("Collecting samples...", plot, kTextFaint, 12.0f, TextAlign::Center);
    }
}

void Menu::drawModuleMark(const Module& module, const Rect& box, const Color& color, float size) {
    auto& r = Renderer::get();

    const wchar_t glyph = module.icon() ? module.icon() : categoryIcon(module.category());
    if (r.drawGlyph(glyph, box, color, size)) return;

    // No icon font at all. Initials in the same box read as a deliberate
    // choice; a tofu box reads as a bug.
    r.drawText(initials(module.name()), box, color, size * 0.62f,
               TextAlign::Center, FontWeight::SemiBold);
}

void Menu::drawScrollbar(const Rect& track, float scroll, float contentH, float viewH) {
    auto& r = Renderer::get();
    if (contentH <= viewH) return;

    r.fillRoundedRect(track, track.w * 0.5f, kTrack.withAlpha(0.35f));

    const float thumbH = std::max(28.0f, track.h * (viewH / contentH));
    const float travel = track.h - thumbH;
    const float t = scroll / (contentH - viewH);
    r.fillRoundedRect(Rect{ track.x, track.y + travel * t, track.w, thumbH },
                      track.w * 0.5f, kTextFaint.withAlpha(0.75f));
}

// ── Widgets ─────────────────────────────────────────────────────────────────

bool Menu::widgetChip(const Rect& r, std::string_view label, bool selected) {
    auto& gfx = Renderer::get();

    if (selected) {
        gfx.fillRoundedRect(r, r.h * 0.5f, kAccent);
    } else if (hovered(r)) {
        gfx.fillRoundedRect(r, r.h * 0.5f, kCardHover);
    }
    gfx.drawText(label, r, selected ? Color::rgba(0xFF0B1020) : kTextDim, 11.8f,
                 TextAlign::Center, FontWeight::Medium);
    return clicked(r);
}

bool Menu::widgetIconButton(const Rect& r, wchar_t glyph, bool emphasised) {
    auto& gfx = Renderer::get();

    const bool over = hovered(r);
    gfx.fillRoundedRect(r, r.h * 0.5f, over ? kCardHover : kField);
    if (emphasised || over) {
        gfx.strokeRoundedRect(r, r.h * 0.5f, kPanelEdge, 1.0f);
    }

    const Color tint = over ? kText : kTextDim;
    if (!gfx.drawGlyph(glyph, r, tint, r.h * 0.36f)) {
        // Two crossed strokes: correct for the close button, and the only
        // icon button in the menu, so a generic fallback is honest here.
        const float inset = r.w * 0.34f;
        gfx.drawLine(r.x + inset, r.y + inset, r.right() - inset, r.bottom() - inset, tint, 1.5f);
        gfx.drawLine(r.right() - inset, r.y + inset, r.x + inset, r.bottom() - inset, tint, 1.5f);
    }
    return clicked(r);
}

bool Menu::widgetToggle(const Rect& r, bool value) {
    auto& gfx = Renderer::get();

    gfx.fillRoundedRect(r, r.h * 0.5f, value ? kAccent : kTrack);

    const float knob = r.h - 6.0f;
    const float knobX = value ? (r.right() - knob - 3.0f) : (r.x + 3.0f);
    gfx.fillEllipse(knobX + knob * 0.5f, r.y + r.h * 0.5f, knob * 0.5f, knob * 0.5f,
                    Color::rgba(0xFFFFFFFF));

    if (hovered(r)) {
        gfx.strokeRoundedRect(r, r.h * 0.5f, kAccent.withAlpha(0.55f), 1.5f);
    }
    return clicked(r);
}

bool Menu::widgetSlider(const Rect& r, Setting& setting) {
    auto& gfx = Renderer::get();
    auto& in  = Input::get();

    // Grabbing anywhere on a padded band around the track starts a drag — a
    // 6px-tall target is unusable otherwise.
    const Rect grab{ r.x, r.y - 9.0f, r.w, r.h + 18.0f };
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

    const float knob = (active || hovered(grab)) ? 8.0f : 6.5f;
    gfx.fillEllipse(r.x + r.w * t, r.y + r.h * 0.5f, knob, knob, Color::rgba(0xFFFFFFFF));

    return active;
}

float Menu::widgetColor(const Rect& swatch, Setting& setting) {
    auto& gfx = Renderer::get();
    auto& in  = Input::get();

    // Checkerboard behind the swatch so a low-alpha color reads as translucent
    // rather than as a dark color.
    constexpr float kCheck = 5.0f;
    gfx.pushClip(swatch);
    for (int i = 0; i * kCheck < swatch.w; ++i) {
        for (int j = 0; j * kCheck < swatch.h; ++j) {
            if (((i + j) & 1) == 0) continue;
            gfx.fillRect(Rect{ swatch.x + i * kCheck, swatch.y + j * kCheck, kCheck, kCheck },
                         Color::rgba(0xFF4A4F58));
        }
    }
    gfx.popClip();

    gfx.fillRoundedRect(swatch, 6.0f, Color::rgba(setting.asColor()));
    gfx.strokeRoundedRect(swatch, 6.0f,
                          hovered(swatch) ? kAccent : kPanelEdge, 1.0f);

    if (clicked(swatch)) {
        m_expandedColor = (m_expandedColor == &setting) ? nullptr : &setting;
    }
    if (m_expandedColor != &setting) return 0.0f;

    // Expanded: one slider per channel, drawn below the swatch row, so the
    // caller must be told how much vertical space they consumed.
    static constexpr std::array<const char*, 4> kNames{ "A", "R", "G", "B" };
    float consumed = 0.0f;

    for (int channel = 0; channel < 4; ++channel) {
        const float y = swatch.bottom() + 12.0f + static_cast<float>(channel) * 22.0f;
        const Rect track{ swatch.right() - 232.0f, y, 176.0f, 6.0f };

        gfx.drawText(kNames[channel], Rect{ track.x - 18.0f, y - 8.0f, 14.0f, 22.0f },
                     kTextDim, 11.0f);

        const Rect grab{ track.x, track.y - 9.0f, track.w, track.h + 18.0f };
        if (in.leftPressed() && hovered(grab) && m_draggingChannel < 0) {
            m_draggingChannel = channel;
            m_expandedColor = &setting;
        }
        if (!in.leftDown() && m_draggingChannel == channel) {
            m_draggingChannel = -1;
        }
        if (m_draggingChannel == channel && track.w > 0.0f) {
            const float t = std::clamp((in.mouseX() - track.x) / track.w, 0.0f, 1.0f);
            setting.setChannel(channel, static_cast<int>(t * 255.0f + 0.5f));
        }

        const float t = static_cast<float>(setting.channel(channel)) / 255.0f;
        gfx.fillRoundedRect(track, track.h * 0.5f, kTrack);
        gfx.fillRoundedRect(Rect{ track.x, track.y, track.w * t, track.h },
                            track.h * 0.5f, kAccent);
        gfx.fillEllipse(track.x + track.w * t, track.y + track.h * 0.5f, 6.5f, 6.5f,
                        Color::rgba(0xFFFFFFFF));

        gfx.drawText(std::to_string(setting.channel(channel)),
                     Rect{ track.right() + 8.0f, y - 8.0f, 40.0f, 22.0f },
                     kText, 11.0f, TextAlign::Right);

        consumed += 22.0f;
    }

    return consumed + 14.0f;
}

bool Menu::widgetKeybind(const Rect& r, Module& module) {
    auto& gfx = Renderer::get();
    auto& in  = Input::get();

    const bool capturing = (m_capturingModule == &module);

    gfx.fillRoundedRect(r, 8.0f, capturing ? kAccent.withAlpha(0.22f) : kField);
    if (hovered(r) || capturing) {
        gfx.strokeRoundedRect(r, 8.0f, kAccent.withAlpha(capturing ? 0.9f : 0.5f), 1.0f);
    }

    gfx.drawText(capturing ? "Press a key..." : keyDisplayName(module.keybind()),
                 r, capturing ? kAccent : kText, 11.5f, TextAlign::Center, FontWeight::Medium);

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
