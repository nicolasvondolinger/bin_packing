
CXX = g++
CXXFLAGS = -O3 -Wall -std=c++17 -g

BIN_DIR = bin
OBJ_DIR = obj

EXECUTABLE = $(BIN_DIR)/solver
SOURCE = main.cpp
OBJECT = $(OBJ_DIR)/main.o

HEADERS = include/common.hpp

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECT)
	$(CXX) $(OBJECT) -o $(EXECUTABLE)

$(OBJECT): $(SOURCE) $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $(SOURCE) -o $(OBJECT)

clean:
	rm -f $(OBJECT)
	rm -f $(EXECUTABLE)

.PHONY: all clean