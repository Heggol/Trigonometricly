set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(VULKAN_SDK "$ENV{HOME}/WinVulkanBuild/1.4.313.0-Windows")

set(CMAKE_FIND_ROOT_PATH  "${VULKAN_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/Include")
set(Vulkan_LIBRARY "${VULKAN_SDK}/Lib/vulkan-1.lib")

include_directories(${Vulkan_INCLUDE_DIR})

link_directories(${Vulkan_LIBRARY})

set(GLFW "$ENV{HOME}/WinVulkanBuild/glfw-3.4.bin.WIN64")

set(GLFW_INCLUDE_DIR "${GLFW}/include")
set(GLFW_LIBRARY "$ENV{HOME}/WinVulkanBuild/glfw-3.4.bin.WIN64/lib-mingw-w64/libglfw3.a")

message(STATUS "GLFW_LIBRARY is set to: ${GLFW_LIBRARY}")

include_directories(${GLFW_INCLUDE_DIR})

set(GLM "$ENV{HOME}/WinVulkanBuild/glm-1.0.1-light")

set(GLM_INCLUDE_DIR "${GLM}")

include_directories(${GLM_INCLUDE_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
