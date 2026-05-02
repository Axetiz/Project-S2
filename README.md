# Project-S2

`Project-S2` is my UFAR second-semester professional project. It continues the original `Project-S1` work by expanding `OrbitSimLite` into a larger scientific visualization and experimentation platform for 2D N-body simulation.

The repository is centered around practical software implementation: numerical simulation, real-time visualization, interactive scene editing, and machine-readable data export. The goal is not only to simulate orbital systems, but also to present the same physical system through multiple representations that are useful for different audiences.

## Repository overview

This repository contains one main application folder:

- `OrbitSimLite/`
  - the C++17 simulation library
  - SFML-based interactive explorer
  - terminal-controlled headless export flow
  - JSON and CSV live snapshot export
  - named scene save/load system
  - demo applications and physics tests

The detailed technical documentation lives in:

- [`OrbitSimLite/README.md`](/home/axetiz/Documents/UFAR/Project-S2/OrbitSimLite/README.md)

## What Project-S2 adds

Compared with the first-semester version, this stage focuses on turning the simulator into a more complete product:

- richer scientific visualization
- multiple field representations
- live body editing and scene management
- trajectory prediction
- runtime switching between numerical methods
- JSON and CSV export for external tools
- cleaner architecture through renderer separation
- CTest-based test execution
- lightweight runtime benchmarking for integrator comparison
- better support for testing, presentation, and user evaluation

## Representation layers

One of the main ideas of `Project-S2` is that the same simulation can be represented in more than one way:

- `SFML`
  - interactive visual exploration of motion, vectors, fields, and scene editing
- `JSON`
  - structured live state export for external programs and integrations
- `CSV`
  - flat numerical output for spreadsheets, plotting tools, and analysis

These representations are complementary rather than conflicting. Users can run one of them or combine them in the same session depending on the task.

## Current status

The core project idea is already implemented. At this stage, the remaining work is mostly supplementary:

- documentation polish
- presentation preparation
- real-user feedback collection
- minor UI/UX improvements
- release cleanup and packaging

## Quick start

Build and usage instructions for the actual simulator are in:

- [`OrbitSimLite/README.md`](/home/axetiz/Documents/UFAR/Project-S2/OrbitSimLite/README.md)

If you want the full technical picture, that file is the canonical reference for:

- architecture
- build commands
- demos
- controls
- export flow
- scene import/export
- test execution
- benchmark usage
