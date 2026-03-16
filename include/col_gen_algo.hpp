#include <vector>
#include <map>
#include "prep_functions.hpp"

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
);

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
);