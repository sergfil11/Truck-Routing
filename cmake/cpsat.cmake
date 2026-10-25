# Reserved for the upcoming CP-SAT model. Do not silently build Gurobi here.
message(STATUS "CP-SAT scaffold: building truck_routing_core only. No optimizer executable yet.")
# When the model is ported, add find_package(ortools CONFIG REQUIRED),
# src/cpsat sources and apps/cpsat entry point here; link ortools::ortools.
