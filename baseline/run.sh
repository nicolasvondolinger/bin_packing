#!/bin/bash

# --- Configuration ---
# Directory where your compiled executable is located
EXECUTABLE_PATH="./main"

# Root directory containing the input datasets (e.g., ../datasets/a/...)
INPUT_ROOT_DIR="../datasets"

# Root directory for output (must contain subdirs 'a', 'b', 'x')
OUTPUT_ROOT_DIR="out"

# Dataset subdirectories to process
DATASET_SUBDIRS=("x")
# ---------------------

# Check if the executable exists
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "Error: Executable not found at $EXECUTABLE_PATH"
    echo "Please ensure you have compiled your C++ code and the executable is named 'main' in the current directory."
    exit 1
fi

echo "Starting challenge solver execution..."
echo "---"

# Loop through each dataset subdirectory (a, b, x)
for SUBDIR in "${DATASET_SUBDIRS[@]}"; do
    INPUT_DIR="$INPUT_ROOT_DIR/$SUBDIR"
    OUTPUT_DIR="$OUTPUT_ROOT_DIR/$SUBDIR"

    # Create the output subdirectory if it doesn't exist
    mkdir -p "$OUTPUT_DIR"
    
    echo "Processing directory: $SUBDIR (Input: $INPUT_DIR, Output: $OUTPUT_DIR)"

    # Loop through all .txt files in the current input directory
    # Note: Using * ensures only files are processed, not subdirectories if they existed.
    for INPUT_FILE in "$INPUT_DIR"/*.txt; do
        # Check if any files were found (handles the case of no .txt files in the dir)
        if [ -f "$INPUT_FILE" ]; then
            # Extract just the filename (e.g., instance_0003.txt)
            FILENAME=$(basename "$INPUT_FILE")
            
            # Construct the full output path
            OUTPUT_FILE="$OUTPUT_DIR/$FILENAME"
            
            # Run the program
            echo "  -> Running $FILENAME"
            # The '$EXECUTABLE_PATH' is run with the input and output paths as arguments
            "$EXECUTABLE_PATH" "$INPUT_FILE" "$OUTPUT_FILE"
        fi
    done
    
    echo "---"
done

echo "All executions finished."
