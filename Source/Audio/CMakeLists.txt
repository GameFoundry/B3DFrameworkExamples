# Target
if(WIN32)
	add_executable(Audio WIN32 "Main.cpp")
else()
	add_executable(Audio "Main.cpp")
endif()
	
# Working directory
set_target_properties(Audio PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")		
	
# Libraries
## Local libs
target_link_libraries(Audio Common)

# Plugin dependencies
add_engine_dependencies(Audio)
add_dependencies(Audio bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# IDE specific
set_property(TARGET Audio PROPERTY FOLDER Examples)