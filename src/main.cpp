#include "prep_functions.hpp"
#include "preprocessing.hpp"
#include "gurobi.hpp"
#include "parsing.hpp"
#include <iostream>
#include <assert.h>
#include <locale>
#include "gurobi_c++.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>


using namespace std;



string create_filename(bool double_piped, bool loading_prepared, double scale) {
    stringstream filename;
    filename << "configs/"
            //  << "" << (double_piped ? "double_piped" : "") << "_"
            //  << "" << (loading_prepared ? "loading_prepared" : "") << "_"
            //  << "beta=" << fixed << setprecision(2) << scale
             << "gen_data_test.txt";
    return filename.str();
    }


int main() {
    setlocale(LC_ALL, ".utf8");
    setlocale(LC_NUMERIC, "C");
    // vector<bool> l_prep = {false, true};
    // vector<bool> double_p = {false, true};
    // vector<double> betas = {1.0, 2.0, 3.0, 4.0, 5.0, 10.0};

    
    // for (auto beta : betas) {
    //     for (auto l_prep_c : l_prep) {
    //         for (auto double_p_c : double_p) {
    string filename = create_filename(true, true, 1.0);
    ofstream file(filename);


    auto data = load_real_data("C:\\Users\\Sergey\\Desktop\\Diplom\\main\\project_cpp\\instances\\generated_data");

    cout << "=== Результат load_real_data ===\n";
    print_input(data);


    int N = data.N;
    int H = data.H;
    int K = data.K;
    vector<vector<double>> time_matrix = data.time_matrix;
    vector<map<string, double>> depot_times = data.depot_times;
    vector<map<string, vector<double>>> stations = data.stations;
    vector<vector<double>> trucks = data.trucks;
    vector<vector<int>> access = data.access;
    vector<bool> double_piped = data.dual_piped;
    float daily_coefficient = data.daily_coefficient;
    vector<double> docs_fill = data.docs_fill;
    vector<double> H_k = data.H_k;
    vector<bool> loading_prepared = data.loading_prepared;
    map<string, vector<string>> reservoir_to_product = data.reservoir_to_product;
    map<string, set<vector<string>>> truck_to_variants = data.truck_to_variants;
    vector<int> owning = data.owning;
    vector<int> is_critical = data.is_critical;
    vector<vector<vector<double>>> consumption_percent = data.consumption_percent;
    vector<double> starting_time = data.starting_time;
    vector<vector<double>> consumption = data.consumption;

    auto old_buf = cout.rdbuf(file.rdbuf());
    
    cout << "Раскидали данные внутри main\n";

    int R1 = 3;
    int R2 = 5;
    daily_coefficient = 1.5;
    
    double_piped = vector<bool>(K, false);
    // H_k = vector<double>(K, 720.0);
    loading_prepared = vector<bool>(K, false);
    
    // double docs_scale_coef = beta;
    // for (int i = 0; i < N; i++) {
    //     docs_fill[i] /= docs_scale_coef;
    // }

    // cout << "double_piped = " << double_p_c << endl;

    // for (int i = 0; i < N; i++) {
    //     cout << "docs_fill[" << i << "] = " << docs_fill[i] << endl;
    // }
    // if (l_prep_c == true) {
    //     loading_prepared[2] = true;
    //     loading_prepared[3] = true;
    //     loading_prepared[7] = true;
    //     loading_prepared[12] = true;
    //     loading_prepared[27] = true;
    // }
    // for (int i = 0; i < K; i++) {
    //     cout << "loading_prepared[" << i << "] = " << loading_prepared[i] << endl;
    // }

// tuple<
//     unique_ptr<map<int, vector<vector<int>>>>,
//     unique_ptr<map<pair<int,int>, double>>,
//     unique_ptr<vector<map<string,double>>>,
//     int,
//     unique_ptr<map<pair<int,int>, int>>,
//     unique_ptr<map<pair<int,int>, vector<string>>>,
//     unique_ptr<vector<double>>,
//     unique_ptr<vector<double>>
//     > result
    auto t4 = chrono::system_clock::now();
    auto [filling_on_route, sigma, reservoirs, tank_count, gl_num, log, H_k_out, filling_times] = gurobi_preprocessing(
        N, H, K,
        time_matrix,
        depot_times,
        stations,
        trucks,
        R1, R2,
        access,
        double_piped,
        daily_coefficient,
        docs_fill,
        H_k,
        loading_prepared,
        consumption_percent,
        reservoir_to_product,
        truck_to_variants,
        consumption,
        starting_time,
        owning
    );

    
    cout << "=== Gurobi_preprocessing закончилось за " << roundN(chrono::duration<double>(chrono::system_clock::now() - t4).count(), 3) << " сек. ===\n";

    

    int load_number = 0;
    // for (int j = 0; j < 10; j++) {
        int double_race_number = 0;     // число бензовозов, делающих по 2 рейса

        // for (int i = 0; i < 20; i++) {
        //     int load_number = i;     // число бензовозов, после рейса заполняющихся под сменщика

        cout << "=== Начало работы Gurobi с параметрами load_number = " << load_number << ", double_race_number = " << double_race_number << " ===\n";
        auto t3 = chrono::system_clock::now();
        auto res = gurobi_covering(
            filling_on_route,
            sigma,
            reservoirs,
            tank_count,
            720,      // H
            K,
            900,      // timelimit
            gl_num,
            H_k_out,
            owning,
            is_critical,
            load_number,
            double_race_number,
            filling_times
        );

        auto t5 = chrono::system_clock::now();
        cout << "Время работы Gurobi: " << roundN(chrono::duration<double>(t5 - t3).count(), 3) << " сек." << endl;
 
        bool print_logs = true;

        gurobi_results(*res->model, res->y, res->g, res->l, res->s, filling_on_route, gl_num, log, sigma, trucks, owning, print_logs);      
    // }
    cout.rdbuf(old_buf);
    // gurobi_results(*res->model, res->y, res->g, filling_on_route, gl_num, log, sigma);

    // GRBModel& model = ;
    // auto& y = ;
    // auto& g = ;
    // }}}


    return 0;
};