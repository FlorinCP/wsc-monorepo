#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <array>
#include <immintrin.h>
#include <x86intrin.h>

// Advanced SIMD types and constants
namespace {
    using Vec16 = __m256i;
    using Vec8 = __m128i;
    
    constexpr uint16_t kAll = 0x1FF;
    constexpr uint16_t kNone = 0;
    
    // Precomputed lookup tables for triad operations
    alignas(32) constexpr uint8_t kTriadShuffleMasks[4][32] = {
        {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3},
        {8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7},
        {12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11}
    };
    
    // Advanced SIMD operations with compiler hints
    __attribute__((always_inline)) inline Vec16 Load(const uint16_t* ptr) {
        return _mm256_load_si256((const __m256i*)ptr);
    }
    
    __attribute__((always_inline)) inline void Store(uint16_t* ptr, Vec16 v) {
        _mm256_store_si256((__m256i*)ptr, v);
    }
    
    __attribute__((always_inline)) inline Vec16 And(Vec16 a, Vec16 b) {
        return _mm256_and_si256(a, b);
    }
    
    __attribute__((always_inline)) inline Vec16 Or(Vec16 a, Vec16 b) {
        return _mm256_or_si256(a, b);
    }
    
    __attribute__((always_inline)) inline Vec16 AndNot(Vec16 a, Vec16 b) {
        return _mm256_andnot_si256(a, b);
    }
    
    __attribute__((always_inline)) inline Vec16 Shuffle(Vec16 a, const uint8_t* mask) {
        return _mm256_shuffle_epi8(a, _mm256_load_si256((const __m256i*)mask));
    }
    
    __attribute__((always_inline)) inline Vec16 Broadcast(uint16_t x) {
        return _mm256_set1_epi16(x);
    }
    
    __attribute__((always_inline)) inline bool AnyZero(Vec16 a) {
        return _mm256_testz_si256(a, a);
    }
}

// Ultra-optimized solver class
class SudokuSolver {
private:
    struct alignas(32) Box {
        Vec16 data;
    };
    
    struct alignas(32) Row {
        Vec16 data;
    };
    
    struct alignas(32) Col {
        Vec16 data;
    };
    
    std::array<Box, 9> boxes;
    std::array<Row, 9> row_constraints;
    std::array<Col, 9> col_constraints;
    
    alignas(32) static constexpr int kBoxIndices[81] = {
        0,0,0,1,1,1,2,2,2,
        0,0,0,1,1,1,2,2,2,
        0,0,0,1,1,1,2,2,2,
        3,3,3,4,4,4,5,5,5,
        3,3,3,4,4,4,5,5,5,
        3,3,3,4,4,4,5,5,5,
        6,6,6,7,7,7,8,8,8,
        6,6,6,7,7,7,8,8,8,
        6,6,6,7,7,7,8,8,8
    };
    
    alignas(32) static constexpr int kCellIndices[81] = {
        0,1,2,0,1,2,0,1,2,
        4,5,6,4,5,6,4,5,6,
        8,9,10,8,9,10,8,9,10,
        0,1,2,0,1,2,0,1,2,
        4,5,6,4,5,6,4,5,6,
        8,9,10,8,9,10,8,9,10,
        0,1,2,0,1,2,0,1,2,
        4,5,6,4,5,6,4,5,6,
        8,9,10,8,9,10,8,9,10
    };
    
    size_t solutions_found = 0;
    size_t max_solutions = 1;
    char* solution_buffer = nullptr;
    
    #define LIKELY(x) __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    
    __attribute__((always_inline)) bool Initialize(const char* puzzle) {
        Vec16 all_vec = Broadcast(kAll);
        Vec16 none_vec = Broadcast(kNone);
        
        for (int i = 0; i < 9; i++) {
            boxes[i].data = all_vec;
            row_constraints[i].data = none_vec;
            col_constraints[i].data = none_vec;
        }
        
        for (int i = 0; i < 81; i++) {
            if (LIKELY(puzzle[i] != '.')) {
                if (UNLIKELY(!SetCell(i, puzzle[i] - '1'))) {
                    return false;
                }
            }
        }
        return true;
    }
    
    __attribute__((always_inline)) bool SetCell(int pos, int num) {
        const int box_idx = kBoxIndices[pos];
        const int cell_idx = kCellIndices[pos];
        const int row = pos / 9;
        const int col = pos % 9;
        
        Vec16 num_mask = Broadcast(1u << num);
        if (UNLIKELY(!AnyZero(AndNot(num_mask, row_constraints[row].data)) ||
                     !AnyZero(AndNot(num_mask, col_constraints[col].data)))) {
            return false;
        }
        
        alignas(32) uint16_t data[16];
        Store(data, boxes[box_idx].data);
        
        data[cell_idx] = (1u << num);
        data[(cell_idx / 4) * 4 + 3] &= ~(1u << num);
        data[12 + (cell_idx % 4)] &= ~(1u << num);
        
        Vec16 new_box = Load(data);
        Vec16 triad_mask = Shuffle(new_box, kTriadShuffleMasks[cell_idx / 4]);
        new_box = And(new_box, triad_mask);
        boxes[box_idx].data = new_box;
        
        row_constraints[row].data = Or(row_constraints[row].data, num_mask);
        col_constraints[col].data = Or(col_constraints[col].data, num_mask);
        
        return true;
    }
    
    __attribute__((always_inline)) int FindMostConstrainedCell() {
        int min_pos = -1;
        int min_count = 10;
        
        for (int box_idx = 0; box_idx < 9; box_idx++) {
            alignas(32) uint16_t data[16];
            Store(data, boxes[box_idx].data);
            
            for (int i = 0; i < 9; i += 3) {
                int count1 = __builtin_popcount(data[i]);
                int count2 = __builtin_popcount(data[i + 1]);
                int count3 = __builtin_popcount(data[i + 2]);
                
                if (LIKELY(count1 > 1 && count1 < min_count)) {
                    min_count = count1;
                    min_pos = (box_idx / 3) * 27 + (box_idx % 3) * 3 + i;
                }
                if (LIKELY(count2 > 1 && count2 < min_count)) {
                    min_count = count2;
                    min_pos = (box_idx / 3) * 27 + (box_idx % 3) * 3 + i + 1;
                }
                if (LIKELY(count3 > 1 && count3 < min_count)) {
                    min_count = count3;
                    min_pos = (box_idx / 3) * 27 + (box_idx % 3) * 3 + i + 2;
                }
            }
        }
        
        return min_pos;
    }
    
    bool Solve() {
        int pos = FindMostConstrainedCell();
        
        if (UNLIKELY(pos == -1)) {
            if (LIKELY(solution_buffer)) {
                for (int box_idx = 0; box_idx < 9; box_idx++) {
                    alignas(32) uint16_t data[16];
                    Store(data, boxes[box_idx].data);
                    
                    for (int i = 0; i < 9; i += 3) {
                        int row = (box_idx / 3) * 3 + i / 3;
                        int col = (box_idx % 3) * 3 + i % 3;
                        solution_buffer[row * 9 + col] = '1' + __builtin_ctz(data[i]);
                        solution_buffer[row * 9 + col + 1] = '1' + __builtin_ctz(data[i + 1]);
                        solution_buffer[row * 9 + col + 2] = '1' + __builtin_ctz(data[i + 2]);
                    }
                }
            }
            solutions_found++;
            return solutions_found >= max_solutions;
        }
        
        const int box_idx = kBoxIndices[pos];
        const int cell_idx = kCellIndices[pos];
        const int row = pos / 9;
        const int col = pos % 9;
        
        alignas(32) uint16_t data[16];
        Store(data, boxes[box_idx].data);
        uint32_t possibilities = data[cell_idx];
        
        Box old_box = boxes[box_idx];
        Row old_row = row_constraints[row];
        Col old_col = col_constraints[col];
        
        while (LIKELY(possibilities)) {
            int num = __builtin_ctz(possibilities);
            possibilities &= possibilities - 1;
            
            if (LIKELY(SetCell(pos, num))) {
                if (LIKELY(Solve())) {
                    return true;
                }
            }
            
            boxes[box_idx] = old_box;
            row_constraints[row] = old_row;
            col_constraints[col] = old_col;
        }
        
        return false;
    }

public:
    size_t SolveSudoku(const char* puzzle, size_t limit, char* solution) {
        solutions_found = 0;
        max_solutions = limit;
        solution_buffer = solution;
        
        if (UNLIKELY(!Initialize(puzzle))) {
            return 0;
        }
        
        Solve();
        return solutions_found;
    }
};

class ResultsManager {
    std::vector<std::string> results;
public:
    ResultsManager(size_t size) : results(size) {}
    void setResult(size_t index, const char* solution) {
        results[index] = std::string(solution, 81);
    }
    const std::vector<std::string>& getResults() const { return results; }
};

void solverWorker(const std::vector<std::string>& puzzles, ResultsManager& manager, size_t start, size_t end) {
    thread_local SudokuSolver solver;
    char solution[82]; // 81 chars + null terminator
    for (size_t i = start; i < end; ++i) {
        solution[81] = '\0'; // Ensure null termination
        solver.SolveSudoku(puzzles[i].c_str(), 1, solution);
        manager.setResult(i, solution);
    }
}

int main() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::ifstream input("input.txt");
    if (!input) {
        std::cerr << "Failed to open input.txt" << std::endl;
        return 1;
    }

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
        workers.emplace_back(solverWorker, std::ref(puzzles), std::ref(results), start, end);
        start = end;
    }

    for (auto& t : workers) t.join();

    std::ofstream output("output.txt");
    for (const auto& solution : results.getResults()) {
        output << solution << '\n';
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime).count();
    std::cout << "Solved " << puzzles.size() << " puzzles in " << duration << " ms\n";
}