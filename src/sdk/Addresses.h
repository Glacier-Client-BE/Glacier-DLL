#pragma once
//
// Game-version-specific signature table.
//
// THIS FILE IS THE ONLY PLACE THAT NEEDS TO CHANGE WHEN THE GAME UPDATES.
// Everything else in the SDK works through the pointers resolved here.
//
// Patterns and resolver semantics are ported from
// https://github.com/LatiteClient/Latite (src/mc/Addresses.h, master,
// fetched 2026-05-03). Three resolver shapes are used:
//
//   res        - return the match address as-is
//                  (function bodies, jcc patch sites)
//   deref(N)   - read the rel32 at match+N, return absolute pointer
//                  (RIP-relative globals, vtable LEAs, E8 call targets)
//   ref(N)     - return the 4-byte literal at match+N
//                  (struct field offsets baked into instructions)
//
#include <Windows.h>
#include <cstdint>
#include "../core/Util.h"

namespace Glacier::sdk {

class Addresses {
public:
    static Addresses& get();

    // Call once after the DLL is loaded into Minecraft.Windows.exe.
    void resolve();
    [[nodiscard]] bool ready()    const { return ready_; }
    [[nodiscard]] int  hitCount() const { return hits_; }
    [[nodiscard]] int  missCount()const { return misses_; }

    // ---- Globals (RIP-relative pointers) ----------------------------------
    struct {
        void* MinecraftGame         = nullptr; // deref(3)
        void* GameCore              = nullptr; // deref(3)  - 0x770
        void* ClickMap              = nullptr; // deref(2)  - MouseDevice::_instance
        void* UIFillColorMaterial   = nullptr; // deref(3)
        void* MouseInputVector      = nullptr; // deref(3)
        void* KeyMap                = nullptr; // deref(3)
        void* GpuInfo               = nullptr; // deref(3)
        void* MceCommonMaterial     = nullptr; // deref(3)  - mce::RenderMaterialGroup::common
    } misc;

    // ---- Patch sites ------------------------------------------------------
    struct {
        void* ThirdPersonNametag    = nullptr; // res - jcc to patch / hook
    } patch;

    // ---- Field offsets (baked-in immediates) -----------------------------
    struct {
        std::uint32_t cursorGrabbed = 0;       // ref(2)  MinecraftGame::cursorGrabbed
        std::uint32_t playerOrigin  = 0;       // ref(4)  LevelRendererPlayer::origin
    } offset;

    // ---- Component try_get stubs -----------------------------------------
    struct {
        void* MoveInput             = nullptr;
        void* ActorRuntimeID        = nullptr;
        void* ActorType             = nullptr;
        void* Attributes            = nullptr;
        void* ActorEquipment        = nullptr;
        void* ActorDataFlags        = nullptr;
    } component;

    // ---- VTables ----------------------------------------------------------
    struct {
        void* TextPacket            = nullptr; // deref(3)
        void* CommandRequestPacket  = nullptr; // deref(3)
        void* Level                 = nullptr; // deref(3)
        void* SetTitlePacket        = nullptr; // deref(3)
    } vtable;

    // ---- Functions --------------------------------------------------------
    struct {
        void* renderLevel               = nullptr; // deref(1) - LevelRenderer::renderLevel
        void* mainWindowProc            = nullptr; // res - MainWindow::_windowProcCallback
        void* getGamma                  = nullptr; // Options::getGamma
        void* getPerspective            = nullptr; // Options::getPerspective
        void* getHideHand               = nullptr; // Options::getHideHand
        void* grabCursor                = nullptr; // ClientInstance::grabCursor
        void* releaseCursor             = nullptr; // ClientInstance::releaseCursor
        void* tickLevel                 = nullptr; // deref(1) - Level::tick
        void* sendChatMessage           = nullptr; // ClientInstanceScreenModel::sendChatMessage
        void* onDeviceLost              = nullptr; // MinecraftGame::onDeviceLost
        void* handleMouseInput          = nullptr; // GameCore::handleMouseInput
        void* getOverlayColor           = nullptr; // RenderController::getOverlayColor
        void* setupAndRender            = nullptr; // deref(1) - ScreenView::setupAndRender
        void* updateGame                = nullptr; // deref(1) - MinecraftGame::_update
        void* getAveragePing            = nullptr; // RakPeer::GetAveragePing
        void* applyTurnDelta            = nullptr; // LocalPlayer::applyTurnDelta
        void* tickBaseInput             = nullptr; // ClientInputUpdateSystem::tickBaseInput
        void* cameraViewBob             = nullptr; // CameraViewBob
        void* getHoverName              = nullptr; // ItemStackBase::getHoverName
        void* tessVertex                = nullptr; // Tessellator::vertex
        void* tessBegin                 = nullptr; // Tessellator::begin
        void* tessColor                 = nullptr; // Tessellator::color
        void* renderMeshImmediately     = nullptr; // deref(1) - MeshHelpers::renderMeshImmediately
        void* baseActorRenderCtx        = nullptr; // BaseActorRenderContext::ctor
        void* renderGuiItemNew          = nullptr; // deref(1) - ItemRenderer::renderGuiItemNew
        void* baseAttributeMapInstance  = nullptr; // deref(1) - BaseAttributeMap::getInstance
        void* uiControlGetPosition      = nullptr; // UIControl::getPosition
        void* getPrimaryClient          = nullptr; // MinecraftGame::getPrimaryClientInstance
        void* renderActor               = nullptr; // ActorRenderDispatcher::render
        void* renderOutlineSelection    = nullptr; // deref(1) - LevelRendererPlayer::renderOutlineSelection
        void* getTimeOfDay              = nullptr; // Dimension::getTimeOfDay
        void* tickDimension             = nullptr; // Dimension::tick
        void* getSkyColor               = nullptr; // Dimension::getSkyColor
        void* getDamageValue            = nullptr; // ItemStackBase::getDamageValue
        void* createPacket              = nullptr; // MinecraftPackets::createPacket
        void* attack                    = nullptr; // Actor::attack
        void* addMessage                = nullptr; // GuiData::_addMessage
        void* getArmor                  = nullptr; // deref(1) - Actor::getArmor
        void* setNameTag                = nullptr; // Actor::setNameTag
        void* updatePlayerCamera        = nullptr; // UpdatePlayerFromCameraSystemUtil::_updatePlayer
        void* onUri                     = nullptr; // GameArguments::_onUri (showHowToPlayScreen anchor)
        void* displayClientMessage      = nullptr; // GuiData::displayClientMessage
        void* renderText                = nullptr; // deref(1) - BaseActorRenderer::renderText
        void* releaseMouse              = nullptr; // AppPlatformGDK::releaseMouse
    } fn;

private:
    bool    ready_  = false;
    int     hits_   = 0;
    int     misses_ = 0;
    HMODULE base_   = nullptr;
};

} // namespace Glacier::sdk
