#include "common.hpp"
#include "caches.hpp"

namespace HeurCached {
    void construction(const Problem &p, const Caches &c, State& state) {
        random_device rd;
        mt19937 rng(rd());

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
                        - log(state.aisleSolution.size() + estimatedNewAisles));

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
            state.addOrder(pick);
            
            // Repair the solution (add aisles to cover new deficits)
            if (state.addAislesToRepairSolution() == -1) {
                // If repair fails (impossible), rollback
                state.removeOrder(pick);
            }

            // Remove from candidates list
            candidates.erase(std::remove(candidates.begin(), candidates.end(), pick), candidates.end());
        }
        
        // Final cleanup
        state.pruneAislesToFitOrders();
    }

    void refinement(const Problem &p, const Caches &c, State& state) {
        random_device rd;
        mt19937 rng(rd());

        // We clear aisles and rebuild them optimally for the current orders
        // This is often better than trusting the input aisles
        state.addAislesToRepairSolution();
        state.pruneAislesToFitOrders();

        bool improved = true;
        while (improved) {
            improved = false;
            double currentScore = state.calculateScore();

            // Get unselected orders
            vector<int> unselected;
            for(int i = 0; i < p.orders.size(); i += 1)
                if(!state.orderSelected[i]) unselected.push_back(i);

            // --- MOVE: ADD ---
            // Try to add an order if it fits UB and improves score
            // We specifically look for "Free Fills" first (no new aisles needed)
            for (int u : unselected) {
                if (state.canFitOrder(u)) {
                    // Try adding
                    state.addOrder(u);
                    
                    double newScore = state.calculateScore();
                    
                    if (state.isFeasible() && newScore > currentScore + 1e-9) {
                        improved = true;
                        break;
                    } else {
                        state.removeOrder(u);
                    }
                }
            }
            if (improved) continue;

            // --- MOVE: DROP & REPAIR ---
            // Try removing an order to see if we can drop massive amounts of aisles
            auto scanning = state.orderSolution;
            for (int orderIdx: scanning) {
                // Snapshot state logic is hard with mutable structs without copying.
                // We will just do the operation and reverse it if it fails.
                
                // 1. Remove Order
                state.removeOrder(orderIdx);
                
                // 2. Prune Aisles (This is the heavy part)
                // We need to see if removing this order allows removing aisles.
                auto removedAisles = state.pruneAislesToFitOrders();

                double newScore = (double)state.currentTotalUnits / (double)state.aisleSelected.size();

                if (newScore > currentScore + 1e-9 && state.currentTotalUnits >= p.lb) {
                    improved = true;
                    break;
                } else {
                    // Revert: Add order back
                    state.addOrder(orderIdx);
                    // Restore aisles
                    for(auto a: removedAisles) state.addAisle(a);
                }
            }
            if (improved) continue;

            // Add an aisle
            if(!p.aisles.empty()) {
                std::vector<size_t> aisleCandidates(16);
                for(auto &aisle: aisleCandidates)
                    aisle = uniform_int_distribution<>(0, p.aisles.size() - 1)(rng);
    
                size_t bestAisle = aisleCandidates[0];
                int newItems = state.estimateNewItemsForAisle(bestAisle);
                for(size_t i = 1; i < aisleCandidates.size(); i += 1) {
                    int newItems2 = state.estimateNewItemsForAisle(bestAisle);
                    if(newItems2 > newItems) {
                        bestAisle = aisleCandidates[i];
                        newItems = newItems2;
                    }
                }

                double newScore = (double)(state.currentTotalUnits + newItems) / (state.aisleSelected.size() + 1);
                if(newScore > currentScore) {
                    state.addAisleWithOrdersGreedy(bestAisle);
                    improved = true;
                }
            }
            if (improved) continue;
        }
    }
}
