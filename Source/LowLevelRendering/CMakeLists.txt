# Target
if(WIN32)
	add_executable(LowLevelRendering WIN32 "Main.cpp")
else()
	add_executable(LowLevelRendering "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(LowLevelRendering)

# Working directory
set_target_properties(LowLevelRendering PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(LowLevelRendering Common)

# Plugin dependencies
B3DAddRuntimeDependencies(LowLevelRendering)
add_dependencies(LowLevelRendering bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# Install
B3DSetUpPostBuildAndInstallSteps(LowLevelRendering)

# IDE specific
set_property(TARGET LowLevelRendering PROPERTY FOLDER Examples)
