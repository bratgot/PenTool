# PenTool — Nuke NDK Freehand Annotation Plugin

Draw freehand notes, arrows, and scribbles directly on the Nuke Node Graph.
Strokes are saved as part of the `.nk` script via an `AnnotationNode`.

---

## Project layout

```
PenTool/
├── CMakeLists.txt          Build system
├── .gitignore
├── README.md
│
├── include/                Public headers (declarations only)
│   ├── PenStroke.h         Stroke + StrokeSet data structures, serialisation
│   ├── PenOverlay.h        Qt overlay widget + floating toolbar
│   └── AnnotationNode.h    Nuke NoIop-derived node
│
├── src/                    Implementation
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

4. A `PenOverlay` widget is created as a **child** of the DAG widget, sized
   to fill it, with `WA_TranslucentBackground` so the graph shows through.

5. A floating `PenToolbar` (`Qt::Tool` window) provides colour, width,
   eraser, undo, clear, and exit controls.

6. On each mouse release the stroke is appended to `StrokeSet` and
   immediately serialised back into the node's `stroke_data` knob.

7. **Escape** or the Exit button leaves pen mode.

---

## Building

### Prerequisites

| Requirement | Notes |
|---|---|
| Nuke 13/14/15 NDK | Headers + `libDDImage.so` |
| CMake 3.16+ | |
| Qt 5 | Use the version bundled with your Nuke install |
| GCC/Clang or MSVC | C++17 |

### Linux / macOS

```bash
mkdir build && cd build
cmake .. -DNUKE_ROOT=/usr/local/Nuke15.0v4
cmake --build . --config Release
cmake --install . --prefix ~/.nuke/plugins/PenTool
```

### Windows

```bat
mkdir build && cd build
cmake .. -DNUKE_ROOT="C:\Program Files\Nuke15.0v4" -G "Visual Studio 17 2022"
cmake --build . --config Release
cmake --install . --prefix "%USERPROFILE%\.nuke\plugins\PenTool"
```

> **Important (Linux):** Nuke is built with `_GLIBCXX_USE_CXX11_ABI=0`.
> The CMakeLists already sets this. Removing it will cause linker errors.

---

## Installation

1. Copy `PenTool.so` and `menu.py` into a Nuke plugin path directory,
   e.g. `~/.nuke/plugins/PenTool/`.

2. In `~/.nuke/init.py`:
   ```python
   nuke.pluginAddPath('/path/to/plugins/PenTool')
   ```

3. Restart Nuke.

---

## Usage

| Action | How |
|---|---|
| Enter drawing mode | `Ctrl+Shift+P` or **Edit > Pen Tool** |
| Draw | Left-drag |
| Pick colour | Colour swatch in toolbar |
| Change width | Width slider in toolbar |
| Eraser | "⌫ Eraser" toggle |
| Undo last stroke | `Ctrl+Z` or "↩ Undo" |
| Clear all | "🗑 Clear" |
| Exit drawing mode | `Escape` or "✕ Exit" |

---

## Known limitations & roadmap

- **DAG-space coordinates** — strokes are currently screen-space; pan/zoom
  after drawing will misalign them. Fix: map via `DAGView::toDAGCoordinates()`
  (newer NDK) or the QGLWidget modelview matrix.
- **Multiple DAG panels** — only the first panel found by `DagFinder` gets
  the overlay.
- **Wacom pressure** — `QTabletEvent::pressure()` can drive stroke width.
- **Text labels** — click-to-place text annotations.
- **Per-frame strokes** — extend `stroke_data` to an `Array_knob`.
