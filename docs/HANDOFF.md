# Glacier DLL — session handoff

Paste the "Prompt" section below into a fresh Claude Code session. Everything
after it is reference material that session can read from the repo.

---

## Prompt

> I'm working on Glacier DLL (`C:\Users\User\GlacierDll`), an open-source
> internal client for Minecraft: Bedrock Edition 1.26.40, C++20 / MSBuild /
> vcpkg, AGPLv3. Read `docs/HANDOFF.md` first — it has full context on the
> architecture, what's verified in-game, what's unverified, and the open
> items at the top.
>
> Build/verify **only** through GitHub Actions (`gh run watch`), never locally.
> Commit directly to `master` and push; CI runs on every push to master.
>
> Work through § "Open items right now", top to bottom. The user tests
> in-game on Windows and pastes back the console log — read it closely, it's
> the only ground truth this project has. The user's own machine runs the
> **D3D11** path; two friends test as well, one on Win10 and one on Win11,
> and at least one of them is on the **D3D12** path — that difference is the
> single most important variable in this project right now.
>
> Reference clients: Latite (GPLv3), Flarial dll-oss (AGPLv3), Selaura
> (GPLv3), LeviLamina (LGPL-3.0, BDS-only) are copyleft-compatible with our
> AGPLv3. Horion (CC BY-NC 4.0), Onix-Client-v2 (proprietary), and
> Solstice-release (unlicensed) are checked out under direct permission from
> their authors — read for technique and cross-checking, never copy source
> verbatim. All seven are under `reference/` — read the real source there
> before guessing at how something works. Imported signature data lives only
> in `src/memory/Signatures.cpp`, which is generated — see § "Signature sync".

---

## What this is

Injected DLL. `dllmain` spawns a thread → `Glacier::start()` resolves
signatures, builds modules, installs hooks, then runs a logic loop.

```
Minecraft.Windows.exe
  └─ dllmain.cpp → CreateThread → Glacier::start()
       ├─ memory/      pattern scanner + name-keyed signature registry
       ├─ sdk/GameSDK  resolves game pointers by name
       ├─ sdk/ItemRendering    borrows the game's item renderer for icons
       ├─ sdk/HitConfirmation  packet-vtable hook, confirms melee hits
       ├─ module/      self-registering modules (GLACIER_MODULE macro)
       ├─ core/        EventBus, Config
       ├─ hook/        MinHook wrapper + DXGI Present/ResizeBuffers hook
       └─ ui/          Direct2D overlay, immediate-mode menu, HUD editor,
                       InputGuard (OS-level input blocking)
```

**Controls:** `G` or `M` opens the menu (only during actual gameplay —
`cursorGrabbed()` — not chat/pause/inventory/main-menu). `G` or `Escape`
closes it; `M` only opens, never closes. `END` unloads. `F1` hides HUD
modules without opening the menu. Config at `%APPDATA%\Glacier\config.ini`.

## Open items right now

Newest first. **Nothing in items 1–3 has been tested in-game yet** — all
three landed after the most recent logs were captured.

### 1. Unload race — DIAGNOSED THIS SESSION, NOT YET FIXED

**This is the highest-value item in the file: a confirmed root cause with a
clear fix, just not written yet.** Read the evidence before touching it.

**Symptom, from the Win11 friend's log:**

```
13:41:34.635 [info] D3D hook confirmed live on the game's swapchain
   … ~2 minutes of normal operation …
13:43:23.536 [warn] live swapchain vtable 0x7fface950168 differs from the probed vtable 0x0
13:43:23.536 [error] *** access violation at 0x0 (no module) — Glacier was: (idle)
13:43:23.536 [error]     writing address 0x0 (near-null: a missing null check)
13:43:23.582 [info] all hooks removed
13:43:23.582 [info] Glacier detached
```

**Why this is provably the unload race, not a guess.** The vtable check in
`hkPresent` is `s_vtableChecked.exchange(true)` — it can only run **once**
per value of that flag. It logged "confirmed live" at attach. It then logged
the *mismatch branch* two minutes later. Both branches running means the
flag went back to false in between, and the only code that does that is
`D3DHook::shutdown()` (`D3DHook.cpp:193`). So: the user pressed END,
`shutdown()` ran and nulled `s_originalPresent`/`s_hookedVTable`, while the
game's render thread was **already inside `hkPresent`**. That in-flight
detour then re-ran the vtable check (against the now-null `s_hookedVTable`,
hence "differs from the probed vtable 0x0") and fell through to
`return s_originalPresent(sc, sync, flags)` — a call through a null pointer.
Hence "access violation at 0x0 (no module)".

**The fix is a teardown-ordering problem, not a null check.** Adding `if
(!s_originalPresent) return S_OK;` would paper over it and *still* be wrong:
returning without calling the real Present drops a frame, and the deeper
issue is that `HookManager::shutdown()` calls `MH_RemoveHook` while game
threads can be executing inside our detours. Look at how the reference
clients sequence unload — MinHook's `MH_DisableHook` already suspends
threads and rewrites instruction pointers, so the ordering fix is to let
MinHook fully disable/remove **before** any trampoline pointer is nulled,
and ideally to not null them at all (a removed hook means the detour can no
longer be entered). Right now `Glacier::start()`'s teardown calls
`D3DHook::shutdown()` (nulls the pointers) *before*
`HookManager::shutdown()` (removes the hooks) — that ordering is backwards
and is the bug. Check `Renderer::shutdown()` and `ModuleManager::shutdown()`
for the same shape while you're there.

### 2. Rendering on D3D12 — fix shipped, needs a real test

Bedrock renders through **either D3D11 or D3D12** depending on build and
machine. The user's own machine is D3D11 and has always rendered fine; both
friends saw *nothing* render, and their logs eventually explained why.

Two bugs, in sequence:

- **First:** `D3DHook::hkPresent` gated the entire overlay callback on
  `sc->GetDevice(ID3D11Device)` succeeding. That call correctly returns
  `E_NOINTERFACE` on a D3D12-backed swap chain — not an error, just DXGI
  telling the truth — so the whole overlay was silently skipped with nothing
  logged. Nothing downstream even used the device it produced
  (`Renderer::beginFrame` binds straight to the swap chain). Fixed: called
  unconditionally now, device/context are best-effort extras.
- **Then:** with that gate gone, the friend's log showed
  `could not get the back buffer as a DXGI surface (D3D11) or bridge it via
  D3D11On12 (D3D12): 0x80004002` every frame, plus `game is rendering
  through D3D12 but no command queue has been captured yet`. The D3D11On12
  bridge needs a live `ID3D12CommandQueue*`, and the
  `IDXGIFactory2::CreateSwapChainForHwnd` hook meant to capture it **never
  fired** — the game creates its swap chain long before an injected DLL
  attaches, so that hook is structurally too late. (Latite gets away with it
  because of how it loads; we can't.) Fixed by hooking
  `ID3D12CommandQueue::ExecuteCommandLists` instead, via the same
  probe-object vtable trick already used for `Present`/`ResizeBuffers` — the
  game's real queue calls it constantly, so it captures within a frame.
  `CreateSwapChainForHwnd` stays installed as a secondary path.

**To verify (needs the D3D12 friend, not the user):** watch for `captured
the game's D3D12 command queue via ExecuteCommandLists`, then `bridged the
overlay onto the game's D3D12 swap chain via D3D11On12`, then `first
successful beginFrame`. If the capture line appears but the bridge line
doesn't, `D3D11On12CreateDevice` is failing and its `hr` is logged. If
neither appears, the probe queue never got created — that's logged too.

### 3. Cursor and input handoff — rewritten twice this session, needs a test

The through-line of this session: **the game's own
`grabCursor()`/`releaseCursor()` are not sufficient for any of this**, even
when they demonstrably work. The user's log shows `releaseCursor called —
cursorGrabbed() now reports released (ok)` and the matching `grabbed (ok)`,
and *still* the player could move, look, and never got the cursor back
cleanly. Three separate mechanisms now cover what that one call was wrongly
assumed to handle:

- **Movement/attack** — `ui::InputGuard` (`src/ui/InputGuard.h/.cpp`), a
  `WH_KEYBOARD_LL` + `WH_MOUSE_LL` pair on a dedicated pump thread, gated on
  `ui::Menu::open()`. Blocks W/A/S/D/Space/Shift/Ctrl and left/right/middle
  click. Same technique `NullMovement` already used, just menu-gated. **The
  user has confirmed this part works** ("its good that i cant move").
- **Camera look** — the same `WH_MOUSE_LL` hook now also swallows
  `WM_MOUSEMOVE` while the menu is open. Because that freezes the real OS
  cursor too, `InputGuard` accumulates its own **virtual cursor** from the
  raw deltas before dropping them, and `ui::Input::pollMouse` reads that
  instead of `GetCursorPos` while the menu is open. Buttons still come from
  `GetAsyncKeyState` (physical state, unaffected by the block).
- **Cursor grab/release** — `Glacier::setCursorReleased` is now a direct
  port of Flarial's `CursorHandler::grabCursor`/`releaseCursor`
  (`reference/flarial/src/Client/Hook/Hooks/Input/CursorHandler.hpp`): on
  close, `ClipCursor` to a zero-size rect at the window centre + `SetCapture`
  + `ShowCursor(FALSE)`; on open, undo all three. Pure OS APIs, no signature.
  Made **edge-triggered** — the Present hook calls it every frame regardless
  of menu state, and re-clipping every frame while the menu is *closed*
  would fight the game's own cursor handling for the whole session rather
  than just at the moment of close.

The custom-drawn menu reticle (`Menu::drawCursor`) was **removed** at the
user's request, on the assumption the real OS cursor is now reliably
visible. If the cursor turns out to still be invisible over the menu, that
assumption is the thing to re-check first — not to re-add the reticle
blindly.

**To verify:** open the menu → cursor visible and freely movable, camera
does not turn, player does not move, menu is clickable. Close → game takes
the mouse back immediately, look works normally again.

### 4. An access violation inside Glacier.dll itself, cause unknown

Distinct from item #1 (that one is at address `0x0` and now explained).
These are **near-null reads inside our own module**, three logs now:

| Log | Address | Reading | Build |
|---|---|---|---|
| A | `Glacier.dll+0x296A6` | `0x9C` | — |
| B | `Glacier.dll+0x295E6` | `0xA0` | 1.26.40.5 |
| C | `Glacier.dll+0x29626` | `0xC4` | 1.26.40.5 |

Shared preconditions in all three: **outside a world** (`first
getLocalPlayer returned 0x0`, no "world is loaded" line ever printed), and
several `config saved` lines a few seconds apart immediately before —
meaning the user was toggling module keybinds, which goes through
`ModuleManager::handleKey` on the window thread and does *not* require being
in a world. `Glacier was: (idle)`, so no `GLACIER_ACTIVITY` scope was on the
stack.

**Audited twice, not found.** Checked `cursorGrabbed()`, `ModuleManager`
(tick/render/handleKey), `Config::save()`, `HudEditor::update`, every
module's `onTick`/`onEnable`, and `GameSDK`'s `hotbar()`/`inventory()`/
`armor()`/`localPlayer()` (all correctly guard on `localPlayer()` first).
`ModuleManager`'s lock-free access to the module list is a documented
design choice — the vector never resizes after `initialize()` — not the bug.

**Next step is a repro, not another audit.** The offsets differ per build so
none of the three numbers is reusable. Ask exactly which module keybinds
were pressed in the ~10 seconds before. A debugger attach (WinDbg or VS
"Attach to Process", if a JIT prompt appears) maps the address to a function
and line, which is what actually unblocks this.

### 5. Hit confirmation — built, never run, off by default on purpose

`hitConfirmation` under `[Glacier]` stays off until someone runs it against
a live client with a debugger available. Full writeup below. The two
hand-derived offsets are now **independently cross-confirmed** by Horion and
Onix-Client-v2 (see that section) — meaningfully less risky than it was, but
still never executed.

### 6. Phase 8 module catalog — batches remain

~150 Latite/Flarial modules unexamined. See the Phase 8 row in the roadmap
table for what's shipped, what's excluded on purpose, and how to pick the
next batch.

## Verified working in-game

**On the user's machine (D3D11, build 1.26.42.1), most recent log:**
injection, version detection, 12/12 signatures, all SDK hooks, D3D hook on
the real swapchain, `overlay bound directly to the game's back buffer
(1920x1051, format 28)`, config load/save, world load, **item icons** (`first
item icon batch drawn (15 icons)`), a clean `ResizeBuffers` → rebind cycle,
`releaseCursor`/`grabCursor` both reporting ok, and the menu's module
list/toggles/sliders (config saves reflected real interaction).
**Coordinates** and **Day Counter** confirmed in earlier sessions.

**Confirmed by the user this session:** `InputGuard` blocking movement while
the menu is open.

**Not verified by anyone:** everything in open items 1–3 above, the D3D12
path end-to-end, the menu background blur, hit confirmation, Inventory HUD's
icon grid, and every Phase 8 module (Speedometer, Walk Distance, Low
Durability Warning, Combo Counter, Hotbar HUD, Frame Time Display,
Stopwatch, DVD Screen, Custom Crosshair). None have a known problem — they
are simply unobserved.

## Architecture facts that are easy to get wrong

These each cost a debugging cycle already. Don't re-derive them.

1. **Bedrock renders through D3D11 *or* D3D12**, depending on build and
   machine, and you cannot tell which without probing. Anything that assumes
   D3D11 — `GetDevice(ID3D11Device)`, `GetBuffer(IDXGISurface)` — will fail
   with `E_NOINTERFACE` (`0x80004002`) on a D3D12 machine and must fall back
   to the D3D11On12 bridge rather than treating that as an error. This is
   why the overlay rendered for the user and for nobody else.

2. **A hook installed at attach time cannot catch a call the game already
   made.** `CreateSwapChainForHwnd` fires once, at game startup, long before
   injection. Anything needed from a one-shot call has to be obtained from a
   *recurring* call instead (`ExecuteCommandLists` for the command queue).
   Latite hooks the former because of how it loads; copying that verbatim
   into an injected DLL silently gets you nothing.

3. **Bedrock reads keyboard/mouse through RawInput.** `WM_KEYDOWN` and mouse
   messages are *not* reliably delivered to our WndProc. Everything
   input-related polls `GetAsyncKeyState`/`GetCursorPos` on the logic thread
   or the Present callback instead — the menu key, the mouse for the menu,
   Stopwatch's keybinds, all of it.

4. **The game's `cursorGrabbed()` flag is not the mechanism for anything
   you want.** It does not gate movement, does not gate attack, and does not
   by itself free or capture the OS cursor — a log with `released (ok)` and
   a player still walking around proved all three. Movement/attack/look are
   blocked at the OS input layer (`ui::InputGuard`); the cursor is handed
   over with `ClipCursor`/`SetCapture`/`ShowCursor` (Flarial's approach).

5. **Never compute the same edge in two places.** `GetAsyncKeyState` sees a
   key before `WM_KEYDOWN` arrives, so a poll and a WndProc handler racing on
   the same physical press double-fire. The menu key, the mouse buttons, and
   Stopwatch's keybinds each have exactly one owner of their press/release
   edge; WndProc only ever swallows keys so they don't leak into the game.

6. **`GetAsyncKeyState` doesn't care which window has focus.** It reports the
   physical key state system-wide. Any hotkey polled this way needs an
   explicit "are we actually in a context where this key means what I think
   it means" gate — see the menu open condition (`cursorGrabbed()`, not just
   `inGame()`) and Stopwatch's identical guard. Forgetting this is why typing
   "good game" in chat used to pop the menu open mid-sentence.

7. **The overlay draws straight onto the game's back buffer.** `Renderer`
   calls `D2D1CreateDeviceContext` on the swap chain's DXGI surface, on the
   game's own device — Flarial's approach. No second device, no shared
   texture, no keyed mutex, no compositing pass. `beginFrame` must never
   clear the target — it's the game's own frame. On D3D12 the same D2D path
   runs against a `CreateWrappedResource` surface from the On12 bridge.

8. **Nothing may hold a back-buffer reference when the game calls
   `ResizeBuffers`.** Unconditional: no size comparison, no zero check
   (`width`/`height` of 0 means "keep current size", not "ignore this call").
   `Renderer::resize` drops every reference before the game's call runs.

9. **The Present hook and the ScreenView hook run on the game's threads.**
   Nothing there may block, take a lock the game could hold, or send a
   window message (`SetWindowText`/`GetWindowText` block synchronously on the
   window-owning thread — from the render thread, that's an instant deadlock
   against the game's main thread). A hang with no exception in the log is
   almost always this, not a bad pointer.

10. **Teardown runs while the game's threads are inside our detours.** See
    open item #1 — nulling a trampoline pointer that an in-flight detour is
    about to call through is a crash at address `0x0`. Order teardown so
    hooks are fully removed before anything they reference is destroyed.

11. **Menu open/close is asymmetric on purpose.** `G` opens and closes, `M`
    only opens, `Escape` only closes — not three redundant ways to do the
    same thing. This matches Latite's own `ScreenManager::onKey` while
    keeping the historical G/M "either key works" bootstrap for opening.

12. **HUD widgets under the menu panel must not be draggable.** `HudEditor`
    declines to process a widget whose bounds intersect `Menu::panelRect()` —
    otherwise a click meant for a menu button that happens to land over a
    widget hidden behind the panel grabs the widget instead of the button.

## Signature sync — read before touching Signatures.cpp

`src/memory/Signatures.cpp` is **generated**. Never hand-edit it.

```bash
python tools/sync_signatures.py            # regenerate from Latite master
python tools/sync_signatures.py --check    # exit 1 if stale
```

The mapping is explicit: every imported value is named on both sides in
`tools/sync_signatures.py`. If Latite renames something, the script refuses
to write and names what it couldn't find — a silent wrong value is the
failure mode that costs days, so this is deliberate.

A few offsets have no `CLASS_FIELD` source to extract (hand-derived from a
type's declared layout instead) — `Packet::handler` and
`ActorEventPacket::eventID` are the current examples. These are `OffsetEntry`
entries with `literal=` set and a comment explaining the derivation; they
still live in this file, never inline elsewhere, even though the sync tool
can't verify them against upstream.

`.github/workflows/sync-signatures.yml` runs daily and opens a PR on change.

**Licensing:** that one file is why Glacier is AGPLv3. Keep all imported data
in it — never inline a pattern or struct offset elsewhere. See
`docs/acknowledgements.md`.

## Roadmap — remaining phases

Phases 0–7 are done and verified in-game (scaffold, hooks/SDK, EventBus +
menu, module catalog, config persistence, version targeting + sync tooling,
interaction & pause, item rendering).

| Phase | Scope |
|---|---|
| **8** | **Module catalog expansion.** Full scope is the whole Latite (~40 modules, `reference/latite`) and Flarial (~130 modules, `reference/flarial`) catalogs, minus three exclusions: `Doom` (an embedded game engine, not a Minecraft client feature), `SkinStealer` (copies another player's skin without consent), and server-specific utilities tied to one community (`HiveUtils`, `HiveStat`, `HiveModeCatcher`, `HiveTranslate`, `ZeqaUtils`). Ported in batches, smallest-new-SDK-plumbing first. **Shipped:** Speedometer, Walk Distance (from `playerPosition()` deltas — no velocity accessor exists); Low Durability Warning (`armor()`'s `durabilityFraction()`); Combo Counter (via `sdk::HitConfirmation`, off by default); Hotbar HUD, Inventory HUD (`GameSDK::hotbar()`/`inventory()`, mirroring `armor()`'s pattern over `PlayerInventory` instead of the ECS); Frame Time Display, Stopwatch, DVD Screen, Custom Crosshair (zero new signatures). **Next batch is unplanned** — scan `ModuleManager.cpp`/`Manager.cpp` in the reference trees for what's left, sort by "needs a new SDK accessor" vs. "needs a new hook/signature" vs. "already covered". |
| **9** | **HUD widget visual pass — done for the current catalog.** Shared design tokens (`HudModule.h`) and a `TextHudModule` base. Background defaults **off** everywhere, matching Latite's `HUDModule`. Armor HUD and Hotbar HUD are chrome-less, keyed off Latite's `ArmorHUD.cpp`. **Re-open whenever Phase 8 adds a widget.** |
| **10** | **Theming & polish** — menu animation, per-module colour presets, keybind-conflict UX. Background blur landed this session (`Renderer::blurBackdrop`, Gaussian blur via a captured D2D bitmap, drawn behind the whole overlay before the dim fill) but is **untested**. |
| **11** | **Packaging** — version-string exports for a launcher, tag-triggered release zip (`build.yml` already has the release job). |

Scope boundary (enforced since Phase 0): **visual / HUD / QoL only**. No
KillAura, reach, fly, aimbot, or server-hitbox manipulation.

## Hit confirmation — `sdk/HitConfirmation`, off by default

Combo Counter confirms hits via the game's own `ActorEventPacket`
(`HURT_ANIMATION`) rather than counting swings, following
`reference/latite/.../ComboCounter.cpp`.

**What it does:** resolves `MinecraftPackets::createPacket`, calls it once
to construct a throwaway `ActorEventPacket`, reads that packet's `handler`
field to reach a dispatcher object, reads *that* object's vtable, and swaps
vtable slot 1 — the same technique
`reference/latite/src/client/memory/hook/hooks/PacketHooks.cpp` uses across
~140 packet types, scoped here to just the one type Combo Counter needs.

**Why it stays off by default:**

- `Packet::handler` (`0x20`) and `ActorEventPacket::eventID` (`0x38`) are
  hand-derived from `Packet.h`'s declared layout, not a `CLASS_FIELD`, and
  Flarial has no equivalent type to cross-check — **but both are now
  independently confirmed** by two other reverse-engineered clients.
  Horion's `SDK/Packet.h` lays out `Packet` with its dispatcher pointer at
  `0x20` (vtable `0x00`, header `0x08`–`0x1F`, dispatcher `0x20`, base size
  `0x30`) and `ActorEventPacket::eventId` at `0x30 + 0x08` = `0x38`;
  Onix-Client-v2's `SDK/static/IPacket.h` independently places `mHandler` at
  `0x20` too. Both target older MC versions (1.16–1.18-ish), so this is
  evidence the layout is architecturally stable, not proof for 1.26.40.
- Bootstrapping the vtable means calling a function that returns
  `std::shared_ptr<Packet>` **by value** — a hidden caller-owned return-slot
  pointer ahead of the real argument on MSVC x64, modelled explicitly since
  there's no declared C++ type for a compiler to generate that call from.
  The resulting `shared_ptr` is deliberately **never destroyed** — modelling
  its destructor would mean guessing at an atomic refcount decrement and a
  virtual deleter call with nothing to check the guess against.

Both hand-derived reads happen only inside `__try`/`__except`. A wrong value
should degrade the feature (logged), not crash — but nobody has run it
against a live client to confirm that "should".

**Deliberately not ported from Latite:** matching the hurt animation's
`runtimeID` against the entity actually attacked. `Actor::getRuntimeID()` has
no discoverable signature, offset, or vtable index anywhere in the reference
sources. Without it, this counts "a swing, then any actor's hurt animation
shortly after" rather than confirming same-target.

**To verify:** set `hitConfirmation = true`, reload, watch the console.
`hit confirmation ready` = installed; `hit confirmation unavailable —
missing …` names which prerequisite failed. **If the game crashes shortly
after enabling it, this is the first suspect.**

## References

The first four are copyleft-compatible with our AGPLv3 (GPLv3 material may
be incorporated under GPLv3 §13; LGPL-3.0 likewise). The last three are
checked out on a different basis — the user has **direct permission from
their authors**, not a public copyleft grant — so treat them as
read-for-technique / cross-check-only, never copy-paste source. All seven
are checked out locally:

| Project | License | Local path | Use it for |
|---|---|---|---|
| **[Latite](https://github.com/LatiteClient/Latite)** | GPLv3 | `reference/latite` | **Primary signature source.** Our sync tool reads it directly. Also the primary behavior reference — module logic, menu handling, packet hooking, the DX11/DX12 renderer split. |
| **[Flarial (dll-oss)](https://github.com/flarialmc/dll-oss)** | AGPLv3 | `reference/flarial` | Rendering internals, the larger module catalog, item rendering, **cursor grab/release** (`CursorHandler.hpp` — ported directly). |
| **[Selaura](https://github.com/selauraclient/selaura)** | GPLv3 | — | Small, clean framework. Good for scanning technique; no module catalog. |
| **[LeviLamina](https://github.com/liteldev/levilamina)** | LGPL-3.0 | — | Named class/struct/method headers. **BDS-only** — client types don't exist there. Names and shapes, never offsets. |
| **Horion** | CC BY-NC 4.0 | `reference/Horion/HorionContinued` | Author permission. Clean, fully-named `SDK/Packet.h` — independently confirms both of `HitConfirmation`'s hand-derived offsets. Older MC version; corroboration, not proof. |
| **Onix-Client-v2** | Proprietary | `reference/Onix-Client-v2` | Author permission. `SDK/static/IPacket.h` has a fully-laid-out `Packet` class (1.16/1.17) — second confirmation of `Packet::handler`. Per-version SDK folders if a version-specific struct is ever needed. |
| **Solstice-release** | Unlicensed | `reference/Solstice-release` | Author permission. CMake, ImGui + Luau + Kiero. Mostly third-party integration patterns rather than Bedrock struct data; **not yet mined**. |

Attribution belongs in `docs/acknowledgements.md`; imported *data* belongs
only in `src/memory/Signatures.cpp`. **The pack at
`C:\Users\User\Desktop\Glacier v7\packs\...\hud_modules\` is for module
IDEAS only** — what widgets exist, not how to build them.

### Specific files that mattered this session

- `reference/flarial/src/Client/Hook/Hooks/Input/CursorHandler.hpp` — the
  `ClipCursor`-to-a-zero-rect + `SetCapture` + `ShowCursor` grab/release
  pair, ported verbatim in shape into `Glacier::setCursorReleased`.
- `reference/latite/src/client/memory/hook/hooks/DXHooks.cpp` — the
  D3D11On12 bridge and the `CreateSwapChainForHwnd` command-queue capture.
  Read the caveat in architecture fact #2 before copying the latter.
- `reference/latite/src/client/render/Renderer.cpp` — the D3D12 side:
  `CreateWrappedResource`, `AcquireWrappedResources`/`ReleaseWrappedResources`
  around the draw, and the `Flush()` that makes it actually submit.
- `reference/Horion/HorionContinued/SDK/Packet.h` and
  `reference/Onix-Client-v2/OnixClient/SDK/static/IPacket.h` — the two
  independent confirmations of `HitConfirmation`'s offsets.
- `reference/latite/src/client/screen/Screen.cpp` / `ScreenManager.cpp` —
  the menu-open/close shape. Note that their cursor calls alone were
  **not** sufficient for us; see architecture fact #4.

### Libraries

- [MinHook](https://github.com/TsudaKageyu/minhook) — hooking engine (vcpkg).
- [EnTT](https://github.com/skypjack/entt) — Bedrock's ECS, for armor/component lookups.
- [kiero](https://github.com/Rebzzel/kiero) — the vtable-discovery technique `D3DHook` reimplements, now for both `IDXGISwapChain` and `ID3D12CommandQueue`.

### This project

- Repo: <https://github.com/Glacier-Client-BE/Glacier-DLL>
- CI: <https://github.com/Glacier-Client-BE/Glacier-DLL/actions>

## Verification reality

CI proves compilation. It cannot prove anything works in-game — that needs
the user to inject on Windows and read the debug console. When you change
anything touching rendering, input, or signatures, say plainly that it's
unverified and name exactly what log line or on-screen behaviour would
confirm it.

**When the user pastes back a real log, read every line.** Everything of
value found this session came from a log, not from reasoning about the code:
the D3D11-only gate, the never-firing `CreateSwapChainForHwnd` hook, and the
unload race were all sitting in plain text. The unload race in particular
was found by noticing that a `LOG_ONCE`-style once-only branch had logged
*twice* — the kind of detail that only shows up if you actually read the
timestamps.

**Three machines, three configurations.** The user is D3D11 on 1.26.42.1;
one friend is on 1.26.40.5; at least one is D3D12. "Works for me" has
repeatedly meant "works on D3D11 only". Ask which log came from whom.

`docs/signatures.md` tracks per-entry status; entries stay ⚠️ *inherited*
until someone confirms them against a running client.

## Diagnosing a crash

`CrashHandler` (installed first thing at attach) logs any access violation:

```
[Glacier][error] *** access violation at Minecraft.Windows.exe+0x1A2B3C4 — Glacier was: reading the ClientInstance map
[Glacier][error]     reading address 0x28 (near-null: a missing null check)
```

Three things to read off it:

- **`Glacier was:`** — the innermost `GLACIER_ACTIVITY` scope on that thread.
  `(idle)` means the fault happened somewhere with no activity marker —
  usually Glacier's own logic rather than a game-memory read. Add one to any
  new path that calls into game code.
- **module+offset**, not a bare address — ASLR makes absolute addresses
  useless between runs. `Minecraft.Windows.exe+0x...` is a wrong game
  offset/signature; `Glacier.dll+0x...` is a bug in **our own code**;
  `0x0 (no module)` is a call through a null function pointer, which in this
  codebase means the unload race (open item #1).
- **near-null vs wild** — near-null is a missing null check (ours, usually).
  Wild means a wrong offset or a signature resolving to the wrong function.

Some reported exceptions are first-chance and handled normally downstream;
the last line before the log ends is the informative one. Reporting stops
after 5.

## Past incidents worth remembering (all fixed, keep the lessons)

**The D3D11-only assumption.** The overlay rendered perfectly for the
developer and for nobody else, for multiple sessions, because `hkPresent`
bailed out on a `GetDevice(ID3D11Device)` that legitimately fails on D3D12 —
and bailed *silently*, logging nothing. Lesson: an early-return on a failed
capability probe needs a log line, and "works on my machine" is not a data
point when the thing that differs between machines is the entire graphics
backend.

**The wrong-window bug.** Glacier once hooked its own debug console instead
of the game (`Logger::attachConsole()` ran before `findGameWindow()`, so a
freshly allocated console won the enumeration). Produced four unrelated-
looking symptoms at once: wrong window title, dead mouse input, hover
highlighting the wrong widget, CPS Counter never counting. Lesson: several
symptoms that look like separate broken features can be one shared wrong
assumption. Fixed by making `DXGI_SWAP_CHAIN_DESC::OutputWindow` from the
first real frame authoritative instead of guessing.

**The world-load crash (item rendering).** Root cause: MSVC puts a
`vtable` pointer at offset `0x00` on any class with virtual functions, even
if it declares data members "first" in the source. Latite's
`MinecraftUIRenderContext.h` declares two pointers first but the class is
polymorphic, so `0x00`/`0x08` read the vtable and the real `ClientInstance*`
respectively. The fix (corrected offsets to `0x08`/`0x10`, plus a
cross-check against the independently-resolved `ClientInstance`) is why
`itemIcons` is on by default now.

**ResizeBuffers.** `IDXGISwapChain::ResizeBuffers` fails with
`DXGI_ERROR_INVALID_CALL` if any reference to a back buffer is outstanding.
The game doesn't necessarily check that return value, so a failed resize
looks fine until DXGI dies later on a wild pointer, with crash activity
reading `(idle)` because no Glacier code is on the stack by then. Fix:
`Renderer::resize` drops its back-buffer reference unconditionally, before
the game's call runs — no size comparison, no zero check.

**Cursor-grab as a universal lever.** Multiple sessions assumed the game's
`releaseCursor()`/`grabCursor()` would stop movement, stop camera look, and
free/capture the OS cursor. It does none of those reliably, and a log
showing it reporting success was mistaken for a log showing it working.
Lesson: "the call succeeded" and "the call had the effect I wanted" are
different claims, and only the second one is worth anything.
