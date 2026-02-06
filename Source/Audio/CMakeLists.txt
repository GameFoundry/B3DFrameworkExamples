# Target
if(WIN32)
	add_executable(Audio WIN32 "Main.cpp")
else()
	add_executable(Audio "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(Audio)

# Working directory
set_target_properties(Audio PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(Audio Common)

# Plugin dependencies
B3DAddRuntimeDependencies(Audio)
add_dependencies(Audio bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# Install
B3DSetUpPostBuildAndInstallSteps(Audio)

# IDE specific
set_property(TARGET Audio PROPERTY FOLDER Examples)
