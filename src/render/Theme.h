#pragma once
//
// Glacier ImGui style. Applies the launcher palette + 12px rounded corners +
// soft accent glow consistently with %appdata%\Glacier\config.json overrides.
//
struct ImGuiStyle;
struct ImVec4;

namespace Glacier {

class Theme {
public:
    static void apply();              // applies to the current ImGui context
    static ImVec4 toVec4(unsigned argb); // 0xAARRGGBB -> ImVec4 (0..1)
};

} // namespace Glacier
