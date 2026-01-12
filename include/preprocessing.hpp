#pragma once

#include <vector>
#include <map>
#include <tuple>
#include <string>
#include <set>


  struct Result {
    std::map<int, std::vector<std::vector<string>>> filling_on_route;
    std::map<std::pair<int,int>, double> sigma;
    std::vector<std::map<std::string,double>> reservoirs;
    int tank_count;
    std::map<std::pair<int,int>, int> gl_num;
    std::map<std::pair<int,int>, std::vector<std::string>> log;
    std::vector<double> H_k;
    std::vector<double> filling_times;
  };

using namespace std;

// tuple< 
//     map<int, vector<vector<int>>>,
//     map<pair<int, int>, double>,
//     vector<map<string, double>>, 
//     int,
//     map<pair<int, int>, int>, 
//     map<pair<int, int>, vector<string>>,
//     vector<double>,
//     vector<double>
//     >
Result
gurobi_preprocessing(
    int N,
    int H,
    int K,
    const vector<vector<double>>& time_matrix,
    const vector<map<string, double>>& depot_times,
    const vector<map<string, vector<double>>>& stations,
    const vector<vector<double>>& trucks,
    int R1,
    int R2,
    vector<vector<int>>& access,
    vector<bool>& double_piped,
    double daily_coefficient,
    vector<double>& docs_fill,
    vector<double>& H_k,
    vector<bool>& loading_prepared,
    vector<vector<vector<double>>>& consumption_percent,
    const map<string, vector<string>>& reservoir_to_product = {},
    const map<string, set<vector<string>>>& truck_to_variants = {},
    const vector<vector<double>>& consumption = {},
    const vector<double>& starting_time = {},
    const vector<int>& owning = {}
  );