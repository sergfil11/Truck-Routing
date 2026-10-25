#include "cp_sat.hpp"
#include "parsing.hpp"
#include "preprocessing.hpp"
#include "run_options.hpp"
#include <chrono>
#include <clocale>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) try {
    std::setlocale(LC_ALL, ".utf8");
    std::setlocale(LC_NUMERIC, "C");
    const auto options = parse_run_options(argc, argv, "outputs/cpsat/main.txt");
    if (options.help) return 0;
    auto data = load_real_data(options.input.string());
    if (data.N <= 0 || data.K <= 0 || data.time_matrix.empty())
        throw std::invalid_argument("Expected a nonempty station matrix and positive N, K");
    std::ofstream file(options.output);
    if (!file) throw std::runtime_error("Cannot open output file: " + options.output.string());
    ScopedOutput redirect(file);
    print_input(data);

    // Same active scenario and shared preprocessing as apps/gurobi/main.cpp.
    const int R1 = 3, R2 = 7;
    data.daily_coefficient = 1.5;
    data.dual_piped = std::vector<bool>(data.K, false);
    data.H_k = std::vector<double>(data.K, 720.0);
    data.loading_prepared = std::vector<bool>(data.K, false);
    const auto start = std::chrono::steady_clock::now();
    auto prepared = gurobi_preprocessing(
        data.N, data.H, data.K, data.time_matrix, data.depot_times,
        data.stations, data.trucks, R1, R2, data.access, data.dual_piped,
        data.daily_coefficient, data.docs_fill, data.H_k, data.loading_prepared,
        data.consumption_percent, data.reservoir_to_product, data.truck_to_variants,
        data.consumption, data.starting_time, data.owning);
    std::cout << "Препроцессинг: "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
              << " секунд\nОкругление времени: ceil до целых минут после preprocessing.\n";
    const CpSatInput input{prepared.best_by_pattern, prepared.tank_count,
                           prepared.H_k, data.owning, data.is_critical,
                           prepared.filling_times, 0, 0};
    const auto result = cp_sat_covering(input);
    print_cp_sat_results(std::cout, result, prepared.gl_num, data.trucks);
    // Distinguish a completed run without a feasible solution from success.
    return result.has_solution ? 0 : 2;
} catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
}
