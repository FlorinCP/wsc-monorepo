#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <array>
#include <bitset>
#include <numeric>
#include <cctype> // Include for std::isspace

// Structure to precompute cell information (row, column, 3x3 box)
struct CellInfo {
    uint8_t row, col, box;
};

// Precomputed cell information for all 81 cells
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
    alignas(64) std::bitset<9> rows[9];
    alignas(64) std::bitset<9> cols[9];
    alignas(64) std::bitset<9> boxes[9];
    char grid[81];          // Stores the puzzle grid ('0' for empty, '1'-'9' for numbers)
    uint8_t emptyCells[81]; // Array storing indices of empty cells
    uint8_t position[81];   // Maps cell index to its position in emptyCells array
    int emptyCount = 0;     // Number of empty cells remaining

    // Check if a number can be placed in a specific cell
    __attribute__((always_inline))
    inline bool canPlace(int cell, int num) const {
        const auto& info = preCell[cell];
        // Check row, column, and box constraints using bitsets
        // No fix needed here as member access is correct once declarations are fixed
        return !(rows[info.row].test(num) || cols[info.col].test(num) || boxes[info.box].test(num));
    }

    // Place a number in a cell and update constraints and empty cell tracking
    __attribute__((always_inline))
    inline void place(int cell, int num) {
        const auto& info = preCell[cell];
        // Set the bit corresponding to 'num' in the respective row, col, box bitsets
        // No fix needed here
        rows[info.row].set(num);
        cols[info.col].set(num);
        boxes[info.box].set(num);
        grid[cell] = '1' + num; // Update grid character

        // Remove the cell from the list of empty cells efficiently
        const int pos = position[cell]; // Get current position in emptyCells
        const uint8_t last = emptyCells[--emptyCount]; // Get the last empty cell index
        emptyCells[pos] = last;   // Move the last empty cell to the current position
        position[last] = pos;     // Update the position mapping for the moved cell
    }

    // Remove a number from a cell (backtracking) and update constraints
    __attribute__((always_inline))
    inline void remove(int cell, int num) {
        const auto& info = preCell[cell];
        // Reset the bit corresponding to 'num'
        // No fix needed here
        rows[info.row].reset(num);
        cols[info.col].reset(num);
        boxes[info.box].reset(num);
        grid[cell] = '0'; // Reset grid character

        // Add the cell back to the list of empty cells
        position[cell] = emptyCount; // Map the cell to the end of the current empty list
        emptyCells[emptyCount++] = cell; // Add the cell index back
    }

    // Find the empty cell with the Minimum Remaining Values (MRV heuristic)
    __attribute__((always_inline))
    int findMRV() const {
        int minCount = 10, bestCell = -1;
        // Iterate through the current list of empty cells
        for (int i = 0; i < emptyCount; ++i) {
            const int cell = emptyCells[i];
            const auto& info = preCell[cell];
            // --- FIX: Added template argument <9> ---
            // Explicitly type 'used' or rely on auto deduction (which works once members are fixed)
            // std::bitset<9> used = rows[info.row] | cols[info.col] | boxes[info.box];
            // Using auto is fine here now:
            auto used = rows[info.row] | cols[info.col] | boxes[info.box];
            // --- End Fix ---
            const int count = (9 - used.count()); // Calculate remaining possible values
            if (count == 0) return -1; // Contradiction found (should not happen in valid puzzles)
            if (count < minCount) {
                minCount = count;
                bestCell = cell;
                if (minCount == 1) break; // Optimization: If only 1 value is possible, choose it immediately
            }
        }
        return bestCell;
    }

    // Recursive backtracking solver function
    bool solveInternal() {
        if (emptyCount == 0) return true; // Base case: no empty cells left, puzzle solved

        const int cell = findMRV(); // Find the best cell to try next
        if (cell == -1) return false; // No valid cell/move found (backtrack)

        const auto& info = preCell[cell];
        // --- FIX: Added template argument <9> ---
        // Explicitly type 'used' or rely on auto deduction
        // std::bitset<9> used = rows[info.row] | cols[info.col] | boxes[info.box];
        // Using auto is fine here now:
        auto used = rows[info.row] | cols[info.col] | boxes[info.box];
        // --- End Fix ---

        // Try placing numbers 0-8 (representing 1-9)
        for (int num = 0; num < 9; ++num) {
            if (!used.test(num)) { // If the number 'num' is not already used in row/col/box
                place(cell, num);
                if (solveInternal()) return true; // Recurse; if it returns true, solution found
                remove(cell, num); // Backtrack: undo the placement
            }
        }
        return false; // None of the numbers worked for this cell, backtrack
    }

public:
    // Initialize the solver with a puzzle string (81 chars, '0' for empty)
    void initialize(const std::string& puzzle) {
        emptyCount = 0;
        // Reset all constraint bitsets
        // No fix needed here, loop is correct
        for(int i = 0; i < 9; ++i) rows[i].reset();
        for(int i = 0; i < 9; ++i) cols[i].reset();
        for(int i = 0; i < 9; ++i) boxes[i].reset();
        memcpy(grid, puzzle.data(), 81); // Copy puzzle string to grid

        // Process the initial grid state
        for (int i = 0; i < 81; ++i) {
            if (grid[i] >= '1' && grid[i] <= '9') { // Added check for validity
                const int num = grid[i] - '1'; // Convert char '1'-'9' to int 0-8
                const auto& info = preCell[i];
                // Check for initial contradictions (optional but good practice)
                if (rows[info.row].test(num) || cols[info.col].test(num) || boxes[info.box].test(num)) {
                    // Handle error: Invalid initial puzzle state
                    // For simplicity here, we'll just continue, but a real implementation might throw or return false
                    // std::cerr << "Warning: Initial puzzle contradiction at cell " << i << " value " << grid[i] << std::endl;
                }
                // Set initial constraints based on pre-filled cells
                // No fix needed here
                rows[info.row].set(num);
                cols[info.col].set(num);
                boxes[info.box].set(num);
            } else if (grid[i] == '0') { // Changed else to else if for clarity/safety
                // Add empty cell to tracking lists
                emptyCells[emptyCount] = i;
                position[i] = emptyCount;
                emptyCount++;
            } else {
                 // Handle unexpected character in input string
                 // std::cerr << "Warning: Invalid character '" << grid[i] << "' at index " << i << " in puzzle. Treating as empty." << std::endl;
                 grid[i] = '0'; // Treat invalid char as empty
                 emptyCells[emptyCount] = i;
                 position[i] = emptyCount;
                 emptyCount++;
            }
        }
    }


    // Public method to start the solving process
    bool solve() {
         if (emptyCount < 0) return false; // Check if initialization found contradiction (if error handling added above)
         return solveInternal();
    }

    // Get the solved grid (or the original grid if not solved/solvable)
    const char* getSolution() const { return grid; }
};

class ResultsManager {
    std::vector<std::array<char, 81>> results; // Vector to hold solved grids
public:
    ResultsManager(size_t size) : results(size) {} // Constructor initializes vector size

    void setResult(size_t index, const char* solution) {
        std::memcpy(results[index].data(), solution, 81);
    }

    const std::vector<std::array<char, 81>>& getResults() const { return results; }
};

// Function executed by each worker thread
void solverWorker(const std::vector<std::string>& puzzles, ResultsManager& manager, size_t start, size_t end) {
    // Each thread gets its own solver instance
    SudokuSolver solver; // thread_local is not strictly necessary here as it's a local variable
    for (size_t i = start; i < end; ++i) {
        solver.initialize(puzzles[i]);
        solver.solve(); // Solve the puzzle
        manager.setResult(i, solver.getSolution()); // Store the result
    }
}

int main() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::ifstream input("input.txt"); // Input file containing puzzles (one per line)
    if (!input) {
        std::cerr << "Error: Cannot open input.txt" << std::endl;
        return 1;
    }

    std::vector<std::string> puzzles;
    std::string line;
    while (std::getline(input, line)) {
        // Remove whitespace from the line more robustly
        line.erase(std::remove_if(line.begin(), line.end(),
            [](unsigned char c){ return std::isspace(c); }), line.end());

        // Add to puzzles if it's a valid 81-character grid
        if (line.size() == 81) {
            // Optional: Add validation for characters '0'-'9' here if desired
            puzzles.push_back(line);
        } else if (!line.empty()) {
            std::cerr << "Warning: Skipping invalid line (length " << line.size() << " != 81): \"" << line << "\"" << std::endl;
        }
    }
    input.close(); // Close input file

    if (puzzles.empty()) {
        std::cerr << "Error: No valid puzzles found in input.txt" << std::endl;
        return 1;
    }

    // Determine the number of threads to use
    const unsigned num_puzzles = puzzles.size(); // Avoid repeated calls to size()
    const unsigned hardware_threads = std::thread::hardware_concurrency();
    const unsigned threads = std::max(1u, std::min(hardware_threads == 0 ? 1u : hardware_threads, (unsigned)num_puzzles)); // Handle hardware_concurrency returning 0

    ResultsManager results(num_puzzles); // Create results manager
    std::vector<std::thread> workers;       // Vector to hold worker threads
    size_t start = 0;
    // Calculate chunk size per thread, ensuring all puzzles are covered
    const size_t chunk_size = (num_puzzles + threads - 1) / threads; // Ceiling division

    for (unsigned i = 0; i < threads; ++i) {
        size_t end = std::min(start + chunk_size, static_cast<size_t>(num_puzzles));
        if (start < end) { // Ensure we don't create threads for empty chunks (important if num_puzzles < threads)
            workers.emplace_back(solverWorker, std::ref(puzzles), std::ref(results), start, end);
        } else {
            break; // No more puzzles to assign
        }
        start = end;
    }

    // Wait for all worker threads to complete
    for (auto& t : workers) {
        if (t.joinable()) { // Good practice to check if joinable
           t.join();
        }
    }

    // Write the solved puzzles to the output file
    std::ofstream output("output.txt");
    if (!output) {
         std::cerr << "Error: Cannot open output.txt for writing" << std::endl;
         return 1;
    }
    for (const auto& s : results.getResults()) {
        // Ensure the array contains 81 valid characters before writing
        output.write(s.data(), 81); // Write raw character data
        output << '\n';                   // Add newline after each puzzle
    }
    output.close(); // Close output file

    // Calculate and print the execution time
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    std::cout << "Solved " << num_puzzles << " puzzles in " << duration << " ms\n";

    return 0; // Indicate successful execution
}