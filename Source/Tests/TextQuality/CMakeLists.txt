# Target
if(WIN32)
	add_executable(TextQuality WIN32 "Main.cpp")
else()
	add_executable(TextQuality "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(TextQuality)
target_compile_definitions(TextQuality PRIVATE TEXT_QUALITY_CSS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/TextQuality.css")

# Working directory
set_target_properties(TextQuality PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
## Local libs
target_link_libraries(TextQuality Common)

# Plugin dependencies
B3DAddRuntimeDependencies(TextQuality)
if(B3D_BUILD_IMPORTERS)
	add_dependencies(TextQuality bsfFBXImporter bsfFontImporter bsfFreeImgImporter)
endif()

# Install
B3DSetUpPostBuildAndInstallSteps(TextQuality)

# IDE specific
set_property(TARGET TextQuality PROPERTY FOLDER Examples/Tests)
