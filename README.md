<div align="center">

# ❄️ Glacier DLL

**Open-source internal client for Minecraft: Bedrock Edition**
Modern C++20 · DirectX 11 overlay · HTML/CSS/JS interface via Ultralight

</div>

> ⚠️ **Educational / research project.** Glacier is provided for learning about
> Windows internals, hooking, and embedded UI rendering. Using third-party
> clients may violate the game's Terms of Service — run it only on accounts and
> servers where you have permission to do so.

---

## Architecture

Glacier is structured as a set of single-responsibility singletons wired
together by a top-level `Glacier` client object. Startup ordering is strict
(SDK → modules → hooks → UI) and teardown reverses it exactly, so the library
can hot-unload cleanly without leaving trampolines in game memory.

```
Minecraft.Windows.exe
        │  (DLL injected)
        ▼
  dllmain.cpp ──► CreateThread ──► Glacier::start()
        │
        ├─ sdk::GameSDK        resolve game pointers from byte signatures
        ├─ ModuleManager       register + own every feature ("module")
        ├─ HookManager         MinHook wrapper (tracked install/remove)
        ├─ D3DHook             IDXGISwapChain::Present / ResizeBuffers
        │       └─ each Present ─► ModuleManager::renderAll()
        │                      ─► UIRenderer::render()
        ├─ UIRenderer          Ultralight → CPU bitmap → D3D11 texture → quad
        │       └─ JsBridge    installs window.glacier.* native functions
        └─ WndProc hook        input → overlay + module keybinds
```

### Why these choices

| Concern        | Decision | Rationale |
|----------------|----------|-----------|
| Address resolution | **Signature scanning** (`memory::findSignature`) | Survives game updates — only signatures move, not architecture. |
| Hooking        | **MinHook** via `HookManager` | Battle-tested; the manager tracks every hook so unload is one pass. |
| Render hook    | **DXGI Present** (vtable discovered from a throwaway device) | Bedrock renders through D3D11; no hardcoded offsets. |
| UI             | **Ultralight** (HTML/CSS/JS → owned pixel buffer) | No child HWND or extra process, unlike WebView2 — ideal for an internal overlay. |
| C++ ⇄ JS       | **JsBridge** installs a `glacier` global on `OnDOMReady` | C++ stays the source of truth; JS only reflects + sends actions. |

## Project layout

```
GlacierDll/
├── src/
│   ├── dllmain.cpp              # entry — spawns init thread off the loader lock
│   ├── Glacier.{h,cpp}          # lifecycle + WndProc input hook
│   ├── util/Logger.h            # thread-safe console + OutputDebugString logger
│   ├── memory/Memory.{h,cpp}    # IDA-style signature scanner + RIP resolver
│   ├── hook/
│   │   ├── HookManager.h        # MinHook wrapper (tracked hooks)
│   │   └── D3DHook.{h,cpp}      # DXGI Present / ResizeBuffers hook
│   ├── sdk/GameSDK.{h,cpp}      # minimal Bedrock SDK resolved from signatures
│   ├── module/
│   │   ├── Setting.h            # typed, UI-serializable setting
│   │   ├── Module.h             # feature base class
│   │   ├── ModuleManager.{h,cpp}
│   │   └── modules/             # Fullbright, AutoSprint, CpsCounter, …
│   └── ui/
│       ├── UIRenderer.{h,cpp}   # Ultralight ↔ D3D11 compositor + input
│       └── JsBridge.{h,cpp}     # native ⇄ JavaScript bridge
├── web/                         # the interface (shipped under ./assets)
│   ├── index.html               # sidebar nav, module cards, sliders
│   ├── style.css                # dark themed design system + click-GUI layout
│   └── main.js                  # renders state, sends toggles back to C++
├── .github/workflows/build.yml  # CI: MSBuild + vcpkg + Ultralight → Glacier.dll
├── Glacier.sln / Glacier.vcxproj
└── vcpkg.json                   # MinHook
```

## The C++ ⇄ JS bridge

The web layer never holds its own truth. On every DOM load `JsBridge` installs:

```js
window.glacier.getState()              // -> JSON snapshot of all modules
window.glacier.toggleModule(name)      // -> bool (new enabled state)
window.glacier.setSetting(mod, id, v)  // slider / checkbox change
window.glacier.setKeybind(mod, vk)
window.glacier.closeMenu()
```

C++ pushes updates the other way by calling `window.glacier.onState(json)` after
any change (`JsBridge::pushState`). `web/main.js` ships a **mock backend** that
mirrors these semantics, so `index.html` is fully interactive in a plain browser
for design work — no game required.

## Building

### Locally (Visual Studio 2022)

1. Install the **Desktop development with C++** workload.
2. `vcpkg install` (manifest mode picks up `vcpkg.json` → MinHook).
3. Download the [Ultralight SDK](https://ultralig.ht) (win-x64) into
   `third_party/ultralight/` (`include/`, `lib/`, `bin/`).
4. Open `Glacier.sln`, select **Release / x64**, build.

Output: `build/x64/Release/Glacier.dll`. Ship it next to the Ultralight runtime
DLLs and an `assets/` folder containing `web/` + a FontAwesome kit.

### CI

`.github/workflows/build.yml` runs on every push/PR to `main`/`master`: it sets
up MSBuild, restores MinHook via vcpkg, fetches the Ultralight SDK, compiles
`Release|x64`, and uploads `Glacier.dll` (plus runtime + UI assets) as a build
artifact. Pushing a `v*` tag additionally publishes a GitHub Release.

## Adding a module

```cpp
// src/module/modules/MyModule.h
class MyModule final : public Module {
public:
    MyModule() : Module("MyModule", "What it does", Category::Combat, 'K') {
        addSetting(Setting{ "speed", "Speed", 1.0f, 0.0f, 5.0f, 0.1f });
    }
    void onEnable()  override { /* ... */ }
    void onTick()    override { /* per game tick */ }
};
```

Register it in `ModuleManager::initialize()` — it appears in the UI
automatically (the bridge reflects every module + setting generically).

## Acknowledgements

Glacier is an independent, from-scratch implementation, but its low-level
design draws directly on two open-source Bedrock clients. Concretely:

**From [Flarial](https://github.com/flarialmc/dll-oss) (dll-oss):**
- The **central signature/offset registry** pattern — one `Mgr`-style table,
  resolved in a single scan ([SignatureManager](src/memory/SignatureManager.h)).
  The seeded **1.21.13x signatures and offsets are sourced from Flarial's**
  `SigInit`/`OffsetInit` (e.g. `Options::getGamma`, `ItemStack::getMaxDamage`,
  `ClientInstance::update`, `ClientInstance::minecraftGame` @ `0x1A0`).
- The **memory helpers** — `offsetFromSig` (RIP resolver), `memberAt`
  (`direct_access`), `callVirtualI`, `followChain`, and the `ScopedProtect`
  RAII guard ([Memory.h](src/memory/Memory.h)).
- Resolving **`getLocalPlayer`'s vtable index from a signature** and invoking it
  via virtual dispatch, and capturing **`ClientInstance` live from a hook**
  rather than a global ([GameSDK.cpp](src/sdk/GameSDK.cpp)).
- The tracked `Hook`/`HookManager` model and kiero-style DXGI `Present` hooking.

**From [Latite](https://github.com/LatiteClient/Latite):**
- The **`Feature` → `Module` → `HUDModule`** hierarchy, where the HUD base bakes
  in position + scale settings so widgets only describe their content
  ([HudModule.h](src/module/HudModule.h)).
- The templated `Manager<T>` and the typed setting model behind
  [Setting.h](src/module/Setting.h).

Fullbright is implemented the canonical way — a hook on `Options::getGamma` that
returns an override — rather than poking a field, so it needs no fragile gamma
offset. Durability bars are real, populated by the resolved `ItemStack::
getMaxDamage`.

## License

MIT — see `LICENSE`.
