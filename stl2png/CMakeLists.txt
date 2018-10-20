
file (GLOB STL2PNG_SOURCES 
	"*.h" 
	"*.cpp" )

link_directories("../../glfw/lib-vc2015")

include_directories("../../stb")
include_directories("../../glm")
include_directories( "../../glfw/include")

add_executable(stl2png main.cpp ${STL2PNG_SOURCES})

target_link_libraries(stl2png glad)

find_package(OpenGL REQUIRED)
include_directories(${OpenGL_INCLUDE_DIRS})
link_directories(${OpenGL_LIBRARY_DIRS})
add_definitions(${OpenGL_DEFINITIONS})
if(NOT OPENGL_FOUND)
    message(ERROR " OPENGL not found!")
endif(NOT OPENGL_FOUND)
target_link_libraries(stl2png opengl32)

#find_package(glfw3 REQUIRED)

target_link_libraries(stl2png glfw3)

source_group(source FILES ${STL2PNG_SOURCES} )