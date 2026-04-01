cmake_minimum_required(VERSION 3.15)
project(OrbitSimLite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

option(ORBITSIMLITE_BUILD_DEMO "Build the demo applications" ON)
option(ORBITSIMLITE_BUILD_TESTS "Build simple numerical tests" ON)

# Library sources
set(ORBITSIMLITE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(ORBITSIMLITE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(orbitsimlite STATIC
    ${ORBITSIMLITE_SRC_DIR}/vec2.cpp
    ${ORBITSIMLITE_SRC_DIR}/body.cpp
    ${ORBITSIMLITE_SRC_DIR}/physics.cpp
    ${ORBITSIMLITE_SRC_DIR}/simulator.cpp
    ${ORBITSIMLITE_SRC_DIR}/renderer.cpp
)

target_include_directories(orbitsimlite
    PUBLIC
        $<BUILD_INTERFACE:${ORBITSIMLITE_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Find SFML for the renderer and demo
find_package(SFML 2.5 REQUIRED COMPONENTS system window graphics)

target_link_libraries(orbitsimlite PUBLIC sfml-system sfml-window sfml-graphics)

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
endif()

if (ORBITSIMLITE_BUILD_TESTS)
    add_executable(orbitsimlite_tests tests/physics_tests.cpp)
    target_link_libraries(orbitsimlite_tests PRIVATE orbitsimlite)
    target_include_directories(orbitsimlite_tests PRIVATE ${ORBITSIMLITE_INCLUDE_DIR})
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
