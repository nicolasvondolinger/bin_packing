#pragma once

#include "common.hpp"
#include <random>

using namespace std;

/**
 * IMMUTABLE CACHE
 * Calculated once per Problem instance.
 * Provides O(1) access to static relationships and precomputed sums.
 */
struct Caches {
    // Inverse Index: item_id -> list of {quantity, aisle_index}
    // Optimization: Sorted by quantity descending.
    // Usage: When an order needs item X, quickly find the aisle with the most of X.
    vector<vector<pair<int, int>>> itemToAisles;

    // Inverse Index: item_id -> list of {quantity, order_index}
    // Usage: If we pick an aisle with item X, which orders does this help?
    vector<vector<pair<int, int>>> itemToOrders;

    // Precomputed total units per order (the numerator for the greedy score)
    vector<ll> orderTotalUnits;

    // The maximum possible quantity of an item available in the entire warehouse.
    // Usage: Fast fail if an order requests more than physically exists.
    vector<ll> globalItemAvailability;

    Caches(const Problem &p) {
        // 1. Resize everything based on itemCount (assuming item IDs are 0..itemCount-1)
        // If IDs are sparse/large, we would need a coordinate compression map, 
        // but typically these problems use dense IDs.
        // We'll use p.itemCount + safety buffer just in case.
        int size = p.itemCount + 1;
        
        itemToAisles.resize(size);
        itemToOrders.resize(size);
        globalItemAvailability.resize(size, 0);
        orderTotalUnits.resize(p.orders.size(), 0);

        // 2. Process Aisles (Build itemToAisles)
        for(int i = 0; i < p.aisles.size(); ++i) {
            for(const auto& line : p.aisles[i]) {
                int item = line.ff;
                int quant = line.ss;
                if(item < size) {
                    itemToAisles[item].push_back({quant, i});
                    globalItemAvailability[item] += quant;
                }
            }
        }

        // 3. Process Orders (Build itemToOrders and orderTotalUnits)
        for(int i = 0; i < p.orders.size(); ++i) {
            ll currentOrderUnits = 0;
            for(const auto& line : p.orders[i]) {
                int item = line.ff;
                int quant = line.ss;
                if(item < size) {
                    itemToOrders[item].push_back({quant, i});
                }
                currentOrderUnits += quant;
            }
            orderTotalUnits[i] = currentOrderUnits;
        }

        // 4. Sort itemToAisles by quantity DESC
        // This is critical for the greedy heuristic:
        // "I need item X, give me the aisle that has the MOST of it."
        for(auto& vec : itemToAisles) {
            sort(vec.rbegin(), vec.rend());
        }
    }
};

/**
 * MUTABLE CACHE (State)
 * Maintained during construction/refinement.
 * Allows O(1) delta updates instead of full re-scans.
 */
struct State {
    const Problem& p;
    const Caches& c;

    // Tracks: (Available Quantity - Required Quantity) for each item.
    // If balance[i] < 0, we have a deficit.
    vector<ll> itemBalance;

    // Tracks how unique items with a deficit (balance < 0).
    // If deficitItemCount == 0, the solution is FEASIBLE regarding items.
    std::unordered_set<size_t> deficitItems;

    // Current total units picked (to check lb/ub bounds fast)
    ll currentTotalUnits;

    // Fast lookups for what is currently selected
    vector<bool> aisleSelected;
    vector<bool> orderSelected;

    State(const Problem& prob, const Caches& caches) 
        : p(prob), c(caches), currentTotalUnits(0) {
        reset();
    }

    void reset() {
        currentTotalUnits = 0;
        deficitItems.clear();
        itemBalance.clear();
        aisleSelected.clear();
        orderSelected.clear();
        itemBalance.resize(p.itemCount + 1, 0);
        aisleSelected.resize(p.aisles.size(), false);
        orderSelected.resize(p.orders.size(), false);
    }

    // O(Number of items in the aisle)
    void addAisle(int aisleIdx) {
        if (aisleSelected[aisleIdx]) return;
        aisleSelected[aisleIdx] = true;

        for (const auto& line : p.aisles[aisleIdx]) {
            int item = line.ff;
            int qty = line.ss;
            
            // Before update: was it in deficit?
            bool wasDeficit = (itemBalance[item] < 0);
            
            itemBalance[item] += qty;
            
            // After update: is it solved?
            if (wasDeficit && itemBalance[item] >= 0) {
                deficitItems.erase(item);
            }
        }
    }

    // O(Number of items in the aisle)
    void removeAisle(int aisleIdx) {
        if (!aisleSelected[aisleIdx]) return;
        aisleSelected[aisleIdx] = false;

        for (const auto& line : p.aisles[aisleIdx]) {
            int item = line.ff;
            int qty = line.ss;

            bool wasOK = (itemBalance[item] >= 0);
            
            itemBalance[item] -= qty;
            
            if (wasOK && itemBalance[item] < 0) {
                deficitItems.insert(item);
            }
        }
    }

    // O(Number of items in the order)
    void addOrder(int orderIdx) {
        if (orderSelected[orderIdx]) return;
        orderSelected[orderIdx] = true;
        currentTotalUnits += c.orderTotalUnits[orderIdx];

        for (const auto& line : p.orders[orderIdx]) {
            int item = line.ff;
            int qty = line.ss;

            bool wasOK = (itemBalance[item] >= 0);
            
            itemBalance[item] -= qty; // Requirement reduces balance
            
            if (wasOK && itemBalance[item] < 0) {
                deficitItems.insert(item);
            }
        }
    }

    // O(Number of items in the order)
    void removeOrder(int orderIdx) {
        if (!orderSelected[orderIdx]) return;
        orderSelected[orderIdx] = false;
        currentTotalUnits -= c.orderTotalUnits[orderIdx];

        for (const auto& line : p.orders[orderIdx]) {
            int item = line.ff;
            int qty = line.ss;
            
            bool wasDeficit = (itemBalance[item] < 0);
            
            itemBalance[item] += qty; // Removing requirement increases balance
            
            if (wasDeficit && itemBalance[item] >= 0) {
                deficitItems.erase(item);
            }
        }
    }

    // Fast feasibility check
    bool isFeasible() const {
        return deficitItems.empty() && 
               currentTotalUnits >= p.lb && 
               currentTotalUnits <= p.ub;
    }

    // Fast check if a specific order CAN fit into current aisle selection
    // without adding new aisles (Free Fill Heuristic)
    bool canFitOrder(int orderIdx) const {
        // Quick fail: Global bounds
        if (currentTotalUnits + c.orderTotalUnits[orderIdx] > p.ub) return false;

        // Detailed Item Check
        for (const auto& line : p.orders[orderIdx]) {
            int item = line.ff;
            int qty = line.ss;
            // Balance represents (Available - Required). 
            // We need (Balance - NewReq) >= 0 => Balance >= NewReq
            if (itemBalance[item] < qty) return false; 
        }
        return true;
    }

    // Helper: Prune redundant aisles
    void pruneAisles(vector<int>& aisleSolution) {
        for(int i = 0; i < aisleSolution.size();) {
            int aisleIdx = aisleSolution[i], canRemove = 1;
            for (const auto& line : p.aisles[aisleIdx]) {
                int item = line.ff;
                int qty = line.ss;
                if(itemBalance[item] - qty < 0) {
                    canRemove = 0;
                    break;
                }
            }
            if (canRemove == 1) {
                removeAisle(aisleIdx);
                std::swap(aisleSolution[i], aisleSolution.back());
                aisleSolution.pop_back();
            } else {
                i += 1;
            }
        }
    }

    void pruneOrders(vector<int>& orderSolution) {
        auto deficit = deficitItems;
        for(auto i: deficit)
            while(itemBalance[i] < 0)
                for(auto order: c.itemToOrders[i])
                    if(orderSelected[order.ss])
                        removeOrder(order.ss);
    }

    // Helper: Greedy Aisle Selection to satisfy deficits in State
    // Returns the number of new aisles added
    int repairSolution(vector<int>& aisleSolution) {
        int addedCount = 0;

        while (!deficitItems.empty()) {
            int bestAisle = -1;
            ll bestCover = -1;

            // Optimization: Instead of scanning ALL aisles, we only look at aisles
            // that contain the items we currently need.
            // We collect candidate aisles from the inverse index of items in deficit.
            
            // To avoid iterating all items, we can maintain a set of "deficit items" in State, 
            // but for now, iterating items is O(I). If I is large, this loop is the bottleneck.
            // Heuristic optimization: Just look at the first few items in deficit.
            
            // Collect candidates
            // Map: aisle_index -> quantity_of_deficit_covered
            unordered_map<int, ll> aisleScores;

            for (auto i: deficitItems) {
                ll needed = -itemBalance[i]; // Positive magnitude of need
                
                // Look at the best aisles for this item from Cache
                // We only look at top X aisles to keep it fast
                int checks = 0;
                for(const auto& pair : c.itemToAisles[i]) {
                    int qty = pair.first;
                    int aisleIdx = pair.second;
                    
                    // Skip if already selected
                    if (aisleSelected[aisleIdx]) continue;

                    ll useful = min((ll)qty, needed);
                    aisleScores[aisleIdx] += useful;

                    checks++;
                    if (checks > 5) break; // Optimization: only check top 5 providers per item
                }
            }

            if (aisleScores.empty()) return -1; // Impossible to satisfy

            // Pick best
            for (auto const& [aisle, score] : aisleScores) {
                if (score > bestCover) {
                    bestCover = score;
                    bestAisle = aisle;
                }
            }

            if (bestAisle != -1) {
                addAisle(bestAisle);
                aisleSolution.push_back(bestAisle);
                addedCount++;
            } else {
                return -1; // Should not happen if global feasibility is checked
            }
        }
        return addedCount;
    }
};
