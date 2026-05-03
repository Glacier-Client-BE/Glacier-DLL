#include "DrawUtil.h"
#include "../core/Glacier.h"

namespace Glacier::Draw {

unsigned argbToImU32(unsigned argb) {
    // 0xAARRGGBB -> 0xAABBGGRR (ImU32)
    return ((argb & 0xFF000000)) |
           ((argb & 0x00FF0000) >> 16) |
           ((argb & 0x0000FF00)) |
           ((argb & 0x000000FF) << 16);
}

void panel(ImDrawList* dl, ImVec2 p, ImVec2 size, unsigned bgARGB,
           float radius, bool accent, float shadowAlpha) {
    // shadow
    auto sa = static_cast<unsigned>(255.f * shadowAlpha);
    unsigned shadow = (sa << 24);
    for (int i = 0; i < 4; ++i) {
        float k = 4.f - static_cast<float>(i);
        dl->AddRectFilled(
            ImVec2(p.x - k, p.y - k),
            ImVec2(p.x + size.x + k, p.y + size.y + k),
            argbToImU32(shadow), radius + k);
    }
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), argbToImU32(bgARGB), radius);
    if (accent) {
        dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), argbToImU32(COL_ACCENT), radius, 0, 1.5f);
    }
}

void chip(ImDrawList* dl, ImVec2 p, std::string_view label, unsigned accentARGB) {
    ImVec2 ts = ImGui::CalcTextSize(label.data(), label.data() + label.size());
    ImVec2 pad{ 10.f, 5.f };
    ImVec2 size{ ts.x + pad.x * 2, ts.y + pad.y * 2 };
    panel(dl, p, size, COL_BG_PANEL, ROUND_PILL, false, 0.35f);
    // Accent dot
    ImVec2 dot{ p.x + 9.f, p.y + size.y * 0.5f };
    dl->AddCircleFilled(dot, 3.5f, argbToImU32(accentARGB));
    // Text
    ImVec2 tp{ p.x + 18.f, p.y + pad.y };
    dl->AddText(tp, argbToImU32(COL_TEXT), label.data(), label.data() + label.size());
}

void textShadow(ImDrawList* dl, ImVec2 p, std::string_view text,
                unsigned fillARGB, unsigned shadowARGB) {
    auto s = argbToImU32(shadowARGB);
    auto f = argbToImU32(fillARGB);
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy)
                dl->AddText(ImVec2(p.x + dx, p.y + dy), s, text.data(), text.data() + text.size());
    dl->AddText(p, f, text.data(), text.data() + text.size());
}

} // namespace Glacier::Draw
