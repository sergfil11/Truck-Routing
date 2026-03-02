#include "prep_functions.hpp"
#include <iostream>
#include <format>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <assert.h>
#include <cstdio>
#include <cstring>

using namespace std;

// Классы Truck и Station для дальнейшего использования в препроцессинге

Station::Station(int number, double time_to_depot, double time_from_depot,
            const vector<double>& demand, const vector<double>& remaining_spaces, const vector<vector<double>>& consumption_percent, int docs_fill):
  number(number),
  time_to_depot(time_to_depot),
  time_from_depot(time_from_depot),
  demand(demand),
  remaining_spaces(remaining_spaces),
  consumption_percent(consumption_percent),
  docs_fill(docs_fill){};



Truck::Truck(int number, const vector<double>& compartments, int starting_time, bool loaded, int owning): 
  number(number), 
  compartments(compartments),
  starting_time(starting_time),
  loaded(loaded),
  owning(owning)
  {};



Filling::Filling(vector<pair<int, vector<int>>> res_comps, vector<double> arrival_times, vector<int> stations_ordered, vector<string> timelog, double total_time, bool start_shifted):
  res_comps(move(res_comps)),
  arrival_times(move(arrival_times)),
  stations_ordered(move(stations_ordered)),
  timelog(move(timelog)),
  total_time(total_time), 
  start_shifted(start_shifted)
  {};

  
vector<int> make_pattern(const Filling& f) {
    vector<int> p;
    p.reserve(f.res_comps.size());
    for (const auto& [res_id, comps] : f.res_comps) p.push_back(res_id);
    sort(p.begin(), p.end());
    // p.erase(unique(p.begin(), p.end()), p.end());
    return p;
}


// Функции для работы с маской внутри алгоритма динамического программирования
//'245' → bool_mask = 52 (00110100) → mask_to_digits → '245'
inline int bool_mask(const string& s, int comp_num) {    // перевод строки из цифр - отсеков в булеву маску
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

inline string mask_to_digits(int mask, int comp_num) {
    string s;
    for (int i = 0; i <= comp_num; ++i) {   // перебор всех битов
        if (mask & (1 << i)) {              // если есть бит
            s += char('0' + i);             // добавляем цифру (ASCII-код нуля + величина цифры)
        }
    }
    sort(s.begin(), s.end());   // сортируем по возрастанию
    return s;
}

inline vector<int> parse_digits(const string& s) {
    vector<int> comps;
    comps.reserve(s.size());
    for (char ch : s) {
        if (ch >= '0' && ch <= '9')
            comps.push_back(ch - '0');
    }
    return comps;
}


// Алгоритм динамического программирования 
vector<vector<pair<int, vector<int>>>> dp_max_unique_digits_all_masks(
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

    // TODO: move'ать векторы здесь 

    // собираем все лучшие маски
    vector<int> best_masks;
    vector<vector<vector<string>>> best_selections;


    for (const auto& [mask, selections] : dp) {
        if (__popcnt(mask) == max_bits) {
            best_masks.push_back(mask);
            best_selections.push_back(move(selections));
        }
    }

    
    if (best_masks[0] == (1 << comp_num) - 1) {  // есть вариант, в котором все отсеки использованы (он же и будет первым)
        vector<vector<pair<int, vector<int>>>> result;
        result.resize(best_selections[0].size());

        for (int i = 0; i < best_selections[0].size(); i++) {             // filling index
            const auto& filling = best_selections[0][i];

            for (size_t j = 0; j < filling.size(); ++j) {                 // reservoir index
                const string& s = filling[j];

                if (s.empty()) continue;
                result[i].emplace_back((int)j, parse_digits(s));
            }
        }
        return result;
    }
    return vector<vector<pair<int, vector<int>>>> {};           // каждый вектор пар - заполнение, делаем вектор заполнений
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
vector<vector<pair<int, vector<int>>>> possible_filling(const vector<double>& compartments, const vector<double>& mins, const vector<double>& maxs) {
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

    auto best_selections = dp_max_unique_digits_all_masks(possible_combinations, compartments.size());

    return best_selections;
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


// переводит заполнение в названия продуктов в бензовозе {'none', 'diesel, 'none', 'petrol', 'petrol'}
vector<string> convert_compartments(int compartments_count, const Filling& fill, const map<int, string>& res_to_product) {
    // Создаем результат размера compartments_count, заполненный "none"
    vector<string> result(compartments_count, "none");
    

    // fill.res_comps = [(25, [3,5]), (13, [1]), ...]
    for (auto& [reservoir, compartments] : fill.res_comps) {
        for (int compartment : compartments) {
            if (0 <= compartment && compartment < compartments_count) {
                 result[compartment] = res_to_product.at(reservoir);
            }
            else {
                cout << "ERR";
            }  
        }
    }

    return result;
}


/**  
* Возвращает заполнения для выбранного грузовика и станций (в глобальной нумерации)

......
* @param arrival_time                   Массив времён прибытия на станции
* @param hourly_accumulated_consumption Массив кумулятивных почасовых потреблений в абсолютных величинах топлива для каждого резервуара
*/ 

vector<Filling> get_fillings(
    const Truck& truck, 
    const vector<Station>& chosen_stations, 
    const map<pair<int, int>, int>& gl_num, 
    const vector<double>& arrival_time,
    bool start_shifted
    ) {
    vector<double> mins, maxs;

    for (int i = 0; i < chosen_stations.size(); ++i){        // добавляем резервуары из станций, аналог extend
        Station st = chosen_stations[i];

        int hour_number = (arrival_time[i])/ 60;             // целая часть от времени прибытия на станцию
        hour_number = max(hour_number, 0);                   // строго положительный
        hour_number = min(hour_number, 11);                  // не больше 11
        // cout << "hour_number: " << hour_number;
        // cout << "arrival_time: " << arrival_time[i] << " starting_time: " << truck.starting_time << " hour_number: " << hour_number << endl;
        
        vector<double> upper_bounds; 
        for (int res_idx = 0; res_idx < st.remaining_spaces.size(); res_idx++) {
            upper_bounds.push_back(st.remaining_spaces[res_idx] + st.consumption_percent[res_idx][hour_number]);    
            // cout << "st_addition: " << st.consumption_percent[res_idx][hour_number] << " ";
        }
        
        maxs.insert(maxs.end(), upper_bounds.begin(), upper_bounds.end());
        mins.insert(mins.end(), st.demand.begin(), st.demand.end());
        //maxs.insert(maxs.end(), st.remaining_spaces.begin(), st.remaining_spaces.end());
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



    vector<vector<pair<int, vector<int>>>> fillings;
    fillings = possible_filling(truck.compartments, mins, maxs);

    if (fillings.empty()) {         // если нет возможных заполнений
        return vector<Filling>();
    }

    // переводим номера резервуаров в глобальные
    for (vector<pair<int, vector<int>>>& filling : fillings){
        for (auto& [res_local, comps] : filling) {
            res_local = local_to_global[res_local];
        }
    }

    //TODO: проверить, что первый отсек выгружается на последней станции

    vector<Filling> out;
    out.reserve(fillings.size());

    vector<int> stations_ordered;
    stations_ordered.reserve(chosen_stations.size());
    for (const Station& s : chosen_stations) stations_ordered.push_back(s.number);

    for (auto& f : fillings) {
        out.emplace_back(move(f), arrival_time, stations_ordered, vector<string>{}, 0.0, start_shifted);  // timelog="", total_time=0, start_shifted=true
    }
    return out;
}

// TODO: unordered_set<vector<int>>, написать хэш для вектора

vector<Filling> find_routes(
    vector<Station> current_route,
    vector<double> arrival_times,  
    const vector<Station>& stations,
    set<vector<int>>& seen_routes, 
    const Truck& truck,
    const map<pair<int, int>, int>& gl_num,
    const map<int,int>& local_index,
    const vector<vector<double>>& time_to_station, 
    double current_time, 
    int st_in_trip, 
    int top_nearest,
    int H, 
    bool start_shifted
    ) {
    if (current_route.size() == st_in_trip){
        vector<int> route_key;
        route_key.reserve(current_route.size());
        for (const auto& station : current_route) {
            route_key.push_back(station.number);                    // сохраняем изначальные номера станций в множестве
        }
        if (seen_routes.find(route_key) != seen_routes.end()){      // если уже есть такой маршрут
            return vector<Filling> {};
        }
        seen_routes.insert(route_key);      // если ещё не видели, добавляем
        
        return get_fillings(truck, current_route, gl_num, arrival_times, start_shifted);
        
        // if (fillings.empty()) return set<vector<string>> {};        // заполнения не нашлись
        // return fillings                                             // возвращаем множество заполнений
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

    if (start_shifted) {
        current_time += last_station.docs_fill;     // увеличиваем текущее время на заполнение документов
        current_time += 12;                         // увеличиваем текущее время на время выгрузки отсеков (хотя бы одного, размером 4000л.)
    } else {
        current_time -= last_station.docs_fill;
        current_time -= 12; 
    }

    // 3.Обрезаем те, что не подходят по времени (берём с запасом чтобы ничего не упустить, время всё равно ещё пересчитывать)
    if (start_shifted) {
        auto it = find_if(res.begin(), res.end(),
        [&](const pair<double,int>& p){
            return current_time + p.first + stations[p.second].time_to_depot*truck.owning > H * 1.1;    // последний член прибавляется только в случае чужого бензовоза
        });
        res.erase(it, res.end());
    }
    else {
        auto it = std::find_if(res.begin(), res.end(),
        [&](const pair<double,int>& p){
            return current_time - (p.first + stations[p.second].time_to_depot*truck.owning) < -H * 0.1;
        });
        res.erase(it, res.end());
    } 
    
    // vector<pair<int,int>> filtered;
    // for (const auto& p : res) {
    //     int time = p.first;
    //     int idx  = p.second;
    //     if (current_time + time + stations[idx].time_to_depot <= H) filtered.push_back(p); else break; 
    //     // дальше все элементы уже будут больше по времени
    // }

    // 4.Для оставшихся станций строим новые маршруты
    vector<Filling> result;
    vector<double> new_arrival_times = arrival_times;  
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
        if (start_shifted) 
            current_time += time;
        else 
            current_time -= time;
        new_arrival_times.push_back(current_time); 
        vector<Filling> tmp = find_routes(current_route, new_arrival_times, stations,
                                        seen_routes, truck, gl_num, local_index,
                                        time_to_station, current_time, st_in_trip,
                                        top_nearest, H, start_shifted);
        current_route.pop_back();
        new_arrival_times.pop_back();

        result.insert(result.end(), tmp.begin(), tmp.end());
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
vector<Filling> all_fillings(              // TODO: сделать unordered_set, но написать хеш для vector<int>
    const vector<Station>& stations, 
    const Truck& truck, 
    const vector<vector<double>>& time_to_station,
    const map<pair<int,int>,int>& gl_num,
    int H,
    int st_in_trip, 
    int top_nearest,
    map<int, int> local_index,
    bool start_shifted
    ) {
    
    if (local_index.empty()) {
    for (size_t i = 0; i < stations.size(); ++i)
        local_index[i] = i;  // индекс в векторе
    }

    set<vector<int>> seen_routes {};
    vector<Filling> final_vector;
    
    for (const Station& station : stations){
        vector<Station> initial_route = {station};  
        
        double start_time;
        if (start_shifted) {                                                // считаем начальное время
            start_time = station.time_from_depot + truck.starting_time;

            if (!truck.loaded)
                // cout <<"ERROR" << "\n";
                start_time += (accumulate(truck.compartments.begin(), truck.compartments.end(), 0.0) / 1000.0) * 3;
            // cout << "start_time = " << start_time << ", loaded = " << truck.loaded << "\n";

            if (truck.owning == 1){
                start_time += 33.0;
            }
            else {
                start_time += 6.0;
            }
        }
        else {                                                             // считаем конечное время
            start_time = H;
            if (truck.owning == 1) {
                start_time -= station.time_from_depot;    // возвращаемся в депо после рейса
            }
        }

        vector<Filling> tmp = find_routes(
            move(initial_route), 
            {start_time},
            stations,
            seen_routes,
            truck,
            gl_num,
            local_index,
            time_to_station, 
            start_time, 
            st_in_trip, 
            top_nearest, 
            H,
            start_shifted
        );
        final_vector.insert(final_vector.end(), tmp.begin(), tmp.end());
    }

    return final_vector;
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

string roundN_str(double val, int n) {

    double factor = pow(10.0, n);
    double rounded = round(val * factor) / factor;

    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", n, rounded);

    // обрезаем нули
    char* end = buf + strlen(buf) - 1;
    while (end > buf && *end == '0') end--;
    if (*end == '.') end--;        // если осталась одна точка
    *(end + 1) = '\0';

    // если целое — добавляем .0
    if (strchr(buf, '.') == nullptr)
        return string(buf) + ".0";

    return string(buf);
}



inline void log_time(const string& message, vector<string>& time_log) {
    time_log.emplace_back(message); 
}

// vibecoding
bool unload_is_prefix(
    const vector<int>& perm,
    const unordered_map<int, vector<pair<int, vector<int>>>>& st_to_rescomps
    ) {
        auto has_unload = [&](int st) -> bool {
            auto it = st_to_rescomps.find(st);
            if (it == st_to_rescomps.end()) return false;
            for (const auto& [res_id, comps] : it->second) {
                if (!comps.empty()) return true;
            }
            return false;
        };

        bool unloading_started = false;  // уже была выгрузка хотя бы на одной станции
        bool unloading_ended   = false;  // встретили станцию без выгрузки ПОСЛЕ начала выгрузки

        for (int st : perm) {
            const bool unload_here = has_unload(st);

            if (unload_here) {
                unloading_started = true;
                if (unloading_ended) {
                    // выгрузка снова появилась после "паузы" -> пропуск
                    return false;
                }
            } else {
                // если выгрузка уже началась, то первая пустая станция завершает "префикс"
                if (unloading_started) unloading_ended = true;
            }
        }
        return true;
    }

// вычисление длительности маршрута 
pair<double, vector<string>> compute_time_for_route(
    const map<int, pair<int, int>>& reverse_index,
    const vector<double>& compartments, 
    bool loaded,
    const Filling& fill,
    bool double_piped,
    const vector<Station>& input_station_list,
    const vector<vector<double>>& demanded_matrix,
    const vector<double>& docs_fill,
    const int owned
    ){
    double pour_time = (accumulate(compartments.begin(), compartments.end(), 0.0) / 1000.0) * 3;      // время заполнения бензовоза в депо
   
    // находим номера резервуаров, заполняемых на этом маршруте
    // vector<int> idx {};
    // for (const auto& [res_id, comps] : fill.res_comps) {
    //     idx.push_back(res_id);
    // }

    // // теперь для тех резервуаров, которые реально заполняются, найдём номера их станций
    // set<int> tmp;                        // множество уникальных элементов
    // for (int i : idx) {
    //     tmp.insert(reverse_index.at(i).first);  
    // }

    

    const vector<int>& ordered_st = fill.stations_ordered;     // вектор посещаемых станций


    unordered_map<int, vector<pair<int, vector<int>>>> st_to_rescomps;     // [ (res1, [..]), (res2, [..]), ... ]

    for (const auto& [res_id, comps] : fill.res_comps) {
        int st = reverse_index.at(res_id).first;
        st_to_rescomps[st].push_back({res_id, comps});
    }

    // TODO: если отсеки выгружаются на второй станции и не выгружаются на первой или выгружаются на последней но не выгружаются на первой и второй то мы пропускаем комбинацию

    const vector<int>& perm = fill.stations_ordered;
 
    if (!unload_is_prefix(perm, st_to_rescomps)) { return {10000, {}}; }

    // // первый отсек должен выгружаться на последней станции
    // if (st_to_rescomps[ordered_st[0]]){   // если первый отсек выгружается не на последней станции, пропускаем комбинацию
    //     continue;
    // }

    vector<string> time_log {};
    double time = 0;
    if (owned == 1) {
        time += 33.6;
    } else {
        time += 6;
    }

    // заполнение документов на каждой из станций
    for (int stn_n : ordered_st) {
        time += docs_fill[stn_n];
        log_time(roundN_str(docs_fill[stn_n], 3) + " минут - время заполнения документов на станции " + to_string(stn_n), time_log);
    }

    // если только один рукав, знаем времена
    if (!double_piped){
        if (!loaded) {
            time += 2 * roundN(pour_time, 3);    // время на заполнение бензовоза (до рейса и во время)
            log_time(roundN_str(2 * pour_time, 3) + " минут - заполнение резервуаров одним шлангом на станциях и в депо", time_log);
        }
        else {
            time += roundN(pour_time, 3);    // время на заполнение бензовоза (во время рейса)
            log_time(roundN_str(pour_time, 3) + " минут - заполнение резервуаров одним шлангом на станциях", time_log);
        }
        
    }
    else {      // двушланговый, пытаемся быстрее разгрузить
        for (int st : perm) {
            auto it = st_to_rescomps.find(st);
            if (it == st_to_rescomps.end()) {
                // На этой станции нет резервуаров для выгрузки — просто пропускаем разгрузку
                log_time("0 минут - на станции " + to_string(st) + " нет выгрузки", time_log);
                continue;
            }
            const auto& res_list = it->second;

            // если вдруг есть ключ, но список пустой
            if (res_list.empty()) {
                log_time("0 минут - на станции " + to_string(st) + " нет выгрузки", time_log);
                continue;
            }

            size_t total_comps = 0;
            for (const auto& [res_id, comps] : res_list) total_comps += comps.size();

            if (total_comps > 1) {   // если на станции выгружается больше одного отсека
                vector<double> reservoir_fill_values {}; 
                for (const auto& [res_id, vector_comps] : res_list){   // беру строчки - комбинации отсеков для каждого резервуара
                    double local_sum = 0;
                    for (int comp_number : vector_comps) {                          // итерируюсь по каждой цифре - номеру отсека
                        local_sum += compartments[comp_number] / 1000.0 * 3.0;
                    }
                    reservoir_fill_values.push_back(local_sum);
                }
                double optimal_filling_time = roundN(two_pipes_opt(reservoir_fill_values), 3);
                time += optimal_filling_time;
                log_time(roundN_str(optimal_filling_time,3) + " минут - заполнение резервуаров двумя шлангами на станции " + to_string(st), time_log);
            }
            else {
                double optimal_filling_time = roundN(compartments[res_list[0].second[0]] / 1000.0 * 3.0, 3);
                time += optimal_filling_time;
                log_time(roundN_str(optimal_filling_time,3) + " минут - заполнение одного резервуара одним шлангом на станции " + to_string(st), time_log);
            }
        }
        if (!loaded) {
            time += roundN(pour_time, 3);
            log_time(roundN_str(pour_time,3) + "минут - заполнение резервуаров одним шлангом в депо", time_log);
        }
    }

    int first = perm[0];                                      // достали станцию
    time += input_station_list[first].time_from_depot;          // доехали до нее от депо
    log_time(roundN_str(input_station_list[first].time_from_depot, 3) + " минут - время от депо до станции " + to_string(first), time_log);  

    for (int i = 0; i + 1 < perm.size(); i++) {            // ездим между станциями
        int curr_station = perm[i];
        int next_station = perm[i + 1];
        
        time += demanded_matrix[curr_station][next_station];
        log_time(roundN_str(demanded_matrix[curr_station][next_station],3) + " минут - время от станции " + to_string(curr_station) + " до станции " + to_string(next_station), time_log);
    }

    
    if (owned == 1) {
        int last_st = perm.back();
        time += input_station_list[last_st].time_to_depot;    // возвращаемся в депо
        log_time(roundN_str(input_station_list[last_st].time_to_depot, 3) + " минут - время от станции " + to_string(last_st) + " до депо", time_log);
    }
    
    return {time, vector<string>{}};  // time_log
}

