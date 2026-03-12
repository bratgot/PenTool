# PenTool — Nuke NDK Freehand Annotation Plugin

Draw freehand notes, arrows, and scribbles directly on the Nuke Node Graph.
Strokes are saved as part of the `.nk` script via an `AnnotationNode`.

---

## Project layout

```
PenTool/
├── CMakeLists.txt          Build system (auto-detects Nuke 17 + Qt6)
├── Build-PenTool.ps1       One-command Windows build + install script
├── .gitignore
├── README.md
│
├── include/
│   ├── PenStroke.h         Stroke + StrokeSet data structures, serialisation
│   ├── PenOverlay.h        Qt overlay widget + floating toolbar
│   └── AnnotationNode.h    Nuke NoIop-derived node
│
├── src/
│   ├── PenStroke.cpp       Serialise / deserialise
│   ├── PenOverlay.cpp      Qt painting, mouse handling, toolbar
│   ├── AnnotationNode.cpp  Knobs, knob_changed, overlay activation
│   └── PenTool.cpp         Plugin entry (MMSInit / MMSLoaded)
│
├── python/
│   └── menu.py             Nuke menu + keyboard shortcut registration
│
└── build/                  Out-of-source build directory (git-ignored)
```

---

## How it works

1. `AnnotationNode` is a standard Nuke node (subclass of `NoIop`).  
   A hidden `String_knob` called `stroke_data` stores the serialised strokes
   and is saved/loaded with the `.nk` script automatically.

2. **Ctrl+Shift+P** (or the Properties panel button) calls
   `AnnotationNode::activatePenOverlay()`.

3. `DagFinder::findDagViewport()` walks the Qt widget tree to locate the
   Node Graph panel (`objectName` starting with `"DAG"`).

4. A `PenOverlay` top-level `Qt::Window` is created, sized to cover the DAG
   exactly, with `WA_TranslucentBackground` so the graph shows through.
   A 1-alpha fill prevents Windows from treating alpha-zero pixels as
   click-through.

5. **Strokes are stored in DAG-space coordinates** (polled from
   `nuke.zoom()` / `nuke.center()` via the Python C API loaded at runtime
   from Nuke's `python3xx.dll`). This means strokes stay anchored to the
   graph when you pan or zoom.

6. A floating `PenToolbar` sits just above the DAG panel and shrinks to fit
   if the panel is narrower than the toolbar's natural size.

7. Navigation inputs (middle mouse, alt-drag, scroll wheel) are forwarded
   to the DAG's native Win32 `HWND` via `PostMessage` so Nuke's pan/zoom
   remains fully functional while the overlay is active.

8. A 60 fps `QTimer` polls the DAG transform and repositions the overlay
   whenever the Nuke window is moved or resized.

9. **Ghost mode** — the overlay becomes fully transparent to mouse events
   (`WA_TransparentForMouseEvents`), strokes render at 40% opacity, and the
   toolbar dims to 40% opacity but remains clickable. Any button click exits
   ghost mode.

10. On each mouse release the stroke is appended to `StrokeSet` and
    immediately serialised into the node's `stroke_data` knob.

---

## Building (Windows / Nuke 17)

### Prerequisites

| Requirement | Version | Notes |
|---|---|---|
| Nuke 17 | 17.0v1 | Installed at `C:\Program Files\Nuke17.0v1` |
| Qt 6 SDK | 6.5.3 msvc2019_64 | Separate install — not included with Nuke |
| CMake | 3.28+ | |
| Visual Studio | 2022 (v143) | MSVC toolset, `/MD` CRT |

### Quick build

```powershell
cd C:\dev\PenTool
.\Build-PenTool.ps1
```

The script:
- Auto-detects Nuke root and Qt6 SDK
- Configures, builds, and copies `PenTool.dll` + `menu.py` to
  `%USERPROFILE%\.nuke\plugins\PenTool\`
- Patches `%USERPROFILE%\.nuke\init.py` to add the plugin path

### Manual CMake

```powershell
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DNUKE_ROOT="C:\Program Files\Nuke17.0v1" `
    -DCMAKE_PREFIX_PATH="C:\Qt\6.5.3\msvc2019_64" `
    -DQt6_DIR="C:\Qt\6.5.3\msvc2019_64\lib\cmake\Qt6"
cmake --build . --config Release
```

### Notes

- Nuke 17 ships `python3xx.dll` but **no** `python3xx.lib`. The plugin
  loads Python symbols at runtime via `GetProcAddress` — no Python SDK
  needed.
- The Python C API is called to query `nuke.zoom()` and `nuke.center()`
  for DAG-space coordinate tracking.

---

## Installation

1. Run `Build-PenTool.ps1` — it handles everything automatically.

   **Or** manually:
   ```powershell
   $inst = "$env:USERPROFILE\.nuke\plugins\PenTool"
   New-Item -ItemType Directory -Force -Path $inst | Out-Null
   Copy-Item "build\Release\PenTool.dll" "$inst\PenTool.dll" -Force
   Copy-Item "python\menu.py"            "$inst\menu.py"     -Force
   ```

2. Ensure `%USERPROFILE%\.nuke\init.py` contains:
   ```python
   import nuke
   nuke.pluginAddPath(r'C:\Users\<you>\.nuke\plugins\PenTool')
   nuke.load('PenTool')
   ```

3. Restart Nuke.

---

## Usage

| Action | How |
|---|---|
| Enter drawing mode | `Ctrl+Shift+P` or **Edit > Pen Tool** |
| Draw | Left-click drag in the Node Graph |
| Navigate (pan/zoom) | Middle mouse / scroll / Alt+drag — works normally |
| Pick colour | Colour swatch in toolbar |
| Change width | Width slider in toolbar |
| Eraser | **Eraser** toggle button |
| Undo last stroke | `Ctrl+Z` or **Undo** button |
| Clear all | **Clear** button |
| Ghost mode | **Ghost** button — annotations track pan/zoom, overlay is click-through |
| Exit ghost mode | Click any toolbar button |
| Exit drawing mode | `Escape` or **Exit** button |
| Strokes saved? | Yes — serialised into the node's `stroke_data` knob, saved with `.nk` |

---

## Roadmap

- [ ] **Linux / macOS** — port the Win32-specific overlay window and
      `PostMessage` forwarding to X11 / Cocoa
- [ ] **Wacom pressure** — drive stroke width from `QTabletEvent::pressure()`
- [ ] **Text labels** — click-to-place text annotations
- [ ] **Per-frame strokes** — extend `stroke_data` to an `Array_knob`
- [ ] **Multiple DAG panels** — currently only the first panel found by
      `DagFinder` gets the overlay
