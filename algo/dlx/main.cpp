#include <iostream>
#include <vector>
#include <string>
#include <numeric>      // For std::iota
#include <algorithm>    // For std::min_element, std::copy, std::replace
#include <array>
#include <chrono>
#include <thread>
#include <fstream>
#include <cstring>      // For std::memcpy
#include <cctype>       // For std::isspace
#include <stdexcept>    // For std::runtime_error
#include <cstdio>       // For FILE*, fopen, fwrite, fclose (faster I/O)

// --- Configuration ---
constexpr int GRID_SIZE = 9;
constexpr int BOX_SIZE = 3;
constexpr int NUM_OPTIONS = 9; // 1-9
constexpr int CELLS = GRID_SIZE * GRID_SIZE; // 81 <-- MOVED HERE

// --- DLX Node Structures ---

struct ColumnNode; // Forward declaration

// Represents a '1' in the exact cover matrix
struct Node {
    Node *left, *right, *up, *down;
    ColumnNode* col;
    int row_id; // Identifies the choice (e.g., cell + number info)

    // Link node 'n' to the right of 'this' node
    inline void linkRight(Node* n) {
        n->right = this->right;
        n->right->left = n;
        n->left = this;
        this->right = n;
    }

    // Link node 'n' below 'this' node
    inline void linkDown(Node* n) {
        n->down = this->down;
        n->down->up = n;
        n->up = this;
        this->down = n;
    }

    // Unlink node horizontally
    inline void unlinkLR() {
        this->left->right = this->right;
        this->right->left = this->left;
    }

    // Unlink node vertically
    inline void unlinkUD() {
        this->up->down = this->down;
        this->down->up = this->up;
    }

    // Relink node horizontally
    inline void relinkLR() {
        this->left->right = this;
        this->right->left = this;
    }

    // Relink node vertically
    inline void relinkUD() {
        this->up->down = this;
        this->down->up = this;
    }
};

// Header node for a column (constraint)
struct ColumnNode : Node {
    int size; // Number of nodes (1s) in this column
    int id;   // Constraint ID (0-323)

    ColumnNode() : size(0), id(-1) {
        left = right = up = down = this; // Initialize links to self
        col = this;                     // Column header points to itself
        row_id = -1;                    // Not used for column headers typically
    }

    // Remove column 'c' and all rows intersecting it
    void cover() {
        unlinkLR(); // Remove column header from header list
        for (Node* i = this->down; i != this; i = i->down) { // For each row node in this column
            for (Node* j = i->right; j != i; j = j->right) { // For each node in that row (except itself)
                j->unlinkUD();   // Remove node from its column's list
                j->col->size--; // Decrement column size
            }
        }
    }

    // Add column 'c' back and all rows intersecting it
    void uncover() {
        // Add rows back first (iterate upwards/leftwards to reverse cover order)
        for (Node* i = this->up; i != this; i = i->up) {
            for (Node* j = i->left; j != i; j = j->left) {
                j->col->size++; // Increment column size
                j->relinkUD();  // Add node back to its column's list
            }
        }
        relinkLR(); // Add column header back to header list
    }
};

// --- Sudoku DLX Solver Class ---

class DLXSolver {
    // --- Constants for Sudoku Constraints ---
    // Note: CELLS is now defined globally
    static constexpr int NUM_CONSTRAINTS = 4; // pos, row, col, box
    static constexpr int EXACT_COVER_COLS = CELLS * NUM_CONSTRAINTS; // 324
    static constexpr int MAX_CHOICES = CELLS * NUM_OPTIONS; // 729

    ColumnNode root; // Master header node for column list
    std::vector<ColumnNode> columns; // Column headers (0-323)
    std::vector<Node> nodes; // Node pool for the sparse matrix 1s
    size_t node_count = 0;   // Number of nodes used in the pool

    std::vector<int> solution_row_ids; // Stores row_ids of the solution choices
    std::array<char, CELLS + 1> solution_grid; // Stores the final grid string (+null term)
    bool solution_found = false;

    // --- Helper Functions for Column Indices (Constraints) ---
    // Constraint 0-80: Cell (r, c) must be filled
    inline int posConstraint(int r, int c) const { return r * GRID_SIZE + c; }
    // Constraint 81-161: Row 'r' must have number 'n' (0-8)
    inline int rowNumConstraint(int r, int n) const { return CELLS + r * GRID_SIZE + n; }
    // Constraint 162-242: Col 'c' must have number 'n' (0-8)
    inline int colNumConstraint(int c, int n) const { return CELLS * 2 + c * GRID_SIZE + n; }
    // Constraint 243-323: Box 'b' must have number 'n' (0-8)
    inline int boxNumConstraint(int r, int c, int n) const {
        int box_id = (r / BOX_SIZE) * BOX_SIZE + (c / BOX_SIZE);
        return CELLS * 3 + box_id * GRID_SIZE + n;
    }

    // --- Build the DLX Matrix based on the initial Sudoku grid ---

        // --- Build the DLX Matrix based on the initial Sudoku grid ---
    void buildMatrix(const char* initial_grid) {
        solution_found = false;
        solution_row_ids.clear();
        // Keep solution_row_ids reservation across calls if desired:
        // solution_row_ids.reserve(CELLS);

        node_count = 0; // Reset node pool usage index

        // --- Reset DLX Structure ---
        root = ColumnNode(); // Reset root node (links to self)

        // Ensure vectors have the correct size (should only happen on first call typically)
        // These calls are okay as they happen *before* any nodes/pointers are stored.
        if (columns.size() != EXACT_COVER_COLS) {
            columns.resize(EXACT_COVER_COLS);
        }
        if (nodes.size() != MAX_CHOICES * NUM_CONSTRAINTS) {
            // Make sure the node pool is large enough. MAX_CHOICES * NUM_CONSTRAINTS should be sufficient.
            // If crashing near here, maybe MAX_CHOICES is underestimated?
            nodes.resize(MAX_CHOICES * NUM_CONSTRAINTS);
        }

        // **Crucial Reset:** Reset all column headers (size and links)
        for (int j = 0; j < EXACT_COVER_COLS; ++j) {
             columns[j] = ColumnNode(); // Resets links to self, size to 0
             columns[j].id = j;         // Set the ID
        }

        // Link column headers horizontally under root
        Node* current_header = &root;
        for (int j = 0; j < EXACT_COVER_COLS; ++j) {
            current_header->linkRight(&columns[j]);
            current_header = &columns[j];
        }
        // Close the horizontal loop for the header list
        root.left = current_header;
        current_header->right = &root;
        // --- End Reset ---


        // Add rows to the matrix for each possible choice (r, c, n)
        for (int r = 0; r < GRID_SIZE; ++r) {
            for (int c = 0; c < GRID_SIZE; ++c) {
                int initial_val_idx = initial_grid[r * GRID_SIZE + c] - '1'; // 0-8 or invalid

                int start_num = (initial_val_idx >= 0 && initial_val_idx < NUM_OPTIONS) ? initial_val_idx : 0;
                int end_num   = (initial_val_idx >= 0 && initial_val_idx < NUM_OPTIONS) ? initial_val_idx + 1 : NUM_OPTIONS;

                for (int num = start_num; num < end_num; ++num) {
                    int choice_row_id = r * CELLS + c * NUM_OPTIONS + num;

                    int constraint_cols[NUM_CONSTRAINTS] = {
                        posConstraint(r, c),
                        rowNumConstraint(r, num),
                        colNumConstraint(c, num),
                        boxNumConstraint(r, c, num)
                    };

                    Node* row_start_node = nullptr;
                    Node* prev_node = nullptr;
                    for (int i = 0; i < NUM_CONSTRAINTS; ++i) {
                        // **Assertion:** Check if node pool is exhausted BEFORE using node_count
                        if (node_count >= nodes.size()) {
                             // This should NOT happen with correct preallocation.
                             // If it does, MAX_CHOICES or NUM_CONSTRAINTS calculation might be wrong,
                             // or the logic has a flaw leading to excessive node creation.
                             std::cerr << "FATAL ERROR: Node pool exhausted! node_count=" << node_count
                                       << ", nodes.size()=" << nodes.size() << std::endl;
                             // You might want to output r, c, num here too for debugging.
                             throw std::runtime_error("Node pool exhausted in buildMatrix. Increase MAX_CHOICES or check logic.");
                        }

                        // Get pointer to the next available node, THEN increment count
                        Node* new_node = &nodes[node_count];
                        node_count++; // Increment AFTER getting the pointer address

                        new_node->row_id = choice_row_id;

                        // Check if constraint index is valid (should be 0-323)
                        int current_constraint_col = constraint_cols[i];
                        if (current_constraint_col < 0 || current_constraint_col >= EXACT_COVER_COLS) {
                             std::cerr << "FATAL ERROR: Invalid constraint column index: " << current_constraint_col
                                       << " for r=" << r << ", c=" << c << ", num=" << num << std::endl;
                             throw std::runtime_error("Invalid constraint column index calculated.");
                        }
                        new_node->col = &columns[current_constraint_col];

                        // Link horizontally
                        if (row_start_node == nullptr) {
                            row_start_node = new_node;
                            new_node->left = new_node;
                            new_node->right = new_node;
                        } else {
                            prev_node->linkRight(new_node);
                        }
                        prev_node = new_node;

                        // Link vertically (ensure column header's up pointer is valid)
                        columns[current_constraint_col].up->linkDown(new_node);
                        columns[current_constraint_col].size++;
                    }
                    // Close the horizontal circular link if nodes were created
                    if (row_start_node && prev_node != row_start_node) {
                        prev_node->linkRight(row_start_node);
                    } else if (row_start_node && prev_node == row_start_node) {
                         // Only one node in the row? Should not happen with NUM_CONSTRAINTS=4
                         // If it could, ensure left/right point to self correctly.
                         // row_start_node->left = row_start_node;
                         // row_start_node->right = row_start_node;
                         // This case is unlikely needed for Sudoku DLX.
                    }
                }
            }
        }
    }

    // --- The Recursive DLX Search Algorithm (Algorithm X) ---
    bool search() {
        if (root.right == &root) {
            // Base case: All columns covered - solution found!
            solution_found = true;
            return true;
        }

        // Heuristic: Choose column 'c' with the smallest size (MRV)
        ColumnNode* c = nullptr;
        int min_size = MAX_CHOICES + 1; // Initialize higher than max possible
        for (ColumnNode* j = static_cast<ColumnNode*>(root.right); j != &root; j = static_cast<ColumnNode*>(j->right)) {
            if (j->size < min_size) {
                min_size = j->size;
                c = j;
                if (min_size <= 1) break; // Optimization: cannot get smaller
            }
        }

         // If min_size is 0 here, it means a constraint cannot be satisfied.
         // This happens if the initial puzzle is invalid or we reached a dead end.
         if (c == nullptr || min_size == 0) {
             return false;
         }

        c->cover(); // Tentatively select constraint 'c'

        // Iterate through rows 'r' linked to column 'c'
        for (Node* r_node = c->down; r_node != c; r_node = r_node->down) {
            // Tentatively select row 'r_node' (representing a choice)
            solution_row_ids.push_back(r_node->row_id);

            // Cover all columns linked by other nodes 'j' in this row
            for (Node* j = r_node->right; j != r_node; j = j->right) {
                j->col->cover();
            }

            // Recurse
            if (search()) {
                return true; // Solution found down this path
            }

            // Backtrack: Uncover columns linked by row 'r_node' (in reverse order)
             for (Node* j = r_node->left; j != r_node; j = j->left) {
                 j->col->uncover();
             }

            solution_row_ids.pop_back(); // Unselect row 'r_node'
        }

        c->uncover(); // Backtrack: Uncover column 'c'
        return false; // No solution found from this path
    }

    // --- Decode the stored solution row IDs into the final grid ---
    void decodeSolution(const char* initial_grid) {
        // Start with the initial grid
        std::memcpy(solution_grid.data(), initial_grid, CELLS);

        // Fill in the cells determined by the DLX solution
        for (int row_id : solution_row_ids) {
            int val = row_id % NUM_OPTIONS;       // 0-8
            int col = (row_id / NUM_OPTIONS) % GRID_SIZE; // 0-8
            int row = row_id / (NUM_OPTIONS * GRID_SIZE); // 0-8
            // Only fill if it wasn't part of the initial grid
            if (solution_grid[row * GRID_SIZE + col] == '0' || solution_grid[row * GRID_SIZE + col] == '.') {
                 solution_grid[row * GRID_SIZE + col] = '1' + val;
            }
        }
        solution_grid[CELLS] = '\0'; // Null-terminate
    }


public:
    DLXSolver() {
         // Pre-allocate to avoid runtime reallocations
         columns.resize(EXACT_COVER_COLS);
         nodes.resize(MAX_CHOICES * NUM_CONSTRAINTS);
         solution_grid.fill('0'); // Initialize grid
         solution_grid[CELLS] = '\0';
    }

    // Public method to solve a puzzle string
    bool solve(const std::string& puzzle_str) {
        if (puzzle_str.length() != CELLS) {
            std::cerr << "Warning: Invalid puzzle string length: " << puzzle_str.length() << std::endl;
            // Copy as much as possible, ensure null termination
            size_t copy_len = std::min((size_t)CELLS, puzzle_str.size());
            std::memcpy(solution_grid.data(), puzzle_str.data(), copy_len);
            solution_grid[copy_len] = '\0';
            return false;
        }

        // Build the matrix based on the input puzzle
        // This resets internal state like node_count, solution_row_ids etc.
        buildMatrix(puzzle_str.data());

        // Run the search
        if (search()) {
            // If found, decode it into the solution grid
            decodeSolution(puzzle_str.data());
            return true;
        } else {
            // If no solution, ensure the grid reflects the original input
            std::memcpy(solution_grid.data(), puzzle_str.data(), CELLS);
            solution_grid[CELLS] = '\0';
            return false; // Indicate failure or unsolvable
        }
    }

    // Get the resulting grid (solved or original if failed)
    const char* getSolution() const {
        return solution_grid.data();
    }
};

// --- Results Manager (Thread-Safe for this specific usage pattern) ---
class ResultsManager {
    // CELLS is now global, so this is fine
    std::vector<std::array<char, CELLS + 1>> results; // Store results including null term

public:
    ResultsManager(size_t size) : results(size) {}

    void setResult(size_t index, const char* solution) {
        if (index < results.size()) {
            // Assumes solution is at least CELLS characters long
            // CELLS is now global, so this is fine
            std::memcpy(results[index].data(), solution, CELLS + 1);
        }
    }

    const std::vector<std::array<char, CELLS + 1>>& getResults() const {
        return results;
    }
};

// --- Worker Thread Function ---
void solverWorker(const std::vector<std::string>& puzzles, ResultsManager& manager, size_t start, size_t end) {
    DLXSolver solver; // Each thread gets its own solver instance
    for (size_t i = start; i < end; ++i) {
        if (i < puzzles.size()) { // Ensure index is valid
            solver.solve(puzzles[i]);
            manager.setResult(i, solver.getSolution());
        }
    }
}

// --- Main Application Logic ---
int main() {
    auto startTime = std::chrono::high_resolution_clock::now();

    // --- Faster Input Reading ---
    std::ifstream input_file("input.txt", std::ios::binary | std::ios::ate);
    if (!input_file) {
        std::cerr << "Error: Cannot open input.txt" << std::endl;
        return 1;
    }
    std::streamsize size = input_file.tellg();
    input_file.seekg(0, std::ios::beg);
    std::string file_buffer(size, '\0'); // Use std::string as buffer
    if (!input_file.read(&file_buffer[0], size)) {
         std::cerr << "Error: Could not read input.txt into buffer" << std::endl;
         input_file.close();
         return 1;
    }
    input_file.close();

    std::vector<std::string> puzzles;
    // CELLS is now global, so this is fine
    puzzles.reserve(size / CELLS + 1);
    size_t current_pos = 0;
    while (current_pos < file_buffer.size()) {
        // Find start of next potential puzzle (skip whitespace)
        size_t puzzle_start = file_buffer.find_first_not_of(" \t\r\n", current_pos);
        if (puzzle_start == std::string::npos) break; // No more non-whitespace

        // Check if enough characters remain for a puzzle
        // CELLS is now global, so this is fine
        if (puzzle_start + CELLS <= file_buffer.size()) {
             // CELLS is now global, so this is fine
             std::string potential_puzzle = file_buffer.substr(puzzle_start, CELLS);
             bool valid_puzzle = true;
             // CELLS is now global, so this is fine
             for (size_t i = 0; i < CELLS; ++i) {
                 char c = potential_puzzle[i];
                 if (!((c >= '0' && c <= '9') || c == '.')) {
                     valid_puzzle = false;
                     break;
                 }
             }

             if (valid_puzzle) {
                 std::replace(potential_puzzle.begin(), potential_puzzle.end(), '.', '0'); // Standardize empty cells
                 puzzles.push_back(std::move(potential_puzzle));
                 // CELLS is now global, so this is fine
                 current_pos = puzzle_start + CELLS;
             } else {
                 // Skip the invalid chunk - advance past the first newline or to end
                 std::cerr << "Warning: Skipping potentially invalid chunk near position " << puzzle_start << std::endl;
                 size_t next_line = file_buffer.find('\n', puzzle_start);
                 current_pos = (next_line == std::string::npos) ? file_buffer.size() : next_line + 1;
             }
        } else {
             // Not enough characters left, check if it's just trailing whitespace
             bool only_whitespace = true;
             for(size_t i = puzzle_start; i < file_buffer.size(); ++i) {
                 if (!std::isspace(static_cast<unsigned char>(file_buffer[i]))) {
                     only_whitespace = false;
                     break;
                 }
             }
             if (!only_whitespace) {
                 std::cerr << "Warning: Trailing characters (" << file_buffer.size() - puzzle_start << ") ignored at end of file." << std::endl;
             }
             break; // Stop processing
        }
    }
    // --- End Input Reading ---

    if (puzzles.empty()) {
        std::cerr << "Error: No valid puzzles found in input.txt" << std::endl;
        return 1;
    }

    const size_t num_puzzles = puzzles.size();
    const unsigned hardware_threads = std::thread::hardware_concurrency();
    const unsigned num_threads = std::max(1u, std::min(hardware_threads == 0 ? 1u : hardware_threads, static_cast<unsigned>(num_puzzles)));
    std::cout << "Using " << num_threads << " threads to solve " << num_puzzles << " puzzles." << std::endl;

    ResultsManager results(num_puzzles);
    std::vector<std::thread> workers;
    workers.reserve(num_threads);
    size_t start_index = 0;
    const size_t chunk_size = (num_puzzles + num_threads - 1) / num_threads; // Ceiling division

    for (unsigned i = 0; i < num_threads; ++i) {
        size_t end_index = std::min(start_index + chunk_size, num_puzzles);
        if (start_index < end_index) {
            workers.emplace_back(solverWorker, std::ref(puzzles), std::ref(results), start_index, end_index);
        } else {
            break; // No more puzzles to assign
        }
        start_index = end_index;
    }

    // Wait for all threads to complete
    for (auto& t : workers) {
        if (t.joinable()) {
           t.join();
        }
    }

    // --- Faster Output Writing ---
    FILE* outfile = fopen("output.txt", "wb"); // Binary mode avoids newline translation issues
    if (!outfile) {
        std::cerr << "Error: Cannot open output.txt for writing" << std::endl;
        return 1;
    }
    // Optional: Buffer output for potentially fewer large writes
    std::vector<char> output_buffer;
    // CELLS is now global, so this is fine
    output_buffer.reserve(num_puzzles * (CELLS + 1)); // puzzle + newline

    for (const auto& solved_puzzle_arr : results.getResults()) {
         // CELLS is now global, so this is fine
        output_buffer.insert(output_buffer.end(), solved_puzzle_arr.begin(), solved_puzzle_arr.begin() + CELLS); // Copy 81 chars
        output_buffer.push_back('\n'); // Add newline
    }

    if (!output_buffer.empty()) {
        size_t written = fwrite(output_buffer.data(), 1, output_buffer.size(), outfile);
        if (written != output_buffer.size()) {
            std::cerr << "Warning: Error writing all data to output.txt" << std::endl;
        }
    }
    fclose(outfile);
    // --- End Output Writing ---

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    std::cout << "Finished solving " << num_puzzles << " puzzles in " << duration << " ms\n";

    return 0;
}