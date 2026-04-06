# Target
if(WIN32)
	add_executable(NullBackends WIN32 "Main.cpp")
else()
	add_executable(NullBackends "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(NullBackends)

# Working directory
set_target_properties(NullBackends PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(NullBackends Common)

# Plugin dependencies
B3DAddRuntimeDependencies(NullBackends)

# Install
B3DSetUpPostBuildAndInstallSteps(NullBackends)

# IDE specific
set_property(TARGET NullBackends PROPERTY FOLDER Examples/Tests)
