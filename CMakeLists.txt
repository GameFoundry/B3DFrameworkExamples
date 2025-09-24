cmake_minimum_required (VERSION 3.12.0)
project (bsfExamples)

set (SECONDARY_APP_ROOT_DIR "${PROJECT_SOURCE_DIR}")
set(B3D_EXAMPLES_BUILTIN_ASSETS_VERSION 9)

# Generate config file
configure_file("${PROJECT_SOURCE_DIR}/Source/CMake/B3DExampleConfig.h.in" "${PROJECT_SOURCE_DIR}/Source/Common/B3DExampleConfig.h")

# Check data dependencies
B3DUpdateBuiltinAssetsIfNeeded(bsfExamples ${PROJECT_SOURCE_DIR}/Data Data ${B3D_EXAMPLES_BUILTIN_ASSETS_VERSION})
	
# Sub-directories
add_subdirectory(Source/Common)
add_subdirectory(Source/LowLevelRendering)
add_subdirectory(Source/PhysicallyBasedShading)
add_subdirectory(Source/CustomMaterials)
add_subdirectory(Source/GUI)
add_subdirectory(Source/GUICulling)
add_subdirectory(Source/Audio)
add_subdirectory(Source/SkeletalAnimation)
add_subdirectory(Source/Physics)
add_subdirectory(Source/Particles)
add_subdirectory(Source/Decals)
add_subdirectory(Source/VectorGraphics)