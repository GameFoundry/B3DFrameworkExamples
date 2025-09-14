# Target
if(WIN32)
	add_executable(GUI WIN32 "Main.cpp")
else()
	add_executable(GUI "Main.cpp")
endif()
	
# Working directory
set_target_properties(GUI PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")		
	
# Libraries
## Local libs
target_link_libraries(GUI Common)

# Plugin dependencies
add_engine_dependencies(GUI)
add_dependencies(GUI bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# IDE specific
set_property(TARGET GUI PROPERTY FOLDER Examples)