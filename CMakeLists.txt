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

find_package(glfw3)
find_package(glm)
find_package(spdlog)
find_package(Boost COMPONENTS REQUIRED system)

target_link_libraries(app
  glm::glm
  spdlog::spdlog
  glfw::glfw
  Boost::Boost
  )

target_include_directories(app PUBLIC src/)
