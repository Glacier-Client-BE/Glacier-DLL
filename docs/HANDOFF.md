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
> the only ground truth this project has.
>
> Reference clients (all copyleft-compatible, we're AGPLv3): Latite (GPLv3),
> Flarial dll-oss (AGPLv3), Selaura (GPLv3), LeviLamina (LGPL-3.0, BDS-only —
> names and struct shapes, never offsets). All four are checked out locally
> under `reference/` — read the real source there before guessing at how
> something works. Imported signature data lives only in
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
       ├─ sdk/ItemRendering    borrows the game's item renderer for icons
       ├─ sdk/HitConfirmation packet-vtable hook, confirms melee hits
       ├─ module/      self-registering modules (GLACIER_MODULE macro)
       ├─ core/        EventBus, Config
       ├─ hook/        MinHook wrapper + D3D11 Present/ResizeBuffers hook
       └─ ui/          Direct2D overlay, immediate-mode menu, HUD editor
```

**Controls:** `G` or `M` opens the menu (only during actual gameplay —
`cursorGrabbed()` — not chat/pause/inventory/main-menu). `G` or `Escape`
closes it; `M` only opens, never closes. `END` unloads. `F1` hides HUD
modules without opening the menu. Config at `%APPDATA%\Glacier\config.ini`.

## Open items right now

Newest first — these came out of the most recent in-game logs and haven't
been re-tested since the fix landed.

### 1. An access violation inside Glacier.dll itself, cause unknown

**Symptom, from a real log:** `access violation at Glacier.dll+0x296A6 —
Glacier was: (idle)`, reading address `0x9C` (near-null). Happened ~2 minutes
after attach, on the character-select/main-menu screen — the player never
loaded into a world that session (no "world is loaded" log line ever
printed). Five `config saved` lines preceded it, meaning the user was
toggling module keybinds directly (that path doesn't require being in a
world — see `ModuleManager::handleKey`).

**Not yet found.** Audited `cursorGrabbed()`, `ModuleManager` (tick/render/
handleKey), `Config::save()`, and every new module's `onTick()` for an
unguarded null pointer reachable without a world loaded — nothing jumped
out. This crash is in **Glacier's own address space**, not the game's, so
it's very likely a bug in our C++, not a wrong game offset.
**Next step:** get a repro. Ask exactly what was pressed/clicked in the
~10 seconds before it happened. `Glacier.dll+0x296A6` needs symbols to mean
anything — if the user can attach a debugger (even just `WinDbg`/VS "Attach
to Process" after the crash, if a JIT debugger prompt appears) that address
maps straight to a function and line.

### 2. Cursor grab/release — fix shipped, needs a real test

The user reported: opening the menu shows no cursor, and the player can
still move/look. Two separate things were fixed for this without being able
to verify either in-game:

- **No visible cursor was a certain bug, now fixed.** Glacier's menu is a
  D2D overlay, not a real Bedrock screen, so the game never had a reason to
  draw a cursor for it. `Menu::drawCursor()` now draws a small reticle at
  the tracked mouse position, on top of everything, every frame the menu is
  open.
- **Movement not stopping is unconfirmed.** Read Latite's actual
  `reference/latite/src/client/screen/Screen.cpp` /
  `ScreenManager.cpp` — their `Screen::onUpdate` calls `releaseCursor()`
  every update tick a screen is active, `ScreenManager::exitCurrentScreen`
  calls `grabCursor()` once on close, and **Latite draws no cursor of its
  own** (their `SetCursor` code is dead/commented out) and still shows one —
  which only makes sense if `releaseCursor()` itself unhides the OS pointer.
  Glacier's `applyCursorState` already matched that shape before this
  session. What's new: `GameSDK::cursorControlWorking()` goes false the
  first time a `releaseCursor()` call is *observed* not to change
  `cursorGrabbed()` (logged as `releaseCursor called — cursorGrabbed() now
  reports still grabbed (did NOT take effect)`), and
  `Glacier::setCursorReleased`'s `ShowCursor`/`ClipCursor` fallback now
  triggers on that, not just on the signature failing to resolve at all.

**To verify:** open the menu in a world and watch for the `releaseCursor
called — cursorGrabbed() now reports …` line (prints once). "released (ok)"
means the real mechanism works and the drawn reticle was the whole fix.
"did NOT take effect" followed by `falling back to ShowCursor` means the
`ClientInstance::releaseCursor` signature resolved to the wrong function for
this build — same failure class as the item-icon vptr bug in the history
below, and the next step is re-deriving that one pattern specifically.

### 3. Item-render race — fixed, needs a real test

First real in-game log caught a genuine bug: enabling Hotbar HUD faulted
`ItemRenderer::renderGuiItemNew` reading near-null. `drawBatchGuarded`
resolves each slot's `ItemStack*` fresh at draw time (one frame after the
HUD decided to submit it) but never re-checked whether the slot was still
non-empty — a hotbar slot that had something when submitted and went empty
(swapped, scrolled past) a frame later got handed straight to the item
renderer, which has no null check of its own for that. Fixed in
`ItemRendering.cpp`'s `drawBatchGuarded` — re-checks `ItemStack::item`
against the freshly-resolved pointer, not the frame-old decision. Armor HUD
never hit this (armor churns far less than the hotbar); Inventory HUD's
main grid is exposed to the identical race and was never observed crashing,
but is covered by the same fix.

**To verify:** enable Hotbar HUD (or Inventory HUD) with `itemIcons = true`
and scroll through hotbar slots, including empty ones, for a while. No
crash = fixed.

### 4. Hit confirmation — built, never run, off by default on purpose

`hitConfirmation` under `[Glacier]` is off by default and should **stay**
off until someone runs it against a live client with a debugger available.
Full writeup below (§ "Hit confirmation"). Two hand-derived struct offsets
with no independent cross-check, plus a hand-modelled ABI for a function
returning `std::shared_ptr` by value — all three are real, deliberate,
user-approved risks, not oversights, but genuinely unverified.

### 5. Phase 8 module catalog — batches remain

~150 Latite/Flarial modules haven't been looked at yet. See the Phase 8 row
in the roadmap table below for what's shipped, what's excluded on purpose,
and how to pick the next batch.

## Verified working in-game (1.26.40.5)

Injection, version detection, all signatures resolving, all SDK hooks, D3D
hook on the real swapchain, config load/save, overlay compositing,
**Coordinates**, **Day Counter**, item rendering (icons drew successfully in
the most recent log — `first item icon batch drawn (4 icons)` — before the
unrelated Hotbar HUD race in item #3 above), the menu opening/closing and
its module list/toggles/sliders (config saves reflected real interaction).

**Not yet verified by anyone:** cursor grab/release actually pausing
movement, the drawn menu cursor, the item-render race fix, hit confirmation,
Inventory HUD's icon grid, and every Phase 8 module added this session
(Speedometer, Walk Distance, Low Durability Warning, Combo Counter, Hotbar
HUD, Frame Time Display, Stopwatch, DVD Screen, Custom Crosshair). None of
these have a known problem — they're just unobserved.

## Architecture facts that are easy to get wrong

These each cost a debugging cycle already. Don't re-derive them.

1. **Bedrock reads keyboard/mouse through RawInput.** `WM_KEYDOWN` and mouse
   messages are *not* reliably delivered to our WndProc. Everything
   input-related in Glacier polls `GetAsyncKeyState`/`GetCursorPos` on the
   logic thread or the Present callback instead — the menu key, the mouse for
   the menu (`ui::Input::pollMouse`), Stopwatch's keybinds, all of it.

2. **Never compute the same edge in two places.** `GetAsyncKeyState` sees a
   key before `WM_KEYDOWN` arrives, so a poll and a WndProc handler racing on
   the same physical press double-fire. The menu key, the mouse buttons, and
   Stopwatch's keybinds each have exactly one owner of their press/release
   edge; WndProc only ever swallows keys so they don't leak into the game.

3. **`GetAsyncKeyState` doesn't care which window has focus.** It reports the
   physical key state system-wide. Any hotkey polled this way needs an
   explicit "are we actually in a context where this key means what I think
   it means" gate — see the menu open condition (`cursorGrabbed()`, not just
   `inGame()`) and Stopwatch's identical guard. Forgetting this is why typing
   "good game" in chat used to pop the menu open mid-sentence.

4. **The overlay draws straight onto the game's back buffer.** `Renderer`
   calls `D2D1CreateDeviceContext` on the swap chain's DXGI surface, on the
   game's own device — Flarial's approach. No second device, no shared
   texture, no keyed mutex, no compositing pass. `beginFrame` must never
   clear the target — it's the game's own frame.

5. **Nothing may hold a back-buffer reference when the game calls
   `ResizeBuffers`.** Unconditional: no size comparison, no zero check
   (`width`/`height` of 0 means "keep current size", not "ignore this call").
   `Renderer::resize` drops every reference before the game's call runs.

6. **The Present hook and the ScreenView hook run on the game's threads.**
   Nothing there may block, take a lock the game could hold, or send a
   window message (`SetWindowText`/`GetWindowText` block synchronously on the
   window-owning thread — from the render thread, that's an instant deadlock
   against the game's main thread). A hang with no exception in the log is
   almost always this, not a bad pointer.

7. **Menu open/close is asymmetric on purpose.** `G` opens and closes, `M`
   only opens, `Escape` only closes — not three redundant ways to do the same
   thing. This matches Latite's own `ScreenManager::onKey` (Escape always
   exits the active screen) while keeping the historical G/M "either key
   works" bootstrap property for opening.

8. **HUD widgets under the menu panel must not be draggable.** `HudEditor`
   declines to process a widget whose bounds intersect `Menu::panelRect()` —
   otherwise a click meant for a menu button that happens to land over a
   widget hidden behind the panel grabs the widget instead of (or as well
   as) hitting the button.

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
| **8** | **Module catalog expansion.** Full scope is the whole Latite (~40 modules, `reference/latite`) and Flarial (~130 modules, `reference/flarial`) catalogs, minus three exclusions: `Doom` (an embedded game engine, not a Minecraft client feature), `SkinStealer` (copies another player's skin without consent), and server-specific utilities tied to one community (`HiveUtils`, `HiveStat`, `HiveModeCatcher`, `HiveTranslate`, `ZeqaUtils`). Ported in batches, smallest-new-SDK-plumbing first. **Shipped:** Speedometer, Walk Distance (from `playerPosition()` deltas — no velocity accessor exists); Low Durability Warning (`armor()`'s `durabilityFraction()`); Combo Counter (via `sdk::HitConfirmation`, off by default — see below); Hotbar HUD, Inventory HUD (`GameSDK::hotbar()`/`inventory()`, mirroring `armor()`'s pattern over `PlayerInventory` instead of the ECS); Frame Time Display, Stopwatch, DVD Screen, Custom Crosshair (zero new signatures — see each module's class comment for what got scoped down). **Next batch is unplanned** — scan `ModuleManager.cpp`/`Manager.cpp` in the reference trees for what's left, sort by "needs a new SDK accessor" vs. "needs a new hook/signature" vs. "already covered". |
| **9** | **HUD widget visual pass — done for the current catalog.** Shared design tokens (`HudModule.h`, padding/corner-radius/shadow settings) and a `TextHudModule` base. Background defaults **off** everywhere, matching Latite's `HUDModule` (its dark-fill `renderFrame` is only ever used for the editor's "selected" highlight, never normal play). Armor HUD and Hotbar HUD are chrome-less, keyed off Latite's `ArmorHUD.cpp`. **Re-open whenever Phase 8 adds a widget** — new widgets need the same "does this box earn its place" pass. |
| **10** | **Theming & polish** — menu animation, blur, per-module colour presets, keybind-conflict UX. |
| **11** | **Packaging** — version-string exports for a launcher, tag-triggered release zip (`build.yml` already has the release job). |

Scope boundary (enforced since Phase 0): **visual / HUD / QoL only**. No
KillAura, reach, fly, aimbot, or server-hitbox manipulation.

## Hit confirmation — `sdk/HitConfirmation`, off by default

Combo Counter confirms hits via the game's own `ActorEventPacket`
(`HURT_ANIMATION`) rather than counting swings, following
`reference/latite/.../ComboCounter.cpp`. This needed a new subsystem, and
it's the riskiest thing in the tree — riskier than the item-icon path, which
has a cross-check this one doesn't.

**What it does:** resolves `MinecraftPackets::createPacket`, calls it once
to construct a throwaway `ActorEventPacket`, reads that packet's `handler`
field to reach a dispatcher object, reads *that* object's vtable, and swaps
vtable slot 1 — the same technique
`reference/latite/src/client/memory/hook/hooks/PacketHooks.cpp` uses across
~140 packet types, scoped here to just the one type Combo Counter needs.
Every `ActorEventPacket` the game receives afterward calls Glacier's detour
first, which reads the packet's `eventID` byte and records a timestamp on
`HURT_ANIMATION`.

**Why it stays off by default:**

- `Packet::handler` (offset `0x20`) and `ActorEventPacket::eventID` (offset
  `0x38`) are hand-derived from `Packet.h`'s declared field layout, not a
  `CLASS_FIELD` — and Flarial has no equivalent type to cross-check against.
- Bootstrapping the vtable means calling a function that returns
  `std::shared_ptr<Packet>` **by value** — a hidden caller-owned return-slot
  pointer ahead of the real argument on MSVC x64, modelled explicitly since
  there's no declared C++ type here for a compiler to generate that call on
  its own. The resulting `shared_ptr` is deliberately **never destroyed**
  (see `HitConfirmation.cpp`) — modelling its destructor would mean guessing
  at an atomic refcount decrement and a virtual deleter call with nothing to
  check the guess against.

Both hand-derived reads happen only inside `__try`/`__except`. A wrong value
should degrade the feature (logged), not crash the game — but nobody has
run it against a live client to confirm that "should".

**Deliberately not ported from Latite:** matching the hurt animation's
`runtimeID` against the entity actually attacked. `Actor::getRuntimeID()` has
no discoverable signature, offset, or vtable index anywhere in the reference
sources — genuinely unavailable, not a risk that was weighed. Without it,
this counts "a swing, then any actor's hurt animation shortly after" rather
than confirming same-target — usually right, but an unrelated hurt animation
nearby within the window would count too.

**To verify:** set `hitConfirmation = true`, reload, watch the console.
`hit confirmation ready` = installed; `hit confirmation unavailable —
missing …` names which prerequisite failed. With it on, hit something with
Combo Counter enabled — should climb past 1x on consecutive landed hits,
reset after ~3s idle or when the player takes damage. **If the game crashes
shortly after enabling it, this is the first suspect** — turn it back off,
and the two hand-derived offsets above are where to start reading a crash
address.

## References

All four reference clients are copyleft-compatible with our AGPLv3 (GPLv3
material may be incorporated under GPLv3 §13; LGPL-3.0 likewise), and all
four are checked out locally:

| Project | License | Local path | Use it for |
|---|---|---|---|
| **[Latite](https://github.com/LatiteClient/Latite)** | GPLv3 | `reference/latite` | **Primary signature source.** Our sync tool reads it directly. Also the primary behavior reference — module logic, menu/cursor handling, packet hooking. |
| **[Flarial (dll-oss)](https://github.com/flarialmc/dll-oss)** | AGPLv3 | `reference/flarial` | Rendering internals, the larger module catalog, item rendering. |
| **[Selaura](https://github.com/selauraclient/selaura)** | GPLv3 | — | Small, clean framework. Good for scanning technique; no module catalog. |
| **[LeviLamina](https://github.com/liteldev/levilamina)** | LGPL-3.0 | — | Named class/struct/method headers. **BDS-only** — client types don't exist there. Names and shapes, never offsets. |

Attribution belongs in `docs/acknowledgements.md`; imported *data* belongs
only in `src/memory/Signatures.cpp`. **The pack at
`C:\Users\User\Desktop\Glacier v7\packs\...\hud_modules\` is for module
IDEAS only** — what widgets exist, not how to build them. Its JSON UI
techniques (`gc.lpr`, render controllers) are a pack-side mechanism with no
bearing on how the DLL should do anything; Latite/Flarial's actual C++ is
always the better reference when both exist for the same feature.

### Specific files that mattered this session

- `reference/latite/src/client/screen/Screen.cpp` /
  `ScreenManager.cpp` — the actual menu-open/close and cursor grab/release
  shape (`onUpdate` releases every tick, `exitCurrentScreen` grabs once,
  Escape always exits). Confirms Glacier's `applyCursorState` design.
- `reference/latite/src/client/memory/hook/hooks/PacketHooks.cpp` — the
  vtable-swap technique `sdk::HitConfirmation` reuses for one packet type.
- `reference/latite/src/mc/common/network/Packet.h` /
  `.../packet/ActorEventPacket.h` — the plain (non-`CLASS_FIELD`) field
  declarations `HitConfirmation`'s two hand-derived offsets come from.
- `reference/latite/src/client/feature/module/modules/hud/ArmorHUD.cpp` —
  chrome-less bare-icon rendering, the reference for Phase 9's visual pass.
- `reference/flarial/src/Client/Hook/Hooks/Render/DirectX/DX11/
  SwapchainHook_DX11.cpp` — the `OMSetRenderTargets(1, &rtv, nullptr)` that
  fixed Glacier's blank overlay.

### Libraries

- [MinHook](https://github.com/TsudaKageyu/minhook) — hooking engine (vcpkg).
- [EnTT](https://github.com/skypjack/entt) — Bedrock's ECS, for armor/component lookups.
- [kiero](https://github.com/Rebzzel/kiero) — the vtable-discovery technique `D3DHook` reimplements.

### This project

- Repo: <https://github.com/Glacier-Client-BE/Glacier-DLL>
- CI: <https://github.com/Glacier-Client-BE/Glacier-DLL/actions>

## Verification reality

CI proves compilation. It cannot prove anything works in-game — that needs
the user to inject on Windows and read the debug console. When you change
anything touching rendering, input, or signatures, say plainly that it's
unverified and name exactly what log line or on-screen behaviour would
confirm it. **When the user pastes back a real log, read every line** — the
item-render race and the missing-cursor bug were both found this way, not by
reasoning about the code in the abstract.

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
  usually either Glacier's own logic (not a game-memory read) or a path
  nobody thought to mark yet. Add one to any new path that calls into game
  code.
- **module+offset**, not a bare address — ASLR makes absolute addresses
  useless between runs. `Minecraft.Windows.exe+0x...` is a wrong game
  offset/signature; `Glacier.dll+0x...` is a bug in **our own code**, a
  different investigation entirely (see Open item #1 above for a live
  example — no symbols to map it yet).
- **near-null vs wild** — near-null is a missing null check (ours, usually).
  Wild means a wrong offset or a signature resolving to the wrong function.

Some reported exceptions are first-chance and handled normally downstream;
the last line before the log ends is the informative one. Reporting stops
after 5.

## Past incidents worth remembering (all fixed, keep the lessons)

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
respectively — Glacier read a vtable pointer as its `ClientInstance`,
derived a garbage `MinecraftGame` from it, and crashed constructing the
render context. The fix (corrected offsets to `0x08`/`0x10`, plus a
cross-check against the independently-resolved `ClientInstance`) is the
reason `itemIcons` is on by default now, and the reason `drawPending` will
disable itself with a named error instead of trusting a mismatch.

**ResizeBuffers.** `IDXGISwapChain::ResizeBuffers` fails with
`DXGI_ERROR_INVALID_CALL` if any reference to a back buffer is outstanding.
The game doesn't necessarily check that return value, so a failed resize
looks fine until DXGI dies later on a wild pointer, with crash activity
reading `(idle)` because no Glacier code is on the stack by then. Fix:
`Renderer::resize` drops its back-buffer reference unconditionally, before
the game's call runs — no size comparison, no zero check.
