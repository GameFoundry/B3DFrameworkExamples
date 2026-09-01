# Target
if(WIN32)
	add_executable(CustomMaterials WIN32 "Main.cpp")
else()
	add_executable(CustomMaterials "Main.cpp")
endif()
	
# Working directory
set_target_properties(CustomMaterials PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")		
	
# Libraries
## Local libs
target_link_libraries(CustomMaterials Common)

# Plugin dependencies
B3DAddRuntimeDependencies(CustomMaterials)
if(B3D_BUILD_IMPORTERS)
	add_dependencies(CustomMaterials bsfFBXImporter bsfFontImporter bsfFreeImgImporter)
endif()

# Install
B3DSetUpPostBuildAndInstallSteps(CustomMaterials)

# IDE specific
set_property(TARGET CustomMaterials PROPERTY FOLDER Examples)