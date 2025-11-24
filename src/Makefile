CXX = g++
CXXFLAGS = -O3 -std=c++17 -g

BIN_DIR = bin
OBJ_DIR = obj

EXECUTABLE = $(BIN_DIR)/solver
SOURCE = main.cpp
OBJECT = $(OBJ_DIR)/main.o

FILES = include/*

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECT) ${BIN_DIR}
	$(CXX) $(OBJECT) -o $(EXECUTABLE)

$(OBJECT): $(SOURCE) $(FILES) ${OBJ_DIR}
	$(CXX) $(CXXFLAGS) -c $(SOURCE) -o $(OBJECT)

${BIN_DIR}:
	mkdir -p ${BIN_DIR}

${OBJ_DIR}:
	mkdir -p ${OBJ_DIR}

run: ${EXECUTABLE}
	./${EXECUTABLE}

clean:
	rm -f $(OBJECT) $(EXECUTABLE)
	rmdir ${OBJ_DIR} ${BIN_DIR}

.PHONY: all clean