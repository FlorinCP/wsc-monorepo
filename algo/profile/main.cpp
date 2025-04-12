#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <array>
#include <iomanip>
#include <mutex>
#include <map>
#include <unordered_map>
#include <sstream>

// Mutex for thread-safe console output
std::mutex cout_mutex;

// Profiling class for measuring performance
class ProfilerTimer {
    std::string name;
    std::chrono::high_resolution_clock::time_point start_time;
    bool completed = false;

public:
    // Use maps for more flexible operation tracking
    static std::unordered_map<std::string, uint64_t> total_times;
    static std::unordered_map<std::string, uint64_t> call_counts;
    static std::mutex profiler_mutex;

    ProfilerTimer(const std::string& operation_name)
        : name(operation_name), start_time(std::chrono::high_resolution_clock::now()) {}

    ~ProfilerTimer() {
        if (!completed) {
            stop();
        }
    }

    void stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

        std::lock_guard<std::mutex> lock(profiler_mutex);
        total_times[name] += duration;
        call_counts[name]++;

        completed = true;
    }

    static void print_results() {
        std::lock_guard<std::mutex> lock(cout_mutex);

        // Collect all operation names
        std::vector<std::string> operations;
        for (const auto& [name, _] : total_times) {
            operations.push_back(name);
        }

        // Sort operations by total time (descending)
        std::sort(operations.begin(), operations.end(), [](const std::string& a, const std::string& b) {
            return total_times[a] > total_times[b];
        });

        // Print header
        std::cout << "\nPerformance Profile:\n";
        std::cout << std::string(80, '-') << "\n";
        std::cout << std::left << std::setw(30) << "Operation"
                 << std::right << std::setw(15) << "Total Time (ms)"
                 << std::right << std::setw(15) << "Calls"
                 << std::right << std::setw(20) << "Avg Time (µs)" << "\n";
        std::cout << std::string(80, '-') << "\n";

        // Print operations
        uint64_t grand_total = 0;
        for (const auto& op : operations) {
            double total_ms = total_times[op] / 1000000.0;
            double avg_us = total_times[op] / (1000.0 * call_counts[op]);
            grand_total += total_times[op];

            std::cout << std::left << std::setw(30) << op
                     << std::right << std::setw(15) << std::fixed << std::setprecision(2) << total_ms
                     << std::right << std::setw(15) << call_counts[op]
                     << std::right << std::setw(20) << std::fixed << std::setprecision(3) << avg_us << "\n";
        }

        // Print total
        double grand_total_ms = grand_total / 1000000.0;
        std::cout << std::string(80, '-') << "\n";
        std::cout << std::left << std::setw(30) << "GRAND TOTAL"
                 << std::right << std::setw(15) << std::fixed << std::setprecision(2) << grand_total_ms
                 << std::right << std::setw(35) << "" << "\n";
        std::cout << std::string(80, '-') << "\n";
    }

    // Export profiling data to CSV for visualization
    static void export_to_csv(const std::string& filename) {
        std::ofstream csv(filename);
        if (!csv) {
            std::cerr << "Failed to open " << filename << " for writing\n";
            return;
        }

        csv << "Operation,Total_Time_ms,Calls,Avg_Time_us,Percentage\n";

        // Calculate grand total
        uint64_t grand_total = 0;
        for (const auto& [_, time] : total_times) {
            grand_total += time;
        }

        // Collect and sort operations
        std::vector<std::string> operations;
        for (const auto& [name, _] : total_times) {
            operations.push_back(name);
        }

        std::sort(operations.begin(), operations.end(), [](const std::string& a, const std::string& b) {
            return total_times[a] > total_times[b];
        });

        // Write data
        for (const auto& op : operations) {
            double total_ms = total_times[op] / 1000000.0;
            double avg_us = total_times[op] / (1000.0 * call_counts[op]);
            double percentage = (grand_total > 0) ? (100.0 * total_times[op] / grand_total) : 0.0;

            csv << "\"" << op << "\","
                << std::fixed << std::setprecision(3) << total_ms << ","
                << call_counts[op] << ","
                << std::fixed << std::setprecision(3) << avg_us << ","
                << std::fixed << std::setprecision(2) << percentage << "\n";
        }

        std::cout << "Profiling data exported to " << filename << std::endl;
    }
};

std::unordered_map<std::string, uint64_t> ProfilerTimer::total_times;
std::unordered_map<std::string, uint64_t> ProfilerTimer::call_counts;
std::mutex ProfilerTimer::profiler_mutex;

#define PROFILE_SCOPE(name) ProfilerTimer timer##__LINE__(name)
#define PROFILE_FUNCTION() ProfilerTimer functionTimer(__func__)

// Optional memory usage tracking
class MemoryTracker {
public:
    static void print_memory_usage() {
    #ifdef __linux__
        std::ifstream status("/proc/self/status");
        if (!status) return;

        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmRSS:") != std::string::npos ||
                line.find("VmSize:") != std::string::npos) {
                std::cout << line << std::endl;
            }
        }
    #else
        std::cout << "Memory tracking only available on Linux systems" << std::endl;
    #endif
    }
};

// Structure to hold precomputed cell information
struct CellInfo {
    uint8_t row, col, box;
};

// Precomputed cell information mapping cell index to row/col/box
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

// Structure to hold affected cells for each position
struct AffectedCellsInfo {
    uint8_t count;
    uint8_t cells[20];  // Max 20 affected cells per cell (row + col + box - duplicates)
};

// Precomputed affected cells (cells in same row, column, or box)
static AffectedCellsInfo affectedCells[81];

// Initialize precomputed tables
void initializeAffectedCells() {
    // For each cell, find all cells that share a row, column, or box
    for (int cell = 0; cell < 81; ++cell) {
        const auto& info = preCell[cell];
        std::vector<uint8_t> affected;

        // Add cells in the same row
        for (int c = 0; c < 9; ++c) {
            if (c != info.col) {
                affected.push_back(info.row * 9 + c);
            }
        }

        // Add cells in the same column
        for (int r = 0; r < 9; ++r) {
            if (r != info.row) {
                affected.push_back(r * 9 + info.col);
            }
        }

        // Add cells in the same box
        int boxStartRow = (info.box / 3) * 3;
        int boxStartCol = (info.box % 3) * 3;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                int affectedCell = (boxStartRow + r) * 9 + (boxStartCol + c);
                if (affectedCell != cell) {
                    affected.push_back(affectedCell);
                }
            }
        }

        // Remove duplicates
        std::sort(affected.begin(), affected.end());
        affected.erase(std::unique(affected.begin(), affected.end()), affected.end());

        // Store the affected cells
        affectedCells[cell].count = affected.size();
        for (size_t i = 0; i < affected.size(); ++i) {
            affectedCells[cell].cells[i] = affected[i];
        }
    }
}

// Precomputed bit-count lookup table for faster popcount
static uint8_t bitCountTable[1024];

// Initialize the bit count table
void initializeBitCountTable() {
    for (int i = 0; i < 1024; ++i) {
        // Count the bits manually
        int count = 0;
        int value = i;
        while (value) {
            count += value & 1;
            value >>= 1;
        }
        bitCountTable[i] = count;
    }
}

// Initialize all precomputed tables
void initializeTables() {
    PROFILE_SCOPE("TableInitialization");
    initializeAffectedCells();
    initializeBitCountTable();
}

// Statistics tracking for Sudoku solving
struct SolverStats {
    size_t backtracks = 0;
    size_t placements = 0;
    size_t removals = 0;
    size_t mrv_calls = 0;

    void reset() {
        backtracks = 0;
        placements = 0;
        removals = 0;
        mrv_calls = 0;
    }

    void print() {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Solver Statistics:\n";
        std::cout << "  Backtracks: " << backtracks << "\n";
        std::cout << "  Placements: " << placements << "\n";
        std::cout << "  Removals: " << removals << "\n";
        std::cout << "  MRV Calls: " << mrv_calls << "\n";
        std::cout << "  Backtrack ratio: " << (double)backtracks / placements << "\n";
    }
};

class SudokuSolver {
    // Aligned data structures for better cache usage
    alignas(64) uint16_t rows[9] = {0};
    alignas(64) uint16_t cols[9] = {0};
    alignas(64) uint16_t boxes[9] = {0};

    // Array of possibilities for each cell (0x3FE = 0b1111111110 represents digits 1-9)
    alignas(64) uint16_t cellPossibilities[81];

    // Count of possibilities for each cell
    alignas(64) uint8_t possibilityCounts[81];

    char grid[81];
    uint8_t emptyCells[81];
    uint8_t position[81];
    int emptyCount = 0;

    // Track the MRV cell and count to avoid full recalculation
    int currentMRVCell = -1;
    int currentMRVCount = 10;

    // Statistics
    SolverStats stats;

    // Configuration options
    bool enableCaching = true;
    bool useFastPopcount = true;

    // Fast inline popcount for 16-bit integers
    __attribute__((always_inline))
    inline int fastPopCount(uint16_t x) const {
        PROFILE_SCOPE("fastPopCount");

        if (!useFastPopcount) {
            return __builtin_popcount(x);
        }

        // Use our precomputed lookup table for smaller values
        if (x < 1024) {
            return bitCountTable[x];
        }

        // For larger values, use built-in function
        return __builtin_popcount(x);
    }

    __attribute__((always_inline))
    inline bool canPlace(int cell, int num) const {
        PROFILE_SCOPE("canPlace");
        // Just check if the bit is set in the possibilities
        return (cellPossibilities[cell] & (1 << num)) != 0;
    }

    __attribute__((always_inline))
    inline void place(int cell, int num) {
        PROFILE_SCOPE("place");
        stats.placements++;

        const auto& info = preCell[cell];
        const uint16_t mask = 1 << num;

        // Set the digit in the constraint arrays
        rows[info.row] |= mask;
        cols[info.col] |= mask;
        boxes[info.box] |= mask;
        grid[cell] = '0' + num;

        // Update the empty cells list
        const int pos = position[cell];
        const uint8_t last = emptyCells[--emptyCount];
        emptyCells[pos] = last;
        position[last] = pos;

        // Reset MRV tracking if caching is enabled
        if (enableCaching) {
            currentMRVCell = -1;
            currentMRVCount = 10;
        }

        // Update possibilities for affected cells
        updatePossibilities(cell, num);
    }

    __attribute__((always_inline))
    inline void remove(int cell, int num) {
        PROFILE_SCOPE("remove");
        stats.removals++;
        stats.backtracks++;

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

        // Reset MRV tracking if caching is enabled
        if (enableCaching) {
            currentMRVCell = -1;
            currentMRVCount = 10;
        }

        // Recompute possibilities for affected cells
        recomputePossibilities(cell, num);
    }

    // Update possibilities when a value is placed
    __attribute__((always_inline))
    inline void updatePossibilities(int cell, int num) {
        PROFILE_SCOPE("updatePossibilities");

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

                // Update MRV tracking if caching is enabled
                if (enableCaching && currentMRVCount > possibilityCounts[affectedCell] &&
                    possibilityCounts[affectedCell] > 0) {
                    currentMRVCount = possibilityCounts[affectedCell];
                    currentMRVCell = affectedCell;
                }
            }
        }
    }

    // Recompute possibilities when backtracking
    __attribute__((always_inline))
    inline void recomputePossibilities(int cell, int num) {
        PROFILE_SCOPE("recomputePossibilities");

        // For each affected cell, recompute possibilities
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

    // Find cell with Minimum Remaining Values
    __attribute__((always_inline))
    inline int findMRV() {
        PROFILE_SCOPE("findMRV");
        stats.mrv_calls++;

        // If caching is enabled and we have a cached MRV cell that's still valid, use it
        if (enableCaching && currentMRVCell >= 0 && grid[currentMRVCell] == '0') {
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

        // Cache the result if enabled
        if (enableCaching) {
            currentMRVCell = bestCell;
            currentMRVCount = minCount;
        }

        return bestCell;
    }

    bool solveInternal() {
        PROFILE_SCOPE("solveInternal");

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
    // Constructor with configuration options
    SudokuSolver(bool enableMRVCaching = true, bool useFastPopcountTable = true)
        : enableCaching(enableMRVCaching), useFastPopcount(useFastPopcountTable) {}

    void initialize(const std::string& puzzle) {
        PROFILE_FUNCTION();

        emptyCount = 0;
        memset(rows, 0, sizeof(rows));
        memset(cols, 0, sizeof(cols));
        memset(boxes, 0, sizeof(boxes));
        memcpy(grid, puzzle.data(), 81);

        // Reset MRV tracking
        currentMRVCell = -1;
        currentMRVCount = 10;

        // Reset statistics
        stats.reset();

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

    bool solve() {
        PROFILE_FUNCTION();
        return solveInternal();
    }

    const char* getSolution() const { return grid; }

    // Get solver statistics
    const SolverStats& getStats() const { return stats; }
};

// Thread-safe results manager
class ResultsManager {
    std::vector<std::array<char, 81>> results;
    std::mutex resultsMutex;

public:
    ResultsManager(size_t size) : results(size) {}

    void setResult(size_t index, const char* solution) {
        std::lock_guard<std::mutex> lock(resultsMutex);
        std::memcpy(results[index].data(), solution, 81);
    }

    const std::vector<std::array<char, 81>>& getResults() const { return results; }
};

// Worker thread that processes a chunk of puzzles
void solverWorker(const std::vector<std::string>& puzzles, ResultsManager& manager,
                 size_t start, size_t end, int workerId) {
    std::ostringstream threadName;
    threadName << "Worker-" << workerId;

    PROFILE_SCOPE(threadName.str());

    // Each thread has its own solver instance
    SudokuSolver solver;

    for (size_t i = start; i < end; ++i) {
        PROFILE_SCOPE(threadName.str() + "-Puzzle" + std::to_string(i));

        // Process one puzzle
        solver.initialize(puzzles[i]);
        solver.solve();
        manager.setResult(i, solver.getSolution());
    }
}

// Command line options parser
struct Options {
    std::string inputFile = "input.txt";
    std::string outputFile = "output.txt";
    std::string profileOutputFile = "profile_data.csv";
    bool enableThreads = true;
    bool enableProfiling = true;
    bool exportProfileData = true;
    int numThreads = 0;  // 0 means auto-detect
    
    void parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-i" || arg == "--input") {
                if (i + 1 < argc) inputFile = argv[++i];
            } else if (arg == "-o" || arg == "--output") {
                if (i + 1 < argc) outputFile = argv[++i];
            } else if (arg == "-p" || arg == "--profile") {
                if (i + 1 < argc) profileOutputFile = argv[++i];
            } else if (arg == "-t" || arg == "--threads") {
                if (i + 1 < argc) numThreads = std::stoi(argv[++i]);
            } else if (arg == "--no-threads") {
                enableThreads = false;
            } else if (arg == "--no-profile") {
                enableProfiling = false;
            } else if (arg == "--no-export") {
                exportProfileData = false;
            } else if (arg == "-h" || arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                          << "Options:\n"
                          << "  -i, --input FILE       Input file (default: input.txt)\n"
                          << "  -o, --output FILE      Output file (default: output.txt)\n"
                          << "  -p, --profile FILE     Profile data output file (default: profile_data.csv)\n"
                          << "  -t, --threads N        Number of threads (default: auto)\n"
                          << "  --no-threads           Disable multi-threading\n"
                          << "  --no-profile           Disable profiling\n"
                          << "  --no-export            Disable profile data export\n"
                          << "  -h, --help             Show this help message\n";
                exit(0);
            }
        }
    }
    
    void print() {
        std::cout << "Running with options:\n"
                  << "  Input file: " << inputFile << "\n"
                  << "  Output file: " << outputFile << "\n";
        if (enableProfiling) {
            std::cout << "  Profiling: enabled\n";
            if (exportProfileData) {
                std::cout << "  Profile output: " << profileOutputFile << "\n";
            }
        } else {
            std::cout << "  Profiling: disabled\n";
        }
        
        if (enableThreads) {
            std::cout << "  Threading: enabled";
            if (numThreads > 0) {
                std::cout << " (" << numThreads << " threads)";
            } else {
                std::cout << " (auto)";
            }
            std::cout << "\n";
        } else {
            std::cout << "  Threading: disabled\n";
        }
    }
};

int main(int argc, char* argv[]) {
    Options options;
    options.parse(argc, argv);
    options.print();
    
    // Overall timing
    PROFILE_SCOPE("Total");
    auto startTime = std::chrono::high_resolution_clock::now();

    // Read the input file
    std::vector<std::string> puzzles;
    {
        PROFILE_SCOPE("FileReading");
        std::ifstream input(options.inputFile);
        if (!input) {
            std::cerr << "Failed to open input file: " << options.inputFile << std::endl;
            return 1;
        }

        std::string line;
        while (std::getline(input, line)) {
            line.erase(std::remove_if(line.begin(), line.end(),
                [](unsigned char c) { return std::isspace(c); }), line.end());
            if (line.size() == 81) puzzles.push_back(line);
        }
        
        std::cout << "Read " << puzzles.size() << " puzzles from " << options.inputFile << std::endl;
    }

    // Check if there are any puzzles to solve
    if (puzzles.empty()) {
        std::cerr << "No valid puzzles found in input file" << std::endl;
        return 1;
    }

    // Set up the multi-threading
    ResultsManager results(puzzles.size());
    
    if (options.enableThreads) {
        PROFILE_SCOPE("Multithreaded-Solving");
        
        const unsigned availableThreads = std::thread::hardware_concurrency();
        unsigned threads = (options.numThreads > 0) ? 
            options.numThreads : std::max(1u, std::min<unsigned>(availableThreads, puzzles.size()));
        
        std::cout << "Using " << threads << " threads to solve " << puzzles.size() << " puzzles" << std::endl;
        
        std::vector<std::thread> workers;
        size_t start = 0;
        const size_t chunk = (puzzles.size() + threads - 1) / threads;

        for (unsigned i = 0; i < threads; ++i) {
            size_t end = std::min(start + chunk, puzzles.size());
            workers.emplace_back(solverWorker, std::ref(puzzles), std::ref(results), start, end, i);
            start = end;
        }

        for (auto& t : workers) t.join();
    } else {
        // Single-threaded solving
        PROFILE_SCOPE("Single-Threaded-Solving");
        std::cout << "Using single thread to solve " << puzzles.size() << " puzzles" << std::endl;
        
        SudokuSolver solver;
        for (size_t i = 0; i < puzzles.size(); ++i) {
            PROFILE_SCOPE("Puzzle-" + std::to_string(i));
            solver.initialize(puzzles[i]);
            solver.solve();
            results.setResult(i, solver.getSolution());
        }
    }

    // Write the solutions to the output file
    {
        PROFILE_SCOPE("FileWriting");
        std::ofstream output(options.outputFile);
        if (!output) {
            std::cerr << "Failed to open output file: " << options.outputFile << std::endl;
            return 1;
        }

        for (const auto& s : results.getResults()) {
            output.write(s.data(), s.size());
            output << '\n';
        }

        std::cout << "Wrote " << results.getResults().size() << " solutions to " << options.outputFile << std::endl;
    }

    // Calculate and report the overall execution time
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime).count();
    std::cout << "Solved " << puzzles.size() << " puzzles in " << duration << " ms" << std::endl;

    // Print memory usage information if available
    MemoryTracker::print_memory_usage();

    // Print profiling results if enabled
    if (options.enableProfiling) {
        ProfilerTimer::print_results();

        if (options.exportProfileData) {
            ProfilerTimer::export_to_csv(options.profileOutputFile);
        }
    }

    return 0;
}