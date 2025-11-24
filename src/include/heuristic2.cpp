#include "common.hpp"
#include "caches.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include <set>
#include <map>
#include <numeric>

namespace HeurCached {
    void construction(const Problem &p, const Caches &c, Solution& temp) {
        random_device rd;
        mt19937 rng(rd());

        temp.mOrders.clear();
        temp.mAisles.clear();

        State state(p, c);
        
        vector<int> candidates(p.orders.size());
        iota(candidates.begin(), candidates.end(), 0);

        const double alpha = 0.5;

        while (!candidates.empty()) {
            vector<pair<double, int>> rcl;
            double minCost = 1e18, maxCost = -1e18;
            
            // 1. Evaluate Candidates
            // We use a simplified cost estimation to avoid full State cloning
            for (int orderIdx : candidates) {
                // Fast Bounds Check
                if (state.currentTotalUnits + c.orderTotalUnits[orderIdx] > p.ub) continue;
                
                // Heuristic Score Calculation
                // Benefit: Units gained
                // Cost: Estimated new aisles needed.
                
                int estimatedNewAisles = 0;
                // Check a sample of items in the order
                for(const auto& line : p.orders[orderIdx]) {
                    int item = line.ff;
                    int qty = line.ss;
                    // If we have a deficit or will cause one
                    if (state.itemBalance[item] < qty) {
                        // Very rough estimate: 1 aisle per missing item type?
                        // Or check if the best aisle for this item is already selected?
                        bool coveredBySelected = false;
                        // Fast check: is the Top 1 aisle for this item already in our solution?
                        if (!c.itemToAisles[item].empty()) {
                            int bestAisle = c.itemToAisles[item][0].second;
                            if (state.aisleSelected[bestAisle]) coveredBySelected = true;
                        }
                        if (!coveredBySelected) estimatedNewAisles++;
                    }
                }
                
                double score = (log(state.currentTotalUnits + c.orderTotalUnits[orderIdx])
                        - log(temp.mAisles.size() + estimatedNewAisles));

                rcl.push_back({score, orderIdx});
                if(score < minCost) minCost = score;
                if(score > maxCost) maxCost = score;
            }

            if (rcl.empty()) break;

            // 2. Filter RCL
            double threshold = maxCost - alpha * (maxCost - minCost);
            vector<int> finalCandidates;
            for(auto& pair : rcl) {
                if(pair.first >= threshold) finalCandidates.push_back(pair.second);
            }

            if (finalCandidates.empty()) finalCandidates.push_back(rcl[0].second); // Safety

            // 3. Pick & Update
            int i = uniform_int_distribution<>(0, finalCandidates.size() - 1)(rng);
            int pick = finalCandidates[i];
            temp.mOrders.push_back(pick);
            state.addOrder(pick);
            
            // Repair the solution (add aisles to cover new deficits)
            if (state.repairSolution(temp.mAisles) == -1) {
                // If repair fails (impossible), rollback
                state.removeOrder(pick);
                temp.mOrders.pop_back();
            }

            // Remove from candidates list
            candidates.erase(std::remove(candidates.begin(), candidates.end(), pick), candidates.end());
        }
        
        // Final cleanup
        state.pruneAisles(temp.mAisles);
    }

    void refinement(const Problem &p, const Caches &c, Solution& temp) {
        random_device rd;
        mt19937 rng(rd());

        // Rebuild state from input solution
        State state(p, c);
        for (int o : temp.mOrders) state.addOrder(o);
        // We clear aisles and rebuild them optimally for the current orders
        // This is often better than trusting the input aisles
        temp.mAisles.clear(); 
        state.repairSolution(temp.mAisles);
        state.pruneAisles(temp.mAisles);

        bool improved = true;
        while (improved) {
            improved = false;
            double currentScore = temp.calculateScore(p);

            // Get unselected orders
            vector<int> unselected;
            vector<bool> isSelected(p.orders.size(), false);
            for(int o : temp.mOrders) isSelected[o] = true;
            for(int i=0; i<p.orders.size(); ++i) if(!isSelected[i]) unselected.push_back(i);

            // --- MOVE: ADD ---
            // Try to add an order if it fits UB and improves score
            // We specifically look for "Free Fills" first (no new aisles needed)
            for (int u : unselected) {
                if (state.canFitOrder(u)) {
                    // Try adding
                    state.addOrder(u);
                    
                    // Since canFitOrder checks balance, we might not strictly need repair,
                    // but logic might be complex. Let's do a quick check.
                    if (!state.deficitItems.empty()) {
                        // Should not happen if canFitOrder is strict, but if it is loose:
                        state.repairSolution(temp.mAisles);
                    }

                    double newScore = (double)state.currentTotalUnits / (double)temp.mAisles.size();
                    
                    if (newScore > currentScore + 1e-9) {
                        temp.mOrders.push_back(u);
                        improved = true;
                        break; 
                    } else {
                        // Revert
                        state.removeOrder(u);
                        // If we added aisles in repair, we'd need to revert them too.
                        // For simplicity in this heuristic step, we only take "perfect fits"
                    }
                }
            }
            if (improved) continue;

            // --- MOVE: DROP & REPAIR ---
            // Try removing an order to see if we can drop massive amounts of aisles
            for (size_t i = 0; i < temp.mOrders.size(); i++) {
                int orderIdx = temp.mOrders[i];
                
                // Snapshot state logic is hard with mutable structs without copying.
                // We will just do the operation and reverse it if it fails.
                
                // 1. Remove Order
                state.removeOrder(orderIdx);
                
                // 2. Prune Aisles (This is the heavy part)
                // We need to see if removing this order allows removing aisles.
                vector<int> oldAisles = temp.mAisles;
                state.pruneAisles(temp.mAisles);
                
                double newScore = (double)state.currentTotalUnits / (double)temp.mAisles.size();
                
                if (newScore > currentScore + 1e-9 && state.currentTotalUnits >= p.lb) {
                    temp.mOrders.erase(temp.mOrders.begin() + i);
                    improved = true;
                    break;
                } else {
                    // Revert: Add order back
                    state.addOrder(orderIdx);
                    // Restore aisles
                    // To be perfectly safe/fast, we just restore the old vector
                    // and re-add the missing ones to the state
                    for(int a : oldAisles) {
                        if(!state.aisleSelected[a]) state.addAisle(a);
                    }
                    temp.mAisles = oldAisles;
                }
            }
            if (improved) continue;

            // Add an aisle
            size_t aisleIdx = uniform_int_distribution<>(0, p.aisles.size() - 1)(rng);
            if(!state.aisleSelected[aisleIdx]) {
                int estimatedNewItems = state.estimateNewItemsForAisle(aisleIdx);
                double newScore = (double)(state.currentTotalUnits + estimatedNewItems) / (temp.mAisles.size() + 1);
                if(newScore > currentScore) {
                    state.addAisleWithOrders(aisleIdx, temp.mAisles, temp.mOrders);
                    improved = true;
                }
            }
            if (improved) continue;
        }
    }
}
