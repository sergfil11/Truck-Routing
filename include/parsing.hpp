#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace std;

struct RealData {
    int N;
    int H;
    int K;
    vector<vector<double>> time_matrix;
    vector<map<string, double>> depot_times;
    vector<map<string, vector<double>>> stations;
    vector<vector<double>> trucks;
    vector<vector<int>> access;
    vector<bool> dual_piped;
    double daily_coefficient;
    vector<double> docs_fill;
    vector<double> H_k;
    vector<int> loading_prepared;
    vector<int> owning;
    map<string, vector<string>> reservoir_to_product;
    map<string, set<vector<string>>> truck_to_variants;
    vector<int> is_critical;
};


RealData load_real_data(const string& name);

void print_input(const RealData& d);