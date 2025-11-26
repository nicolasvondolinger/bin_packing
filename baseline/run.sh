#!/bin/bash

# --- Configuration ---
# Directory where your compiled executable is located
EXECUTABLE_PATH="./main"

# Root directory containing the input datasets (e.g., ../datasets/a/...)
INPUT_ROOT_DIR="../datasets"

# Root directory for output
OUTPUT_ROOT_DIR="out"

# DEFININDO APENAS A PASTA 'b'
DATASET_SUBDIRS=("b")
# ---------------------

# Check if the executable exists
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "Error: Executable not found at $EXECUTABLE_PATH"
    echo "Please ensure you have compiled your C++ code and the executable is named 'main' in the current directory."
    exit 1
fi

echo "Starting challenge solver execution (Specific Range: 11-15)..."
echo "---"

# Loop through each dataset subdirectory (neste caso, apenas 'b')
for SUBDIR in "${DATASET_SUBDIRS[@]}"; do
    INPUT_DIR="$INPUT_ROOT_DIR/$SUBDIR"
    OUTPUT_DIR="$OUTPUT_ROOT_DIR/$SUBDIR"

    # Create the output subdirectory if it doesn't exist
    mkdir -p "$OUTPUT_DIR"
    
    echo "Processing directory: $SUBDIR (Input: $INPUT_DIR, Output: $OUTPUT_DIR)"

    # --- ALTERAÇÃO AQUI: Loop apenas de 10 a 15 ---
    for i in {11..15}; do
        
        # Formata o número para 4 dígitos (ex: 10 vira 0010)
        # Se os seus arquivos usam 3 dígitos, mude para "%03d"
        FMT_NUM=$(printf "%04d" "$i")
        
        # Constrói o nome do arquivo
        FILENAME="instance_${FMT_NUM}.txt"
        INPUT_FILE="$INPUT_DIR/$FILENAME"
        
        # Verifica se o arquivo existe antes de rodar
        if [ -f "$INPUT_FILE" ]; then
            # Construct the full output path
            OUTPUT_FILE="$OUTPUT_DIR/$FILENAME"
            
            # Run the program
            echo "  -> Running $FILENAME"
            "$EXECUTABLE_PATH" "$INPUT_FILE" "$OUTPUT_FILE"
        else
            echo "  -> Aviso: $FILENAME não encontrado em $INPUT_DIR"
        fi
    done
    
    echo "---"
done

echo "Selected executions finished."