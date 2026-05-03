#pragma once
//
// Glacier brand tokens.
// These match the Glacier-Launcher (WPF/Blazor) so the in-game DLL menu and
// the desktop launcher feel like one product. Hex codes are taken from the
// launcher's wwwroot CSS palette.
//
#include <cstdint>

namespace Glacier {

// ---- Palette (0xAARRGGBB packed) -------------------------------------------
constexpr std::uint32_t COL_ACCENT       = 0xFF7289DA; // brand primary
constexpr std::uint32_t COL_ACCENT_HOVER = 0xFF8EA0E0;
constexpr std::uint32_t COL_ACCENT_DEEP  = 0xFF5865F2; // discord-blue, 2nd accent

constexpr std::uint32_t COL_BG_BASE      = 0xFF23272A;
constexpr std::uint32_t COL_BG_PANEL     = 0xFF2C2F33;
constexpr std::uint32_t COL_BG_DEEP      = 0xFF1E2124;
constexpr std::uint32_t COL_BG_DEEPEST   = 0xFF141520;

constexpr std::uint32_t COL_TEXT         = 0xFFFFFFFF;
constexpr std::uint32_t COL_TEXT_DIM     = 0xFF99AAB5;
constexpr std::uint32_t COL_TEXT_DARK    = 0xFF6B7280;

constexpr std::uint32_t COL_SUCCESS      = 0xFF43B581;
constexpr std::uint32_t COL_DANGER       = 0xFFF04747;
constexpr std::uint32_t COL_WARNING      = 0xFFFAA61A;

// ---- Geometry --------------------------------------------------------------
constexpr float ROUND_SM = 8.0f;
constexpr float ROUND_MD = 12.0f;
constexpr float ROUND_LG = 20.0f;
constexpr float ROUND_PILL = 999.0f;

// ---- Typography ------------------------------------------------------------
constexpr float FONT_SIZE_SM = 13.0f;
constexpr float FONT_SIZE_MD = 15.0f;
constexpr float FONT_SIZE_LG = 18.0f;
constexpr float FONT_SIZE_XL = 24.0f;

// ---- Brand strings (filled by CMake) ---------------------------------------
#ifndef GLACIER_VERSION
#define GLACIER_VERSION "0.0.0-dev"
#endif
#ifndef GLACIER_BRAND
#define GLACIER_BRAND "Glacier"
#endif

inline constexpr const char* kVersion = GLACIER_VERSION;
inline constexpr const char* kBrand   = GLACIER_BRAND;

} // namespace Glacier
