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

This is a from-scratch rebuild in progress. See the phased roadmap in this
repository's plan history for the current milestone — early phases focus on
the hooking/signature-scanning core and a single proven end-to-end module
(Fullbright) before the wider module catalog is ported back.

## Architecture

```
Minecraft.Windows.exe
        │  (DLL injected)
        ▼
  dllmain.cpp ──► CreateThread ──► client startup
        │
        ├─ sdk::GameSDK        resolve game pointers from byte signatures
        ├─ ModuleManager       self-registering modules ("features")
        ├─ HookManager         MinHook wrapper (tracked install/remove)
        ├─ D3DHook             IDXGISwapChain::Present / ResizeBuffers
        │       └─ each Present ─► ModuleManager::renderAll()
        │                      ─► native menu render (Direct2D/DirectWrite)
        └─ WndProc hook        input → overlay + module keybinds
```

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
