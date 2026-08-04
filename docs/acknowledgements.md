# Acknowledgements

Glacier is an independent, from-scratch implementation. No source code, byte
signatures, or struct offsets are copied from any other project — Glacier's
own signatures/offsets are reverse-engineered independently against the
Minecraft Bedrock build it targets.

Two other open-source Bedrock clients informed Glacier's low-level
*architecture* (not its code):

- **[Latite](https://github.com/LatiteClient/Latite)** (GPLv3)
- **[Flarial (dll-oss)](https://github.com/flarialmc/dll-oss)** (AGPLv3)

Both are strong-copyleft licensed; Glacier is MIT. Because of that, only
*techniques* are reimplemented here, never transcribed source. This file
tracks which technique came from which project as modules land.

| Technique | Observed in | Glacier's (independent) implementation |
|---|---|---|
| Central signature/offset registry keyed by name | Flarial's `SignatureAndOffsetManager` | `memory::SignatureManager` |
| `Module` → `HudModule` hierarchy (position/scale baked into the HUD base) | Latite's `Module`/`HUDModule` | `Module` / `HudModule` |
| Present-hook vtable discovery via a throwaway D3D device (kiero-style), plus a rehook-from-live-swapchain defensive pass | Flarial's `SwapchainHook` (kiero + rehook) | `hook::D3DHook` |
| Tracked hook manager (every install recorded so shutdown removes all in one pass) | both projects' hook managers | `hook::HookManager` |
| Self-registering module list (`GLACIER_MODULE` macro → factory registry) | Flarial's (mostly-unused) `ModuleRegistry`/`REGISTER_MODULE` | `module::ModuleRegistry` |
| Typed pub/sub event bus decoupling hooks from modules | Latite's `Eventing`, Flarial's `EventManager` | `core::EventBus` |

Entries are added here as the corresponding Glacier subsystem is implemented,
not written speculatively ahead of the code.
