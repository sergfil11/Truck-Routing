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
    const vector<double>& starting_time,
    const vector<int>& owning
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
  

  // вектор результатов для каждого бензовоза
  vector<vector<Filling>> routes_by_truck(trucks.size());
  vector<vector<Filling>> routes_by_truck_shifted(trucks.size());

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
      

      // генерируем сначала обычные рейсы, которые начинаются с начала смены, а затем рейсы, прижатые к концу смены
      // для параметров (1..R1)
      for (int r = 1; r < R1+1; r++) {
          vector<Filling> val = all_fillings(accessible_st, Truck(idx, truck, starting_time[idx], loading_prepared[idx], owning[idx]), accessible_matrix, gl_num, H_k[idx], r, R2, local_index, true);
          vector<Filling> shifted_val = all_fillings(accessible_st, Truck(idx, truck, starting_time[idx], loading_prepared[idx], owning[idx]), accessible_matrix, gl_num, H_k[idx], r, R2, local_index, false);

          // нужно проверить, можно ли эти заполнения использовать для текущего бензовоза, для этого:
          // 1. Для каждого заполнения берём номера используемых резервуаров в глобальной нумерации
          //    А затем конвертируем их в используемые виды топлива
          // 2. Берём доступные схемы загрузки бензовоза и проверяем, есть ли схема загрузки с такими видами топлива среди доступных
          // 3. Если есть, то берём заполнение и добавляем его в результат

          const set<vector<string>>& allowed_schemes = truck_to_variants.at(to_string(idx));
          //cout << "Количество доступных схем загрузки для бензовоза " << idx << ": " << allowed_schemes.size() << endl;

          for (Filling& filling : val) {
              vector<string> converted_filling = convert_compartments(truck.size(), filling, gl_res_to_product);    // по заполнению строим схему загрузки
              if (allowed_schemes.find(converted_filling) != allowed_schemes.end()) {                               // если она доступна, добавляем заполнение в результат
                  routes_by_truck[idx].push_back(move(filling));
              }
          }

          for (Filling& shifted_filling : shifted_val) {
              vector<string> converted_filling = convert_compartments(truck.size(), shifted_filling, gl_res_to_product);    // по заполнению строим схему загрузки
              if (allowed_schemes.find(converted_filling) != allowed_schemes.end()) {                                       // если она доступна, добавляем заполнение в результат
                  routes_by_truck_shifted[idx].push_back(move(shifted_filling));
              }
          }

          // TODO: правильнее не первый попавшийся индекс брать, а тот, который пустой в БОЛЬШЕМ числе схем для этого бензовоза
          // Рассмотрели варианты с заполнением всего бензовоза, а теперь попробуем реализовать схему с пропуском отсека
          // энивей, пока что мы пропускаем не больше одного отсека (и всегда пропускаем один и тот же по номеру)
          bool empty_comp_gen = false;
          int comp_n = -1;
          for (const vector<string>& scheme : allowed_schemes) {
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
          
            vector<Filling> val = all_fillings(accessible_st, Truck(idx, truck_cut, starting_time[idx], loading_prepared[idx], owning[idx]), accessible_matrix, gl_num, H_k[idx], r, R2, local_index, true);
            vector<Filling> shifted_val = all_fillings(accessible_st, Truck(idx, truck_cut, starting_time[idx], loading_prepared[idx], owning[idx]), accessible_matrix, gl_num, H_k[idx], r, R2, local_index, false);
            //TODO:


            for (Filling& filling : val) {
              // по заполнению строим схему загрузки
              vector<string> converted_filling = convert_compartments(truck_cut.size(), filling, gl_res_to_product);
              // теперь нужно в пропущенный отсек схемы загрузки поставить '', то есть пропуск, чтобы соответствовать схеме
              converted_filling.insert(converted_filling.begin() + comp_n, "");
              // если она доступна, добавляем заполнение в результат
              if (allowed_schemes.find(converted_filling) != allowed_schemes.end()) {
                  routes_by_truck[idx].push_back(move(filling));
              }
            }
            for (Filling& shifted_filling : shifted_val) {
              vector<string> converted_filling = convert_compartments(truck_cut.size(), shifted_filling, gl_res_to_product);
              converted_filling.insert(converted_filling.begin() + comp_n, "");
              if (allowed_schemes.find(converted_filling) != allowed_schemes.end()) {
                  routes_by_truck_shifted[idx].push_back(move(shifted_filling));
              }
            }

          }
      }
      // cout << "start_sh: " <<  local_results[idx].size() << " finish_sh: " << local_results_shifted[idx].size() << endl;
  }


  auto t2 = chrono::system_clock::now();
  cout << "Время вычисления маршрутов " << roundN(chrono::duration<double>(t2 - t1).count(), 3) << " сек." << endl;
  size_t total_start = 0;
  for (const auto& v : routes_by_truck) total_start += v.size();
  size_t total_end = 0;
  for (const auto& v : routes_by_truck_shifted) total_end += v.size();
  cout << "Маршрутов, прижатых к началу смены: " << total_start << endl;
  cout << "Маршрутов, прижатых к концу смены: " << total_end << endl;



  // // заполнение резервуаров (в глобальной нумерации) бензовозами на маршрутах
  // map<pair<bool, int>, vector<vector<string>>> filling_on_route = {};                      // (n_truck, start_shifted) : truck_fillings, start_shifted [bool] - прижатый к началу/концу смены
  // for (pair<int, vector<string>> elem : result) {                 
  //   filling_on_route[pair(true, elem.first)].push_back(elem.second);
  // }
  // for (pair<int, vector<string>> elem : result_shifted) {                 
  //   filling_on_route[pair(false, elem.first)].push_back(elem.second);
  // }
 
  // fiiling_on_route[pair(st_sh, n_truck)] = routes_by_truck[n_truck] or routes_by_truck_shifted[n_truck] if st_sh = true or false, respectively


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

  for (int truck = 0; truck < K; ++truck) {
    if (routes_by_truck[truck].size() > 0) {
      cout << "Количество прижатых к старту разгрузок для бензовоза " << truck + 1 << ": " << routes_by_truck[truck].size() << endl;
    }
    if (routes_by_truck_shifted[truck].size() > 0) {
      cout << "Количество прижатых к финишу разгрузок для бензовоза " << truck + 1 << ": " << routes_by_truck_shifted[truck].size() << endl;
    }
  }

  for (bool start_shifted : {true, false}) {
    #pragma omp parallel for schedule(guided)
    for (int truck = 0; truck < K; ++truck) {
      auto& routes = start_shifted ? routes_by_truck[truck] : routes_by_truck_shifted[truck];
      for (int route = 0; route < routes.size(); ++route) {
          Filling& fill = routes[route];

          auto [computed_time, timelog] = compute_time_for_route(
              reverse_global,
              trucks[truck],
              loading_prepared[truck],
              fill,
              double_piped[truck],
              input_station_list,
              demanded_matrix,
              docs_fill, 
              owning[truck]
          );
          
          fill.timelog = timelog;
          fill.total_time = computed_time;
      }
    }
  }

  vector<vector<map<vector<int>, pair<double, Filling>>>> best_by_pattern(K,vector<map<vector<int>, pair<double, Filling>>>(2));

  for (bool start_shifted : {true, false}) {
    #pragma omp parallel for schedule(static)
    for (int truck = 0; truck < K; ++truck) {

      const vector<Filling>& routes = start_shifted
          ? routes_by_truck[truck]
          : routes_by_truck_shifted[truck];

      map<vector<int>, pair<double, Filling>>& best_map = best_by_pattern[truck][start_shifted ? 1 : 0];

      for (int route_idx = 0; route_idx < (int)routes.size(); ++route_idx) {
        const Filling& f = routes[route_idx];

        vector<int> pattern = make_pattern(f);
        double t = f.total_time;

        auto it = best_map.find(pattern);
        if (it == best_map.end()) {
            best_map.emplace(std::move(pattern), std::make_pair(t, f));
        } else if (t < it->second.first) {
            it->second = std::make_pair(t, f);
        }
      }
    }
  }

  size_t total_best = 0;
  for (int k = 0; k < (int)best_by_pattern.size(); ++k) {
      for (int sh = 0; sh < (int)best_by_pattern[k].size(); ++sh) {
          total_best += best_by_pattern[k][sh].size();
      }
  }
  cout << "Лучших по паттерну маршрутов: " << total_best << endl;


  t2 = chrono::system_clock::now();
  cout << "Время вычисления длительностей:" << roundN(chrono::duration<double>(t2 - t1).count(), 3) << " сек." << endl;


  return {move(best_by_pattern), move(reservoirs), tank_count, move(gl_num), move(H_k), move(filling_times)};
}