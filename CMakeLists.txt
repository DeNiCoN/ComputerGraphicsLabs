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
  shaderc::shaderc
  )

target_include_directories(app PUBLIC src/)

target_compile_definitions(app PUBLIC
  GLFW_INCLUDE_VULKAN
  )

add_custom_command(TARGET app POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink
                   ${CMAKE_SOURCE_DIR}/shaders/ $<TARGET_FILE_DIR:app>/shaders
                   COMMENT "Symlink shaders directory"
                   DEPENDS  ${dest}
                   )
