#pragma once

#include <vector>
#include <map>
#include <tuple>
#include <string>
#include <set>

using namespace std;

tuple< 
    map<int, vector<vector<int>>>,
    map<pair<int, int>, double>,
    vector<map<string, double>>, 
    int,
    map<pair<int, int>, int>, 
    map<pair<int, int>, vector<string>>,
    vector<double>,
    vector<double>
    >
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
    vector<int>& loading_prepared,
    const map<string, vector<string>>& reservoir_to_product = {},
    const map<string, set<vector<string>>>& truck_to_variants = {}
  );