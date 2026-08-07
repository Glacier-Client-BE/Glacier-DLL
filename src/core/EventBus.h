#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

// Typed publish/subscribe.
//
// Hooks publish events; modules subscribe to them. Without this, every module
// that needs to know about a game event has to wire a bespoke callback into the
// hook that produces it — which is how the pre-reset codebase ended up with
// hand-rolled per-feature sequence-counter caches inside the SDK just so HUD
// widgets could poll "did something happen since I last looked?".
//
// Threading: publish() may be called from the render thread (Present), the game
// thread (SDK detours), or the logic thread. Handlers run on whichever thread
// published, so a handler that touches render state must be subscribed to a
// render-thread event. The subscriber list itself is copied under lock before
// dispatch, so a handler may subscribe or unsubscribe during dispatch without
// invalidating the iteration.
namespace glacier {

using SubscriptionId = std::uint64_t;

class EventBus {
public:
    static EventBus& get() {
        static EventBus instance;
        return instance;
    }

    template <typename Event>
    SubscriptionId subscribe(std::function<void(Event&)> handler) {
        std::scoped_lock lock(m_mutex);
        const SubscriptionId id = ++m_nextId;
        m_handlers[std::type_index(typeid(Event))].push_back(Entry{
            id,
            [fn = std::move(handler)](void* payload) {
                fn(*static_cast<Event*>(payload));
            }
        });
        return id;
    }

    template <typename Event>
    void unsubscribe(SubscriptionId id) {
        std::scoped_lock lock(m_mutex);
        auto it = m_handlers.find(std::type_index(typeid(Event)));
        if (it == m_handlers.end()) return;

        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [id](const Entry& e) { return e.id == id; }),
                   list.end());
    }

    template <typename Event>
    void publish(Event& event) {
        std::vector<Entry> snapshot;
        {
            std::scoped_lock lock(m_mutex);
            auto it = m_handlers.find(std::type_index(typeid(Event)));
            if (it == m_handlers.end() || it->second.empty()) return;
            snapshot = it->second;   // dispatch outside the lock, see class note
        }
        for (const auto& e : snapshot) {
            e.invoke(&event);
        }
    }

    // Convenience for events with no state to carry back.
    template <typename Event>
    void publish() {
        Event e{};
        publish(e);
    }

    void clear() {
        std::scoped_lock lock(m_mutex);
        m_handlers.clear();
    }

private:
    EventBus() = default;

    struct Entry {
        SubscriptionId id;
        std::function<void(void*)> invoke;
    };

    std::mutex m_mutex;
    std::unordered_map<std::type_index, std::vector<Entry>> m_handlers;
    SubscriptionId m_nextId = 0;
};

// ── Event types ──
// Kept as plain structs so publishing is free. An event that a handler can
// influence exposes a mutable field (see `cancelled`).

// Fired once per Present, on the render thread, before the menu draws.
struct RenderEvent {
    float width  = 0;
    float height = 0;
};

// Fired once per logic-loop iteration, on the logic thread.
struct TickEvent {};

// Fired from the WndProc hook, on the window thread. Set `cancelled` to stop
// the message reaching the game.
struct KeyEvent {
    int  vk   = 0;
    bool down = false;
    bool cancelled = false;
};

} // namespace glacier
