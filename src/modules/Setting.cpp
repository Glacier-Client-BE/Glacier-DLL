#include "Setting.h"
#include "../core/Glacier.h"
#include "../render/Theme.h"
#include <imgui.h>
#include <Windows.h>

namespace Glacier {

// ---------------------------------------------------------------------------
// BoolSetting
// ---------------------------------------------------------------------------
void BoolSetting::draw() {
    bool v = value_;
    if (ImGui::Checkbox(display_.c_str(), &v)) value_ = v;
    if (!description_.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description_.c_str());
}

// ---------------------------------------------------------------------------
// FloatSetting
// ---------------------------------------------------------------------------
void FloatSetting::draw() {
    float v = value_;
    ImGui::PushItemWidth(180);
    if (ImGui::SliderFloat(display_.c_str(), &v, min_, max_, "%.2f")) value_ = v;
    ImGui::PopItemWidth();
    if (!description_.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description_.c_str());
}
std::string FloatSetting::format() const {
    char b[32]; std::snprintf(b, sizeof(b), "%.2f", value_); return b;
}

// ---------------------------------------------------------------------------
// IntSetting
// ---------------------------------------------------------------------------
void IntSetting::draw() {
    int v = value_;
    ImGui::PushItemWidth(180);
    if (ImGui::SliderInt(display_.c_str(), &v, min_, max_)) value_ = v;
    ImGui::PopItemWidth();
    if (!description_.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description_.c_str());
}
std::string IntSetting::format() const { return std::to_string(value_); }

// ---------------------------------------------------------------------------
// EnumSetting
// ---------------------------------------------------------------------------
void EnumSetting::draw() {
    ImGui::PushItemWidth(180);
    if (ImGui::BeginCombo(display_.c_str(), options_[value_].c_str())) {
        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            bool sel = i == value_;
            if (ImGui::Selectable(options_[i].c_str(), sel)) value_ = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    if (!description_.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description_.c_str());
}

// ---------------------------------------------------------------------------
// ColorSetting
// ---------------------------------------------------------------------------
void ColorSetting::draw() {
    auto v = Theme::toVec4(value_);
    float c[4] = { v.x, v.y, v.z, v.w };
    if (ImGui::ColorEdit4(display_.c_str(), c, ImGuiColorEditFlags_AlphaBar)) {
        auto pack = [](float f) { return static_cast<std::uint32_t>(f * 255.f + 0.5f); };
        value_ = (pack(c[3]) << 24) | (pack(c[0]) << 16) | (pack(c[1]) << 8) | pack(c[2]);
    }
    if (!description_.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description_.c_str());
}

// ---------------------------------------------------------------------------
// KeybindSetting
// ---------------------------------------------------------------------------
static const char* vkName(int vk) {
    static char buf[16];
    if (vk == 0) return "None";
    if (vk == VK_LBUTTON) return "LMB";
    if (vk == VK_RBUTTON) return "RMB";
    if (vk == VK_MBUTTON) return "MMB";
    UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    GetKeyNameTextA(static_cast<LONG>(scan << 16), buf, sizeof(buf));
    return buf[0] ? buf : "?";
}

void KeybindSetting::draw() {
    static int* gListening = nullptr;
    auto* mySlot = &value_;
    bool listening = (gListening == mySlot);
    char label[64];
    std::snprintf(label, sizeof(label), "%s##key_%s", listening ? "[press a key]" : vkName(value_), name_.c_str());
    if (ImGui::Button(label, ImVec2(120, 0))) {
        gListening = listening ? nullptr : mySlot;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(display_.c_str());
    if (listening) {
        for (int i = 1; i < 256; ++i) {
            if (i == VK_LBUTTON || i == VK_RBUTTON || i == VK_MBUTTON) continue;
            if (GetAsyncKeyState(i) & 0x8000) {
                value_ = (i == VK_ESCAPE) ? 0 : i;
                gListening = nullptr;
                break;
            }
        }
    }
    if (!description_.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description_.c_str());
}

// ---------------------------------------------------------------------------
// TextSetting
// ---------------------------------------------------------------------------
void TextSetting::draw() {
    char buf[256]{};
    std::snprintf(buf, sizeof(buf), "%s", value_.c_str());
    ImGui::PushItemWidth(220);
    if (ImGui::InputText(display_.c_str(), buf, sizeof(buf))) value_ = buf;
    ImGui::PopItemWidth();
    if (!description_.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description_.c_str());
}

} // namespace Glacier
