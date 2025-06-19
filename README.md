| CI            | Community   | Support |
| ------------- |-------------|--------|
[![Build Status](https://travis-ci.org/GameFoundry/bsfExamples.svg?branch=master)](https://travis-ci.org/GameFoundry/bsfExamples) [![Build status](https://ci.appveyor.com/api/projects/status/wvb1erthd4pvehqr?svg=true)](https://ci.appveyor.com/project/BearishSun/bsfexamples) | [![Community](https://img.shields.io/discourse/https/discourse.bsframework.io/posts.svg)](https://discourse.bsframework.io) | [![Patreon](https://img.shields.io/badge/Donate-Patreon-orange.svg)](https://www.patreon.com/bePatron?c=1646501) [![Paypal](https://img.shields.io/badge/Donate-Paypal-blue.svg)](https://www.paypal.me/MarkoPintera/10)

# Compile steps

- Install git (https://git-scm.com) and CMake 3.12.4 or higher (https://cmake.org)
	- Ensure they are added to your *PATH* environment variable
- Run the following commands in the terminal/command line:
	- `git clone https://github.com/GameFoundry/bsfExamples.git`
	- `cd bsfExamples`
	- `mkdir Build`
	- `cd Build`
	- `cmake -G "$generator$" ../`
		- Where *$generator$* should be replaced with any of the supported generators. Some common ones:
			- `Visual Studio 15 2017 Win64` - Visual Studio 2017 (64-bit build)
			- `Unix Makefiles`
			- `Ninja`
			- `Xcode`
			- See all valid generators: [cmake-generators](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html)
- Build the project using your chosen tool
	- Build files are in the `bsfExamples/Build` folder
- Run the examples
	- Example binaries are placed in the `bsfExamples/Build/bin` folder

# Examples
* Audio - Demonstrates how to import audio clips and use audio sources and listeners.
* CustomMaterials - Demonstrates how to use custom materials that override vertex, surface and lighting aspects of the renderer.
* Decals - Demonstrates how to project decal textures onto other surfaces.
* GUI - Demonstrates how to use the built-in GUI system. Demoes a variety of basic controls, the layout system and shows how to use styles to customize the look of GUI elements.
* LowLevelRendering - Demonstrates how to use the low-level rendering system to manually issue rendering commands. This is similar to using DirectX/OpenGL/Vulkan, except it uses B3D Framework's platform-agnostic rendering layer.
* Particles - Demonstrates how to use the particle system to render traditional billboard particles, 3D mesh particles and GPU simulated particles.
* PhysicallyBasedRendering - Demonstrates the physically based renderer using the built-in shaders & lighting by rendering an object in a HDR environment.
* Physics - Demonstrates the use of variety of physics related components, including a character controller, rigidbodies and colliders.
* SkeletalAnimation - Demonstrates how to import an animation clip and animate a 3D model using skeletal (skinned) animation.