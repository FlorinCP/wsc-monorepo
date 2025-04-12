#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <pthread.h>

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

// Memory buffers for batch processing
char* inputBuffer = nullptr;
char* outputBuffer = nullptr;
int inputBufferSize = 0;
int outputBufferSize = 0;
bool* solvedFlags = nullptr;
int solvedFlagsSize = 0;

// Mutex for thread synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int completedThreads = 0;
int totalSolvedCount = 0;
bool stopProcessing = false;

// Thread work configuration
struct ThreadWork {
    int startIdx;
    int count;
};

// Thread function to solve a batch of puzzles
void* solvePuzzlesBatch(void* arg) {
    ThreadWork* work = (ThreadWork*)arg;
    int solvedCount = 0;

    for (int i = 0; i < work->count; i++) {
        // Check if processing should stop
        if (stopProcessing) break;

        int puzzleIdx = work->startIdx + i;
        SudokuSolver solver;

        // Initialize solver with the puzzle
        solver.initialize(inputBuffer + (puzzleIdx * 81));
        bool solved = solver.solve();

        solvedFlags[puzzleIdx] = solved;
        if (solved) {
            memcpy(outputBuffer + (puzzleIdx * 81), solver.getSolution(), 81);
            solvedCount++;
        } else {
            // For unsolved puzzles, copy original puzzle
            memcpy(outputBuffer + (puzzleIdx * 81), inputBuffer + (puzzleIdx * 81), 81);
        }
    }

    // Update shared counter with mutex protection
    pthread_mutex_lock(&mutex);
    totalSolvedCount += solvedCount;
    completedThreads++;
    pthread_mutex_unlock(&mutex);

    delete work;
    return nullptr;
}

extern "C" {
    // Single puzzle solver (keep for compatibility)
    char* solveSudoku(const char* puzzle) {
        static char solution[82];
        static SudokuSolver solver;  // Use a local solver for this function

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

    // Allocate input buffer
    void allocateInputBuffer(int size) {
        if (inputBuffer && size <= inputBufferSize) return;

        if (inputBuffer) free(inputBuffer);
        inputBuffer = (char*)malloc(size);
        inputBufferSize = size;
    }

    // Allocate output buffer
    void allocateOutputBuffer(int size) {
        if (outputBuffer && size <= outputBufferSize) return;

        if (outputBuffer) free(outputBuffer);
        outputBuffer = (char*)malloc(size);
        outputBufferSize = size;
    }

    // Allocate solved flags buffer
    void allocateSolvedFlags(int size) {
        if (solvedFlags && size <= solvedFlagsSize) return;

        if (solvedFlags) free(solvedFlags);
        solvedFlags = (bool*)malloc(size * sizeof(bool));
        solvedFlagsSize = size;
    }

    // Free all buffers
    void freeAllBuffers() {
        if (inputBuffer) {
            free(inputBuffer);
            inputBuffer = nullptr;
        }
        if (outputBuffer) {
            free(outputBuffer);
            outputBuffer = nullptr;
        }
        if (solvedFlags) {
            free(solvedFlags);
            solvedFlags = nullptr;
        }
        inputBufferSize = 0;
        outputBufferSize = 0;
        solvedFlagsSize = 0;
    }

    // Set input puzzle at specified index
    void setPuzzle(int index, const char* puzzle) {
        if (!inputBuffer || index >= inputBufferSize / 81) return;
        memcpy(inputBuffer + (index * 81), puzzle, 81);
    }

    // Get solution at specified index
    char* getSolution(int index) {
        static char solution[82];
        if (!outputBuffer || index >= outputBufferSize / 81) {
            solution[0] = '\0';
            return solution;
        }

        memcpy(solution, outputBuffer + (index * 81), 81);
        solution[81] = '\0';
        return solution;
    }

    // Check if puzzle at index was solved
    bool wasSolved(int index) {
        if (!solvedFlags || index >= solvedFlagsSize) return false;
        return solvedFlags[index];
    }

    // Get number of completed threads
    int getCompletedThreadCount() {
        return completedThreads;
    }

    // Request stopping the processing
    void requestStop() {
        stopProcessing = true;
    }

    // Reset the stop flag
    void resetStopFlag() {
        stopProcessing = false;
    }

    // TRUE BATCH PROCESSING: Process multiple puzzles using pthreads
    int solveBatch(int count) {
        if (!inputBuffer || !outputBuffer || !solvedFlags ||
            count > inputBufferSize / 81 ||
            count > outputBufferSize / 81 ||
            count > solvedFlagsSize) {
            return -1;  // Error: buffers not properly allocated
        }

        // Reset counters and flags
        completedThreads = 0;
        totalSolvedCount = 0;
        stopProcessing = false;

        // Use 4 threads for batch processing
        int numThreads = 4;

        // Calculate work per thread
        int puzzlesPerThread = (count + numThreads - 1) / numThreads;

        // Create and start threads
        pthread_t threads[numThreads];
        for (int i = 0; i < numThreads; i++) {
            int startIdx = i * puzzlesPerThread;
            int threadCount = std::min(puzzlesPerThread, count - startIdx);

            if (threadCount <= 0) break;

            ThreadWork* work = new ThreadWork{startIdx, threadCount};

            if (pthread_create(&threads[i], nullptr, solvePuzzlesBatch, work) != 0) {
                // If thread creation fails, process this batch on the main thread
                solvePuzzlesBatch(work);
            }
        }

        // Wait for all threads to complete
        for (int i = 0; i < numThreads; i++) {
            int startIdx = i * puzzlesPerThread;
            if (startIdx < count) {
                pthread_join(threads[i], nullptr);
            }
        }

        return totalSolvedCount;
    }
}