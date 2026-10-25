#include "parsing.hpp"
#include "preprocessing.hpp"
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
    for (int i = 1; i < argc; ++i) check_input(argv[i]);
    std::cout << "Product mapping and preprocessing checks passed\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
