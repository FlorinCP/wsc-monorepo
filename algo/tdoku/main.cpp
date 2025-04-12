#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <array>

// Declare the function prototype
extern "C" size_t TdokuSolverDpllTriadSimd(const char* puzzle, size_t limit,
                                          uint32_t configuration,
                                          char* solution, size_t* num_guesses);

class ResultsManager {
    std::vector<std::array<char, 81>> results;
public:
    ResultsManager(size_t size) : results(size) {}
    void setResult(size_t index, const char* solution) {
        std::memcpy(results[index].data(), solution, 81);
    }
    const std::vector<std::array<char, 81>>& getResults() const { return results; }
};

void optimizedSolverWorker(const std::vector<std::string>& puzzles, ResultsManager& manager, size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
        char solution[81];
        size_t num_guesses = 0;
        size_t result = TdokuSolverDpllTriadSimd(puzzles[i].c_str(), 1, 1, solution, &num_guesses);
        if (result == 0) {
            std::cout << "Failed to solve puzzle " << i << ": " << puzzles[i].substr(0, 20) << "..." << std::endl;
        }
        manager.setResult(i, solution);
    }
}

int main() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::ifstream input("../input.txt");
    if (!input) return 1;

    std::vector<std::string> puzzles;
    std::string line;
    while (std::getline(input, line)) {
        line.erase(std::remove_if(line.begin(), line.end(),
            [](unsigned char c) { return std::isspace(c); }), line.end());
        if (line.size() == 81) puzzles.push_back(line);
    }

    const unsigned threads = std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), puzzles.size()));
    ResultsManager results(puzzles.size());
    std::vector<std::thread> workers;
    size_t start = 0;
    const size_t chunk = (puzzles.size() + threads - 1) / threads;

    for (unsigned i = 0; i < threads; ++i) {
        size_t end = std::min(start + chunk, puzzles.size());
        workers.emplace_back(optimizedSolverWorker, std::ref(puzzles), std::ref(results), start, end);
        start = end;
    }

    for (auto& t : workers) t.join();

    std::ofstream output("output.txt");
    for (const auto& s : results.getResults()) {
        output.write(s.data(), s.size());
        output << '\n';
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime).count();
    std::cout << "Solved " << puzzles.size() << " puzzles in " << duration << " ms\n";
}