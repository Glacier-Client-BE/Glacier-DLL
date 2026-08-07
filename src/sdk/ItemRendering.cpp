#include "ItemRendering.h"

#include "GameSDK.h"
#include "../hook/HookManager.h"
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
    // Call through first so our icons land on top of the game's own UI rather
    // than under the hotbar it is about to draw.
    o_setupAndRender(screenView, uiRenderContext);
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

} // namespace

void ItemRendering::installHooks() {
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

    void* clientInstance = memory::memberAt<void*>(
        uiRenderContext, sigs.offset("MinecraftUIRenderContext::clientInstance"));
    void* screenContext = memory::memberAt<void*>(
        uiRenderContext, sigs.offset("MinecraftUIRenderContext::screenContext"));
    if (!clientInstance || !screenContext) return;

    void* minecraftGame = *reinterpret_cast<void**>(
        reinterpret_cast<std::uintptr_t>(clientInstance) + sigs.offset("ClientInstance::minecraftGame"));
    if (!minecraftGame) return;

    const float frac = sdk.guiScaleFrac();
    if (frac <= 0.0f) return;   // no usable GUI scale — see GameSDK::guiScaleFrac

    // The context is a plain buffer the game's constructor fills in. Its exact
    // size isn't known (upstream declares a deliberate over-estimate), so this
    // holds a larger fixed buffer and zeroes only what the table claims, which
    // is what upstream does. Stack rather than heap, and 16-byte aligned: the
    // type contains matrices, and handing the constructor an under-aligned
    // pointer would fault on the first aligned store.
    alignas(16) static thread_local std::uint8_t context[0x800];

    const auto contextSize = static_cast<std::size_t>(sigs.offset("BaseActorRenderContext::size"));
    if (contextSize == 0 || contextSize > sizeof(context)) {
        LOG_WARN("BaseActorRenderContext::size is {} — outside what the item renderer "
                 "reserves ({}); not drawing icons", contextSize, sizeof(context));
        return;
    }

    std::memset(context, 0, contextSize);
    s_barcCtor(context, screenContext, clientInstance, minecraftGame);

    void* itemRenderer = memory::memberAt<void*>(
        context, sigs.offset("BaseActorRenderContext::itemRenderer"));
    if (!itemRenderer) return;

    for (const auto& draw : batch) {
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
}

} // namespace glacier::sdk
