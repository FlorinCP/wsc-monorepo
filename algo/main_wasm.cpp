#include <iostream>
#include <vector>
#include <string>
#include <cstring>

struct CellInfo {
    uint8_t row, col, box;
};

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

class SudokuSolver {
    alignas(64) uint16_t rows[9] = {0};
    alignas(64) uint16_t cols[9] = {0};
    alignas(64) uint16_t boxes[9] = {0};
    char grid[81];
    uint8_t emptyCells[81];
    uint8_t position[81];
    int emptyCount = 0;

    inline bool canPlace(int cell, int num) const {
        const uint16_t mask = 1 << num;
        const auto& info = preCell[cell];
        return !((rows[info.row] | cols[info.col] | boxes[info.box]) & mask);
    }

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

    // Count bits set in a number (replacement for __builtin_popcount)
    int popcount(uint16_t x) const {
        int count = 0;
        while (x) {
            count += x & 1;
            x >>= 1;
        }
        return count;
    }

    // Count trailing zeros (replacement for __builtin_ctz)
    int ctz(uint16_t x) const {
        if (x == 0) return 16;
        int count = 0;
        while ((x & 1) == 0) {
            count++;
            x >>= 1;
        }
        return count;
    }

    int findMRV() const {
        int minCount = 10, bestCell = -1;
        for (int i = 0; i < emptyCount; ++i) {
            const int cell = emptyCells[i];
            const auto& info = preCell[cell];
            const uint16_t used = rows[info.row] | cols[info.col] | boxes[info.box];
            const int count = popcount(~used & 0x3FE);
            if (count == 0) return -1;
            if (count < minCount) {
                minCount = count;
                bestCell = cell;
                if (minCount == 1) break;
            }
        }
        return bestCell;
    }

    bool solveInternal() {
        if (emptyCount == 0) return true;
        const int cell = findMRV();
        if (cell == -1) return false;

        const auto& info = preCell[cell];
        const uint16_t used = rows[info.row] | cols[info.col] | boxes[info.box];
        uint16_t possible = ~used & 0x3FE;

        while (possible) {
            const int num = ctz(possible);
            possible ^= (1 << num);
            place(cell, num);
            if (solveInternal()) return true;
            remove(cell, num);
        }
        return false;
    }

public:
    void initialize(const char* puzzle) {
        emptyCount = 0;
        memset(rows, 0, sizeof(rows));
        memset(cols, 0, sizeof(cols));
        memset(boxes, 0, sizeof(boxes));
        memcpy(grid, puzzle, 81);

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
    }

    bool solve() { return solveInternal(); }
    const char* getSolution() const { return grid; }
};

// Global solver for WebAssembly
SudokuSolver solver;

// Export functions for JavaScript to call
extern "C" {
    char* solveSudoku(const char* puzzle) {
        static char solution[82];
        solver.initialize(puzzle);
        bool solved = solver.solve();

        if (solved) {
            memcpy(solution, solver.getSolution(), 81);
            solution[81] = '\0';
        } else {
            strcpy(solution, "No solution found");
        }

        return solution;
    }
}