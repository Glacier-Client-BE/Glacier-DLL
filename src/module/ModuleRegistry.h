#pragma once

#include <functional>
#include <vector>

#include "Module.h"

// Self-registering module list.
//
// A concrete module drops GLACIER_MODULE(ClassName) beneath its definition and
// is constructed by ModuleManager automatically — there is no central list to
// edit, so adding a module touches exactly one file (plus the .vcxproj, which
// fails loudly if a translation unit is missing).
//
// This is deliberately in place from the first module rather than retrofitted:
// migrating a hand-written list later is pure busywork, and the cost of doing
// it correctly while there is only one module is zero.
namespace glacier {

class ModuleRegistry {
public:
    using Factory = std::function<ModulePtr()>;

    // Meyers singleton: the registry must be fully constructed before any
    // static-init registration runs, and function-local statics guarantee that
    // regardless of translation-unit initialization order.
    static ModuleRegistry& get() {
        static ModuleRegistry instance;
        return instance;
    }

    void add(Factory factory) { m_factories.push_back(std::move(factory)); }

    const std::vector<Factory>& factories() const { return m_factories; }

private:
    ModuleRegistry() = default;
    std::vector<Factory> m_factories;
};

// Registers a module type. Place inside `namespace glacier`, after the class
// body, in the module's single .cpp:
//
//     class Fullbright final : public Module { ... };
//     GLACIER_MODULE(Fullbright);
//
// It must be inside the namespace so `Type` resolves unqualified — a qualified
// name would not produce a valid identifier for the registrar object.
namespace detail {
struct ModuleRegistrar {
    explicit ModuleRegistrar(ModuleRegistry::Factory factory) {
        ModuleRegistry::get().add(std::move(factory));
    }
};
} // namespace detail

} // namespace glacier

#define GLACIER_MODULE(Type)                                                   \
    namespace {                                                                \
    const ::glacier::detail::ModuleRegistrar g_register_##Type{                \
        [] { return ::glacier::ModulePtr{ new Type() }; } };                   \
    }                                                                          \
    static_assert(true, "GLACIER_MODULE requires a trailing semicolon")
