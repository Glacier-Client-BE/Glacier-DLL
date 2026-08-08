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

3. **The overlay draws straight onto the game's back buffer.** `Renderer` calls
   `D2D1CreateDeviceContext` on the swap chain's DXGI surface, on the game's own
   device — Flarial's approach. There is no second device, no shared texture, no
   keyed mutex and no compositing pass.

   Items 3–6 here used to describe that machinery: an adapter-matched private
   device, a fullscreen-triangle composite needing an explicit null-DSV bind,
   and a keyed-mutex protocol every early return had to preserve. **All of it is
   gone.** It existed to work around a belief that D2D could not bind Bedrock's
   back buffer, which item 6 had already recorded as wrong. Both hangs this
   client suffered came from that machinery.

4. **Nothing may hold a back-buffer reference when the game calls
   `ResizeBuffers`.** This is the single rule that replaced all of the above —
   see the ResizeBuffers section below. It is unconditional: no size comparison,
   no zero check.

5. **`beginFrame` must not clear the target.** The target is the game's own
   frame now. Clearing it erases the game.

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
| **9** | **HUD widget visual pass** — bring each existing widget to a finished, reference-matched look rather than a functional one. Done so far: a shared design-token system (`HudModule.h`) with per-widget padding/corner-radius/shadow settings, and a `TextHudModule` base so text widgets can't drift from each other; the HUD background now defaults **off** everywhere, matching Latite's `HUDModule` (its `renderFrame`, a dark fill + border, is only ever used for the editor's "selected" highlight — never during normal play, confirmed by reading `reference/latite/src/client/feature/module/HUDModule.cpp`); Armor HUD's slot chrome (box + outline + filled durability track) is gone, replaced with a chrome-less look keyed off Latite's `ArmorHUD.cpp` — bare icon, a thin colour-coded durability rail with no track, a shadowed stack-count badge. Remaining: pass the rest of the catalog (Phase 8's new widgets included) through the same "does this box earn its place" test; nothing here has been seen in-game yet, only reasoned from the reference source — verify against a running client before trusting it. |
| **10** | **Theming & polish** — menu animation, blur, per-module colour presets, keybind-conflict UX. |
| **11** | **Packaging** — version-string exports for a launcher, tag-triggered release zip (`build.yml` already has the release job). |

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

## ResizeBuffers: release the back buffer unconditionally

`IDXGISwapChain::ResizeBuffers` fails with `DXGI_ERROR_INVALID_CALL` if **any**
outstanding reference to a back buffer exists. Glacier holds one
(`m_backBufferRtv`, from compositing), so `Renderer::resize` must drop it before
the game's call runs — with no early-out in front of it.

The two cases that are easy to skip and must not be:

- **`width`/`height` of `0`.** Not a degenerate call to ignore: it is DXGI's
  "keep the current size", and it is the most common form.
- **An unchanged size.** Still called for buffer-count and fullscreen changes.

The failure is silent and remote. `ResizeBuffers` returns an error the game
does not necessarily check, the game continues on a swap chain it believes it
resized, and it dies later inside DXGI with a wild pointer — with Glacier's
crash activity reading `(idle)`, because by then no Glacier code is on the
stack. `D3DHook::hkResizeBuffers` now logs a failed `ResizeBuffers` explicitly
so this can never be silent again.

## Never block the game's threads

Two separate hangs came from Glacier blocking a game thread. Both looked like
crashes, neither raised an exception, and neither was findable by reasoning
about "what changed last".

1. **Window text from inside `Present`.** `SetWindowText`/`GetWindowText` send
   `WM_SETTEXT`/`WM_GETTEXT` *synchronously* to the thread owning the window
   and block until it pumps. The game's main thread cannot pump while it waits
   on the render thread — instant deadlock. Anything touching window state now
   happens on Glacier's own logic thread; the D3D hook only records the handle.
2. **A 1000 ms keyed-mutex timeout.** On the game's render thread, that turns
   any mutex bug into one frame per second.

The rule: **the Present hook and the ScreenView hook run on the game's threads.
Nothing there may block, take a lock the game could hold, or send a window
message.** Record what you need and let the logic loop act on it.

A hang with no exception in the log is almost always this, not a bad pointer.

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

## The world-load crash — root-caused, fix unverified in-game

**Symptom:** injected fine, then the game crashed after loading a world.

**Root cause, found in commit `d997063`:** not a signature, not the context
buffer size — `MinecraftUIRenderContext::clientInstance`/`screenContext` were
seeded at `0x00`/`0x08` from Latite's header declaration order. That class
declares its two pointers first but *also* declares virtual functions, so MSVC
puts the vtable pointer at `0x00` and the real members land at `0x08`/`0x10`.
Glacier was passing a vtable pointer as the `ClientInstance`, deriving a
garbage `MinecraftGame` from it (`vptr + 0x1A0`), and handing both to the
game's render-context constructor — which dereferenced them and killed the
process. The evidence was in the first crash log the whole time: the printed
`cinst` sat in the module image range (`.rdata`, i.e. a vtable) while
`screenContext` was byte-identical to the `ClientInstance` already resolved
through the unrelated global walk — two independent routes disagreeing, just
not being compared. Two earlier builds spent effort widening the context
buffer instead, which was never the problem.

**What changed as a result** (all in `src/sdk/ItemRendering.cpp`):

- The offsets are corrected (Flarial independently lists the same `0x8`/`0x10`).
- `drawPending` now cross-checks the context's `ClientInstance` against the
  object graph's own — two independent routes to the same object — and
  disables itself with a named `LOG_ERROR` on mismatch instead of calling the
  constructor with a wrong pointer.
- `itemIcons` is back to **on by default** (`ItemRendering::m_enabled = true`)
  now that the cause is understood rather than unknown. `docs/HANDOFF.md` used
  to say off-by-default here; that was true before this commit and is stale —
  trust `src/sdk/ItemRendering.h`'s own comment over this file if they ever
  disagree again.
- The two calls that actually enter game code with guessed arguments
  (`BaseActorRenderContext`'s constructor, `ItemRenderer::renderGuiItemNew`)
  stay wrapped in structured-exception guards from the earlier hardening pass,
  so a *still*-wrong offset disables the feature and names which call faulted
  rather than ending the session.

**Still unverified in-game.** The vptr explanation fits the evidence in the
log, and the cross-check plus guards mean a wrong offset should now degrade
rather than crash — but nobody has confirmed a crash-free world load with
`itemIcons = true` (the current default) since this fix landed. If a world
load still crashes:

1. No `item icons disabled: '…' faulted` in the log before the crash → the
   fault is somewhere the guards don't cover, or is unrelated to item
   rendering entirely — next suspects are the `Platform_GameCore` deref move
   (`GameSDK::resolve` trusting the table's own deref) and the per-frame
   `inGame()` walk in the Present hook.
2. Set `itemIcons = false` and reload. Still crashes → it was never item
   rendering. No crash → the vptr fix didn't fully hold; re-check the cross-check
   log line (`item icons disabled: MinecraftUIRenderContext gave ClientInstance
   … but the object graph says …`) for a mismatch that's being hit every frame
   without actually crashing yet.

## Verifying phases 6 and 7

What changed, and what confirms each piece. All of this is unverified: CI only
proves it compiles.

| Bug | Change | Confirms it works |
|---|---|---|
| 1 | `ui::Input::pollMouse` samples `GetCursorPos` + `GetAsyncKeyState` once per frame from the Present hook; WndProc no longer produces mouse position or button edges (it keeps only the wheel). | Open the menu and move the mouse: a sidebar category should highlight under the cursor, and clicking one should switch the module list. |
| 2 | No separate fix — the HUD editor reads the same `Input`. | With the menu open, drag the Coordinates widget; it should follow the cursor and stay where dropped after a close/reopen. |
| 3 | `GameSDK::applyCursorState` calls `ClientInstance::grabCursor` / `releaseCursor`, re-asserting the release every frame the menu is open and grabbing once on close (see "Pausing the game: release is not a one-shot" above); `Glacier::setCursorReleased` drives it and only falls back to `ShowCursor` when the call is unavailable. | With the menu open you should not be able to move or look. On attach, no `grabCursor/releaseCursor not resolved` warning in the console. |
| 4 | Fullbright's gamma setting is now `15.0` default over a `0..25` range. | Enabling Fullbright at night, or in a cave, visibly brightens the world. |

| 5 | `ItemRendering` hooks `ScreenView::setupAndRender` and replays icon requests inside the game's UI pass. Armor HUD submits one per slot. **On by default** as of `d997063` — see "The world-load crash" above; the earlier off-by-default state is stale. | With `itemIcons = true` (the default): console shows `item rendering ready` at attach, and equipped armor shows real icons. |
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
