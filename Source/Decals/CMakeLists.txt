# Target
if(WIN32)
	add_executable(Decals WIN32 "Main.cpp")
else()
	add_executable(Decals "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(Decals)

# Working directory
set_target_properties(Decals PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(Decals Common)

# Plugin dependencies
B3DAddRuntimeDependencies(Decals)
add_dependencies(Decals bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# Install
B3DSetUpPostBuildAndInstallSteps(Decals)

# IDE specific
set_property(TARGET Decals PROPERTY FOLDER Examples)
