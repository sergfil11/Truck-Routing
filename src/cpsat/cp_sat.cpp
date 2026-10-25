#include "cp_sat.hpp"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
using namespace operations_research::sat;

std::int64_t weight(const CpSatInput& in, int k) {
    if (in.owning.empty()) return 1;
    return in.owning[k] == 0 ? 1001 : in.owning[k];
}

void validate_input(const CpSatInput& in) {
    const auto K = in.routes.size();
    if (K > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        in.tank_count < 0 || in.is_critical.size() != static_cast<std::size_t>(in.tank_count))
        throw std::invalid_argument("Invalid truck/tank count or is_critical size");
    if ((!in.owning.empty() && in.owning.size() != K) ||
        (!in.H_k.empty() && in.H_k.size() != K))
        throw std::invalid_argument("owning and H_k must be empty or have K entries");
    const bool loads = !in.H_k.empty() && in.load_number > 0;
    if (loads && in.filling_times.size() != K)
        throw std::invalid_argument("Active load_number requires K filling_times");
    for (std::size_t k = 0; k < K; ++k) {
        if (in.routes[k].size() != 2) throw std::invalid_argument("Expected two route modes per truck");
        if (!in.H_k.empty()) ceil_minutes(in.H_k[k]);
        if (loads) ceil_minutes(in.filling_times[k]);
        for (const auto& mode : in.routes[k]) {
            if (mode.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                throw std::invalid_argument("Too many routes");
            for (const auto& [pattern, data] : mode) {
                ceil_minutes(data.first);
                if (!std::is_sorted(pattern.begin(), pattern.end()) ||
                    std::adjacent_find(pattern.begin(), pattern.end()) != pattern.end())
                    throw std::invalid_argument("Route pattern must be sorted and unique");
                for (int tank : pattern)
                    if (tank < 0 || tank >= in.tank_count)
                        throw std::invalid_argument("Route pattern contains an unknown tank");
            }
        }
    }
}
} // namespace

std::int64_t ceil_minutes(double value) {
    // Reject nonfinite/negative times and leave headroom for solver arithmetic.
    // No epsilon subtraction: even a representable value just above n rounds to n+1.
    if (!std::isfinite(value) || value < 0 ||
        value >= static_cast<double>(std::numeric_limits<std::int64_t>::max() / 4))
        throw std::invalid_argument("Time must be finite, nonnegative and fit int64");
    return static_cast<std::int64_t>(std::ceil(value));
}

CpSatCoveringModel build_cp_sat_covering(const CpSatInput& in) {
    validate_input(in);
    const int K = static_cast<int>(in.routes.size());
    const bool loads = !in.H_k.empty() && in.load_number > 0;
    CpModelBuilder model;
    CpSatCoveringModel result;
    std::vector<BoolVar> y, y1, y2, l;
    LinearExpr objective, used_trucks, all_routes, all_loads;
    std::vector<LinearExpr> coverage(in.tank_count);
    for (int k = 0; k < K; ++k) {
        const auto suffix = std::to_string(k);
        y.push_back(model.NewBoolVar().WithName("y_" + suffix));
        y1.push_back(model.NewBoolVar().WithName("y1_" + suffix));
        y2.push_back(model.NewBoolVar().WithName("y2_" + suffix));
        result.y.push_back(y.back().index());
        result.y1.push_back(y1.back().index());
        result.y2.push_back(y2.back().index());
        objective += LinearExpr(y.back()) * weight(in, k);
        used_trucks += y.back();
        if (loads) {
            l.push_back(model.NewBoolVar().WithName("l_" + suffix));
            result.l.push_back(l.back().index());
            all_loads += l.back();
            model.AddLessOrEqual(l.back(), y.back()).WithName("load_if_used_" + suffix);
        }
        LinearExpr duration;
        for (int sh = 0; sh < 2; ++sh) {
            LinearExpr count;
            int r = 0;
            for (const auto& [pattern, data] : in.routes[k][sh]) {
                const auto g = model.NewBoolVar().WithName(
                    "g_" + std::to_string(sh) + "_" + suffix + "_" + std::to_string(r));
                result.g.emplace(RouteKey{sh == 1, k, r++}, g.index());
                count += g;
                all_routes += g;
                if (!in.H_k.empty()) duration += LinearExpr(g) * ceil_minutes(data.first);
                for (int tank : pattern) coverage[tank] += g;
            }
            model.AddLessOrEqual(count, sh == 0 ? y1.back() : y2.back())
                .WithName("Link_" + suffix + (sh == 0 ? "_y1" : "_y2"));
        }
        // Keep exactly the Gurobi link, including permissible unused auxiliary flags.
        model.AddLessOrEqual(LinearExpr(y1.back()) + y2.back(), LinearExpr(y.back()) * 2)
            .WithName("Shifts_link_" + suffix);
        if (!in.H_k.empty()) {
            if (loads) duration += LinearExpr(l.back()) * ceil_minutes(in.filling_times[k]);
            model.AddLessOrEqual(duration, ceil_minutes(in.H_k[k])).WithName("Shift_" + suffix);
        }
    }
    for (int i = 0; i < in.tank_count; ++i) {
        const auto name = "Reservoir_" + std::to_string(i);
        if (in.is_critical[i] == 1) model.AddEquality(coverage[i], 1).WithName(name);
        else model.AddLessOrEqual(coverage[i], 1).WithName(name);
    }
    if (loads) model.AddGreaterOrEqual(all_loads, in.load_number).WithName("load_number_satisfied");
    if (in.double_race_number > 0)
        model.AddGreaterOrEqual(all_routes, used_trucks + in.double_race_number).WithName("double_races");
    model.Minimize(objective);
    result.proto = model.Build();
    const auto error = ValidateCpModel(result.proto);
    if (!error.empty()) throw std::invalid_argument("Invalid CP-SAT model: " + error);
    return result;
}

CpSatResult cp_sat_covering(const CpSatInput& in, const CpSatOptions& options) {
    if (!std::isfinite(options.time_limit_seconds) || options.time_limit_seconds < 0 || options.workers < 1)
        throw std::invalid_argument("Invalid CP-SAT time limit or worker count");
    const auto model = build_cp_sat_covering(in);
    SatParameters params;
    params.set_max_time_in_seconds(options.time_limit_seconds);
    params.set_num_search_workers(options.workers);
    params.set_random_seed(options.random_seed);
    params.set_log_search_progress(options.log_search);
    params.set_absolute_gap_limit(0);
    params.set_relative_gap_limit(0);
    const auto response = SolveWithParameters(model.proto, params);
    CpSatResult result;
    result.status = CpSolverStatus_Name(response.status());
    result.solver_version = CpSatSolverVersion();
    result.diagnostic = response.solution_info();
    result.wall_time_seconds = response.wall_time();
    result.best_bound = response.best_objective_bound();
    result.has_solution = response.status() == CpSolverStatus::OPTIMAL ||
                          response.status() == CpSolverStatus::FEASIBLE;
    if (!result.has_solution) return result;
    const auto read = [&](const std::vector<int>& indices) {
        std::vector<int> values;
        for (int index : indices) values.push_back(static_cast<int>(response.solution(index)));
        return values;
    };
    result.y = read(model.y);
    result.y1 = read(model.y1);
    result.y2 = read(model.y2);
    result.l = read(model.l);
    for (int k = 0; k < static_cast<int>(in.routes.size()); ++k) {
        result.objective += weight(in, k) * result.y[k];
        for (int sh = 0; sh < 2; ++sh) {
            int r = 0;
            for (const auto& [pattern, data] : in.routes[k][sh]) {
                const RouteKey key{sh == 1, k, r++};
                if (response.solution(model.g.at(key)) == 1)
                    result.selected.push_back({key, pattern, data.first, ceil_minutes(data.first), data.second});
            }
        }
    }
    const auto errors = check_cp_sat_solution(in, result);
    if (!errors.empty()) throw std::runtime_error("CP-SAT solution verification failed: " + errors.front());
    return result;
}

std::vector<std::string> check_cp_sat_solution(const CpSatInput& in, const CpSatResult& result) {
    validate_input(in);
    std::vector<std::string> errors;
    const auto fail = [&](const std::string& text) { errors.push_back(text); };
    if (!result.has_solution) { fail("No solution to check"); return errors; }
    const int K = static_cast<int>(in.routes.size());
    const bool loads = !in.H_k.empty() && in.load_number > 0;
    const auto bits = [](const std::vector<int>& v, std::size_t size) {
        return v.size() == size && std::all_of(v.begin(), v.end(), [](int b) { return b == 0 || b == 1; });
    };
    if (!bits(result.y, K) || !bits(result.y1, K) || !bits(result.y2, K) || !bits(result.l, loads ? K : 0)) {
        fail("Wrong decision vector size or nonbinary value"); return errors;
    }
    std::vector<int> cover(in.tank_count, 0);
    std::vector<std::vector<int>> counts(K, std::vector<int>(2, 0));
    std::vector<long double> times(K, 0);
    std::set<RouteKey> seen;
    for (const auto& route : result.selected) {
        const auto [shift, k, r] = route.key;
        const int sh = shift ? 1 : 0;
        if (k < 0 || k >= K || r < 0 || static_cast<std::size_t>(r) >= in.routes[k][sh].size()) {
            fail("Unknown selected route"); continue;
        }
        if (!seen.insert(route.key).second) fail("Repeated selected route");
        auto it = in.routes[k][sh].begin();
        std::advance(it, r);
        if (route.pattern != it->first || route.original_minutes != it->second.first ||
            route.model_minutes != ceil_minutes(it->second.first)) fail("Route identity or time mismatch");
        for (int tank : it->first) ++cover[tank];
        ++counts[k][sh];
        times[k] += ceil_minutes(it->second.first);
    }
    for (int i = 0; i < in.tank_count; ++i)
        if ((in.is_critical[i] == 1 && cover[i] != 1) || cover[i] > 1) fail("Reservoir_" + std::to_string(i));
    std::int64_t objective = 0;
    int used = 0, load_count = 0;
    for (int k = 0; k < K; ++k) {
        if (counts[k][0] > result.y1[k] || counts[k][1] > result.y2[k]) fail("Link_" + std::to_string(k));
        if (result.y1[k] + result.y2[k] > 2 * result.y[k]) fail("Shifts_link_" + std::to_string(k));
        if (loads) {
            if (result.l[k] > result.y[k]) fail("load_if_used_" + std::to_string(k));
            times[k] += static_cast<long double>(ceil_minutes(in.filling_times[k])) * result.l[k];
            load_count += result.l[k];
        }
        if (!in.H_k.empty() && times[k] > ceil_minutes(in.H_k[k])) fail("Shift_" + std::to_string(k));
        objective += weight(in, k) * result.y[k];
        used += result.y[k];
    }
    if (loads && load_count < in.load_number) fail("load_number_satisfied");
    if (in.double_race_number > 0 && static_cast<std::int64_t>(result.selected.size()) <
        static_cast<std::int64_t>(used) + in.double_race_number) fail("double_races");
    if (objective != result.objective) fail("Objective mismatch");
    return errors;
}
