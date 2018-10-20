
file (GLOB PAK_SOURCES 
	"*.h" 
	"*.cpp" )

add_executable(stl2png main.cpp ${PAK_SOURCES})

source_group(source FILES ${PAK_SOURCES} )