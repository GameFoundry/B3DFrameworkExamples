# Target
if(WIN32)
	add_executable(GUICulling WIN32 "Main.cpp")
else()
	add_executable(GUICulling "Main.cpp")
endif()
	
# Working directory
set_target_properties(GUICulling PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")
	
# Libraries
## Local libs
target_link_libraries(GUICulling Common)

# Plugin dependencies
B3DAddRuntimeDependencies(GUICulling)
add_dependencies(GUICulling bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# IDE specific
set_property(TARGET GUICulling PROPERTY FOLDER Examples)