#include "prep_functions.hpp"
#include <iostream>
#include <format>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <assert.h>

using namespace std;

// Классы Truck и Station для дальнейшего использования в препроцессинге

Station::Station(int number, double time_to_depot, double time_from_depot,
            const vector<double>& demand, const vector<double>& remaining_spaces) :
  number(number),
  time_to_depot(time_to_depot),
  time_from_depot(time_from_depot),
  demand(demand),
  remaining_spaces(remaining_spaces){};


Truck::Truck(int number, const vector<double>& compartments): 
  number(number), 
  compartments(compartments){};


// Функции для работы с маской внутри алгоритма динамического программирования
//'245' → bool_mask = 52 (00110100) → mask_to_digits → '245'
int bool_mask(string s, int comp_num) {    // перевод строки из цифр - отсеков в булеву маску
    int mask = 0;
    for (char c : s) {                   // итерируемся по символам строки
        int d = c - '0';                 // получаем значение цифры за счёт вычитания ASCII кодов
        if (0 <= d && d <= comp_num) {
            int bit = 1 << d;            // сдвигаем бит на нужное место 
            mask |= bit;                 // совмещаем его с маской 
        }
    }
    return mask;
}

string mask_to_digits(int mask, int comp_num) {
    string s;
    for (int i = 0; i <= comp_num; ++i) {   // перебор всех битов
        if (mask & (1 << i)) {              // если есть бит
            s += char('0' + i);             // добавляем цифру (ASCII-код нуля + величина цифры)
        }
    }
    sort(s.begin(), s.end());   // сортируем по возрастанию
    return s;
}


// Алгоритм динамического программирования 
pair<vector<int>, vector<vector<vector<string>>>> dp_max_unique_digits_all_masks(
  const map<int, vector<string>>& possible_combinations, 
  int comp_num
) {
  map<int, vector<vector<string>>> dp;        // словарь вида маска цифр(отсеки) : вектор выбранных отсеков ("" если резервуар пропущен)
  dp[0] = {{}};                               // начальное состояние: ни одна цифра не использована, вектор векторов пуст
  
  for (const auto& [key, current_options] : possible_combinations) {
        map<int, vector<vector<string>>> next_dp;

        for (const auto& [mask, selections] : dp) {                   // перебираем все состояния (маски : выбор отсеков)
            for (const auto& selection : selections) {                // для каждой селекции выбранной маски 

                // пропускаем резервуар
                for (const auto& num_str : current_options) {         // выбираем один из вариантов заполнения текущего резервуара num_str (например, '254')
                    int num_mask = bool_mask(num_str, comp_num);      // берём маску от него, чтобы удобно пересекать

                    if ((mask & num_mask) == 0) {                     // если нет пересечений в масках (общих используемых отсеков)
                        int new_mask = mask | num_mask;               // совмещаем маски
                        vector<string> new_selection = selection;     
                        new_selection.push_back(num_str);             // добавляем в конец текущий вариант заполнения резервуара
                        next_dp[new_mask].push_back(new_selection);
                    }
                }

                // заполняем резервуар
                vector<string> new_selection = selection;
                new_selection.push_back("");                           // добавляем в конец "" - пропуск резервуара
                next_dp[mask].push_back(new_selection);
            }
        }

        dp = move(next_dp);
    }


    // находим максимальное число использованных битов
    int max_bits = 0;
    for (const auto& [mask, _] : dp) {
        int bits = __popcnt(mask);      // считаем число битиков - это и есть число использованных отсеков
        if (bits > max_bits) max_bits = bits;     // и если что, обновляем максимум
    }
    
    // full_mask = 2**comp_num - 1
    //     if full_mask in dp:
    //         # возвращаем сразу варианты для полной маски
    //         return full_mask, dp[full_mask]

    // TODO: move'ать векторы здесь 

    // собираем все лучшие маски
    vector<int> best_masks;
    vector<vector<vector<string>>> best_selections;

    for (const auto& [mask, selections] : dp) {
        if (__popcnt(mask) == max_bits) {
            best_masks.push_back(mask);
            best_selections.push_back(selections);
        }
    }

    return {best_masks, best_selections};
}


// аналог itertools.combinations
vector<vector<int>> combinations(int n, int k) {
    vector<vector<int>> result;

    // маска: (n - k) нулей, потом k единиц
    vector<bool> mask(n);
    fill(mask.end() - k, mask.end(), true);  // 000...111

    do {                        // берём максимальную маску и двигаемся назад, перебирая все размещения 
        vector<int> comb;
        for (int i = 0; i < n; i++) {
            if (mask[i]) comb.push_back(i);             // по маске берём индексы
        }
        result.push_back(comb);
    } while (next_permutation(mask.begin(), mask.end()));

    return result;
}



// функция, перебирающая все комбинации отсеков, чтобы узнать, какими комбинациями заполняются резервуары
// по завершении вызывает алгоритм ДП
vector<vector<string>> possible_filling(const vector<double>& compartments, const vector<double>& mins, const vector<double>& maxs) {
    map<int, vector<string>> possible_combinations;
    vector<vector<int>> result;

    for (int el_num = 1; el_num <= compartments.size(); el_num++) {      // число отсеков в комбинации
         for (auto comb : combinations(compartments.size(), el_num)) {     // перебираем комбинации с el_num числом отсеков
            double val = 0;
            for (int i : comb) {            // суммарная вместимость отсеков комбинации
                val += compartments[i];
            }
            for (int res_num = 0; res_num < mins.size(); res_num++) {   // проходим по резервуарам и проверяем, вмещается ли
                possible_combinations[res_num];                         // создаём set, если его еще нет, а иначе не перезапсиываем
                if (mins[res_num] <= val && val <= maxs[res_num]){
                    stringstream ss;
                    for (int i : comb) ss << i;
                    string s = ss.str();
                    possible_combinations[res_num].push_back(s);
                }
            }
        }
    }
    auto [best_masks, best_selections] = dp_max_unique_digits_all_masks(possible_combinations, compartments.size());

    if (best_masks[0] == (1 << compartments.size()) - 1) {  // есть вариант, в котором все отсеки использованы (он же и будет первым)
       return best_selections[0];
    }
    return vector<vector<string>> {};
}


// [[None, "z", "x"], ["a", None, "b"], [None, "c", "d"]] -> {[1, 0, 1], [0, 1, 1]}
set<vector<int>> boolify_reservoirs(const vector<vector<string>>& fillings) {
    set<vector<int>> masks;
    for (const auto& filling : fillings) {
        vector<int> mask;
        for (const auto& elem : filling) {
            mask.push_back(!elem.empty());      // если строка не пустая → 1, иначе 0
        }
        masks.insert(mask);
    }
    return masks;
}


// хочу вернуть словарь который по номеру станции и номеру резервуара даст глобальный номер
map<pair<int, int>, int> global_numeration(const vector<int>& lengths){
    int global_number = 0;
    map<pair<int, int>, int> num;

    for (int st_num = 0; st_num < lengths.size(); st_num++){
        int total_res = lengths[st_num];        // число резервуаров на станции
        int res_num = 0;                        // номер текущего резервуара на станции                
        while (res_num < total_res){
            num[{st_num, res_num}] = global_number;
            res_num += 1;
            global_number += 1;
        }
    }
    
    return num;
}


// "разворачивает" карту из вида station_n: vector<product_name> в reservoir_global_n: product_name
map<int, string> global_product_mapping(const map<string, vector<string>>& reservoir_to_product) {
    map<int, string> global_mapping;
    int global_index = 0;
    
    for (const auto& [reservoir, products] : reservoir_to_product) {
        for (const auto& product : products) {
            global_mapping[global_index] = product;
            global_index++;
        }
    }
    
    return global_mapping;
}


// переводит заполнение вида {'', '34', '', '1'} в названия продуктов в бензовозе {'none', 'diesel, 'none', 'petrol', 'petrol'}
vector<string> convert_compartments(int compartments_count, const vector<string>& fill, const map<int, string>& res_to_product) {
    // Создаем результат размера compartments_count, заполненный "none"
    vector<string> result(compartments_count, "none");
    
    for (int i = 0; i < fill.size(); i++) {
            if (!fill[i].empty()) {
            for (char c : fill[i]) {                        // если строка непуста, обрабатываем каждый символ (отсек) в строке
                if (isdigit(c)) {
                    int key = c - '0';
                    result[key] = res_to_product.at(i);     // отсек key заполнен продуктом для резервуара i
                }
            }
        }
    }
    return result;
}


// Возвращает заполнения для выбранного грузовика и станций (в глобальной нумерации)
vector<vector<string>> get_fillings(const Truck& truck, const vector<Station>& chosen_stations, const map<pair<int, int>, int>& gl_num) {
    vector<double> mins, maxs;
    for (Station st : chosen_stations){                                 // добавляем резервуары из станций, аналог extend
        mins.insert(mins.end(), st.demand.begin(), st.demand.end());
        maxs.insert(maxs.end(), st.remaining_spaces.begin(), st.remaining_spaces.end());
    }

    vector<int> local_to_global;
    for (const Station& st : chosen_stations) {
        int num_res = st.demand.size();  // число резервуаров на станции
        for (int res_idx = 0; res_idx < num_res; res_idx++) {
            // if (gl_num.count({st.number, res_idx}) == 0) {
            //     cout << "missing: " << st.number << " " << res_idx << endl;
            // }
            local_to_global.push_back(gl_num.at({st.number, res_idx}));
        }
    }



    vector<vector<string>> fillings;
    fillings = possible_filling(truck.compartments, mins, maxs);

    if (fillings.empty()) {         // если нет возможных заполнений
        return vector<vector<string>> {};
    }


    // осталось перевести заполнения в глобальную нумерацию
    int total_res_num = gl_num.size();      // всего резервуаров             
    vector<vector<string>> global_fillings;
    for (vector<string> filling : fillings){
        vector<string> curr_fill(total_res_num, "");       // создаём глобальный массив заполнений

        for (int local_idx = 0; local_idx < (int)filling.size(); local_idx++) {
            if (!filling[local_idx].empty()) {
                int global_idx = local_to_global[local_idx];    // достаём глобальный индекс
                curr_fill[global_idx] = filling[local_idx];     // ставим нужные отсеки
            }
        }
        global_fillings.push_back(move(curr_fill));
    }

    return global_fillings;

}

// TODO: unordered_set<vector<int>>, написать хэш для вектора

set<vector<string>> find_routes(
    vector<Station> current_route, 
    const vector<Station>& stations,
    set<set<int>>& seen_routes, 
    const Truck& truck,
    const map<pair<int, int>, int>& gl_num,
    const map<int,int>& local_index,
    const vector<vector<double>>& time_to_station, 
    int current_time, 
    int st_in_trip, 
    int top_nearest,
    int H
    ) {
    if (current_route.size() == st_in_trip){
        set<int> route_key;
        for (const auto& station : current_route) {
            route_key.insert(station.number);           // сохраняем изначальные номера станций в множестве
        }
        if (seen_routes.find(route_key) != seen_routes.end()){      // если уже есть такой маршрут
            return set<vector<string>> {};
        }
        seen_routes.insert(route_key);      // если ещё не видели, добавляем
    
        // cout << "route: [";
        // for (size_t i = 0; i < current_route.size(); ++i) {
        //     cout << current_route[i].number;
        //     if (i + 1 < current_route.size()) cout << ", ";
        // }
        // cout << "], curr_time: " << current_time << endl;
        
        vector<vector<string>> fillings = get_fillings(truck, current_route, gl_num);

        // if (fillings.empty()) {
        //     cout << "⚠️ Нет заполнений для маршрута: ";
        //     for (Station s : current_route) cout << s.number << " ";
        //     cout << endl;
        // }
        
        if (fillings.empty()) return set<vector<string>> {}; // заполнения не нашлись        
        set<vector<string>> s(fillings.begin(), fillings.end());  // возвращаем множество заполнений 
        return s;
    }
    
    // если ещё не набрали нужное число станций, делаем следующие шаги:
    Station last_station = current_route.back();    // последняя станция маршрута
    int last_station_local = local_index.at(last_station.number);   // номер этой станции среди нашего подмножества доступных станций

    vector<pair<double,int>> res;          // вектор пар (время, номер станции)
    for (int i = 0; i < (int)time_to_station[last_station_local].size(); i++) {
        res.emplace_back(time_to_station[last_station_local][i], i);                // в конце вектора создаём пару (время, номер станции)
    }

    // 1.Сортировка по времени
    sort(res.begin(), res.end(),                                // сортируем от начала до конца по правилу сравнения пар - если первый элемент больше, то это бОльшая пара
         [](const pair<double,int>& a, const pair<double,int>& b) {
             return a.first < b.first;
         });

    // 2.Обрезаем до top_nearest
    if ((int)res.size() > top_nearest) {
        res.resize(top_nearest);
    }

    // 3.Обрезаем те, что не подходят по времени
    auto it = find_if(res.begin(), res.end(),
    [&](const pair<double,int>& p){
        return current_time + p.first + stations[p.second].time_to_depot > H;
    });
    res.erase(it, res.end());

    // vector<pair<int,int>> filtered;
    // for (const auto& p : res) {
    //     int time = p.first;
    //     int idx  = p.second;
    //     if (current_time + time + stations[idx].time_to_depot <= H) filtered.push_back(p); else break; 
    //     // дальше все элементы уже будут больше по времени
    // }

    // 4.Для оставшихся станций строим новые маршруты
    set<vector<string>> result;
    for (const auto& [time, idx] : res) {
        bool already_in_route = false;
        for (const Station& st : current_route) {
            if (stations[idx].number == st.number){
                already_in_route = true;
                break;
            }
        }
        if (already_in_route) continue;

        current_route.push_back(stations[idx]);
        set<vector<string>> tmp = find_routes(current_route, stations, seen_routes, truck, gl_num, local_index, time_to_station, current_time + time, st_in_trip, top_nearest, H);
        current_route.pop_back();

        result.insert(tmp.begin(), tmp.end());
    }
    return result;
}


/**
 * @brief Возвращает все возможные варианты заполнения резервуаров на одном маршруте бензовоза.
 * 
 * @param stations Список станций, доступных бензовозу k.
 * @param truck Бензовоз с известными отсеками.
 * @param time_to_station Матрица времени поездки между доступными станциями.
 * @param gl_num Словарь глобальной нумерации: 
 *        ключ = (номер станции, номер резервуара на станции)
 *        значение = глобальный индекс резервуара.
 * @param H Длина рабочей смены.
 * @param st_in_trip Точное число станций в маршруте (по умолчанию 3).
 * @param top_nearest Размер окрестности на каждом шаге поиска (по умолчанию 4).
 * @param local_index Маппинг из глобального номера станции в локальный индекс  
 *        в списке stations и time_to_station, которые содержат только доступные станции.
 * 
 * @return set<vector<int,...>> Множество кортежей длины reservoir_count,
 *         каждый кортеж — вариант заполнения резервуаров в глобальной нумерации.
 */
set<vector<string>> all_fillings(              // TODO: сделать unordered_set, но написать хеш для vector<int>
    const vector<Station>& stations, 
    const Truck& truck, 
    const vector<vector<double>>& time_to_station,
    const map<pair<int,int>,int>& gl_num,
    int H,
    int st_in_trip, 
    int top_nearest,
    map<int, int> local_index
    ) {
    
    if (local_index.empty()) {
    for (size_t i = 0; i < stations.size(); ++i)
        local_index[i] = i;  // индекс в векторе
    }

    set<set<int>> seen_routes {};
    set<vector<string>> final_set;
    
    for (const Station& station : stations){
        vector<Station> initial_route = {station};  
        int start_time = station.time_from_depot + (accumulate(truck.compartments.begin(), truck.compartments.end(), 0.0) / 1000.0) * 3;        // pour_time
        set<vector<string>> tmp = find_routes(initial_route, stations, seen_routes, truck, gl_num, local_index, time_to_station, start_time, st_in_trip, top_nearest, H);
        final_set.insert(tmp.begin(), tmp.end());
    }

    return final_set;
}


// Ищет оптимальное распределение времён на 2 работы полным перебором
double two_pipes_opt(const vector<double>& fill_times){
    int n = fill_times.size();
    if (n == 1) return fill_times[0];
    vector<int> idxs(n);
    iota(idxs.begin(), idxs.end(), 0);  // [0,1,2,...,n-1]
    double best = INT_MAX;

    // vector<bool> in_comb(n, false);
    // for (int i : comb) in_comb[i] = true;

    // for (int i = 0; i < n; i++) {
    //     if (in_comb[i]) t1 += fill_times[i];
    //     else            t2 += fill_times[i];
    // }

    // перебор размера первого подмножества (до n/2)
    for (int k = 0; k <= n/2; k++){             
        auto combs = combinations(n, k);    // перебираем комбинации размера k
        for (auto &comb : combs) {
            double t1 = 0, t2 = 0;
            for (int i = 0, j = 0; i < n; i++) {                // проходим по всем временам и создаём две суммы
                if (j < (int)comb.size() && comb[j] == i) {
                    t1 += fill_times[i];                        // если в комбинации, то в первую сумму
                    j++;                                        // и сдвигаем внутрениий counter внутри комбинации
                } else {
                    t2 += fill_times[i];                        // иначе во вторую сумму
                }
            }
            best = min(best, max(t1, t2));
        }
    }
    return best;
}

double roundN(double val, int n) {
    double factor = pow(10, n);
    return round(val * factor) / (double)factor;
}


inline void log_time(const string& message, vector<string>& time_log) {
    time_log.emplace_back(message); 
}

// вычисление длительности маршрута 
pair<double, vector<string>> compute_time_for_route(
    const map<int, pair<int, int>>& reverse_index,
    const vector<double>& compartments, 
    const vector<string>& fill,
    bool double_piped,
    const vector<Station>& input_station_list,
    const vector<vector<double>>& demanded_matrix,
    const vector<double>& docs_fill
    ){
    double pour_time = (accumulate(compartments.begin(), compartments.end(), 0.0) / 1000.0) * 3;      // время заполнения бензовоза в депо
    
    vector<int> idx {};
    for (int i = 0; i < fill.size(); i++){
        if (!fill[i].empty()) idx.push_back(i);             // индексы резервуаров, заполняемых на этом маршруте
    }

    // теперь для тех резервуаров, которые реально заполняются, найдём номера их станций
    set<int> tmp;                        // множество уникальных элементов
    for (int i : idx) {
        tmp.insert(reverse_index.at(i).first);  
    }
    vector<int> ordered_st(tmp.begin(), tmp.end());     // отсортированный вектор посещаемых станций
    // for (int i = 0; i < ordered_st.size(); i++){
    //    cout << ordered_st[i] << " ";
    // }

    unordered_map<int, vector<string>> station_resevoirs {}; 
    unordered_map<int, string> station_comps {};

    for (int st : ordered_st){
        vector <string> comps {};               // отсеки, выгружаемые на этой станции
        string& comps_str = station_comps[st];  // ссылка на строку в map, чтобы не создавать копию

        for (int i : idx) {
            const string& f = fill[i];

            if (reverse_index.at(i).first == st) {
                comps.push_back(f); 
                station_comps[st] += f;
            }
        }
        station_resevoirs[st] = move(comps);          // резервуары, выгружаемые на этой станции
    }

    vector<vector<int>> permutations;   // лист комбинаций
    do {
        permutations.push_back(ordered_st);  // сохраняем текущую перестановку
    } while (next_permutation(ordered_st.begin(), ordered_st.end()));

    vector<double> times{};
    vector<vector<string>> timelogs {};

    for (const vector<int>& perm : permutations) {

        if (station_comps[perm.back()].find('0') == string::npos){   // если первый отсек выгружается не на последней станции, пропускаем комбинацию
            continue;
        }
        vector<string> time_log {};
        double time = 0;

        // заполнение документов на каждой из станций
        for (int stn_n : ordered_st) {
            time += docs_fill[stn_n];
            log_time(to_string(docs_fill[stn_n]) + " минут - время заполнения документов на станции " + to_string(stn_n), time_log);
        }

        // если только один рукав, знаем времена
        if (!double_piped){
            time += 2 * roundN(pour_time, 3);    // время на заполнение бензовоза (до рейса и во время)
            log_time(to_string(2 * roundN(pour_time, 3)) + " минут - заполнение резервуаров одним шлангом на станциях и в депо", time_log);
        }
        else {      // двушланговый, пытаемся быстрее разгрузить
            for (int st : perm) {
                if (station_comps[st].size() > 1) {   // если на станции выгружается больше одного отсека
                    vector<double> reservoir_fill_values {}; 
                    for (const string& string_of_comps : station_resevoirs[st]){   // беру строчки - комбинации отсеков для каждого резервуара
                        double local_sum = 0;
                        for (char comp_number : string_of_comps) {                          // итерируюсь по каждой цифре - номеру отсека
                            local_sum += compartments[int(comp_number - '0')] / 1000.0 * 3;
                        }
                        reservoir_fill_values.push_back(local_sum);
                    }
                    double optimal_filling_time = roundN(two_pipes_opt(reservoir_fill_values), 3);
                    time += optimal_filling_time;
                    log_time(to_string(optimal_filling_time) + " минут - заполнение резервуаров двумя шлангами на станции " + to_string(st), time_log);
                }
                else {
                    double optimal_filling_time = roundN(compartments[int(station_comps[st][0] - '0')] / 1000.0 * 3, 3);
                       time += optimal_filling_time;
                       log_time(to_string(optimal_filling_time) + " минут - заполнение одного резервуара одним шлангом на станции " + to_string(st), time_log);
                }
            }
            time += roundN(pour_time, 3);
            log_time(to_string(roundN(pour_time,3)) + "минут - заполнение резервуаров одним шлангом в депо", time_log);
            
        }

        int first = perm[0];                                      // достали станцию
        time += input_station_list[first].time_to_depot;          // доехали до нее от депо
        log_time(to_string(roundN(input_station_list[first].time_to_depot, 3)) + " минут - время от депо до станции " + to_string(first), time_log);  

        for (int i = 0; i + 1 < perm.size(); i++) {            // ездим между станциями
            int curr_station = perm[i];
            int next_station = perm[i + 1];

            time += demanded_matrix[curr_station][next_station];
            log_time(to_string(roundN(demanded_matrix[curr_station][next_station],3)) + " минут - время от станции " + to_string(curr_station) + " до станции " + to_string(next_station), time_log);
        }

        int last_st = perm.back();
        time += input_station_list[last_st].time_to_depot;    // возвращаемся в депо
        log_time(to_string(roundN(input_station_list[last_st].time_to_depot, 3)) + " минут - время от станции " + to_string(last_st) + " до депо", time_log);

        times.push_back(time);
        timelogs.emplace_back(move(time_log));      // переносим логи
    }
    
    if (!times.empty()) {
        int indx = distance(times.begin(), min_element(times.begin(), times.end()));         // индекс ближайшего (с начала массива) минимального элемента
        return {times[indx], timelogs[indx]};
    }

    return {10000, {}};              // 10000 - просто большое число
}

