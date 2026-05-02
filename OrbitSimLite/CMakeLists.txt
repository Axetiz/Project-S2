cmake_minimum_required(VERSION 3.15)
project(OrbitSimLite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
enable_testing()

option(ORBITSIMLITE_BUILD_DEMO "Build the demo applications" ON)
option(ORBITSIMLITE_BUILD_TESTS "Build simple numerical tests" ON)
option(ORBITSIMLITE_BUILD_BENCHMARKS "Build benchmark utilities" ON)

# Library sources
set(ORBITSIMLITE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(ORBITSIMLITE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(orbitsimlite STATIC
    ${ORBITSIMLITE_SRC_DIR}/vec2.cpp
    ${ORBITSIMLITE_SRC_DIR}/body.cpp
    ${ORBITSIMLITE_SRC_DIR}/physics.cpp
    ${ORBITSIMLITE_SRC_DIR}/simulator.cpp
    ${ORBITSIMLITE_SRC_DIR}/renderer_core.cpp
    ${ORBITSIMLITE_SRC_DIR}/renderer_interaction.cpp
    ${ORBITSIMLITE_SRC_DIR}/renderer_collision.cpp
    ${ORBITSIMLITE_SRC_DIR}/renderer_draw.cpp
    ${ORBITSIMLITE_SRC_DIR}/renderer_scene.cpp
    ${ORBITSIMLITE_SRC_DIR}/renderer_export.cpp
)

target_include_directories(orbitsimlite
    PUBLIC
        $<BUILD_INTERFACE:${ORBITSIMLITE_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Find SFML for the renderer and demo
# Prefer SFML 3 (Arch default), fallback to SFML 2.5.
find_package(SFML 3 QUIET COMPONENTS Graphics Window System)
if(SFML_FOUND)
    set(ORBITSIMLITE_SFML_LIBS SFML::Graphics SFML::Window SFML::System)
else()
    find_package(SFML 2.5 REQUIRED COMPONENTS system window graphics)
    if(TARGET SFML::Graphics AND TARGET SFML::Window AND TARGET SFML::System)
        set(ORBITSIMLITE_SFML_LIBS SFML::Graphics SFML::Window SFML::System)
    else()
        set(ORBITSIMLITE_SFML_LIBS sfml-graphics sfml-window sfml-system)
    endif()
endif()

target_link_libraries(orbitsimlite PUBLIC ${ORBITSIMLITE_SFML_LIBS})

if (MSVC)
    target_compile_options(orbitsimlite PRIVATE /W4 /permissive-)
else()
    target_compile_options(orbitsimlite PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (ORBITSIMLITE_BUILD_DEMO)
    # Solar system style demo
    add_executable(demo_solar_system examples/demo_solar_system.cpp)
    target_link_libraries(demo_solar_system PRIVATE orbitsimlite)
    target_include_directories(demo_solar_system PRIVATE ${ORBITSIMLITE_INCLUDE_DIR})

    # Two equal-mass suns in mutual orbit
    add_executable(demo_binary_stars examples/demo_binary_stars.cpp)
    target_link_libraries(demo_binary_stars PRIVATE orbitsimlite)
    target_include_directories(demo_binary_stars PRIVATE ${ORBITSIMLITE_INCLUDE_DIR})

    # Three-body figure-eight choreography
    add_executable(demo_threebody_figure8 examples/demo_threebody_figure8.cpp)
    target_link_libraries(demo_threebody_figure8 PRIVATE orbitsimlite)
    target_include_directories(demo_threebody_figure8 PRIVATE ${ORBITSIMLITE_INCLUDE_DIR})

    # Empty plane for manual testing and building scenes from scratch
    add_executable(demo_simulator examples/demo_simulator.cpp)
    target_link_libraries(demo_simulator PRIVATE orbitsimlite)
    target_include_directories(demo_simulator PRIVATE ${ORBITSIMLITE_INCLUDE_DIR})
endif()

if (ORBITSIMLITE_BUILD_TESTS)
    add_executable(orbitsimlite_tests tests/physics_tests.cpp)
    target_link_libraries(orbitsimlite_tests PRIVATE orbitsimlite)
    target_include_directories(orbitsimlite_tests PRIVATE ${ORBITSIMLITE_INCLUDE_DIR})

    # Register the existing test executable with CTest so the project can use
    # standard CMake-driven test workflows without rewriting the test code.
    add_test(NAME orbitsimlite_physics_tests COMMAND orbitsimlite_tests)
endif()

if (ORBITSIMLITE_BUILD_BENCHMARKS)
    add_executable(benchmark_integrators examples/benchmark_integrators.cpp)
    target_link_libraries(benchmark_integrators PRIVATE orbitsimlite)
    target_include_directories(benchmark_integrators PRIVATE ${ORBITSIMLITE_INCLUDE_DIR})
endif()

# Install rules for library-style usage
install(TARGETS orbitsimlite
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${ORBITSIMLITE_INCLUDE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
