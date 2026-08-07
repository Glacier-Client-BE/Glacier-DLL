# Glacier DLL — session handoff

Paste the "Prompt" section below into a fresh Claude Code session. Everything
after it is reference material that session can read from the repo.

---

## Prompt

> I'm working on Glacier DLL (`C:\Users\User\GlacierDll`), an open-source
> internal client for Minecraft: Bedrock Edition 1.26.40, C++20 / MSBuild /
> vcpkg, AGPLv3. Read `docs/HANDOFF.md` first — it has full context on the
> architecture, what works, what's broken, and the diagnosis for each open bug.
>
> Build/verify **only** through GitHub Actions (`gh run watch`), never locally.
> Commit directly to `master` and push; CI runs on every push to master.
>
> The client currently injects, resolves all signatures, and renders its overlay
> correctly. Work through the open bugs in `docs/HANDOFF.md` § "Open bugs", in
> the order given — bug 1 blocks testing everything else.
>
> Reference clients (all copyleft-compatible, we're AGPLv3): Latite (GPLv3),
> Flarial dll-oss (AGPLv3), Selaura (GPLv3), LeviLamina (LGPL-3.0, BDS-only —
> names and struct shapes, never offsets). Imported signature data lives only in
> `src/memory/Signatures.cpp`, which is generated — see § "Signature sync".

---

## What this is

Injected DLL. `dllmain` spawns a thread → `Glacier::start()` resolves
signatures, builds modules, installs hooks, then runs a logic loop.

```
Minecraft.Windows.exe
  └─ dllmain.cpp → CreateThread → Glacier::start()
       ├─ memory/      pattern scanner + name-keyed signature registry
       ├─ sdk/GameSDK  resolves game pointers by name
       ├─ module/      self-registering modules (GLACIER_MODULE macro)
       ├─ core/        EventBus, Config
       ├─ hook/        MinHook wrapper + D3D11 Present/ResizeBuffers hook
       └─ ui/          Direct2D overlay, immediate-mode menu, HUD editor
```

**Verified working in-game (1.26.40.5):** injection, version detection, all 5
signatures resolving, all 4 SDK hooks, D3D hook on the real swapchain, config
load/save, overlay compositing, **Coordinates**, **Day Counter**.

**Controls:** `G` or `M` opens the menu, `END` unloads. Config at
`%APPDATA%\Glacier\config.ini`.

## Architecture facts that are easy to get wrong

These each cost a debugging cycle already. Don't re-derive them.

1. **Bedrock reads keyboard/mouse through RawInput.** `WM_KEYDOWN` and the
   mouse messages are *not* reliably delivered to our WndProc. The menu toggle
   therefore polls `GetAsyncKeyState` on the logic thread. **This is the root
   cause of open bugs 1 and 2 as well.**

2. **Never toggle the menu from two places.** It was previously toggled from
   both the WndProc and the polling loop, with a shared "was down" flag.
   `GetAsyncKeyState` sees the key before `WM_KEYDOWN` arrives, so one press
   toggled twice and the menu appeared never to open. The polling loop is the
   single owner; WndProc only swallows the key.

3. **The overlay device must be on the game's adapter.** Keyed-mutex shared
   textures cannot cross adapters. This machine runs Bedrock on *Intel UHD
   Graphics*; creating our device on the default adapter silently broke all
   compositing. `Renderer::initialize(gameDevice)` is called lazily from
   `beginFrame` for exactly this reason.

4. **`composite()` must bind the back buffer explicitly.** At Present time the
   game has already unbound its render target. Bind with a **null DSV** so the
   fullscreen triangle isn't depth-tested away. This was the "renders nothing
   but logs success" bug.

5. **Every early return in the frame path must leave the keyed mutex at
   `kKeyWrite`.** Bailing after `endFrame` released it to `kKeyRead` deadlocks
   every later frame.

6. **D2D can bind an R8G8B8A8 back buffer directly.** Flarial does. Our
   private-device compositing was justified by a belief that it couldn't — that
   was over-cautious. Compositing works and stays, but if it ever becomes a
   problem, drawing straight onto the back buffer is a legitimate simplification.

## Open bugs

> **Phase 6 status:** bugs 1–4 have fixes committed but **not verified in-game**.
> They compile; nobody has injected them yet. See § "Verifying Phase 6" at the
> end of this file for exactly what to check. Bug 5 is untouched.

### 1. Menu can't be interacted with — BLOCKS EVERYTHING ELSE

The menu renders but does not respond to the mouse.

**Diagnosis:** almost certainly the same RawInput problem as the keyboard.
`ui::Input` is fed exclusively from WndProc mouse messages
(`WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, …) in `Glacier::wndProc`. If Bedrock
consumes mouse input via RawInput, those messages never arrive, so the menu
sees the cursor parked at (0,0) — no hover, no clicks, no drags.

**Fix:** poll instead. Once per frame (in the Present callback, before
`Menu::render`), when the menu is open:

```cpp
POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p);
Input::get().onMouseMove((float)p.x, (float)p.y);
// edge-detect these rather than trusting WM_*BUTTONDOWN:
const bool lDown = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
```

Keep the existing WndProc path as a supplement (harmless if messages do
arrive) but do not let both produce click *edges* — same double-fire trap as
bug 2 in the history above. Have exactly one place compute edges.

Verify: hovering a category should highlight it.

### 2. HUD modules can't be dragged

Same root cause as bug 1 — `HudEditor::update` hit-tests against
`Input::mouseX/Y`. Fixing bug 1 very likely fixes this. Re-test before
investigating separately.

### 3. Game isn't paused while the menu is open

Currently `wndProc` swallows game input messages. That does nothing when the
game reads RawInput directly, which is why you can still move and look.

**Fix — use the game's own cursor state, like Latite and Flarial do.** Latite
already publishes both signatures in `src/mc/Addresses.h`:

- `ClientInstance::grabCursor`
- `ClientInstance::releaseCursor`

Call `releaseCursor()` on menu open and `grabCursor()` on close. Both are
`void(__fastcall*)(ClientInstance*)`. Add them to the mapping table in
`tools/sync_signatures.py` (`SIGNATURES` list) and regenerate — do **not**
hand-edit `Signatures.cpp`.

This also fixes the cursor: the current `ShowCursor`/`ClipCursor` juggling in
`Glacier::setCursorReleased` is a workaround that should be removed once the
game's own grab state is driven properly.

### 4. Fullbright does nothing

The hook is installed and fires — the bug is the value.

**Diagnosis:** our gamma setting is a 0.0–1.0 slider defaulting to **1.0**,
which is the game's *normal* brightness. Latite's Fullbright uses a slider
range of **0–25**:

```cpp
addSliderSetting("gamma", ..., FloatValue(0.f), FloatValue(25.f), FloatValue(1.f));
```

**Fix:** in `src/module/modules/Fullbright.cpp`, change the setting to
`Setting{ "gamma", "Gamma", 15.0f, 0.0f, 25.0f, 0.5f }`. Note
`GameSDK::setGammaOverride` treats negative as "disabled", so the 0..25 range
is fine as-is.

### 5. No item renderers (Armor HUD shows no icons)

Armor HUD draws durability bars and counts but no item textures, because the
game's texture atlas isn't reachable from the overlay's private D3D device.

This is a **feature, not a bug fix** — see Phase 7 below. It is the largest
remaining piece of work. Do not attempt it before bugs 1–4 are closed.

Two viable approaches:
- **Hook the game's own item renderer** and let it draw into the game's UI
  pass. Flarial: `src/Client/Hook/Hooks/Render/ItemRendererRenderGroupHook.*`
  and `TextureGroup_getTextureHook.*`.
- **Resolve `TextureGroup::getTexture`**, obtain the item atlas SRV, and sample
  it in our own composite shader with per-item UVs. More work, but keeps
  everything inside our renderer.

The user's own resource pack does this with JSON UI entity renderers
(`C:\Users\User\Desktop\Glacier v7\packs\Glacier Client v7 [Main]\ui\glacier\
screens\hud_screen\hud_modules\entity_modules\glacier_armorhud.json`, using
`gc.lpr`). That is a *pack-side* technique and is **not** the route for the
DLL — noted only so it isn't confused for one.

## Signature sync — read before touching Signatures.cpp

`src/memory/Signatures.cpp` is **generated**. Never hand-edit it.

```bash
python tools/sync_signatures.py            # regenerate from Latite master
python tools/sync_signatures.py --check    # exit 1 if stale
```

The mapping is explicit: every imported value is named on both sides in
`tools/sync_signatures.py`. If Latite renames something, the script refuses to
write and names what it couldn't find — a silent wrong value is the failure
mode that costs days, so this is deliberate.

`.github/workflows/sync-signatures.yml` runs daily and opens a PR on change.

**Licensing:** that one file is why Glacier is AGPLv3. Keep all imported data
in it — never inline a pattern or struct offset elsewhere. See
`docs/acknowledgements.md`.

## Roadmap — remaining phases

Phases 0–5 are done (scaffold, hooks/SDK, EventBus + menu, module catalog,
config persistence, version targeting + sync tooling).

| Phase | Scope |
|---|---|
| **6** | **Interaction & pause** — bugs 1–3. Poll-based mouse input, real game pause via `grabCursor`/`releaseCursor`, remove the `ShowCursor` workaround. |
| **7** | **Item rendering** — texture atlas access so Armor HUD, Inventory HUD, and hotbar widgets can draw real items. Largest remaining piece. |
| **8** | **Module catalog expansion** — the pack ships ~30 widgets worth porting: Armor Bar, Bow Indicator, Combo Counter, Death Coords, Direction HUD, Kill Counter, Offhand HUD, Speedometer, Status HUD, Target HUD, Chunk Map, EXP Calculator, Inventory HUD, Low Durability warning, Player List, Server Display, Timer HUD, Walk Distance. Most need only SDK accessors that don't exist yet. |
| **9** | **Theming & polish** — menu animation, blur, per-module colour presets, keybind-conflict UX. |
| **10** | **Packaging** — version-string exports for a launcher, tag-triggered release zip (`build.yml` already has the release job). |

Scope boundary (enforced since Phase 0): **visual / HUD / QoL only**. No
KillAura, reach, fly, aimbot, or server-hitbox manipulation.

## References

All four reference clients are copyleft-compatible with our AGPLv3 (GPLv3
material may be incorporated under GPLv3 §13; LGPL-3.0 likewise). Attribution
belongs in `docs/acknowledgements.md`, and any imported *data* belongs only in
`src/memory/Signatures.cpp`.

### Reference clients

| Project | License | Use it for |
|---|---|---|
| **[Latite](https://github.com/LatiteClient/Latite)** | GPLv3 | **Primary signature source.** Actively maintained, supports 1.26.4x. Our sync tool reads it directly. |
| **[Flarial (dll-oss)](https://github.com/flarialmc/dll-oss)** | AGPLv3 | Rendering internals — swapchain hooking, item rendering, ECS components, DX12. |
| **[Selaura](https://github.com/selauraclient/selaura)** | GPLv3 | Small, clean framework. Good for scanning technique; no module catalog. |
| **[LeviLamina](https://github.com/liteldev/levilamina)** | LGPL-3.0 | Named class/struct/method headers. **BDS-only** — client types (`ClientInstance`, `Options`, `LevelRenderer`) don't exist there, and shared-type offsets can still differ. Names and shapes, never offsets. |

### Specific files that mattered

Direct links, because finding these again costs a session:

**Latite**
- [`src/mc/Addresses.h`](https://github.com/LatiteClient/Latite/blob/master/src/mc/Addresses.h) — every AOB pattern. What `tools/sync_signatures.py` parses. Contains `ClientInstance::grabCursor` / `releaseCursor` needed for bug 3.
- [`src/mc/common/client/game/ClientInstance.cpp`](https://github.com/LatiteClient/Latite/blob/master/src/mc/common/client/game/ClientInstance.cpp) — the object-graph chain and `getLocalPlayer` vtable index (`0x1F`).
- [`src/mc/common/client/game/Platform_GameCore.cpp`](https://github.com/LatiteClient/Latite/blob/master/src/mc/common/client/game/Platform_GameCore.cpp) — how the root global is walked.
- [`src/client/feature/module/modules/visual/Fullbright.cpp`](https://github.com/LatiteClient/Latite/blob/master/src/client/feature/module/modules/visual/Fullbright.cpp) — the 0–25 gamma range that fixes bug 4.
- `src/mc/common/world/actor/Actor.h`, `.../player/Player.h`, `.../PlayerInventory.h` — `CLASS_FIELD` offsets the sync tool extracts.

**Flarial**
- [`src/Client/Hook/Hooks/Render/DirectX/DX11/SwapchainHook_DX11.cpp`](https://github.com/flarialmc/dll-oss/blob/HEAD/src/Client/Hook/Hooks/Render/DirectX/DX11/SwapchainHook_DX11.cpp) — the `OMSetRenderTargets(1, &rtv, nullptr)` that fixed our blank overlay. Also shows D2D binding an RGBA back buffer directly.
- `src/Client/Hook/Hooks/Render/ItemRendererRenderGroupHook.*` and `TextureGroup_getTextureHook.*` — **the starting point for Phase 7 (item rendering).**
- `src/SDK/Client/Actor/Components/ActorEquipmentComponent.hpp` — armor container layout.
- `src/SDK/Client/Actor/EntityContext.hpp` — entt traits for the Bedrock registry.

**Selaura**
- [`src/hooks/memory.hpp`](https://github.com/selauraclient/selaura/blob/HEAD/src/hooks/memory.hpp) — [libhat](https://github.com/BasedInc/libhat) SIMD pattern scanning. Worth adopting over our naive byte loop in `memory/Memory.cpp`.
- `src/api/mc/client/*.hpp` — padded struct layouts, useful for cross-checking offsets against a *different* build.

### Libraries

- [MinHook](https://github.com/TsudaKageyu/minhook) — our hooking engine (via vcpkg).
- [libhat](https://github.com/BasedInc/libhat) — candidate scanner upgrade.
- [EnTT](https://github.com/skypjack/entt) — Bedrock's ECS. Needed for component lookups; the registry layout must match the game's build.
- [kiero](https://github.com/Rebzzel/kiero) — the throwaway-device vtable discovery technique our `D3DHook` reimplements.

### Bedrock docs

- [bedrock.dev](https://bedrock.dev/) — protocol and JSON UI docs.
- [wiki.bedrock.dev JSON UI](https://wiki.bedrock.dev/json-ui/json-ui-intro) — relevant only for the *pack*, not the DLL.

### This project

- Repo: <https://github.com/Glacier-Client-BE/Glacier-DLL>
- CI: <https://github.com/Glacier-Client-BE/Glacier-DLL/actions>
- Local pack (widget reference for Phase 8):
  `C:\Users\User\Desktop\Glacier v7\packs\Glacier Client v7 [Main]\ui\glacier\screens\hud_screen\hud_modules\`
  — `entity_modules/` and `jsonui_modules/` list ~30 widgets worth porting.

## Verification reality

CI proves compilation. It cannot prove anything works in-game — that needs the
user to inject on Windows and read the debug console. When you change anything
touching rendering, input, or signatures, say plainly that it's unverified and
tell the user exactly what log line or on-screen behaviour would confirm it.

`docs/signatures.md` tracks per-entry status; entries stay ⚠️ *inherited* until
someone confirms them against a running client. Note that its "Seeded" table has
drifted from `src/memory/Signatures.cpp` (it still lists entries such as
`ClientInstance::update`, `GameMode::attack`, and `Container::begin` that the
current generated table does not contain) — worth a pass.

## Verifying Phase 6

What changed, and what confirms each piece. All of this is unverified: CI only
proves it compiles.

| Bug | Change | Confirms it works |
|---|---|---|
| 1 | `ui::Input::pollMouse` samples `GetCursorPos` + `GetAsyncKeyState` once per frame from the Present hook; WndProc no longer produces mouse position or button edges (it keeps only the wheel). | Open the menu and move the mouse: a sidebar category should highlight under the cursor, and clicking one should switch the module list. |
| 2 | No separate fix — the HUD editor reads the same `Input`. | With the menu open, drag the Coordinates widget; it should follow the cursor and stay where dropped after a close/reopen. |
| 3 | `GameSDK::setCursorGrabbed` calls `ClientInstance::grabCursor` / `releaseCursor`; `Glacier::setCursorReleased` drives it and only falls back to `ShowCursor` when the call is unavailable. | With the menu open you should not be able to move or look. On attach, no `grabCursor/releaseCursor not resolved` warning in the console. |
| 4 | Fullbright's gamma setting is now `15.0` default over a `0..25` range. | Enabling Fullbright at night, or in a cave, visibly brightens the world. |

If the menu still doesn't respond to clicks, the next thing to check is whether
`D3DHook::window()` is the window the cursor is actually over —
`ScreenToClient` against the wrong HWND yields plausible-looking but wrong
coordinates, which is indistinguishable from "hit-testing is broken".
