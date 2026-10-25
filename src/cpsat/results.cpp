#include "cp_sat.hpp"
#include <iomanip>
#include <ostream>
#include <stdexcept>

void print_cp_sat_results(std::ostream& out, const CpSatResult& result,
                          const std::map<std::pair<int, int>, int>& gl_num,
                          const std::vector<std::vector<double>>& trucks) {
    out << "CP-SAT " << result.solver_version << "\nСтатус: " << result.status
        << "\nВремя решения: " << result.wall_time_seconds << " секунд\n";
    if (!result.has_solution) {
        out << "Допустимое решение не получено.\n";
        if (!result.diagnostic.empty()) out << result.diagnostic << '\n';
        return;
    }
    out << "Целевая функция: " << result.objective << "\nНижняя граница: " << result.best_bound
        << "\nПроверка ограничений минутной модели: OK\n"
        << "Индексы ниже начинаются с 0; АЗС и резервуары — после фильтрации preprocessing.\n";
    std::map<int, std::pair<int, int>> reverse;
    for (const auto& [local, global] : gl_num) reverse.emplace(global, local);
    for (const auto& route : result.selected) {
        const auto [shift, k, r] = route.key;
        out << "\nБензовоз " << k << ", sh=" << (shift ? 1 : 0)
            << (shift ? " (от начала смены)" : " (от конца смены)") << ", r=" << r
            << "\nВремя маршрута: " << route.model_minutes << " мин (до ceil: "
            << std::setprecision(12) << route.original_minutes << ")\nПорядок АЗС:";
        for (int station : route.filling.stations_ordered) out << ' ' << station;
        out << '\n';
        for (const auto& [tank, compartments] : route.filling.res_comps) {
            out << "  Глобальный резервуар " << tank;
            const auto it = reverse.find(tank);
            if (it != reverse.end()) out << " (АЗС " << it->second.first << ", резервуар " << it->second.second << ')';
            out << "; отсеки:";
            double volume = 0;
            for (int comp : compartments) {
                if (comp < 0) throw std::runtime_error("Negative compartment index in Filling");
                out << ' ' << comp;
                volume += trucks.at(k).at(comp);
            }
            out << "; объём " << volume << " л\n";
        }
    }
    for (std::size_t k = 0; k < result.y.size(); ++k) {
        if (!result.y[k]) continue;
        std::int64_t total = 0;
        int count = 0;
        for (const auto& route : result.selected) if (std::get<1>(route.key) == static_cast<int>(k)) {
            total += route.model_minutes; ++count;
        }
        out << "\nБензовоз " << k << ": рейсов " << count << ", сумма времени рейсов " << total << " мин";
        if (!result.l.empty()) out << ", загрузка под сменщика l=" << result.l[k];
        out << '\n';
    }
}
