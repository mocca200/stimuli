# MVT — Audio-Reactive Visualization Suite

A C++20 / JUCE 9 audio-reactive visualizer, VST3 plugin, standalone app, and
engine-first UI designer.

## Status

Milestone M0–M4 in progress:

- [x] CMake scaffold, JUCE 9.0.5 pin, Catch2 v3
- [x] Lock-free SPSC ring buffer and triple buffer with stress tests
- [x] Headless flex layout parser and solver with golden tests
- [x] Headless tokenised theme registry with alias/cycle/variant/override support
- [x] DSP analysis pipeline: FFT, STFT features, chroma, HPSS, vectorscope
- [x] Audio-thread capture with allocation-free tests
- [x] Skin loader (`manifest.json`, `layout.json`, `theme.json`)
- [x] Layout-driven renderer with spectrum, spectrogram, waveform, vectorscope,
      chroma wheel, and HPSS views
- [x] Plugin editor and designer preview driving the same render engine
- [x] Photosensitivity safety gate: sliding-window flash limiter, frame
      measurement, and attenuation as the final framebuffer authority
- [x] Radial spectrum, Tonnetz, chroma self-similarity, and CPU particle field
      renderers
- [x] Designer workspace: hierarchy tree with drag-and-drop, property
      inspector, direct manipulation, undo/redo, saving, hot reload
- [ ] GPU particle rendering via OpenGL
- [ ] Explicit Direct2D/OpenGL backend selection
- [ ] MP4 export
- [x] Constant-Q transform: 84 log-spaced bands feeding a CQT-derived chroma

## Build

Prerequisites: CMake ≥ 3.22, a C++20 compiler, and Git. On Windows the project
is tested with VS 2022 BuildTools (MSVC 14.44) and the Windows 10/11 SDK.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

Enable only what you need:

```powershell
cmake -S . -B build -DMVT_BUILD_PLUGIN=OFF -DMVT_BUILD_DESIGNER=OFF
```

The first configure downloads JUCE 9.0.5 and Catch2. Set `MVT_JUCE_SOURCE_DIR`
or `MVT_CATCH2_SOURCE_DIR` to use local checkouts.

Release artefacts (MSVC x64, no symbols):

| Artefact | Size |
|---|---|
| `MVT Visualizer.vst3` | 8.2 MB |
| `MVT Visualizer.exe` (standalone) | 10.3 MB |
| `MVT Designer.exe` | 7.2 MB |

Ship the binaries with the `Assets/` folder beside them. PDBs, `.ilk` files,
and static libraries are build intermediates and are not part of a package.

## Tools

```powershell
# Print the solved layout tree for a skin document.
build\Tools\Debug\mvt_layout_dump.exe Assets/layouts/default.json `
    --theme Assets/themes/default.json --width 1280 --height 720

# Render a skin with deterministic demo analysis to a PNG.
build\Tools\Debug\mvt_render_preview.exe --skin Assets/skins/showcase `
    --out preview.png --width 1600 --height 900
```

Skins live in `Assets/skins/<name>/` and contain `manifest.json`, `layout.json`,
`theme.json`, and optional assets. Set `MVT_SKIN_DIR` to load a different skin
folder in the plugin or designer.

Visualizer nodes use these `type` values in `layout.json`:

| Type | Description |
|---|---|
| `spectrum` | Filled spectrum analyser |
| `spectrogram` | Scrolling 180-frame spectrogram |
| `waveform` | Time-domain waveform with envelope |
| `vectorscope` | Stereo goniometer |
| `chromawheel` | Twelve-bin chroma wheel |
| `hpss` | Harmonic/percussive dual layer |
| `radialspectrum` | Spectrum wrapped around a circle |
| `tonnetz` | Pitch-class lattice |
| `selfsimilarity` | 64-frame chroma similarity matrix |
| `particles` | CPU fallback 3D particle field |

## Designer

The designer renders the same skin, layout solver, theme registry, DSP snapshots,
and safety gate as the plugin, so a skin looks identical in both.

- **Canvas** — click to select, drag to move, eight resize handles, marquee to
  pick the topmost intersecting node, snap to sibling edges and parent padding.
  Arrow keys nudge (1 px, 10 px with Shift), Del removes, Ctrl/Cmd+D duplicates,
  Ctrl/Cmd+G groups the selection into a container. Moving or resizing turns the
  node into an absolutely positioned one (inset + explicit size).
- **Hierarchy** — the node tree with drag-and-drop reparenting into containers;
  drops that would create a cycle are rejected. Group, Duplicate, and Delete
  buttons sit under the tree.
- **Inspector** — id, type, text, every style field (sizes, margins, padding,
  inset, flex, flow), theme token picker with live colour overrides, and a live
  readout of the analysis snapshot.
- **Persistence and reload** — Save writes `layout.json`, `theme.json`, and a
  minimal `manifest.json` when the folder lacks one. The document model also
  provides bounded undo/redo. Hot reload polls the skin folder's modification
  stamps (JUCE 9 has no `juce::FileWatcher`) and reloads after a save unless the
  document has unsaved edits; the status bar says which.

The toolbar holds Open, Save, Reload, Undo, Redo, Pause, and a design viewport
size (fill, 1280x720, 1600x900, 1920x1080). The status line reports the active
skin, selection, dirty state, and whether the last frame was attenuated by the
photosensitivity gate. `--snapshot <file.png>` renders the whole shell offscreen
and exits, which is how the UI is smoke-tested.

## Architecture

```text
Engine/Core      lock-free SPSC ring, triple buffer, atomic meters
Engine/Layout    JSON schema parser, flex solver, hit testing
Engine/Theme     token registry, aliases, variants, runtime overrides
Engine/DSP       STFT/FFT analysis, HPSS, features
Engine/Safety    flash limiter and frame metrics
UIEngine         skin loader, palette, layout-driven renderer, safety gate, views
Plugin           VST3 + standalone processor/editor
Designer         engine-first designer preview
Tests            headless Catch2 suites
```

The layout and theme engines have no GUI dependency: they link `juce_core` for
JSON only, and every test runs without creating a window or initialising a
message loop.

## Photosensitivity safety

Every frame is rendered into an offscreen buffer and passed through
`FrameSafetyGate` before it reaches the window or encoder. The gate measures
mean linear relative luminance (sRGB to linear, Rec. 709 weights) and a
simplified saturated-red share, then lets `FlashLimiter` count opposing
transitions in a one-second sliding window. Six opposing transitions are
allowed per second (three flashes); anything beyond that is blended back
toward the last accepted frame until it is below the 10% significant-change
threshold.

The gate treats the whole frame as one visual field. This is deliberately
conservative for full-screen flashes but does not yet implement the WCAG
25%-of-10-degree-field area test or a blockwise worst-region measure. Steady
content below the threshold passes through unchanged.

## Real-time contract

The audio callback is delay-neutral and bit-transparent. It may only:

- read the host buffer,
- write samples into preallocated SPSC rings,
- update atomic meters.

No allocation, free, lock, log, file I/O, or blocking system call is permitted
in `processBlock`. Analysis, rendering, and encoding live on other threads.

## Budgets

| Area | Target |
|---|---|
| Audio callback | ≤ 0.5% of one core (design target ≈ 0.1%) |
| DSP + analysis worker | ≤ 3% of one core at Standard tier |
| UI paint | p99 < 8 ms at 1080p editor size |
| Visual latency | ≤ 35 ms |

## Licensing

This project is intended to be distributed under **AGPLv3**. JUCE is
dual-licensed AGPLv3/commercial; the VST3 SDK and other dependencies have their
own terms. See `THIRD_PARTY_NOTICES.md`. A commercial JUCE licence may be
required for closed-source distribution.

## Design decisions

- The CQT runs on its own 8192-point frame at a quarter of the STFT hop rate. A
  2048-point frame resolves ~23 Hz, which is wider than a semitone below 250 Hz
  and folds low notes onto the wrong pitch class; at 8192 points every tone from
  C2 to A7 folds correctly (asserted in `Tests/CqtTests.cpp`). Bands narrower
  than the bin spacing interpolate instead of reporting zero. Measured cost is
  0.29% of one core, and `AnalysisSnapshot` carries both `cqt` and `chromaCqt`.
- Nunito is the only UI family. Normal copy is set in the Nunito Bold face and
  emphasised copy in Nunito Black; both faces are embedded via BinaryData and
  resolve identically in the plugin, standalone app, Designer, and render tools.
- JSON is the canonical schema; YAML is deferred. `LayoutWriter` is the exact
  inverse of `LayoutParser`, and a parse of a written document reproduces the
  same canonical output.
- Skins are folders: `manifest.json`, `layout.json`, `theme.json`, `assets/`.
- Native Direct2D is the primary Windows backend, with optional OpenGL and a
  guaranteed software fallback.
- The photosensitivity limiter has final authority over the framebuffer.
- Deterministic offline rendering drives time from sample counts, never the
  wall clock.
