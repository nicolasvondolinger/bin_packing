#include "include/common.hpp"
#include "include/caches.hpp"
#include "include/heuristic1.cpp"
#include "include/heuristic2.cpp"
#include "include/heuristic3.cpp"

#include <functional>
#include <mutex>

int main(int argc, char *argv[]) { _
    srand(time(NULL));

    int chosenHeuristic = -1;
    if(1 < argc) chosenHeuristic = std::stoi(argv[1]);

    std::cerr << "Reading problem" << std::endl;
    const Problem p = Problem::ReadFrom(cin);

    std::cerr << "Computing caches" << std::endl;
    const Caches c(p);

    std::function<void(Solution&)> heuristic;
    switch(chosenHeuristic) {
        case 0:
            heuristic = [&p](Solution &s)
            {
                Heur1::construction(p, s);
                Heur1::refinement(p, s);
            };
            break;
        default:
        case 1:
            heuristic = [&p, &c](Solution &s)
            {
                HeurCached::construction(p, c, s);
                HeurCached::refinement(p, c, s);
            };
            break;
        case 2:
            heuristic = [&p, &c](Solution &s)
            {
                Heur3::construction(p, c, s);
                HeurCached::refinement(p, c, s);
            };
            break;
    }

    size_t threadCount = std::thread::hardware_concurrency();
    
    auto lastImprovement = chrono::high_resolution_clock::now();
    auto patience = chrono::seconds(60);

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
                    std::cerr << "Produced unfeasible solution!" << std::endl;
                    continue;
                }
                
                solutionMutex.lock();
                now = chrono::high_resolution_clock::now();
                if (score > bestScore) {
                    std::cerr << "New best! " << score << ' '
                        << (feasible ? "Feasible" : "Unfeasible") << std::endl;
                    bestScore = score;
                    bestSolution = solution;
                    lastImprovement = now;
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
