#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

// Class to manage results from multiple threads
class ResultsManager {
private:
    std::vector<std::string> results;
    std::mutex mutex;

public:
    ResultsManager(size_t size) : results(size) {}

    void setResult(size_t index, const std::string& result) {
        std::lock_guard<std::mutex> lock(mutex);
        results[index] = result;
    }

    const std::vector<std::string>& getResults() const {
        return results;
    }
};

// Check if a sudoku is valid
bool isValidSudoku(const std::string& puzzle) {
    if (puzzle.size() != 81) return false;

    // Check each row
    for (int row = 0; row < 9; row++) {
        bool used[10] = {false};
        for (int col = 0; col < 9; col++) {
            char digit = puzzle[row * 9 + col];
            if (digit < '1' || digit > '9') return false;
            int num = digit - '0';
            if (used[num]) return false;
            used[num] = true;
        }
    }

    // Check each column
    for (int col = 0; col < 9; col++) {
        bool used[10] = {false};
        for (int row = 0; row < 9; row++) {
            char digit = puzzle[row * 9 + col];
            int num = digit - '0';
            if (used[num]) return false;
            used[num] = true;
        }
    }

    // Check each 3x3 box
    for (int boxRow = 0; boxRow < 3; boxRow++) {
        for (int boxCol = 0; boxCol < 3; boxCol++) {
            bool used[10] = {false};
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    int r = boxRow * 3 + row;
                    int c = boxCol * 3 + col;
                    char digit = puzzle[r * 9 + c];
                    int num = digit - '0';
                    if (used[num]) return false;
                    used[num] = true;
                }
            }
        }
    }

    return true;
}

// Worker function for validating sudokus in parallel
void validatorWorker(const std::vector<std::string>& puzzles, ResultsManager& results, size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
        bool valid = isValidSudoku(puzzles[i]);
        results.setResult(i, valid ? "Valid" : "Invalid");
    }
}

int main() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::ifstream input("output.txt");
    if (!input) {
        std::cerr << "Error: Cannot open input file." << std::endl;
        return 1;
    }

    std::vector<std::string> puzzles;
    std::string line;
    while (std::getline(input, line)) {
        line.erase(std::remove_if(line.begin(), line.end(),
            [](unsigned char c) { return std::isspace(c); }), line.end());
        if (line.size() == 81) puzzles.push_back(line);
    }

    if (puzzles.empty()) {
        std::cerr << "No valid puzzles found in input file." << std::endl;
        return 1;
    }

    const unsigned threads = std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), puzzles.size()));
    ResultsManager results(puzzles.size());
    std::vector<std::thread> workers;
    size_t start = 0;
    const size_t chunk = (puzzles.size() + threads - 1) / threads;

    for (unsigned i = 0; i < threads; ++i) {
        size_t end = std::min(start + chunk, puzzles.size());
        workers.emplace_back(validatorWorker, std::ref(puzzles), std::ref(results), start, end);
        start = end;
    }

    for (auto& t : workers) t.join();

    // Count valid and invalid puzzles
    int validCount = 0, invalidCount = 0;
    for (const auto& result : results.getResults()) {
        if (result == "Valid") validCount++;
        else invalidCount++;
    }

    // Write results to output file
    std::ofstream output("validation_results.txt");
    for (size_t i = 0; i < results.getResults().size(); ++i) {
        output << "Puzzle " << (i + 1) << ": " << results.getResults()[i] << '\n';
    }

    // Write summary to output file
    output << "\nSummary:\n";
    output << "Valid puzzles: " << validCount << '\n';
    output << "Invalid puzzles: " << invalidCount << '\n';

    // Calculate and display execution time
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime).count();
    std::cout << "Validated " << puzzles.size() << " puzzles in " << duration << " ms\n";
    std::cout << "Valid puzzles: " << validCount << "\n";
    std::cout << "Invalid puzzles: " << invalidCount << "\n";

    return 0;
}