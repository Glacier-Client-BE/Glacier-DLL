# Glacier Client v2.0.0

A Minecraft Bedrock Edition (MCBE) DLL client — DirectX 11 + Dear ImGui.

---

## What's new in v2.0.0

### Internal Client — No External Injector Required

Glacier Client is now a fully **internal** DLL, launched and injected automatically by the
[Glacier Launcher](https://github.com/Glacier-Client-BE/Glacier-Launcher).
Font Awesome icons are embedded directly into the DLL binary — no external `.ttf` file needed.

### Redesigned Mod Menu (matches screenshot)

The old sidebar-list layout is replaced by a **3-column icon-card grid** with four tabs:

| Tab | Purpose |
|---|---|
| **Modules** | Icon grid with category pills + live search |
| **Elements** | List of HUD modules — toggle + see current position |
| **Editors** | Keybind reference table |
| **Information** | Client info, menu keybind remapper, statistics |

Each card shows:
- Large centered FontAwesome icon (blurple accent when enabled)
- Green indicator dot when active, glow border on top edge
- **ⓘ** button opens a description panel on the right
- **⚙** button opens the settings panel (sliders / toggles)

### ModuleSettings — `defineString` / `getString` added

All modules that need string settings (NickHider, AutoGG, Waypoints) now use the type-safe `defineString` / `getString` API.

---

## Full module list (34)

### HUD (15)
| Module | Notes |
|---|---|
| FPS Counter | EMA-smoothed, configurable sample window |
| Keystrokes | WASD + LMB/RMB with press animations |
| CPS Counter | Per-button bars with configurable max scale |
| Armor HUD | Slot icons + durability bars, drag to reposition |
| Coordinates HUD | XYZ / biome / dimension |
| Speed HUD | m/s speedometer |
| Active Mods List | On-screen enabled-module list |
| **Clock & Compass** | Combined real clock + animated compass — replaces old Clock |
| Ping Display | Latency in ms |
| Reach Display | Last recorded attack reach |
| Click Stats Dashboard | Session click analytics |
| Inventory Viewer | Hotbar slot contents |
| Session Info | Play-time + K/D |
| **Potion HUD** *(new — from Onix)* | Active effects + duration |
| **Chunk Map** *(new — from Onix)* | Chunk coordinate grid widget |

### Combat (4)
| Module | Notes |
|---|---|
| Combo Counter | Hit streak with fade-out |
| Target HUD | Animated health bar + distance + armor |
| Kill Counter | Session tracker |
| Hit Color | Custom entity hurt-flash tint |

### Movement (3)
| Module | Notes |
|---|---|
| Toggle Sprint | Keeps sprint while pressing W |
| Safe Walk | Edge-detection auto-sneak |
| **Toggle Sneak** *(new — from Onix)* | Persist sneak without holding key |

### Visual (10)
| Module | Notes |
|---|---|
| FullBright | Max gamma override |
| HurtCam Disable | Remove screen shake on damage |
| FreeCam | Detached spectator camera |
| Chunk Borders | 16×16 chunk edge lines |
| Crosshair | Fully custom crosshair renderer |
| Health Indicator | Color-coded health bar |
| **Block Outline** *(new — from Onix)* | Custom outline/overlay color + width |
| **Hitboxes** *(new — from Onix)* | Entity AABB wireframe ESP |
| **Free Look** *(new — from Onix)* | Hold-key orbit camera, smooth return |
| **Environment Changer** *(new — from Onix)* | Gamma/time/rain override |

### Utility (7)
| Module | Notes |
|---|---|
| Auto Respawn | Instant respawn on death |
| Zoom | FOV zoom on keyhold |
| Timer | Tick-speed multiplier |
| Notification System | On-screen toast messages |
| Packet Logger | Debug packet inspector |
| **Auto GG** *(new — from Onix)* | Auto-send GG/GL with configurable messages |
| **Nick Hider** *(new — from Onix)* | Override displayed username |
| **Waypoints** *(new — from Onix)* | Named world markers + edge arrows |

---

## Usage

1. Download the [Glacier Launcher](https://github.com/Glacier-Client-BE/Glacier-Launcher)
2. Press **Launch** — the launcher injects `glacier.dll` automatically
3. Press **M** in-game to open the mod menu

---

## Build

Requires **Visual Studio 2022**, **CMake ≥ 3.20**, Windows SDK, x64.

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Output → `build/Release/glacier.dll`

> Font Awesome 5 is automatically downloaded and **embedded** into the DLL at build time.
> No external `.ttf` file is needed at runtime.

To set a custom version (CI):
```bat
cmake -B build -A x64 -DGLACIER_VERSION=2.1.0
```

---

## Architecture

```
src/
├── dllmain.cpp              DLL entry point
├── Client.cpp/h             Init + main loop
├── sdk/ClientInstance.h     MCBE SDK — RE'd offsets for v26.x
├── hooks/
│   ├── HookManager          MinHook wrapper
│   └── SwapChainHook        IDXGISwapChain::Present → ImGui frame
├── modules/
│   ├── ModuleBase.h         Base class (name, icon, category, key, settings)
│   ├── ModuleSettings       Typed setting store: bool/int/float/string
│   ├── ModuleManager        Register + dispatch tick/render
│   └── impl/                34 module implementations
├── render/
│   ├── ModMenu.cpp/h        Grid UI — Modules / Elements / Editors / Info
│   └── Renderer.cpp/h       ImGui + DX11 atlas init (FA font embedded)
├── icons/
│   └── IconManager.cpp/h    WIC PNG loader + procedural fallback
└── utils/
    ├── ClientConfig.h       Persistent JSON config
    ├── Logger.cpp/h         fmt-style logger
    └── SigScanner.cpp/h     AOB pattern scanner
```

---

## CI / Releases

The GitHub Actions workflow (`.github/workflows/release.yml`) automatically:
- Determines the next version from commit message prefix (`hotfix:` / `update:`)
- Builds the DLL with that version baked in
- Creates a git tag + GitHub Release with auto-generated release notes
- Triggers the Glacier Website repo to update

---

## Credits
- Dear ImGui — ocornut
- MinHook — TsudaKageyu
- Font Awesome 5 — fontawesome.com
- Module concepts from Onix Client V2 (open-source)
- Reference architecture inspired by [Flarial](https://github.com/flarialmc/dll-oss) & [Latite](https://github.com/LatiteClient/Latite)
