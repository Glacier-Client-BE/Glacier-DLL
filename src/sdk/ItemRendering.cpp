#include "ItemRendering.h"

#include "GameSDK.h"
#include "../core/Config.h"
#include "../hook/HookManager.h"
#include "../util/CrashHandler.h"
#include "../memory/Memory.h"
#include "../memory/SignatureManager.h"
#include "../util/Logger.h"

#include <array>
#include <cstring>

namespace glacier::sdk {

using memory::SignatureManager;

namespace {

// ── The game functions this borrows ──

// Constructor. Called on a zeroed buffer; upstream does the same and never
// calls the matching destructor, because the object owns nothing we allocated.
using BarcCtorFn = void*(__fastcall*)(void* self, void* screenContext,
                                      void* clientInstance, void* minecraftGame);
BarcCtorFn s_barcCtor = nullptr;

// The icon draw itself.
//
// The argument order below is NOT the order the function's C++ declaration
// implies — `ench`, `opacity`, `a9` and `scale` are shuffled relative to how
// upstream declares the wrapper. This order is the one upstream actually calls
// with, and it is the only one worth trusting. The trailing 17 is a constant
// upstream passes and does not explain.
using RenderGuiItemFn = void(__fastcall*)(void* itemRenderer, void* renderCtx, void* itemStack,
                                          int mode, float x, float y, bool enchantGlint,
                                          float opacity, float a9, float scale, int unknown);
RenderGuiItemFn s_renderGuiItem = nullptr;

// ── ScreenView::setupAndRender hook ──
using SetupAndRenderFn = void(__fastcall*)(void* screenView, void* uiRenderContext);
SetupAndRenderFn o_setupAndRender = nullptr;

void __fastcall hkSetupAndRender(void* screenView, void* uiRenderContext) {
    LOG_ONCE("ScreenView hook fired first time (ctx {:#x})",
             reinterpret_cast<std::uintptr_t>(uiRenderContext));

    // Call through first so our icons land on top of the game's own UI rather
    // than under the hotbar it is about to draw.
    {
        GLACIER_ACTIVITY("inside the original ScreenView::setupAndRender");
        o_setupAndRender(screenView, uiRenderContext);
    }
    LOG_ONCE("original ScreenView::setupAndRender returned");

    ItemRendering::get().drawPending(uiRenderContext);
}

// Upstream's ArmorHUD lays items out on a 48-pixel grid and draws them with a
// size modifier of 1. That is the only place upstream pins the relationship
// between its size modifier and actual pixels, so it is what this converts
// against: a request for a 48px icon is a size modifier of 1.
//
// If icons come out uniformly too large or too small, this constant is the
// first thing to change — nothing else in the path is a guess.
constexpr float kPixelsPerSizeUnit = 48.0f;

// Runs the part that actually enters game code, under a structured-exception
// guard. Returns false if it faulted.
//
// A guard is warranted here and almost nowhere else in Glacier: every value
// this path uses is imported and unverified, several of them are function
// addresses, and the cost of one being wrong is the player's game dying
// mid-session rather than a HUD looking wrong. Catching the fault lets the
// client switch the feature off and keep running.
//
// Split into its own function because MSVC will not compile __try in a
// function that needs C++ object unwinding, and deliberately takes only
// pointers and PODs so there is nothing to unwind here.
bool drawBatchGuarded(void* itemRenderer, void* context,
                      const ItemDraw* draws, std::size_t count, float frac) {
    __try {
        auto& sdk = GameSDK::get();
        for (std::size_t i = 0; i < count; ++i) {
            const ItemDraw& draw = draws[i];

            void* stack = (draw.ref.source == ItemRef::Source::Armor)
                        ? sdk.rawArmorStack(draw.ref.index)
                        : sdk.rawHotbarStack(draw.ref.index);
            if (!stack) continue;

            // Glacier lays HUDs out in swapchain pixels; the game's UI renderer
            // works in GUI units. One multiply bridges them.
            const float x = draw.x * frac;
            const float y = draw.y * frac;
            const float sizeUnits = (draw.size / kPixelsPerSizeUnit) * frac;

            s_renderGuiItem(itemRenderer, context, stack, /*mode*/ 0, x, y,
                            /*enchantGlint*/ false, draw.opacity, /*a9*/ 1.0f,
                            sizeUnits * 3.0f, /*unknown*/ 17);
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Same reasoning, for the constructor call.
bool constructContextGuarded(void* context, void* screenContext,
                             void* clientInstance, void* minecraftGame) {
    __try {
        s_barcCtor(context, screenContext, clientInstance, minecraftGame);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Same reasoning, for walking uiRenderContext -> clientInstance -> screenContext
// -> minecraftGame. These are raw pointer dereferences through imported offsets
// against a pointer the game handed us — no different in kind from the
// constructor call above, and just as capable of taking the whole game down if
// an offset is wrong. Pointers and PODs only, same constraint as the others.
bool readUiRenderContextGuarded(void* uiRenderContext, std::ptrdiff_t clientInstanceOffset,
                                std::ptrdiff_t screenContextOffset,
                                std::ptrdiff_t minecraftGameOffset,
                                void** outClientInstance, void** outScreenContext,
                                void** outMinecraftGame) {
    __try {
        *outClientInstance = memory::memberAt<void*>(uiRenderContext, clientInstanceOffset);
        *outScreenContext = memory::memberAt<void*>(uiRenderContext, screenContextOffset);
        if (!*outClientInstance || !*outScreenContext) return true;
        *outMinecraftGame = *reinterpret_cast<void**>(
            reinterpret_cast<std::uintptr_t>(*outClientInstance) + minecraftGameOffset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace

void ItemRendering::installHooks() {
    if (!m_enabled) {
        LOG_INFO("item icons turned off in {} — HUDs will draw everything except icons",
                 Config::path());
        return;
    }

    auto& sigs  = SignatureManager::get();
    auto& hooks = HookManager::get();

    s_barcCtor = reinterpret_cast<BarcCtorFn>(
        sigs.sig("BaseActorRenderContext::BaseActorRenderContext"));
    s_renderGuiItem = reinterpret_cast<RenderGuiItemFn>(
        sigs.sig("ItemRenderer::renderGuiItemNew"));

    const auto screenView = sigs.sig("ScreenView::setupAndRender");

    // Name what is missing rather than reporting a bare failure: these four
    // fail independently, and which one went is the whole diagnosis.
    if (!s_barcCtor || !s_renderGuiItem || !screenView) {
        LOG_WARN("item rendering unavailable — missing{}{}{}. HUDs that show item icons "
                 "will draw everything except the icons.",
                 s_barcCtor      ? "" : " BaseActorRenderContext::BaseActorRenderContext",
                 s_renderGuiItem ? "" : " ItemRenderer::renderGuiItemNew",
                 screenView      ? "" : " ScreenView::setupAndRender");
        return;
    }

    if (!hooks.create<SetupAndRenderFn>("ScreenView::setupAndRender",
                                        reinterpret_cast<void*>(screenView),
                                        &hkSetupAndRender, &o_setupAndRender)) {
        LOG_WARN("could not hook ScreenView::setupAndRender — no item icons");
        return;
    }

    m_available = true;
    LOG_INFO("item rendering ready");
}

void ItemRendering::submit(const ItemDraw& draw) {
    if (!m_available) return;
    std::scoped_lock lock(m_mutex);
    // A HUD that somehow asks for thousands of icons is a bug, not a workload.
    // Cap rather than let it grow without bound inside a render callback.
    if (m_building.size() >= 64) return;
    m_building.push_back(draw);
}

void ItemRendering::publish() {
    if (!m_available) return;
    std::scoped_lock lock(m_mutex);
    m_ready.swap(m_building);
    m_building.clear();
}

void ItemRendering::drawPending(void* uiRenderContext) {
    if (!m_available || !uiRenderContext) return;

    // Copy the batch out under the lock, then draw without holding it — the
    // draw calls re-enter the game and must not run with a lock the overlay
    // thread could be waiting on.
    std::vector<ItemDraw> batch;
    {
        std::scoped_lock lock(m_mutex);
        if (m_ready.empty()) return;
        batch = m_ready;
    }

    auto& sigs = SignatureManager::get();
    auto& sdk  = GameSDK::get();

    // Everything from here to the draw reads through imported offsets against a
    // pointer the game handed us. None of it was previously marked, so a fault
    // anywhere in it reported the useless "Glacier was: (idle)".
    GLACIER_ACTIVITY("reading MinecraftUIRenderContext for item drawing");

    void* clientInstance = nullptr;
    void* screenContext = nullptr;
    void* minecraftGame = nullptr;
    if (!readUiRenderContextGuarded(
            uiRenderContext, sigs.offset("MinecraftUIRenderContext::clientInstance"),
            sigs.offset("MinecraftUIRenderContext::screenContext"),
            sigs.offset("ClientInstance::minecraftGame"),
            &clientInstance, &screenContext, &minecraftGame)) {
        disableAfterFault("MinecraftUIRenderContext::clientInstance/screenContext/ClientInstance::minecraftGame");
        return;
    }
    LOG_ONCE("item draw context: cinst {:#x}, screenContext {:#x}",
             reinterpret_cast<std::uintptr_t>(clientInstance),
             reinterpret_cast<std::uintptr_t>(screenContext));
    if (!clientInstance || !screenContext) return;
    if (!minecraftGame) return;

    const float frac = sdk.guiScaleFrac();
    if (frac <= 0.0f) return;   // no usable GUI scale — see GameSDK::guiScaleFrac

    // The context is a plain buffer the game's constructor fills in. Its exact
    // size isn't reliably known — Latite's own struct comment admits its 0x500
    // pad was never measured, and that undersized guess is almost certainly
    // why this used to take the whole game down: the constructor wrote past a
    // buffer sized off it, on a thread_local (not stack) allocation, and SEH
    // around the call couldn't save a corruption like that. This buffer
    // matches Flarial's independently-padded 0x1000 instead — see the comment
    // on BaseActorRenderContext::size in Signatures.cpp. 16-byte aligned: the
    // type contains matrices, and handing the constructor an under-aligned
    // pointer would fault on the first aligned store.
    alignas(16) static thread_local std::uint8_t context[0x1000];

    const auto contextSize = static_cast<std::size_t>(sigs.offset("BaseActorRenderContext::size"));
    if (contextSize == 0 || contextSize > sizeof(context)) {
        LOG_WARN("BaseActorRenderContext::size is {} — outside what the item renderer "
                 "reserves ({}); not drawing icons", contextSize, sizeof(context));
        return;
    }

    // Zero the whole reserved buffer, not just contextSize bytes. Both
    // reference clients zero the type's full declared size before calling the
    // constructor (`memset(this, 0, sizeof(BaseActorRenderContext))`) — this
    // matches that rather than the partial zero this used to do.
    std::memset(context, 0, sizeof(context));

    LOG_ONCE("constructing BaseActorRenderContext ({} bytes)", contextSize);
    if (!constructContextGuarded(context, screenContext, clientInstance, minecraftGame)) {
        disableAfterFault("BaseActorRenderContext::BaseActorRenderContext");
        return;
    }

    void* itemRenderer = memory::memberAt<void*>(
        context, sigs.offset("BaseActorRenderContext::itemRenderer"));
    LOG_ONCE("BaseActorRenderContext built, itemRenderer {:#x}",
             reinterpret_cast<std::uintptr_t>(itemRenderer));
    if (!itemRenderer) return;

    {
        GLACIER_ACTIVITY("drawing item icons via ItemRenderer::renderGuiItemNew");
        if (!drawBatchGuarded(itemRenderer, context, batch.data(), batch.size(), frac)) {
            disableAfterFault("ItemRenderer::renderGuiItemNew");
            return;
        }
    }
    LOG_ONCE("first item icon batch drawn ({} icons)", batch.size());
}

void ItemRendering::disableAfterFault(const char* what) {
    m_available = false;
    LOG_ERROR("item icons disabled: '{}' faulted, which means its address or its "
              "arguments are wrong for this build. The rest of Glacier is unaffected. "
              "Leave 'itemIcons' off under [Glacier] until this is re-derived.", what);
}

} // namespace glacier::sdk
