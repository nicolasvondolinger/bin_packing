CPLEX_ROOT = /opt/ibm/ILOG/CPLEX_Studio2212

SYSTEM = x86-64_linux
LIBFORMAT = static_pic

# Compilador
CXX = g++
CXXFLAGS = -O3 -fPIC -fno-strict-aliasing -fexceptions -DNDEBUG -DIL_STD -std=c++17

# Diretórios de Include
INCLUDES = -I$(CPLEX_ROOT)/cplex/include -I$(CPLEX_ROOT)/concert/include

# Diretórios de Libs e Flags de Linker
LIBDIR = -L$(CPLEX_ROOT)/cplex/lib/$(SYSTEM)/$(LIBFORMAT) -L$(CPLEX_ROOT)/concert/lib/$(SYSTEM)/$(LIBFORMAT)
LIBS = -lilocplex -lcplex -lconcert -lm -lpthread -ldl

# Nome do executável
TARGET = main
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(SRC) $(LIBDIR) $(LIBS)

clean:
	rm -f $(TARGET)