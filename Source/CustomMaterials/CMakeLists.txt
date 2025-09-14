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
add_engine_dependencies(CustomMaterials)
add_dependencies(CustomMaterials bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# IDE specific
set_property(TARGET CustomMaterials PROPERTY FOLDER Examples)