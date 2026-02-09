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
    const map<pair<bool,int>, vector<vector<string>>>& filling_on_route, // маршруты
    const map<tuple<bool,int,int>, double>& sigma,                      // время на маршрут
    const vector<map<string, double>>& reservoirs,                      // {min,max}
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
    const vector<double>& filling_times
) {
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

    // for (int k = 0; k < K; ++k) {
    //    cout << owning[k] << " ";
    // }
    
    // b[(shift,tank,k,r)]
    
    map<tuple<bool,int,int,int>, int> b;
    for (bool start_shifted : {true, false}) {
        for (int k = 0; k < K; ++k) {
            if (filling_on_route.count(pair(start_shifted, k)) == 0) continue;
            const auto& routes = filling_on_route.at(pair(start_shifted, k));
            
            // for (int r = 0; r < (int)routes.size(); ++r) {
            //     if (routes[r].size() != tank_count){
            //         cout << "OH NO RETARD ALERT CLASS" << endl;
            //     }
            // }
            
            for (int r = 0; r < (int)routes.size(); ++r) {
                for (int tank = 0; tank < tank_count; ++tank) {
                    b[make_tuple(start_shifted,tank,k,r)] = (!routes[r][tank].empty() ? 1 : 0);
                }
            }
        }
    }

    // Переменные y[k]
    for (int k = 0; k < K; ++k) {
      result->y[k] = result->model->addVar(0.0, 1.0, 0.0, GRB_BINARY, "y_"+to_string(k));
    }

    // Переменные g[k,r]
    // for (bool start_shifted : {true, false}) {
        for (const auto& [shift_and_k, routes] : filling_on_route) {
            bool shift = shift_and_k.first;
            int k = shift_and_k.second;
            for (int r = 0; r < (int)routes.size(); ++r) {
                string var_name = "g_" + to_string(shift) + "_" + to_string(k) + "_" + to_string(r);
                result->g[{shift,k,r}] = result->model->addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
            }
        }
    // }




    // Целевая функция
    GRBLinExpr obj = 0;
    for (int k = 0; k < K; ++k) obj += owning[k] * result->y[k];
    result->model->setObjective(obj, GRB_MINIMIZE);


    // Ограничения (4.2)–(4.5)
    for (int i = 0; i < tank_count; ++i) {
        GRBLinExpr lhs = 0;
        for (bool start_shifted : {true, false}) {
            for (int k = 0; k < K; ++k) {
                auto it = filling_on_route.find({start_shifted, k});
                if (it == filling_on_route.end()) continue;

                const auto& routes = it->second;
                for (int r = 0; r < (int)routes.size(); ++r) {
                    lhs += b[{start_shifted, i, k, r}] * result->g[{start_shifted, k, r}];
                }
            }
        }

        if (is_critical[i] == 1.0) {
            result->model->addConstr(lhs == 1, "Reservoir_" + to_string(i));
        } else {
            result->model->addConstr(lhs <= 1, "Reservoir_" + to_string(i));
        }
    }
    

    // // связь b и g
    // for (bool start_shifted : {true, false}) {
    //     for (int i = 0; i < tank_count; ++i) {
    //         if (is_critical[i] == 1.0) {
    //             GRBLinExpr lhs = 0;
    //             for (int k = 0; k < K; ++k) {
    //                 if (filling_on_route.count(pair(start_shifted, k)) == 0) continue;
    //                 const auto& routes = filling_on_route.at(pair(start_shifted, k));
    //                 for (int r = 0; r < (int)routes.size(); ++r) lhs += b[{start_shifted,i,k,r}] * result->g[{start_shifted,k,r}];
    //             }
    //             result->model->addConstr(lhs == 1, "Reservoir_" + to_string(i));
    //         } else {
    //             GRBLinExpr lhs = 0;
    //             for (int k = 0; k < K; ++k) {
    //                 if (filling_on_route.count(pair(start_shifted, k)) == 0) continue;
    //                 const auto& routes = filling_on_route.at(pair(start_shifted, k));
    //                 for (int r = 0; r < (int)routes.size(); ++r) lhs += b[{start_shifted,i,k,r}] * result->g[{start_shifted,k,r}];
    //             }
    //             result->model->addConstr(lhs <= 1, "Reservoir_" + to_string(i));
    //         }
    //     }
    // }

    // переменные y1, y2 и связь с y
    for (int k = 0; k < K; ++k) {
      result->y1[k] = result->model->addVar(0.0, 1.0, 0.0, GRB_BINARY, "y1_"+to_string(k));
      result->y2[k] = result->model->addVar(0.0, 1.0, 0.0, GRB_BINARY, "y2_"+to_string(k));
    }

    for (bool start_shifted : {true, false}) {
        for (int k = 0; k < K; ++k) {
            if (filling_on_route.count(pair(start_shifted, k)) == 0) continue;
            const auto& routes = filling_on_route.at(pair(start_shifted, k));

            if (start_shifted == true) {
                GRBLinExpr rhs = 0;
                for (int r = 0; r < (int)routes.size(); ++r) {
                    rhs += result->g[{start_shifted,k,r}];
                }
                result->model->addConstr(rhs <= result->y2[k], "Link_" + to_string(k) + "_y2");
            }
            else {
                GRBLinExpr rhs = 0;
                for (int r = 0; r < (int)routes.size(); ++r) {
                    rhs += result->g[{start_shifted,k,r}];
                }
                result->model->addConstr(rhs <= result->y1[k], "Link_" + to_string(k) + "_y1");
            }
        }
    }

    for (int k = 0; k < K; ++k) {
        result->model->addConstr(2 * result->y[k] >= result->y1[k] + result->y2[k], "Shifts_link_" + to_string(k));
    }
 

    if (!H_k.empty()) {
        if (load_number > 0) {                  // если некоторое число бензовозов хотим загрузить под сменщика
             // создаём переменные
            for (int k = 0; k < K; ++k) {      
              result->l[k] = result->model->addVar(0.0, 1.0, 0.0, GRB_BINARY, "l_"+to_string(k));
            }
            // вычитаем время загрузки под сменщика
            for (bool start_shifted : {true, false}) {
                for (int k = 0; k < K; ++k) {       
                    if (filling_on_route.count(pair(start_shifted, k)) == 0) continue;
                    GRBLinExpr lhs = 0;
                    const auto& routes = filling_on_route.at(pair(start_shifted, k));
                    for (int r = 0; r < (int)routes.size(); ++r) lhs += sigma.at({start_shifted,k,r}) * result->g[{start_shifted,k,r}];
                    result->model->addConstr(lhs <= (H_k[k] - result->l[k] * filling_times[k]), "Shift_" + to_string(k));
                }
            }
            // проверяем, что загрузок необходимое число
            GRBLinExpr lhs = 0;
            for (int k = 0; k < K; ++k) lhs += result->l[k];
            result->model->addConstr(lhs >= load_number, "load_number satisfied");
            // загружаем под сменщика только используемые бензовозы
            for (int k = 0; k < K; ++k) {
              result->model->addConstr(result->y[k] >= result->l[k], "load_if_used" + to_string(k));
            }
        }
        else {
            for (int k = 0; k < K; ++k) {
                GRBLinExpr lhs = 0;
                for (bool start_shifted : {true, false}) {
                    if (filling_on_route.count(pair(start_shifted, k)) == 0) continue;
                    const auto& routes = filling_on_route.at(pair(start_shifted, k));
                    for (int r = 0; r < (int)routes.size(); ++r) lhs += sigma.at({start_shifted,k,r}) * result->g[{start_shifted,k,r}];
                }
                result->model->addConstr(lhs <= H_k[k], "Shift_" + to_string(k));
            }
        }
    }



    if (double_race_number > 0) {
        GRBLinExpr all_races_sum = 0;
        for (bool start_shifted : {true, false}) {
            for (int k = 0; k < K; ++k) {
                if (filling_on_route.count(pair(start_shifted, k)) == 0) continue;
                for (int r = 0; r < (int)filling_on_route.at(pair(start_shifted, k)).size(); ++r) {
                    all_races_sum += result->g[{start_shifted,k,r}];
                }
            }
        }

        GRBLinExpr used_trucks_sum = 0;
        for (int k = 0; k < K; ++k) {
            used_trucks_sum += result->y[k];
        }
        
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
    const map<pair<bool,int>, vector<vector<string>>>& filling_on_route,   // грузовику -> список маршрутов -> список заполнений
    const map<pair<int,int>, int>& gl_num,                      // (станция, резервуар) -> глобальный номер
    const map<tuple<bool,int,int>, vector<string>>& log,              // (k,r) -> лог действий
    const map<tuple<bool,int,int>, double>& sigma,                    // (k,r) -> время маршрута
    const vector<vector<double>> trucks,
    const vector<int>& owning,
    bool print_logs
) {
    int status = model.get(GRB_IntAttr_Status);

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

        // reverse gl_num
        map<int, pair<int,int>> reversed_gl;
        for (const auto& [key, val] : gl_num) {
            reversed_gl[val] = key;
        }

        

        // перебираем грузовики
        if (print_logs == true) {
            // for (bool start_shifted : {true, false}) {
            for (const auto& [shift_and_k, routes] : filling_on_route) {
                bool shift = shift_and_k.first;
                int k = shift_and_k.second;
                if (y.at(k).get(GRB_DoubleAttr_X) > 0.5) {
                    if (s.count(k) && s.at(k).get(GRB_DoubleAttr_X) > 0.5) cout << "Бензовоз " << (k+1) << " выполняет два рейса\n";
                    if (l.count(k) && l.at(k).get(GRB_DoubleAttr_X) > 0.5) cout << "Бензовоз " << (k+1) << " загружен под сменщика\n";
                    if (shift == 0 and y1.at(k).get(GRB_DoubleAttr_X) > 0.5) cout << "Бензовоз " << (k+1) << " используется, в подсмену 1, выбранные маршруты:" << endl;
                    if (shift == 1 and y2.at(k).get(GRB_DoubleAttr_X) > 0.5) cout << "Бензовоз " << (k+1) << " используется, в подсмену 2, выбранные маршруты:" << endl;
                    double total_time = 0.0;
                    
                    if (shift == 0 and y1.at(k).get(GRB_DoubleAttr_X) > 0.5) {
                         if (owning[k] == 1) {
                            total_time += 33.6;
                            cout << "  Время поездки до депо 33.6 минут" << endl;
                        } else {
                            total_time += 6.0;
                            cout << "  Время поездки до депо 6.0 минут" << endl;
                        }
                    }

                    if (shift == 1 and y2.at(k).get(GRB_DoubleAttr_X) > 0.5) {
                         if (owning[k] == 1) {
                            total_time += 33.6;
                            cout << "  Время поездки до депо 33.6 минут" << endl;
                        } else {
                            total_time += 6.0;
                            cout << "  Время поездки до депо 6.0 минут" << endl;
                        }
                    }

                    for (int r = 0; r < (int)routes.size(); ++r) {
                        if (g.at({shift,k,r}).get(GRB_DoubleAttr_X) > 0.5) {
                            cout << "\n  Маршрут №" << r << endl; 
                            
                            vector<string> comps_distr = {}; 
                            for (int res_idx = 0; res_idx < (int)routes[r].size(); ++res_idx) {
                                if (!routes[r][res_idx].empty()) {
                                    // cout << "    Выливаем отсеки с номерами: " << routes[r][res_idx] << endl;
                                    comps_distr.push_back(routes[r][res_idx]);
                                }
                            }
                            
                            // все резервуары на маршруте
                            vector<int> temp_res;
                            for (int res_idx = 0; res_idx < (int)routes[r].size(); ++res_idx) {
                                 if (!routes[r][res_idx].empty()) {
                                    temp_res.push_back(res_idx);
                                }
                            }

                            // станции в маршруте
                            vector<int> st_in_route;
                            vector<pair<int,int>> res_in_local;
                            for (int elem : temp_res) {
                                auto key = reversed_gl[elem]; // (station, reservoir)
                                st_in_route.push_back(key.first);
                                res_in_local.push_back(key);
                            }

                            // сортировка и уникализация станций
                            sort(st_in_route.begin(), st_in_route.end());
                            st_in_route.erase(unique(st_in_route.begin(), st_in_route.end()), st_in_route.end());

                            // сортировка резервуаров
                            sort(res_in_local.begin(), res_in_local.end());

                            int cmps_idx = 0;
                            for (int s : st_in_route) {
                                vector<int> res_in_local_curr;
                                for (auto& elem : res_in_local) {
                                    if (elem.first == s) {
                                        res_in_local_curr.push_back(elem.second);
                                    }
                                }
                                cout << "    Станция №" << station_mapping[s] << endl;
                                cout << "       Заполнены резервуары:" << endl;
                                for (size_t idx = 0; idx < res_in_local_curr.size(); ++idx) {
                                    vector<int> res_map = res_nmbrs[s];
                                    cout << "         №" << res_map[res_in_local_curr[idx]] << " используя отсек(и): ";
                                    bool first = true;
                                    for (char ch : comps_distr[cmps_idx]) {
                                        if (!first) {
                                            cout << ", ";
                                        }
                                        cout << ch;
                                        first = false;
                                    }
                                    int fuel_sum = 0;
                                    for (char ch : comps_distr[cmps_idx]) {
                                        if (ch >= '0' && ch <= '9') {
                                            fuel_sum += trucks[k][ch - '0'];
                                        }
                                    }
                                    cout << " и выгружая " << fuel_sum << " литров топлива";
                                    cmps_idx += 1;
                                    cout << endl;
                                }
                            }

                            double route_time = roundN(sigma.at({shift,k,r}), 3);
                            cout << endl;
                            cout << "  Время маршрута - " << fixed << setprecision(2) << route_time << " минут, в том числе:" << endl;
                            total_time += route_time;

                            for (const auto& entry : log.at({shift,k,r})) {
                                cout << "    " << replaceStations(entry, station_mapping) << endl;
                            }
                        }
                    }
                    // cout << "  Суммарное время: " << fixed << setprecision(2) << total_time << " минут" << endl;
                    cout << endl;
                }
                
            }
        }
    } else {
        cout << "Оптимальное решение не найдено." << endl;
    }
}