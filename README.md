# B3D Framework Examples

A collection of small, focused projects demonstrating how to use the B3D Framework. Each example is self-contained and well commented, intended to serve both as a learning resource and as a starting point for your own projects.

# Building

The examples are bundled with the main [B3D Framework](https://github.com/GameFoundry/B3DFramework) repository as a git submodule and are built as part of the framework — there are no separate build steps. Follow the framework's [build instructions](https://github.com/GameFoundry/B3DFramework/blob/master/Documentation/GitHub/compiling.md), and the example binaries will be placed alongside the rest of the framework output.

Toolchain requirements match the parent project: Git, CMake 3.31 or higher, and a C++17 compiler. Example projects are enabled by default and can be toggled via the `B3D_BUILD_EXAMPLES` CMake option.

# Examples

* **Audio** — Importing audio clips and using audio sources and listeners.
* **Decals** — Projecting decal textures onto other surfaces.
* **GUI** — The built-in GUI system, including common controls, the layout system and styling.
* **Lighting** — Radial and spot lights, with and without shadow casting.
* **LowLevelRendering** — Issuing rendering commands directly through the framework's platform-agnostic low-level GPU API.
* **Particles** — Billboard particles, 3D mesh particles and GPU-simulated particles.
* **PhysicallyBasedShading** — The physically based renderer rendering an object in an HDR environment.
* **Physics** — Character controllers, rigidbodies, colliders and related components.
* **SkeletalAnimation** — Importing an animation clip and animating a 3D model with skeletal (skinned) animation.
