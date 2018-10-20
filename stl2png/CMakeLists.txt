
file (GLOB STL2PNG_SOURCES 
	"*.h" 
	"*.cpp" )

add_executable(stl2png main.cpp ${STL2PNG_SOURCES})

source_group(source FILES ${STL2PNG_SOURCES} )