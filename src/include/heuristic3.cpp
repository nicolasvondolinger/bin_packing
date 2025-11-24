#include "common.hpp"
#include "caches.hpp"
#include <cstdlib>

namespace Heur3 {
    void construction(const Problem &p, const Caches &c, Solution& temp) {
        // 1. Setup State and RNG
        static thread_local std::mt19937 rng(std::random_device{}());

        temp.mOrders.clear();
        temp.mAisles.clear();

        State state(p, c);
        
        // Candidates pool management
        // We use a swapping technique to remove items in O(1) without 'erase()'
        vector<int> candidates(p.orders.size());
        iota(candidates.begin(), candidates.end(), 0);
        int valid_count = candidates.size();

        const double alpha = 0.5;     
        const int SAMPLE_SIZE = 80; // Constant sample size = Linear Complexity

        // 2. Main Loop: Runs at most N times
        while (valid_count > 0) {
            
            // --- A. Sampling (Tournament) ---
            // We pick 'SAMPLE_SIZE' random indices from the *valid* range [0, valid_count-1]
            
            vector<pair<double, int>> sampleRCL; // {Score, Index_In_Candidates_Array}
            double minScore = 1e18, maxScore = -1e18;
            
            int attempts = min(valid_count, SAMPLE_SIZE);
            
            for(int k = 0; k < attempts; ++k) {
                // Pick random index from valid range
                int randPos = std::uniform_int_distribution<>(0, valid_count - 1)(rng);
                int orderIdx = candidates[randPos];
                
                // --- B. Fast Evaluation ---
                
                // 1. Strict Bounds Check
                if (state.currentTotalUnits + c.orderTotalUnits[orderIdx] > p.ub) {
                    // Score -1.0 signifies "Invalid/Poison"
                    sampleRCL.push_back({-1.0, randPos});
                    continue;
                }

                // 2. Adaptive Score Calculation (Same as quadratic version)
                int estimatedNewAisles = 0;
                
                for(const auto& line : p.orders[orderIdx]) {
                    int item = line.ff;
                    int qty = line.ss;
                    
                    if (state.itemBalance[item] < qty) {
                        bool covered = false;
                        if (!c.itemToAisles[item].empty()) {
                            int t1a = c.itemToAisles[item][0].second;
                            if (state.aisleSelected[t1a]) covered = true;
                        }
                        if (!covered) estimatedNewAisles++;
                    }
                }

                double score = (log(state.currentTotalUnits + c.orderTotalUnits[orderIdx])
                        - log(temp.mAisles.size() + estimatedNewAisles));
                
                // Store {Score, Position_In_Array} so we can swap-pop later
                sampleRCL.push_back({score, randPos});
                
                if (score > maxScore) maxScore = score;
                if (score < minScore && score > -0.5) minScore = score;
            }

            // --- C. Selection (RCL on Sample) ---
            
            // Filter valid candidates from the sample
            vector<int> validSamplePositions;
            double threshold = maxScore - alpha * (maxScore - minScore);

            for(auto& s : sampleRCL) {
                if (s.first < -0.5) continue; // Skip bounds violations
                if (s.first >= threshold) {
                    validSamplePositions.push_back(s.second);
                }
            }

            int chosenPos = -1;
            bool tryToAdd = false;

            if (validSamplePositions.empty()) {
                // All sampled items were bad (hit UB). 
                // We must remove *one* of them to ensure progress (avoid infinite loop).
                // We pick the first sampled one to discard.
                if (!sampleRCL.empty()) chosenPos = sampleRCL[0].second;
                else chosenPos = std::uniform_int_distribution<>(0, valid_count - 1)(rng);
                tryToAdd = false; // Just discard, don't try to add
            } else {
                // Pick one from RCL
                int rIdx = std::uniform_int_distribution<>(0, (int)validSamplePositions.size() - 1)(rng);
                chosenPos = validSamplePositions[rIdx];
                tryToAdd = true;
            }
            
            // --- D. Commit & Repair (The Critical Safety Check) ---
            
            if (tryToAdd) {
                int orderIdx = candidates[chosenPos];
                
                // 1. Add Order to State
                state.addOrder(orderIdx);
                
                // 2. Exact Repair (The slow but safe check)
                // If this fails, we MUST rollback to stay feasible.
                if (state.repairSolution(temp.mAisles) != -1) {
                    // Success! Keep it.
                    temp.mOrders.push_back(orderIdx);
                } else {
                    // Failure! Rollback.
                    state.removeOrder(orderIdx);
                    // We don't add it to temp.mOrders
                    // We WILL remove it from candidates below so we don't retry it.
                }
            }

            // --- E. Swap-and-Pop (O(1) Removal) ---
            // Whether we successfully added it, failed to repair it, or it hit UB,
            // we are done with this candidate for this construction run.
            
            // Swap chosen element with the last valid element
            std::swap(candidates[chosenPos], candidates[valid_count - 1]);
            
            // Shrink the valid range
            valid_count--;
        }
        
        // Final cleanup
        state.pruneAisles(temp.mAisles);
    }
}
