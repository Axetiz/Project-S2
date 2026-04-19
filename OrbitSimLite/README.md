# OrbitSimLite 2.0

OrbitSimLite 2.0 is the second-semester continuation of the original OrbitSimLite project. It is a C++17 educational N-body simulation and scientific visualization platform that combines Newtonian gravity, numerical integration, and an interactive SFML-based explorer.

The project keeps the original core idea intact and extends it with better visualization, richer interaction, and stronger presentation/debugging tools. The result is a more complete platform for demonstrating orbital motion, gravitational fields, and the practical use of numerical methods.

## Project focus

This version is built around three goals:

- simulate 2D Newtonian gravitational systems with reusable physics code,
- visualize scalar/vector behavior in a way that is understandable for users,
- support interactive experimentation through presets, editing tools, and live inspection.

## Current features

### Physics and simulation

- 2D point-mass Newtonian gravity using SI units and double precision.
- Two numerical methods:
  - `Euler`
  - `RK4`
- Configurable timestep, gravity constant, and substeps.
- Reusable `Simulator` API independent from rendering.

### Visualization

- Interactive SFML explorer with a side information panel.
- Three field modes:
  - `None`
  - `Arrows`
  - `Curved grid`
- Velocity and acceleration vector overlays.
- Selected-body trajectory prediction preview.
- Body trails stored in world coordinates, so camera motion does not corrupt paths.
- Curved gravitational grid that bends according to a softened field-based warp.

### Interaction and editing

- Presets for:
  - Solar System
  - Binary Stars
  - Figure-Eight
  - Sandbox / editable scene
- Live body selection and inspection.
- Add, remove, move, and edit bodies during simulation.
- Toggle body role as star or satellite.
- Collision modes:
  - `Prompt remove`
  - `Auto remove`
  - `Merge`
  - `Ignore`
- Runtime switching between integrators for comparison.

### Data export and testing

- Continuous export of current simulation state to `bodies.json`.
- Export of recent state history to `bodies_history.json`.
- Numerical test suite for the physics and simulator layers.

## Architecture

The codebase is split into a few clear parts:

- `Vec2`
  - small 2D vector math utility
- `Body`
  - simulation object with physical and visual properties
- `Physics`
  - gravitational acceleration and integrator steps
- `Simulator`
  - owns bodies and advances the system in time
- `Renderer`
  - visualizes the simulator, handles controls, and writes JSON output

This separation makes it easier to discuss the project academically: the physics core can be explained separately from the user-facing visualization layer.

## Repository structure

```text
OrbitSimLite/
├── CMakeLists.txt
├── README.md
├── examples/
│   ├── demo_binary_stars.cpp
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
│   ├── simulator.cpp
│   └── vec2.cpp
└── tests/
    └── physics_tests.cpp
```

## Build

Requirements:

- CMake `>= 3.15`
- C++17 compiler
- SFML `>= 2.5`

Build commands:

```bash
cmake -S . -B build -DORBITSIMLITE_BUILD_DEMO=ON -DORBITSIMLITE_BUILD_TESTS=ON
cmake --build build
```

## Run

From the project root:

```bash
./build/demo_solar_system
./build/demo_binary_stars
./build/demo_threebody_figure8
```

## Explorer controls

- `Space`: pause/resume
- `.`: single-step while paused
- `R`: reset current preset
- `1-4`: switch presets
- `+ / -`: increase or decrease time scale
- `F`: cycle field mode
- `I`: cycle integrator (`Euler` / `RK4`)
- `V`: toggle body velocity/acceleration vectors
- `P`: toggle trajectory prediction for selected body
- `M`: cycle collision mode
- `Left Click`: select body
- `Right Mouse Drag`: pan view
- `N`: add body at cursor
- `Delete / Backspace`: remove selected body
- `Arrow Keys`: move selected body
- `W A S D`: edit selected body velocity
- `Q / E`: decrease/increase mass
- `Z / X`: decrease/increase visual radius
- `T`: toggle star flag
- `Y`: toggle satellite flag
- `C`: confirm lighter-body removal during prompted collision
- `Esc`: exit

## JSON outputs

When the interactive explorer runs, it continuously writes:

- `bodies.json`
  - current simulation snapshot
- `bodies_history.json`
  - recent simulation history frames

These files are useful for debugging, external analysis, or connecting the simulator to other tools.

## Tests

Build and run the tests with:

```bash
cmake --build build --target orbitsimlite_tests
./build/orbitsimlite_tests
```

The test suite currently checks:

- analytic gravity for simple cases,
- field superposition behavior,
- RK4 orbit quality,
- Euler vs RK4 accuracy comparison,
- momentum conservation in a two-body system,
- simulator time accumulation,
- substep behavior,
- center-of-mass conservation,
- mass-ratio motion behavior,
- simulator parameter roundtrips,
- reset-time behavior,
- body flag preservation.

## Current scope

At this stage, the main project idea is already implemented. The remaining improvements are mostly supplementary rather than conceptual:

- UI/UX polish,
- clearer presentation materials,
- more user testing and feedback collection,
- small visualization and documentation improvements.

That makes the current codebase a strong foundation for presentation, mentor reviews, and final project packaging.
