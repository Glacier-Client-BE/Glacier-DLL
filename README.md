# Glacier Client v1.0.0

A Minecraft Bedrock Edition external client built with C++20, ImGui (DX11) and MinHook.

## Requirements
- Windows 10/11 x64
- Visual Studio 2022 (MSVC v143, C++20)
- CMake 3.20+
- Git (for FetchContent)

## Build

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Output: `build/Release/glacier.dll`  
Font:   `build/Release/fa-solid-900.ttf`  ← must live next to the DLL.

## Inject
Use any standard DLL injector (e.g. Process Hacker, Xenos) targeting `Minecraft.Windows.exe`.

## Default keybinds
| Key | Action |
|-----|--------|
| `M` | Open / close mod menu |
| `END` | Unload client |
| `C` (hold) | Zoom |

All per-module hotkeys are assignable in the **Client → Menu Keybind** tab.

## Module list (42 modules)

### HUD
FPS Counter · Keystrokes · CPS Counter · Armor HUD · Coordinates · Speed HUD ·
Active Mods List · Clock · Ping Display · Reach Display · Click Stats ·
Inventory Viewer · Session Info

### Combat
Combo Counter · Target HUD · Kill Counter · Hit Color · Criticals

### Movement
Safe Walk · Auto Sprint · No Fall · Velocity · High Jump · Step Height · Scaffold

### Visual
Full Bright · Anti Blind · No Hurt Cam · Tracers · ESP Boxes · Item ESP ·
Block ESP · Free Cam · Chunk Borders · Name Tags · Crosshair · Health Indicator

### Utility
Auto Respawn · Anti AFK · Auto Tool · Zoom · Timer · Water Breath ·
Notification System · Packet Logger

## SDK notes
Offsets are for Bedrock v26.x. After a game update, re-scan with IDA/Ghidra and update
`src/sdk/ClientInstance.h`. The sig-scan patterns in `Client.cpp → resolveSDKPointers()`
will automatically re-resolve the pointer addresses at runtime.

## Architecture
```
dllmain.cpp         — DLL entry, spins up Client on a new thread
Client.cpp          — init: sig-scan → ModuleManager → SwapChainHook
hooks/
  SwapChainHook     — IDXGISwapChain::Present vtable hook; drives the render loop
  HookManager       — MinHook wrapper
render/
  Renderer          — ImGui DX11 init, beginFrame/endFrame
  ModMenu           — full mod menu UI (sidebar, module cards, settings panel)
modules/
  ModuleBase        — base class with toggle, hotkey poll, settings
  ModuleManager     — owns all modules; dispatches tick/render
  impl/             — one .h/.cpp pair per module
sdk/
  ClientInstance.h  — all SDK structs, helpers, tracker singletons
utils/
  SigScanner        — .text section pattern scanner + RIP resolver
  Logger            — thread-safe timestamped file + DebugView logger
  ClientConfig      — runtime config (menu key, DLL directory)
```
