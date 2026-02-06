# Target
if(WIN32)
	add_executable(Physics WIN32 "Main.cpp")
else()
	add_executable(Physics "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(Physics)

# Working directory
set_target_properties(Physics PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(Physics Common)

# Plugin dependencies
B3DAddRuntimeDependencies(Physics)
add_dependencies(Physics bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# Install
B3DSetUpPostBuildAndInstallSteps(Physics)

# IDE specific
set_property(TARGET Physics PROPERTY FOLDER Examples)
