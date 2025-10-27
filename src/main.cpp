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
    
    // MASKS TEST
    // vector<int> demand = {0, 5, 0, 3};
    // vector<int> spaces = {100, 100, 100, 100};
    // Station s(1, 10, 15, demand, spaces);
    // for (int i : s.demanded_res) {
    //     cout << i << " ";  // → 1 3
    // }
    // cout << bool_mask("245", 7) << " ";
    // cout << mask_to_digits(52, 7) << " ";
    // cout << "\n";

    // POSSIBLE COMBINATIONS TEST 
    // map<int, vector<string>> possible_combinations = {
    // {0, {string("0"), string("356"), string("107"), string("256"), string("123456"), string("56789")}},
    // {1, {}},
    // {2, {string("23"), string("6"), string("780")}},
    // {3, {string("5"), string("72"), string("8"), string("0123")}},
    // {4, {string("1"), string("34"), string("2"), string("9"), string("4")}},
    // {5, {string("01234")}},
    // };

    // int comp_num = 10;
    // auto [best_masks, best_selections] = dp_max_unique_digits_all_masks(possible_combinations, comp_num);
    // vector<string> digits_combination;
    // for (int mask : best_masks) {
    //     digits_combination.push_back(mask_to_digits(mask, comp_num));
    // }
    // cout << "Digits combinations:\n";
    // for (const auto &s : digits_combination) cout << s << " ";
    // cout << endl;

    // cout << "Best selections:\n";
    // for (auto &s : best_selections) {
    //     for (auto &selection : s) {
    //         for (auto &sel : selection) {
    //             cout << sel << " ";
    //         }
    //         cout << "\n";            
    //     }
    // }

    // vector<vector<string>> testic = possible_filling({1,2,3,4}, {4,4,4,12}, {5,7,9,16});
    // for (auto &combination : testic) {
    //     cout << "[";  
    //     for (auto &elem : combination) {
    //         if (elem != "")
    //             cout << elem + ',' << " ";
    //         else
    //             cout << "None, ";

    //     }
    //     cout << "]\n";            
    // }

    // GET FILLINGS TEST
    // map<pair<int, int>, int> gl_num = global_numeration({4,4});
    // for (const auto& [key, value] : gl_num) {
    //     const auto& [station, res_idx] = key; // распаковка пары-ключа
    //     cout << "(" << station << "," << res_idx << ") -> " << value << '\n';
    // }
    // Truck truck = Truck(1, {1000, 10, 1000, 1000});
    // vector<Station> chosen_stations = {Station(2, 10, 15, {0, 5, 0, 3}, {100, 100, 100, 100}), Station(3, 10, 15, {1000, 5, 3000, 3}, {1001, 100, 3005, 100})};
    // vector<vector<string>> fillings = get_fillings(truck, chosen_stations, gl_num);
    // for (auto &filling : fillings) {
    //     for (auto &elem : filling) {
    //         if (!elem.empty())
    //             cout << elem << " ";
    //         else
    //             cout << "x ";
    //     }
    //     cout << "\n";
    // }

    // ALL_FILLINGS_TEST
    // vector<Station> stations = {
    //     Station(0, 0, 2, {5, 0}, {10, 10}),
    //     Station(1, 1, 1, {3, 2}, {10, 10})
    // };
    // Truck truck(1, {5, 5});  // два отсека по 5 единиц
    // vector<vector<int>> time_to_station = {
    //     {0, 1},
    //     {1, 0}
    // };
    // map<pair<int,int>,int> gl_num = {
    //     {{0,0}, 0}, {{0,1}, 1},
    //     {{1,0}, 2}, {{1,1}, 3}
    // };
    // int H = 5;
    // int st_in_trip = 2;
    // int top_nearest = 2;
    // set<vector<string>> result = all_fillings(stations, truck, time_to_station, gl_num, H, st_in_trip, top_nearest);
    // int cnt = 0;
    // for (const auto& filling : result) {
    //     cout << "Filling " << cnt++ << ": ";
    //     for (const auto& val : filling) cout << (val.empty() ? "x" : val) << " ";
    //     cout << endl;
    // }

    // TWO_PIPES_OPT_TEST
    // vector<int> fill_times = {3, 1, 4, 2, 7, 9};
    // int best = two_pipes_opt(fill_times);
    // cout << "BEST TIME = " << best << endl;


    // COMPUTE_TIME_FOR_ROUTE_TEST
    // map<int, pair<int,int>> reverse_index = {
    //     {0, {1,0}}, {1, {1,0}}, {2, {2,0}}, {3, {3,0}}
    // };
    // vector<int> compartments = {1000, 2000, 1500, 1200};
    // vector<string> fill = {"0", "1", "0", "3"};  // строки как в твоей функции
    // bool double_piped = true;
    // vector<Station> input_station_list = {
    //     Station(0, 5, 5, {0, 1}, {10, 10}),
    //     Station(1, 3, 3, {1, 0}, {10, 10}),
    //     Station(2, 4, 4, {0, 1}, {10, 10}),
    //     Station(2, 4, 4, {0, 1}, {10, 10})
    // };
    // vector<vector<int>> demanded_matrix = {
    //     {0,5,10,15},
    //     {5,0,8,12},
    //     {10,8,0,6},
    //     {15,12,6,0}
    // };
    // vector<int> docs_fill = {3,4,5,2};
    // pair<int, vector<string>> result = compute_time_for_route(
    //     reverse_index,
    //     compartments,
    //     fill,
    //     double_piped,
    //     input_station_list,
    //     demanded_matrix,
    //     docs_fill
    // );
    // cout << "Минимальное время: " << result.first << " минут\n";
    // cout << "Лог времени:\n";
    // for (const string& s : result.second) {
    //     cout << "  " << s << "\n";
    // }

    // GRB_TEST 
    // try {
    //     GRBEnv env = GRBEnv(true);
    //     env.start();
    //     cout << "Gurobi C++ API работает!" << endl;
    // } catch (GRBException &e) {
    //     cout << "Ошибка: " << e.getMessage() << endl;
    // }

    // PREPROCESSING_TEST
    // параметры
    // int N = 4;              // число станций (без депо)
    // int H = 480;            // длительность смены (мин)
    // int K = 2;              // число бензовозов
    // int R1 = 3;             // число станций в маршруте (параметр) → побольше
    // int R2 = 3;             // ближайших станций для рассмотрения

    // // матрица времен между станциями (мин)
    // vector<vector<int>> time_matrix = {
    //     {0, 10, 20, 30},
    //     {10, 0, 15, 25},
    //     {20, 15, 0, 12},
    //     {30, 25, 12, 0}
    // };

    // // времена до/от депо (ключи "to" и "from")
    // vector<map<string,int>> depot_times(N);
    // depot_times[0] = {{"to", 5}, {"from", 5}};
    // depot_times[1] = {{"to", 8}, {"from", 8}};
    // depot_times[2] = {{"to", 6}, {"from", 6}};
    // depot_times[3] = {{"to", 12}, {"from", 12}};

    // // станции: уберём пустые, везде есть резервуары
    // vector<map<string, vector<int>>> stations(N);
    // stations[0] = { {"min", {100}}, {"max", {300}} };
    // stations[1] = { {"min", {50, 60}}, {"max", {200, 150}} };
    // stations[2] = { {"min", {40}}, {"max", {120}} };       // раньше была пустая, теперь норм
    // stations[3] = { {"min", {30}}, {"max", {100}} };

    // // грузовики
    // vector<vector<int>> trucks = {
    //     {100, 100},   // truck 0
    //     {50, 80}     // truck 1
    // };

    // // доступность станций для каждого грузовика (N x K)
    // vector<vector<int>> access(N, vector<int>(K, 1));
    // // оставим как есть (все станции доступны)

    // // возможность двух шлангов
    // vector<bool> double_piped = { true, false };

    // // дневной коэффициент
    // float daily_coefficient = 1.0f;

    // // время на документы
    // vector<int> docs_fill = { 5, 7, 4, 6 };

    // // H_k пустой
    // vector<double> H_k = {}; 

    // // кто загружен под сменщика
    // vector<bool> loading_prepared = { true, false };

    // // дополнительные параметры можно оставить пустыми
    // map<int, vector<string>> reservoir_to_product;
    // map<int, set<tuple<string>>> truck_to_variants;

    // // Вызов функции
    // tuple< 
    // map<int, vector<vector<int>>>,
    // map<pair<int, int>, double>,
    // vector<map<string, int>>, 
    // int,
    // map<pair<int, int>, int>, 
    // map<pair<int, int>, vector<string>>,
    // vector<double>
    // > result = gurobi_preprocessing(
    //     N, H, K,
    //     time_matrix,
    //     depot_times,
    //     stations,
    //     trucks,
    //     R1, R2,
    //     access,
    //     double_piped,
    //     daily_coefficient,
    //     docs_fill,
    //     H_k,
    //     loading_prepared,
    //     reservoir_to_product,
    //     truck_to_variants
    // );

    // // Распаковка результата
    // auto& filling_on_route = get<0>(result);
    // auto& sigma            = get<1>(result);
    // auto& reservoirs       = get<2>(result);
    // int tank_count         = get<3>(result);
    // auto& gl_num           = get<4>(result);
    // auto& log              = get<5>(result);
    // auto& H_k_out          = get<6>(result);

    // cout << "=== Результат gurobi_preprocessing ===\n";
    // cout << "tank_count: " << tank_count << "\n";
    // cout << "filling_on_route.size(): " << filling_on_route.size() << "\n";

    // for (const auto& [truck, routes] : filling_on_route) {
    //     cout << "Бензовоз " << truck << " используется, выбранные маршруты:\n\n";
    //     int route_num = 0;
    //     for (const auto& route : routes) {   // route = vector<int>
    //         cout << "  Маршрут №" << route_num << "\n";
    //         for (size_t station = 0; station < route.size(); ++station) {
    //             cout << "    Станция №" << station << ", заполненные резервуары №: ";
    //             cout << route[station] << "\n";   // если у тебя vector<int>, здесь число заполненных резервуаров
    //         }
    //         route_num++;
    //         cout << "\n";
    //     }
    // }
    // vector<int> owning(K, 1);


    // auto res = gurobi_covering(
    //     filling_on_route,
    //     sigma,
    //     reservoirs,
    //     tank_count,
    //     720,      // H
    //     K,
    //     900,      // timelimit
    //     gl_num,
    //     H_k_out,
    //     owning
    // );

    // // GRBModel& model = ;
    // // auto& y = ;
    // // auto& g = ;

    // gurobi_results(*res->model, res->y, res->g, filling_on_route, gl_num, log, sigma);
    

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
    vector<int> loading_prepared = data.loading_prepared;
    map<string, vector<string>> reservoir_to_product = data.reservoir_to_product;
    map<string, set<vector<string>>> truck_to_variants = data.truck_to_variants;
    vector<int> owning = data.owning;
    vector<int> is_critical = data.is_critical;

    int R1 = 3;
    int R2 = 10;
    double_piped = vector<bool>(K, true);
    H_k = vector<double>(K, 720);
    //loading_prepared = vector<int>(K, 1);


    tuple< 
    map<int, vector<vector<int>>>,
    map<pair<int, int>, double>,
    vector<map<string, double>>, 
    int,
    map<pair<int, int>, int>, 
    map<pair<int, int>, vector<string>>,
    vector<double>
    > result = gurobi_preprocessing(
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
        reservoir_to_product,
        truck_to_variants
    );

    // Распаковка результата
    auto& filling_on_route = get<0>(result);
    auto& sigma            = get<1>(result);
    auto& reservoirs       = get<2>(result);
    int tank_count         = get<3>(result);
    auto& gl_num           = get<4>(result);
    auto& log              = get<5>(result);
    auto& H_k_out          = get<6>(result);

    cout << "=== Gurobi_preprocessing закончилось ===\n";
    

    // for (const auto& [truck, routes] : filling_on_route) {
    //     cout << "Бензовоз " << truck << " используется, выбранные маршруты:\n\n";
    //     int route_num = 0;
    //     for (const auto& route : routes) {   // route = vector<int>
    //         cout << "  Маршрут №" << route_num << "\n";
    //         for (size_t station = 0; station < route.size(); ++station) {
    //             cout << "    Станция №" << station << ", заполненные резервуары №: ";
    //             cout << route[station] << "\n";   // если у тебя vector<int>, здесь число заполненных резервуаров
    //         }
    //         route_num++;
    //         cout << "\n";
    //     }
    // }

    ofstream file("output.txt");
    for (const auto& entry : sigma) {
        file << "Truck, route: (" << entry.first.first << ", " << entry.first.second 
            << "), Time: " << entry.second << endl;
    }
    file.close();

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
        is_critical
    );
    auto t4 = chrono::system_clock::now();
    cout << "Время работы Gurobi: " << roundN(chrono::duration<double>(t4 - t3).count(), 3) << " сек." << endl;

    // GRBModel& model = ;
    // auto& y = ;
    // auto& g = ;

    gurobi_results(*res->model, res->y, res->g, filling_on_route, gl_num, log, sigma);



    return 0;
};