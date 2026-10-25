#include <iostream>
#include <algorithm>
#include <memory>
#include <prep_functions.hpp>
#include <iomanip>
#include <regex>
#include "gurobi.hpp"
using namespace std;


// struct GurobiCoveringResult {
//     unique_ptr<GRBModel> model;
//     vector<GRBVar> y;
//     map<pair<int,int>, GRBVar> g;
// };

unique_ptr<GurobiCoveringResult> gurobi_covering(
    const vector<vector<map<vector<int>, pair<double, Filling>>>>& best_by_pattern, // маршруты
    const vector<map<string, double>>& reservoirs,                                 // {min,max}
    int tank_count,
    int H,
    int K,
    int timelimit,
    const map<pair<int,int>, int>& gl_num,
    const vector<double>& H_k,
    vector<int> owning,
    const vector<int>& is_critical,
    int load_number,
    int double_race_number,
    const vector<double>& filling_times,
    bool is_relaxation
) {

    char vtype = is_relaxation ? GRB_CONTINUOUS : GRB_BINARY;
    // patterns[k][sh][r] = vector<int> pattern
    // times[k][sh][r] = double time
    vector<vector<vector<vector<int>>>> patterns(K, vector<vector<vector<int>>>(2));
    vector<vector<vector<double>>> times(K, vector<vector<double>>(2));

    for (int k = 0; k < K; ++k) {
        for (int sh = 0; sh < 2; ++sh) {
            for (const auto& kv : best_by_pattern[k][sh]) {
                patterns[k][sh].push_back(kv.first);        // pattern
                times[k][sh].push_back(kv.second.first);    // time (лучшее)
            }
        }
    }

    auto env = make_unique<GRBEnv>(true);
    env->set(GRB_IntParam_OutputFlag, 0);
    env->start();
    auto result = make_unique<GurobiCoveringResult>();
    result->model = make_unique<GRBModel>(*env);
    // GRBEnv env = GRBEnv(true);
    // env.start();

    // result->model = make_unique<GRBModel>(GRBModel(env));
    result->model->set(GRB_DoubleParam_TimeLimit, timelimit);

    if (owning.empty()) {
        owning = vector<int>(K, 1);
    } else {
        for (int k = 0; k < K; ++k) {
            if (owning[k] == 0){
                owning[k] = 1001;
            }
            // else {                   // итак единичка
            //     owning[k] = 1;
            // }
        }
    }

    // b[(shift,tank,k,r)]
    
    map<tuple<bool,int,int,int>, int> b;

    for (bool start_shifted : {true, false}) {
        int sh = start_shifted ? 1 : 0;
        for (int k = 0; k < K; ++k) {
            for (int r = 0; r < (int)patterns[k][sh].size(); ++r) {

                const vector<int>& pat = patterns[k][sh][r];

                for (int tank = 0; tank < tank_count; ++tank) {
                    int val = (find(pat.begin(), pat.end(), tank) != pat.end()) ? 1 : 0;
                    b[make_tuple(start_shifted, tank, k, r)] = val;
                }
            }
        }
    }

    // Переменные y[k]
    for (int k = 0; k < K; ++k) {
      result->y[k] = result->model->addVar(0.0, 1.0, 0.0, vtype, "y_"+to_string(k));
    }

    // Переменные g[k,r]
    for (bool start_shifted : {true, false}) {
        int sh = start_shifted ? 1 : 0;

        for (int k = 0; k < K; ++k) {
            int R = (int)patterns[k][sh].size();
            for (int r = 0; r < R; ++r) {
            std::string var_name = "g_" + std::to_string(start_shifted) + "_" + std::to_string(k) + "_" + std::to_string(r);
            result->g[{start_shifted, k, r}] = result->model->addVar(0.0, 1.0, 0.0, vtype, var_name);
            }
        }
    }

    // Целевая функция
    GRBLinExpr obj = 0;
    for (int k = 0; k < K; ++k) obj += owning[k] * result->y[k];
    result->model->setObjective(obj, GRB_MINIMIZE);


    // Ограничения (4.2)–(4.5)
    for (int i = 0; i < tank_count; ++i) {
    GRBLinExpr lhs = 0;

    for (bool start_shifted : {true, false}) {
        int sh = start_shifted ? 1 : 0;

        for (int k = 0; k < K; ++k) {
            int R = (int)patterns[k][sh].size();
            for (int r = 0; r < R; ++r) {
                lhs += b[{start_shifted, i, k, r}] * result->g[{start_shifted, k, r}];
            }
        }
    }

    if (is_critical[i] == 1) {
        result->model->addConstr(lhs == 1, "Reservoir_" + std::to_string(i));
    } else {
        result->model->addConstr(lhs <= 1, "Reservoir_" + std::to_string(i));
    }
    }


    // переменные y1, y2 и связь с y
    for (int k = 0; k < K; ++k) {
      result->y1[k] = result->model->addVar(0.0, 1.0, 0.0, vtype, "y1_"+to_string(k));
      result->y2[k] = result->model->addVar(0.0, 1.0, 0.0, vtype, "y2_"+to_string(k));
    }

    for (int k = 0; k < K; ++k) {

        {
            bool start_shifted = false;
            int sh = 0;
            GRBLinExpr rhs = 0;
            for (int r = 0; r < (int)patterns[k][sh].size(); ++r) {
                rhs += result->g[{start_shifted, k, r}];
            }
            result->model->addConstr(rhs <= result->y1[k], "Link_" + to_string(k) + "_y1");
        }

        {
            bool start_shifted = true;
            int sh = 1;
            GRBLinExpr rhs = 0;
            for (int r = 0; r < (int)patterns[k][sh].size(); ++r) {
                rhs += result->g[{start_shifted, k, r}];
            }
            result->model->addConstr(rhs <= result->y2[k], "Link_" + to_string(k) + "_y2");
        }
    }

    for (int k = 0; k < K; ++k) {
        result->model->addConstr(2 * result->y[k] >= result->y1[k] + result->y2[k], "Shifts_link_" + to_string(k));
    }

    if (!H_k.empty()) {
        if (load_number > 0) {
            for (int k = 0; k < K; ++k) {
                result->l[k] = result->model->addVar(0.0, 1.0, 0.0, vtype, "l_" + to_string(k));
            }
            
           
            
            for (int k = 0; k < K; ++k) {
                GRBLinExpr lhs = 0;
                for (bool start_shifted : {true, false}) {
                    int sh = start_shifted ? 1 : 0;
                    for (int r = 0; r < (int)patterns[k][sh].size(); ++r) {
                        lhs += times[k][sh][r] * result->g[{start_shifted, k, r}];
                    }
                }
                result->model->addConstr(lhs <= (H_k[k] - result->l[k] * filling_times[k]), "Shift_" + to_string(k));
            }

            GRBLinExpr loads_sum = 0;
            for (int k = 0; k < K; ++k) loads_sum += result->l[k];
            result->model->addConstr(loads_sum >= load_number, "load_number_satisfied");

            for (int k = 0; k < K; ++k) {
                result->model->addConstr(result->y[k] >= result->l[k], "load_if_used_" + to_string(k));
            }
        }
        else {
            for (int k = 0; k < K; ++k) {
                GRBLinExpr lhs = 0;
                for (bool start_shifted : {true, false}) {
                    int sh = start_shifted ? 1 : 0;
                    for (int r = 0; r < (int)patterns[k][sh].size(); ++r) {
                        lhs += times[k][sh][r] * result->g[{start_shifted, k, r}];
                    }
                }
                result->model->addConstr(lhs <= H_k[k], "Shift_" + to_string(k));
            }
        }
    }


    if (double_race_number > 0) {
        GRBLinExpr all_races_sum = 0;

        for (bool start_shifted : {true, false}) {
            int sh = start_shifted ? 1 : 0;

            for (int k = 0; k < K; ++k) {
                for (int r = 0; r < (int)patterns[k][sh].size(); ++r) {
                    all_races_sum += result->g[{start_shifted, k, r}];
                }
            }
        }

        GRBLinExpr used_trucks_sum = 0;
        for (int k = 0; k < K; ++k) used_trucks_sum += result->y[k];

        result->model->addConstr(all_races_sum >= used_trucks_sum + double_race_number, "double_races");
    }

    result->model->optimize();
    return result;
}


string replaceStations(const string& input,
                            const map<int,int>& mapping)
{
    regex r("(станци(?:я|и)\\s+)(\\d+)");
    string result;
    result.reserve(input.size());

    sregex_iterator it(input.begin(), input.end(), r);
    sregex_iterator end;
    size_t last = 0;

    for (; it != end; ++it) {
        const auto& m = *it;

        // текст до совпадения
        result.append(input, last, m.position() - last);

        int old_id = stoi(m[2].str());
        int new_id = old_id;

        auto f = mapping.find(old_id);
        if (f != mapping.end())
            new_id = f->second;

        // пишем замену
        result += m[1].str();
        result += to_string(new_id);

        last = m.position() + m.length();
    }

    // хвост
    result.append(input, last);

    return result;
}

void gurobi_results(
    GRBModel& model,
    const map<int, GRBVar>& y, 
    const map<int, GRBVar>& y1,
    const map<int, GRBVar>& y2,
    const map<tuple<bool,int,int>, GRBVar>& g,
    const map<int, GRBVar>& l,
    const map<int, GRBVar>& s,
    const vector<vector<map<vector<int>, pair<double, Filling>>>>& best_by_pattern,
    const map<pair<int,int>, int>& gl_num,                      // (станция, резервуар) -> глобальный номер
    const vector<vector<double>> trucks,
    const vector<int>& owning,
    bool print_logs
) {
    int status = model.get(GRB_IntAttr_Status);

    int K = trucks.size();
    vector<vector<vector<const Filling*>>> routes(K, vector<vector<const Filling*>>(2));
    vector<vector<vector<double>>> times(K, vector<vector<double>>(2));
    vector<vector<vector<vector<int>>>> patterns(K, vector<vector<vector<int>>>(2));

    for (int k = 0; k < K; ++k) {
        for (int sh = 0; sh < 2; ++sh) {
            for (const auto& kv : best_by_pattern[k][sh]) {
            patterns[k][sh].push_back(kv.first);
            times[k][sh].push_back(kv.second.first);
            routes[k][sh].push_back(&kv.second.second);
            }
        }
    }

    map<int, int> station_mapping = {
        {0, 30005},
        {1, 30017},
        {2, 30022},
        {3, 30026},
        {4, 30028},
        {5, 30030},
        {6, 30091},
        {7, 30099},
        {8, 30109},
        {9, 30112},
        {10, 30120},
        {11, 30134},
        {12, 30139},
        {13, 30140},
        {14, 30141},
        {15, 30146},
        {16, 30147},
        {17, 30153},
        {18, 30154},
        {19, 30156},
        {20, 30157},
        {21, 30159},
        {22, 30160},
        {23, 30162},
        {24, 30164},
        {25, 30169},
        {26, 30171},
        {27, 33138},
    };

    vector<vector<int>> res_nmbrs = {
        {2, 3},
        {3, 4},
        {3, 5},
        {1},
        {3, 4, 5},
        {1, 3},
        {1, 2, 3},
        {6},
        {3},
        {3},
        {4},
        {1, 3},
        {6},
        {4, 6},
        {1, 4},
        {1, 6},
        {1},
        {2, 5},
        {1, 3},
        {1, 2},
        {1},
        {1},
        {2, 3},
        {2, 4},
        {5},
        {1, 4},
        {2, 3},
        {3}
    };

    if (status == GRB_OPTIMAL || status == GRB_TIME_LIMIT) {
        int solCount = model.get(GRB_IntAttr_SolCount);
        if (solCount == 0) {
            cout << "Достигнут лимит времени. Допустимое решение не найдено." << endl;
            return;
        }

        double objVal = model.get(GRB_DoubleAttr_ObjVal);
        if (status == GRB_TIME_LIMIT) {
            cout << "Достигнут лимит времени.\nЛучшее найденное решение: " << objVal << endl;
        } else {
            cout << "Оптимальное значение: " << (long long)objVal << endl;
        }

        double runtime = model.get(GRB_DoubleAttr_Runtime);
        double nodeCount = model.get(GRB_DoubleAttr_NodeCount);
        cout << "Время решения: " << runtime << " секунд" << endl;
        cout << "Количество узлов в дереве поиска: " << nodeCount << "\n" << endl;

        // reverse gl_num: global_tank_id -> (station_idx, local_res_idx)
        std::map<int, std::pair<int,int>> reversed_gl;
        for (const auto& kv : gl_num) {
            reversed_gl[kv.second] = kv.first;
        }

        // Распаковка best_by_pattern в списки, чтобы r был индексом как в модели
        // routes_ptr[k][sh][r] -> Filling*
        // times[k][sh][r]      -> double
        std::vector<std::vector<std::vector<const Filling*>>> routes_ptr(
            K, std::vector<std::vector<const Filling*>>(2)
        );
        std::vector<std::vector<std::vector<double>>> times(
            K, std::vector<std::vector<double>>(2)
        );

        for (int k = 0; k < K; ++k) {
            for (int sh = 0; sh < 2; ++sh) {
                routes_ptr[k][sh].reserve(best_by_pattern[k][sh].size());
                times[k][sh].reserve(best_by_pattern[k][sh].size());

                for (const auto& kv : best_by_pattern[k][sh]) {
                    // kv.first  = pattern (vector<int>)
                    // kv.second = pair<double, Filling>
                    times[k][sh].push_back(kv.second.first);
                    routes_ptr[k][sh].push_back(&kv.second.second);
                }
            }
        }

        // Перебираем грузовики/смены/маршруты и печатаем выбранные
        if (print_logs) {
            for (int k = 0; k < K; ++k) {

                if (y.at(k).get(GRB_DoubleAttr_X) <= 0.5) continue;

                if (s.count(k) && s.at(k).get(GRB_DoubleAttr_X) > 0.5)
                    std::cout << "Бензовоз " << (k + 1) << " выполняет два рейса\n";

                if (l.count(k) && l.at(k).get(GRB_DoubleAttr_X) > 0.5)
                    std::cout << "Бензовоз " << (k + 1) << " загружен под сменщика\n";

                for (bool shift : {false, true}) {
                    int sh = shift ? 1 : 0;

                    if (!shift && y1.at(k).get(GRB_DoubleAttr_X) <= 0.5) continue;
                    if ( shift && y2.at(k).get(GRB_DoubleAttr_X) <= 0.5) continue;

                    if (!shift) std::cout << "Бензовоз " << (k + 1) << " используется, в подсмену 1, выбранные маршруты:\n";
                    else        std::cout << "Бензовоз " << (k + 1) << " используется, в подсмену 2, выбранные маршруты:\n";

                    double total_time = 0.0;

                    // Время поездки до депо (как у тебя было)
                    if (owning[k] == 1) {
                        total_time += 33.6;
                        std::cout << "  Время поездки до депо 33.6 минут\n";
                    } else {
                        total_time += 6.0;
                        std::cout << "  Время поездки до депо 6.0 минут\n";
                    }

                    for (int r = 0; r < (int)routes_ptr[k][sh].size(); ++r) {
                        if (g.count({shift, k, r}) && g.at({shift, k, r}).get(GRB_DoubleAttr_X) <= 0.5) continue;

                        const Filling& f = *routes_ptr[k][sh][r];

                        std::cout << "\n  Маршрут №" << r << "\n";

                        // Сгруппируем выгрузки по станциям:
                        // st -> list of (local_res_idx, comps_vector)
                        std::map<int, std::vector<std::pair<int, std::vector<int>>>> by_station;

                        for (const auto& rc : f.res_comps) {
                            int tank_id = rc.first;
                            const std::vector<int>& comps = rc.second;

                            auto it = reversed_gl.find(tank_id);
                            if (it == reversed_gl.end()) continue; // или можно assert/throw

                            int st = it->second.first;
                            int local_res = it->second.second;

                            by_station[st].push_back({local_res, comps});
                        }

                        for (auto& kv : by_station) {
                            int st = kv.first;
                            auto& lst = kv.second;

                            std::sort(lst.begin(), lst.end(),
                                    [](const auto& a, const auto& b) { return a.first < b.first; });

                            std::cout << "    Станция №" << station_mapping[st] << "\n";
                            std::cout << "       Заполнены резервуары:\n";

                            for (const auto& item : lst) {
                                int local_res = item.first;
                                const std::vector<int>& comps = item.second;

                                const std::vector<int>& res_map = res_nmbrs[st];

                                std::cout << "         №" << res_map[local_res] << " используя отсек(и): ";

                                for (int i = 0; i < (int)comps.size(); ++i) {
                                    if (i) std::cout << ", ";
                                    std::cout << comps[i];
                                }

                                int fuel_sum = 0;
                                for (int comp_number : comps) {
                                    fuel_sum += (int)trucks[k][comp_number];
                                }

                                std::cout << " и выгружая " << fuel_sum << " литров топлива\n";
                            }
                        }

                        double route_time = roundN(times[k][sh][r], 3);
                        std::cout << "\n  Время маршрута - " << std::fixed << std::setprecision(2)
                                << route_time << " минут\n";
                        total_time += route_time;

                        // ЛОГИ:
                        // Если у тебя в Filling есть vector<string> timelog_lines — раскомментируй и подставь имя поля.
                        //
                        std::cout << "  В том числе:\n";
                        for (const auto& entry : f.timelog) {
                            std::cout << "    " << replaceStations(entry, station_mapping) << "\n";
                        }

                        // Если лог хранится одной строкой f.timelog (string) — можно так:
                        // if (!f.timelog.empty()) std::cout << f.timelog << "\n";
                    }

                    std::cout << "\n";
                }
            }
        }
    } else {
        cout << "Оптимальное решение не найдено." << endl;
    }
}