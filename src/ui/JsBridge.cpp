#include "JsBridge.h"

#include "UIRenderer.h"
#include "../module/ModuleManager.h"
#include "../util/Json.h"
#include "../util/Logger.h"

#include <AppCore/JSHelpers.h>
#include <sstream>

using namespace ultralight;

namespace glacier {

namespace {

// JSON string escaping for the small set of fields we emit (names/labels).
std::string jsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

const char* settingTypeName(SettingType t) {
    switch (t) {
        case SettingType::Bool:  return "bool";
        case SettingType::Float: return "float";
        case SettingType::Int:   return "int";
        case SettingType::Enum:  return "enum";
        case SettingType::Key:   return "key";
    }
    return "bool";
}

} // namespace

JsBridge::JsBridge(RefPtr<View> view) : m_view(std::move(view)) {
    m_view->set_load_listener(this);
}

JsBridge::~JsBridge() {
    if (m_view) m_view->set_load_listener(nullptr);
}

void JsBridge::OnDOMReady(View* caller, uint64_t, bool is_main_frame, const String&) {
    if (!is_main_frame) return;

    // Enter the page's JS context and install the global `glacier` API.
    auto scoped = caller->LockJSContext();
    SetJSContext((*scoped));

    JSObject global = JSGlobalObject();
    JSObject api;

    api["getState"]     = BindJSCallbackWithRetval(&JsBridge::jsGetState);
    api["toggleModule"] = BindJSCallbackWithRetval(&JsBridge::jsToggleModule);
    api["setSetting"]   = BindJSCallbackWithRetval(&JsBridge::jsSetSetting);
    api["setKeybind"]   = BindJSCallbackWithRetval(&JsBridge::jsSetKeybind);
    api["closeMenu"]    = BindJSCallbackWithRetval(&JsBridge::jsCloseMenu);

    global["glacier"] = api;

    LOG_INFO("glacier JS API installed");
    pushState();
}

std::string JsBridge::buildStateJson() const {
    auto& mm = ModuleManager::get();
    std::scoped_lock lock(mm.mutex());

    std::ostringstream os;
    os << "{\"modules\":[";

    bool firstModule = true;
    for (const auto& m : mm.modules()) {
        if (!firstModule) os << ",";
        firstModule = false;

        os << "{"
           << "\"name\":\""        << jsonEscape(m->name()) << "\","
           << "\"description\":\""  << jsonEscape(m->description()) << "\","
           << "\"category\":\""     << categoryName(m->category()) << "\","
           << "\"enabled\":"        << (m->enabled() ? "true" : "false") << ","
           << "\"keybind\":"        << m->keybind() << ","
           << "\"settings\":[";

        bool firstSetting = true;
        for (const auto& s : m->settings()) {
            if (!firstSetting) os << ",";
            firstSetting = false;

            os << "{"
               << "\"id\":\""    << jsonEscape(s.id()) << "\","
               << "\"label\":\"" << jsonEscape(s.label()) << "\","
               << "\"type\":\""  << settingTypeName(s.type()) << "\","
               << "\"min\":"     << s.min() << ","
               << "\"max\":"     << s.max() << ","
               << "\"step\":"    << s.step() << ",";

            os << "\"value\":";
            switch (s.type()) {
                case SettingType::Bool: os << (s.asBool() ? "true" : "false"); break;
                case SettingType::Int:  os << s.asInt();   break;
                default:                os << s.asFloat(); break;
            }
            os << "}";
        }
        os << "]}";
    }
    os << "]}";
    return os.str();
}

void JsBridge::pushState() {
    if (!m_view) return;
    const std::string json = buildStateJson();

    // Call window.glacier.onState(json) if the page registered a handler.
    const std::string script =
        "if (window.glacier && typeof window.glacier.onState === 'function') {"
        "  window.glacier.onState(" + json + ");"
        "}";

    String exception;
    m_view->EvaluateScript(script.c_str(), &exception);
    if (!exception.empty()) {
        LOG_WARN("pushState script error: {}", exception.utf8().data());
    }
}

void JsBridge::pushHud() {
    if (!m_view) return;

    json::Array huds;
    {
        auto& mm = ModuleManager::get();
        std::scoped_lock lock(mm.mutex());
        for (const auto& m : mm.modules()) {
            if (m->enabled() && m->hasHud()) {
                m->writeHud(huds);
            }
        }
    }

    const std::string script =
        "if(window.glacier&&typeof window.glacier.onHud==='function'){"
        "window.glacier.onHud(" + huds.str() + ");}";

    String exception;
    m_view->EvaluateScript(script.c_str(), &exception);
}

void JsBridge::pushMenuVisible(bool visible) {
    if (!m_view) return;
    const std::string script =
        std::string("if(window.glacier&&typeof window.glacier.setMenuVisible==='function'){"
                    "window.glacier.setMenuVisible(") + (visible ? "true" : "false") + ");}";
    String exception;
    m_view->EvaluateScript(script.c_str(), &exception);
}

JSValue JsBridge::jsGetState(const JSObject&, const JSArgs&) {
    return JSValue(buildStateJson().c_str());
}

JSValue JsBridge::jsToggleModule(const JSObject&, const JSArgs& args) {
    if (args.empty() || !args[0].IsString()) return JSValue(false);
    const String name = args[0].ToString();
    const bool nowEnabled = ModuleManager::get().toggle(name.utf8().data());
    pushState();
    return JSValue(nowEnabled);
}

JSValue JsBridge::jsSetSetting(const JSObject&, const JSArgs& args) {
    if (args.size() < 3 || !args[0].IsString() || !args[1].IsString()) {
        return JSValue(false);
    }
    const String moduleName = args[0].ToString();
    const String settingId  = args[1].ToString();

    auto& mm = ModuleManager::get();
    std::scoped_lock lock(mm.mutex());

    Module* mod = mm.find(moduleName.utf8().data());
    if (!mod) return JSValue(false);

    Setting* setting = mod->setting(settingId.utf8().data());
    if (!setting) return JSValue(false);

    switch (setting->type()) {
        case SettingType::Bool:
            setting->set(args[2].ToBoolean());
            break;
        case SettingType::Int:
            setting->set(static_cast<int>(args[2].ToNumber()));
            break;
        default:
            setting->set(static_cast<float>(args[2].ToNumber()));
            break;
    }
    return JSValue(true);
}

JSValue JsBridge::jsSetKeybind(const JSObject&, const JSArgs& args) {
    if (args.size() < 2 || !args[0].IsString()) return JSValue(false);
    const String name = args[0].ToString();

    auto& mm = ModuleManager::get();
    std::scoped_lock lock(mm.mutex());
    if (Module* mod = mm.find(name.utf8().data())) {
        mod->setKeybind(static_cast<int>(args[1].ToNumber()));
        return JSValue(true);
    }
    return JSValue(false);
}

JSValue JsBridge::jsCloseMenu(const JSObject&, const JSArgs&) {
    UIRenderer::get().setMenuOpen(false);
    return JSValue(true);
}

} // namespace glacier
