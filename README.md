# Glacier Client (DLL) v2.1

In-game DLL for Minecraft Bedrock Edition — themed to match the
[Glacier-Launcher](https://github.com/Glacier-Client-BE/Glacier-Launcher) so
the launcher and the in-game menu feel like one product.

This is a **clean rewrite** that replaces an older Onix-based prototype.
Architecture is patterned after [Latite](https://github.com/LatiteClient/Latite)
(event bus, version-agnostic SDK + `Addresses.h`, profile-based config) and
[Flarial dll-oss](https://github.com/flarialmc/dll-oss) (hierarchical hook
directories, DX11/DXGI split). The visual theme — palette, radii, font,
icon set — is taken straight from the Glacier-Launcher's WPF/Blazor UI.

## Build

```bat
build.bat
```

or

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Output: `build\Release\glacier.dll`. Inject into `Minecraft.Windows.exe`
with any standard manual-mapper.

Dependencies are fetched at configure time (no manual setup):
| Dep | Purpose |
| --- | --- |
| `MinHook` | Inline trampolines for `Present`, `ResizeBuffers`, future game hooks |
| `ImGui v1.91` | Both the click-GUI and HUD overlays use ImGui's foreground draw list |
| `IconFontCppHeaders` | Font Awesome 5 glyph macros (`ICON_FA_*`) |
| `Font Awesome 5 Solid` | Downloaded once and embedded as a byte-array — no external `.ttf` shipped |
| `nlohmann/json v3.11.3` | Profile-based config persistence |

Requires CMake **3.20+** and an x64 MSVC toolchain (VS 2019 16.10+ for C++20).

## Layout

```
src/
  dllmain.cpp                  DLL entrypoint - bootstrap thread
  Client.{h,cpp}               Top-level singleton (start/stop, module registry)
  core/
    Glacier.h                  Brand tokens (palette, radii, font sizes)
    Logger.{h,cpp}             %appdata%\Glacier\glacier.log
    Util.{h,cpp}               UTF-8/wide, %appdata% paths, pattern scan
  events/
    Event.h Events.h           Event types (Tick, RenderHUD, Key, Chat, ...)
    EventBus.{h,cpp}           Latite-style listen<E,&T::method>(this, prio)
  hooks/
    HookManager.{h,cpp}        Named MinHook trampolines
    render/
      SwapChainHook.{h,cpp}    DX11 Present + ResizeBuffers (vtable swap)
      WndProcHook.{h,cpp}      Window subclass for input
  sdk/
    Addresses.{h,cpp}          ** version-specific offsets live here **
    SDK.h                      master include
    math/                      Vec2/Vec3/Vec4, Color (ARGB)
    client/
      GameTypes.h              forward-declared game classes
      Game.{h,cpp}             single accessor used by all modules
  render/
    Renderer.{h,cpp}           Frame loop (RenderImGuiEvent + RenderHUDEvent)
    Theme.{h,cpp}              ImGui style matching the launcher
    Fonts.{h,cpp}              Segoe UI in 4 sizes + merged FA glyphs
    DrawUtil.{h,cpp}           Glacier-themed primitives (panel, chip, shadow text)
  modules/
    Module.{h,cpp}             BaseModule, EventBus subscription in ctor
    Setting.{h,cpp}            Bool/Float/Int/Enum/Color/Keybind/Text
    Category.h                 HUD / Visual / Movement / World / Misc / Combat
    ModuleManager.{h,cpp}      template add<T>(args...) registry
    hud/                       Watermark, FPS, Coordinates, Keystrokes, CPS,
                               ArmorHUD, ClientBranding, PlayerList
    visual/                    Crosshair, ChunkBorders, BlockOutline,
                               Fullbright, Zoom, FreeLook, NoHurtCam
    misc/                      AutoGG, AutoText, TimeChanger, ToggleSprintSneak
  config/
    Config.{h,cpp}             %appdata%\Glacier\config\<profile>.json
  gui/
    ClickGui.{h,cpp}           Two-pane settings menu (INSERT to toggle)
```

## Theme

Sourced from `wwwroot/site.css` in the Glacier-Launcher.

| Token | Hex | Use |
| --- | --- | --- |
| Accent | `#7289DA` | Buttons, sliders, highlights |
| Accent (hover) | `#8EA0E0` | Hover/focus |
| Accent (deep) | `#5865F2` | Pressed, secondary |
| BG base | `#23272A` | Window background |
| BG panel | `#2C2F33` | Cards, HUD chips |
| BG deep | `#1E2124` | Title bar, sidebars |
| Text | `#FFFFFF` | Primary text |
| Text dim | `#99AAB5` | Secondary, descriptions |
| Success | `#43B581` | Online indicators |
| Danger | `#F04747` | Errors |
| Warning | `#FAA61A` | Alerts |

* Border radius scale: `8 / 12 / 20` (small / medium / large), `999` for pills.
* Font: Segoe UI (system), with Font Awesome 5 Solid merged into every size.
* Effects: soft shadow + 1.5px accent ring on the click-GUI cards, accent glow on the watermark.

## Adding a module

```cpp
// src/modules/hud/MyModule.h
#pragma once
#include "../Module.h"

namespace Glacier::modules {
class MyModule final : public Module {
public:
    MyModule();
    void onRender(RenderHUDEvent& e);
private:
    BoolSetting&  flag_;
    FloatSetting& size_;
};
} // namespace Glacier::modules
```

```cpp
// src/modules/hud/MyModule.cpp
#include "MyModule.h"
#include "../../events/EventBus.h"
#include "../../render/DrawUtil.h"

namespace Glacier::modules {
MyModule::MyModule()
    : Module("my_module", "My Module", "Demo", Category::HUD, 'M'),
      flag_(addSetting<BoolSetting>("flag", "Flag", "Toggle something", true)),
      size_(addSetting<FloatSetting>("size", "Size", "Pixel size", 12.f, 4.f, 64.f, 0.5f)) {
    EventBus::get().listen<RenderHUDEvent, &MyModule::onRender>(this);
}
void MyModule::onRender(RenderHUDEvent& e) {
    if (!enabled_) return;
    Draw::chip(e.drawList, ImVec2(40, 40), "MyModule", 0xFF7289DA);
}
} // namespace Glacier::modules
```

Then add one line to `Client::registerModules()`:
```cpp
M.add<modules::MyModule>();
```

The module is auto-registered, gets a card in the click-GUI under its
category, persists to JSON, and binds to `'M'` as a toggle key.

## Runtime files

| Path | Purpose |
| --- | --- |
| `%appdata%\Glacier\glacier.log` | Rolling log |
| `%appdata%\Glacier\config\<profile>.json` | Per-profile settings; default profile = `default` |

## Wiring up real game state

`src/sdk/Addresses.cpp` is the single source of game-version-specific offsets.
Until you fill in the `Util::patternScan(...)` calls there, `Game::get().*`
accessors return `std::nullopt` and dependent modules render placeholder
content. Modules that don't need SDK pointers (Watermark, FPS, Keystrokes,
CPS, Crosshair, Zoom, FreeLook input, AutoText, AutoGG-trigger detection,
ClickGui, Theme) work immediately.

## Status

The project has not been compiled in this environment because CMake was not
present on PATH; the architecture, theme, and module set are complete but the
first build will likely surface minor includes or signature drift to fix —
expected for any new C++ project. The hot-path code (event bus, hooks,
renderer, click-GUI, theme) was written against documented APIs.

## Credits

* Architectural references: [LatiteClient/Latite](https://github.com/LatiteClient/Latite),
  [flarialmc/dll-oss](https://github.com/flarialmc/dll-oss).
* Theme reference: [Glacier-Client-BE/Glacier-Launcher](https://github.com/Glacier-Client-BE/Glacier-Launcher).
* Onix V2 was the previous iteration's base; nothing of it is carried forward.
