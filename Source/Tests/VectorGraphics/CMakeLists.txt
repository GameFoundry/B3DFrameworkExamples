# Target
if(WIN32)
	add_executable(VectorGraphics WIN32 "Main.cpp")
else()
	add_executable(VectorGraphics "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(VectorGraphics)

# Working directory
set_target_properties(VectorGraphics PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(VectorGraphics Common)

# Plugin dependencies
B3DAddRuntimeDependencies(VectorGraphics)
add_dependencies(VectorGraphics bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# Install
B3DSetUpPostBuildAndInstallSteps(VectorGraphics)

# IDE specific
set_property(TARGET VectorGraphics PROPERTY FOLDER Examples/Tests)
