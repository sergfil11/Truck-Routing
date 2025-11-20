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
    


using namespace std;

int main() {
    setlocale(LC_ALL, ".utf8");
    


    auto data = load_real_data("C:\\Users\\Sergey\\Desktop\\Diplom\\main\\project_cpp\\instances\\16_depot");

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


    int R1 = 3;
    int R2 = 12;
    double_piped = vector<bool>(K, false);
    H_k = vector<double>(K, 720);
    loading_prepared = vector<bool>(K, true);


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
        starting_time
    );

    
    cout << "=== Gurobi_preprocessing закончилось за " << roundN(chrono::duration<double>(chrono::system_clock::now() - t4).count(), 3) << " сек. ===\n";
    


    ofstream file("load_and_double_race.txt");
    auto old_buf = cout.rdbuf(file.rdbuf());


    // for (int j = 0; j < 5; j++) {
        int double_race_number = 3;     // число бензовозов, делающих по 2 рейса

        for (int i = 0; i < 13; i++) {
        int load_number = i;     // число бензовозов, после рейса заполняющихся под сменщика

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

            auto t4 = chrono::system_clock::now();
            cout << "Время работы Gurobi: " << roundN(chrono::duration<double>(t4 - t3).count(), 3) << " сек." << endl;

            bool print_logs;
            if (double_race_number == 3) {
                print_logs = true;
            }
            else {
                print_logs = false;
            }

            gurobi_results(*res->model, res->y, res->g, filling_on_route, gl_num, log, sigma, print_logs);
        }        
    // }
    cout.rdbuf(old_buf);
    // gurobi_results(*res->model, res->y, res->g, filling_on_route, gl_num, log, sigma);

    // GRBModel& model = ;
    // auto& y = ;
    // auto& g = ;



    return 0;
};