#include <prep_functions.hpp>
#include <iostream>
#include <algorithm>
#include <memory>
#include <iomanip>
#include <regex>
#include "gurobi.hpp"
using namespace std;



vector<vector<map<vector<int>, pair<double, Filling>>>> initial_routes (
  const vector<vector<map<vector<int>, pair<double, Filling>>>>& best_by_pattern,
  const vector<map<string, double>>& reservoirs,
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
  int step
) {

  // структура для сортировки
  struct RouteEntry {     
    vector<int> pattern;
    double time;
    Filling filling;
  };

  vector<vector<vector<RouteEntry>>> sorted_routes(K, vector<vector<RouteEntry>>(2));   // отсортированные маршруты
  vector<vector<int>> used(K, vector<int>(2, 0));   // число добавленных маршрутов для каждой смены и бензовоза
  
  for (int k = 0; k < K; k++) {
    for (int sh = 0; sh < 2; sh++){
        vector<RouteEntry> routes;

        for (const auto& [pattern, data] : best_by_pattern[k][sh]) {
            routes.push_back({pattern, data.first, data.second});
        }

        sort(routes.begin(), routes.end(),
            [](const RouteEntry& a, const RouteEntry& b) {
                return a.time < b.time;
            });

        sorted_routes[k][sh] = routes;
    }
  }
  
  using Route = map<vector<int>, pair<double, Filling>>;
  using Routes = vector<vector<Route>>;
  
  Routes selected_routes (K, vector<Route>(2));
  bool solved = false;
  
  while (!solved) {
    for (int k = 0; k < K; k++) {
      for (int s = 0; s < 2; s++) {
          int start = used[k][s];
          int end = min(start + step, (int)sorted_routes[k][s].size());  // +step маршрутов если они есть 

          for (int i = start; i < end; i++) {
            const auto& r = sorted_routes[k][s][i];

            selected_routes[k][s].try_emplace(r.pattern, r.time, r.filling);
          }
          used[k][s] = end;
      }
    }

    bool is_relaxation = true;
    auto res = gurobi_covering( 
      selected_routes,
      reservoirs,
      tank_count,
      720,      // H
      K,
      900,      // timelimit
      gl_num,
      H_k,
      owning,
      is_critical,
      load_number,
      double_race_number,
      filling_times,
      is_relaxation
    );

    GRBModel& g_model = *res->model;
    if (g_model.get(GRB_IntAttr_Status) == GRB_OPTIMAL) {
      solved = true;
    }

  }

  int total = 0;
  for (int k = 0; k < K; k++) {
      cout << "Бензовоз " << k << ": ";
      for (int s = 0; s < 2; s++) {
          int cnt = selected_routes[k][s].size();
          total += cnt;

          cout << cnt;
          if (s == 0) cout << " | ";
      }
      cout << "\n";
  }
  cout << "Всего: " << total << "\n\n";

  return selected_routes;
}

vector<vector<map<vector<int>, pair<double, Filling>>>> column_generation( 
    const vector<vector<map<vector<int>, pair<double, Filling>>>>& best_by_pattern, // маршруты
    vector<vector<map<vector<int>, pair<double, Filling>>>>& initial_routes,        // начальные маршруты
    const vector<map<string, double>>& reservoirs,                                  // {min,max}
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

  bool finished = false;
  while (!finished) {
    auto init_sol = gurobi_covering( 
          initial_routes,
          reservoirs,
          tank_count,
          720,      // H
          K,
          900,      // timelimit
          gl_num,
          H_k,
          owning,
          is_critical,
          load_number,
          double_race_number,
          filling_times,
          true
    );

    vector<double> pi(tank_count);
    for (int i = 0; i < tank_count; i++) {
        GRBConstr constr = (*init_sol->model).getConstrByName("Reservoir_" + to_string(i));
        pi[i] = constr.get(GRB_DoubleAttr_Pi);
    }

    using Route = map<vector<int>, pair<double, Filling>>;
    using Routes = vector<vector<Route>>;

    
    double best_red_c = 0.0;
    set<pair<int, int>> best_k_sh = {};

    for (int k = 0; k < K; ++k) {
        for (int sh = 0; sh < 2; ++sh) {
            for (const auto& [pattern, fill] : best_by_pattern[k][sh]) {

                double red_c = owning[k];
                for (int tank : pattern) {
                    red_c -= pi[tank];
                }
                // TODO: добавить owning[k]

                // если этого решения ещё нет в выбранных маршрутах
                if (red_c < best_red_c && initial_routes[k][sh].count(pattern) == 0) {
                    best_red_c = red_c;
                    best_k_sh.clear();
                    best_k_sh.insert({k,sh});
                }
                
                if (abs(red_c - best_red_c) < 1e-9 && initial_routes[k][sh].count(pattern) == 0) {
                    best_k_sh.insert(pair(k, sh));
                }
            }
        }
    }

    if (best_red_c >= 0 || best_k_sh.empty()) {
        break;
    }

    for (const auto& [best_k, best_sh] : best_k_sh) {
      for (const auto& [pattern, fill] : best_by_pattern[best_k][best_sh]) {

          double red_c = owning[best_k];
          for (int tank : pattern) {
              red_c -= pi[tank];
          }

          if (abs(red_c - best_red_c) < 1e-9 && initial_routes[best_k][best_sh].count(pattern) == 0) {
            initial_routes[best_k][best_sh].try_emplace(pattern, fill.first, fill.second);
          }
      }
    }

  }

  int total = 0;
  for (int k = 0; k < K; k++) {
      cout << "Бензовоз " << k << ": ";
      for (int s = 0; s < 2; s++) {
          int cnt = initial_routes[k][s].size();
          total += cnt;

          cout << cnt;
          if (s == 0) cout << " | ";
      }
      cout << "\n";
  }
  cout << "Всего: " << total << "\n\n";

return initial_routes;
}
