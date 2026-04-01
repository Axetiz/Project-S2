# OrbitSimLite

OrbitSimLite is a small C++17 project for experimenting with 2D Newtonian orbits. It consists of a reusable simulation/renderer library and a couple of example demos (solar system, binary stars, and figure‑eight trajectories) built on top of SFML.

## What it does

- Simulates point-mass bodies under Newtonian gravity in 2D (SI units, double precision).
- Supports two integrators: symplectic Euler and a simple RK4 step.
- Handles basic collision rules:
  - planet–planet collisions: pause + option to remove the lighter body,
  - body–star collisions: non‑star body is removed immediately,
  - natural satellites can be exempted from some collision rules.
- Renders bodies as circles with fading trails in an SFML window.
- Continuously exports the **current** simulation state to `bodies.json` (no history), including named bodies and kinematic data.
- Provides several demos:
  - `demo_solar_system`: Sun–Mercury–Venus–Earth–Moon–Mars + one experimental planet.
  - `demo_binary_stars`: two equal‑mass stars orbiting their common barycenter.
  - `demo_threebody_figure8`: three equal masses following the same figure‑eight choreography.

Internally the library uses a clear separation between the physics core (`Vec2`, `Body`, `Physics`, `Simulator`) and the SFML renderer, so you can link only the simulation parts into other applications or game/visualisation engines.

## Requirements

- C++17 compiler (GCC, Clang, or MSVC).
- CMake ≥ 3.15.
- SFML ≥ 2.5 (`system`, `window`, `graphics` components).

On Debian/Ubuntu (example):

```bash
sudo apt install libsfml-dev cmake build-essential
```

## Building

From the project root (where `CMakeLists.txt` lives):

```bash
mkdir -p build
cd build
cmake .. -DORBITSIMLITE_BUILD_DEMO=ON
cmake --build . --config Release
```

This builds:

- static library `liborbitsimlite.a`,
- demo executables `demo_solar_system`, `demo_binary_stars`, `demo_threebody_figure8`,
- optional numerical test runner `orbitsimlite_tests` (see below).

## Running the demos

From the `build` directory:

```bash
./demo_solar_system      # solar system style demo
./demo_binary_stars      # two‑suns binary orbit demo
./demo_threebody_figure8 # three‑body figure‑eight choreography demo
```

Both demos ask for a time‑step multiplier on startup (scales the base dt used by the integrator).

### Controls (both demos)

- `SPACE` – pause/resume simulation.
- `R` – reset bodies to initial configuration and reset simulated time.
- `ESC` – exit.
- `C` – when a planet–planet collision is detected, remove the lighter body and continue.

The window title displays the accumulated simulated time in **Earth years**. The view auto‑adjusts on resize so circles remain round (no stretching into ovals).

## Library usage and benefits

At its core OrbitSimLite provides:

- `Vec2`: a small 2D vector type used throughout the physics.
- `Body`: a point mass with position/velocity/acceleration and basic rendering attributes.
- `Physics`: stateless functions for Newtonian gravity and Euler/RK4 steps.
- `Simulator`: owns a list of `Body` objects, steps them forward in time, and exposes the current state.
- `Renderer`: optional SFML component that visualises a `Simulator` instance and exports JSON.

Typical usage in your own application:

```cpp
orbitsimlite::Simulator sim(orbitsimlite::Physics::DefaultG, 3600.0, orbitsimlite::Integrator::RK4);
sim.set_substeps(12); // improve stability for tight orbits

orbitsimlite::Body sun(/*mass*/ 1.989e30,
                       /*pos*/  {0.0, 0.0},
                       /*vel*/  {0.0, 0.0},
                       /*radius*/ 30.0,
                       /*color*/ orbitsimlite::rgb_u32(255, 255, 0));
sim.add_body(sun);

// Advance the simulation e.g. in a game loop
sim.step();
const auto& bodies = sim.get_bodies();
```

Where it can be useful:

- teaching/learning: introducing numerical integration, N‑body dynamics, and stability issues;
- prototyping: quickly testing orbit logic before porting to a larger engine (Unity, Unreal, etc.);
- tools/pipelines: driving content (e.g. particle systems, AI or camera paths) from physically‑motivated trajectories using the JSON output.

Benefits:

- clear, small codebase that is easy to read and extend for coursework;
- separation between physics and rendering so you can reuse only what you need;
- numerical tests that give a quick, quantitative check of orbit accuracy.

## JSON output

Each frame the renderer writes a snapshot of the current bodies to `bodies.json` in the working directory (typically `build/` when running from there). The file contains only the latest state:

- per‑body name, mass, radius, packed colour,
- position, velocity, acceleration in SI units.

This is designed to be easy to consume from external tools/engines that want to drive logic based on a continuously changing set of physical parameters.

## Numerical tests

For basic verification of the physics layer, you can build and run the test target:

```bash
cmake -S . -B build -DORBITSIMLITE_BUILD_TESTS=ON
cmake --build build --target orbitsimlite_tests
./build/orbitsimlite_tests
```

The current tests report:

- agreement of the gravitational acceleration with an analytic one‑mass case,
- conservation of orbital radius and energy for a circular orbit integrated with RK4,
along with a percentage of time steps that remain within the specified tolerances.
