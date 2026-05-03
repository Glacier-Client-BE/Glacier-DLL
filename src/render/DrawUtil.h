#pragma once
//
// Glacier-themed primitives drawn straight to an ImDrawList. These keep the
// HUD overlays consistent with the click-GUI's palette.
//
#include <imgui.h>
#include <string>

namespace Glacier::Draw {

// Filled rounded rect with a soft drop shadow + (optional) accent ring.
void panel(ImDrawList* dl, ImVec2 p, ImVec2 size,
           unsigned bgARGB, float radius = 12.f,
           bool accent = false, float shadowAlpha = 0.45f);

// Pill background + label, used by status chips like "70 FPS" / "X: 0.0".
void chip(ImDrawList* dl, ImVec2 p, std::string_view label, unsigned accentARGB);

// Outlined text (1px stroke) for HUD legibility on bright backgrounds.
void textShadow(ImDrawList* dl, ImVec2 p, std::string_view text,
                unsigned fillARGB = 0xFFFFFFFF, unsigned shadowARGB = 0xC0000000);

// Convert 0xAARRGGBB to ImU32.
unsigned argbToImU32(unsigned argb);

} // namespace Glacier::Draw
