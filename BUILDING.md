# Building & injecting Glacier DLL

This walks through compiling `Glacier.dll` and loading it into Minecraft: Bedrock
Edition. Read the **whole** "Injecting" section first — Bedrock is a UWP app and
will silently refuse to load a DLL that doesn't have the right permissions.

> Bedrock is a moving target. Glacier resolves everything from signatures, but
> the seeded offsets target **1.26.x**. On a different build, expect the
> inventory/player HUDs to read wrong data until the offsets are reconfirmed.

---

## 1. Prerequisites

| Need | Notes |
|------|-------|
| **Windows 10/11 x64** | Same machine you run Bedrock on. |
| **Visual Studio 2022** | Install the **"Desktop development with C++"** workload. The free Community edition is fine. |
| **vcpkg** | Ships with VS 2022 (`C:\Program Files\Microsoft Visual Studio\2022\<ed>\VC\vcpkg`) or clone it yourself. Provides **MinHook**. |
| **Ultralight SDK 1.4.0** | Free, from <https://ultralig.ht>. Not on vcpkg — fetched manually (step 3). |
| **A UWP-capable DLL injector** | See "Injecting". |

---

## 2. Get the source ready

```powershell
git clone <your-fork-url> Glacier
cd Glacier
```

The repo already contains everything except the two binary dependencies.

---

## 3. Install dependencies

### MinHook (via vcpkg, manifest mode)

`vcpkg.json` lists MinHook, so VS restores it automatically when you build. To
pre-install it from a terminal:

```powershell
# adjust the path to your vcpkg
& "$env:VCPKG_INSTALLATION_ROOT\vcpkg.exe" install --triplet x64-windows-static-md
```

### Ultralight SDK (assembled from component archives)

Ultralight's website no longer offers an unauthenticated packaged-SDK download.
Instead, fetch the four **pinned component archives** that Ultralight's own CMake
build uses — they're public, and together they form the complete SDK. Run this
from the repo root (needs 7-Zip on PATH; `winget install 7zip.7zip`):

```powershell
$domain = ".sfo2.cdn.digitaloceanspaces.com"
$components = [ordered]@{
  "ultralightcore-bin" = "41a04e01"   # base types, platform/*.h, Config.h
  "webcore-bin"        = "e9fa2251"   # WebCore.dll + bin/resources (icudt, cacert)
  "ultralight-bin"     = "561bd7be"   # Ultralight.dll, Renderer/View headers
  "appcore-bin"        = "5f88113"    # AppCore.dll + AppCore headers
}
$sdk = "third_party/ultralight"
New-Item -ItemType Directory -Force $sdk | Out-Null
foreach ($name in $components.Keys) {
  $rev = $components[$name]
  Invoke-WebRequest "https://$name$domain/$name-$rev-win-x64.7z" -OutFile "$name.7z"
  & 7z x "$name.7z" -o"$sdk" -y    # the four archives merge into include/ lib/ bin/
  Remove-Item "$name.7z"
}
```

Resulting layout (what the `.vcxproj` expects at `third_party/ultralight`):

```
third_party/ultralight/
├── include/   (Ultralight/, Ultralight/platform/, AppCore/)
├── lib/       (UltralightCore.lib, WebCore.lib, Ultralight.lib, AppCore.lib)
└── bin/       (the four .dll runtime files + resources/{icudt67l.dat,cacert.pem})
```

> These revisions match the v1.3.0 / v1.4.0-beta **C++** API the code targets.
> Don't grab Ultralight 1.4's CAPI-only packaging — the `Ultralight/platform/*`
> headers this project includes won't be there.

---

## 4. Compile

### Option A — Visual Studio (easiest)

1. Open `Glacier.sln`.
2. Set the configuration dropdowns to **Release** / **x64**.
3. **Build → Build Solution** (Ctrl+Shift+B).

### Option B — command line

From a **"x64 Native Tools Command Prompt for VS 2022"** (or any shell with
MSBuild on PATH):

```powershell
msbuild Glacier.sln /m /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnableManifest=true
```

**Output:** `build\x64\Release\Glacier.dll`

> Don't have VS locally? Push to GitHub — `.github/workflows/build.yml` compiles
> `Glacier.dll` on a `windows-latest` runner and uploads it (plus the runtime
> DLLs and UI assets) as a build artifact you can download.

---

## 5. Package the runtime folder

Glacier loads its HTML UI from an `assets/` folder next to the DLL, and Ultralight
needs its runtime DLLs + `resources/` folder. Lay it out like this:

```
GlacierClient/
├── Glacier.dll
├── Ultralight.dll
├── UltralightCore.dll
├── WebCore.dll
├── AppCore.dll
├── resources/              (copied from third_party/ultralight/resources)
│   ├── icudt67l.dat
│   └── cacert.pem
└── assets/                 (copy the entire contents of web/)
    ├── index.html
    ├── style.css
    ├── main.js
    └── assets/fontawesome/ (bundled icon kit)
```

PowerShell to assemble it from a build:

```powershell
$out = "GlacierClient"
New-Item -ItemType Directory -Force $out, "$out\assets" | Out-Null
Copy-Item build\x64\Release\Glacier.dll $out
Copy-Item third_party\ultralight\bin\*.dll $out
Copy-Item -Recurse third_party\ultralight\bin\resources $out\resources
Copy-Item -Recurse web\* $out\assets
```

---

## 6. Injecting into Bedrock (the UWP part)

Minecraft Bedrock (`Minecraft.Windows.exe`) runs inside a **UWP AppContainer
sandbox**. Two consequences:

### a) Grant file permissions

The sandbox can only read/execute files that grant access to the
`ALL APPLICATION PACKAGES` group. Apply it to the **whole** folder:

```powershell
icacls GlacierClient /grant "*S-1-15-2-1:(OI)(CI)(RX)" /T
```

(`*S-1-15-2-1` is the SID for ALL APPLICATION PACKAGES — language-independent.)
Skip this and the game will fail to load the DLL or its assets with no obvious
error.

### b) Use a UWP-capable injector

A normal `CreateRemoteThread` + `LoadLibrary` injector usually works **only if**
the ACL above is applied. Manual-mapping injectors also work. Steps:

1. Launch Minecraft and load into a world (so the swap chain + ClientInstance
   exist — Glacier captures `ClientInstance` from a hook on its first update).
2. Point your injector at the process **`Minecraft.Windows.exe`**.
3. Inject `GlacierClient\Glacier.dll`.

### c) Use it

- A debug console window appears (logs).
- Press **`Insert`** to open/close the menu — the game **pauses** behind it.
- Toggle modules; set keybinds in the UI.
- Press **`End`** to cleanly unload the client.

---

## 7. Troubleshooting

| Symptom | Likely cause |
|--------|--------------|
| Game crashes on inject | Offsets don't match your Bedrock build. Confirm you're on 1.26.x, or update `seedBedrock()` in `src/memory/SignatureManager.cpp`. |
| Injects but no menu / blank overlay | `assets/` folder or Ultralight `resources/`/DLLs missing or lacking the ACL (step 6a). |
| Injector reports access denied | Missing ACL, or injector isn't UWP-aware. |
| `MH_Initialize`/hook errors in console | Another hook/overlay (e.g. Discord, RTSS) conflicting, or signatures not found for this build. |
| HUD shows but armor/inventory empty | Player/inventory offsets need reconfirming for your exact build (see note at top). |

> **Legal/ToS:** third-party clients can violate the game's Terms of Service and
> get your account banned. Use only where you're permitted to.
