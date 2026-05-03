#pragma once
//
// Forward-declared game types. We treat them as opaque pointers - real layouts
// must come from RE work and live alongside Addresses.h. Modules call helper
// functions in Game.h instead of dereferencing directly.
//

namespace Glacier::sdk {

struct Minecraft;
struct ClientInstance;
struct LocalPlayer;
struct Level;
struct Actor;
struct GuiData;
struct Options;
struct Packet;

} // namespace Glacier::sdk
