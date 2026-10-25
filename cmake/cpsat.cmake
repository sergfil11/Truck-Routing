# ORTOOLS_ROOT is the extracted C++ distribution, not the Python package.
set(ORTOOLS_ROOT "$ENV{ORTOOLS_ROOT}" CACHE PATH "OR-Tools C++ SDK root")
if(ORTOOLS_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${ORTOOLS_ROOT}")
elseif(EXISTS "${PROJECT_SOURCE_DIR}/build/deps/ortools/lib/cmake/ortools")
    list(PREPEND CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/build/deps/ortools")
endif()
find_package(ortools 9.15 CONFIG REQUIRED)

add_library(truck_routing_cpsat_backend STATIC src/cpsat/cp_sat.cpp src/cpsat/results.cpp)
target_include_directories(truck_routing_cpsat_backend PUBLIC include/cpsat)
target_link_libraries(truck_routing_cpsat_backend PUBLIC truck_routing_core ortools::ortools)
add_executable(truck_routing_cpsat apps/cpsat/main.cpp)
target_link_libraries(truck_routing_cpsat PRIVATE truck_routing_cpsat_backend)
set_target_properties(truck_routing_cpsat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

if(BUILD_TESTING)
    add_executable(cpsat_model_tests tests/cpsat_model_tests.cpp)
    target_link_libraries(cpsat_model_tests PRIVATE truck_routing_cpsat_backend)
    add_test(NAME cpsat_model_equivalence COMMAND cpsat_model_tests "${PROJECT_SOURCE_DIR}/tests/fixtures/tiny.json")
    add_test(NAME cpsat_cli_help COMMAND truck_routing_cpsat --help)
    add_test(NAME cpsat_cli_tiny COMMAND truck_routing_cpsat
        "${PROJECT_SOURCE_DIR}/tests/fixtures/tiny.json" "${CMAKE_BINARY_DIR}/test-output/tiny.txt")
endif()
