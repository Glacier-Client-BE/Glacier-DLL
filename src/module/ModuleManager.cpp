#include "ModuleManager.h"

#include "../util/Logger.h"
#include "SimpleModule.h"
#include "modules/Fullbright.h"
#include "modules/AutoSprint.h"
#include "modules/CpsCounter.h"
#include "modules/FpsCounter.h"
#include "modules/ArmorHud.h"
#include "modules/InventoryHud.h"
#include "modules/Watermark.h"
#include "modules/Coordinates.h"
#include "modules/BlockHit.h"
#include "modules/ForceCoords.h"
#include "modules/HitPing.h"
#include "modules/Hitboxes.h"
#include "modules/JavaViewBobbing.h"
#include "modules/MaterialBinLoader.h"
#include "modules/MemoryDisplay.h"
#include "modules/MinimalViewBobbing.h"
#include "modules/MovableCoordinates.h"
#include "modules/MovableDayCounter.h"
#include "modules/MovableHud.h"
#include "modules/MovablePaperdoll.h"
#include "modules/NullMovement.h"
#include "modules/OpponentReach.h"
#include "modules/ReachDisplay.h"
#include "modules/SnapLook.h"

namespace glacier {

void ModuleManager::initialize() {
    std::scoped_lock lock(m_mutex);
    if (!m_modules.empty()) return;

    // ── Modules with bespoke logic / live HUD widgets ──
    registerModule<Fullbright>();
    registerModule<AutoSprint>();
    registerModule<CpsCounter>();
    registerModule<FpsCounter>();
    registerModule<ArmorHud>();
    registerModule<InventoryHud>();
    registerModule<Watermark>();
    registerModule<Coordinates>();

    // ── Newly ported Flarial features with bespoke logic / hooks / HUD ──
    registerModule<BlockHit>();
    registerModule<ForceCoords>();
    registerModule<HitPing>();
    registerModule<Hitboxes>();
    registerModule<JavaViewBobbing>();
    registerModule<MaterialBinLoader>();
    registerModule<MemoryDisplay>();
    registerModule<MinimalViewBobbing>();
    registerModule<MovableCoordinates>();
    registerModule<MovableDayCounter>();
    registerModule<MovableHud>();
    registerModule<MovablePaperdoll>();
    registerModule<NullMovement>();
    registerModule<OpponentReach>();
    registerModule<ReachDisplay>();
    registerModule<SnapLook>();

    // ── Declarative feature set ──
    // The union of the legitimate modules shipped by Flarial (dll-oss) and
    // Latite — visual / HUD / utility / QoL features. Combat- and movement-
    // exploit modules (KillAura, Reach hack, Fly, Aim, etc.) are deliberately
    // excluded. Each is a real toggleable, configurable, keybindable module; its
    // effect is realized by the matching game hook reading its state.
    const auto add = [&](std::string name, std::string desc, Category cat,
                         std::initializer_list<Setting> settings = {}, int keybind = 0) {
        m_modules.emplace_back(std::make_unique<SimpleModule>(
            std::move(name), std::move(desc), cat, settings, keybind));
    };

    // ── Visual ──
    add("Block Outline", "Customize the block selection outline", Category::Visual,
        { Setting{ "thickness", "Thickness", 1.0f, 0.5f, 4.0f, 0.1f } });
    add("Chunk Borders", "Show chunk boundary lines", Category::Visual, {}, 'G');
    add("Hurt Color", "Customize the damage screen tint", Category::Visual);
    add("No Hurt Cam", "Disable the camera tilt when hurt", Category::Visual);
    add("Fog Control", "Customize or remove fog", Category::Visual,
        { Setting{ "remove", "Remove fog", true } });
    add("Glint Color", "Change the enchantment glint color", Category::Visual);
    add("Chroma", "RGB color cycling for client elements", Category::Visual,
        { Setting{ "speed", "Speed", 1.0f, 0.1f, 5.0f, 0.1f } });
    add("View Model", "Adjust the held-item model position", Category::Visual,
        { Setting{ "scale", "Scale", 1.0f, 0.5f, 2.0f, 0.05f } });
    add("Custom Crosshair", "Customize your crosshair", Category::Visual);
    add("Particle Multiplier", "Control particle density", Category::Visual,
        { Setting{ "amount", "Multiplier", 1.0f, 0.0f, 5.0f, 0.5f } });
    add("Item Physics", "Physics for dropped items", Category::Visual);
    add("Swing Animations", "Custom hand swing animations", Category::Visual,
        { Setting{ "speed", "Speed", 1.0f, 0.5f, 3.0f, 0.1f } });
    add("View Bobbing", "Adjust the view-bobbing style", Category::Visual,
        { Setting{ "amount", "Amount", 1.0f, 0.0f, 1.0f, 0.05f } });
    add("Depth of Field", "Depth-of-field blur", Category::Visual);
    add("Motion Blur", "Per-frame motion blur", Category::Visual,
        { Setting{ "amount", "Amount", 0.5f, 0.0f, 1.0f, 0.05f } });
    add("Cinematic Camera", "Smooth, cinematic camera movement", Category::Visual,
        { Setting{ "smoothness", "Smoothness", 0.5f, 0.0f, 1.0f, 0.05f } });
    add("Render Options", "Extra rendering tweaks", Category::Visual);
    add("Paper Doll", "Show your player model in a corner", Category::Visual);
    add("Dynamic Light", "Held light sources emit light", Category::Visual);
    add("Environment Changer", "Override sky, weather, and biome visuals", Category::Visual);
    add("Third Person Nametag", "Show your nametag in third person", Category::Visual);
    add("Nametag Modifier", "Adjust nametag scale and visibility", Category::Visual,
        { Setting{ "scale", "Scale", 1.0f, 0.5f, 3.0f, 0.1f } });
    add("Instant Hurt Animation", "Skip the hurt-animation delay", Category::Visual);
    add("Upside Down", "Flip the screen upside down", Category::Visual);
    add("Deepfry", "Over-saturate the screen", Category::Visual);
    add("DVD Screen", "Bouncing-logo screensaver", Category::Visual);

    // ── Movement (no exploits) ──
    add("Sprint", "Toggle sprint on", Category::Movement);
    add("Sneak", "Toggle or hold sneak", Category::Movement,
        { Setting{ "toggle", "Toggle mode", false } });
    add("Toggle Sprint/Sneak", "Make sprint and sneak toggle instead of hold", Category::Movement);
    add("Freelook", "Look around without turning your character", Category::Movement, {}, 'V');
    add("Auto Perspective", "Auto-switch perspective in set situations", Category::Movement);

    // ── Player / QoL ──
    add("Auto GG", "Send a message at the end of a match", Category::Player);
    add("Inventory Lock", "Lock chosen inventory slots from moving", Category::Player);
    add("Faster Inventory", "Remove the inventory interaction delay", Category::Player);
    add("Java Inventory Hotkeys", "Java-style number-key item swapping", Category::Player);
    add("Item Use Delay Fix", "Remove the right-click use delay", Category::Player);
    add("Durability Warning", "Warn when armor or tools are low", Category::Player,
        { Setting{ "threshold", "Threshold %", 10, 1, 50 } });
    add("Low Health Indicator", "Screen indicator at low health", Category::Player,
        { Setting{ "threshold", "Threshold (HP)", 6, 1, 19 } });
    add("Skin Stealer", "Copy another player's skin", Category::Player);
    add("Nickname", "Change your displayed name locally", Category::Player);
    add("Item Tweaks", "Miscellaneous item interaction tweaks", Category::Player);
    add("Java Drop", "Java-style drop-key behaviour", Category::Player);
    add("Bow Sensitivity", "Lower sensitivity while drawing a bow", Category::Player,
        { Setting{ "multiplier", "Multiplier", 0.7f, 0.1f, 1.0f, 0.05f } });
    add("Sensitivity Multiplier", "Multiply your mouse sensitivity", Category::Player,
        { Setting{ "multiplier", "Multiplier", 1.0f, 0.1f, 3.0f, 0.05f } });
    add("Raw Input", "Use raw, unaccelerated mouse input", Category::Player);
    add("Disable Mouse Wheel", "Stop the scroll wheel changing hotbar slot", Category::Player);
    add("Modern Keybind Handling", "Improved keybind handling", Category::Player);

    // ── World ──
    add("Waypoints", "Place and view waypoints", Category::World);
    add("Block Break Indicator", "Show block-break progress", Category::World);
    add("Waila", "Show info about the block you're looking at", Category::World);
    add("Entity Counter", "Count nearby entities", Category::World);
    add("Time Changer", "Override the client-side time of day", Category::World,
        { Setting{ "time", "Time", 6000, 0, 24000 } });
    add("Weather Changer", "Override client-side weather", Category::World);
    add("TNT Timer", "Show the TNT fuse countdown", Category::World);
    add("Death Logger", "Log when players die", Category::World);
    add("Player Notifier", "Notify when players enter render distance", Category::World);

    // ── Misc / HUD ──
    add("Click GUI", "Open the Glacier menu", Category::Misc, {}, VK_INSERT);
    add("Keystrokes", "On-screen WASD and click keystrokes", Category::Misc);
    add("Mousestrokes", "On-screen mouse-button display", Category::Misc);
    add("CPS Limiter", "Cap your clicks per second", Category::Misc,
        { Setting{ "max", "Max CPS", 16, 1, 30 } });
    add("Zoom", "Hold to zoom in", Category::Misc,
        { Setting{ "level", "Zoom level", 4.0f, 1.0f, 10.0f, 0.5f } }, 'C');
    add("FOV Changer", "Override your field of view", Category::Misc,
        { Setting{ "fov", "FOV", 90, 30, 130 } });
    add("Dynamic FOV", "Java-style FOV changes with speed", Category::Misc);
    add("GUI Scale", "Override the GUI scale", Category::Misc,
        { Setting{ "scale", "Scale", 3, 1, 6 } });
    add("Subtitles", "Show sound subtitles", Category::Misc);
    add("Discord Presence", "Show Glacier in your Discord status", Category::Misc);
    add("Clear Chat", "Clear the chat", Category::Misc);
    add("Compact Chat", "Stack duplicate chat messages", Category::Misc);
    add("Message Logger", "Log deleted chat messages", Category::Misc);
    add("Clear Scoreboard", "Hide scoreboard numbers", Category::Misc);
    add("Stopwatch", "On-screen stopwatch", Category::Misc);
    add("Command Hotkeys", "Bind commands or text to keys", Category::Misc);
    add("Ping Display", "Show your connection ping", Category::Misc);
    add("Server Display", "Show the current server IP", Category::Misc);
    add("Frame Time Display", "Show a frame-time graph", Category::Misc);
    add("Combo Counter", "Show your current hit combo", Category::Combat);
    add("Totem Counter", "Count totems in your inventory", Category::Combat);
    add("Potion Counter", "Count potions in your inventory", Category::Combat);
    add("Arrow Counter", "Count arrows in your inventory", Category::Combat);
    add("Item Counter", "Count a chosen item", Category::Combat);
    add("Experience Info", "Show XP level and progress", Category::Misc);
    add("Direction HUD", "Show the direction you're facing", Category::Misc);
    add("Speed Display", "Show your movement speed", Category::Misc);
    add("Pitch Display", "Show your pitch and yaw", Category::Misc);
    add("Behind You", "Alert when a player is behind you", Category::Combat);
    add("Tab List", "Custom tab / player list", Category::Misc);
    add("Hive Utils", "Server-specific utilities for The Hive", Category::Misc);
    add("Zeqa Utils", "Server-specific utilities for Zeqa", Category::Misc);
    add("Mumble Link", "Positional audio via Mumble", Category::Misc);
    add("Debug Menu", "Developer debug overlay", Category::Misc);
    add("20-20-20", "Eye-rest reminder every 20 minutes", Category::Misc);

    // ── Expansion set (+35) — still visual / HUD / utility only ──

    // Visual
    add("Custom Animations", "Customize hand and item animations", Category::Visual);
    add("Better Hunger Bar", "Cleaner hunger-bar rendering", Category::Visual);
    add("Hue Changer", "Shift the hue of the entire screen", Category::Visual,
        { Setting{ "shift", "Hue shift", 0.0f, 0.0f, 360.0f, 5.0f } });
    add("Hotbar Customizer", "Reposition and restyle the hotbar", Category::Visual);
    add("Block Highlight", "Highlight the block you're looking at", Category::Visual);
    add("Custom Skybox", "Replace the sky with a custom color or gradient", Category::Visual);
    add("Chat Bubbles", "Show recent chat above players", Category::Visual);
    add("Doom Mode", "Retro shader filter", Category::Visual);
    add("Twerk", "Make your character twerk", Category::Visual);

    // World
    add("Minimap", "Top-down minimap of your surroundings", Category::World,
        { Setting{ "scale", "Scale", 1.0f, 0.5f, 3.0f, 0.1f } });
    add("Day Counter", "Count the in-game days survived", Category::World);
    add("Block Counter", "Count blocks of a chosen type nearby", Category::World);
    add("Beacon Beam", "Render a vertical beam at your waypoints", Category::World);

    // Player
    add("Health Warning", "Sound and screen flash at low health", Category::Player,
        { Setting{ "threshold", "Threshold (HP)", 6, 1, 19 } });
    add("Held Item Info", "Show details of your held item", Category::Player);
    add("Auto Respawn", "Automatically respawn on death", Category::Player);

    // Combat (info / HUD only)
    add("Bow Indicator", "Show bow charge progress", Category::Combat);
    add("Hit Marker", "Show a marker when you land a hit", Category::Combat);
    add("Gap Counter", "Count golden apples in your inventory", Category::Combat);

    // Misc / HUD
    add("Potion HUD", "Show your active potion effects", Category::Misc);
    add("Clock", "Real-time clock on screen", Category::Misc,
        { Setting{ "seconds", "Show seconds", true } });
    add("Movable Boss Bar", "Reposition the boss bar", Category::Misc);
    add("Movable Chat", "Reposition the chat", Category::Misc);
    add("Movable Hotbar", "Reposition the hotbar", Category::Misc);
    add("Movable Scoreboard", "Reposition the scoreboard", Category::Misc);
    add("Movable Title", "Reposition titles and subtitles", Category::Misc);
    add("Hive Stats", "Show your Hive game stats", Category::Misc);
    add("Hive Translate", "Translate Hive chat messages", Category::Misc);
    add("IP Display", "Show the current server address", Category::Misc);
    add("Text Hotkey", "Send chat text on a keypress", Category::Misc);
    add("Command Shortcuts", "Short aliases for long commands", Category::Misc);
    add("Block Game", "A small block game inside the menu", Category::Misc);
    add("Toggle Sounds", "Play a sound when a module toggles", Category::Misc);
    add("Network Stats", "Show packets sent/received per second", Category::Misc);
    add("Quick Connect", "Saved-server quick-connect list", Category::Misc);

    // ── Final pass — remaining declarative modules from Flarial + Latite ──
    // (The 16 features that needed bespoke logic — Block Hit, Hitboxes, the
    //  view-bobbing pair, Material Bin Loader, Null Movement, Snap Look, Force
    //  Coords, the reach/ping trio, Memory Display, and the Movable widgets —
    //  are now real classes registered above, so they're no longer listed here.)

    LOG_INFO("registered {} modules", m_modules.size());
}

void ModuleManager::shutdown() {
    std::scoped_lock lock(m_mutex);
    for (auto& m : m_modules) {
        if (m->enabled()) m->setEnabled(false);
    }
    m_modules.clear();
}

void ModuleManager::tickAll() {
    std::scoped_lock lock(m_mutex);
    for (auto& m : m_modules) {
        if (m->enabled()) m->onTick();
    }
}

void ModuleManager::renderAll() {
    std::scoped_lock lock(m_mutex);
    for (auto& m : m_modules) {
        if (m->enabled()) m->onRender();
    }
}

bool ModuleManager::handleKey(int vk) {
    std::scoped_lock lock(m_mutex);
    bool consumed = false;
    for (auto& m : m_modules) {
        if (m->keybind() != 0 && m->keybind() == vk) {
            m->toggle();
            LOG_INFO("{} -> {}", m->name(), m->enabled() ? "on" : "off");
            consumed = true;
        }
    }
    return consumed;
}

void ModuleManager::handleClick(bool rightButton) {
    std::scoped_lock lock(m_mutex);
    for (auto& m : m_modules) {
        if (m->enabled()) m->onClick(rightButton);
    }
}

Module* ModuleManager::find(std::string_view name) {
    for (auto& m : m_modules) {
        if (m->name() == name) return m.get();
    }
    return nullptr;
}

bool ModuleManager::toggle(std::string_view name) {
    std::scoped_lock lock(m_mutex);
    if (auto* m = find(name)) {
        m->toggle();
        return m->enabled();
    }
    return false;
}

} // namespace glacier
