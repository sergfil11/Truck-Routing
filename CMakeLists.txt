cmake_minimum_required(VERSION 3.21)
project(TruckRouting LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
if(MSVC)
    add_compile_options(/utf-8 /EHsc)
endif()
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(TRUCK_ROUTING_SOLVER "cpsat" CACHE STRING "Build variant: gurobi or cpsat")
set_property(CACHE TRUCK_ROUTING_SOLVER PROPERTY STRINGS gurobi cpsat)
if(NOT TRUCK_ROUTING_SOLVER MATCHES "^(gurobi|cpsat)$")
    message(FATAL_ERROR "TRUCK_ROUTING_SOLVER must be gurobi or cpsat")
endif()

# Sources shared by both backends. No Gurobi or OR-Tools dependency here.
add_library(truck_routing_core STATIC
    src/common/parsing.cpp
    src/common/prep_functions.cpp
    src/common/preprocessing.cpp
)
target_include_directories(truck_routing_core PUBLIC include/common)
target_include_directories(truck_routing_core SYSTEM PRIVATE third_party/nlohmann)

option(TRUCK_ROUTING_OPENMP "Use OpenMP if available" ON)
if(TRUCK_ROUTING_OPENMP)
    find_package(OpenMP QUIET COMPONENTS CXX)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(truck_routing_core PUBLIC OpenMP::OpenMP_CXX)
    else()
        message(STATUS "OpenMP not found: route generation will run sequentially")
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(preprocessing_tests tests/preprocessing_tests.cpp)
    target_link_libraries(preprocessing_tests PRIVATE truck_routing_core)
    add_test(NAME preprocessing_product_mapping COMMAND preprocessing_tests)
endif()

include(cmake/${TRUCK_ROUTING_SOLVER}.cmake)
