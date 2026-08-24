# Target
if(WIN32)
	add_executable(GUI WIN32 "Main.cpp")
else()
	add_executable(GUI "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(GUI)

# Working directory
set_target_properties(GUI PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(GUI Common)

# Plugin dependencies
B3DAddRuntimeDependencies(GUI)
if(B3D_BUILD_IMPORTERS)
	add_dependencies(GUI bsfFBXImporter bsfFontImporter bsfFreeImgImporter)
endif()

# Install
B3DSetUpPostBuildAndInstallSteps(GUI)

# IDE specific
set_property(TARGET GUI PROPERTY FOLDER Examples)
