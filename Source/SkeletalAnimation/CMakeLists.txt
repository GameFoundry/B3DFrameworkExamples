# Target
if(WIN32)
	add_executable(SkeletalAnimation WIN32 "Main.cpp")
else()
	add_executable(SkeletalAnimation "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(SkeletalAnimation)

# Working directory
set_target_properties(SkeletalAnimation PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(SkeletalAnimation Common)

# Plugin dependencies
B3DAddRuntimeDependencies(SkeletalAnimation)
add_dependencies(SkeletalAnimation bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# Install
B3DSetUpPostBuildAndInstallSteps(SkeletalAnimation)

# IDE specific
set_property(TARGET SkeletalAnimation PROPERTY FOLDER Examples)
