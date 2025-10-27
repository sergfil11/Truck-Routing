#include <iostream>
#include <algorithm>
#include <memory>
#include <prep_functions.hpp>
#include "gurobi.hpp"
using namespace std;

// struct GurobiCoveringResult {
//     unique_ptr<GRBModel> model;
//     vector<GRBVar> y;
//     map<pair<int,int>, GRBVar> g;
// };

unique_ptr<GurobiCoveringResult> gurobi_covering(
    const map<int, vector<vector<int>>>& filling_on_route,  // маршруты
    const map<pair<int,int>, double>& sigma,                // время на маршрут
    const vector<map<string, double>>& reservoirs,          // {min,max}
    int tank_count,
    int H,
    int K,
    int timelimit,
    const map<pair<int,int>, int>& gl_num,
    const vector<double>& H_k,
    vector<int> owning,
    const vector<int>& is_critical
) {
    auto env = make_unique<GRBEnv>(true);
    env->set(GRB_IntParam_OutputFlag, 0);
    env->start();
    auto result = std::make_unique<GurobiCoveringResult>();
    result->model = std::make_unique<GRBModel>(*env);
    // GRBEnv env = GRBEnv(true);
    // env.set(GRB_IntParam_OutputFlag, 0);   // отключаем вывод
    // env.start();

    // result->model = make_unique<GRBModel>(GRBModel(env));
    result->model->set(GRB_DoubleParam_TimeLimit, timelimit);

    if (owning.empty()) {
        owning = vector<int>(K, 1);
    } else {
        for (int k = 0; k < K; ++k) {
            owning[k] = owning[k]*1000 + 1;
        }
    }

    // b[(tank,k,r)]
    map<tuple<int,int,int>, int> b;
    for (int k = 0; k < K; ++k) {
        if (filling_on_route.count(k) == 0) continue;
        const auto& routes = filling_on_route.at(k);
        for (int r = 0; r < (int)routes.size(); ++r) {
            for (int tank = 0; tank < tank_count; ++tank) {
                b[make_tuple(tank,k,r)] = (routes[r][tank] == 1 ? 1 : 0);
            }
        }
    }

    // Переменные y[k]
    for (int k = 0; k < K; ++k) {
      result->y[k] = result->model->addVar(0.0, 1.0, 0.0, GRB_BINARY, "y_"+std::to_string(k));
    }

    // Переменные g[k,r]
    for (const auto& [k, routes] : filling_on_route) {
        for (int r = 0; r < (int)routes.size(); ++r) {
            string var_name = "g_" + to_string(k) + "_" + to_string(r);
            result->g[{k,r}] = result->model->addVar(0.0, 1.0, 0.0, GRB_BINARY, var_name);
        }
    }

    // Целевая функция
    GRBLinExpr obj = 0;
    for (int k = 0; k < K; ++k) obj += owning[k] * result->y[k];
    result->model->setObjective(obj, GRB_MINIMIZE);

    // Ограничения (4.2)–(4.5)
    for (int i = 0; i < tank_count; ++i) {
        if (is_critical[i] == 1.0) {
            GRBLinExpr lhs = 0;
            for (int k = 0; k < K; ++k) {
                if (filling_on_route.count(k) == 0) continue;
                const auto& routes = filling_on_route.at(k);
                for (int r = 0; r < (int)routes.size(); ++r) lhs += b[{i,k,r}] * result->g[{k,r}];
            }
            result->model->addConstr(lhs == 1, "Reservoir_" + to_string(i));
        } else {
            GRBLinExpr lhs = 0;
            for (int k = 0; k < K; ++k) {
                if (filling_on_route.count(k) == 0) continue;
                const auto& routes = filling_on_route.at(k);
                for (int r = 0; r < (int)routes.size(); ++r) lhs += b[{i,k,r}] * result->g[{k,r}];
            }
            result->model->addConstr(lhs <= 1, "Reservoir_" + to_string(i));
        }
    }

    for (int k = 0; k < K; ++k) {
        if (filling_on_route.count(k) == 0) continue;
        const auto& routes = filling_on_route.at(k);
        for (int r = 0; r < (int)routes.size(); ++r) {
            result->model->addConstr(result->g[{k,r}] <= result->y[k], "Link_" + to_string(k) + "_" + to_string(r));
        }
    }

    if (!H_k.empty()) {
        for (int k = 0; k < K; ++k) {
            if (filling_on_route.count(k) == 0) continue;
            GRBLinExpr lhs = 0;
            const auto& routes = filling_on_route.at(k);
            for (int r = 0; r < (int)routes.size(); ++r) lhs += sigma.at({k,r}) * result->g[{k,r}];
            result->model->addConstr(lhs <= H_k[k], "Shift_" + to_string(k));
        }
    }

    result->model->optimize();
    return result;
}


void gurobi_results(
    GRBModel& model,
    const map<int, GRBVar>& y,
    const map<pair<int,int>, GRBVar>& g,
    const map<int, vector<vector<int>>>& filling_on_route,   // грузовику -> список маршрутов -> список заполнений
    const map<pair<int,int>, int>& gl_num,                   // (станция, резервуар) -> глобальный номер
    const map<pair<int,int>, vector<string>>& log,           // (k,r) -> лог действий
    const map<pair<int,int>, double>& sigma                     // (k,r) -> время маршрута
) {
    int status = model.get(GRB_IntAttr_Status);

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
        for (const auto& [k, routes] : filling_on_route) {
            if (y.at(k).get(GRB_DoubleAttr_X) > 0.5) {
                cout << "Бензовоз " << (k+1) << " используется, выбранные маршруты:" << endl;
                double total_time = 0.0;

                for (int r = 0; r < (int)routes.size(); ++r) {
                    if (g.at({k,r}).get(GRB_DoubleAttr_X) > 0.5) {
                        cout << "\n  Маршрут №" << r << endl;

                        // все резервуары на маршруте
                        vector<int> temp_res;
                        for (int res_idx = 0; res_idx < (int)routes[r].size(); ++res_idx) {
                            if (routes[r][res_idx] != 0) {
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

                        for (int s : st_in_route) {
                            vector<int> res_in_local_curr;
                            for (auto& elem : res_in_local) {
                                if (elem.first == s) {
                                    res_in_local_curr.push_back(elem.second);
                                }
                            }
                            cout << "    Станция №" << s << ", заполненные резервуары №: ";
                            for (size_t idx = 0; idx < res_in_local_curr.size(); ++idx) {
                                if (idx > 0) cout << ", ";
                                cout << res_in_local_curr[idx];
                            }
                            cout << endl;
                        }

                        double route_time = roundN(sigma.at({k,r}), 3);
                        cout << "  Время маршрута - " << route_time << " минут, в том числе:" << endl;
                        total_time += route_time;

                        for (const auto& entry : log.at({k,r})) {
                            cout << "    " << entry << endl;
                        }
                    }
                }
                cout << "Суммарное время: " << total_time << " минут" << endl;
            }
            cout << endl;
        }
    } else {
        cout << "Оптимальное решение не найдено." << endl;
    }
}