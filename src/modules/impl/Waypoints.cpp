#include "Waypoints.h"
#include "../../sdk/ClientInstance.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

static constexpr float kPi = 3.14159265f;

Waypoints::Waypoints()
    : ModuleBase("Waypoints", "Marks world positions with colored labels visible through terrain",
                 "waypoints", ModuleCategory::Utility)
{
    m_settings.defineBool ("showDist",   "Show Distance",    true);
    m_settings.defineBool ("showArrow",  "Show Arrow",       true);
    m_settings.defineFloat("maxDist",    "Max Distance",     512.f, 16.f, 2048.f);
    m_settings.defineFloat("fontSize",   "Font Size",        14.f,  8.f, 28.f);
}

void Waypoints::onRender(ImDrawList* dl) {
    if (m_waypoints.empty()) return;
    auto* lp = getLocalPlayer();
    if (!lp) return;
    const float* vp = ViewProjectionCache::get().get4x4();
    if (!vp) return;

    float maxDist = m_settings.getFloat("maxDist");
    float fs      = m_settings.getFloat("fontSize");
    bool  showDist  = m_settings.getBool("showDist");
    bool  showArrow = m_settings.getBool("showArrow");
    Vec3  origin    = lp->getPosition();
    int   dim       = lp->getDimension();

    for (auto& wp : m_waypoints) {
        if (wp.dimension != dim) continue;
        float dx = wp.x - origin.x, dy = wp.y - origin.y, dz = wp.z - origin.z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        if (dist > maxDist) continue;

        ImU32 col = IM_COL32((int)(wp.r*255), (int)(wp.g*255), (int)(wp.b*255), 255);

        ImVec2 screen;
        bool onScreen = worldToScreen({wp.x, wp.y + 1.f, wp.z}, screen, vp);

        if (onScreen) {
            const ImGuiIO& io = ImGui::GetIO();
            screen.x = std::clamp(screen.x, 8.f, io.DisplaySize.x - 8.f);
            screen.y = std::clamp(screen.y, 8.f, io.DisplaySize.y - 8.f);

            char label[128];
            if (showDist)
                snprintf(label, sizeof(label), "%s [%.0fm]", wp.name.c_str(), dist);
            else
                snprintf(label, sizeof(label), "%s", wp.name.c_str());

            ImVec2 tsz = ImGui::CalcTextSize(label);
            float bx = screen.x - tsz.x * 0.5f - 4.f;
            float by = screen.y - tsz.y - 6.f;
            dl->AddRectFilled({bx, by}, {bx + tsz.x + 8.f, by + tsz.y + 4.f},
                IM_COL32(0,0,0,160), 4.f);
            dl->AddRect({bx, by}, {bx + tsz.x + 8.f, by + tsz.y + 4.f}, col, 4.f, 0, 1.f);
            dl->AddText(nullptr, fs, {bx + 4.f, by + 2.f}, col, label);

            // Dot
            dl->AddCircleFilled({screen.x, screen.y + 2.f}, 3.5f, col);
        } else if (showArrow) {
            // Edge arrow pointing toward off-screen waypoint
            const ImGuiIO& io = ImGui::GetIO();
            float cx = io.DisplaySize.x * 0.5f, cy = io.DisplaySize.y * 0.5f;
            float angle  = atan2f(dz, dx);
            float margin = 40.f;
            float ax = cx + cosf(angle) * (cx - margin);
            float ay = cy + sinf(angle) * (cy - margin);
            ax = std::clamp(ax, margin, io.DisplaySize.x - margin);
            ay = std::clamp(ay, margin, io.DisplaySize.y - margin);

            // Arrow triangle
            float as = 8.f;
            ImVec2 ap1 = { ax + cosf(angle) * as, ay + sinf(angle) * as };
            ImVec2 ap2 = { ax + cosf(angle + kPi * 0.75f) * as, ay + sinf(angle + kPi * 0.75f) * as };
            ImVec2 ap3 = { ax + cosf(angle - kPi * 0.75f) * as, ay + sinf(angle - kPi * 0.75f) * as };
            dl->AddTriangleFilled(ap1, ap2, ap3, col);

            if (showDist) {
                char dbuf[32]; snprintf(dbuf, sizeof(dbuf), "%.0fm", dist);
                ImVec2 dsz = ImGui::CalcTextSize(dbuf);
                dl->AddText({ ax - dsz.x * 0.5f, ay + as + 2.f }, col, dbuf);
            }
        }
    }
}

void Waypoints::onRenderImGui() {
    // Waypoint manager sub-panel — called by ModMenu when this module is selected & shown
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.667f, 0.71f, 1.f));
    ImGui::TextUnformatted("Waypoints");
    ImGui::PopStyleColor();

    auto* lp = getLocalPlayer();

    for (int i = 0; i < (int)m_waypoints.size(); i++) {
        auto& wp = m_waypoints[i];
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(wp.r, wp.g, wp.b, 1.f));
        ImGui::Bullet();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s (%.0f,%.0f,%.0f)", wp.name.c_str(), wp.x, wp.y, wp.z);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            m_waypoints.erase(m_waypoints.begin() + i);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::InputTextWithHint("##wpname", "Name...", m_newName, sizeof(m_newName));
    if (ImGui::Button("Add at Current Pos") && m_newName[0] && lp) {
        Vec3 p = lp->getPosition();
        m_waypoints.push_back({ m_newName, p.x, p.y, p.z, lp->getDimension(), 0.447f, 0.537f, 0.855f });
        m_newName[0] = '\0';
    }
}
