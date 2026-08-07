# Re-deriving signatures

When Minecraft: Bedrock updates, the compiler rearranges code and some byte
patterns stop matching. This is the runbook for fixing that. It also happens to
be the route to replacing the imported tables with independently derived ones —
the single change that would free Glacier from its AGPLv3 obligation (see
[acknowledgements.md](acknowledgements.md)).

## 1. Confirm it's a signature problem

Attach the client and read the debug console. `SignatureManager::scanAll()`
prints every failure by name:

```
[Glacier][warn] signature not found: Options::getGamma
[Glacier][info] resolved 2/3 signatures across 16 thread(s)
[Glacier][warn] 1 signature(s) unresolved — see docs/reverse-engineering.md
```

If a **blocking** signature is missing, the attach aborts cleanly:

```
[Glacier][error] ClientInstance::update not resolved — aborting attach
```

That is the intended failure mode. A crash instead means something else is
wrong — don't start rewriting patterns.

Two failure shapes worth distinguishing:

- **Everything unresolved** → you are probably scanning the wrong module, or the
  game is packed/protected. Check `moduleRange()` returned a sane base.
- **Some unresolved** → normal post-update drift. Continue below.

## 2. Useful external references

| Project | License | What it's good for | Caveat |
|---|---|---|---|
| [Flarial (dll-oss)](https://github.com/flarialmc/dll-oss) | AGPLv3 | Client-side AOB patterns and offsets, per-version tables | Already the source of Glacier's imported data |
| [Latite](https://github.com/LatiteClient/Latite) | GPLv3 | Client architecture, some overlapping signatures | Same |
| [LeviLamina](https://github.com/liteldev/levilamina) | LGPL-3.0 | Extensive **named** class/struct/method headers for Bedrock, generated from symbol data — often the fastest way to learn a struct's real field layout and a function's true signature | Targets **Bedrock Dedicated Server**, not the client |

The LeviLamina caveat matters. BDS and the client are built from a shared
codebase, so shared engine types (`Actor`, `Level`, `ItemStack`, `Player`)
usually match, and its headers are excellent for confirming what a field *is*.
But client-only types (`ClientInstance`, `MinecraftGame`, `Options`,
`LevelRenderer`, anything render- or input-related) do not exist in BDS at all,
and **field offsets can still differ** even for shared types because the two
binaries are compiled with different feature sets. Treat it as a source of
names and shapes to confirm against the client binary — never as a source of
offsets to paste in.

Note also that BDS ships with a symbol/PDB story the client does not, which is
precisely why the names are available there and not here.

## 3. Get the binary and a disassembler

`Minecraft.Windows.exe` lives under
`C:\Program Files\WindowsApps\Microsoft.MinecraftUWP_*\`. The directory is ACL'd;
copy the exe out rather than fighting the permissions.

Use IDA or Ghidra. Give it time to finish auto-analysis — the binary is large
(~100MB) and partial analysis produces misleading cross-references.

## 4. Locate the function

Ordered by how well they survive updates:

1. **String cross-reference.** The most durable anchor. Find a string the
   function uses (or that its caller uses) and follow the xref. Options/settings
   code is especially easy to reach this way — setting names are string
   literals.
2. **Vtable position.** For virtual functions, find the class vtable and count
   slots. Slot order is stable far more often than code layout.
3. **Call-graph shape.** Find a function you already have a working signature
   for and walk to its neighbours.
4. **Old pattern, loosened.** Take the previous pattern, replace the bytes most
   likely to have changed (displacements, immediates) with wildcards, and see
   what it matches now. Fastest when the drift is small.

## 5. Build the pattern

Copy the first 15–30 bytes of the function prologue and wildcard every byte that
could legitimately change:

- **Wildcard:** relative call/jump displacements (`E8`/`E9` operands), RIP-relative
  displacements, stack-frame sizes, and any absolute immediate.
- **Keep:** opcodes, register encodings, and the overall instruction shape.

Glacier accepts IDA-style patterns — `??` or `?` for a wildcard byte:

```
48 89 5C 24 ? 57 48 83 EC ? 48 8B 81 ? ? ? ?
```

### Verify uniqueness

**This is the step people skip, and it is the one that causes crashes.** A
pattern that matches two places will silently resolve to the wrong one.
`findSignature` returns the *first* match with no ambiguity warning. Search the
whole `.text` section in your disassembler and confirm exactly one hit before
committing.

Rule of thumb: a pattern under ~10 concrete (non-wildcard) bytes is almost
certainly not unique.

## 6. Offsets

Struct offsets move more often than code. Derive them from the instruction that
touches the field:

```asm
mov rax, [rcx+1A0h]      ; ClientInstance::minecraftGame = 0x1A0
```

Sanity-check the result at runtime before trusting it — a wrong offset reads
adjacent memory and produces plausible-looking garbage rather than an obvious
failure.

## 7. Update and record

1. Edit **only** [`src/memory/Signatures.cpp`](../src/memory/Signatures.cpp).
   Never inline a pattern or offset elsewhere — the whole containment argument
   in [acknowledgements.md](acknowledgements.md) depends on this.
2. Update the entry's row in [signatures.md](signatures.md), including its
   status and, if you derived it yourself, its provenance.
3. Rebuild, inject, and confirm the console reports it resolved **and** that the
   dependent feature actually works. Only then mark it ✅ confirmed.

## Independently derived signatures

If you derive a pattern yourself without consulting Flarial or Latite, say so in
the `signatures.md` provenance column. If every entry in `Signatures.cpp` ever
reaches that state, the file can be relicensed and Glacier's AGPLv3 constraint
can be revisited. Partial progress still helps — it shrinks the imported
surface — but the license cannot change until the last imported entry is gone.
