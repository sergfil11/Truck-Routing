#pragma once
#include <memory>       // unique_ptr, make_unique
#include <map>          // map
#include <vector>       // vector
#include <string>       // string
#include <tuple>        // tuple
#include "gurobi_c++.h" // GRBModel, GRBVar и др.

using namespace std;

struct GurobiCoveringResult {
    unique_ptr<GRBModel> model;
    map<int, GRBVar> y;
    map<pair<int,int>, GRBVar> g;
    map<int, GRBVar> l;
    map<int, GRBVar> s;
};

void gurobi_results(
    GRBModel& model,
    const map<int, GRBVar>& y,
    const map<pair<int,int>, GRBVar>& g,
    const map<int, GRBVar>& l,                               // загрузка под сменщика
    const map<int, GRBVar>& s,                               // два рейса
    const map<int, vector<vector<int>>>& filling_on_route,   // грузовику -> список маршрутов -> список заполнений
    const map<pair<int,int>, int>& gl_num,                   // (станция, резервуар) -> глобальный номер
    const map<pair<int,int>, vector<string>>& log,           // (k,r) -> лог действий
    const map<pair<int,int>, double>& sigma,                 // (k,r) -> время маршрута
    bool print_logs
);

unique_ptr<GurobiCoveringResult> gurobi_covering(
    const map<int, vector<vector<int>>>& filling_on_route,  // маршруты
    const map<pair<int,int>, double>& sigma,                // время на маршрут
    const vector<map<string, double>>& reservoirs,          // {min,max}
    int tank_count,
    int H = 720,
    int K = 20,
    int timelimit = 900,
    const map<pair<int,int>, int>& gl_num = {},
    const vector<double>& H_k = {},
    vector<int> owning = {},
    const vector<int>& is_critical = {},
    int load_number = 0,
    int double_race_number = 0,
     const vector<double>& filling_times = {}
);