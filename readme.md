# STL 2 PNG


## Dependencies

* STB, https://github.com/nothings/stb
* GLM, https://github.com/g-truc/glm
* GLFW, https://github.com/glfw/glfw
* CMake, https://cmake.org/

## Building

Clone STB and GLM. Clone and build GLFW, or download the binaries and place all repositories and installed files in folders next to stl2png repository folder. In the stl2png/CMakeLists.txt it referes to "../glm", "../stb" and "../glfw/" (lib-vc2015 hardcoded atm).

Update stl2png/CMakeLists.txt to reflect the path to where you have the GLFW (static) library installed and the includes. Run cmake in the root and build.

