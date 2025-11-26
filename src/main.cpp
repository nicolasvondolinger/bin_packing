#include "include/common.hpp"
#include "include/caches.hpp"
#include "include/heuristic1.cpp"
#include "include/heuristic2.cpp"
#include "include/heuristic3.cpp"
#include "include/heuristic4.cpp"

#include <functional>
#include <mutex>
#include <fstream> // Adicionado para manipulação de arquivos
#include <iomanip> // Adicionado para precisão do tempo

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int chosenHeuristic = -1;
    // Lendo a heurística (argumento 1)
    if(1 < argc) chosenHeuristic = std::stoi(argv[1]);

    // Lendo o caminho do log (argumento 2 - opcional)
    std::string logPath = "";
    if(2 < argc) logPath = argv[2];

    // Se tiver caminho de log, limpa o arquivo antigo criando um novo vazio
    if (!logPath.empty()) {
        std::ofstream ofs(logPath, std::ios::out | std::ios::trunc);
        ofs.close();
    }

    std::cerr << "Reading problem" << std::endl;
    const Problem p = Problem::ReadFrom(cin);

    std::cerr << "Computing caches" << std::endl;
    const Caches c(p);

    std::function<void(Solution&)> heuristic;
    switch(chosenHeuristic) {
        case 0:
            heuristic = [&p](Solution &s) { Heur1::construction(p, s); Heur1::refinement(p, s); };
            break;
        default:
        case 1:
            heuristic = [&p, &c](Solution &s) { State state(p, c, s); HeurCached::construction(p, c, state); HeurCached::refinement(p, c, state); };
            break;
        case 2:
            heuristic = [&p, &c](Solution &s) { State state(p, c, s); Heur3::construction(p, c, state); HeurCached::refinement(p, c, state); };
            break;
        case 3:
            heuristic = [&p, &c](Solution &s) { State state(p, c, s); Heur4::construction(p, c, state); HeurCached::refinement(p, c, state); };
            break;
    }

    size_t threadCount = std::thread::hardware_concurrency();
    
    // Marca o tempo de início total
    auto startTime = chrono::high_resolution_clock::now(); 
    auto lastImprovement = startTime;
    auto patience = chrono::seconds(3);

    std::cerr << "Running threads" << std::endl;
    std::vector<std::thread> threads;
    
    mutex solutionMutex;
    Solution bestSolution;
    double bestScore = 0.0;

    threads.reserve(threadCount);
    for(size_t threadIndex = 0; threadIndex < threadCount; threadIndex += 1) {
        threads.emplace_back([&] {
            auto now = chrono::high_resolution_clock::now();
            while (now < lastImprovement + patience) {
                Solution solution; 
                heuristic(solution);
                double score = solution.calculateScore(p);
                bool feasible = solution.checkFeasibility(p);

                if(!feasible) {
                    continue;
                }
                
                solutionMutex.lock();
                now = chrono::high_resolution_clock::now();
                if (score > bestScore) {
                    std::cerr << "New best! " << score << ' ' << (feasible ? "Feasible" : "Unfeasible") << std::endl;
                    
                    bestScore = score;
                    bestSolution = solution;
                    lastImprovement = now;

                    // --- TRECHO ADICIONADO: LOG PARA GRÁFICO ---
                    if (!logPath.empty()) {
                        // Calcula tempo decorrido desde o início em segundos
                        chrono::duration<double> elapsed = now - startTime;
                        
                        // Abre em modo append (adicionar ao final)
                        std::ofstream logFile(logPath, std::ios::app);
                        if (logFile.is_open()) {
                            logFile << std::fixed << std::setprecision(6) << elapsed.count() << " " << score << "\n";
                            logFile.close();
                        }
                    }
                    // ------------------------------------------
                }
                solutionMutex.unlock();
            }
        });
    }
    for(auto &t: threads) t.join();

    std::cerr << "Final best " << bestSolution.calculateScore(p) << ' '
        << (bestSolution.checkFeasibility(p) ? "Feasible" : "Unfeasible") << ", "
        << bestSolution.mOrders.size() << " orders, "
        << bestSolution.mAisles.size() << " aisles, "
        << bestSolution.getTotalUnits(p) << " units" << std::endl;

    bestSolution.print();
    
    exit(0);
}