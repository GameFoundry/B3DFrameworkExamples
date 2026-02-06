# Target
if(WIN32)
	add_executable(Lighting WIN32 "Main.cpp")
else()
	add_executable(Lighting "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(Lighting)

# Working directory
set_target_properties(Lighting PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(Lighting Common)

# Plugin dependencies
B3DAddRuntimeDependencies(Lighting)
add_dependencies(Lighting bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# Install
B3DSetUpPostBuildAndInstallSteps(Lighting)

# IDE specific
set_property(TARGET Lighting PROPERTY FOLDER Examples)
