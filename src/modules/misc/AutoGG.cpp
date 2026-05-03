#include "AutoGG.h"
#include "../../core/Util.h"
#include "../../core/Logger.h"
#include "../../events/EventBus.h"

namespace Glacier::modules {

AutoGG::AutoGG()
    : Module("auto_gg", "AutoGG", "Sends a message after match-end keywords are seen", Category::Misc),
      message_ (addSetting<TextSetting>("message",  "Message",   "What to send",        std::string("gg"))),
      triggers_(addSetting<TextSetting>("triggers", "Triggers",  "Comma-separated",     std::string("won the game,wins the match,Victory!"))),
      delayMs_ (addSetting<IntSetting> ("delay_ms", "Delay (ms)","Wait before sending", 1500, 0, 10000))
{
    EventBus::get().listen<ChatReceiveEvent, &AutoGG::onChat>(this);
}

void AutoGG::onChat(ChatReceiveEvent& e) {
    if (!enabled_) return;
    auto trigs = Util::split(triggers_.get(), ',');
    for (auto& t : trigs) {
        auto tt = Util::trim(t);
        if (!tt.empty() && e.message.find(tt) != std::string::npos) {
            // ChatSendEvent is dispatched by the future ChatScreen hook; we
            // log for now to confirm the trigger pipeline works.
            Logger::get().info("AutoGG", "Trigger \"", tt, "\" hit -> would send: ", message_.get());
            return;
        }
    }
}

} // namespace Glacier::modules
