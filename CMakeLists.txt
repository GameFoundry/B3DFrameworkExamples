cmake_minimum_required (VERSION 3.12.0)
project (bsfExamples)

set (SECONDARY_APP_ROOT_DIR "${PROJECT_SOURCE_DIR}")

# Example projects to build.
set(EXAMPLE_SUBDIRECTORIES
	Source/Common
	Source/LowLevelRendering
	Source/Tests/AdvancedLowLevelRendering
	Source/PhysicallyBasedShading
	# Source/CustomMaterials # Outdated, until the shading language update is done
	Source/GUI
	Source/Tests/GUICulling
	Source/Audio
	Source/SkeletalAnimation
	Source/Physics
	Source/Particles
	Source/Decals
	Source/Tests/VectorGraphics
	Source/Tests/NullBackends
	Source/Lighting)

# Platform-specific overrides, if the target platform defines any (see Platform/<name>/Platform.cmake).
# A platform may trim the example set to the projects that build on it.
if(COMMAND B3DApplyPlatformExampleOverrides)
	B3DApplyPlatformExampleOverrides()
endif()

# Generate config file
configure_file("${PROJECT_SOURCE_DIR}/Source/CMake/B3DExampleConfig.h.in" "${PROJECT_BINARY_DIR}/Generated/B3DExampleConfig.h")

# Check data dependencies
B3DCheckAndUpdateAssetPackage(ExampleData)

# Ensures example shaders are cooked into the prebuilt shader store
set_property(GLOBAL APPEND PROPERTY B3D_SHADER_COOK_INPUT_FOLDERS "${PROJECT_SOURCE_DIR}/Data/Shaders")

# Sub-directories
foreach(exampleSubdirectory ${EXAMPLE_SUBDIRECTORIES})
	add_subdirectory(${exampleSubdirectory})
endforeach()
