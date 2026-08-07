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

> **Status:** bugs 1–5 all have fixes committed. Phase 6 and 7 were injected
> once and **the game crashed after loading a world**; see § "The world-load
> crash" below for the suspect and what it would take to confirm. Nothing else
> in phases 6–7 has been observed working or failing yet — the crash happened
> before any of it could be checked. See § "Verifying phases 6 and 7".

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

**Implemented — the first approach below.** See `src/sdk/ItemRendering.{h,cpp}`.
The second was not attempted; the notes stay because they are still the
fallback if the first turns out not to work in-game.

Two viable approaches:
- **Hook the game's own item renderer** and let it draw into the game's UI
  pass. Flarial: `src/Client/Hook/Hooks/Render/ItemRendererRenderGroupHook.*`
  and `TextureGroup_getTextureHook.*`.
- **Resolve `TextureGroup::getTexture`**, obtain the item atlas SRV, and sample
  it in our own composite shader with per-item UVs. More work, but keeps
  everything inside our renderer.

What was built, and the two things about it that surprise people:

- Icons are drawn by the **game**, from inside a `ScreenView::setupAndRender`
  hook. HUD modules can't draw them directly — they call
  `ItemRendering::submit()` during the overlay pass with a pixel rect and a
  *description* of the slot (never an `ItemStack*`, which would be a pointer
  captured across passes). The hook resolves the pointer fresh and draws.
- Therefore icons **lag one frame** and render **under** the overlay. Armor HUD
  now outlines its slots rather than filling them, so the fill doesn't wash out
  the icon underneath.

The one genuine guess in the path is `kPixelsPerSizeUnit` in
`ItemRendering.cpp`. Upstream's own ArmorHUD is the only place that pins its
size modifier to real pixels (it lays out on a 48px grid at a modifier of 1),
so that is what the conversion is calibrated against. If icons come out
uniformly too big or too small, change that constant and nothing else — every
other number in the path is imported, not inferred.

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
config persistence, version targeting + sync tooling). Phases 6 and 7 are
written and compiling but **unverified in-game** — see the verification table
at the end of this file.

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

## Pausing the game: release is not a one-shot

`releaseCursor()` called once, when the menu opens, **does not hold** — the game
re-grabs the cursor on its own, and the player keeps moving behind the menu.
This is not a threading problem and no amount of choosing a better thread fixes
it.

Latite's `ScreenManager::onUpdate` is the shape that works, and
`GameSDK::applyCursorState` mirrors it:

```cpp
if (menuOpen) { if (cursorGrabbed()) releaseCursor(); }   // every frame
else if (menuWasOpen) { grabCursor(); }                   // once, on close
```

Two rules that matter:

- **Release repeatedly, grab once.** The release has to be re-asserted every
  frame; the grab is edge-triggered on menu close.
- **Never grab just because the cursor is released.** The game releases it for
  its own screens — pause, inventory, chat. A reconciler that "fixes" that
  would fight the game for control of its own UI.

The gate is `MinecraftGame::isCursorGrabbed()`, a bool at `+0x1D8`.

## The wrong-window bug (fixed — read this before debugging input)

For a while, **Glacier was hooking its own debug console instead of the game.**
`findGameWindow()` returned the first top-level visible window owned by the
process, and `Logger::attachConsole()` runs before it — so a freshly allocated
console won the enumeration.

One wrong `HWND` produced four unrelated-looking symptoms:

- the window title changed on the console, not the game;
- the menu did not respond to the mouse at all (WndProc on the wrong window);
- hovering highlighted *a different widget than the one under the cursor*,
  because `ScreenToClient` converted against the console's client origin;
- CPS Counter never counted.

The fix is to stop guessing: `DXGI_SWAP_CHAIN_DESC::OutputWindow` from the first
real frame is authoritative, and the WndProc and branding re-attach to it. The
pre-frame guess also skips console window classes now.

**The lesson worth keeping:** several symptoms that look like separate features
being broken can be one shared input assumption. Before debugging a UI
interaction bug here, confirm which window is hooked — the log line `game window
corrected to 0x…` tells you it happened.

## Diagnosing a crash

`CrashHandler` (installed first thing at attach) logs any access violation as:

```
[Glacier][error] *** access violation at Minecraft.Windows.exe+0x1A2B3C4 — Glacier was: reading the ClientInstance map
[Glacier][error]     reading address 0x28 (near-null: a missing null check)
```

Three things to read off it:

- **`Glacier was:`** — the innermost `GLACIER_ACTIVITY` scope on that thread.
  Add one to any new path that calls into game code; it costs a thread-local
  pointer store.
- **module+offset**, not a bare address — ASLR makes absolute addresses useless
  between runs, and the offset is what you can look up.
- **near-null vs wild** — a near-null operand is a missing null check in
  Glacier's own code. A wild one means a wrong offset or a signature resolving
  to the wrong function, which is a different investigation entirely.

Some reported exceptions are first-chance and handled normally downstream; the
last line before the log ends is the informative one. Reporting stops after 5.

## The world-load crash

**Symptom:** injected fine, then the game crashed after loading a world.

**Suspect: the `ScreenView::setupAndRender` hook.** It was the only thing
phase 7 added that runs unconditionally inside a world — `readStack`'s new
virtual call needs Armor HUD enabled, and the cursor calls only fire on a menu
toggle. It is also the least verifiable thing in the tree: the pattern matches
a **call site**, and the function address is decoded from the displacement at
+1. If that decode lands on anything other than the intended function, calling
through it with a two-argument signature corrupts the stack the first time the
UI renders — which is exactly when a world finishes loading.

**This is a hypothesis, not a confirmed diagnosis.** Nobody has read a crash
address. What has changed is the blast radius:

- Item icons are **off by default** (`itemIcons` under `[Glacier]`), and the
  hook is not installed at all unless enabled. If the crash is gone with the
  default config, the suspect is confirmed.
- The two calls into game code are wrapped in structured-exception guards, so
  a wrong address now disables the feature and logs which call faulted instead
  of ending the session.

**To confirm or refute, in order:**

1. Run with defaults. Still crashes → it is **not** item rendering; the next
   suspects are the `Platform_GameCore` deref move (`GameSDK::resolve` now
   trusts the table's own deref instead of calling `offsetFromSig` itself) and
   the new per-frame `inGame()` walk in the Present hook.
2. No crash → set `itemIcons = true` and reload. If it now logs `item icons
   disabled: '…' faulted`, the guard caught it and the named call is wrong.
3. If it crashes *without* logging, the fault is somewhere the guards don't
   cover — most likely the `BaseActorRenderContext` buffer being too small
   (`0x500` is upstream's own admitted over-estimate, not a measured size).

## Verifying phases 6 and 7

What changed, and what confirms each piece. All of this is unverified: CI only
proves it compiles.

| Bug | Change | Confirms it works |
|---|---|---|
| 1 | `ui::Input::pollMouse` samples `GetCursorPos` + `GetAsyncKeyState` once per frame from the Present hook; WndProc no longer produces mouse position or button edges (it keeps only the wheel). | Open the menu and move the mouse: a sidebar category should highlight under the cursor, and clicking one should switch the module list. |
| 2 | No separate fix — the HUD editor reads the same `Input`. | With the menu open, drag the Coordinates widget; it should follow the cursor and stay where dropped after a close/reopen. |
| 3 | `GameSDK::setCursorGrabbed` calls `ClientInstance::grabCursor` / `releaseCursor`; `Glacier::setCursorReleased` drives it and only falls back to `ShowCursor` when the call is unavailable. | With the menu open you should not be able to move or look. On attach, no `grabCursor/releaseCursor not resolved` warning in the console. |
| 4 | Fullbright's gamma setting is now `15.0` default over a `0..25` range. | Enabling Fullbright at night, or in a cave, visibly brightens the world. |

| 5 | `ItemRendering` hooks `ScreenView::setupAndRender` and replays icon requests inside the game's UI pass. Armor HUD submits one per slot. **Off by default** — see the crash section above. | With `itemIcons = true`: console shows `item rendering ready` at attach, and equipped armor shows real icons. |
| — | HUD and menu render only when a `LocalPlayer` exists, matching Latite/Flarial. | Nothing of Glacier's is drawn on the main menu; `G`/`M` does nothing there. Both come back on entering a world. |
| — | The game window is retitled to name the client and the **detected** build. | Title bar reads `Glacier Client for Minecraft: Bedrock Edition 1.26.40`. If the number differs from 1.26.40, that mismatch is the explanation for anything else behaving oddly. Unloading with `END` restores the original title. |
| — | `readStack` now fills `maxDurability` via `Item::getMaxDamage` and reads damage through `ItemStackBase::getDamageValue`. | Durability bars appear under damaged armor and shrink as it wears. They have **never** drawn before this, so "no bars" is a failure, not the status quo. |

Order of diagnosis if icons don't appear, cheapest first:

1. **`item rendering unavailable` in the log** → a signature didn't resolve; the
   message names which. Nothing else to investigate.
2. **`item rendering ready`, but nothing renders** → the hook is installed and
   the draw is running. Most likely the argument order or the trailing constant
   in `renderGuiItemNew`, or `BaseActorRenderContext::itemRenderer` reading a
   junk pointer.
3. **Icons appear in the wrong place or the wrong size** → the GUI-unit
   conversion. Wrong *size* is `kPixelsPerSizeUnit`; wrong *position* is
   `GuiData::guiScaleFrac` reading the wrong field.
4. **Icons appear then vanish behind the HUD** → the overlay is painting over
   them; check that Armor HUD's slot fill is still suppressed.

If the menu still doesn't respond to clicks, the next thing to check is whether
`D3DHook::window()` is the window the cursor is actually over —
`ScreenToClient` against the wrong HWND yields plausible-looking but wrong
coordinates, which is indistinguishable from "hit-testing is broken".
