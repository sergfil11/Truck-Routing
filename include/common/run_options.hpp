#pragma once

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

struct RunOptions {
    std::filesystem::path input = "data/16_depot.json";
    std::filesystem::path output;
    bool help = false;
};

inline RunOptions parse_run_options(int argc, char* argv[], const char* output) {
    RunOptions options;
    options.output = output;
    if (argc == 2 && (std::string(argv[1]) == "--help" ||
                      std::string(argv[1]) == "-h")) {
        options.help = true;
        std::cout << "Usage: " << argv[0] << " [input.json] [output.txt]\n"
                  << "Defaults: " << options.input << " -> " << options.output << '\n';
        return options;
    }
    if (argc > 3) throw std::runtime_error("Expected [input.json] [output.txt]; use --help.");
    if (argc > 1) options.input = argv[1];
    if (argc > 2) options.output = argv[2];
    if (options.input.extension() != ".json") options.input += ".json";
    if (!std::filesystem::is_regular_file(options.input)) {
        throw std::runtime_error("Input JSON not found: " + options.input.string());
    }
    if (std::filesystem::exists(options.output) &&
        std::filesystem::equivalent(options.input, options.output)) {
        throw std::runtime_error("Input and output must be different files.");
    }
    if (options.output.has_parent_path()) {
        std::filesystem::create_directories(options.output.parent_path());
    }
    return options;
}

// Restore cout before the output file is destroyed, including on exceptions.
class ScopedOutput {
public:
    explicit ScopedOutput(std::ostream& output) : previous_(std::cout.rdbuf(output.rdbuf())) {}
    ~ScopedOutput() { std::cout.rdbuf(previous_); }
    ScopedOutput(const ScopedOutput&) = delete;
    ScopedOutput& operator=(const ScopedOutput&) = delete;
private:
    std::streambuf* previous_;
};
