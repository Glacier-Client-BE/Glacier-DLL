#pragma once
//
// Type-safe event bus.  Subscribe with the `listen<E, &T::method>(this)` form.
//   class Foo {
//     Foo() { EventBus::get().listen<TickEvent, &Foo::onTick>(this); }
//     void onTick(TickEvent& e) { ... }
//   };
// Internally, listeners are erased to a (void*, void(*)(void*, Event&)) pair
// keyed on type_index, so dispatch is O(N) with N = subscribers for that type.
//
#include "Event.h"
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <utility>
#include <algorithm>

namespace Glacier {

enum class Priority : int { Highest = 0, High = 1, Normal = 2, Low = 3, Lowest = 4 };

class EventBus {
public:
    static EventBus& get();

    template <class E, auto Method, class T>
    void listen(T* self, Priority prio = Priority::Normal) {
        static_assert(std::is_base_of_v<Event, E>, "E must derive from Event");
        auto thunk = +[](void* receiver, Event& e) {
            (static_cast<T*>(receiver)->*Method)(static_cast<E&>(e));
        };
        Slot s{ self, thunk, static_cast<int>(prio) };
        std::unique_lock lk(mu_);
        auto& vec = map_[std::type_index(typeid(E))];
        // insert preserving priority order (lower int = earlier)
        auto it = vec.begin();
        while (it != vec.end() && it->prio <= s.prio) ++it;
        vec.insert(it, s);
    }

    template <class E, auto Method, class T>
    void unlisten(T* self) {
        std::unique_lock lk(mu_);
        auto it = map_.find(std::type_index(typeid(E)));
        if (it == map_.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [self](const Slot& s) { return s.recv == self; }),
            vec.end());
    }

    // Remove all subscriptions for this `self` pointer.
    void unlistenAll(void* self);

    template <class E>
    void dispatch(E& e) {
        static_assert(std::is_base_of_v<Event, E>, "E must derive from Event");
        std::shared_lock lk(mu_);
        auto it = map_.find(std::type_index(typeid(E)));
        if (it == map_.end()) return;
        // Copy slots so listeners can mutate the bus during dispatch.
        auto vec = it->second;
        lk.unlock();
        for (auto& s : vec) {
            s.thunk(s.recv, e);
            if constexpr (std::is_base_of_v<CancellableEvent, E>) {
                if (e.isCancelled()) break;
            }
        }
    }

private:
    EventBus() = default;
    struct Slot {
        void* recv;
        void (*thunk)(void*, Event&);
        int   prio;
    };
    std::unordered_map<std::type_index, std::vector<Slot>> map_;
    std::shared_mutex mu_;
};

} // namespace Glacier
