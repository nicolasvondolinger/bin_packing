#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <numeric>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <ilcplex/ilocplex.h>

ILOSTLBEGIN

struct ChallengeSolution {
    std::set<int> orders;
    std::set<int> aisles;
};

class ChallengeSolver {
private:
    const long MAX_RUNTIME = 600000; // milliseconds; 10 minutes

    std::vector<std::map<int, int>> orders;
    std::vector<std::map<int, int>> aisles;
    int nOrders;
    int nItems;
    int nAisles;
    int waveSizeLB;
    int waveSizeUB;
    std::string outputFile;

    std::vector<int> sum_orders;
    std::vector<int> sum_aisles;

    // CPLEX Components
    IloEnv env;
    IloModel model;
    IloCplex cplex;
    IloIntVarArray x;
    IloIntVarArray y;
    IloObjective obj;
    IloRange constraint;

public:
    ChallengeSolver(
        std::vector<std::map<int, int>> orders, 
        std::vector<std::map<int, int>> aisles, 
        int nOrders, int nItems, int nAisles, 
        int waveSizeLB, int waveSizeUB, 
        std::string outputFile
    ) : orders(orders), aisles(aisles), nOrders(nOrders), nItems(nItems), 
        nAisles(nAisles), waveSizeLB(waveSizeLB), waveSizeUB(waveSizeUB), 
        outputFile(outputFile) 
    {   
        sum_orders.resize(nOrders);
        sum_aisles.resize(nAisles);

        for (int i = 0; i < nOrders; i++) {
            int sum = 0;
            for (auto const& [key, val] : orders[i]) sum += val;
            sum_orders[i] = sum;
        }
        for (int i = 0; i < nAisles; i++) {
            int sum = 0;
            for (auto const& [key, val] : aisles[i]) sum += val;
            sum_aisles[i] = sum;
        }
    }

    ~ChallengeSolver() {
        env.end();
    }

    void writeOutput(const ChallengeSolution& solution) {
        std::ofstream outFile(outputFile);
        if (!outFile.is_open()) {
            std::cerr << "Error writing output to " << outputFile << std::endl;
            return;
        }

        outFile << solution.orders.size() << std::endl;
        for (int order : solution.orders) {
            outFile << order << std::endl;
        }

        outFile << solution.aisles.size() << std::endl;
        for (int aisle : solution.aisles) {
            outFile << aisle << std::endl;
        }

        outFile.close();
        std::cout << "Output written to " << outputFile << std::endl;
    }

    ChallengeSolution get_solution() {
        ChallengeSolution sol;
        try {
            for (int i = 0; i < nOrders; i++) {
                if (cplex.getValue(x[i]) > 0.5) {
                    sol.orders.insert(i);
                }
            }
            for (int i = 0; i < nAisles; i++) {
                if (cplex.getValue(y[i]) > 0.5) {
                    sol.aisles.insert(i);
                }
            }
        } catch (IloException& e) {
            std::cerr << "Concert exception caught: " << e << std::endl;
        }
        return sol;
    }

    bool setTimeLimit(std::chrono::steady_clock::time_point startTime) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        double remaining = (MAX_RUNTIME - elapsed) / 1000.0; 
        
        if (remaining <= 0) return false;
        
        double limit = remaining - 15.0; 
        if (limit < 0) limit = 1.0; 

        cplex.setParam(IloCplex::Param::TimeLimit, limit);
        return true;
    }

    void create_brute_model() {
        try {
            model = IloModel(env);
            cplex = IloCplex(env);

            cplex.setWarning(env.getNullStream());

            x = IloIntVarArray(env, nOrders, 0, 1);
            y = IloIntVarArray(env, nAisles, 0, 1);

            for(int i=0; i<nOrders; i++) x[i].setName(("x_" + std::to_string(i)).c_str());
            for(int i=0; i<nAisles; i++) y[i].setName(("y_" + std::to_string(i)).c_str());

            IloExpr restriction(env);
            for (int i = 0; i < nOrders; i++) {
                restriction += sum_orders[i] * x[i];
            }

            model.add(restriction >= waveSizeLB);
            model.add(restriction <= waveSizeUB);

            for (int i = 0; i < nItems; i++) {
                IloExpr itemOrdRestriction(env);
                for (int j = 0; j < nOrders; j++) {
                    if (orders[j].count(i)) {
                        itemOrdRestriction += x[j] * orders[j].at(i);
                    }
                }

                IloExpr itemAisleRestriction(env);
                for (int j = 0; j < nAisles; j++) {
                    if (aisles[j].count(i)) {
                        itemAisleRestriction += y[j] * aisles[j].at(i);
                    }
                }

                model.add(itemOrdRestriction <= itemAisleRestriction);
                
                itemOrdRestriction.end();
                itemAisleRestriction.end();
            }
            restriction.end();

        } catch (IloException& e) {
            std::cerr << "Concert exception caught: " << e << std::endl;
        }
    }

    void brute(std::chrono::steady_clock::time_point startTime) {
        try {
            create_brute_model();

            // First Step: Minimize Aisles
            IloExpr sumY(env);
            for(int i=0; i<nAisles; i++) sumY += y[i];
            
            obj = IloMinimize(env, sumY);
            model.add(obj);
            
            int minimum_aisles = nAisles;
            ChallengeSolution solution;

            setTimeLimit(startTime);

            cplex.extract(model);

            if (cplex.solve()) {
                minimum_aisles = (int)std::round(cplex.getObjValue());
                solution = get_solution();
                writeOutput(solution);
            }

            model.remove(obj);
            obj.end(); 

            // Second Step: Maximize Orders for fixed K aisles
            IloExpr restriction(env);
            for (int i = 0; i < nOrders; i++) {
                restriction += sum_orders[i] * x[i];
            }
            
            obj = IloMaximize(env, restriction);
            model.add(obj);

            double best = 0;
            
            constraint = IloRange(env, 0, sumY, 0); 
            bool constraintAdded = false;

            for (int K = minimum_aisles; K <= nAisles; K++) {
                if (!setTimeLimit(startTime)) break;

                if (constraintAdded) {
                    model.remove(constraint); // Remover do model
                    constraint.end();
                }
                
                constraint = (sumY == K);
                model.add(constraint); // Adicionar ao model
                constraintAdded = true;

                if (!cplex.solve()) continue;

                double currentObjVal = cplex.getObjValue();
                double score = (K > 0) ? currentObjVal / K : 0;
                
                if (score > best) {
                    best = score;
                    solution = get_solution();
                    writeOutput(solution);
                }
            }

            restriction.end();
            sumY.end();

        } catch (IloException& e) {
            std::cerr << "Concert exception caught: " << e << std::endl;
        }
    }

    void solve() {
        auto startTime = std::chrono::steady_clock::now();
        brute(startTime);
    }
};

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: ./main <inputFilePath> <outputFilePath>" << std::endl;
        return 1;
    }

    std::string inputFilePath = argv[1];
    std::string outputFilePath = argv[2];

    std::ifstream inFile(inputFilePath);
    if (!inFile.is_open()) {
        std::cerr << "Error reading input from " << inputFilePath << std::endl;
        return 1;
    }

    int nOrders, nItems, nAisles;
    inFile >> nOrders >> nItems >> nAisles;

    auto readItemQuantityPairs = [&](int nLines, std::vector<std::map<int, int>>& target) {
        target.resize(nLines);
        for (int i = 0; i < nLines; i++) {
            int nOrderItems;
            inFile >> nOrderItems;
            for (int k = 0; k < nOrderItems; k++) {
                int itemIndex, itemQuantity;
                inFile >> itemIndex >> itemQuantity;
                target[i][itemIndex] = itemQuantity;
            }
        }
    };

    std::vector<std::map<int, int>> orders;
    std::vector<std::map<int, int>> aisles;

    readItemQuantityPairs(nOrders, orders);
    readItemQuantityPairs(nAisles, aisles);

    int waveSizeLB, waveSizeUB;
    inFile >> waveSizeLB >> waveSizeUB;
    inFile.close();

    auto stopWatchStart = std::chrono::steady_clock::now();

    ChallengeSolver solver(orders, aisles, nOrders, nItems, nAisles, waveSizeLB, waveSizeUB, outputFilePath);
    solver.solve();

    auto stopWatchEnd = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = stopWatchEnd - stopWatchStart;
    std::cout << "Total execution time: " << elapsed_seconds.count() << "s\n";

    return 0;
}