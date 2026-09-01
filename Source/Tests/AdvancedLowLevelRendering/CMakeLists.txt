# Traget
if(WIN32)
	add_executable(AdvancedLowLevelRendering WIN32 "Main.cpp")
else()
	add_executable(AdvancedLowLevelRendering "Main.cpp")
endif()

B3DSetDefaultLinkAndCompileFlags(AdvancedLowLevelRendering)
set_target_properties(AdvancedLowLevelRendering PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# Libraries
target_link_libraries(AdvancedLowLevelRendering Common)

# Runtime dependencies
B3DAddRuntimeDependencies(AdvancedLowLevelRendering)

# Plugin depenencies
if(B3D_BUILD_IMPORTERS)
	add_dependencies(AdvancedLowLevelRendering bsfFBXImporter bsfFontImporter bsfFreeImgImporter)
endif()

# Do post-build steps and install
B3DSetUpPostBuildAndInstallSteps(AdvancedLowLevelRendering)

# IDE specific
set_property(TARGET AdvancedLowLevelRendering PROPERTY FOLDER Examples/Tests)
