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

static constexpr uint8_t preRow[81] = {
    0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8
};

static constexpr uint8_t preCol[81] = {
    0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8
};

static constexpr uint8_t preBox[81] = {
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

static constexpr struct {
    uint8_t count;
    uint8_t cells[20];
} affectedCells[81] = {
    {20, {1,2,3,4,5,6,7,8,9,18,27,36,45,54,63,72,10,11,19,20}}, // cell 0
    {20, {0,2,3,4,5,6,7,8,10,19,28,37,46,55,64,73,9,11,18,20}}, // cell 1
    {20, {0,1,3,4,5,6,7,8,11,20,29,38,47,56,65,74,9,10,18,19}}, // cell 2
    {20, {0,1,2,4,5,6,7,8,12,21,30,39,48,57,66,75,13,14,22,23}}, // cell 3
    {20, {0,1,2,3,5,6,7,8,13,22,31,40,49,58,67,76,12,14,21,23}}, // cell 4
    {20, {0,1,2,3,4,6,7,8,14,23,32,41,50,59,68,77,12,13,21,22}}, // cell 5
    {20, {0,1,2,3,4,5,7,8,15,24,33,42,51,60,69,78,16,17,25,26}}, // cell 6
    {20, {0,1,2,3,4,5,6,8,16,25,34,43,52,61,70,79,15,17,24,26}}, // cell 7
    {20, {0,1,2,3,4,5,6,7,17,26,35,44,53,62,71,80,15,16,24,25}}, // cell 8
    {20, {0,1,2,10,11,12,13,14,15,16,17,18,27,36,45,54,63,72,19,20}}, // cell 9
    {20, {0,1,2,9,11,12,13,14,15,16,17,19,28,37,46,55,64,73,18,20}}, // cell 10
    {20, {0,1,2,9,10,12,13,14,15,16,17,20,29,38,47,56,65,74,18,19}}, // cell 11
    {20, {3,4,5,9,10,11,13,14,15,16,17,21,30,39,48,57,66,75,22,23}}, // cell 12
    {20, {3,4,5,9,10,11,12,14,15,16,17,22,31,40,49,58,67,76,21,23}}, // cell 13
    {20, {3,4,5,9,10,11,12,13,15,16,17,23,32,41,50,59,68,77,21,22}}, // cell 14
    {20, {6,7,8,9,10,11,12,13,14,16,17,24,33,42,51,60,69,78,25,26}}, // cell 15
    {20, {6,7,8,9,10,11,12,13,14,15,17,25,34,43,52,61,70,79,24,26}}, // cell 16
    {20, {6,7,8,9,10,11,12,13,14,15,16,26,35,44,53,62,71,80,24,25}}, // cell 17
    {20, {0,1,2,9,10,11,19,20,21,22,23,24,25,26,27,36,45,54,63,72}}, // cell 18
    {20, {0,1,2,9,10,11,18,20,21,22,23,24,25,26,28,37,46,55,64,73}}, // cell 19
    {20, {0,1,2,9,10,11,18,19,21,22,23,24,25,26,29,38,47,56,65,74}}, // cell 20
    {20, {3,4,5,12,13,14,18,19,20,22,23,24,25,26,30,39,48,57,66,75}}, // cell 21
    {20, {3,4,5,12,13,14,18,19,20,21,23,24,25,26,31,40,49,58,67,76}}, // cell 22
    {20, {3,4,5,12,13,14,18,19,20,21,22,24,25,26,32,41,50,59,68,77}}, // cell 23
    {20, {6,7,8,15,16,17,18,19,20,21,22,23,25,26,33,42,51,60,69,78}}, // cell 24
    {20, {6,7,8,15,16,17,18,19,20,21,22,23,24,26,34,43,52,61,70,79}}, // cell 25
    {20, {6,7,8,15,16,17,18,19,20,21,22,23,24,25,35,44,53,62,71,80}}, // cell 26
    {20, {0,9,18,28,29,30,31,32,33,34,35,36,45,54,63,72,37,38,46,47}}, // cell 27
    {20, {1,10,19,27,29,30,31,32,33,34,35,37,46,55,64,73,36,38,45,47}}, // cell 28
    {20, {2,11,20,27,28,30,31,32,33,34,35,38,47,56,65,74,36,37,45,46}}, // cell 29
    {20, {3,12,21,27,28,29,31,32,33,34,35,39,48,57,66,75,40,41,49,50}}, // cell 30
    {20, {4,13,22,27,28,29,30,32,33,34,35,40,49,58,67,76,39,41,48,50}}, // cell 31
    {20, {5,14,23,27,28,29,30,31,33,34,35,41,50,59,68,77,39,40,48,49}}, // cell 32
    {20, {6,15,24,27,28,29,30,31,32,34,35,42,51,60,69,78,43,44,52,53}}, // cell 33
    {20, {7,16,25,27,28,29,30,31,32,33,35,43,52,61,70,79,42,44,51,53}}, // cell 34
    {20, {8,17,26,27,28,29,30,31,32,33,34,44,53,62,71,80,42,43,51,52}}, // cell 35
    {20, {0,9,18,27,28,29,37,38,39,40,41,42,43,44,45,54,63,72,46,47}}, // cell 36
    {20, {1,10,19,27,28,29,36,38,39,40,41,42,43,44,46,55,64,73,45,47}}, // cell 37
    {20, {2,11,20,27,28,29,36,37,39,40,41,42,43,44,47,56,65,74,45,46}}, // cell 38
    {20, {3,12,21,30,31,32,36,37,38,40,41,42,43,44,48,57,66,75,49,50}}, // cell 39
    {20, {4,13,22,30,31,32,36,37,38,39,41,42,43,44,49,58,67,76,48,50}}, // cell 40
    {20, {5,14,23,30,31,32,36,37,38,39,40,42,43,44,50,59,68,77,48,49}}, // cell 41
    {20, {6,15,24,33,34,35,36,37,38,39,40,41,43,44,51,60,69,78,52,53}}, // cell 42
    {20, {7,16,25,33,34,35,36,37,38,39,40,41,42,44,52,61,70,79,51,53}}, // cell 43
    {20, {8,17,26,33,34,35,36,37,38,39,40,41,42,43,53,62,71,80,51,52}}, // cell 44
    {20, {0,9,18,27,28,29,36,37,38,46,47,48,49,50,51,52,53,54,63,72}}, // cell 45
    {20, {1,10,19,27,28,29,36,37,38,45,47,48,49,50,51,52,53,55,64,73}}, // cell 46
    {20, {2,11,20,27,28,29,36,37,38,45,46,48,49,50,51,52,53,56,65,74}}, // cell 47
    {20, {3,12,21,30,31,32,39,40,41,45,46,47,49,50,51,52,53,57,66,75}}, // cell 48
    {20, {4,13,22,30,31,32,39,40,41,45,46,47,48,50,51,52,53,58,67,76}}, // cell 49
    {20, {5,14,23,30,31,32,39,40,41,45,46,47,48,49,51,52,53,59,68,77}}, // cell 50
    {20, {6,15,24,33,34,35,42,43,44,45,46,47,48,49,50,52,53,60,69,78}}, // cell 51
    {20, {7,16,25,33,34,35,42,43,44,45,46,47,48,49,50,51,53,61,70,79}}, // cell 52
    {20, {8,17,26,33,34,35,42,43,44,45,46,47,48,49,50,51,52,62,71,80}}, // cell 53
    {20, {0,9,18,27,36,45,55,56,57,58,59,60,61,62,63,72,64,65,73,74}}, // cell 54
    {20, {1,10,19,28,37,46,54,56,57,58,59,60,61,62,64,73,63,65,72,74}}, // cell 55
    {20, {2,11,20,29,38,47,54,55,57,58,59,60,61,62,65,74,63,64,72,73}}, // cell 56
    {20, {3,12,21,30,39,48,54,55,56,58,59,60,61,62,66,75,67,68,76,77}}, // cell 57
    {20, {4,13,22,31,40,49,54,55,56,57,59,60,61,62,67,76,66,68,75,77}}, // cell 58
    {20, {5,14,23,32,41,50,54,55,56,57,58,60,61,62,68,77,66,67,75,76}}, // cell 59
    {20, {6,15,24,33,42,51,54,55,56,57,58,59,61,62,69,78,70,71,79,80}}, // cell 60
    {20, {7,16,25,34,43,52,54,55,56,57,58,59,60,62,70,79,69,71,78,80}}, // cell 61
    {20, {8,17,26,35,44,53,54,55,56,57,58,59,60,61,71,80,69,70,78,79}}, // cell 62
    {20, {0,9,18,27,36,45,54,55,56,64,65,66,67,68,69,70,71,72,73,74}}, // cell 63
    {20, {1,10,19,28,37,46,54,55,56,63,65,66,67,68,69,70,71,73,72,74}}, // cell 64
    {20, {2,11,20,29,38,47,54,55,56,63,64,66,67,68,69,70,71,74,72,73}}, // cell 65
    {20, {3,12,21,30,39,48,57,58,59,63,64,65,67,68,69,70,71,75,76,77}}, // cell 66
    {20, {4,13,22,31,40,49,57,58,59,63,64,65,66,68,69,70,71,76,75,77}}, // cell 67
    {20, {5,14,23,32,41,50,57,58,59,63,64,65,66,67,69,70,71,77,75,76}}, // cell 68
    {20, {6,15,24,33,42,51,60,61,62,63,64,65,66,67,68,70,71,78,79,80}}, // cell 69
    {20, {7,16,25,34,43,52,60,61,62,63,64,65,66,67,68,69,71,79,78,80}}, // cell 70
    {20, {8,17,26,35,44,53,60,61,62,63,64,65,66,67,68,69,70,80,78,79}}, // cell 71
    {20, {0,9,18,27,36,45,54,55,56,63,64,65,73,74,75,76,77,78,79,80}}, // cell 72
    {20, {1,10,19,28,37,46,54,55,56,63,64,65,72,74,75,76,77,78,79,80}}, // cell 73
    {20, {2,11,20,29,38,47,54,55,56,63,64,65,72,73,75,76,77,78,79,80}}, // cell 74
    {20, {3,12,21,30,39,48,57,58,59,66,67,68,72,73,74,76,77,78,79,80}}, // cell 75
    {20, {4,13,22,31,40,49,57,58,59,66,67,68,72,73,74,75,77,78,79,80}}, // cell 76
    {20, {5,14,23,32,41,50,57,58,59,66,67,68,72,73,74,75,76,78,79,80}}, // cell 77
    {20, {6,15,24,33,42,51,60,61,62,69,70,71,72,73,74,75,76,77,79,80}}, // cell 78
    {20, {7,16,25,34,43,52,60,61,62,69,70,71,72,73,74,75,76,77,78,80}}, // cell 79
    {20, {8,17,26,35,44,53,60,61,62,69,70,71,72,73,74,75,76,77,78,79}}  // cell 80
};

static constexpr uint8_t bitCountTable[1024] = {
    0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    5,6,6,7,6,7,7,8,6,7,7,8,7,8,8,9,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    5,6,6,7,6,7,7,8,6,7,7,8,7,8,8,9,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    5,6,6,7,6,7,7,8,6,7,7,8,7,8,8,9,
    3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    5,6,6,7,6,7,7,8,6,7,7,8,7,8,8,9,
    4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
    5,6,6,7,6,7,7,8,6,7,7,8,7,8,8,9,
    5,6,6,7,6,7,7,8,6,7,7,8,7,8,8,9,
    6,7,7,8,7,8,8,9,7,8,8,9,8,9,9,10
};

class SudokuSolver {
    alignas(64) uint16_t rows[9] = {0};
    alignas(64) uint16_t cols[9] = {0};
    alignas(64) uint16_t boxes[9] = {0};
    alignas(64) uint16_t cellPossibilities[81];
    alignas(64) uint8_t possibilityCounts[81];
    char grid[81];
    uint8_t emptyCells[81];
    uint8_t position[81];
    int emptyCount = 0;
    int currentMRVCell = -1;
    int currentMRVCount = 10;

    inline int fastPopCount(uint16_t x) const {
        return bitCountTable[x]; // No condition needed
    }

    inline bool canPlace(int cell, int num) const {
        return (cellPossibilities[cell] & (1 << num)) != 0;
    }

    inline void place(int cell, int num) {
        const uint8_t r = preRow[cell];
        const uint8_t c = preCol[cell];
        const uint8_t b = preBox[cell];
        const uint16_t mask = 1 << num;
        rows[r] |= mask;
        cols[c] |= mask;
        boxes[b] |= mask;
        grid[cell] = '0' + num;

        const int pos = position[cell];
        const uint8_t last = emptyCells[--emptyCount];
        emptyCells[pos] = last;
        position[last] = pos;
        currentMRVCell = -1;
        currentMRVCount = 10;

        for (uint8_t i = 0; i < affectedCells[cell].count; ++i) {
            const uint8_t affectedCell = affectedCells[cell].cells[i];
            if (grid[affectedCell] != '0') continue;
            uint16_t newPoss = cellPossibilities[affectedCell] & ~mask;
            if (cellPossibilities[affectedCell] != newPoss) {
                cellPossibilities[affectedCell] = newPoss;
                possibilityCounts[affectedCell] = fastPopCount(newPoss);
                if (currentMRVCount > possibilityCounts[affectedCell] && possibilityCounts[affectedCell] > 0) {
                    currentMRVCount = possibilityCounts[affectedCell];
                    currentMRVCell = affectedCell;
                }
            }
        }
    }

    inline void remove(int cell, int num) {
        const uint8_t r = preRow[cell];
        const uint8_t c = preCol[cell];
        const uint8_t b = preBox[cell];
        const uint16_t mask = ~(1 << num);
        rows[r] &= mask;
        cols[c] &= mask;
        boxes[b] &= mask;
        grid[cell] = '0';

        position[cell] = emptyCount;
        emptyCells[emptyCount++] = cell;
        currentMRVCell = -1;
        currentMRVCount = 10;

        for (uint8_t i = 0; i < affectedCells[cell].count; ++i) {
            const uint8_t affectedCell = affectedCells[cell].cells[i];
            if (grid[affectedCell] != '0') continue;
            const uint8_t ar = preRow[affectedCell];
            const uint8_t ac = preCol[affectedCell];
            const uint8_t ab = preBox[affectedCell];
            uint16_t poss = ~(rows[ar] | cols[ac] | boxes[ab]) & 0x3FE;
            cellPossibilities[affectedCell] = poss;
            possibilityCounts[affectedCell] = fastPopCount(poss);
        }
    }

    inline int findMRV() {
        if (currentMRVCell >= 0 && grid[currentMRVCell] == '0') {
            bool stillValid = true;
            for (int i = 0; i < emptyCount && stillValid; ++i) {
                const int cell = emptyCells[i];
                if (cell != currentMRVCell && possibilityCounts[cell] < currentMRVCount) {
                    stillValid = false;
                }
            }
            if (stillValid) return currentMRVCell;
        }

        int minCount = 10, bestCell = -1;
        for (int i = 0; i < emptyCount; ++i) {
            const int cell = emptyCells[i];
            const int count = possibilityCounts[cell];
            if (count == 0) return -1;
            if (count < minCount) {
                minCount = count;
                bestCell = cell;
                if (minCount == 1) break;
            }
        }
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

        currentMRVCell = -1;
        currentMRVCount = 10;

        for (int i = 0; i < 81; ++i) {
            if (grid[i] != '0') {
                const int num = grid[i] - '0';
                const uint8_t r = preRow[i];
                const uint8_t c = preCol[i];
                const uint8_t b = preBox[i];
                rows[r] |= (1 << num);
                cols[c] |= (1 << num);
                boxes[b] |= (1 << num);
            } else {
                emptyCells[emptyCount] = i;
                position[i] = emptyCount;
                emptyCount++;
            }
        }

        for (int i = 0; i < emptyCount; ++i) {
            const int cell = emptyCells[i];
            const uint8_t r = preRow[cell];
            const uint8_t c = preCol[cell];
            const uint8_t b = preBox[cell];
            uint16_t poss = ~(rows[r] | cols[c] | boxes[b]) & 0x3FE;
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