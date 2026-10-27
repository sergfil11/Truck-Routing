#include "parsing.hpp"
#include "preprocessing.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F fn) {
    bool rejected = false;
    try { fn(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Expected invalid product mapping rejection");
}

void check_mapping() {
    std::map<std::string, std::vector<std::string>> products;
    std::vector<int> ids, lengths;
    for (int station = 0; station < 12; ++station) {
        ids.push_back(station);
        lengths.push_back(2);
        products[std::to_string(station)] = {
            "station_" + std::to_string(station) + "_tank_0",
            "station_" + std::to_string(station) + "_tank_1"};
    }
    const auto all = global_product_mapping(products, ids, lengths);
    require(all.size() == 24, "All tanks must be mapped");
    for (int tank = 0; tank < 24; ++tank)
        require(all.at(tank) == "station_" + std::to_string(tank / 2) +
            "_tank_" + std::to_string(tank % 2), "Numeric station order");

    // Removed stations must neither contribute products nor shift global IDs.
    // Different tank counts also check the station/tank boundary.
    products["10"].pop_back();
    const auto filtered = global_product_mapping(products, {2, 10, 11}, {2, 1, 2});
    const std::map<int, std::string> expected{
        {0, "station_2_tank_0"}, {1, "station_2_tank_1"},
        {2, "station_10_tank_0"}, {3, "station_11_tank_0"}, {4, "station_11_tank_1"}};
    require(filtered == expected, "Filtered global tank identity");
    require(global_product_mapping(products, {}, {}).empty(), "No selected stations");
    rejects([&] { global_product_mapping(products, {2}, {}); });
    rejects([&] { global_product_mapping(products, {12}, {1}); });
    rejects([&] { global_product_mapping(products, {2}, {1}); });
    rejects([&] { global_product_mapping(products, {2}, {3}); });
}

void check_preprocessing() {
    // Only original stations 2 and 10 have tanks. Each truck accepts one fuel.
    // The original lexical mapping would swap diesel and petrol here.
    const int n = 12;
    std::vector<std::vector<double>> times(n, std::vector<double>(n, 10));
    std::vector<std::map<std::string, double>> depot(n, {{"to", 10}, {"from", 10}});
    std::vector<std::map<std::string, std::vector<double>>> stations(n, {{"min", {}}, {"max", {}}});
    std::vector<std::vector<std::vector<double>>> profiles(n);
    std::vector<std::vector<double>> consumption(n);
    std::map<std::string, std::vector<std::string>> products;
    for (int i = 0; i < n; ++i) products[std::to_string(i)] = {};
    for (int i : {2, 10}) {
        stations[i] = {{"min", {1000}}, {"max", {1000}}};
        profiles[i] = {std::vector<double>(12, 0)};
        consumption[i] = {0};
    }
    products["2"] = {"diesel"};
    products["10"] = {"petrol"};
    std::vector<std::vector<double>> trucks{{1000}, {1000}};
    std::vector<std::vector<int>> access(n, std::vector<int>(2, 1));
    std::vector<bool> pipes(2, false), loaded(2, false);
    std::vector<double> docs(n, 1), shifts(2, 720);
    // Distinct paperwork times expose indexing into the unfiltered array.
    for (int i = 0; i < n; ++i) docs[i] = i + 1;
    const std::map<std::string, std::set<std::vector<std::string>>> schemes{
        {"0", {{"diesel"}}}, {"1", {{"petrol"}}}};
    const auto p = gurobi_preprocessing(n, 720, 2, times, depot, stations, trucks,
        1, 7, access, pipes, 1.0, docs, shifts, loaded, profiles, products,
        schemes, consumption, {0, 0}, {1, 1});
    require(p.tank_count == 2 && p.gl_num.at({0, 0}) == 0 &&
        p.gl_num.at({1, 0}) == 1, "Filtered tank numbering");
    for (int truck = 0; truck < 2; ++truck) {
        for (int sh = 0; sh < 2; ++sh) {
            const auto& routes = p.best_by_pattern.at(truck).at(sh);
            require(routes.size() == 1, "Exactly one compatible tank per truck and mode");
            const auto& [pattern, entry] = *routes.begin();
            require(pattern == std::vector<int>{truck}, "Fuel-compatible route retained");
            require(entry.second.res_comps == std::vector<std::pair<int, std::vector<int>>>{{truck, {0}}},
                "Correct tank/compartment assignment");
            require(entry.first > 0 && entry.first <= 720, "Route time within shift");
            const int original_station = truck == 0 ? 2 : 10;
            const double expected_time = 33.6 + 20 + 6 + docs[original_station];
            require(std::abs(entry.first - expected_time) < 1e-8,
                "Paperwork time must follow the retained station's original ID");
        }
    }
}

void check_empty_compartment() {
    // The same physical load with the unused 2000-litre compartment at each
    // position. Distinct volumes expose accidental use of compacted indices.
    for (int empty = 0; empty < 3; ++empty) for (bool two_pipes : {false, true})
    for (bool preloaded : {false, true}) {
        std::vector<double> compartments{1000, 3000};
        compartments.insert(compartments.begin() + empty, 2000);
        std::vector<std::string> scheme{"diesel", "petrol"};
        scheme.insert(scheme.begin() + empty, "");
        std::vector<std::vector<double>> trucks{compartments};
        std::vector<std::vector<int>> access{{1}};
        std::vector<bool> pipes{two_pipes}, loaded{preloaded};
        std::vector<double> docs{1}, shifts{720};
        std::vector<std::vector<std::vector<double>>> profiles{
            {std::vector<double>(12, 0), std::vector<double>(12, 0)}};
        const auto p = gurobi_preprocessing(1, 720, 1, {{0}},
            {{{"to", 10}, {"from", 10}}},
            {{{"min", {1000, 3000}}, {"max", {1000, 3000}}}}, trucks,
            1, 7, access, pipes, 1.0, docs, shifts, loaded, profiles,
            {{"0", {"diesel", "petrol"}}}, {{"0", {scheme}}}, {{0, 0}}, {0}, {1});
        for (int sh = 0; sh < 2; ++sh) {
            const std::string label = "Empty compartment " + std::to_string(empty) +
                ", mode " + std::to_string(sh) + ": ";
            const auto& routes = p.best_by_pattern.at(0).at(sh);
            require(routes.size() == 1, label + "one compatible route");
            const auto& [pattern, entry] = *routes.begin();
            const auto& fill = entry.second;
            require(pattern == std::vector<int>{0, 1}, label + "both tanks covered");
            require(fill.start_shifted == (sh == 1), label + "mode preserved");
            require(fill.res_comps.size() == 2, label + "two tank assignments");
            std::vector<bool> used(3, false), served(2, false);
            std::vector<std::string> actual_scheme(3, "");
            for (const auto& [tank, comps] : fill.res_comps) {
                require(tank >= 0 && tank < 2 && !served.at(tank), label + "unique tank");
                served[tank] = true;
                double volume = 0;
                for (int comp : comps) {
                    require(comp >= 0 && comp < 3 && comp != empty && !used.at(comp),
                        label + "original compartment index, empty unused, no reuse");
                    used[comp] = true;
                    volume += compartments.at(comp);
                    actual_scheme[comp] = tank == 0 ? "diesel" : "petrol";
                }
                require(volume == (tank == 0 ? 1000 : 3000), label + "physical volume bounds");
            }
            require(actual_scheme == scheme, label + "original fuel scheme");
            require(entry.first > 0 && entry.first <= 720, label + "shift bound");
            // 4000 litres delivered, irrespective of the unused 2000-litre compartment.
            const double expected_time = 33.6 + 1 + 20 +
                (preloaded ? 0 : 12) + (two_pipes ? 9 : 12);
            require(std::abs(entry.first - expected_time) < 1e-8,
                label + "loading and unloading must use assigned volumes");
        }
    }
}

void check_route_duration() {
    const std::vector<Station> stations{
        Station(0, 10, 10, {}, {}, {}, 5), Station(1, 10, 10, {}, {}, {}, 30)};
    const std::map<int, std::pair<int, int>> reverse{{0, {0, 0}}, {1, {0, 1}}, {2, {1, 0}}};
    const auto duration = [&](const std::vector<double>& compartments, const Filling& fill,
                              bool preloaded, bool two_pipes) {
        return compute_time_for_route(reverse, compartments, preloaded, fill,
            two_pipes, stations, {{0, 12}, {12, 0}}, {5, 30}, 1).first;
    };
    for (bool preloaded : {false, true}) for (bool two_pipes : {false, true}) {
        const Filling full({{0, {0}}, {1, {1}}, {2, {2}}}, {60, 90}, {0, 1});
        // Full truck: 18 minutes loading. Two hoses save 3 minutes at station 0;
        // the last 3000 litres are unloaded separately at station 1.
        const double full_expected = 100.6 + (preloaded ? 0 : 18) + (two_pipes ? 15 : 18);
        require(std::abs(duration({1000, 2000, 3000}, full, preloaded, two_pipes) - full_expected) < 1e-8,
            "Full load and separate station unloading times");
        const Filling partial({{0, {0}}, {2, {2}}}, {60, 90}, {0, 1});
        const double partial_expected = 100.6 + (preloaded ? 0 : 12) + 12;
        require(std::abs(duration({1000, 2000, 3000, 4000}, partial, preloaded, two_pipes) - partial_expected) < 1e-8,
            "Multiple unused compartments must not contribute to duration");
        const Filling one_tank({{0, {0, 2}}}, {60}, {0});
        require(std::abs(duration({1000, 2000, 3000}, one_tank, preloaded, two_pipes) -
            (58.6 + (preloaded ? 0 : 12) + 12)) < 1e-8,
            "Compartments for one tank remain one unloading job with two hoses");
    }
    const Filling duplicate({{0, {0}}, {1, {0}}}, {60}, {0});
    rejects([&] { duration({1000}, duplicate, false, false); });
    const Filling invalid({{0, {1}}}, {60}, {0});
    rejects([&] { duration({1000}, invalid, false, false); });
}

void check_repeated_preprocessing() {
    // At the first arrival: 900 free + 0.25 * 400 consumed = 1000 litres.
    // The 1000-litre truck fits exactly; the 1001-litre truck must not fit.
    // Later hours free another 300 litres, so both trucks fit in the end mode.
    std::vector<std::vector<std::vector<double>>> profiles{
        {{0.25, 0.75, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};
    const auto original_profiles = profiles;
    std::vector<std::vector<int>> access{{1, 1}};
    std::vector<bool> pipes{false, false}, loaded{false, false};
    std::vector<double> docs{1}, shifts{720, 720};
    const auto run = [&] {
        return gurobi_preprocessing(1, 720, 2, {{0}},
            {{{"to", 10}, {"from", 10}}}, {{{"min", {1000}}, {"max", {900}}}},
            {{1000}, {1001}}, 1, 7, access, pipes, 1.0, docs, shifts, loaded,
            profiles, {{"0", {"diesel"}}},
            {{"0", {{"diesel"}}}, {"1", {{"diesel"}}}}, {{400}}, {0, 0}, {1, 1});
    };
    const auto first = run();
    require(first.best_by_pattern.at(0).at(1).size() == 1 &&
        first.best_by_pattern.at(1).at(1).empty(), "First-arrival consumption boundary");
    for (int k = 0; k < 2; ++k)
        require(first.best_by_pattern.at(k).at(0).size() == 1, "Cumulative end-mode consumption");
    const auto second = run();
    require(profiles == original_profiles, "Preprocessing must preserve input consumption fractions");
    require(first.tank_count == second.tank_count && first.gl_num == second.gl_num &&
        first.reservoirs == second.reservoirs && first.H_k == second.H_k &&
        first.filling_times == second.filling_times, "Repeated preprocessing metadata");
    for (int k = 0; k < 2; ++k) for (int sh = 0; sh < 2; ++sh) {
        const auto& a = first.best_by_pattern.at(k).at(sh);
        const auto& b = second.best_by_pattern.at(k).at(sh);
        require(a.size() == b.size(), "Repeated preprocessing route count");
        for (const auto& [pattern, entry] : a) {
            const auto& other = b.at(pattern);
            const auto& f = entry.second;
            const auto& g = other.second;
            require(entry.first == other.first && f.res_comps == g.res_comps &&
                f.arrival_times == g.arrival_times && f.stations_ordered == g.stations_ordered &&
                f.timelog == g.timelog && f.total_time == g.total_time &&
                f.start_shifted == g.start_shifted, "Repeated preprocessing route contents");
        }
    }
}

void check_input(const char* path) {
    const auto d = load_real_data(path);
    std::vector<int> ids, lengths;
    for (int i = 0; i < d.N; ++i) {
        const auto& station = d.stations.at(i);
        if (station.count("max") && !station.at("max").empty()) {
            ids.push_back(i);
            lengths.push_back(static_cast<int>(station.at("max").size()));
        }
    }
    const auto mapping = global_product_mapping(d.reservoir_to_product, ids, lengths);
    const auto numbering = global_numeration(lengths);
    for (const auto& [local, global] : numbering)
        require(mapping.at(global) == d.reservoir_to_product.at(
            std::to_string(ids.at(local.first))).at(local.second), "Input product mismatch");
    require(mapping.size() == numbering.size(), "Input tank count mismatch");
    std::cout << path << ": " << mapping.size() << " tanks, 0 product mismatches\n";
}
} // namespace

int main(int argc, char** argv) try {
    check_mapping();
    check_preprocessing();
    check_empty_compartment();
    check_route_duration();
    check_repeated_preprocessing();
    for (int i = 1; i < argc; ++i) check_input(argv[i]);
    std::cout << "Product mapping, empty-compartment and repeated-preprocessing checks passed\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
