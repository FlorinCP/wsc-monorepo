#!/bin/bash

FOLDER="$1"
MAIN_CPP="$FOLDER/main.cpp"
OUTPUT="$FOLDER/sudoku_solver"

echo "Step 1: Instrumented build with gprof..."
g++-14 -pg -O3 -march=native -std=c++17 -o "$OUTPUT" "$MAIN_CPP" -lpthread || exit 1

echo "Step 2: Running to generate gmon.out..."
"$OUTPUT" || exit 1

echo "Step 3: Generating gprof report..."
gprof "$OUTPUT" gmon.out > "$FOLDER/gprof_report.txt"

echo "Done. Report saved at: $FOLDER/gprof_report.txt"
