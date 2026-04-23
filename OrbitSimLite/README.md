# OrbitSimLite 2.0

`OrbitSimLite 2.0` is the implementation core of my `UFAR Project-S2`. It is a C++17 scientific simulation and visualization platform for 2D Newtonian N-body systems, built as a continuation of the original `OrbitSimLite` from `Project-S1`.

This version keeps the original idea of orbital simulation intact, but extends it into a more complete software product with interactive visualization, editable scenes, multiple output backends, and cleaner architecture for experimentation and presentation.

## Project goals

The current version is built around four practical goals:

- simulate 2D Newtonian gravitational systems with reusable physics code
- visualize motion, vectors, and field behavior in an understandable way
- support interactive experimentation through editing and scene management
- expose live machine-readable data for external tools through JSON and CSV

## Main features

### Simulation and numerical methods

- 2D Newtonian gravity with double-precision arithmetic
- configurable gravity constant, timestep, and substeps
- two numerical integrators:
  - `Euler`
  - `RK4`
- reusable `Simulator` API independent from the renderer

### Interactive visualization

- SFML-based explorer with a live side panel
- field modes:
  - `None`
  - `Arrows`
  - `Curved grid`
- velocity and acceleration vector overlays
- selected-body trajectory prediction
- body trails stored in world coordinates so camera movement does not corrupt them

### Interaction and scene editing

- body selection and inspection
- add, remove, move, and edit bodies during runtime
- mass, radius, and velocity editing from the keyboard
- star/satellite role toggling
- right-mouse view dragging
- collision modes:
  - `Prompt remove`
  - `Auto remove`
  - `Merge`
  - `Ignore`

### Presets and custom scenes

- `Solar System`
- `Binary Stars`
- `Figure Eight`
- `Sandbox`
- `Simulator`
  - empty plane for building scenes from scratch

### Data export and scene management

- live JSON snapshot export
- live CSV snapshot export
- combined output mode selection from the terminal
- one stable export file per enabled format during a run
- atomic file replacement so readers do not see empty intermediate files
- named scene save/load through `Exports/`
- overwrite confirmation on save
- delete confirmation from the import list

### Quality and testing

- numerical and simulator tests
- renderer split into smaller implementation files
- headless export mode for non-visual workflows

## Architecture

The codebase is intentionally divided into distinct layers so the project can be explained and maintained more easily.

### Core simulation layer

- `Vec2`
  - 2D vector math utility
- `Body`
  - physical body definition with simulation and display attributes
- `Physics`
  - gravitational acceleration and numerical integration logic
- `Simulator`
  - owns the bodies and advances the system in time

### Presentation and control layer

- `Renderer`
  - visualizes the current simulation state
  - manages interaction and editing
  - draws field overlays, vectors, trails, and side-panel UI
  - handles scene import/export overlays
  - writes JSON and CSV live snapshots

The renderer implementation is split by responsibility:

- `renderer_core.cpp`
  - renderer construction, shared helpers, interactive orchestration, headless run flow
- `renderer_interaction.cpp`
  - keyboard/mouse handling, body editing, scene switching, save/load interaction
- `renderer_collision.cpp`
  - collision policies and collision-resolution flow
- `renderer_draw.cpp`
  - fields, bodies, vectors, trails, prediction rendering, overlays, and side panel
- `renderer_io.cpp`
  - JSON/CSV export and named scene save/load logic

This split keeps the project easier to read than a single monolithic renderer file and makes it easier to discuss each concern separately.

## Repository structure

```text
OrbitSimLite/
├── CMakeLists.txt
├── README.md
├── examples/
│   ├── demo_binary_stars.cpp
│   ├── demo_cli.hpp
│   ├── demo_simulator.cpp
│   ├── demo_solar_system.cpp
│   └── demo_threebody_figure8.cpp
├── include/
│   ├── body.hpp
│   ├── physics.hpp
│   ├── renderer.hpp
│   ├── simulator.hpp
│   ├── utils.hpp
│   └── vec2.hpp
├── src/
│   ├── body.cpp
│   ├── physics.cpp
│   ├── renderer.cpp
│   ├── renderer_collision.cpp
│   ├── renderer_core.cpp
│   ├── renderer_draw.cpp
│   ├── renderer_interaction.cpp
│   ├── renderer_io.cpp
│   ├── simulator.cpp
│   └── vec2.cpp
└── tests/
    └── physics_tests.cpp
```

`renderer.cpp` remains in the source tree as a lightweight navigation note, while the active implementation lives in the split renderer files listed above.

## Requirements

- CMake `>= 3.15`
- C++17 compiler
- SFML `>= 2.5`

The build system prefers `SFML 3` when available and falls back to `SFML 2.5`.

## Build

From the `OrbitSimLite/` directory:

```bash
cmake -S . -B build -DORBITSIMLITE_BUILD_DEMO=ON -DORBITSIMLITE_BUILD_TESTS=ON
cmake --build build
```

## Run

Available demos:

```bash
./build/demo_solar_system
./build/demo_binary_stars
./build/demo_threebody_figure8
./build/demo_simulator
```

All demos start through a shared terminal launcher. At startup, the user chooses the active output backends:

- `s` for `SFML`
- `j` for `JSON`
- `c` for `CSV`

Examples:

- `sj`
- `sc`
- `jc`
- `sjc`

If `SFML` is not selected, the demo runs in headless mode. In that flow:

- the terminal can ask which preset to simulate
- the terminal asks for real-time export duration
- default duration is `10` seconds
- entering `INF` runs until the process is stopped

## Explorer controls

- `Space`
  - pause or resume simulation
- `.`
  - single-step while paused
- `R`
  - reset current scene
- `+ / -`
  - change time scale
- `F`
  - cycle field mode
- `I`
  - cycle numerical integrator
- `V`
  - toggle velocity and acceleration vectors
- `P`
  - toggle trajectory prediction for the selected body
- `M`
  - cycle collision mode
- `K`
  - open scene export dialog
- `L`
  - open scene import dialog
- `Left Click`
  - select a body
- `Right Mouse Drag`
  - pan the view
- `N`
  - add a body at the current cursor position
- `Delete / Backspace`
  - remove selected body
- `Arrow Keys`
  - move selected body
- `W A S D`
  - edit selected body velocity
- `Q / E`
  - decrease or increase mass
- `Z / X`
  - decrease or increase radius
- `T`
  - toggle star flag
- `Y`
  - toggle satellite flag
- `C`
  - confirm lighter-body removal in prompted collision mode
- `Esc`
  - close the current overlay or exit the app

## Output representations

One of the main ideas of this version is that a simulation can be represented in several ways at the same time.

### SFML

Used for:

- real-time visualization
- interaction and editing
- field and vector exploration
- demo and presentation workflows

### JSON

Used for:

- live structured state export
- external tools that want body names, positions, velocities, and flags
- integrations where a program reads the current simulation state continuously

### CSV

Used for:

- spreadsheets
- plotting
- flat tabular analysis

These modes can be enabled separately or together, depending on the workflow.

## Live export workflow

When JSON or CSV output is enabled, the renderer creates files inside:

- `ExportData/`

The file naming convention is:

- `bodies_<date>_<time>_<simulation-name>.<ext>`

Examples:

- `ExportData/bodies_20260423_145309_Figure_Eight.json`
- `ExportData/bodies_20260423_145309_Binary_Stars.csv`

Important behavior:

- one file per enabled format is created for the run
- the same file path is updated during the run
- writes are done through atomic replacement
- readers should always see a complete file, not a temporarily emptied one

This makes the export flow suitable for another visualization or analysis tool that reads the current simulation state live.

## Scene import and export

Scene files are stored separately from live snapshot files.

### `Exports/`

Stores user-managed scene files such as:

- `Exports/MySystem.json`

These scene files keep:

- scene name
- gravity
- timestep
- integrator
- substeps
- view scale
- all body definitions

### `ExportData/`

Stores automatically updated live snapshots such as:

- `ExportData/bodies_20260423_145309_Figure_Eight.json`

### In-app scene workflow

- `K`
  - opens the save dialog
  - asks for a scene name
  - checks whether the file already exists
  - allows overwrite confirmation or name change
- `L`
  - opens the load dialog
  - lets the user browse saved scenes
  - loads the selected scene
  - can also delete a selected saved scene with confirmation

## Tests

Build and run the test suite with:

```bash
cmake --build build --target orbitsimlite_tests
./build/orbitsimlite_tests
```

The tests currently cover:

- gravitational acceleration in simple cases
- field superposition
- Euler and RK4 behavior
- two-body conservation-oriented checks
- simulator parameter roundtrips
- reset behavior
- body-flag preservation

## Project status

The main software direction of `OrbitSimLite 2.0` is already implemented. The remaining improvements are mostly release-oriented rather than conceptual:

- UI polish
- clearer presentation material
- final user testing and feedback collection
- small supplementary features
- packaging and release cleanup

That makes the current codebase a strong base for final academic presentation, mentor review, and real-user validation.
