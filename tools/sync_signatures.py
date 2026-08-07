#!/usr/bin/env python3
"""
Regenerate src/memory/Signatures.cpp from Latite's current signature data.

Why this exists
---------------
Glacier's signature table is version-locked to a Minecraft build. Latite is
actively maintained and updates its table when the game moves. Rather than
hand-copying values and discovering months later that Fullbright silently
stopped working, this script diffs upstream against our table and regenerates
it. CI runs it on a schedule and opens a pull request when anything changes.

Why CI and not the DLL
----------------------
An injected DLL that fetches signatures over the network at startup would mean
arbitrary remote data steering memory writes inside another process, plus a
client that breaks when offline or when a URL rots. Doing it in CI keeps the
shipped binary self-contained and puts a human in front of every change.

Safety properties
-----------------
* Mapping is EXPLICIT. Every value we import is named on both sides below. An
  upstream rename becomes a loud "not found" failure, never a silent wrong
  value — which is the failure mode that actually costs debugging days.
* The upstream commit SHA is recorded in the generated file, so any table can
  be traced back to the exact source it came from.
* Nothing is written unless every required entry was found.

Licensing
---------
Latite is GPLv3; Glacier is AGPLv3, which can incorporate it (GPLv3 §13). The
generated file carries the attribution header. Do not repoint this at a
non-copyleft-compatible source without revisiting docs/acknowledgements.md.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

REPO = "LatiteClient/Latite"
RAW = f"https://raw.githubusercontent.com/{REPO}"
API = f"https://api.github.com/repos/{REPO}"

ADDRESSES = "src/mc/Addresses.h"

REPO_ROOT = Path(__file__).resolve().parent.parent
OUTPUT = REPO_ROOT / "src" / "memory" / "Signatures.cpp"


# ── What we import, and what we call it ──────────────────────────────────────
# `upstream` is the string Latite labels the value with. `comment` is Glacier's
# own explanation, kept here so the generated file stays readable.

@dataclass(frozen=True)
class SigEntry:
    glacier: str
    upstream: str          # the name literal in Addresses.h
    comment: str = ""
    blank_before: bool = False


@dataclass(frozen=True)
class OffsetEntry:
    glacier: str
    source: str            # header path in the Latite tree
    field: str             # CLASS_FIELD name, or "raw:<regex>" for a literal
    comment: str = ""
    blank_before: bool = False
    literal: int | None = None   # set instead of source/field for values we pin


SIGNATURES: list[SigEntry] = [
    SigEntry(
        "Platform_GameCore", "Platform_GameCore",
        "The root of the object graph. A `mov [rip+disp], r15` store into a\n"
        "// global; the RIP-relative operand at +3 addresses the global itself.\n"
        "// Resolved in GameSDK via offsetFromSig(sig, 3).",
    ),
    SigEntry(
        "Options::getGamma", "Options::getGamma",
        "Hooked by Fullbright to return an override brightness. The trailing\n"
        "// immediate is the option index and it moves between builds, which is\n"
        "// what makes this pattern unusually version-sensitive. If Fullbright\n"
        "// breaks first after an update, look here.",
        blank_before=True,
    ),
    SigEntry(
        "RakPeer::GetAveragePing", "RakPeer::GetAveragePing",
        "Hooked read-only to cache the RTT the game itself queries. We never\n"
        "// call it: reaching into RakNet off the network thread isn't safe.",
        blank_before=True,
    ),
    SigEntry(
        "Dimension::getTimeOfDay", "Dimension::getTimeOfDay",
        "Hooked read-only for the Day Counter.",
        blank_before=True,
    ),
    SigEntry(
        "Actor::attack", "Actor::attack",
        "Observed read-only for the Reach display. `this` is the attacker, so\n"
        "// no GameMode->player indirection is needed.",
        blank_before=True,
    ),
]

OFFSETS: list[OffsetEntry] = [
    # Object graph. These three are read out of Latite's .cpp bodies rather
    # than CLASS_FIELD declarations, so they are pinned with a source note.
    OffsetEntry("WinMain::platformGameCore", "", "", literal=0x08,
                comment="*(Platform_GameCore sig deref) -> winMain; +0x08 -> Platform_GameCore*"),
    OffsetEntry("Platform_GameCore::minecraftGame", "", "", literal=0x18,
                comment="Platform_GameCore::getMinecraftGame"),
    OffsetEntry("MinecraftGame::clientInstances", "", "", literal=0x938,
                comment="map<uint8, shared_ptr<ClientInstance>>; entry 0 is the local one"),

    OffsetEntry("ClientInstance::minecraftGame",
                "src/mc/common/client/game/ClientInstance.h", "minecraftGame",
                blank_before=True),
    OffsetEntry("ClientInstance::levelRenderer",
                "src/mc/common/client/game/ClientInstance.h", "levelRenderer"),
    OffsetEntry("ClientInstance::packetSender",
                "src/mc/common/client/game/ClientInstance.h", "packetSender"),
    OffsetEntry("ClientInstance::guiData", "", "", literal=0x648,
                comment="ClientInstance::getGuiData"),
    OffsetEntry("ClientInstance::options", "", "", literal=0xD78,
                comment="ClientInstance::getOptions"),
    OffsetEntry("ClientInstance::getLocalPlayerVIndex", "", "", literal=0x1F,
                comment="vtable *index*, not a byte offset (ClientInstance::getLocalPlayer)"),

    OffsetEntry("Actor::entityContext",
                "src/mc/common/world/actor/Actor.h", "entityContext",
                comment="Embedded EntityContext — the route into the entt registry for\n"
                        "// component data such as armor. See src/sdk/EntityComponents.h.",
                blank_before=True),
    OffsetEntry("Actor::stateVector",
                "src/mc/common/world/actor/Actor.h", "stateVector",
                comment="Position lives in an ECS component; Actor caches a pointer to it."),
    OffsetEntry("StateVectorComponent::pos", "", "", literal=0x00,
                comment="IEntityComponent is an empty base, so pos sits at offset 0"),

    OffsetEntry("Player::supplies",
                "src/mc/common/world/actor/player/Player.h", "supplies",
                blank_before=True),
    OffsetEntry("PlayerInventory::selectedSlot",
                "src/mc/common/world/actor/player/PlayerInventory.h", "selectedSlot"),
    OffsetEntry("PlayerInventory::inventory",
                "src/mc/common/world/actor/player/PlayerInventory.h", "inventory"),
    OffsetEntry("Inventory::getItemVIndex", "", "", literal=7,
                comment="Inventory::getItem(int) vtable index — the game bounds-checks for us"),

    OffsetEntry("ItemStack::size", "", "", literal=0x98,
                comment="static_assert(sizeof(ItemStack) == 0x98) in Latite's ItemStack.h",
                blank_before=True),
    OffsetEntry("ItemStack::item", "", "", literal=0x08, comment="Item**; null == air"),
    OffsetEntry("ItemStack::auxValue", "", "", literal=0x20, comment="int16"),
    OffsetEntry("ItemStack::count", "", "", literal=0x22, comment="uint8"),
]


# ── Fetching ─────────────────────────────────────────────────────────────────

def fetch(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "glacier-sync"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read().decode("utf-8", errors="replace")


def upstream_sha(ref: str) -> str:
    try:
        data = json.loads(fetch(f"{API}/commits/{ref}"))
        return data.get("sha", "unknown")[:12]
    except Exception:
        return "unknown"


# ── Parsing ──────────────────────────────────────────────────────────────────

# Matches:  "48 83 EC ..."_sig, "Options::getGamma"
SIG_RE = re.compile(r'"([0-9A-Fa-f? ]+)"_sig\s*,\s*"([^"]+)"')

# Matches:  CLASS_FIELD(Type*, name, 0x1A0);
FIELD_RE = re.compile(r"CLASS_FIELD\(\s*[^,]+,\s*(\w+)\s*,\s*(0x[0-9A-Fa-f]+)\s*\)")


def parse_signatures(text: str) -> dict[str, str]:
    return {name: pattern.strip() for pattern, name in SIG_RE.findall(text)}


def parse_fields(text: str) -> dict[str, int]:
    return {name: int(value, 16) for name, value in FIELD_RE.findall(text)}


# ── Generation ───────────────────────────────────────────────────────────────

HEADER = '''\
// ─────────────────────────────────────────────────────────────────────────────
//  THIRD-PARTY-DERIVED DATA — READ docs/acknowledgements.md BEFORE EDITING
//
//  GENERATED FILE. Do not edit by hand; your changes will be overwritten.
//  Regenerate with:  python tools/sync_signatures.py
//
//  Source: Latite ({repo}) @ {sha} — GPLv3
//  Also informed by Flarial (dll-oss) — AGPLv3
//
//  These AOB patterns and struct offsets were NOT independently derived. This
//  single file is the reason Glacier is licensed AGPLv3 rather than
//  permissively; AGPLv3 absorbs both upstreams cleanly (GPLv3 material may be
//  incorporated under GPLv3 §13).
//
//  Two rules follow, and they are why this file is separate from
//  SignatureManager.cpp:
//
//    1. Keep imported data HERE. Never inline a pattern or a struct offset
//       anywhere else in the tree — everything build-specific is looked up
//       through SignatureManager by name.
//
//    2. Everything else in Glacier (the scanner, the registry, the hooks, the
//       module system, the UI) is Glacier's own code and carries no such
//       constraint.
//
//  ── TARGET BUILD: Minecraft: Bedrock Edition 1.26.40 ──
//
//  Declared in SignatureManager::kTarget{{Major,Minor,Patch}}. The attached
//  game's real version is read at startup and compared, so a mismatch is
//  logged rather than discovered through weird behaviour.
//
//  Values come from a project that actively supports this build, which is
//  meaningfully stronger evidence than a carried-forward table — but they have
//  still not been verified against a running client by this project. See
//  docs/signatures.md.
// ─────────────────────────────────────────────────────────────────────────────

#include "SignatureManager.h"

namespace glacier::memory {{

void SignatureManager::seedBedrock() {{
    if (m_seeded) return;
    m_seeded = true;

    // ── Signatures ──
'''

FOOTER = '''}

} // namespace glacier::memory
'''


def emit_comment(text: str, indent: str = "    ") -> list[str]:
    if not text:
        return []
    return [f"{indent}// {line}" if not line.startswith("//") else f"{indent}{line}"
            for line in text.split("\n")]


def generate(sigs: dict[str, str], fields: dict[str, dict[str, int]], sha: str) -> tuple[str, list[str]]:
    """Returns (file contents, list of problems)."""
    problems: list[str] = []
    out: list[str] = [HEADER.format(repo=REPO, sha=sha)]

    for entry in SIGNATURES:
        pattern = sigs.get(entry.upstream)
        if pattern is None:
            problems.append(f"signature '{entry.upstream}' not found upstream")
            continue
        if entry.blank_before:
            out.append("")
        out.extend(emit_comment(entry.comment))
        out.append(f'    addSignature("{entry.glacier}",')
        out.append(f'        "{pattern}");')

    out.append("")
    out.append("    // ── Offsets ──")

    for entry in OFFSETS:
        if entry.literal is not None:
            value = entry.literal
        else:
            table = fields.get(entry.source, {})
            if entry.field not in table:
                problems.append(
                    f"offset '{entry.field}' not found in {entry.source}")
                continue
            value = table[entry.field]

        if entry.blank_before:
            out.append("")
        out.extend(emit_comment(entry.comment))
        # Byte offsets read as hex; vtable indices are ordinals and read as
        # decimal. Mixing the two is how "index 0x1F" gets misread as a byte
        # offset by the next person to touch this.
        formatted = str(value) if "VIndex" in entry.glacier else f"0x{value:02X}"
        out.append(f'    addOffset("{entry.glacier}", {formatted});')

    out.append(FOOTER)
    return "\n".join(out), problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default="master", help="upstream git ref to read")
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if the generated file differs; write nothing")
    args = ap.parse_args()

    print(f"reading {REPO}@{args.ref}", file=sys.stderr)
    sha = upstream_sha(args.ref)

    try:
        sigs = parse_signatures(fetch(f"{RAW}/{args.ref}/{ADDRESSES}"))
    except Exception as exc:
        print(f"error: could not read {ADDRESSES}: {exc}", file=sys.stderr)
        return 2
    print(f"  {len(sigs)} signatures available upstream", file=sys.stderr)

    fields: dict[str, dict[str, int]] = {}
    for path in sorted({e.source for e in OFFSETS if e.source}):
        try:
            fields[path] = parse_fields(fetch(f"{RAW}/{args.ref}/{path}"))
        except Exception as exc:
            print(f"error: could not read {path}: {exc}", file=sys.stderr)
            return 2

    contents, problems = generate(sigs, fields, sha)

    if problems:
        # Loud, and refuses to write. A partial table is worse than a stale one:
        # a stale table fails visibly at scan time, a partial one silently
        # drops whichever feature lost its entry.
        print("\nRefusing to write — upstream no longer provides:", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        print("\nLatite likely renamed or restructured these. Update the mapping "
              "table in tools/sync_signatures.py.", file=sys.stderr)
        return 3

    existing = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""

    # Ignore the SHA line when comparing, or every upstream commit looks like a
    # signature change even when no value moved.
    def strip_sha(text: str) -> str:
        return re.sub(r"@ [0-9a-f]+ — GPLv3", "@ SHA — GPLv3", text)

    if strip_sha(existing) == strip_sha(contents):
        print("no signature changes", file=sys.stderr)
        return 0

    if args.check:
        print("signatures differ from upstream (run without --check to update)",
              file=sys.stderr)
        return 1

    OUTPUT.write_text(contents, encoding="utf-8", newline="\n")
    print(f"wrote {OUTPUT.relative_to(REPO_ROOT)} (upstream {sha})", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
