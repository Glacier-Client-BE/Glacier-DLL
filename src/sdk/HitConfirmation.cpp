#include "HitConfirmation.h"

#include "../hook/HookManager.h"
#include "../memory/SignatureManager.h"
#include "../util/CrashHandler.h"
#include "../util/Logger.h"

#include <Windows.h>
#include <cstdint>

namespace glacier::sdk {

using memory::SignatureManager;

namespace {

// The derivation for Packet::handler and ActorEventPacket::eventID — both
// hand-computed from Packet.h's declared layout, neither backed by a
// CLASS_FIELD or a Flarial cross-check — lives as a comment on their
// OffsetEntry in tools/sync_signatures.py, next to every other offset this
// build resolves by name. Resolved once in installHooks(); everything below
// reads them from these, never a local literal, so the one place that says
// "here is where these numbers come from" stays authoritative.
std::ptrdiff_t s_packetHandlerOffset = 0;
std::ptrdiff_t s_eventIdOffset = 0;

// Protocol constants (reference/latite's Packet.h / ActorEventPacket.h,
// matching bedrock.dev's public documentation) — stable wire-format values,
// not memory offsets, so they carry none of the risk above.
constexpr int kPacketIdActorEvent = 0x1B;
constexpr std::uint8_t kActorEventHurtAnimation = 2;

using CreatePacketFn = void*(__fastcall*)(void* outSharedPtr, int packetId);
CreatePacketFn s_createPacket = nullptr;

// Latite's own declared signature for what this vtable slot actually is —
// discovered and shipped by them, not guessed here.
using HandlePacketFn = void(__fastcall*)(void* instance, void* networkIdentifier,
                                         void* netEventCallback, void* packetRef);
HandlePacketFn o_handlePacket = nullptr;

bool readEventIdGuarded(void* packet, std::uint8_t* outEventId) {
    __try {
        *outEventId = *reinterpret_cast<std::uint8_t*>(
            reinterpret_cast<std::uintptr_t>(packet) + s_eventIdOffset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __fastcall hkHandlePacket(void* instance, void* networkIdentifier,
                               void* netEventCallback, void* packetRef) {
    GLACIER_ACTIVITY("reading ActorEventPacket::eventID");

    // packetRef is a std::shared_ptr<Packet>& — a plain reference, i.e. just
    // the address of a { Packet* ptr; ControlBlock* rep; } pair. The first
    // field is the object pointer; this is the one piece of the shared_ptr's
    // layout this file relies on, and it is Microsoft STL's own stable ABI,
    // not anything game-specific.
    if (void* packet = packetRef ? *reinterpret_cast<void**>(packetRef) : nullptr) {
        std::uint8_t eventId = 0;
        if (readEventIdGuarded(packet, &eventId) && eventId == kActorEventHurtAnimation) {
            HitConfirmation::get().recordHurtAnimation();
        }
    }

    o_handlePacket(instance, networkIdentifier, netEventCallback, packetRef);
}

// Constructs a throwaway ActorEventPacket purely to read its dispatcher
// vtable — vtables are per-class, not per-instance, so any valid instance's
// vtable is the one every REAL packet of this type will dispatch through
// for the rest of the session. The throwaway packet itself is never touched
// again after this.
//
// createPacket returns std::shared_ptr<Packet> BY VALUE, which on MSVC x64
// means a hidden caller-owned return-slot pointer ahead of the real
// (packetId) argument — the standard ABI for any type with a non-trivial
// destructor, not a game-specific guess. The resulting shared_ptr is
// deliberately never destroyed: modelling its destructor correctly would
// mean guessing at an atomic refcount decrement and a virtual deleter call
// with nothing to check the guess against. Leaking one small allocation once
// per session is a far smaller, better-understood cost than getting that
// wrong.
bool bootstrapDispatcherVTable(void*** outVTable) {
    if (!s_createPacket) return false;

    alignas(16) std::uint8_t sharedPtrStorage[16]{};

    __try {
        s_createPacket(sharedPtrStorage, kPacketIdActorEvent);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    void* packet = *reinterpret_cast<void**>(sharedPtrStorage);
    if (!packet) return false;

    __try {
        void** handlerField = *reinterpret_cast<void***>(
            reinterpret_cast<std::uint8_t*>(packet) + s_packetHandlerOffset);
        if (!handlerField) return false;
        *outVTable = *reinterpret_cast<void***>(handlerField);
        return *outVTable != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace

void HitConfirmation::installHooks() {
    if (!m_enabled) {
        LOG_INFO("hit confirmation turned off in config — Combo Counter will not "
                 "confirm hits");
        return;
    }

    auto& sigs = SignatureManager::get();

    const auto createPacketAddr = sigs.sig("MinecraftPackets::createPacket");
    s_packetHandlerOffset = sigs.offset("Packet::handler");
    s_eventIdOffset = sigs.offset("ActorEventPacket::eventID");

    if (!createPacketAddr || s_packetHandlerOffset == 0 || s_eventIdOffset == 0) {
        LOG_WARN("hit confirmation unavailable — missing{}{}{}. Combo Counter will "
                 "count swings, not confirmed hits.",
                 createPacketAddr        ? "" : " MinecraftPackets::createPacket",
                 s_packetHandlerOffset   ? "" : " Packet::handler",
                 s_eventIdOffset         ? "" : " ActorEventPacket::eventID");
        return;
    }
    s_createPacket = reinterpret_cast<CreatePacketFn>(createPacketAddr);

    void** vtable = nullptr;
    if (!bootstrapDispatcherVTable(&vtable) || !vtable) {
        LOG_WARN("hit confirmation unavailable — could not construct a throwaway "
                 "ActorEventPacket to read its dispatcher vtable");
        return;
    }

    // Slot 1: the first virtual after the destructor, on the dispatcher
    // object Packet::handler points to. Latite uses this same index across
    // every one of ~140 packet types it hooks this way, which is itself the
    // closest thing to cross-validation this particular number has.
    o_handlePacket = reinterpret_cast<HandlePacketFn>(
        HookManager::get().hookVTable(vtable, 1, reinterpret_cast<void*>(&hkHandlePacket)));

    m_available.store(true, std::memory_order_relaxed);
    LOG_INFO("hit confirmation ready");
}

void HitConfirmation::recordHurtAnimation() {
    m_stamp.store(GetTickCount64(), std::memory_order_relaxed);
}

std::optional<std::uint64_t> HitConfirmation::lastHurtAnimationAge() const {
    const auto stamp = m_stamp.load(std::memory_order_relaxed);
    if (stamp == 0) return std::nullopt;
    return GetTickCount64() - stamp;
}

} // namespace glacier::sdk
