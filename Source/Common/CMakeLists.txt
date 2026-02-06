# Source files and their filters
include(CMakeSources.cmake)

# Target
add_library(Common STATIC ${B3D_COMMON_SOURCE_FILES})

B3DSetDefaultLinkAndCompileFlags(Common)

# Includes
target_include_directories(Common PUBLIC "./")

# Libraries
## Local libs
target_link_libraries(Common bsf)

# IDE specific
set_property(TARGET Common PROPERTY FOLDER Examples)

# Copy bsf binaries
#copyBsfBinaries(Common ${B3D_FRAMEWORK_ROOT_FOLDER})
