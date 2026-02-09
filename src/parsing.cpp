#pragma once
#include <fstream>
#include <iostream>
#include "json.hpp"
#include "parsing.hpp"
#include "prep_functions.hpp"


using json = nlohmann::json;
using namespace std;


RealData load_real_data(const string& name) {
    string full_path = name + ".json";
    cout << "Trying to open file: " << full_path << endl;

    ifstream f(full_path);
    if (!f.is_open()) {
        cerr << "Cannot open file!" << endl;
        return {};
    }
    else {
        cout << "File opened successfully!" << endl;
    }
    
    json data;
    f >> data;

    cout << "Data read successfully!" << endl;

    RealData result;
    result.N = data["N"];
    result.H = data["H"];
    result.K = data["K"];
    result.time_matrix = data["time_matrix"].get<vector<vector<double>>>();
    result.depot_times = data["depot_times"].get<vector<map<string, double>>>();
    result.stations = data["stations"].get<vector<map<string, vector<double>>>>();
    result.trucks = data["trucks"].get<vector<vector<double>>>();
    result.access = data["access"].get<vector<vector<int>>>();
    result.dual_piped = data["dual_piped"].get<vector<bool>>();
    result.daily_coefficient = data["daily_coefficient"];
    result.docs_fill = data["docs_fill"].get<vector<double>>();
    result.H_k = data["H_k"].get<vector<double>>();
    result.loading_prepared = data["loading_prepared"].get<vector<bool>>();
    result.owning = data["owning"].get<vector<int>>();
    result.reservoir_to_product = data["reservoir_to_product"].get<map<string, vector<string>>>();
    result.truck_to_variants = data["truck_to_variants"].get<map<string, set<vector<string>>>>();
    result.is_critical = data["is_critical"].get<vector<int>>();
    result.consumption_percent = data["consumption_percent"].get<vector<vector<vector<double>>>>();
    result.consumption = data["consumption"].get<vector<vector<double>>>();
    result.starting_time = data["starting_time"].get<vector<double>>();

    cout << "Data loaded successfully!" << endl;
    return result;
}



void print_input(const RealData& d) {
    cout << "N = " << d.N << "\n";
    cout << "H = " << d.H << "\n";
    cout << "K = " << d.K << "\n";

    cout << "\nTime matrix:\n";
    for (auto& row : d.time_matrix) {
        for (auto v : row) cout << v << " ";
        cout << "\n";
    }

    cout << "\nDepot times:\n";
    for (auto v : d.depot_times) {
        for (auto p : v) cout << p.first << ":" << p.second << " ";   
    }
    cout << "\n";

    cout << "\nStations:\n";
    for (auto& st : d.stations) {
        for (auto v : st){ cout << v.first << ":";
            for (auto v2 : v.second) cout << v2 << " ";
        }
        cout << "\n";
    }

    cout << "\nTrucks:\n";
    for (auto& tr : d.trucks) {
        for (auto v : tr) cout << v << " ";
        cout << "\n";
    }

    cout << "\nAccess:\n";
    for (auto& row : d.access) {
        for (auto v : row) cout << v << " ";
        cout << "\n";
    }

    cout << "\nDual piped:\n";
    for (int i = 0; i < d.dual_piped.size(); ++i) {
        if (d.dual_piped[i] == true) cout << "Truck "<< i << " is dualpiped\n";
    }
    cout << "\n";

    cout << "\nDaily coefficient = " << d.daily_coefficient << "\n";

    cout << "\nDocs fill:\n";
    for (auto v : d.docs_fill) cout << v << " ";
    cout << "\n";

    cout << "\nH_k:\n";
    for (auto v : d.H_k) cout << v << " ";
    cout << "\n";

    cout << "\nLoading prepared:\n";
    for (int i = 0; i < d.loading_prepared.size(); ++i) {
        if (d.loading_prepared[i] == true) cout << "Truck "<< i << " is prepared\n";
    }
    cout << "\n";
 
    cout << "\nOwning:\n";
    for (int i = 0; i < d.owning.size(); ++i) {
        if (d.owning[i] == 0) cout << "Truck "<< i << " is not owning\n";
    }
    cout << "\n";

    cout << "\nCritical:\n";
    for (int i = 0; i < d.is_critical.size(); ++i) {
        if (d.is_critical[i] == 1) cout << "Reservoir "<< i << " is critical\n";
    }
    cout << "\n";

   // Reservoir to product - конвертируем строки в int для сортировки
    cout << "\nReservoir to product:\n";
    map<int, vector<string>> temp_reservoirs;
    for (auto& [reservoir_str, products] : d.reservoir_to_product) {
        try {
            int reservoir_int = stoi(reservoir_str);
            temp_reservoirs[reservoir_int] = products;
        } catch (const exception& e) {
            cerr << "Warning: reservoir key is not a number: " << reservoir_str << endl;
        }
    }

    for (auto& [station, products] : temp_reservoirs) {
        cout << "Station " << station << ": ";
        for (auto& product : products) cout << product << " ";
        cout << "\n";
    }

    cout << "\nConsumption percents:\n";
    int c = 0;
    for (auto& st : d.consumption_percent) {
        cout << "Station " << c++ << ": \n";
        for (auto& res : st){
            for (auto v : res) cout << (roundN(v, 2) * 100.0) << "% ";
            cout << "\n";
        }
    }

    cout << "\nConsumption:\n";
    c = 0;
    for (auto& row : d.consumption) {
        cout << "Station " << c++ << ": ";
        for (auto v : row) cout << v << " ";
        cout << "\n";
    }

    cout << "\nStarting time:\n";
    c = 0;
    for (auto v : d.starting_time) {
       cout << "Truck " << c++ << ": " << v << " minutes\n";
    }
    cout << "\n";
    
}