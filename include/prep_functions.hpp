#pragma once   // защита от двойного включения

#include <vector>
#include <string>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <intrin.h>  // для MSVC

using namespace std;

class Station {
public:
    int number;
    double time_to_depot;
    double time_from_depot;
    vector<double> demand;
    vector<double> remaining_spaces;
    vector<vector<double>> consumption_percent; 
    double docs_fill;

    // конструктор
    Station(int number, double time_to_depot, double time_from_depot,
            const vector<double>& demand, const vector<double>& remaining_spaces, const vector<vector<double>>& consumption_percent, int docs_fill);

};

class Truck {
public:
    int number;
    vector<double> compartments;
    int starting_time;
    bool loaded;
    int owning;
    Truck(int number, const vector<double>& compartments, int starting_time, bool loaded, int owning);
};

// Utility


// перевод числа в битовую маску
int bool_mask(string n, int comp_num);

// обратный перевод маски в строку цифр
string mask_to_digits(int mask, int comp_num);

// аналог itertools.combinations
vector<vector<int>> combinations(int n, int k);

// преобразует вектор векторов строк в множество битовых масок
set<vector<int>> boolify_reservoirs(const vector<vector<string>>& fillings);

// переводит номер станции и резервуара в глобальный номер резервуара
map<pair<int, int>, int> global_numeration(const vector<int>& lengths);

map<int, string> global_product_mapping(const map<string, vector<string>>& reservoir_to_product);

vector<string> convert_compartments(int compartments_count, const vector<string>& fill, const map<int, string>& res_to_product);

double roundN(double val, int n);

// Main

// Динамический алгоритм поиска всех вариантов выгрузки отсеков бензовоза на множестве резервуаров
pair<vector<int>, vector<vector<vector<string>>>> dp_max_unique_digits_all_masks(const map<int, vector<string>>& possible_combinations, int comp_num);

// Находит все допустимые комбинации отсеков между min[i] и max[i]
vector<vector<string>> possible_filling(const vector<double>& compartments, const vector<double>& mins, const vector<double>& maxs);

// Возвращает заполнения для выбранного грузовика и станций
vector<vector<string>> get_fillings(const Truck& truck, const vector<Station>& chosen_stations, const map<pair<int, int>, int>& gl_num, const vector<double>& arrival_time);

set<vector<string>> all_fillings(
    const vector<Station>& stations, 
    const Truck& truck, 
    const vector<vector<double>>& time_to_station,
    const map<pair<int,int>,int>& gl_num,
    int H,
    int st_in_trip = 3, 
    int top_nearest = 4,
    map<int, int> local_index = {}, 
    bool start_shifted = false
    );

set<vector<string>> find_routes(
    vector<Station> current_route, 
    vector<double> arrival_times, 
    const vector<Station>& stations,
    set<set<int>>& seen_routes, 
    const Truck& truck,
    const map<pair<int, int>, int>& gl_num,
    const map<int,int>& local_index,
    const vector<vector<double>>& time_to_station, 
    int current_time, 
    int st_in_trip, 
    int top_nearest,
    int H,
    bool start_shifted
    );

double two_pipes_opt(const vector<double>& fill_times);

// void log_time(const vector<string>& messages, vector<string>& time_log);

pair<double, vector<string>> compute_time_for_route(
    const map<int, pair<int, int>>& reverse_index,
    const vector<double>& compartments, 
    bool loaded,
    const vector<string>& fill,
    bool double_piped,
    const vector<Station>& input_station_list,
    const vector<vector<double>>& demanded_matrix,
    const vector<double>& docs_fill,
    const int owned
    );


struct VectorHash {
    size_t operator()(const std::vector<int>& v) const {
        size_t seed = v.size();
        for (auto& i : v) {
            seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

struct KeyHash {
    std::size_t operator()(const std::pair<int, std::vector<int>>& k) const {
        return std::hash<int>()(k.first) ^ VectorHash()(k.second);
    }
};

struct KeyEqual {
    bool operator()(const std::pair<int, std::vector<int>>& a,
                    const std::pair<int, std::vector<int>>& b) const {
        return a.first == b.first && a.second == b.second;
    }
};

