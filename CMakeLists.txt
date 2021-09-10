cmake_minimum_required(VERSION 3.16)

project(ComputerGraphics)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(libs)
# Required for conan
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/libs)


file(GLOB app_SRC
  "src/*.hpp"
  "src/*.cpp"
  )

add_executable(app
  ${app_SRC}
  )

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Boost COMPONENTS REQUIRED system)
find_package(shaderc REQUIRED)

target_link_libraries(app
  Vulkan::Vulkan
  glm::glm
  spdlog::spdlog
  glfw::glfw
  Boost::Boost
  )

target_include_directories(app PUBLIC src/)

target_compile_definitions(app PUBLIC
  GLFW_INCLUDE_VULKAN
  )

function(add_shader TARGET SHADER)
    find_program(GLSLC glslc)

    set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER})
    set(current-output-path ${CMAKE_BINARY_DIR}/shaders/${SHADER}.spv)

    # Add a custom command to compile GLSL to SPIR-V.
    get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
    file(MAKE_DIRECTORY ${current-output-dir})

    add_custom_command(
           OUTPUT ${current-output-path}
           COMMAND ${GLSLC} -o ${current-output-path} ${current-shader-path}
           DEPENDS ${current-shader-path}
           IMPLICIT_DEPENDS CXX ${current-shader-path}
           VERBATIM)

    # Make sure our build depends on this output.
    set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
    target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

add_shader(app vert.vert)
add_shader(app frag.frag)
