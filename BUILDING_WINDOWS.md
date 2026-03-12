# Building PenTool on Windows with Nuke 17

## Prerequisites

| Tool | Version | Notes |
|---|---|---|
| Visual Studio | 2019 or 2022 | **Desktop C++ workload required** |
| CMake | 3.16+ | Add to PATH during install |
| Nuke 17 | 17.0v1+ | NDK ships inside the install — no separate download |

> **Do NOT use Visual Studio 2017 or older.** Nuke 17 was compiled with
> the MSVC v142 toolset (VS 2019). VS 2022 (v143) works fine.

---

## Step 1 — Open the correct Developer Command Prompt

Nuke is a 64-bit application. You must build with the x64 toolset.

Open **Start → Visual Studio 2022 → x64 Native Tools Command Prompt**.

Do **not** use the plain "Developer Command Prompt" (that defaults to x86).

---

## Step 2 — Clone / download the plugin

```bat
cd C:\dev
git clone https://github.com/yourname/PenTool.git
cd PenTool
```

---

## Step 3 — Configure with CMake

```bat
mkdir build
cd build

cmake .. ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DNUKE_ROOT="C:\Program Files\Nuke17.0v1"
```

**Generator string by Visual Studio version:**

| Visual Studio | `-G` string |
|---|---|
| VS 2019 | `"Visual Studio 16 2019"` |
| VS 2022 | `"Visual Studio 17 2022"` |

CMake will print a summary showing the Qt and NDK paths it found.
Verify they point inside your Nuke install:

```
-- NUKE_ROOT : C:/Program Files/Nuke17.0v1
-- Qt5 version : 5.15.x
-- Qt5 dir     : C:/Program Files/Nuke17.0v1/lib/cmake/Qt5/Qt5Core
-- NDK include : C:/Program Files/Nuke17.0v1/include
-- NDK lib     : C:/Program Files/Nuke17.0v1/DDImage.lib
```

---

## Step 4 — Build

```bat
cmake --build . --config Release
```

The output is `Release\PenTool.dll`.

> **Always build Release** (not Debug) when targeting Nuke.
> Nuke ships a Release-built CRT. Mixing Debug and Release CRTs on Windows
> causes heap corruption the moment memory allocated in one crosses into the other.

---

## Step 5 — Install into Nuke's plugin path

```bat
cmake --install . --prefix "%USERPROFILE%\.nuke\plugins\PenTool"
```

This copies `PenTool.dll` and `menu.py` into your user plugin directory.

---

## Step 6 — Register the plugin path

Add to `%USERPROFILE%\.nuke\init.py` (create it if it doesn't exist):

```python
import nuke
nuke.pluginAddPath(r'C:\Users\<YourName>\.nuke\plugins\PenTool')
```

---

## Step 7 — Verify

1. Launch Nuke 17.
2. Open the **Script Editor** (bottom panel).
3. Check the output tab for: `[PenTool] Plugin loaded.`
4. Press **Ctrl+Shift+P** — the pen overlay should activate on the Node Graph.
5. `AnnotationNode` should appear under **Other/** in the node menu (Tab key).

---

## Troubleshooting

### "The specified module could not be found" on load

Nuke can't find a DLL that `PenTool.dll` depends on (usually a Qt DLL).

```bat
:: From the x64 Native Tools prompt, inspect missing dependencies:
dumpbin /dependents Release\PenTool.dll
```

All Qt5*.dll files should resolve to `C:\Program Files\Nuke17.0v1\`.
If they resolve to a system Qt installation instead, the Qt search in CMake
found the wrong version. Force it:

```bat
cmake .. -DNUKE_ROOT="C:\Program Files\Nuke17.0v1" ^
         -DQt5_DIR="C:\Program Files\Nuke17.0v1\lib\cmake\Qt5"
```

### "MMSInit not found" / plugin silently ignored

The `.def` file export should handle this. As a fallback, confirm the exports:

```bat
dumpbin /exports Release\PenTool.dll
```

You should see both `MMSInit` and `MMSLoaded` listed as undecorated C names.

### Crash on load / heap corruption

You built with a Debug CRT (`/MDd`) or static CRT (`/MT`).
Confirm the build used `/MD` (Release dynamic CRT):

```bat
dumpbin /imports Release\PenTool.dll | findstr MSVCR
:: Should show VCRUNTIME140.dll or MSVCP140.dll — NOT MSVCR140D (Debug)
```

If you see the Debug variant, delete your `build/` folder and reconfigure.
The CMakeLists enforces `/MD` by rewriting the CMake flags — but if you
manually override `CMAKE_CXX_FLAGS` this can be undone.

### Qt version mismatch warning in Nuke's console

Nuke 17.0 and 17.1 both ship Qt **5.15.x**. If you accidentally linked
against a system Qt 5.12 or Qt 6, you'll see a warning and likely a crash.
Use `dumpbin /imports` to verify the Qt DLLs resolve to the Nuke directory.
