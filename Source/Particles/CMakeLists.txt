# Target
if(WIN32)
	add_executable(Particles WIN32 "Main.cpp")
else()
	add_executable(Particles "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(Particles)

# Working directory
set_target_properties(Particles PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(Particles Common)

# Plugin dependencies
B3DAddRuntimeDependencies(Particles)
add_dependencies(Particles bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# Install
B3DSetUpPostBuildAndInstallSteps(Particles)

# IDE specific
set_property(TARGET Particles PROPERTY FOLDER Examples)
