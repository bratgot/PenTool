# UsdAreaLight — Nuke NDK plugin

A C++ Nuke node that creates a **USD `RectLight`** (area light) with full
soft-shadow controls, ready to use in Nuke 14+ USD / Hydra pipelines.

---

## Features

| Category | Controls |
|---|---|
| **Shape** | Width, Height |
| **Photometric** | Intensity, Exposure (EV), Color, Normalize, Visible in Camera |
| **Soft Shadows** | Cast Shadows toggle, Shadow Blur *(penumbra width)*, Shadow Samples, Shadow Color, Shadow Distance, Shadow Falloff |
| **USD** | Configurable prim path (e.g. `/Lights/KeyLight`) |
| **Viewport** | Orange rect + direction arrow + dashed penumbra ring that scales with Shadow Blur |

---

## Why RectLight for soft shadows?

A `UsdLuxRectLight` is a **physically-based area emitter**.  
The size of the light relative to the shadow-caster determines how wide
the *penumbra* (soft edge) is — just like a real soft-box in photography.

Key parameters for soft shadows:

```
shadow:blur     → penumbra spread (0 = hard, 0.1+ = visibly soft)
shadow:samples  → stochastic samples; raise this to reduce noise in the penumbra
width / height  → larger light = softer shadow for nearby casters
```

---

## Build

```bash
# 1. Clone / place files in a directory
cd UsdAreaLight

# 2. Configure
cmake -B build \
      -DNUKE_ROOT=/usr/local/Nuke14.0v6 \
      -DUSD_ROOT=/usr/local/usd \
      -DPYTHON_ROOT=/usr/local/python3.10

# 3. Build
cmake --build build --config Release

# 4. Install to Nuke's plugin path
cmake --install build
```

Alternatively, copy `UsdAreaLight.so` (Linux) / `UsdAreaLight.dylib` (macOS)
/ `UsdAreaLight.dll` (Windows) to any directory in your `NUKE_PATH`.

---

## Usage in Nuke

1. In the **3D** workspace, **Tab → UsdAreaLight** to create the node.
2. Connect it downstream of a `Scene` node or upstream into a
   `ScanlineRender` / Karma USD render node.
3. Adjust **Width / Height** — larger = softer penumbra.
4. Under the **Shadow** tab:
   - Enable **Cast Shadows**
   - Set **Shadow Blur** (start at `0.02–0.05`)
   - Raise **Shadow Samples** to `32` or higher to clean up noise
5. The **dashed blue ring** in the viewport visualises penumbra spread.

---

## USD Prim Path

The generated prim lives at the path you set in the **USD → Prim Path**
knob (default `/Lights/AreaLight`).  You can inspect or override it with
`usdview` or Nuke's built-in USD stage inspector.

---

## Compatibility

| Component | Version |
|---|---|
| Nuke | 14.x (NDK USD headers required) |
| OpenUSD | 22.x – 24.x |
| C++ | 17 |
| OS | Linux x86_64, macOS arm64/x86_64, Windows 10/11 |

---

## License

MIT — see `LICENSE` file.
