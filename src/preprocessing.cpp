#include "prep_functions.hpp"
#include "preprocessing.hpp"
#include <numeric>
#include <iostream>
#include <chrono>
#include <omp.h>


/**
 * Предварительная обработка данных для оптимизации солвером Gurobi.
 *
 * @param N                 Число станций (не учитывая депо)
 * @param H                 Длительность смены в минутах
 * @param K                 Число бензовозов
 * @param time_matrix       Матрица длительностей поездок между станциями
 * @param depot_times       Массив длительностей поездок от станций до депо (и обратно)
 * @param stations          Минимальные и максимальные потребности станций в топливе
 * @param trucks            Вместимости отсеков бензовозов
 * @param R1                Число станций в маршруте
 * @param R2                Число рассматриваемых ближайших станций
 * @param access            Матрица допустимых станций для бензовозов
 * @param double_piped      Булев массив, указывающий на возможности использования двух шлангов
 * @param daily_coefficient Коэффициент длительности поездок между станциями
 * @param docs_fill         Время на заполнение документов на станции
 * @param H_k               Массив длительностей смен для бензовозов
 * @param loading_prepared  Массив отметок, что бензовоз загружен под сменщика
 *
 * @return tuple Кортеж, содержащий
 *         - filling_on_route  Заполнение резервуаров отсеками в глобальной нумерации различными видами топлива
 *         - sigma             Времена выполнения маршрутов
 *         - reservoirs        Минимальные и максимальные потребности резервуаров в топливе
 *         - tank_count        Общее число резервуаров
 *         - H_k               Массив длительностей смен для бензовозов (с учётом времени, добавленного в смену вледствие загрузки под сменщика после обработки loading_prepared)
 */

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
    const map<string, vector<string>>& reservoir_to_product,
    const map<string, set<vector<string>>>& truck_to_variants,
    const vector<vector<double>>& consumption,
    const vector<double>& starting_time
  ){

  // значения по умолчанию
  if (H_k.empty())
    H_k = vector<double>(K, H);
  if (access.empty())                                         
    access = vector<vector<int>> (N, vector<int>(K, 1));
  if (double_piped.empty())
      double_piped = vector<bool> (K, false); 
  if (docs_fill.empty())
      docs_fill = vector<double> (N, 2400);
  if (loading_prepared.empty())
      loading_prepared = vector<bool> (K, false);
  

  // абсолютные значения потребления
  for (int st_idx = 0; st_idx < N; ++st_idx) {
    for (int res_idx = 0; res_idx < consumption_percent[st_idx].size(); ++res_idx) {
      for (int var_idx = 0; var_idx < consumption_percent[st_idx][res_idx].size(); ++var_idx) {
        consumption_percent[st_idx][res_idx][var_idx] *= consumption[st_idx][res_idx];
      }
    }
  }

  // кумулятивные абсолютные значения потребления
  for (int st_idx = 0; st_idx < N; ++st_idx) {
    for (int res_idx = 0; res_idx < consumption_percent[st_idx].size(); ++res_idx) {
      for (int var_idx = 1; var_idx < consumption_percent[st_idx][res_idx].size(); ++var_idx) {
        consumption_percent[st_idx][res_idx][var_idx] += consumption_percent[st_idx][res_idx][var_idx - 1];
      }
    }
  }


  vector<double> filling_times(K, 0);
  // если загружен под сменщика, увеличиваем длину смены на время заполнения в депо
  for (int truck = 0; truck < K; ++truck) {
    if (loading_prepared[truck] == true) {
        double sum_truck = accumulate(trucks[truck].begin(), trucks[truck].end(), 0.0);
        double filling_time = (sum_truck / 1000.0) * 3;
        // H_k[truck] += filling_time;
        filling_times[truck] = filling_time;
    }
  }

  // применяем дневной коэффициент к матрице перемещений
  vector<vector<double>> coef_time_matrix(time_matrix.size(), vector<double>(time_matrix[0].size()));
  for (int i = 0; i < time_matrix.size(); ++i) {
    for (int j = 0; j < time_matrix[i].size(); ++j) {
        coef_time_matrix[i][j] = (time_matrix[i][j] * daily_coefficient > 900)      // если время больше 900, то оставляем 900 (итак больше длины смены)
                                  ? 900
                                  : time_matrix[i][j] * daily_coefficient;
    }
  }

  // ищем пустые станции
  vector <int> demanded_idx = {};
    for (int st_number = 0; st_number < N; st_number++) {
        if (stations[st_number].count("max") && !stations[st_number].at("max").empty())   // если есть ключ "max" и вектор по ключу "max" не пуст
            demanded_idx.push_back(st_number);
    }
    cout << "Станции с ненулевыми запросами " << demanded_idx.size() << endl;

  // "срезаем" пустые станции в матрице времён
  vector<vector<double>> demanded_matrix(demanded_idx.size(), vector<double>(demanded_idx.size()));
  for (int i = 0; i < demanded_idx.size(); ++i) {
      for (int j = 0; j < demanded_idx.size(); ++j) {
          demanded_matrix[i][j] = coef_time_matrix[demanded_idx[i]][demanded_idx[j]];
      }
  }
  
  vector<vector<int>> cut_access;
  vector<map<string, vector<double>>> demanded_st;
  vector<map<string, double>> demanded_depot_times;
  vector<vector<vector<double>>> demanded_consumptions;
  vector<double> demanded_docs_fill;

  // "срезаем" пустые станции в векторах access, stations, depot_times и consumption_percent
  for (int idx : demanded_idx) {
      cut_access.push_back(access[idx]);
      demanded_st.push_back(stations[idx]);
      demanded_depot_times.push_back(depot_times[idx]);
      demanded_consumptions.push_back(consumption_percent[idx]);
      demanded_docs_fill.push_back(docs_fill[idx]);
  }

  // TODO: добавить в станцию вектор кумулятивных потреблений резервуаров (уже обработав consumption и consumption_percent)
  // создаём вектор станций
  vector<Station> input_station_list;
  for (int i = 0; i < demanded_st.size(); ++i) {
    Station st(
        i,          // номер станции в срезе
        demanded_depot_times[i].at("to") * daily_coefficient,
        demanded_depot_times[i].at("from") * daily_coefficient,
        demanded_st[i].at("min"),
        demanded_st[i].at("max"),
        demanded_consumptions[i],
        demanded_docs_fill[i]
    );
    input_station_list.push_back(move(st));
  }

  vector<int> lengths;  // число резервуаров на станции
  for (int i = 0; i < demanded_st.size(); ++i)
    lengths.push_back(demanded_st[i].at("max").size());

  // глобальная нумерация выбранных резервуаров
  map<pair<int, int>, int> gl_num = global_numeration(lengths);

  // глобальная нумерация видов топлива в резервуарах
  map<int, string> gl_res_to_product = global_product_mapping(reservoir_to_product);
  
  auto t1 = chrono::system_clock::now();  
   
  set<pair<int, vector<string>>> result = {};

 // вектор локальных результатов для каждого треда
  vector<set<pair<int, vector<string>>>> local_results(trucks.size());

  #pragma omp parallel for schedule(static)             // лучше guided
  for (int idx = 0; idx < trucks.size(); ++idx) {
      const vector<double>& truck = trucks[idx];
      // станцию и матрицу времени нужно обрезать в случае ограничений a_ik

      // допустимые станции для бензовоза
      vector<int> accessible;
      for (auto& row : cut_access) {
          accessible.push_back(row[idx]);
      }

      // индексы допустимых станций
      vector<int> accessible_idx;
      for (size_t i = 0; i < accessible.size(); ++i) {
          if (accessible[i] == 1) {
              accessible_idx.push_back(i);
          }
      }

      // допустимые станции
      vector<Station> accessible_st;
      for (int i : accessible_idx) {
          accessible_st.push_back(input_station_list[i]);
      }
      
      // часть матрицы времени, соответствующая допустимым станциям бензовоза
      vector<vector<double>> accessible_matrix;
      for (int i : accessible_idx) {
          vector<double> row;
          for (int j : accessible_idx) {
              row.push_back(demanded_matrix[i][j]);
          }
          accessible_matrix.push_back(move(row));
      }
      
      map<int, int> local_index = {};      // сопоставление номера станции и её индекса (нумерация поданных станций)
      for (int i = 0; i < accessible_st.size(); ++i) {
          local_index[accessible_st[i].number] = i;
      }
      
      set<pair<int, vector<string>>> local_set;
      
      // для параметров (1..R1)
      for (int r = 1; r < R1+1; r++) {
          set<vector<string>> val = all_fillings(accessible_st, Truck(idx, truck, starting_time[idx], loading_prepared[idx]), accessible_matrix, gl_num, H_k[idx], r, R2, local_index);
          
          // нужно проверить, можно ли эти заполнения использовать для текущего бензовоза, для этого:
          // 1. Для каждого заполнения берём номера используемых резервуаров в глобальной нумерации
          //    А затем конвертируем их в используемые виды топлива
          // 2. Берём доступные схемы загрузки бензовоза и проверяем, есть ли схема загрузки с такими видами топлива среди доступных
          // 3. Если есть, то берём заполнение и добавляем его в результат

          const set<vector<string>>& allowed_schemes = truck_to_variants.at(to_string(idx));
          //cout << "Количество доступных схем загрузки для бензовоза " << idx << ": " << allowed_schemes.size() << endl;

          for (const vector<string>& filling : val) {
              vector<string> converted_filling = convert_compartments(truck.size(), filling, gl_res_to_product);    // по заполнению строим схему загрузки
              if (allowed_schemes.find(converted_filling) != allowed_schemes.end()) {                               // если она доступна, добавляем заполнение в результат
                  local_set.insert({idx, filling});
              }
          }

          // TODO: правильнее не первый попавшийся индекс брать, а тот, который пустой в БОЛЬШЕМ числе схем для этого бензовоза
          // Рассмотрели варианты с заполнением всего бензовоза, а теперь попробуем реализовать схему с пропуском отсека
          // энивей, пока что мы пропускаем не больше одного отсека (и всегда пропускаем один и тот же по номеру)
          bool empty_comp_gen = false;
          int comp_n = -1;
          for (vector<string> scheme : allowed_schemes) {
            for (int i = 0; i < scheme.size(); ++i) {
              if (scheme[i].empty()) {
                empty_comp_gen = true;
                comp_n = i;
                break;
              }
            }
          }
         
          if (empty_comp_gen) {
            vector<double> truck_cut = truck;              // создаём копию
            truck_cut.erase(truck_cut.begin() + comp_n);   // удаляем i-ый отсек
          
            set<vector<string>> val = all_fillings(accessible_st, Truck(idx, truck_cut, starting_time[idx], loading_prepared[idx]), accessible_matrix, gl_num, H_k[idx], r, R2, local_index);
            
            for (const vector<string>& filling : val) {
              vector<string> converted_filling = convert_compartments(truck_cut.size(), filling, gl_res_to_product);    // по заполнению строим схему загрузки

              // теперь нужно в пропущенный отсек схемы загрузки поставить '', то есть пропуск, чтобы соответствовать схеме
              converted_filling.insert(converted_filling.begin() + comp_n, "");

              if (allowed_schemes.find(converted_filling) != allowed_schemes.end()) {        // если она доступна, добавляем заполнение в результат
                  local_set.insert({idx, filling});
              }
            }

          }





          // NOTE: Версия без схем загрузки
          // for (vector<string> elem : val){
          //     local_set.insert({idx, elem});
          //     // print(elem)
          // }

          
          // print(f'Маршрутов для бензовоза {idx}: {len(val)}')
      }
      local_results[idx] = move(local_set);
  }

  // объединяем результаты всех нитей
  for (const auto& s : local_results) {
    result.insert(s.begin(), s.end());
  }

  auto t2 = chrono::system_clock::now();
  cout << "Время вычисления маршрутов:" << roundN(chrono::duration<double>(t2 - t1).count(), 3) << " сек." << endl;
  cout << "Всего маршрутов:"  << result.size() << endl;




  // заполнение резервуаров (в глобальной нумерации) бензовозами на маршрутах
  map<int, vector<vector<string>>> filling_on_route = {};                      // n_truck, truck_fillings
  for (pair<int, vector<string>> elem : result) {                 
    filling_on_route[elem.first].push_back(elem.second);
  }

  // перевод демандов станций в деманды резервуаров (проходимся по всем станциям и добавляем все их резервуары в список)
  vector<map<string, double>> reservoirs = {};        
  for (int i = 0; i < demanded_st.size(); ++i){
    map<string, vector<double>> st = demanded_st[i];
    for (int j = 0; j < st["min"].size(); ++j){
        reservoirs.push_back({{"min", st["min"][j]}, {"max", st["max"][j]}});
    }   
  }          
  int tank_count = reservoirs.size();
  
  t1 = chrono::system_clock::now(); 
  
  map<int, pair<int,int>> reverse_global;
  for (const pair<const pair<int, int>, int>& kv : gl_num) {
    reverse_global[kv.second] = kv.first;
  }
  
  map<pair<int,int>, double> sigma;
  map<pair<int,int>, vector<string>> timelogs;
  // локальные структуры для потоков
  vector<map<pair<int,int>, double>> local_sigma(K);
  vector<map<pair<int,int>, vector<string>>> local_timelogs(K);

  for (int truck = 0; truck < K; ++truck) {
    if (filling_on_route.count(truck) > 0) {
      cout << "Количество разгрузок для бензовоза " << truck << ": " << filling_on_route.at(truck).size() << endl;
    }
  }

  #pragma omp parallel for schedule(guided)
  for (int truck = 0; truck < K; ++truck) {
    if (filling_on_route.count(truck) > 0) {
        for (int route = 0; route < filling_on_route.at(truck).size(); ++route) {
            const vector<string>& fill = filling_on_route.at(truck)[route];
            auto [computed_time, timelog] = compute_time_for_route(
                reverse_global,
                trucks[truck],
                loading_prepared[truck],
                fill,
                double_piped[truck],
                input_station_list,
                demanded_matrix,
                docs_fill
            );
            local_sigma[truck][{truck, route}] = computed_time;
            local_timelogs[truck][{truck, route}] = timelog;
        }
    }
  }

  for (int truck = 0; truck < K; ++truck) {
    sigma.insert(local_sigma[truck].begin(), local_sigma[truck].end());
    timelogs.insert(local_timelogs[truck].begin(), local_timelogs[truck].end());
  }


  // локальные структуры для потоков
  vector<map<pair<int, vector<int>>, 
                     tuple<double, vector<string>, vector<string>>>> local_best(K);      
  #pragma omp parallel for schedule(static)
  for (int truck = 0; truck < K; ++truck) {                                                             
    if (filling_on_route.count(truck) > 0) {
        for (int route_idx = 0; route_idx < filling_on_route.at(truck).size(); ++route_idx) {
            const vector<string>& fill = filling_on_route[truck][route_idx];
            
            vector<int> pattern = {};
            pattern.reserve(fill.size());
            for (int i = 0; i < fill.size(); ++i) {
                pattern.push_back(fill[i].empty() ? 0 : 1);
            }

            double r_time = sigma[{truck, route_idx}];
            vector<string> r_log = timelogs[{truck, route_idx}];
            pair<int, vector<int>> key = {truck, pattern};

            auto it = local_best[truck].find(key);
            if (it == local_best[truck].end() || r_time < get<0>(it->second))
                local_best[truck][key] = {r_time, fill, r_log};

            // if (best_by_pattern.count(key) == 0 or r_time < get<0>(best_by_pattern[key]))       // если еще нет такого паттерна, или время лучше, обновляем
            //     best_by_pattern[key] = {r_time, fill, r_log};
        }
    }
  }
  
  // по паре (номер бензовоза, паттерн заполнения) возвращаем маршрут с минимальным временем, соответствующий этому паттерну (его время, заполнение и лог)
  map<pair<int, vector<int>>, tuple<double, vector<string>, vector<string>>> best_by_pattern = {};   
  
  for (const auto& lb : local_best) {
    best_by_pattern.insert(lb.begin(), lb.end());
  }

  cout << "Лучших по паттерну маршрутов:"  << best_by_pattern.size() << endl;


  //  возможно надо сначала создать ключи а потом добавлять
  map<int, vector<vector<int>>> new_filling_on_route;  // для каждого бензовоза пустой список маршрутов
  map<pair<int,int>, double> new_sigma;
  map<pair<int,int>, vector<string>> new_log;

  // перезаписываем сигму
  for (const auto& [key, value] : best_by_pattern) {
        const auto& [truck, pattern] = key;
        const auto& [best_time, fill, log] = value;

        int new_route_idx = new_filling_on_route[truck].size();
        new_filling_on_route[truck].push_back(pattern);              // можно и сам fill записать, но запишу паттерн
        new_sigma[{truck, new_route_idx}] = best_time;
        new_log[{truck, new_route_idx}] = log;
  }

  int total_routes = 0;
  for (auto& [truck, routes] : new_filling_on_route)
      total_routes += routes.size();

//   cout << "Маршрутов:" << total_routes << endl;
//   cout << "Сигм:" << new_sigma.size() << endl;
//   cout << "Лог:" << new_log.size() << endl;

  auto t5 = chrono::system_clock::now();
{
    std::map<int, vector<vector<string>>>().swap(filling_on_route);
    std::map<pair<int,int>, double>().swap(sigma);
    std::map<pair<int,int>, vector<string>>().swap(timelogs);
}

  // map<int, vector<vector<string>>>().swap(filling_on_route);
  // map<pair<int,int>, double>().swap(sigma);
  // map<pair<int,int>, int>().swap(gl_num);
  // map<pair<int,int>, vector<string>>().swap(timelogs);

  auto t6 = chrono::system_clock::now();
  cout << "Время очистки:" << roundN(chrono::duration<double>(t6 - t5).count(), 3) << " сек." << endl;


  t2 = chrono::system_clock::now();
  cout << "Время вычисления длительностей:" << roundN(chrono::duration<double>(t2 - t1).count(), 3) << " сек." << endl;


  return {move(new_filling_on_route), move(new_sigma), move(reservoirs), tank_count, move(gl_num), move(new_log), move(H_k), move(filling_times)};
}