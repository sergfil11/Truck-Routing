#pragma once

#include "prep_functions.hpp"
#include "ortools/sat/cp_model.pb.h"
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <tuple>

using RoutePool = std::vector<std::vector<std::map<std::vector<int>, std::pair<double, Filling>>>>;
// Same identity as Gurobi: (start_shifted, truck, position in std::map).
using RouteKey = std::tuple<bool, int, int>;

struct CpSatInput {
    const RoutePool& routes;
    int tank_count;
    const std::vector<double>& H_k;
    const std::vector<int>& owning;
    const std::vector<int>& is_critical;
    const std::vector<double>& filling_times;
    int load_number = 0;
    int double_race_number = 0;
};

struct CpSatOptions {
    double time_limit_seconds = 900;
    int workers = 1;
    int random_seed = 42;
    bool log_search = false;
};

// Exposed for reproducible inspection and assignment-by-assignment equivalence tests.
struct CpSatCoveringModel {
    operations_research::sat::CpModelProto proto;
    std::vector<int> y, y1, y2, l; // indices in proto.variables
    std::map<RouteKey, int> g;
};

struct CpSatSelectedRoute {
    RouteKey key;
    std::vector<int> pattern;
    double original_minutes;
    std::int64_t model_minutes;
    Filling filling; // snapshot: r cannot be confused with an index in another pool
};

struct CpSatResult {
    std::string status;
    std::string solver_version;
    std::string diagnostic;
    bool has_solution = false;
    std::int64_t objective = 0; // only meaningful when has_solution
    double best_bound = std::numeric_limits<double>::quiet_NaN();
    double wall_time_seconds = 0;
    std::vector<int> y, y1, y2, l;
    std::vector<CpSatSelectedRoute> selected;
};

// All temporal constants entering the CP-SAT model use this exact policy.
std::int64_t ceil_minutes(double value);
CpSatCoveringModel build_cp_sat_covering(const CpSatInput& input);
CpSatResult cp_sat_covering(const CpSatInput& input, const CpSatOptions& options = {});
// Independently recomputes the modeled constraints from the original pool.
std::vector<std::string> check_cp_sat_solution(const CpSatInput& input, const CpSatResult& result);
void print_cp_sat_results(std::ostream& out, const CpSatResult& result,
                          const std::map<std::pair<int, int>, int>& gl_num,
                          const std::vector<std::vector<double>>& trucks);
