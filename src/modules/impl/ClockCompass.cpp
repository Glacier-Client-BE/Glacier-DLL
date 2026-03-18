#include "ClockCompass.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <ctime>
#include <cstdio>
#include <cmath>

static constexpr float kPi = 3.14159265f;

ClockCompass::ClockCompass()
    : ModuleBase("Clock & Compass", "Shows current time and a directional compass",
                 ICON_FA_COMPASS, ModuleCategory::HUD, 10.f, 60.f)
{
    m_settings.defineBool ("showClock",   "Show Clock",       true);
    m_settings.defineBool ("show24h",     "24-Hour Format",   false);
    m_settings.defineBool ("showCompass", "Show Compass",     true);
    m_settings.defineBool ("showDegrees", "Show Degrees",     false);
    m_settings.defineFloat("scale",       "Scale",            1.f, 0.5f, 2.f);
    m_settings.defineFloat("bgAlpha",     "BG Alpha",         0.85f, 0.f, 1.f);
}

void ClockCompass::onRenderImGui() {
    float sc = m_settings.getFloat("scale");
    float bga= m_settings.getFloat("bgAlpha");
    bool  sClock   = m_settings.getBool("showClock");
    bool  sCompass = m_settings.getBool("showCompass");
    bool  s24      = m_settings.getBool("show24h");
    bool  sDeg     = m_settings.getBool("showDegrees");

    float w = 150.f * sc;
    float h = ((sClock ? 22.f : 0.f) + (sCompass ? 28.f : 0.f) + 10.f) * sc;

    ImGui::SetNextWindowPos(m_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ w, h });
    ImGui::SetNextWindowBgAlpha(bga);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg,  ImVec4(0.137f,0.153f,0.165f,bga));
    ImGui::PushStyleColor(ImGuiCol_Border,    ImVec4(0.447f,0.537f,0.855f,0.35f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5.f * sc, 5.f * sc });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f * sc);

    if (ImGui::Begin("##clockcompass", nullptr, f)) {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            auto d = ImGui::GetIO().MouseDelta;
            m_pos.x += d.x; m_pos.y += d.y;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float cx = wp.x + 5.f * sc, cy = wp.y + 5.f * sc;

        // ── Clock ────────────────────────────────────────────────────────
        if (sClock) {
            time_t t = time(nullptr);
            tm*    tm = localtime(&t);
            char buf[32];
            if (s24)
                snprintf(buf, sizeof(buf), ICON_FA_CLOCK "  %02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
            else {
                int h12 = tm->tm_hour % 12; if (h12 == 0) h12 = 12;
                snprintf(buf, sizeof(buf), ICON_FA_CLOCK "  %d:%02d %s",
                    h12, tm->tm_min, tm->tm_hour >= 12 ? "PM" : "AM");
            }
            ImVec2 tsz = ImGui::CalcTextSize(buf);
            dl->AddText(nullptr, 13.f * sc, { cx + (w - 10.f * sc - tsz.x) * 0.5f, cy },
                IM_COL32(255,255,255,230), buf);
            cy += 22.f * sc;
        }

        // ── Compass ──────────────────────────────────────────────────────
        if (sCompass) {
            auto* lp = getLocalPlayer();
            float yaw = lp ? lp->getYaw() : 0.f;

            // Normalize yaw to 0-360
            while (yaw < 0.f)   yaw += 360.f;
            while (yaw >= 360.f) yaw -= 360.f;

            const char* dirs[] = { "S", "SW", "W", "NW", "N", "NE", "E", "SE" };
            ImU32 dirCols[] = {
                IM_COL32(237,70,70,255), IM_COL32(200,200,200,160),
                IM_COL32(200,200,200,160), IM_COL32(200,200,200,160),
                IM_COL32(114,137,218,255), IM_COL32(200,200,200,160),
                IM_COL32(200,200,200,160), IM_COL32(200,200,200,160),
            };

            float compassW = w - 10.f * sc;
            float compassX = cx;
            float compassY = cy;
            float compassH = 24.f * sc;
            float segW     = compassW / 8.f;

            for (int i = 0; i < 8; i++) {
                float ang = (float)i * 45.f;
                float diff = ang - yaw;
                while (diff >  180.f) diff -= 360.f;
                while (diff < -180.f) diff += 360.f;

                if (fabsf(diff) > 90.f) continue;

                float t   = diff / 90.f;
                float sx  = compassX + compassW * 0.5f + t * compassW * 0.5f;
                float fade = 1.f - fabsf(t);

                ImVec2 tsz = ImGui::CalcTextSize(dirs[i]);
                float scale2 = (1.f + (1.f - fabsf(t)) * 0.5f) * sc;

                ImU32 col = dirCols[i];
                col = (col & 0x00FFFFFF) | ((ImU32)(((col >> 24) & 0xFF) * fade) << 24);

                if (fabsf(diff) < 2.f) {
                    // Current direction — highlighted
                    float hx = sx - tsz.x * scale2 * 0.5f - 3.f;
                    float hy = compassY + (compassH - tsz.y * scale2) * 0.5f - 2.f;
                    dl->AddRectFilled({ hx, hy }, { hx + tsz.x * scale2 + 6.f, hy + tsz.y * scale2 + 4.f },
                        IM_COL32(114,137,218,30), 3.f);
                }
                dl->AddText(ImGui::GetFont(), 13.f * scale2,
                    { sx - tsz.x * scale2 * 0.5f, compassY + (compassH - tsz.y * scale2) * 0.5f },
                    col, dirs[i]);
            }

            if (sDeg) {
                char dbuf[16]; snprintf(dbuf, sizeof(dbuf), "%.1f\xc2\xb0", yaw);
                ImVec2 dsz = ImGui::CalcTextSize(dbuf);
                dl->AddText(nullptr, 10.f * sc,
                    { cx + (compassW - dsz.x) * 0.5f, compassY + compassH },
                    IM_COL32(153,170,181,180), dbuf);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
