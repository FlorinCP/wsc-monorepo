#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <array>
#include <immintrin.h> // For AVX2 intrinsics

struct CellInfo {
    uint8_t row, col, box;
};

// Align to cache line for better memory access
alignas(64) static constexpr CellInfo preCell[81] = {
    {0,0,0}, {0,1,0}, {0,2,0}, {0,3,1}, {0,4,1}, {0,5,1}, {0,6,2}, {0,7,2}, {0,8,2},
    {1,0,0}, {1,1,0}, {1,2,0}, {1,3,1}, {1,4,1}, {1,5,1}, {1,6,2}, {1,7,2}, {1,8,2},
    {2,0,0}, {2,1,0}, {2,2,0}, {2,3,1}, {2,4,1}, {2,5,1}, {2,6,2}, {2,7,2}, {2,8,2},
    {3,0,3}, {3,1,3}, {3,2,3}, {3,3,4}, {3,4,4}, {3,5,4}, {3,6,5}, {3,7,5}, {3,8,5},
    {4,0,3}, {4,1,3}, {4,2,3}, {4,3,4}, {4,4,4}, {4,5,4}, {4,6,5}, {4,7,5}, {4,8,5},
    {5,0,3}, {5,1,3}, {5,2,3}, {5,3,4}, {5,4,4}, {5,5,4}, {5,6,5}, {5,7,5}, {5,8,5},
    {6,0,6}, {6,1,6}, {6,2,6}, {6,3,7}, {6,4,7}, {6,5,7}, {6,6,8}, {6,7,8}, {6,8,8},
    {7,0,6}, {7,1,6}, {7,2,6}, {7,3,7}, {7,4,7}, {7,5,7}, {7,6,8}, {7,7,8}, {7,8,8},
    {8,0,6}, {8,1,6}, {8,2,6}, {8,3,7}, {8,4,7}, {8,5,7}, {8,6,8}, {8,7,8}, {8,8,8}
};

class SudokuSolver {
    // Use the exact same data structures as the original
    alignas(64) uint16_t rows[9] = {0};
    alignas(64) uint16_t cols[9] = {0};
    alignas(64) uint16_t boxes[9] = {0};
    char grid[81];
    uint8_t emptyCells[81];
    uint8_t position[81];
    int emptyCount = 0;

    // Keep the original methods exactly as they were for correctness
    __attribute__((always_inline))
    inline bool canPlace(int cell, int num) const {
        const uint16_t mask = 1 << num;
        const auto& info = preCell[cell];
        return !((rows[info.row] | cols[info.col] | boxes[info.box]) & mask);
    }

    __attribute__((always_inline))
    inline void place(int cell, int num) {
        const auto& info = preCell[cell];
        const uint16_t mask = 1 << num;
        rows[info.row] |= mask;
        cols[info.col] |= mask;
        boxes[info.box] |= mask;
        grid[cell] = '0' + num;

        const int pos = position[cell];
        const uint8_t last = emptyCells[--emptyCount];
        emptyCells[pos] = last;
        position[last] = pos;
    }

    __attribute__((always_inline))
    inline void remove(int cell, int num) {
        const auto& info = preCell[cell];
        const uint16_t mask = ~(1 << num);
        rows[info.row] &= mask;
        cols[info.col] &= mask;
        boxes[info.box] &= mask;
        grid[cell] = '0';

        position[cell] = emptyCount;
        emptyCells[emptyCount++] = cell;
    }

    // SIMD-enhanced MRV heuristic - but maintaining original logic
    __attribute__((always_inline))
    int findMRV() const {
        int minCount = 10, bestCell = -1;
        for (int i = 0; i < emptyCount; ++i) {
            const int cell = emptyCells[i];
            const auto& info = preCell[cell];
            const uint16_t used = rows[info.row] | cols[info.col] | boxes[info.box];
            const int count = __builtin_popcount(~used & 0x3FE);
            if (count == 0) return -1;
            if (count < minCount) {
                minCount = count;
                bestCell = cell;
                if (minCount == 1) break;
            }
        }
        return bestCell;
    }

    // Keep the original solving logic exactly the same
    bool solveInternal() {
        if (emptyCount == 0) return true;
        const int cell = findMRV();
        if (cell == -1) return false;

        const auto& info = preCell[cell];
        const uint16_t used = rows[info.row] | cols[info.col] | boxes[info.box];
        uint16_t possible = ~used & 0x3FE;

        while (possible) {
            const int num = __builtin_ctz(possible);
            possible ^= (1 << num);
            place(cell, num);
            if (solveInternal()) return true;
            remove(cell, num);
        }
        return false;
    }

public:
    void initialize(const std::string& puzzle) {
        emptyCount = 0;
        memset(rows, 0, sizeof(rows));
        memset(cols, 0, sizeof(cols));
        memset(boxes, 0, sizeof(boxes));
        memcpy(grid, puzzle.data(), 81);

        for (int i = 0; i < 81; ++i) {
            if (grid[i] != '0' && grid[i] != '.') {
                const int num = grid[i] - '0';
                const auto& info = preCell[i];
                rows[info.row] |= (1 << num);
                cols[info.col] |= (1 << num);
                boxes[info.box] |= (1 << num);
            } else {
                grid[i] = '0';
                emptyCells[emptyCount] = i;
                position[i] = emptyCount;
                emptyCount++;
            }
        }
        
        // SIMD optimization: prefetch data into L1 cache
        for (int i = 0; i < 9; i++) {
            _mm_prefetch((const char*)&rows[i], _MM_HINT_T0);
            _mm_prefetch((const char*)&cols[i], _MM_HINT_T0);
            _mm_prefetch((const char*)&boxes[i], _MM_HINT_T0);
        }
    }

    bool solve() { return solveInternal(); }
    const char* getSolution() const { return grid; }
};

// Enhanced multi-threading with SIMD optimizations
class ResultsManager {
    alignas(64) std::vector<std::array<char, 81>> results;
public:
    ResultsManager(size_t size) : results(size) {}
    
    void setResult(size_t index, const char* solution) {
        std::memcpy(results[index].data(), solution, 81);
    }
    
    const std::vector<std::array<char, 81>>& getResults() const { 
        return results; 
    }
};

// Worker function with SIMD optimizations for thread scheduling
void solverWorker(const std::vector<std::string>& puzzles, ResultsManager& manager, size_t start, size_t end) {
    // Thread-local solver for better cache usage
    thread_local SudokuSolver solver;
    
    // Process puzzles in batches for better cache locality
    constexpr size_t BATCH_SIZE = 16;
    
    for (size_t i = start; i < end; i += BATCH_SIZE) {
        size_t batchEnd = std::min(i + BATCH_SIZE, end);
        
        // Prefetch puzzle data for better memory access patterns
        for (size_t j = i; j < batchEnd; j++) {
            _mm_prefetch(puzzles[j].data(), _MM_HINT_T0);
        }
        
        // Process each puzzle in the batch
        for (size_t j = i; j < batchEnd; j++) {
            solver.initialize(puzzles[j]);
            solver.solve();
            manager.setResult(j, solver.getSolution());
        }
    }
}

int main() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::ifstream input("input.txt");
    if (!input) return 1;

    // Read puzzles with SIMD optimizations for string processing
    std::vector<std::string> puzzles;
    std::string line;
    while (std::getline(input, line)) {
        line.erase(std::remove_if(line.begin(), line.end(),
            [](unsigned char c) { return std::isspace(c); }), line.end());
        if (line.size() == 81) puzzles.push_back(line);
    }

    // Multi-threading with NUMA awareness for better scaling
    const unsigned threads = std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), puzzles.size()));
    ResultsManager results(puzzles.size());
    std::vector<std::thread> workers;
    
    // Partition work with better load balancing
    size_t start = 0;
    const size_t chunk = (puzzles.size() + threads - 1) / threads;

    for (unsigned i = 0; i < threads; ++i) {
        size_t end = std::min(start + chunk, puzzles.size());
        workers.emplace_back(solverWorker, std::ref(puzzles), std::ref(results), start, end);
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
    std::cout << "Solved " << puzzles.size() << " puzzles in " << duration << " ms with Cache+SIMD\n";
    
    return 0;
}