#include "common.hpp"
#include "caches.hpp"

namespace Heur4 {
    void construction(const Problem &p, const Caches &c, State& state) {
        // 1. Setup State and RNG
        static thread_local std::mt19937 rng(std::random_device{}());

        // Candidates pool management
        // We use a swapping technique to remove items in O(1) without 'erase()'
        vector<int> aisleCandidates(p.aisles.size());
        iota(aisleCandidates.begin(), aisleCandidates.end(), 0);

        const double alpha = 0.5;
        const int SAMPLE_SIZE = 80; // Constant sample size = Linear Complexity

        // 2. Main Loop: Runs at most N times
        while (!aisleCandidates.empty()) {
            if (state.isFeasible()) break;

            // --- A. Sampling (Tournament) ---
            // We pick 'SAMPLE_SIZE' random indices from the *valid* range [0, valid_count-1]

            vector<pair<double, int>> sampleRCL; // {Score, Index_In_Candidates_Array}
            double minScore = 1e18, maxScore = -1e18;

            int attempts = min<int>(aisleCandidates.size(), SAMPLE_SIZE);
            
            for(int k = 0; k < attempts; ++k) {
                // Pick random index from valid range
                int randPos = std::uniform_int_distribution<>(0, aisleCandidates.size() - 1)(rng);
                int aisleIndex = aisleCandidates[randPos];

                // --- B. Fast Evaluation ---

                int estimatedNewItems = state.estimateNewItemsForAisle(aisleIndex);
                double score = log(state.currentTotalUnits + estimatedNewItems);

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
            if (validSamplePositions.empty()) {
                // All sampled items were bad (hit UB). 
                // We must remove *one* of them to ensure progress (avoid infinite loop).
                // We pick the first sampled one to discard.
                if (!sampleRCL.empty()) chosenPos = sampleRCL[0].second;
                else chosenPos = std::uniform_int_distribution<>(0, aisleCandidates.size() - 1)(rng);
            } else {
                // Pick one from RCL
                int rIdx = std::uniform_int_distribution<>(0, (int)validSamplePositions.size() - 1)(rng);
                chosenPos = validSamplePositions[rIdx];
                int aisleIdx = aisleCandidates[chosenPos];
                state.addAisleWithOrdersGreedy(aisleIdx);
            }

            // --- D. Removal ---
            // Whether we successfully added it, failed to repair it, or it hit UB,
            // we are done with this candidate for this construction run.
            std::swap(aisleCandidates[chosenPos], aisleCandidates.back());
            aisleCandidates.pop_back();
        }
    }
}
