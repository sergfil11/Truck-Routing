list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
find_package(GUROBI REQUIRED)

add_library(truck_routing_gurobi_backend STATIC
    src/gurobi/gurobi.cpp
    src/gurobi/col_gen_algo.cpp
)
target_include_directories(truck_routing_gurobi_backend PUBLIC include/gurobi)
target_link_libraries(truck_routing_gurobi_backend PUBLIC truck_routing_core GUROBI::GUROBI)

add_executable(truck_routing_gurobi apps/gurobi/main.cpp)
add_executable(truck_routing_gurobi_colgen apps/gurobi/col_gen_main.cpp)
foreach(target truck_routing_gurobi truck_routing_gurobi_colgen)
    target_link_libraries(${target} PRIVATE truck_routing_gurobi_backend)
    # Keep the executable path identical for single- and multi-config generators.
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
endforeach()
