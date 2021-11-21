if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.16.1/conan.cmake"
    "${CMAKE_BINARY_DIR}/conan.cmake"
    EXPECTED_HASH SHA256=396e16d0f5eabdc6a14afddbcfff62a54a7ee75c6da23f32f7a31bc85db23484
    TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_autodetect(settings)

conan_cmake_configure(
  REQUIRES
  glfw/[>=3.3.4]
  glm/[>=0.9.9]
  spdlog/[>=1.8.5]
  wayland/1.18.0
  vulkan-loader/[>=1.2.172]
  shaderc/[>=2021.1]
  imgui/[>=1.84.2]
  vulkan-memory-allocator/2.3.0
  tinyobjloader/1.0.6
  stb/cci.20210713

  OPTIONS
  spdlog:header_only=True
  fmt:header_only=True

  GENERATORS cmake_find_package
  IMPORTS "res/bindings, imgui_impl_glfw.* -> ${CMAKE_SOURCE_DIR}/src/bindings"
  IMPORTS "res/bindings, imgui_impl_vulkan.* -> ${CMAKE_SOURCE_DIR}/src/bindings"
  )

conan_cmake_install(PATH_OR_REFERENCE .
  BUILD missing
  REMOTE conancenter
  SETTINGS ${settings}
  )


include(FetchContent)

FetchContent_Declare(
  tracy
  GIT_REPOSITORY git@github.com:wolfpld/tracy.git
)

FetchContent_MakeAvailable(tracy)
