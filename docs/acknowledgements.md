# Acknowledgements

Glacier draws on two other open-source Minecraft: Bedrock clients:

- **[Latite](https://github.com/LatiteClient/Latite)** — GPLv3
- **[Flarial (dll-oss)](https://github.com/flarialmc/dll-oss)** — AGPLv3

It draws on them in two very different ways, and the distinction matters
legally, so it is worth stating precisely.

## 1. Techniques — reimplemented, not copied

Glacier's infrastructure is its own code. The following architectural ideas
were observed in those projects and independently reimplemented here. No source
was transcribed.

| Technique | Observed in | Glacier's implementation |
|---|---|---|
| Central signature/offset registry keyed by name | Flarial's `SignatureAndOffsetManager` | `memory::SignatureManager` |
| Parallel signature scan across hardware threads | Flarial's work-stealing scan | `SignatureManager::scanAll` |
| `Module` → `HudModule` hierarchy (position/scale baked into the HUD base) | Latite's `Module`/`HUDModule` | `Module` / `HudModule` |
| Present-hook vtable discovery via a throwaway D3D device (kiero-style), plus a rehook-from-live-swapchain defensive pass | Flarial's `SwapchainHook` | `D3DHook` |
| Tracked hook manager (every install recorded so shutdown removes all in one pass) | both projects' hook managers | `HookManager` |
| Self-registering module list (macro → factory registry) | Flarial's `ModuleRegistry`/`REGISTER_MODULE` | `ModuleRegistry` / `GLACIER_MODULE` |
| Capturing `ClientInstance` live from an `update` hook instead of chasing a global | Flarial | `GameSDK::setClientInstance` |
| Decoding `getLocalPlayer`'s vtable index from a call-site signature | Flarial's `getLocalPlayerIndex` | `GameSDK::resolve` |
| Fullbright driven by hooking `Options::getGamma` rather than poking the field | both | `GameSDK::setGammaOverride` |

## 2. Signature and offset data — imported, and the reason for the license

`src/memory/Signatures.cpp` is different. The AOB byte patterns and struct
offsets in that file are **derived from the reverse-engineering work published
in Flarial (AGPLv3) and Latite (GPLv3)**. They were not independently derived.

Getting usable signatures has exactly three paths:

1. **Derive them independently.** Requires reverse-engineering a live Bedrock
   binary in IDA/Ghidra against each new game build. This is the only route to
   a permissive license, and it is real, ongoing work.
2. **Ship none**, and require users to supply their own at runtime. This does
   not solve the problem; it relocates it onto every user.
3. **Reuse the existing published tables** and adopt a compatible license.

Glacier took path 3.

### Consequence: Glacier is AGPLv3

AGPLv3 absorbs both references cleanly — GPLv3 material may be incorporated
into an AGPLv3 work under GPLv3 §13 — so data from both projects is
unambiguously usable here with attribution.

This is a real, effectively one-way tradeoff, and it should be understood
before contributing:

- Anyone distributing a modified Glacier must publish their corresponding
  source. Glacier cannot be embedded in a closed-source product.
- Once outside contributions land under AGPLv3, moving to a permissive license
  would require every contributor's agreement.
- Whether short byte patterns are independently copyrightable at all is a
  genuinely unsettled legal question. Adopting AGPLv3 **sidesteps** that
  question rather than answering it. That is the point — it is the
  conservative choice, not a claim about the law. None of this is legal advice.

### Containment

All third-party-derived data is deliberately isolated in **one file**:

```
src/memory/Signatures.cpp
```

Nothing else in the tree carries imported data. If independently derived
signatures ever replace that file's contents — see
[`reverse-engineering.md`](reverse-engineering.md) for the runbook — it is the
only thing standing between Glacier and a return to a permissive license.

Keep it that way: **do not scatter offsets or patterns into other files.**
Everything build-specific goes through `SignatureManager` by name.
