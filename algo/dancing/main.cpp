#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <array>
#include <vector>
#include <numeric> // For std::iota

// --- DLX Data Structures ---

const int MAX_NODES = 1 + 324 + 729 * 4; // Root + Headers + Max '1's
const int MAX_COLS = 324;
const int MAX_ROWS = 729;

struct Node {
    Node *L, *R, *U, *D;
    Node* C; // Column Header
    int rowID; // Identifier for the original row (e.g., (r, c, d) encoded)
    int size;  // Used only by Column Headers (number of 1s in column)
    int colID; // Used only by Column Headers
};

class DLXSolver {
    Node nodes[MAX_NODES];
    Node* root; // Head of column headers list
    Node* columns[MAX_COLS];
    int nodeCount;
    std::vector<int> solution; // Stores rowIDs of the solution
    char resultGrid[81];       // To store the final grid

    // --- DLX Helper Functions ---

    Node* newNode() {
        // Simple pool allocation
        if (nodeCount >= MAX_NODES) {
             throw std::runtime_error("Exceeded maximum DLX nodes.");
        }
        return &nodes[nodeCount++];
    }

    // Map (r, c, d) to a unique row ID (0-728)
    int encodeRowID(int r, int c, int d) {
        return r * 81 + c * 9 + (d - 1);
    }

    // Decode row ID back to (r, c, d)
    void decodeRowID(int rowID, int& r, int& c, int& d) {
        rowID %= (81 * 9); // Remove potential pre-filled offset if added
        d = (rowID % 9) + 1;
        rowID /= 9;
        c = rowID % 9;
        r = rowID / 9;
    }

    // --- Core DLX Algorithm X Operations ---

    void cover(Node* col) {
        col->R->L = col->L;
        col->L->R = col->R;
        for (Node* rowNode = col->D; rowNode != col; rowNode = rowNode->D) {
            for (Node* rightNode = rowNode->R; rightNode != rowNode; rightNode = rightNode->R) {
                rightNode->D->U = rightNode->U;
                rightNode->U->D = rightNode->D;
                rightNode->C->size--;
            }
        }
    }

    void uncover(Node* col) {
        for (Node* rowNode = col->U; rowNode != col; rowNode = rowNode->U) {
            for (Node* leftNode = rowNode->L; leftNode != rowNode; leftNode = leftNode->L) {
                leftNode->C->size++;
                leftNode->D->U = leftNode;
                leftNode->U->D = leftNode;
            }
        }
        col->R->L = col;
        col->L->R = col;
    }

    // --- Recursive Search ---
    bool search(int k) {
        if (root->R == root) { // All columns covered - solution found
            return true;
        }

        // Choose column c with minimum size (heuristic)
        Node* col = root->R;
        for (Node* ptr = col->R; ptr != root; ptr = ptr->R) {
            if (ptr->size < col->size) {
                col = ptr;
            }
        }
         if (col->size == 0) return false; // Cannot cover -> backtrack

        cover(col);

        for (Node* rowNode = col->D; rowNode != col; rowNode = rowNode->D) {
            solution.push_back(rowNode->rowID); // Add choice to solution

            for (Node* rightNode = rowNode->R; rightNode != rowNode; rightNode = rightNode->R) {
                cover(rightNode->C);
            }

            if (search(k + 1)) {
                return true; // Solution found
            }

            // Backtrack
            for (Node* leftNode = rowNode->L; leftNode != rowNode; leftNode = leftNode->L) {
                uncover(leftNode->C);
            }
            solution.pop_back(); // Remove choice from solution
        }

        uncover(col);
        return false; // No solution found from this path
    }


    // --- Building the DLX Matrix ---
    void buildMatrix() {
        nodeCount = 0;
        root = newNode();
        root->L = root->R = root;
        root->U = root->D = root; // Not strictly necessary but consistent
        root->C = nullptr;
        root->rowID = -1;

        // Create Column Headers
        for (int j = 0; j < MAX_COLS; ++j) {
            columns[j] = newNode();
            columns[j]->size = 0;
            columns[j]->colID = j;
            columns[j]->U = columns[j]->D = columns[j];
            // Link headers horizontally
            columns[j]->L = root->L;
            columns[j]->R = root;
            root->L->R = columns[j];
            root->L = columns[j];
            columns[j]->C = columns[j]; // Column header points to itself
        }

        // Create Nodes for each possibility (r, c, d)
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                for (int d = 1; d <= 9; ++d) {
                    int b = (r / 3) * 3 + (c / 3); // Box index (0-8)
                    int rowIdentifier = encodeRowID(r, c, d);

                    // Indices of the 4 constraints satisfied by (r, c, d)
                    int cellConstraint = r * 9 + c;              // 0-80
                    int rowConstraint = 81 + r * 9 + (d - 1);    // 81-161
                    int colConstraint = 162 + c * 9 + (d - 1);   // 162-242
                    int boxConstraint = 243 + b * 9 + (d - 1);   // 243-323

                    int colIndices[] = {cellConstraint, rowConstraint, colConstraint, boxConstraint};
                    Node* rowNodes[4];

                    for (int i = 0; i < 4; ++i) {
                        int colIdx = colIndices[i];
                        Node* colHeader = columns[colIdx];
                        Node* newNodePtr = newNode();
                        newNodePtr->rowID = rowIdentifier;
                        newNodePtr->C = colHeader;

                        // Link vertically
                        newNodePtr->U = colHeader->U;
                        newNodePtr->D = colHeader;
                        colHeader->U->D = newNodePtr;
                        colHeader->U = newNodePtr;
                        colHeader->size++;

                        rowNodes[i] = newNodePtr;
                    }

                    // Link horizontally
                    for (int i = 0; i < 4; ++i) {
                        rowNodes[i]->L = rowNodes[(i + 3) % 4]; // (i-1) mod 4
                        rowNodes[i]->R = rowNodes[(i + 1) % 4];
                    }
                }
            }
        }
    }

     // --- Handling Initial Clues ---
    void handleInitialValue(int r, int c, int d) {
        int b = (r / 3) * 3 + (c / 3);
        int cellConstraint = r * 9 + c;
        int rowConstraint = 81 + r * 9 + (d - 1);
        int colConstraint = 162 + c * 9 + (d - 1);
        int boxConstraint = 243 + b * 9 + (d - 1);
        int targetRowID = encodeRowID(r, c, d);

        int colIndices[] = {cellConstraint, rowConstraint, colConstraint, boxConstraint};

        // Cover the columns corresponding to the constraints satisfied by the clue
        // This implicitly selects the row corresponding to the clue
        for (int colIdx : colIndices) {
             Node* col = columns[colIdx];
              // Check if already covered (can happen if multiple clues conflict indirectly)
             // A robust implementation might check for this, but for valid Sudoku,
             // covering twice shouldn't happen via this initial setup.
             // If it *were* covered, it means the puzzle is invalid from the start.

             cover(col); // Cover the main column first

             // Find the specific row node for (r, c, d) in this column
             // and cover the *other* columns this node belongs to
             for (Node* rowNode = col->D; rowNode != col; rowNode = rowNode->D) {
                 if (rowNode->rowID == targetRowID) {
                    // Found the exact node for the clue in this column
                     solution.push_back(rowNode->rowID); // Add clue to solution stack
                     for(Node* rightNode = rowNode->R; rightNode != rowNode; rightNode = rightNode->R) {
                         // Only cover if the column header isn't the one we started with
                         if (rightNode->C != col) {
                            cover(rightNode->C);
                         }
                     }
                     goto next_column; // Optimization: move to the next column index
                 }
             }
             // If we reach here, the target row wasn't found in the column,
             // which implies an inconsistency or error. For simplicity, assume valid inputs.

             next_column:;
        }
    }


public:
    void initialize(const std::string& puzzle) {
        buildMatrix(); // Build the base DLX structure for an empty grid
        solution.clear();
        std::fill(resultGrid, resultGrid + 81, '0'); // Initialize result grid

        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                char p_char = puzzle[r * 9 + c];
                if (p_char >= '1' && p_char <= '9') {
                    int d = p_char - '0';
                    handleInitialValue(r, c, d);
                     // Optionally place the clue in resultGrid immediately
                    resultGrid[r * 9 + c] = p_char;
                }
            }
        }
    }

    bool solve() {
        if (!search(0)) {
            return false; // No solution found
        }

        // Populate the result grid from the solution vector
        for (int rowID : solution) {
            int r, c, d;
            decodeRowID(rowID, r, c, d);
            // Check if it was already filled by a clue (handled in initialize)
            if (resultGrid[r*9+c] == '0') {
                 resultGrid[r * 9 + c] = d + '0';
            }
        }
        return true;
    }

    const char* getSolution() const {
        return resultGrid;
    }
};

// --- Keep the rest of the main structure (ResultsManager, solverWorker, main) ---
// --- (Minor change: SudokuSolver -> DLXSolver) ---

class ResultsManager {
    std::vector<std::array<char, 81>> results;
public:
    ResultsManager(size_t size) : results(size) {}
    void setResult(size_t index, const char* solution) {
        // Ensure solution is not null before copying
        if (solution) {
            std::memcpy(results[index].data(), solution, 81);
        } else {
             // Handle error or fill with a default value if solver failed
             std::fill(results[index].begin(), results[index].end(), 'X'); // Example error fill
        }
    }
    const std::vector<std::array<char, 81>>& getResults() const { return results; }
};

void solverWorker(const std::vector<std::string>& puzzles, ResultsManager& manager, size_t start, size_t end) {
    // Use thread_local if DLXSolver state needs to be truly thread-isolated
    // For this structure where a new solver is created per call range,
    // thread_local might not be strictly necessary but is safe.
    thread_local DLXSolver solver;
    for (size_t i = start; i < end; ++i) {
        try {
            solver.initialize(puzzles[i]);
            if (solver.solve()) {
                manager.setResult(i, solver.getSolution());
            } else {
                 // Handle case where solver returns false (no solution)
                 std::cerr << "Warning: Puzzle " << i << " has no solution according to DLX.\n";
                 manager.setResult(i, nullptr); // Indicate failure
            }
        } catch (const std::runtime_error& e) {
             std::cerr << "Error processing puzzle " << i << ": " << e.what() << std::endl;
             manager.setResult(i, nullptr); // Indicate failure
        }
    }
}

int main() {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::ifstream input("input_hard.txt"); // Make sure this file exists
    if (!input) {
         std::cerr << "Error: Cannot open input file 'input_hard.txt'" << std::endl;
         return 1;
    }

    std::vector<std::string> puzzles;
    std::string line;
    while (std::getline(input, line)) {
        line.erase(std::remove_if(line.begin(), line.end(),
            [](unsigned char c) { return std::isspace(c); }), line.end());
        // Basic validation - ensure it looks like a puzzle string
        if (line.size() == 81 && std::all_of(line.begin(), line.end(), [](char c){ return (c >= '0' && c <= '9') || c == '.'; })) {
             // Replace '.' with '0' if your solver expects '0' for empty
             std::replace(line.begin(), line.end(), '.', '0');
             puzzles.push_back(line);
        } else if (!line.empty()) {
            std::cerr << "Warning: Skipping invalid line (length != 81 or invalid chars): " << line << std::endl;
        }
    }
     input.close(); // Close the file

    if (puzzles.empty()) {
        std::cerr << "Error: No valid puzzles found in input file." << std::endl;
        return 1;
    }


    const unsigned threads = std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), puzzles.size()));
    std::cout << "Using " << threads << " threads.\n";
    ResultsManager results(puzzles.size());
    std::vector<std::thread> workers;
    size_t start = 0;
    // Ensure chunk size is at least 1, prevent division by zero if threads > puzzles.size()
    const size_t chunk = std::max(1ul, puzzles.size() / threads);
    const size_t remainder = puzzles.size() % threads;


    for (unsigned i = 0; i < threads; ++i) {
        size_t current_chunk_size = chunk + (i < remainder ? 1 : 0);
        size_t end = start + current_chunk_size;
        if (start < puzzles.size()) { // Avoid creating threads for empty ranges
             workers.emplace_back(solverWorker, std::ref(puzzles), std::ref(results), start, end);
        }
        start = end;
    }


    for (auto& t : workers) t.join();

    std::ofstream output("output_dlx.txt"); // Write to a different file
     if (!output) {
         std::cerr << "Error: Cannot open output file 'output_dlx.txt'" << std::endl;
         return 1;
     }
    for (const auto& s : results.getResults()) {
        output.write(s.data(), s.size());
        output << '\n';
    }
    output.close(); // Close the output file


    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime).count();
    std::cout << "Solved " << puzzles.size() << " puzzles using DLX in " << duration << " ms\n";

    return 0; // Indicate successful execution
}