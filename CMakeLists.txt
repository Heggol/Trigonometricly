cmake_minimum_required(VERSION 3.10.0)
project(Trigonometricly VERSION 0.1.0 LANGUAGES C CXX)


if(CMAKE_PRESET_NAME STREQUAL "Windows")
    set(CROSS_COMPILE_WINDOWS ON)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/windows-toolchain.cmake")
    message(STATUS "CROSS_COMPILE_WINDOWS is set to ON based on preset: ${CMAKE_PRESET_NAME}")
else()
    set(CROSS_COMPILE_WINDOWS OFF)
    message(STATUS "CROSS_COMPILE_WINDOWS is set to OFF. Preset: ${CMAKE_PRESET_NAME}")
endif()

if(CROSS_COMPILE_WINDOWS)
    include_directories(${GLM_INCLUDE_DIR})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
endif()

find_package(Vulkan REQUIRED)

find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc compiler not found! Make sure the Vulkan SDK is installed.")
endif()

set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/bin/shaders)
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

set(SHADERS
    vert.spv
    frag.spv
)

foreach(name IN ITEMS vert frag)
    add_custom_command(
        OUTPUT ${SHADER_BIN_DIR}/${name}.spv
        COMMAND ${GLSLC}
        ${SHADER_SRC_DIR}/line.${name} -o
        ${SHADER_BIN_DIR}/${name}.spv
        DEPENDS ${SHADER_SRC_DIR}/line.${name}
        COMMENT "Compiling line.${name} → ${name}.spv"
    )
endforeach()

add_custom_target(Shaders ALL
    DEPENDS
    ${SHADER_BIN_DIR}/vert.spv
    ${SHADER_BIN_DIR}/frag.spv
)

add_executable(Trigonometricly
    main.cpp
    src/sine.cpp
    src/InitVulkan.cpp
)

if(CROSS_COMPILE_WINDOWS)
    set(GLFW_LIBRARY "$ENV{HOME}/WinVulkanBuild/glfw-3.4.bin.WIN64/lib-mingw-w64/libglfw3.a")
    target_link_libraries(Trigonometricly
        PRIVATE ${GLFW_LIBRARY} Vulkan::Vulkan
    )
    target_link_options(Trigonometricly PRIVATE "${GLFW_LIBRARY}")
    target_link_options(Trigonometricly PRIVATE "-Wl,--as-needed")
else()
    target_link_libraries(Trigonometricly
        PRIVATE glfw Vulkan::Vulkan
    )
endif()

message(STATUS "Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")

add_dependencies(Trigonometricly Shaders)

if(CROSS_COMPILE_WINDOWS)
    target_include_directories(Trigonometricly PRIVATE ${GLM_INCLUDE_DIR})
endif()

add_custom_command(TARGET Trigonometricly POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove
    $<TARGET_FILE_DIR:Trigonometricly>/vert.spv
    $<TARGET_FILE_DIR:Trigonometricly>/frag.spv
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET Trigonometricly POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "$ENV{HOME}/WinVulkanBuild/1.4.313.0-Windows/Bin/vulkan-1.dll"
        $<TARGET_FILE_DIR:Trigonometricly>
    )
endif()

set_target_properties(Trigonometricly PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)