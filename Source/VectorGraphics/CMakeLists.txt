# Target
if(WIN32)
	add_executable(VectorGraphics WIN32 "Main.cpp")
else()
	add_executable(VectorGraphics "Main.cpp")
endif()
	
# Working directory
set_target_properties(VectorGraphics PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")	
	
# Libraries
## Local libs
target_link_libraries(VectorGraphics Common)

# Plugin dependencies
add_engine_dependencies(VectorGraphics)
add_dependencies(VectorGraphics bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# IDE specific
set_property(TARGET VectorGraphics PROPERTY FOLDER Examples)