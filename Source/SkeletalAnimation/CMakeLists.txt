# Target
if(WIN32)
	add_executable(SkeletalAnimation WIN32 "Main.cpp")
else()
	add_executable(SkeletalAnimation "Main.cpp")
endif()
	
# Working directory
set_target_properties(SkeletalAnimation PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")		
	
# Libraries
## Local libs
target_link_libraries(SkeletalAnimation Common)

# Plugin dependencies
add_engine_dependencies(SkeletalAnimation)
add_dependencies(SkeletalAnimation bsfFBXImporter bsfFontImporter bsfFreeImgImporter)

# IDE specific
set_property(TARGET SkeletalAnimation PROPERTY FOLDER Examples)