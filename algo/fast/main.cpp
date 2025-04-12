#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <array>

struct CellInfo {
    uint8_t row, col, box;
};

// Precomputed cell information
static constexpr CellInfo preCell[81] = {
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

// Precomputed affected cells for each cell
// Each row contains the indices of cells that share a row, column, or box with the cell
static constexpr struct {
    uint8_t count;
    uint8_t cells[20];
} affectedCells[81] = {
    // This will be filled at compile time below
};

// Precomputed bit-count lookup table for faster popcount
static constexpr uint8_t bitCountTable[1024] = {
    // Will be filled at compile time below
};

class SudokuSolver {
    // Aligned data structures for better cache usage
    alignas(64) uint16_t rows[9] = {0};
    alignas(64) uint16_t cols[9] = {0};
    alignas(64) uint16_t boxes[9] = {0};
    
    // Array of possibilities for each cell (0x3FE = 0b1111111110 represents digits 1-9)
    alignas(64) uint16_t cellPossibilities[81];
    
    // Count of possibilities for each cell (initialized when we compute possibilities)
    alignas(64) uint8_t possibilityCounts[81];
    
    char grid[81];
    uint8_t emptyCells[81];
    uint8_t position[81];
    int emptyCount = 0;
    
    // Track the MRV cell and count to avoid full recalculation
    int currentMRVCell = -1;
    int currentMRVCount = 10;

    // Fast inline popcount for 16-bit integers
    __attribute__((always_inline)) 
    inline int fastPopCount(uint16_t x) const {
        // Use our precomputed lookup table for values < 1024
        if (x < 1024) return bitCountTable[x];
        // For larger values, use intrinsic
        return __builtin_popcount(x);
    }

    __attribute__((always_inline)) 
    inline bool canPlace(int cell, int num) const {
        // Even faster canPlace - just check if the bit is set in the possibilities
        return (cellPossibilities[cell] & (1 << num)) != 0;
    }

    __attribute__((always_inline)) 
    inline void place(int cell, int num) {
        const auto& info = preCell[cell];
        const uint16_t mask = 1 << num;
        
        // Set the digit in the constraint arrays
        rows[info.row] |= mask;
        cols[info.col] |= mask;
        boxes[info.box] |= mask;
        grid[cell] = '0' + num;

        // Update the empty cells list - move the last empty cell to this position
        const int pos = position[cell];
        const uint8_t last = emptyCells[--emptyCount];
        emptyCells[pos] = last;
        position[last] = pos;
        
        // Reset MRV tracking since the puzzle has changed
        currentMRVCell = -1;
        currentMRVCount = 10;
        
        // Update possibilities for affected cells
        updatePossibilities(cell, num);
    }

    __attribute__((always_inline)) 
    inline void remove(int cell, int num) {
        const auto& info = preCell[cell];
        const uint16_t mask = ~(1 << num);
        
        // Clear the digit in the constraint arrays
        rows[info.row] &= mask;
        cols[info.col] &= mask;
        boxes[info.box] &= mask;
        grid[cell] = '0';

        // Update the empty cells list
        position[cell] = emptyCount;
        emptyCells[emptyCount++] = cell;
        
        // Reset MRV tracking since the puzzle has changed
        currentMRVCell = -1;
        currentMRVCount = 10;
        
        // Recompute possibilities for affected cells
        recomputePossibilities(cell, num);
    }
    
    // Update possibilities when a value is placed
    __attribute__((always_inline)) 
    inline void updatePossibilities(int cell, int num) {
        const uint16_t mask = ~(1 << num);
        
        // Update possibilities for all affected cells
        for (uint8_t i = 0; i < affectedCells[cell].count; ++i) {
            const uint8_t affectedCell = affectedCells[cell].cells[i];
            
            // Skip if this cell is already filled
            if (grid[affectedCell] != '0') continue;
            
            // Update possibility and count
            uint16_t newPoss = cellPossibilities[affectedCell] & mask;
            if (cellPossibilities[affectedCell] != newPoss) {
                cellPossibilities[affectedCell] = newPoss;
                possibilityCounts[affectedCell] = fastPopCount(newPoss);
                
                // Update MRV tracking if this is now better than current MRV
                if (currentMRVCount > possibilityCounts[affectedCell] && possibilityCounts[affectedCell] > 0) {
                    currentMRVCount = possibilityCounts[affectedCell];
                    currentMRVCell = affectedCell;
                }
            }
        }
    }
    
    // Recompute possibilities when backtracking
    __attribute__((always_inline)) 
    inline void recomputePossibilities(int cell, int num) {
        // For each affected cell, recompute possibilities from scratch
        for (uint8_t i = 0; i < affectedCells[cell].count; ++i) {
            const uint8_t affectedCell = affectedCells[cell].cells[i];
            
            // Skip if this cell is already filled
            if (grid[affectedCell] != '0') continue;
            
            const auto& info = preCell[affectedCell];
            uint16_t poss = ~(rows[info.row] | cols[info.col] | boxes[info.box]) & 0x3FE;
            cellPossibilities[affectedCell] = poss;
            possibilityCounts[affectedCell] = fastPopCount(poss);
        }
    }

    // Optimized findMRV - faster for multiple calls
    __attribute__((always_inline)) 
    inline int findMRV() {
        // If we have a cached MRV cell that's still valid, use it
        if (currentMRVCell >= 0 && grid[currentMRVCell] == '0') {
            // Verify it's still the MRV
            bool stillValid = true;
            for (int i = 0; i < emptyCount && stillValid; ++i) {
                const int cell = emptyCells[i];
                if (cell != currentMRVCell && possibilityCounts[cell] < currentMRVCount) {
                    stillValid = false;
                }
            }
            
            if (stillValid) {
                return currentMRVCell;
            }
        }
        
        // Full recalculation
        int minCount = 10, bestCell = -1;
        
        for (int i = 0; i < emptyCount; ++i) {
            const int cell = emptyCells[i];
            const int count = possibilityCounts[cell];
            
            if (count == 0) return -1;  // No valid possibilities
            if (count < minCount) {
                minCount = count;
                bestCell = cell;
                if (minCount == 1) break;  // Can't get better than 1
            }
        }
        
        // Cache the result
        currentMRVCell = bestCell;
        currentMRVCount = minCount;
        
        return bestCell;
    }

    bool solveInternal() {
        if (emptyCount == 0) return true;
        
        const int cell = findMRV();
        if (cell == -1) return false;

        uint16_t possible = cellPossibilities[cell];

        while (possible) {
            const int num = __builtin_ctz(possible);
            possible &= ~(1 << num);
            
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
        
        // Reset MRV tracking
        currentMRVCell = -1;
        currentMRVCount = 10;

        for (int i = 0; i < 81; ++i) {
            if (grid[i] != '0') {
                const int num = grid[i] - '0';
                const auto& info = preCell[i];
                rows[info.row] |= (1 << num);
                cols[info.col] |= (1 << num);
                boxes[info.box] |= (1 << num);
            } else {
                emptyCells[emptyCount] = i;
                position[i] = emptyCount;
                emptyCount++;
            }
        }
        
        // Initialize possibilities for all empty cells
        for (int i = 0; i < emptyCount; ++i) {
            const int cell = emptyCells[i];
            const auto& info = preCell[cell];
            uint16_t poss = ~(rows[info.row] | cols[info.col] | boxes[info.box]) & 0x3FE;
            cellPossibilities[cell] = poss;
            possibilityCounts[cell] = fastPopCount(poss);
        }
    }

    bool solve() { return solveInternal(); }
    const char* getSolution() const { return grid; }
};

class ResultsManager {
    std::vector<std::array<char, 81>> results;
public:
    ResultsManager(size_t size) : results(size) {}
    void setResult(size_t index, const char* solution) {
        std::memcpy(results[index].data(), solution, 81);
    }
    const std::vector<std::array<char, 81>>& getResults() const { return results; }
};

void solverWorker(const std::vector<std::string>& puzzles, ResultsManager& manager, size_t start, size_t end) {
    thread_local SudokuSolver solver;
    for (size_t i = start; i < end; ++i) {
        solver.initialize(puzzles[i]);
        solver.solve();
        manager.setResult(i, solver.getSolution());
    }
}

int main() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::ifstream input("input.txt");
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
    std::cout << "Solved " << puzzles.size() << " puzzles in " << duration << " ms\n";
}