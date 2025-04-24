#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <array>
#include <mutex>
#include <atomic>
#include <numeric>

#ifdef __APPLE__
#include <mach/mach.h>
size_t getCurrentRSS() {
    task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    kern_return_t err = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count);
    return (err == KERN_SUCCESS) ? info.resident_size : 0;
}
#else
size_t getCurrentRSS() {
    // On Linux, you might read /proc/self/statm or use getrusage
    // On Windows, use GetProcessMemoryInfo
    // Simple placeholder:
    try {
        std::ifstream statm("/proc/self/statm");
        if (statm.is_open()) {
            long size, resident;
            statm >> size >> resident; // Read the first two values
            statm.close();
            long page_size = sysconf(_SC_PAGESIZE); // Get page size
            return resident * page_size;           // Resident set size in bytes
        }
    } catch (...) {
        // Ignore errors if /proc isn't available or readable
    }
    return 0; // Placeholder for non-Apple/non-Linux or if reading fails
}
#endif // __APPLE__

void reportMemoryUsage(const std::string& stage) {
    size_t rss = getCurrentRSS();
    if (rss > 0) {
        std::cout << "[" << stage << "] Memory usage (RSS): " << rss / 1024 << " KB\n";
    } else {
        std::cout << "[" << stage << "] Memory usage (RSS): Not available\n";
    }
}

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

        if (emptyCount > 0) {
            const int pos = position[cell];
            if (pos < emptyCount) {
                const uint8_t last_cell_index = emptyCells[emptyCount - 1];
                emptyCells[pos] = last_cell_index;
                position[last_cell_index] = pos;
            }
            emptyCount--;
        }
    }


    inline void remove(int cell, int num) {
        const auto& info = preCell[cell];
        const uint16_t mask = ~(1 << num);
        rows[info.row] &= mask;
        cols[info.col] &= mask;
        boxes[info.box] &= mask;
        grid[cell] = '0'; // Mark as empty

        // Correctly add the cell back to the empty list
        if (emptyCount < 81) { // Ensure space before adding
            position[cell] = emptyCount; // Record its new position (at the end)
            emptyCells[emptyCount] = cell; // Add it to the end
            emptyCount++; // Increment count
        }
    }


    // No inline hint needed here, let optimizer decide. It's larger.
    int findMRV() const {
        int minCount = 10; // 9 digits + 1 impossible state
        int bestCell = -1;
        // Iterate only over current empty cells
        for (int i = 0; i < emptyCount; ++i) {
            const int cell = emptyCells[i];
            // Safety check for cell index (should be 0-80)
            if (cell < 0 || cell >= 81) continue; // Should not happen

            const auto& info = preCell[cell];
            const uint16_t used = rows[info.row] | cols[info.col] | boxes[info.box];
            // __builtin_popcount is efficient, counts set bits (possible numbers are 1-9)
            // Mask 0x3FE represents bits 1 through 9 (binary 1111111110)
            const int count = __builtin_popcount(~used & 0x3FE);

            if (count == 0) return -1; // Found a cell with no possible moves -> backtrack immediately
            if (count < minCount) {
                minCount = count;
                bestCell = cell;
                if (minCount == 1) break; // Optimization: Can't do better than 1
            }
        }
         // If emptyCount > 0 but bestCell wasn't updated (e.g., only one empty cell left),
         // return that cell. If emptyCount is 0, this loop doesn't run, and bestCell remains -1.
         if (bestCell == -1 && emptyCount > 0) {
             bestCell = emptyCells[0];
         }
        return bestCell;
    }


    bool solveInternal() {
        if (emptyCount == 0) return true; // Base case: no empty cells left

        const int cell = findMRV();
        if (cell == -1) return false; // No valid moves for any empty cell -> backtrack

        // Ensure cell is valid before proceeding
        if (cell < 0 || cell >= 81) return false; // Should not happen if findMRV is correct

        const auto& info = preCell[cell];
        const uint16_t used = rows[info.row] | cols[info.col] | boxes[info.box];
        uint16_t possible = ~used & 0x3FE; // Mask for numbers 1-9

        while (possible) {
            // Find the lowest set bit (lowest possible number)
            // __builtin_ctz finds the index of the least significant 1-bit
            const int num_bit_index = __builtin_ctz(possible); // Returns 1 for bit 1, 2 for bit 2 etc.

            // The bit index directly corresponds to the number (1-9)
            const int num = num_bit_index;

             // Check if num is valid (1-9) - ctz on 0 is undefined
             if (num < 1 || num > 9) {
                  // This should technically not happen if possible has bits set in 0x3FE
                  // Clear the lowest bit found and continue
                  possible &= possible - 1; // Efficient way to clear LSB
                  continue;
             }

            // Try placing the number
            place(cell, num);

            if (solveInternal()) return true; // Found a solution down this path

            // Backtrack: remove the number
            remove(cell, num);

            // Clear the bit corresponding to the tried number
            // Using `possible &= ~(1 << num);` or `possible ^= (1 << num);` is correct
            // Using `possible &= possible - 1;` also works to clear the lowest set bit
            possible &= ~(1 << num);
        }

        return false; // No number worked for this cell in this branch
    }


public:
    // Initialize the solver for a single puzzle string
    bool initialize(const char* puzzle_data, size_t len) {
        if (len != 81) return false; // Safety check

        emptyCount = 0;
        // Use memset for POD types
        memset(rows, 0, sizeof(rows));
        memset(cols, 0, sizeof(cols));
        memset(boxes, 0, sizeof(boxes));
        // Use memcpy for the grid
        memcpy(grid, puzzle_data, 81);

        // Initialize position array to a default value (e.g., 255 or 81)
        // to indicate cells not currently in emptyCells list
        memset(position, 81, sizeof(position));


        for (int i = 0; i < 81; ++i) {
            if (grid[i] >= '1' && grid[i] <= '9') { // Check for valid digits
                const int num = grid[i] - '0';
                // Check for validity during initialization
                const uint16_t mask = 1 << num;
                const auto& info = preCell[i];
                 if ((rows[info.row] & mask) || (cols[info.col] & mask) || (boxes[info.box] & mask)) {
                    // std::cerr << "Warning: Invalid puzzle input at cell " << i << " (duplicate number " << num << ")\n";
                     // Optionally return false or just mark it invalid and continue
                     // To simplify, we'll allow the solver to potentially fail later if the input is invalid
                     // For strict validation, return false here:
                     // return false;
                 }
                rows[info.row] |= mask;
                cols[info.col] |= mask;
                boxes[info.box] |= mask;
            } else {
                grid[i] = '0'; // Ensure non-digits or '.' are treated as empty
                // Add to empty list
                 if (emptyCount < 81) {
                    emptyCells[emptyCount] = i;
                    position[i] = emptyCount; // Store its index within emptyCells
                    emptyCount++;
                 } else {
                      // Should not happen if len == 81
                      // std::cerr << "Error: More than 81 cells encountered during init.\n";
                      return false;
                 }
            }
        }
        return true;
    }

    bool solve() {
        // Check initial validity before trying to solve (optional but good)
        for(int r=0; r<9; ++r) if(__builtin_popcount(rows[r] & 0x3FE) != __builtin_popcount(rows[r])) return false;
        for(int c=0; c<9; ++c) if(__builtin_popcount(cols[c] & 0x3FE) != __builtin_popcount(cols[c])) return false;
        for(int b=0; b<9; ++b) if(__builtin_popcount(boxes[b] & 0x3FE) != __builtin_popcount(boxes[b])) return false;

        return solveInternal();
    }
    const char* getSolution() const { return grid; }
};

// --- Worker function with Output Batching ---
void solverWorker(
    size_t workerId, // Just for potential logging
    const std::string& inputFilename,
    std::ofstream& outputFile, // Pass stream by reference
    std::mutex& outputMutex,   // Pass mutex by reference
    size_t startLine, // 0-based index of the first line to process
    size_t endLine,   // 0-based index of the line AFTER the last one to process
    std::atomic<size_t>& solvedCounter,
    std::atomic<size_t>& processedCounter) // Track total lines processed by workers
{
    thread_local SudokuSolver solver; // Keep solver thread-local
    std::ifstream inputFile(inputFilename);
    if (!inputFile) {
        // Cannot open file in this thread, maybe log and return?
        // Or rely on the main thread check.
        return;
    }
    std::string line;
    size_t currentLine = 0; // Line number *within the file*

    // --- Batching variables ---
    std::string outputBuffer;
    constexpr size_t BATCH_SIZE = 200; // Write every N results (tune this)
    outputBuffer.reserve(BATCH_SIZE * 82); // Pre-allocate buffer
    size_t resultsInBatch = 0;
    // --- End Batching variables ---

    // Skip lines before the assigned start line
    while (currentLine < startLine && std::getline(inputFile, line)) {
        currentLine++;
    }

    // Process assigned lines (from startLine up to, but not including, endLine)
    while (currentLine < endLine && std::getline(inputFile, line)) {
        processedCounter.fetch_add(1, std::memory_order_relaxed); // Count processed line
        std::string original_line = line; // Keep original for error messages
        line.erase(std::remove_if(line.begin(), line.end(),
            [](unsigned char c) { return std::isspace(c); }), line.end());

        std::string result_line; // To store the result or error message
        bool was_solved = false;

        if (line.size() == 81) {
             if (solver.initialize(line.data(), line.size())) {
                 if (solver.solve()) { // Attempt to solve
                    result_line.assign(solver.getSolution(), 81);
                    was_solved = true;
                 } else {
                    // Puzzle was valid but unsolvable
                    result_line = "Unsolvable puzzle on line " + std::to_string(currentLine + 1); // + ": " + original_line; (optional)
                 }
             } else {
                // Puzzle was invalid during initialization
                result_line = "Invalid puzzle input on line " + std::to_string(currentLine + 1); //+ ": " + original_line; (optional)
             }
        } else if (!line.empty()) {
            // Handle malformed line
            result_line = "Skipping malformed line " + std::to_string(currentLine + 1); // + ": " + original_line; (optional)
        } else {
             // Skip empty lines silently or log them
             result_line = "Skipping empty line " + std::to_string(currentLine + 1);
        }


        // Append result/error to buffer
        if (!result_line.empty()) {
            outputBuffer.append(result_line);
            outputBuffer.push_back('\n');
            resultsInBatch++;
            if (was_solved) {
                solvedCounter.fetch_add(1, std::memory_order_relaxed);
            }
        }


        // Write buffer if batch is full
        if (resultsInBatch >= BATCH_SIZE) {
            std::lock_guard<std::mutex> lock(outputMutex);
            outputFile.write(outputBuffer.data(), outputBuffer.size()); // More efficient write
            outputBuffer.clear();       // Clear for the next batch (keeps capacity)
            resultsInBatch = 0;
        }

        currentLine++;
    }

    // --- Write any remaining results in the buffer after the loop ---
    if (!outputBuffer.empty()) {
        std::lock_guard<std::mutex> lock(outputMutex);
        outputFile.write(outputBuffer.data(), outputBuffer.size());
    }
    // inputFile is closed automatically by RAII when function exits
}


int main(int argc, char* argv[]) { // Allow command-line args for files
    auto startTime = std::chrono::high_resolution_clock::now();
    reportMemoryUsage("Initial");

    std::string inputFilename = "input.txt";
    std::string outputFilename = "output.txt";

    // Optional: Override filenames from command line
    if (argc > 1) inputFilename = argv[1];
    if (argc > 2) outputFilename = argv[2];

    size_t puzzleCount = 0; // Count *potential* puzzle lines (length 81 after trim)

    // --- Stage 1: Count potential puzzles ---
    { // Scope to close the file quickly
        std::ifstream counterInput(inputFilename);
        if (!counterInput) {
            std::cerr << "Error: Cannot open input file: " << inputFilename << std::endl;
            return 1;
        }
        std::string line;
        while (std::getline(counterInput, line)) {
             line.erase(std::remove_if(line.begin(), line.end(),
                 [](unsigned char c) { return std::isspace(c); }), line.end());
             if (line.size() == 81) {
                 puzzleCount++;
             }
             // You could add counts for malformed/empty lines here too if desired
        }
    } // counterInput closed here

    if (puzzleCount == 0) {
        std::cout << "No potential puzzles (lines with 81 non-space chars) found in " << inputFilename << std::endl;
        // Decide if you want to proceed if there are non-puzzle lines
        // Let's proceed to process other lines for error reporting
        // return 0;
    }
     reportMemoryUsage("After Count");
    std::cout << "Found " << puzzleCount << " potential puzzles." << std::endl;


    // --- Stage 2: Setup Workers ---
    // Determine number of threads
    unsigned hardware_threads = std::thread::hardware_concurrency();
    const unsigned numThreads = 6; // Use at least 1 thread

    std::vector<std::thread> workers;
    std::ofstream outputFile(outputFilename); // Open output file *once*
     if (!outputFile) {
         std::cerr << "Error: Cannot open output file: " << outputFilename << std::endl;
         return 1;
     }
    std::mutex outputMutex; // Mutex to protect the single output file stream
    std::atomic<size_t> solvedCounter(0); // Use atomic for safe counting of *successfully solved*
    std::atomic<size_t> processedCounter(0); // Count lines attempted by workers

    // --- Re-calculate line count for accurate distribution ---
    // This is needed because the first count was approximate. We need total lines.
    size_t totalLines = 0;
     {
        std::ifstream lineCounter(inputFilename);
        std::string dummy;
        while (std::getline(lineCounter, dummy)) {
            totalLines++;
        }
     }
     std::cout << "Total lines in file: " << totalLines << std::endl;
     // --- End Re-calculate ---


    size_t startLine = 0;
    // Calculate lines per thread based on *total lines*, not just puzzle lines
    size_t linesPerThread = (totalLines + numThreads - 1) / numThreads; // Ceiling division

    std::cout << "Using " << numThreads << " threads." << std::endl;

    for (unsigned i = 0; i < numThreads; ++i) {
        size_t endLine = std::min(startLine + linesPerThread, totalLines);
        if (startLine < endLine) { // Ensure we don't create threads for empty ranges
            workers.emplace_back(solverWorker, i, std::cref(inputFilename),
                                 std::ref(outputFile), std::ref(outputMutex),
                                 startLine, endLine,
                                 std::ref(solvedCounter), std::ref(processedCounter));
            std::cout << "  Thread " << i << " processing lines " << startLine << " to " << endLine -1 << "\n";
        } else {
            // This might happen if linesPerThread is 0 or totalLines is very small
            std::cout << "  Skipping thread creation for empty range starting at line " << startLine << "\n";
        }
        startLine = endLine;
        if (startLine >= totalLines) break; // Stop if all lines assigned
    }

    reportMemoryUsage("After Thread Creation");

    // --- Stage 3: Join Workers ---
    for (auto& t : workers) {
        if (t.joinable()) {
             t.join();
        }
    }

    reportMemoryUsage("After Join");

    // Ensure output buffer is flushed by closing the file (RAII handles this, but explicit flush is fine too)
    outputFile.flush();
    outputFile.close();


    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "----------------------------------------\n";
    std::cout << "Finished processing.\n";
    std::cout << "  Total lines processed by workers: " << processedCounter.load() << " (should match total lines: " << totalLines << ")\n";
    std::cout << "  Successfully solved puzzles:      " << solvedCounter.load() << "\n";
    std::cout << "  Total execution time:           " << duration << " ms\n";
    std::cout << "----------------------------------------\n";

    return 0;
}