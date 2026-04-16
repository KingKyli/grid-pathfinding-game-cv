cmake_minimum_required(VERSION 3.10)
project(project CXX)

set(CMAKE_CXX_STANDARD 17)
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INC_DIR "${CMAKE_SOURCE_DIR}/include")

include_directories(${INC_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/grid)

file(GLOB SOURCES "${SRC_DIR}/*.cpp")
add_executable(project ${SOURCES})

# GridWorld scaffold
file(GLOB_RECURSE GRID_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/grid/*.cpp"
)
add_executable(run_sim ${CMAKE_SOURCE_DIR}/examples/run_sim.cpp ${GRID_SOURCES})

# Optional SGG integration (required by assignment UI part)
option(WITH_SGG "Build SGG (graphics) targets" OFF)
if(WITH_SGG)
  # In the submission zip we must NOT ship the whole SGG library sources.
  # Support both workflows:
  #  - Development: build SGG from vendored sources in sgg-main/
  #  - Submission/Grading: headers only + external/prebuilt SGG library

  set(SGG_INCLUDE_ROOT "" CACHE PATH "Path that contains sgg/graphics.h (e.g. <root> that has an sgg/ folder)")
  set(SGG_LIBRARY "" CACHE FILEPATH "Path to the prebuilt SGG library (.lib/.a) when sgg-main is not present")

  set(_SGG_TARGET "")
  if (EXISTS "${CMAKE_SOURCE_DIR}/sgg-main/CMakeLists.txt")
    add_subdirectory(sgg-main)
    set(_SGG_TARGET sgg::sgg)
  else()
    find_package(sgg CONFIG QUIET)
    if (TARGET sgg::sgg)
      set(_SGG_TARGET sgg::sgg)
    else()
      if (NOT SGG_LIBRARY)
        message(FATAL_ERROR
          "WITH_SGG=ON but no vendored SGG sources (sgg-main/) and no installed sgg package was found. "
          "Provide -DSGG_LIBRARY=<path-to-sgg-lib> and optionally -DSGG_INCLUDE_ROOT=<path-containing-sgg/graphics.h>."
        )
      endif()
      add_library(sgg_external UNKNOWN IMPORTED)
      set_target_properties(sgg_external PROPERTIES IMPORTED_LOCATION "${SGG_LIBRARY}")
      set(_SGG_TARGET sgg_external)
    endif()
  endif()

  if (NOT SGG_INCLUDE_ROOT)
    if (EXISTS "${CMAKE_SOURCE_DIR}/sgg/graphics.h")
      set(SGG_INCLUDE_ROOT "${CMAKE_SOURCE_DIR}")
    elseif (EXISTS "${CMAKE_SOURCE_DIR}/sgg-main/sgg/graphics.h")
      set(SGG_INCLUDE_ROOT "${CMAKE_SOURCE_DIR}/sgg-main")
    endif()
  endif()

  if (NOT SGG_INCLUDE_ROOT)
    message(FATAL_ERROR "WITH_SGG=ON but could not locate headers. Expected either sgg/graphics.h or sgg-main/sgg/graphics.h or -DSGG_INCLUDE_ROOT=...")
  endif()

  # On Windows, SGG (via SDL2/SDL2_mixer/etc.) needs runtime DLLs next to the exe.
  # When using the vcpkg toolchain, those DLLs live under:
  #   ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/(debug/)bin
  function(copy_vcpkg_runtime_dlls target_name)
    if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
      set(_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
      set(_vcpkg_debug_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
      add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${_vcpkg_bin}" "$<TARGET_FILE_DIR:${target_name}>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${_vcpkg_debug_bin}" "$<TARGET_FILE_DIR:${target_name}>"
        VERBATIM
      )
    endif()
  endfunction()

  # SGG uses glm/gtx/* headers which are marked experimental in recent GLM.
  # Enable them for the vendored SGG target.
  if(TARGET sgg)
    target_compile_definitions(sgg PUBLIC GLM_ENABLE_EXPERIMENTAL)
  endif()
  add_executable(run_sgg ${CMAKE_SOURCE_DIR}/examples/run_sgg.cpp)
  target_link_libraries(run_sgg PRIVATE ${_SGG_TARGET})
  target_include_directories(run_sgg PRIVATE "${SGG_INCLUDE_ROOT}")
  copy_vcpkg_runtime_dlls(run_sgg)

  add_executable(run_sim_sgg ${CMAKE_SOURCE_DIR}/examples/run_sim_sgg.cpp ${GRID_SOURCES})
  target_link_libraries(run_sim_sgg PRIVATE ${_SGG_TARGET})
  target_include_directories(run_sim_sgg PRIVATE "${SGG_INCLUDE_ROOT}")
  copy_vcpkg_runtime_dlls(run_sim_sgg)
endif()

# Tests (simple): build tests if requested
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_executable(test_example
    ${CMAKE_SOURCE_DIR}/tests/test_example.cpp
    ${CMAKE_SOURCE_DIR}/src/example.cpp
  )
  add_test(NAME example_test COMMAND test_example)

  add_executable(pathfinding_test
    ${CMAKE_SOURCE_DIR}/tests/pathfinding_test.cpp
    ${CMAKE_SOURCE_DIR}/grid/core/Map.cpp
    ${CMAKE_SOURCE_DIR}/grid/pathfinding/AStar.cpp
  )
  add_test(NAME pathfinding_test COMMAND pathfinding_test)
endif()