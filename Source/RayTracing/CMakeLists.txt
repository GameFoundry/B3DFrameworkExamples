# Target
if(WIN32)
	add_executable(RayTracing WIN32 "Main.cpp")
else()
	add_executable(RayTracing "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(RayTracing)

# Working directory
set_target_properties(RayTracing PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(RayTracing Common)

# Plugin dependencies
B3DAddRuntimeDependencies(RayTracing)
if(B3D_BUILD_IMPORTERS)
	add_dependencies(RayTracing bsfFBXImporter bsfFontImporter bsfFreeImgImporter)
endif()

# Install
B3DSetUpPostBuildAndInstallSteps(RayTracing)

# IDE specific
set_property(TARGET RayTracing PROPERTY FOLDER Examples)
