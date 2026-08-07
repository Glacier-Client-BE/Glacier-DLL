#pragma once

#include <string>
#include <Windows.h>

// Human-readable virtual-key names, shared by the menu's keybind widget and the
// startup log. Both need to answer "which key is this?" and they must agree —
// a log that says RSHIFT while the menu shows something else is worse than
// either alone.
namespace glacier::ui {

inline std::string keyDisplayName(int vk) {
    if (vk == 0) return "None";

    // GetKeyNameTextW can't distinguish the left/right modifier pairs (it
    // reports both as plain "Shift"), which is exactly the distinction that
    // matters for a menu bound to RSHIFT. Name those explicitly.
    switch (vk) {
        case VK_LSHIFT:   return "Left Shift";
        case VK_RSHIFT:   return "Right Shift";
        case VK_LCONTROL: return "Left Ctrl";
        case VK_RCONTROL: return "Right Ctrl";
        case VK_LMENU:    return "Left Alt";
        case VK_RMENU:    return "Right Alt";
        case VK_LBUTTON:  return "Mouse 1";
        case VK_RBUTTON:  return "Mouse 2";
        case VK_MBUTTON:  return "Mouse 3";
        default: break;
    }

    UINT scan = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    switch (vk) {
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
        case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
        case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
            scan |= 0x100;   // extended key
            break;
        default:
            break;
    }

    wchar_t buf[64]{};
    if (GetKeyNameTextW(static_cast<LONG>(scan) << 16, buf, 64) > 0) {
        char out[128]{};
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, sizeof(out), nullptr, nullptr);
        return out;
    }
    return "Key " + std::to_string(vk);
}

} // namespace glacier::ui
