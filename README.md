<div align="center">

# ❄️ Glacier DLL

**Open-source internal client for Minecraft: Bedrock Edition**
C++20 · DirectX 11 overlay · native Direct2D/DirectWrite menu

</div>

> ⚠️ **Educational / research project.** Glacier is provided for learning about
> Windows internals, hooking, and in-process overlay rendering. Using
> third-party clients may violate the game's Terms of Service — run it only on
> accounts and servers where you have permission to do so.

## Scope

Glacier ships **visual, HUD, and quality-of-life modules only** — Fullbright,
HUD widgets (armor, coordinates, FPS/CPS counters, …), cosmetic tweaks,
client-side visual overlays (e.g. entity hitbox outlines), and small
non-memory-hooked utilities (e.g. an OS-level "null movement" key remapper).

Glacier deliberately does **not** ship exploit-style modules: no KillAura, no
reach hack, no fly, no aimbot, no server-hitbox manipulation. This keeps
Glacier a legitimate client rather than a competitive-cheat tool, and every
module addition is expected to respect this boundary.

## Status

Rebuild in progress.

| Phase | Scope | State |
|---|---|---|
| 0 | Project scaffold + CI | ✅ done |
| 1 | Signature scanning, hooks, minimal SDK, Fullbright | ✅ compiles |
| 2 | EventBus, HudModule, native Direct2D menu | ✅ compiles |
| 3 | Visual/HUD/QoL module catalog + the offsets they need | next |
| 4 | Theming, animation, release packaging | — |

**"Compiles" is not "works."** CI proves the client builds; it cannot prove the
imported signatures match your game build, or that the overlay composites
correctly on your GPU. Both need someone to inject the DLL on Windows. See
[docs/signatures.md](docs/signatures.md) for what has actually been confirmed
against a running game — currently nothing.

Controls: **INSERT** opens the menu, **END** unloads the client.

## Architecture

```
Minecraft.Windows.exe
        │  (DLL injected)
        ▼
  dllmain.cpp ──► CreateThread ──► client startup
        │
        ├─ sdk::GameSDK        resolve game pointers from byte signatures
        ├─ ModuleManager       self-registering modules ("features")
        ├─ EventBus            typed pub/sub; hooks publish, modules subscribe
        ├─ HookManager         MinHook wrapper (tracked install/remove)
        ├─ D3DHook             IDXGISwapChain::Present / ResizeBuffers
        │       └─ each Present ─► ui::Renderer::beginFrame()
        │                      ─► ModuleManager::renderAll()   (HUD widgets)
        │                      ─► ui::Menu::render()
        │                      ─► ui::Renderer::endFrame()     (composite)
        └─ WndProc hook        input → ui::Input → menu + module keybinds
```

### How the overlay reaches the screen

```
 Glacier's private D3D11 device          the game's device
 (created with BGRA support)
        │                                        │
        ▼                                        │
   Direct2D / DirectWrite                        │
        │  draws the UI                          │
        ▼                                        │
   shared B8G8R8A8 texture ──[keyed mutex]──► fullscreen triangle
                                                 │  premultiplied alpha
                                                 ▼
                                            game back buffer
```

Glacier does **not** bind Direct2D to the game's back buffer. D2D only accepts
a BGRA surface and requires `D3D11_CREATE_DEVICE_BGRA_SUPPORT` on the device
that created the swap chain — neither is under our control, and Bedrock's back
buffer is commonly RGBA, so that approach can simply refuse to start. Owning a
private device and compositing costs one texture copy per frame and works
regardless of the game's format or device flags.

### Why these choices

| Concern | Decision | Rationale |
|----------------|----------|-----------|
| Address resolution | **Signature scanning** | Survives game updates — only signatures move, not architecture. |
| Hooking | **MinHook** via a tracked `HookManager` | Battle-tested; every install is tracked so unload is one clean pass. |
| Render hook | **DXGI Present** (vtable discovered from a throwaway device) | Bedrock renders through D3D11; no hardcoded offsets. |
| UI | **Native Direct2D + DirectWrite** | No embedded browser engine or extra runtime files — smaller binary, smaller dependency surface, matches the weight class of comparable open-source Bedrock clients. |
| Build | **MSBuild + vcpkg**, built exclusively via GitHub Actions | Small, Windows-only dependency set; CI on `windows-latest` is the only build/verification path for this project. |

## Building

Glacier builds **only via GitHub Actions** (`.github/workflows/build.yml`) —
there is no requirement to have Visual Studio installed locally to verify a
change; push to a branch and check the workflow run. If you do have a local
Windows + Visual Studio 2022 environment:

1. Install the **Desktop development with C++** workload.
2. `vcpkg install` (manifest mode picks up `vcpkg.json` → MinHook).
3. Open `Glacier.sln`, select **Release / x64**, build.

Output: `build/x64/Release/Glacier.dll`.

## Acknowledgements

Glacier's infrastructure — the pattern scanner, signature registry, hook
manager, D3D hook, module system, and UI — is Glacier's own code. Its *design*
is informed by studying two other open-source Bedrock clients:
[Latite](https://github.com/LatiteClient/Latite) (GPLv3) and
[Flarial (dll-oss)](https://github.com/flarialmc/dll-oss) (AGPLv3), from which
Glacier reimplements *techniques* (a central signature/offset registry keyed by
name, a `Module` → `HudModule` hierarchy, kiero-style Present-hook vtable
discovery via a throwaway device) as original code.

**The signature and offset data is different.** The AOB patterns and struct
offsets in [`src/memory/Signatures.cpp`](src/memory/Signatures.cpp) are derived
from the open reverse-engineering work in those two projects. That single file
is why Glacier is AGPLv3 rather than permissively licensed. See
[docs/acknowledgements.md](docs/acknowledgements.md) for the full reasoning and
a per-technique breakdown.

## License

**AGPLv3** — see [LICENSE](LICENSE).

This is strong copyleft: if you distribute a modified Glacier, or run it as a
network service, you must make your corresponding source available under the
same license. Glacier cannot be embedded in a closed-source product.

The license follows from the signature data described above, not from the
infrastructure. If the imported signatures in `src/memory/Signatures.cpp` are
ever replaced with independently derived ones, that is the only file standing
between Glacier and a permissive relicense.
