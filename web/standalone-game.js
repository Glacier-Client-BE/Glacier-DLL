/* ============================================================================
   Glacier — standalone test harness
   ----------------------------------------------------------------------------
   Renders a lightweight, animated "game" behind the real Glacier UI so the
   client can be tested without Minecraft:
     • Insert / Right-Shift opens & closes the menu (handled in main.js).
     • While the menu is open the scene FREEZES and dims — the same "paused
       behind the menu" behaviour the injected DLL produces by swallowing game
       input.
     • Toggling modules in the menu visibly affects the scene, proving the UI
       actually drives module state:
         Fullbright  → brightens the world
         Motion Blur → motion trails
         Zoom        → zooms the camera in
         Chroma      → cycles the client accent color
   ============================================================================ */

(() => {
    "use strict";

    const canvas = document.getElementById("game");
    const ctx = canvas.getContext("2d");
    const hint = document.getElementById("open-hint");

    let W = 0, H = 0;
    function resize() {
        W = canvas.width = window.innerWidth * devicePixelRatio;
        H = canvas.height = window.innerHeight * devicePixelRatio;
        canvas.style.width = window.innerWidth + "px";
        canvas.style.height = window.innerHeight + "px";
    }
    window.addEventListener("resize", resize);
    resize();

    // ── Pause wiring ──
    let paused = false;
    window.addEventListener("glacier-menu", (e) => {
        paused = !!(e.detail && e.detail.open);
        if (hint) hint.style.opacity = paused ? "0" : "1";
    });

    // ── Module lookup helpers (read live state main.js exposes) ──
    const mods = () => window.glacier && window.glacier.modules || [];
    const on = (name) => { const m = mods().find(x => x.name === name); return !!(m && m.enabled); };
    const sv = (name, id, dflt) => {
        const m = mods().find(x => x.name === name);
        const s = m && m.settings && m.settings.find(x => x.id === id);
        return s ? s.value : dflt;
    };

    // ── Scene data ──
    const clouds = Array.from({ length: 6 }, () => ({
        x: Math.random(), y: 0.08 + Math.random() * 0.22, s: 0.6 + Math.random() * 0.8,
        v: 0.004 + Math.random() * 0.006,
    }));
    // A blocky skyline of columns (Minecraft-ish terrain).
    const cols = 28;
    const terrain = Array.from({ length: cols }, (_, i) =>
        0.55 + Math.sin(i * 0.6) * 0.05 + Math.sin(i * 0.27) * 0.06);
    const cubes = Array.from({ length: 5 }, (_, i) => ({
        x: 0.15 + i * 0.17, y: 0.4 + (i % 2) * 0.06, phase: i,
    }));

    let t = 0;
    let lastAccentHue = null;

    function applyChroma() {
        if (on("Chroma")) {
            const speed = sv("Chroma", "speed", 1.0);
            const hue = Math.round((t * 20 * speed) % 360);
            if (hue !== lastAccentHue) {
                document.documentElement.style.setProperty("--accent", `hsl(${hue}, 60%, 62%)`);
                document.documentElement.style.setProperty("--accent-hover", `hsl(${hue}, 60%, 70%)`);
                lastAccentHue = hue;
            }
        } else if (lastAccentHue !== null) {
            document.documentElement.style.removeProperty("--accent");
            document.documentElement.style.removeProperty("--accent-hover");
            lastAccentHue = null;
        }
    }

    function lerpColor(a, b, m) {
        return `rgb(${Math.round(a[0] + (b[0] - a[0]) * m)},${Math.round(a[1] + (b[1] - a[1]) * m)},${Math.round(a[2] + (b[2] - a[2]) * m)})`;
    }

    function drawScene() {
        const bright = on("Fullbright");
        const zoom = on("Zoom") ? sv("Zoom", "level", 4.0) * 0.25 + 0.9 : 1;

        // Motion blur: fade instead of clear.
        if (on("Motion Blur") && !paused) {
            ctx.fillStyle = `rgba(0,0,0,${0.18 * (1 - sv("Motion Blur", "amount", 0.5)) + 0.05})`;
            ctx.fillRect(0, 0, W, H);
        } else {
            ctx.clearRect(0, 0, W, H);
        }

        ctx.save();
        // Zoom about the centre.
        ctx.translate(W / 2, H / 2);
        ctx.scale(zoom, zoom);
        ctx.translate(-W / 2, -H / 2);

        // Sky gradient (brighter when Fullbright on).
        const top = bright ? [120, 180, 240] : [74, 124, 196];
        const bot = bright ? [200, 225, 245] : [150, 190, 225];
        const sky = ctx.createLinearGradient(0, 0, 0, H);
        sky.addColorStop(0, lerpColor(top, [30, 40, 70], 0));
        sky.addColorStop(1, `rgb(${bot[0]},${bot[1]},${bot[2]})`);
        ctx.fillStyle = sky;
        ctx.fillRect(0, 0, W, H);

        // Sun.
        ctx.fillStyle = bright ? "rgba(255,250,220,0.95)" : "rgba(255,235,170,0.85)";
        ctx.beginPath();
        ctx.arc(W * 0.8, H * 0.2, 46 * devicePixelRatio, 0, Math.PI * 2);
        ctx.fill();

        // Clouds.
        ctx.fillStyle = "rgba(255,255,255,0.8)";
        for (const c of clouds) {
            if (!paused) c.x = (c.x + c.v * 0.016) % 1.2;
            const cx = c.x * W - W * 0.1, cy = c.y * H, s = c.s * 40 * devicePixelRatio;
            for (const [dx, dy, r] of [[0, 0, 1], [s, 6, 0.8], [-s, 4, 0.7], [s * 0.5, -s * 0.4, 0.7]]) {
                ctx.beginPath();
                ctx.arc(cx + dx, cy + dy, s * r, 0, Math.PI * 2);
                ctx.fill();
            }
        }

        // Floating cubes (bob).
        for (const cu of cubes) {
            const bob = Math.sin(t * 1.5 + cu.phase) * 10 * devicePixelRatio;
            drawCube(cu.x * W, cu.y * H + bob, 30 * devicePixelRatio, ["#8a5a3c", "#6e4630", "#a06a44"]);
        }

        // Blocky terrain along the bottom.
        const bw = W / cols;
        for (let i = 0; i < cols; i++) {
            const gh = terrain[i] * H;
            // grass top
            ctx.fillStyle = bright ? "#7ec850" : "#5fa83e";
            ctx.fillRect(i * bw, gh, bw + 1, 16 * devicePixelRatio);
            // dirt
            ctx.fillStyle = bright ? "#9a6a44" : "#7a5436";
            ctx.fillRect(i * bw, gh + 16 * devicePixelRatio, bw + 1, H - gh);
            // subtle column edge
            ctx.fillStyle = "rgba(0,0,0,0.06)";
            ctx.fillRect(i * bw, gh, 1, H - gh);
        }

        ctx.restore();

        // Pause dim + label (drawn unscaled, full screen).
        if (paused) {
            ctx.fillStyle = "rgba(10,12,18,0.55)";
            ctx.fillRect(0, 0, W, H);
            ctx.fillStyle = "rgba(255,255,255,0.9)";
            ctx.font = `600 ${22 * devicePixelRatio}px 'Segoe UI', system-ui, sans-serif`;
            ctx.textAlign = "center";
            ctx.fillText("❚❚  Game paused", W / 2, H * 0.13);
            ctx.font = `400 ${13 * devicePixelRatio}px 'Segoe UI', system-ui, sans-serif`;
            ctx.fillStyle = "rgba(255,255,255,0.55)";
            ctx.fillText("Glacier menu open — press Insert to resume", W / 2, H * 0.13 + 30 * devicePixelRatio);
        }
    }

    function drawCube(x, y, s, [top, left, right]) {
        ctx.fillStyle = top;
        ctx.fillRect(x - s / 2, y - s / 2, s, s);
        ctx.fillStyle = left;
        ctx.fillRect(x - s / 2, y - s / 2, s * 0.22, s);
        ctx.fillStyle = right;
        ctx.fillRect(x + s / 2 - s * 0.22, y - s / 2, s * 0.22, s);
        ctx.strokeStyle = "rgba(0,0,0,0.18)";
        ctx.strokeRect(x - s / 2, y - s / 2, s, s);
    }

    function frame() {
        if (!paused) t += 0.016;
        applyChroma();
        drawScene();
        requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
})();
