#include "Addresses.h"
#include "../core/Logger.h"
#include <cstring>

//
// Signature patterns ported from
//   https://github.com/LatiteClient/Latite (src/mc/Addresses.h, master, 2026-05-03)
// All sigs target Minecraft.Windows.exe and may need refreshing on game updates.
//

namespace Glacier::sdk {

Addresses& Addresses::get() {
    static Addresses A;
    return A;
}

// --- Resolver helpers --------------------------------------------------------
//
// Latite's signature_store has three accessors: deref(N), ref(N), and the raw
// match. Implement each as a free helper that takes the matched address.
//
static std::uintptr_t resolveDeref(std::uintptr_t match, int off) {
    if (!match) return 0;
    std::int32_t rel = 0;
    std::memcpy(&rel, reinterpret_cast<const void*>(match + off), sizeof(rel));
    // x86-64 RIP-relative: rel32 added to the address of the *next* instruction
    // (= match + off + 4).
    return match + off + 4 + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
}

static std::uint32_t resolveRef(std::uintptr_t match, int off) {
    if (!match) return 0;
    std::uint32_t v = 0;
    std::memcpy(&v, reinterpret_cast<const void*>(match + off), sizeof(v));
    return v;
}

// --- Resolution macros (keep call sites readable) ----------------------------
#define RES_RES(field, pattern) \
    do { \
        auto _m = Util::patternScan(base_, pattern); \
        if (!_m) { Logger::get().warn("SDK", "miss: ", #field); ++misses_; } \
        else     { (field) = reinterpret_cast<void*>(_m); ++hits_; } \
    } while (0)

#define RES_DEREF(field, off, pattern) \
    do { \
        auto _m = Util::patternScan(base_, pattern); \
        if (!_m) { Logger::get().warn("SDK", "miss: ", #field); ++misses_; } \
        else     { (field) = reinterpret_cast<void*>(resolveDeref(_m, off)); ++hits_; } \
    } while (0)

#define RES_REF(field, off, pattern) \
    do { \
        auto _m = Util::patternScan(base_, pattern); \
        if (!_m) { Logger::get().warn("SDK", "miss: ", #field); ++misses_; } \
        else     { (field) = resolveRef(_m, off); ++hits_; } \
    } while (0)

void Addresses::resolve() {
    base_   = GetModuleHandleW(nullptr); // host EXE
    hits_   = 0;
    misses_ = 0;

    if (!base_) {
        Logger::get().error("SDK", "Failed to obtain host module handle");
        return;
    }

    // ---- Globals ----------------------------------------------------------
    RES_DEREF(misc.MinecraftGame,        3, "48 89 15 ? ? ? ? 49 8B 40");
    RES_DEREF(misc.GameCore,             3, "48 8B 05 ? ? ? ? 4C 8D 40");
    RES_DEREF(misc.ClickMap,             2, "89 0D ? ? ? ? 41 B7");
    RES_DEREF(misc.UIFillColorMaterial,  3, "48 8B 05 ? ? ? ? 4C 8B A5");
    RES_DEREF(misc.MouseInputVector,     3, "48 2B 05 ? ? ? ? 8B 0D");
    RES_DEREF(misc.KeyMap,               3, "48 8D 0D ? ? ? ? 48 8B 15 ? ? ? ? 48 8D 3C 99");
    RES_DEREF(misc.GpuInfo,              3, "48 8D 0D ? ? ? ? E8 ? ? ? ? 49 8B 07 49 8B CF 48 8B 40");
    RES_DEREF(misc.MceCommonMaterial,    3, "48 8D 15 ? ? ? ? E8 ? ? ? ? 90 49 8D 8E ? ? ? ? E8");

    // ---- Patch sites ------------------------------------------------------
    RES_RES(patch.ThirdPersonNametag,
        "0F 84 ? ? ? ? 49 8B 45 ? 49 8B CD 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 0F 85");

    // ---- Field offsets ----------------------------------------------------
    RES_REF(offset.cursorGrabbed, 2,
        "80 B9 ? ? ? ? ? 74 ? C6 81 ? ? ? ? ? 48 8D 4C 24 ? E8 ? ? ? ? 90");
    RES_REF(offset.playerOrigin,  4,
        "F3 0F 58 B3 ? ? ? ? 48 8B 8B");

    // ---- Components -------------------------------------------------------
    // The first 28 bytes of each pattern are identical; the trailing 4 bytes
    // are the component-hash discriminator.
    RES_RES(component.MoveInput,
        "4C 8B 41 48 4C 8B D1 48 8B 41 50 8B 12 49 2B C0 48 C1 F8 03 48 FF C8 25 2E CD 8B 46");
    RES_RES(component.ActorRuntimeID,
        "4C 8B 41 48 4C 8B D1 48 8B 41 50 8B 12 49 2B C0 48 C1 F8 03 48 FF C8 25 14 14 A1 3C");
    RES_RES(component.ActorType,
        "4C 8B 41 48 4C 8B D1 48 8B 41 50 8B 12 49 2B C0 48 C1 F8 03 48 FF C8 25 14 AD F3 51");
    RES_RES(component.Attributes,
        "48 89 5C 24 ? 57 48 83 EC ? 8B 79 10 BA 44 94 B2 B6");
    RES_RES(component.ActorEquipment,
        "4C 8B 41 48 4C 8B D1 48 8B 41 50 8B 12 49 2B C0 48 C1 F8 03 48 FF C8 25 36 48 C4 71");
    RES_RES(component.ActorDataFlags,
        "4C 8B 41 48 4C 8B D1 48 8B 41 50 8B 12 49 2B C0 48 C1 F8 03 48 FF C8 25 76 59 47 33");

    // ---- VTables ----------------------------------------------------------
    RES_DEREF(vtable.TextPacket, 3,
        "48 8D 3D ? ? ? ? 48 89 3B 48 8D 4B ? 48 8B D0");
    RES_DEREF(vtable.CommandRequestPacket, 3,
        "48 8D 05 ? ? ? ? 48 89 01 48 8D 59 ? 48 89 5C 24 ? 0F 57 C0 0F 11 03 48 89 6B ? 48 89 6B");
    RES_DEREF(vtable.Level, 3,
        "48 8D 05 ? ? ? ? 48 89 07 48 8D 05 ? ? ? ? 48 89 47 ? 48 8D 05 ? ? ? ? 48 89 47 ? 4C 8D B6");
    RES_DEREF(vtable.SetTitlePacket, 3,
        "48 8D 3D ? ? ? ? 4C 3B F8");

    // ---- Functions --------------------------------------------------------
    RES_DEREF(fn.renderLevel, 1,
        "E8 ? ? ? ? 45 32 F6 48 8B 8E");
    RES_RES(fn.mainWindowProc,
        "40 53 56 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 ? 48 8B F9");
    RES_RES(fn.getGamma,
        "48 83 EC ? 48 8B 01 48 8D 54 ? ? 41 B8 36 00 00 00");
    RES_RES(fn.getPerspective,
        "48 83 EC ? 48 8B 01 48 8D 54 ? ? 41 B8 03 00 00 00");
    RES_RES(fn.getHideHand,
        "48 83 EC ? 48 8B 01 48 8D 54 ? ? 41 B8 A2 01 00 00");

    RES_RES(fn.grabCursor,
        "40 53 48 83 EC ? 48 8B 01 48 8B D9 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 74 ? 48 8B 8B ? ? ? ? 48 8B 01 48 8B 80 ? ? ? ? 48 83 C4 ? 5B 48 FF 25 ? ? ? ? 48 83 C4 ? 5B C3 40 53");
    RES_RES(fn.releaseCursor,
        "40 53 48 83 EC ? 48 8B 01 48 8B D9 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 74 ? 48 8B 8B ? ? ? ? 48 8B 01 48 8B 80 ? ? ? ? 48 83 C4 ? 5B 48 FF 25 ? ? ? ? 48 83 C4 ? 5B C3 48 89 5C 24");

    RES_DEREF(fn.tickLevel, 1,
        "E8 ? ? ? ? 48 8B 4B ? 48 85 C9 74 ? 48 8B 41 ? 48 83 C1 ? 48 8B 40");
    RES_RES(fn.sendChatMessage,
        "48 89 5C 24 ? 48 89 74 24 ? 55 57 41 54 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 4C 8B FA 4C 8B F1 45 33 E4 48 8B 49");
    RES_RES(fn.onDeviceLost,
        "48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC ? 4C 8B F9 33 F6");
    RES_RES(fn.handleMouseInput,
        "48 8B C4 48 89 58 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D 68 ? 48 81 EC ? ? ? ? 0F 29 70 ? 0F 29 78 ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 4C 89 45");
    RES_RES(fn.getOverlayColor,
        "40 53 55 56 57 48 83 EC ? 49 8B 78");
    RES_DEREF(fn.setupAndRender, 1,
        "E8 ? ? ? ? 48 8B 4B ? 48 85 C9 74 ? 48 8B 01 48 8B D6");
    RES_DEREF(fn.updateGame, 1,
        "E8 ? ? ? ? BA ? ? ? ? 48 8B 8E ? ? ? ? E8 ? ? ? ? 48 8B BE");

    RES_RES(fn.getAveragePing,
        "48 8B C4 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? 0F 10 4A ? 4C 8B 1A 4C 3B 1D ? ? ? ? 0F 10 42 ? 48 89 58 ? 48 8B D9 0F 10 52 ? 0F 10 5A ? 0F 10 62 ? 0F 10 6A ? 0F 29 70 ? 0F 10 72 ? 0F 29 78 ? 0F B7 82 ? ? ? ? 0F 10 BA ? ? ? ? 66 89 45 ? 0F B7 82 ? ? ? ? 66 89 45 ? 0F 11 4C 24 ? 74 ? 44 8B 49");
    RES_RES(fn.applyTurnDelta,
        "48 8B C4 48 89 58 ? 48 89 70 ? 48 89 78 ? 55 41 54 41 55 41 56 41 57 48 8D 68 ? 48 81 EC ? ? ? ? 0F 29 70 ? 0F 29 78 ? 44 0F 29 40 ? 44 0F 29 48 ? 44 0F 29 50 ? 44 0F 29 98 ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 4C 8B EA");
    RES_RES(fn.tickBaseInput,
        "4C 8B DC 53 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 ? 48 8B 9C 24");
    RES_RES(fn.cameraViewBob,
        "40 53 48 81 ec ? ? ? ? 48 8b 05 ? ? ? ? 48 33 c4 48 89 44 24 ? 49 8b 00");
    RES_RES(fn.getHoverName,
        "48 89 5C 24 ? 48 89 54 24 ? 55 56 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 48 8B FA");

    RES_RES(fn.tessVertex,
        "48 8B C4 48 89 58 ? 48 89 68 ? 56 57 41 54 41 56 41 57 48 81 EC ? ? ? ? 0F 29 70 ? 0F 29 78 ? 44 0F 29 40 ? 44 0F 29 48 ? 44 0F 28 C3");
    RES_RES(fn.tessBegin,
        "40 57 48 83 EC ? 80 B9 ? ? ? ? ? 4C 8B D1");
    RES_RES(fn.tessColor,
        "F3 0F 10 42 ? 41 B8");
    RES_DEREF(fn.renderMeshImmediately, 1,
        "E8 ? ? ? ? 41 C6 45 ? ? F3 41 0F 10 5E");
    RES_RES(fn.baseActorRenderCtx,
        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 4C 24 ? 57 48 83 EC ? 49 8B F8 48 8B DA 48 8B F1 48 8D 05 ? ? ? ? 48 89 01 33 ED");
    RES_DEREF(fn.renderGuiItemNew, 1,
        "E8 ? ? ? ? 4C 8D 4C 24 ? 4C 8D 44 24 ? 48 8B D5 E8 ? ? ? ? 80 BF");
    RES_DEREF(fn.baseAttributeMapInstance, 1,
        "E8 ? ? ? ? 4C 8B 76 ? 48 8B 44 24");
    RES_RES(fn.uiControlGetPosition,
        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 0F 29 74 24 ? 0F 29 7C 24 ? 48 8B F9 F6 41");
    RES_RES(fn.getPrimaryClient,
        "4C 8B 81 ? ? ? ? 49 8B D0 49 8B 48");
    RES_RES(fn.renderActor,
        "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 0F 29 B4 24 ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 4D 8B E1 49 8B F0 4C 8B FA");
    RES_DEREF(fn.renderOutlineSelection, 1,
        "E8 ? ? ? ? EB ? 0F B6 44 24 ? 88 44 24 ? C6 44 24");

    RES_RES(fn.getTimeOfDay,
        "44 8B C2 B8 ? ? ? ? F7 EA");
    RES_RES(fn.tickDimension,
        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 48 8B F9 48 8B 99");
    RES_RES(fn.getSkyColor,
        "41 0F 10 08 48 8B C2 0F 28 D3");
    RES_RES(fn.getDamageValue,
        "40 53 48 83 EC ? 48 8B 51 ? 33 DB");
    RES_RES(fn.createPacket,
        "48 89 5C 24 ? 48 89 74 24 ? 55 57 41 56 48 8B EC 48 83 EC ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 48 8B F9");
    RES_RES(fn.attack,
        "48 8B C4 48 89 58 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 70 ? 0F 29 78 ? 44 0F 29 40 ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 4C 89 4C 24");
    RES_RES(fn.addMessage,
        "40 53 55 56 57 41 54 41 56 41 57 48 83 EC ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 ? 45 8B F8");
    RES_DEREF(fn.getArmor, 1,
        "e8 ? ? ? ? 48 85 c0 0f 84 ? ? ? ? 48 8b 08 48 8b 01 ba ? ? ? ? 48 8b 40 ? ff 15 ? ? ? ? 48 8b f8 80 78 ? ? 0f 84 ? ? ? ? 48 8b 40 ? 48 85 c0 0f 84");
    RES_RES(fn.setNameTag,
        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B FA 48 8B D9 48 8B 89 ? ? ? ? 48 85 C9");
    RES_RES(fn.updatePlayerCamera,
        "48 89 5C 24 ? 55 56 57 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 49 8B 58");
    RES_RES(fn.onUri,
        "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 0F 29 B4 24 ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 48 8B F2 4C 8B F1 33 DB");
    RES_RES(fn.displayClientMessage,
        "40 55 53 56 57 41 56 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 41 0F B6 ? 49 8B D8");
    RES_DEREF(fn.renderText, 1,
        "E8 ? ? ? ? 48 83 C3 ? 48 3B DE 75 ? 48 8B 5C 24 ? 48 8B 6C 24 ? 48 8B 74 24 ? 48 83 C4 ? 41 5F");
    RES_RES(fn.releaseMouse,
        "40 53 48 83 EC ? 48 8B D9 B9 ? ? ? ? FF 15 ? ? ? ? 85 C0");

    Logger::get().info("SDK", "Sigs resolved: ", hits_, " hits, ", misses_, " misses");
    ready_ = true;
}

#undef RES_RES
#undef RES_DEREF
#undef RES_REF

} // namespace Glacier::sdk
