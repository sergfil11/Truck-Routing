#include "cp_sat.hpp"
#include "parsing.hpp"
#include "preprocessing.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message); // active in Release too
}
struct Fixture {
    RoutePool routes;
    int tanks;
    std::vector<double> limits, fills;
    std::vector<int> owning, critical;
    int loads = 0, doubles = 0;
    Fixture(int K, int T) : routes(K, std::vector<std::map<std::vector<int>, std::pair<double, Filling>>>(2)),
        tanks(T), limits(K, 10), fills(K, 0), owning(K, 1), critical(T, 1) {}
    CpSatInput input() const { return {routes, tanks, limits, owning, critical, fills, loads, doubles}; }
    void add(int k, int sh, std::vector<int> pattern, double minutes) {
        // These are synthetic master-problem columns; physical Filling is tested separately.
        routes[k][sh].emplace(pattern, std::make_pair(minutes, Filling({}, {}, {}, {}, minutes, sh == 1)));
    }
};

// Independent oracle transcribed from src/gurobi/gurobi.cpp (integer branch).
// It deliberately does not use production rounding/model/checker helpers.
bool reference_feasible(const Fixture& f, const CpSatCoveringModel& model, const std::vector<int>& x) {
    std::vector<int> coverage(f.tanks, 0);
    int all_routes = 0, all_used = 0, all_loads = 0;
    const bool loads = !f.limits.empty() && f.loads > 0;
    for (int k = 0; k < static_cast<int>(f.routes.size()); ++k) {
        double duration = 0;
        for (int sh = 0; sh < 2; ++sh) {
            int count = 0, r = 0;
            for (const auto& [pattern, entry] : f.routes[k][sh]) {
                const int selected = x.at(model.g.at({sh == 1, k, r++}));
                count += selected;
                duration += std::ceil(entry.first) * selected;
                for (int tank = 0; tank < f.tanks; ++tank)
                    if (std::find(pattern.begin(), pattern.end(), tank) != pattern.end()) coverage[tank] += selected;
            }
            if (count > x.at(sh == 0 ? model.y1[k] : model.y2[k])) return false;
            all_routes += count;
        }
        const int y = x.at(model.y[k]);
        if (2 * y < x.at(model.y1[k]) + x.at(model.y2[k])) return false;
        all_used += y;
        if (loads) {
            const int l = x.at(model.l[k]);
            if (l > y) return false;
            all_loads += l;
            duration += std::ceil(f.fills[k]) * l;
        }
        if (!f.limits.empty() && duration > std::ceil(f.limits[k])) return false;
    }
    for (int i = 0; i < f.tanks; ++i) {
        if (f.critical[i] == 1 ? coverage[i] != 1 : coverage[i] > 1) return false;
    }
    if (loads && all_loads < f.loads) return false;
    if (f.doubles > 0 && all_routes < all_used + f.doubles) return false;
    return true;
}

bool in_domain(std::int64_t value, const google::protobuf::RepeatedField<std::int64_t>& domain) {
    for (int i = 0; i < domain.size(); i += 2)
        if (domain[i] <= value && value <= domain[i + 1]) return true;
    return false;
}
bool proto_feasible(const CpSatCoveringModel& model, const std::vector<int>& x) {
    for (int j = 0; j < model.proto.variables_size(); ++j)
        if (!in_domain(x[j], model.proto.variables(j).domain())) return false;
    for (const auto& ct : model.proto.constraints()) {
        require(ct.has_linear() && ct.enforcement_literal_size() == 0, "Expected direct linear constraints");
        std::int64_t lhs = 0;
        for (int j = 0; j < ct.linear().vars_size(); ++j)
            lhs += ct.linear().coeffs(j) * x.at(ct.linear().vars(j));
        if (!in_domain(lhs, ct.linear().domain())) return false;
    }
    return true;
}

std::uint64_t assignments_checked = 0;
int cases_checked = 0;
CpSatResult compare(const Fixture& f, const std::string& label) {
    const auto model = build_cp_sat_covering(f.input());
    const int n = model.proto.variables_size();
    require(n < 21, "Keep exhaustive fixtures small");
    std::optional<std::int64_t> optimum;
    std::vector<int> x(n);
    for (std::uint64_t mask = 0; mask < (std::uint64_t{1} << n); ++mask) {
        for (int j = 0; j < n; ++j) x[j] = (mask >> j) & 1;
        require(reference_feasible(f, model, x) == proto_feasible(model, x), label + ": feasible set differs");
        std::int64_t expected_cost = 0;
        for (int k = 0; k < static_cast<int>(f.routes.size()); ++k) {
            const auto w = f.owning.empty() ? 1 : f.owning[k] == 0 ? 1001 : f.owning[k];
            expected_cost += static_cast<std::int64_t>(w) * x[model.y[k]];
        }
        const auto& obj = model.proto.objective();
        // CpObjectiveProto uses a zero (unset) scaling_factor to mean one.
        require(obj.offset() == 0 && (obj.scaling_factor() == 0 || obj.scaling_factor() == 1),
                label + ": unexpected objective scaling");
        std::int64_t actual_cost = 0;
        for (int j = 0; j < obj.vars_size(); ++j) actual_cost += obj.coeffs(j) * x.at(obj.vars(j));
        require(actual_cost == expected_cost, label + ": objective differs");
        if (reference_feasible(f, model, x) && (!optimum || expected_cost < *optimum)) optimum = expected_cost;
        ++assignments_checked;
    }
    CpSatOptions options;
    options.time_limit_seconds = 10;
    const auto result = cp_sat_covering(f.input(), options);
    require(result.has_solution == optimum.has_value(), label + ": solver feasibility differs");
    if (optimum) {
        require(result.status == "OPTIMAL" && result.objective == *optimum, label + ": optimum differs");
        require(std::abs(result.best_bound - *optimum) < 1e-8, label + ": bound differs");
        require(check_cp_sat_solution(f.input(), result).empty(), label + ": invalid extracted solution");
    } else require(result.status == "INFEASIBLE", label + ": expected proof of infeasibility");
    ++cases_checked;
    return result;
}

template<class F> void rejects(F fn) {
    bool rejected = false;
    try { fn(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Expected invalid input rejection");
}

void check_preprocessing(const char* path) {
    auto d = load_real_data(path);
    auto p = gurobi_preprocessing(d.N, d.H, d.K, d.time_matrix, d.depot_times, d.stations,
        d.trucks, 3, 7, d.access, d.dual_piped, 1.5, d.docs_fill, d.H_k, d.loading_prepared,
        d.consumption_percent, d.reservoir_to_product, d.truck_to_variants,
        d.consumption, d.starting_time, d.owning);
    const CpSatInput in{p.best_by_pattern, p.tank_count, p.H_k, d.owning, d.is_critical, p.filling_times};
    const auto solution = cp_sat_covering(in);
    require(solution.status == "OPTIMAL" && solution.objective == 1, "Preprocessing fixture optimum");
    require(solution.selected.size() == 1, "Preprocessing fixture must choose one route");
    const auto& route = solution.selected.front();
    require(route.pattern == std::vector<int>{0}, "Critical tank coverage");
    require(route.filling.res_comps.size() == 1 && route.filling.res_comps.front().first == 0 &&
        route.filling.res_comps.front().second == std::vector<int>{0}, "Compartment assignment");
    const double volume = d.trucks.at(0).at(0);
    require(volume >= d.stations[0].at("min")[0] && volume <= d.stations[0].at("max")[0], "Volume bounds");
    require(route.model_minutes == std::ceil(route.original_minutes) && route.model_minutes <= 720, "Time bound");
    require(route.filling.start_shifted == std::get<0>(route.key), "start_shifted identity");
    std::ostringstream out;
    print_cp_sat_results(out, solution, p.gl_num, d.trucks);
    require(out.str().find("1000") != std::string::npos, "Selected filling report");
}
} // namespace

int main(int argc, char** argv) try {
    require(ceil_minutes(33.6) == 34 && ceil_minutes(0) == 0 && ceil_minutes(720) == 720, "ceil policy");
    require(ceil_minutes(std::nextafter(5.0, 6.0)) == 6, "ceil must not subtract epsilon");
    rejects([] { ceil_minutes(-1); });
    rejects([] { ceil_minutes(std::numeric_limits<double>::infinity()); });
    rejects([] { ceil_minutes(std::numeric_limits<double>::quiet_NaN()); });
    rejects([] { ceil_minutes(1e30); });

    Fixture base(2, 3);
    base.add(0, 0, {0, 1}, 4.1);
    base.add(0, 1, {2}, 4.2);
    base.add(1, 0, {1, 2}, 3);
    base.add(1, 1, {0}, 3);
    require(compare(base, "base").objective == 1, "Base optimum");
    // Cross product exercises both conditional blocks and LP-like auxiliary flags.
    for (int loads : {0, 1, 2, 3}) for (int doubles : {0, 1, 2, 3}) {
        auto f = base;
        f.loads = loads; f.doubles = doubles; f.fills = {0.2, 1.2};
        compare(f, "loads/doubles");
    }
    auto f = base;
    f.limits.clear(); f.loads = 100; f.fills.clear();
    compare(f, "empty H ignores loads");
    f.doubles = 1;
    compare(f, "empty H keeps double races");
    f = base; f.owning = {0, 1};
    auto res = compare(f, "external truck penalty");
    require(res.y == std::vector<int>({0, 1}), "Prefer owned truck");
    f.owning = {7, 0}; compare(f, "nonstandard positive weight");
    f.owning.clear(); compare(f, "default weights");
    f = base; f.critical = {1, 0, 2}; compare(f, "only value 1 is critical");
    f = base; f.limits = {4, 4}; compare(f, "infeasible time");
    Fixture missing(1, 1); compare(missing, "uncovered critical tank");
    missing.critical = {0}; compare(missing, "empty optional pool");
    missing.owning = {-1};
    require(compare(missing, "negative weight preserves unused y").y[0] == 1, "Do not strengthen y to OR(g)");
    Fixture empty(0, 0); compare(empty, "empty model");
    Fixture fractional(1, 2);
    fractional.add(0, 0, {0}, .4); fractional.add(0, 1, {1}, .4);
    fractional.limits = {1};
    require(!compare(fractional, "ceil each route, not total").has_solution, "Rounding must change feasibility here");
    fractional.limits = {1.1};
    require(compare(fractional, "ceil shift limit").has_solution, "H must round upward too");
    fractional.loads = 1; fractional.fills = {.1};
    require(!compare(fractional, "ceil extra loading").has_solution, "Loading must round upward");
    Fixture conflict(1, 1);
    conflict.add(0, 0, {0}, 1); conflict.add(0, 1, {0}, 1); conflict.doubles = 1;
    compare(conflict, "no duplicate service even for same truck");

    // Fixed-seed independent small instances, including empty patterns and zero times.
    std::mt19937 rng(42);
    for (int test = 0; test < 24; ++test) {
        Fixture random(2, 2);
        random.critical = {static_cast<int>(rng() % 2), static_cast<int>(rng() % 2)};
        random.limits = {double(rng() % 8), double(rng() % 8) + .1};
        random.owning = {static_cast<int>(rng() % 2), 1};
        random.loads = rng() % 3; random.doubles = rng() % 3;
        random.fills = {1.1, 2.2};
        for (int k = 0; k < 2; ++k) for (int sh = 0; sh < 2; ++sh) {
            std::vector<int> pattern;
            for (int t = 0; t < 2; ++t) if (rng() % 2) pattern.push_back(t);
            random.add(k, sh, pattern, (rng() % 30) / 10.0);
        }
        compare(random, "random fixture " + std::to_string(test));
    }
    // Map order differs from insertion order; preserve (shift,k,r) during extraction.
    Fixture order(1, 2);
    order.add(0, 1, {1}, 1); order.add(0, 1, {0}, 1);
    order.critical = {1, 0};
    res = compare(order, "map order");
    require(res.selected.size() == 1 && res.selected[0].key == RouteKey{true, 0, 0}, "Map index drift");
    auto broken = res; broken.y[0] = 0;
    require(!check_cp_sat_solution(order.input(), broken).empty(), "Checker must reject broken links");
    broken = res; broken.selected.push_back(broken.selected.front());
    require(!check_cp_sat_solution(order.input(), broken).empty(), "Checker must reject duplicate coverage");
    broken = res; ++broken.objective;
    require(!check_cp_sat_solution(order.input(), broken).empty(), "Checker must reject bad objective");
    f = base; f.critical.clear(); rejects([&] { build_cp_sat_covering(f.input()); });
    f = base; f.routes[0][0].clear(); f.add(0, 0, {99}, 1); rejects([&] { build_cp_sat_covering(f.input()); });
    CpSatOptions timeout; timeout.time_limit_seconds = 0;
    res = cp_sat_covering(base.input(), timeout);
    require(!res.has_solution && res.status == "UNKNOWN" && res.selected.empty() && res.y.empty(), "No extraction after timeout without solution");
    require(argc == 2, "Pass synthetic preprocessing fixture path");
    check_preprocessing(argv[1]);
    std::cout << "PASS: " << cases_checked << " models, " << assignments_checked
              << " binary assignments; solver optima, rounding, extraction, preprocessing and filling checks.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
