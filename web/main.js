/* ============================================================================
   Glacier DLL — UI controller / native bridge
   ----------------------------------------------------------------------------
   C++ owns the source of truth. This script:
     • receives the module snapshot via window.glacier.onState(json)
       (pushed by JsBridge::pushState on open and after every change), and
     • sends user actions back through the native methods JsBridge installs:
         glacier.toggleModule(name)        -> bool
         glacier.setSetting(mod, id, val)
         glacier.setKeybind(mod, vk)
         glacier.closeMenu()

   When window.glacier is absent (running in a plain browser for design work) a
   mock backend is installed so the UI is fully interactive standalone.
   ============================================================================ */

(() => {
    "use strict";

    // Standalone test build (standalone.html) sets this — there's no native C++
    // backend, so the page drives menu open/close + pause itself.
    const STANDALONE = !!window.GLACIER_STANDALONE;

    // ── Category metadata: order + FontAwesome icon ──
    const CATEGORIES = [
        { id: "Combat",   icon: "fa-solid fa-gavel" },
        { id: "Movement", icon: "fa-solid fa-person-running" },
        { id: "Visual",   icon: "fa-solid fa-eye" },
        { id: "Player",   icon: "fa-solid fa-user" },
        { id: "World",    icon: "fa-solid fa-cube" },
        { id: "Misc",     icon: "fa-solid fa-ellipsis" },
    ];

    const state = {
        modules: [],
        activeCategory: "Combat",
        search: "",
        bindingModule: null, // module currently capturing a new keybind
    };

    const el = {
        menu:       document.getElementById("menu"),
        nav:        document.getElementById("nav"),
        modules:    document.getElementById("modules"),
        title:      document.getElementById("categoryTitle"),
        search:     document.getElementById("search"),
        empty:      document.getElementById("emptyState"),
        closeBtn:   document.getElementById("closeBtn"),
        toastHost:  document.getElementById("toastHost"),
        free:       document.getElementById("hud-free"),
        corners: {
            "top-left":     document.getElementById("hud-tl"),
            "top-right":    document.getElementById("hud-tr"),
            "bottom-left":  document.getElementById("hud-bl"),
            "bottom-right": document.getElementById("hud-br"),
        },
    };

    /* ── Native bridge installation ──
       Expose onState so C++ can push. If no native backend exists, install a
       local mock that mirrors the real semantics. */
    const hasNative = typeof window.glacier === "object" && typeof window.glacier.toggleModule === "function";
    if (!window.glacier) window.glacier = {};

    if (!hasNative) installMockBackend();

    // C++ calls this with the full snapshot.
    window.glacier.onState = (snapshot) => {
        state.modules = (snapshot && snapshot.modules) || [];
        window.glacier.modules = state.modules;   // live ref for the standalone game sim
        render();
    };

    // C++ shows/hides the click-GUI; the HUD keeps rendering regardless.
    window.glacier.setMenuVisible = (visible) => {
        el.menu.hidden = !visible;
        if (visible) render();
        // Broadcast so the standalone game sim can pause/resume + dim.
        window.dispatchEvent(new CustomEvent("glacier-menu", { detail: { open: visible } }));
    };

    // C++ pushes the always-on HUD payload (~15 Hz).
    window.glacier.onHud = (huds) => renderHud(huds || []);

    /* ── HUD rendering ──
       Nodes are reused by key so widgets only animate in once, not on every
       refresh. Text widgets live in fixed corners; armor/inventory are
       absolutely positioned from C++-supplied coordinates. */
    const hudText = new Map();   // id -> element

    function renderHud(huds) {
        const seenText = new Set();
        let armor = null, inventory = null;

        for (const w of huds) {
            if (w.type === "text") { renderTextWidget(w); seenText.add(w.id); }
            else if (w.type === "armor")     armor = w;
            else if (w.type === "inventory") inventory = w;
        }
        for (const [id, node] of hudText) {
            if (!seenText.has(id)) { node.remove(); hudText.delete(id); }
        }
        renderArmor(armor);
        renderInventory(inventory);
    }

    function renderTextWidget(w) {
        let node = hudText.get(w.id);
        if (!node) {
            node = document.createElement("div");
            node.className = "hud-widget hud-text";
            node.innerHTML = `<span class="ht-label"></span><span class="ht-value"></span><span class="ht-sub"></span>`;
            (el.corners[w.anchor] || el.corners["top-right"]).appendChild(node);
            hudText.set(w.id, node);
        }
        node.querySelector(".ht-label").textContent = w.label || "";
        node.querySelector(".ht-value").textContent = w.value || "";
        const sub = node.querySelector(".ht-sub");
        sub.textContent = w.sub || "";
        sub.style.display = w.sub ? "" : "none";
    }

    const ARMOR_ICONS  = ["fa-helmet-safety", "fa-shirt", "fa-person", "fa-shoe-prints"];
    const ARMOR_LABELS = ["Helmet", "Chestplate", "Leggings", "Boots"];

    function ensureFreeNode(key, className) {
        let node = el.free.querySelector(`[data-hud="${key}"]`);
        if (!node) {
            node = document.createElement("div");
            node.className = "hud-widget " + className;
            node.dataset.hud = key;
            el.free.appendChild(node);
        }
        return node;
    }

    function removeFreeNode(key) {
        const node = el.free.querySelector(`[data-hud="${key}"]`);
        if (node) node.remove();
    }

    function renderArmor(w) {
        if (!w) { removeFreeNode("armor"); return; }
        const node = ensureFreeNode("armor", "hud-armor");
        node.classList.toggle("vertical", !!w.vertical);
        node.style.left = w.x + "px";
        node.style.top = w.y + "px";
        node.style.transform = `scale(${w.scale || 1})`;
        node.innerHTML = "";

        (w.slots || []).forEach((s, i) => {
            const slot = document.createElement("div");
            slot.className = "armor-slot" + (s.valid ? "" : " empty") + (s.ench ? " ench" : "");
            const icon = `<div class="armor-icon"><i class="fa-solid ${ARMOR_ICONS[i] || "fa-shield"}"></i></div>`;
            if (!s.valid) {
                slot.innerHTML = icon + `<div class="armor-meta"><div class="armor-top"><span class="armor-name">${ARMOR_LABELS[i]}</span><span>—</span></div></div>`;
            } else {
                const name = s.name && s.name.length ? s.name : ARMOR_LABELS[i];
                const count = s.count > 1 ? `<span class="armor-count">x${s.count}</span>` : "";
                let dura = "";
                if (w.durability && s.max > 0) {
                    const ratio = Math.max(0, Math.min(1, s.dura / s.max));
                    const hue = Math.round(ratio * 120);   // red → green
                    dura = `<div class="dura-bar"><div class="dura-fill" style="width:${ratio * 100}%;background:hsl(${hue},70%,50%)"></div></div>`;
                }
                slot.innerHTML = icon +
                    `<div class="armor-meta"><div class="armor-top"><span class="armor-name">${escapeHtml(name)}</span>${count}</div>${dura}</div>`;
            }
            node.appendChild(slot);
        });
    }

    function renderInventory(w) {
        if (!w) { removeFreeNode("inventory"); return; }
        const node = ensureFreeNode("inventory", "hud-inventory");
        const cols = w.columns || 9;
        const cell = 34, gap = 3;
        node.style.left = w.x + "px";
        node.style.top = w.y + "px";
        node.style.transform = `scale(${w.scale || 1})`;
        node.style.gridTemplateColumns = `repeat(${cols}, ${cell}px)`;
        node.style.background = `rgba(20, 22, 26, ${w.bg != null ? w.bg : 0.5})`;
        node.innerHTML = "";

        (w.slots || []).forEach((s) => {
            const c = document.createElement("div");
            c.className = "inv-slot" + (s.valid ? " filled" : "");
            if (s.valid) {
                c.innerHTML = `<i class="fa-solid fa-cube" data-tooltip="${escapeHtml(s.name || "")}"></i>` +
                    (w.counts && s.count > 1 ? `<span class="inv-count">${s.count}</span>` : "");
            }
            node.appendChild(c);
        });
        void gap;
    }

    /* ── Rendering ── */
    function moduleVisible(m) {
        if (m.category !== state.activeCategory) return false;
        if (!state.search) return true;
        const q = state.search.toLowerCase();
        return m.name.toLowerCase().includes(q) || m.description.toLowerCase().includes(q);
    }

    function render() {
        renderNav();
        renderModules();
    }

    function renderNav() {
        el.nav.innerHTML = "";
        for (const cat of CATEGORIES) {
            const count = state.modules.filter(m => m.category === cat.id && m.enabled).length;
            const total = state.modules.filter(m => m.category === cat.id).length;
            if (total === 0) continue;

            const btn = document.createElement("button");
            btn.className = "nav-item" + (cat.id === state.activeCategory ? " active" : "");
            btn.innerHTML =
                `<i class="${cat.icon}"></i><span>${cat.id}</span>` +
                (count > 0 ? `<span class="count">${count}</span>` : "");
            btn.addEventListener("click", () => {
                state.activeCategory = cat.id;
                state.search = "";
                el.search.value = "";
                render();
            });
            el.nav.appendChild(btn);
        }
    }

    function renderModules() {
        el.title.textContent = state.activeCategory;
        el.modules.innerHTML = "";

        const visible = state.modules.filter(moduleVisible);
        el.empty.hidden = visible.length > 0;
        el.modules.style.display = visible.length > 0 ? "" : "none";

        visible.forEach((m, i) => {
            const card = document.createElement("div");
            card.className = "module-card" + (m.enabled ? " enabled" : "");
            card.style.animationDelay = `${Math.min(i * 18, 180)}ms`;

            // ── Header: info + toggle ──
            const head = document.createElement("div");
            head.className = "module-head";

            const info = document.createElement("div");
            info.className = "module-info";
            info.innerHTML =
                `<div class="module-name">${escapeHtml(m.name)}</div>` +
                `<div class="module-desc">${escapeHtml(m.description)}</div>`;

            // Keybind editor
            const bind = document.createElement("div");
            bind.className = "module-keybind";
            const keyLabel = m.keybind ? vkName(m.keybind) : "None";
            const isBinding = state.bindingModule === m.name;
            bind.innerHTML = `<i class="fa-solid fa-keyboard"></i> Bind: ` +
                `<kbd class="${isBinding ? "binding" : ""}">${isBinding ? "press a key…" : keyLabel}</kbd>`;
            bind.querySelector("kbd").addEventListener("click", (e) => {
                e.stopPropagation();
                state.bindingModule = isBinding ? null : m.name;
                render();
            });
            info.appendChild(bind);

            const toggle = document.createElement("div");
            toggle.className = "toggle" + (m.enabled ? " on" : "");
            toggle.setAttribute("role", "switch");
            toggle.setAttribute("aria-checked", String(m.enabled));
            toggle.addEventListener("click", () => {
                const now = window.glacier.toggleModule(m.name);
                m.enabled = (now === undefined) ? !m.enabled : now;
                toast(m.name, m.enabled);
                render();
            });

            head.append(info, toggle);
            card.appendChild(head);

            // ── Settings (only when enabled & present) ──
            if (m.enabled && m.settings && m.settings.length) {
                const wrap = document.createElement("div");
                wrap.className = "module-settings";
                for (const s of m.settings) {
                    wrap.appendChild(renderSetting(m, s));
                }
                card.appendChild(wrap);
            }

            el.modules.appendChild(card);
        });
    }

    function renderSetting(module, s) {
        const row = document.createElement("div");
        row.className = "setting";

        const label = document.createElement("span");
        label.className = "setting-label";
        label.textContent = s.label;
        row.appendChild(label);

        if (s.type === "bool") {
            const t = document.createElement("div");
            t.className = "toggle mini" + (s.value ? " on" : "");
            t.addEventListener("click", () => {
                s.value = !s.value;
                window.glacier.setSetting(module.name, s.id, s.value);
                t.classList.toggle("on", s.value);
            });
            row.appendChild(t);
        } else {
            // float / int slider
            const valueLabel = document.createElement("span");
            valueLabel.className = "setting-value";
            const fmt = (v) => s.type === "int" ? String(Math.round(v)) : Number(v).toFixed(2);
            valueLabel.textContent = fmt(s.value);

            const slider = document.createElement("input");
            slider.type = "range";
            slider.min = s.min;
            slider.max = s.max;
            slider.step = s.type === "int" ? 1 : (s.step || 0.01);
            slider.value = s.value;
            slider.addEventListener("input", () => {
                const v = s.type === "int" ? parseInt(slider.value, 10) : parseFloat(slider.value);
                s.value = v;
                valueLabel.textContent = fmt(v);
                window.glacier.setSetting(module.name, s.id, v);
            });

            row.append(slider, valueLabel);
        }
        return row;
    }

    /* ── Events ── */
    el.search.addEventListener("input", () => {
        state.search = el.search.value.trim();
        renderModules();
    });

    el.closeBtn.addEventListener("click", () => window.glacier.closeMenu());

    document.addEventListener("keydown", (e) => {
        // Capture a keybind if we're in binding mode.
        if (state.bindingModule) {
            e.preventDefault();
            const vk = e.keyCode || e.which;
            if (e.key === "Escape") {
                state.bindingModule = null;
            } else {
                window.glacier.setKeybind(state.bindingModule, vk);
                const m = state.modules.find(x => x.name === state.bindingModule);
                if (m) m.keybind = vk;
                state.bindingModule = null;
            }
            render();
            return;
        }
        // Standalone build: there's no C++ to handle the toggle key, so the page
        // does it. Insert / Right-Shift open & close the menu.
        if (STANDALONE && (e.key === "Insert" || e.code === "ShiftRight")) {
            e.preventDefault();
            window.glacier.setMenuVisible(el.menu.hidden);   // hidden -> show
            return;
        }
        if (e.key === "Escape") window.glacier.closeMenu();
    });

    /* ── Toast helper ── */
    function toast(name, on) {
        const t = document.createElement("div");
        t.className = "toast " + (on ? "on" : "off");
        t.innerHTML = `<i class="fa-solid ${on ? "fa-circle-check" : "fa-circle-minus"}"></i>` +
            `<span>${escapeHtml(name)} ${on ? "enabled" : "disabled"}</span>`;
        el.toastHost.appendChild(t);
        setTimeout(() => {
            t.style.opacity = "0";
            t.style.transform = "translateY(8px)";
            setTimeout(() => t.remove(), 200);
        }, 1400);
    }

    /* ── Utilities ── */
    function escapeHtml(s) {
        return String(s).replace(/[&<>"']/g, c => (
            { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]
        ));
    }

    // Maps a handful of common virtual-key codes to readable names.
    function vkName(vk) {
        const map = {
            8: "Back", 9: "Tab", 13: "Enter", 16: "Shift", 17: "Ctrl", 18: "Alt",
            20: "Caps", 27: "Esc", 32: "Space", 45: "Insert", 46: "Del",
            37: "Left", 38: "Up", 39: "Right", 40: "Down",
        };
        if (map[vk]) return map[vk];
        if (vk >= 65 && vk <= 90)  return String.fromCharCode(vk);          // A-Z
        if (vk >= 48 && vk <= 57)  return String.fromCharCode(vk);          // 0-9
        if (vk >= 112 && vk <= 123) return "F" + (vk - 111);               // F1-F12
        return "0x" + vk.toString(16).toUpperCase();
    }

    /* ── Mock backend for standalone/design preview ── */
    function installMockBackend() {
        const mock = {
            modules: [
                // No combat/movement exploits — visual / HUD / utility only.
                { name: "AutoSprint", description: "Always sprint while moving forward", category: "Movement", enabled: true, keybind: 0,
                  settings: [ { id: "omni", label: "Sprint sideways/backwards", type: "bool", value: false } ] },
                { name: "Sprint", description: "Toggle sprint on", category: "Movement", enabled: false, keybind: 0, settings: [] },
                { name: "Sneak", description: "Toggle or hold sneak", category: "Movement", enabled: false, keybind: 0,
                  settings: [ { id: "toggle", label: "Toggle mode", type: "bool", value: false } ] },
                { name: "Freelook", description: "Look around without turning your character", category: "Movement", enabled: false, keybind: 86, settings: [] },
                { name: "Fullbright", description: "Brightens the world to remove darkness", category: "Visual", enabled: true, keybind: 66,
                  settings: [ { id: "gamma", label: "Gamma", type: "float", value: 1.0, min: 0, max: 1, step: 0.05 } ] },
                { name: "Block Outline", description: "Customize the block selection outline", category: "Visual", enabled: false, keybind: 0,
                  settings: [ { id: "thickness", label: "Thickness", type: "float", value: 1.0, min: 0.5, max: 4, step: 0.1 } ] },
                { name: "Chunk Borders", description: "Show chunk boundary lines", category: "Visual", enabled: false, keybind: 71, settings: [] },
                { name: "Hurt Color", description: "Customize the damage screen tint", category: "Visual", enabled: false, keybind: 0, settings: [] },
                { name: "Motion Blur", description: "Per-frame motion blur", category: "Visual", enabled: false, keybind: 0,
                  settings: [ { id: "amount", label: "Amount", type: "float", value: 0.5, min: 0, max: 1, step: 0.05 } ] },
                { name: "Chroma", description: "RGB color cycling for client elements", category: "Visual", enabled: false, keybind: 0,
                  settings: [ { id: "speed", label: "Speed", type: "float", value: 1.0, min: 0.1, max: 5, step: 0.1 } ] },
                { name: "Nametag Modifier", description: "Adjust nametag scale and visibility", category: "Visual", enabled: false, keybind: 0,
                  settings: [ { id: "scale", label: "Scale", type: "float", value: 1.2, min: 0.5, max: 3, step: 0.1 } ] },
                { name: "Durability Warning", description: "Warn when armor or tools are low", category: "Player", enabled: false, keybind: 0,
                  settings: [ { id: "threshold", label: "Threshold %", type: "int", value: 10, min: 1, max: 50 } ] },
                { name: "Low Health Indicator", description: "Screen indicator at low health", category: "Player", enabled: false, keybind: 0,
                  settings: [ { id: "threshold", label: "Threshold (HP)", type: "int", value: 6, min: 1, max: 19 } ] },
                { name: "Nickname", description: "Change your displayed name locally", category: "Player", enabled: false, keybind: 0, settings: [] },
                { name: "Bow Sensitivity", description: "Lower sensitivity while drawing a bow", category: "Player", enabled: false, keybind: 0,
                  settings: [ { id: "multiplier", label: "Multiplier", type: "float", value: 0.7, min: 0.1, max: 1, step: 0.05 } ] },
                { name: "Coordinates", description: "Displays your XYZ position", category: "World", enabled: true, keybind: 0,
                  settings: [ { id: "dimensionScale", label: "Show Nether (÷8)", type: "bool", value: false } ] },
                { name: "Waypoints", description: "Place and view waypoints", category: "World", enabled: false, keybind: 0, settings: [] },
                { name: "Waila", description: "Show info about the block you're looking at", category: "World", enabled: false, keybind: 0, settings: [] },
                { name: "Time Changer", description: "Override the client-side time of day", category: "World", enabled: false, keybind: 0,
                  settings: [ { id: "time", label: "Time", type: "int", value: 6000, min: 0, max: 24000 } ] },
                { name: "Combo Counter", description: "Show your current hit combo", category: "Combat", enabled: false, keybind: 0, settings: [] },
                { name: "Totem Counter", description: "Count totems in your inventory", category: "Combat", enabled: false, keybind: 0, settings: [] },
                { name: "Behind You", description: "Alert when a player is behind you", category: "Combat", enabled: false, keybind: 0, settings: [] },
                { name: "Zoom", description: "Hold to zoom in", category: "Misc", enabled: false, keybind: 67,
                  settings: [ { id: "level", label: "Zoom level", type: "float", value: 4.0, min: 1, max: 10, step: 0.5 } ] },
                { name: "FOV Changer", description: "Override your field of view", category: "Misc", enabled: false, keybind: 0,
                  settings: [ { id: "fov", label: "FOV", type: "int", value: 90, min: 30, max: 130 } ] },
                { name: "Keystrokes", description: "On-screen WASD and click keystrokes", category: "Misc", enabled: false, keybind: 0, settings: [] },
                { name: "Discord Presence", description: "Show Glacier in your Discord status", category: "Misc", enabled: false, keybind: 0, settings: [] },
                { name: "Ping Display", description: "Show your connection ping", category: "Misc", enabled: false, keybind: 0, settings: [] },
                { name: "Tab List", description: "Custom tab / player list", category: "Misc", enabled: false, keybind: 0, settings: [] },
                { name: "Watermark", description: "Shows the Glacier client name and version", category: "Misc", enabled: true, keybind: 0, settings: [] },
                { name: "Hue Changer", description: "Shift the hue of the entire screen", category: "Visual", enabled: false, keybind: 0,
                  settings: [ { id: "shift", label: "Hue shift", type: "float", value: 0, min: 0, max: 360, step: 5 } ] },
                { name: "Block Highlight", description: "Highlight the block you're looking at", category: "Visual", enabled: false, keybind: 0, settings: [] },
                { name: "Minimap", description: "Top-down minimap of your surroundings", category: "World", enabled: false, keybind: 0,
                  settings: [ { id: "scale", label: "Scale", type: "float", value: 1.0, min: 0.5, max: 3, step: 0.1 } ] },
                { name: "Block Counter", description: "Count blocks of a chosen type nearby", category: "World", enabled: false, keybind: 0, settings: [] },
                { name: "Health Warning", description: "Sound and screen flash at low health", category: "Player", enabled: false, keybind: 0,
                  settings: [ { id: "threshold", label: "Threshold (HP)", type: "int", value: 6, min: 1, max: 19 } ] },
                { name: "Held Item Info", description: "Show details of your held item", category: "Player", enabled: false, keybind: 0, settings: [] },
                { name: "Bow Indicator", description: "Show bow charge progress", category: "Combat", enabled: false, keybind: 0, settings: [] },
                { name: "Hit Marker", description: "Show a marker when you land a hit", category: "Combat", enabled: false, keybind: 0, settings: [] },
                { name: "Gap Counter", description: "Count golden apples in your inventory", category: "Combat", enabled: false, keybind: 0, settings: [] },
                { name: "Potion HUD", description: "Show your active potion effects", category: "Misc", enabled: false, keybind: 0, settings: [] },
                { name: "Clock", description: "Real-time clock on screen", category: "Misc", enabled: false, keybind: 0,
                  settings: [ { id: "seconds", label: "Show seconds", type: "bool", value: true } ] },
                { name: "Toggle Sounds", description: "Play a sound when a module toggles", category: "Misc", enabled: false, keybind: 0, settings: [] },
                { name: "Armor HUD", description: "Displays your equipped armor and durability", category: "Visual", enabled: true, keybind: 0,
                  settings: [
                      { id: "x",          label: "X position", type: "int", value: 8, min: 0, max: 1920 },
                      { id: "y",          label: "Y position", type: "int", value: 8, min: 0, max: 1080 },
                      { id: "scale",      label: "Scale", type: "float", value: 1.0, min: 0.5, max: 3, step: 0.1 },
                      { id: "vertical",   label: "Vertical layout", type: "bool", value: true },
                      { id: "durability", label: "Show durability", type: "bool", value: true },
                  ] },
                { name: "Inventory HUD", description: "Shows your inventory as an on-screen grid", category: "Visual", enabled: true, keybind: 0,
                  settings: [
                      { id: "x",      label: "X position", type: "int", value: 8, min: 0, max: 1920 },
                      { id: "y",      label: "Y position", type: "int", value: 220, min: 0, max: 1080 },
                      { id: "scale",  label: "Scale", type: "float", value: 1.0, min: 0.5, max: 3, step: 0.1 },
                      { id: "hotbar", label: "Include hotbar", type: "bool", value: false },
                      { id: "counts", label: "Show stack counts", type: "bool", value: true },
                      { id: "bg",     label: "Background opacity", type: "float", value: 0.5, min: 0, max: 1, step: 0.05 },
                  ] },
                { name: "CPS Counter", description: "Displays your clicks per second", category: "Misc", enabled: true, keybind: 0, settings: [] },
                { name: "FPS Counter", description: "Displays your current frame rate", category: "Misc", enabled: true, keybind: 0,
                  settings: [
                      { id: "smooth",  label: "Smoothing", type: "float", value: 0.90, min: 0, max: 0.99, step: 0.01 },
                      { id: "showAvg", label: "Show 1% low", type: "bool", value: true },
                  ] },
            ],
        };
        window.glacier.toggleModule = (name) => {
            const m = mock.modules.find(x => x.name === name);
            if (m) m.enabled = !m.enabled;
            return m ? m.enabled : false;
        };
        window.glacier.setSetting = (mod, id, val) => {
            const m = mock.modules.find(x => x.name === mod);
            const s = m && m.settings.find(x => x.id === id);
            if (s) s.value = val;
        };
        window.glacier.setKeybind = (mod, vk) => {
            const m = mock.modules.find(x => x.name === mod);
            if (m) m.keybind = vk;
        };
        window.glacier.closeMenu = () => window.glacier.setMenuVisible(false);

        // ── Live HUD driver (preview only) ──
        // Reads the mock modules' own enabled flags + settings so toggling a HUD
        // module or dragging its sliders in the menu updates the on-screen HUD
        // exactly as the native client would.
        const isOn = (name) => { const m = mock.modules.find(x => x.name === name); return m && m.enabled; };
        const sv = (name, id, dflt) => {
            const m = mock.modules.find(x => x.name === name);
            const s = m && m.settings.find(x => x.id === id);
            return s ? s.value : dflt;
        };
        const armorSlots = [
            { valid: true, name: "Netherite Helmet",   count: 1, dura: 400, max: 444, ench: true },
            { valid: true, name: "Diamond Chestplate", count: 1, dura: 380, max: 528, ench: false },
            { valid: false },
            { valid: true, name: "Diamond Boots",       count: 1, dura: 64,  max: 429, ench: true },
        ];
        const invItems = ["Diamond Sword", "Bow", "Golden Apple", "Cobblestone", "Ender Pearl", "Totem"];
        const invSlots = Array.from({ length: 27 }, (_, i) =>
            i % 5 === 0
                ? { valid: true, name: invItems[(i / 5) % invItems.length], count: 1 + (i % 16) }
                : { valid: false });

        let tick = 0;
        setInterval(() => {
            tick++;
            const huds = [];
            const fps = 142 + Math.round(Math.sin(tick / 7) * 7);
            const cps = 6 + (tick % 5);
            if (isOn("FPS Counter")) {
                const w = { type: "text", id: "fps", anchor: "top-right", label: "FPS", value: String(fps) };
                if (sv("FPS Counter", "showAvg", true)) w.sub = "1% low " + (fps - 38);
                huds.push(w);
            }
            if (isOn("CPS Counter")) {
                huds.push({ type: "text", id: "cps", anchor: "top-right", label: "CPS", value: String(cps) });
            }
            if (isOn("Armor HUD")) {
                huds.push({ type: "armor",
                    x: sv("Armor HUD", "x", 8), y: sv("Armor HUD", "y", 8),
                    scale: sv("Armor HUD", "scale", 1),
                    vertical: sv("Armor HUD", "vertical", true),
                    durability: sv("Armor HUD", "durability", true),
                    slots: armorSlots });
            }
            if (isOn("Inventory HUD")) {
                huds.push({ type: "inventory",
                    x: sv("Inventory HUD", "x", 8), y: sv("Inventory HUD", "y", 220),
                    scale: sv("Inventory HUD", "scale", 1), columns: 9,
                    counts: sv("Inventory HUD", "counts", true),
                    bg: sv("Inventory HUD", "bg", 0.5),
                    slots: invSlots });
            }
            if (isOn("Watermark")) {
                huds.push({ type: "text", id: "watermark", anchor: "top-left",
                    label: "", value: "Glacier", sub: "v1.0 · MCBE 1.26" });
            }
            if (isOn("Coordinates")) {
                const x = 128 + Math.round(Math.sin(tick / 20) * 12);
                const z = -640 - Math.round(Math.cos(tick / 20) * 12);
                const w = { type: "text", id: "coords", anchor: "bottom-left",
                    label: "XYZ", value: `${x}, 72, ${z}` };
                if (sv("Coordinates", "dimensionScale", false)) {
                    w.sub = `Nether ${Math.round(x / 8)}, ${Math.round(z / 8)}`;
                }
                huds.push(w);
            }
            window.glacier.onHud(huds);
        }, 100);

        // Deliver the snapshot. In the design preview we auto-open the menu so
        // both layers show; in the standalone test build we start closed (HUD
        // only) so you open it yourself with Insert, like the real client.
        setTimeout(() => {
            window.glacier.onState(mock);
            if (!STANDALONE) window.glacier.setMenuVisible(true);
        }, 0);
    }
})();
