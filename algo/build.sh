#!/bin/bash

# Check if folder argument is provided
if [ -z "$1" ]; then
  echo "Usage: $0 <folder_with_main_cpp>"
  exit 1
fi

FOLDER="$1"
MAIN_CPP="$FOLDER/main.cpp"
OUTPUT="$FOLDER/sudoku_solver"

# Check if main.cpp exists
if [ ! -f "$MAIN_CPP" ]; then
  echo "main.cpp not found in $FOLDER"
  exit 1
fi

echo "Step 1: Instrumented build..."
g++-14 -O3 -march=native -fprofile-generate=. -std=c++17 -o "$OUTPUT" "$MAIN_CPP" -lpthread || exit 1

echo "Step 2: Running to generate profile data..."
"$OUTPUT" || exit 1

echo "Step 3: Final optimized build using profile data..."
g++-14 -O3 -march=native -fprofile-use=. -fprofile-correction -std=c++17 -o "$OUTPUT" "$MAIN_CPP" -lpthread || exit 1

echo "Done! Final binary: $OUTPUT"
