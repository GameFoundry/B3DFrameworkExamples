# Target
if(WIN32)
	add_executable(PhysicallyBasedShading WIN32 "Main.cpp")
else()
	add_executable(PhysicallyBasedShading "Main.cpp")
endif()
	
# Working directory
set_target_properties(PhysicallyBasedShading PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")		
	
# Libraries
## Local libs
target_link_libraries(PhysicallyBasedShading Common)

# Plugin dependencies
add_engine_dependencies(PhysicallyBasedShading)
add_dependencies(PhysicallyBasedShading bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# IDE specific
set_property(TARGET PhysicallyBasedShading PROPERTY FOLDER Examples)