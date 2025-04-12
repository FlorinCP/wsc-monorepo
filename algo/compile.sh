#!/bin/bash

# Compile C++ to WebAssembly with simpler options
#emcc main_wasm.cpp -o sudoku.js \
#  -s WASM=1 \
#  -s EXPORTED_FUNCTIONS="['_solveSudoku']" \
#  -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
#  -O3
#
#echo "Compilation complete. Run 'python -m http.server' and open http://localhost:8000"



## Compile C++ to WebAssembly with batch processing functions
#emcc main_wasm_batch.cpp -o sudoku_batch.js \
#  -s WASM=1 \
#  -s EXPORTED_FUNCTIONS="['_solveSudoku', '_createInputBuffer', '_createOutputBuffer', '_processBatch', '_getTotalSolved', '_resetSolver', '_getSolution']" \
#  -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap', 'getValue', 'setValue']" \
#  -s ALLOW_MEMORY_GROWTH=1 \
#  -O3
#
#echo "Compilation complete. Run 'python -m http.server' and open http://localhost:8000"


#!/bin/bash

# Compile C++ to WebAssembly with batch processing support
#emcc main_wasm_batch.cpp -o sudoku_batch.js \
#  -s WASM=1 \
#  -s EXPORTED_FUNCTIONS="['_solveSudoku', '_solveBatch', '_createBuffer', '_freeBuffer']" \
#  -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
#  -s ALLOW_MEMORY_GROWTH=1 \
#  -s MAXIMUM_MEMORY=256MB \
#  -O3
#
#echo "Compilation complete. Run 'python -m http.server' and open http://localhost:8000"

#!/bin/bash

# Compile C++ to WebAssembly with batch processing and OpenMP support
#emcc main_wasm_batch.cpp -o sudoku_batch.js \
#  -s WASM=1 \
#  -s EXPORTED_FUNCTIONS="['_solveSudoku', '_solveBatch', '_allocateInputBuffer', '_allocateOutputBuffer', '_allocateSolvedFlags', '_freeAllBuffers', '_setPuzzle', '_getSolution', '_wasSolved']" \
#  -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
#  -s ALLOW_MEMORY_GROWTH=1 \
#  -s MAXIMUM_MEMORY=256MB \
#  -O3
#
#echo "Compilation complete. Run 'python -m http.server' and open http://localhost:8000"


#
## Compile C++ to WebAssembly with pthreads support
#emcc main_wasm.cpp -o sudoku_batch.js \
#  -s WASM=1 \
#  -s EXPORTED_FUNCTIONS="['_solveSudoku', '_solveBatch', '_allocateInputBuffer', '_allocateOutputBuffer', '_allocateSolvedFlags', '_freeAllBuffers', '_setPuzzle', '_getSolution', '_wasSolved', '_getNumCores', '_isThreadingSupported', '_getCompletedThreadCount']" \
#  -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
#  -s ALLOW_MEMORY_GROWTH=1 \
#  -s MAXIMUM_MEMORY=256MB \
#  -s USE_PTHREADS=1 \
#  -s PTHREAD_POOL_SIZE=8 \
#  -s INITIAL_MEMORY=134217728 \
#  -O3 \
#  -pthread
#
#echo "Compilation complete. Run 'python -m http.server' and open http://localhost:8000"

emcc main_wasm_pt.cpp -o sudoku_pt.js \
  -O3 \
  -s WASM=1 \
  -s USE_PTHREADS=1 \
  -s PTHREAD_POOL_SIZE=4 \
  -s ALLOW_MEMORY_GROWTH=0 \
  -s INITIAL_MEMORY=128MB \
  -s EXPORTED_FUNCTIONS="['_solveSudoku', '_solveBatch', '_allocateInputBuffer', '_allocateOutputBuffer', '_allocateSolvedFlags', '_freeAllBuffers', '_setPuzzle', '_getSolution', '_wasSolved', '_getCompletedThreadCount', '_requestStop', '_resetStopFlag']" \
  -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap']"


echo "Compilation complete. Run 'python -m http.server' and open http://localhost:8000"


# best
#emcc main_wasm_pt.cpp -o sudoku_pt.js \
#  -O3 \
#  -s WASM=1 \
#  -s USE_PTHREADS=1 \
#  -s PTHREAD_POOL_SIZE=8 \
#  -s ALLOW_MEMORY_GROWTH=0 \
#  -s INITIAL_MEMORY=128MB \
#  -s ASSERTIONS=0 \
#  -s DISABLE_EXCEPTION_CATCHING=1 \
#  -s EXIT_RUNTIME=0 \
#  -s NO_FILESYSTEM=1 \
#  -s EXPORTED_FUNCTIONS="['_solveSudoku', '_solveBatch', '_allocateInputBuffer', '_allocateOutputBuffer', '_allocateSolvedFlags', '_freeAllBuffers', '_setPuzzle', '_getSolution', '_wasSolved', '_getCompletedThreadCount', '_requestStop', '_resetStopFlag']" \
#  -s EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap']" \
#  --closure 1