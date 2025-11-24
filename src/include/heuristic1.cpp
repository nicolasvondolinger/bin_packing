#include "./common.hpp"
#include <random>

namespace Heur1 {
    void construction(const Problem &p, Solution& temp) {
        random_device rd;
        mt19937 rng(rd());

        const double alpha = 0.3;
    
        vector<int> candidates(p.orders.size());
        iota(candidates.begin(), candidates.end(), 0);
    
        vector<map<int, int>> remainingStock(p.aisles.size());
        for (size_t j = 0; j < p.aisles.size(); j += 1) {
            const auto& aisle = p.aisles[j];
            for (const auto& item : aisle) { 
                remainingStock[j][item.ff] += item.ss;
            }
        }
    
        int currentTotalUnits = 0;
    
        while(!candidates.empty()){
            vector<pair<double, int>> costList;
            vector<int> invalidCandidates;
    
            double bestCost = -1e18, worstCost = 1e18;
    
            set<int> currentAisles;
            for(auto const & idx : temp.mAisles) currentAisles.insert(idx);
    
            for (size_t c = 0; c < candidates.size(); c++) {
                int orderIndex = candidates[c];
                const auto& order = p.orders[orderIndex];
    
                // Checa se esse pedido + atuais está abaixo de UB
                // Ao mesmo tempo, prepara estrutura que vai chegar a disponibilidade de estoque
    
                int unitsInCandidate = 0;
                bool feasible = true;
                map<int, int> itemsNeeded;
    
                for (const auto& item : order) {
                    unitsInCandidate += item.ss;
                    itemsNeeded[item.ff] += item.ss;
                }
    
                if (currentTotalUnits + unitsInCandidate > p.ub){ 
                    invalidCandidates.pb(c);
                    continue;
                }
                
                for (const auto& needed : itemsNeeded) {
                    int item = needed.ff, quant = needed.ss;
                    int total_available = 0;
                    for (int j = 0; j < p.aisles.size(); j++) {
                        if (remainingStock[j].count(item)) {
                            total_available += remainingStock[j][item];
                        }
                    }
                    if (total_available < quant) {
                        feasible = false;
                        break;
                    }
                }
    
                if (!feasible) {
                    invalidCandidates.pb(c);
                    continue;
                }
                
                // Calcula Custo Guloso 
                // Custo = Unidades / (1 + Novos Corredores)
                set<int> newAislesNeeded;
                for (const auto& item_quant : order) {
                    int item = item_quant.ff;
                    for (int j = 0; j < p.aisles.size(); j++) {
                        if (remainingStock[j].count(item) && remainingStock[j][item] > 0) {
                            if (currentAisles.find(j) == currentAisles.end()) {
                                newAislesNeeded.insert(j);
                            }
                        }
                    }
                }
                
                double cost = (double)unitsInCandidate / (1.0 + newAislesNeeded.size());
                costList.push_back({cost, orderIndex});
                bestCost = max(bestCost, cost);
                worstCost = min(worstCost, cost);
            }
    
            // Remove candidatos inválidos
            sort(invalidCandidates.rbegin(), invalidCandidates.rend());
            for (int pos : invalidCandidates) {
                if (pos < (int)candidates.size()) { 
                    candidates.erase(candidates.begin() + pos);
                }
            }
    
            if (candidates.empty() || costList.empty()) break; 
    
            // Construir RCL
            double limiteRCL = bestCost - alpha * (bestCost - worstCost);
            
            vector<int> rcl;
            for (const auto& cost_order : costList) {
                // cost_order.ff é 'double' (custo)
                if (cost_order.ff >= limiteRCL) {
                    rcl.push_back(cost_order.ss); // Adiciona orderIndex
                }
            }
     
            if (rcl.empty()) {
                // Fallback: se o alfa filtrou tudo, pega o melhor
                if (!costList.empty()) {
                    // Encontra o melhor da costList
                    double max_cost = -1e18;
                    int best_order = -1;
                    for(auto& p : costList) {
                        if(p.ff > max_cost) {
                            max_cost = p.ff;
                            best_order = p.ss;
                        }
                    }
                    if(best_order != -1) rcl.push_back(best_order);
                }
            }
    
            if (rcl.empty()) break; // Nenhum candidato viável
            
            // Seleciona Aleatoriamente e Adiciona à Solução
            int rclIndex = std::uniform_int_distribution<>(0, rcl.size() - 1)(rng);
            int chosenOrderIndex = rcl[rclIndex];
            temp.mOrders.push_back(chosenOrderIndex);
            
            // Atualiza Estado (Estoque e Corredores)
            const auto& chosenOrder = p.orders[chosenOrderIndex];
            int unitsAdded = 0; // Rastreia unidades adicionadas
            for (const auto& item_quant : chosenOrder) {
                int item = item_quant.ff;
                int quantNeeded = item_quant.ss;
                unitsAdded += quantNeeded;
                
                // Tenta pegar de corredores atuais
                for (int aisleIdx : temp.mAisles) {
                    if (quantNeeded == 0) break;
                    if (remainingStock[aisleIdx].count(item)) {
                        int take = min(quantNeeded, remainingStock[aisleIdx][item]);
                        remainingStock[aisleIdx][item] -= take;
                        quantNeeded -= take;
                    }
                }
                
                // Se ainda precisar, pega de novos corredores
                if (quantNeeded > 0) {
                    for (int j = 0; j < p.aisles.size(); j++) {
                        if (quantNeeded == 0) break;
                        if (currentAisles.find(j) == currentAisles.end()) {
                            if (remainingStock[j].count(item) && remainingStock[j][item] > 0) {
                                int take = min(quantNeeded, remainingStock[j][item]);
                                remainingStock[j][item] -= take;
                                quantNeeded -= take;
                                temp.mAisles.push_back(j);
                                currentAisles.insert(j);
                            }
                        }
                    }
                }
            } 
    
            currentTotalUnits += unitsAdded; // Atualiza total de unidades
    
            // Remover PedidoEscolhido dos Candidatos
            for (size_t c = 0; c < candidates.size(); c++) {
                if (candidates[c] == chosenOrderIndex) {
                    candidates.erase(candidates.begin() + c);
                    break;
                }
            }
        }
    }
    
    bool recomputeSolution(const Problem &p, Solution& s) {
        s.mAisles.clear();
        
        // Solução vazia é viável por definição (se lb <= 0)
        if (s.mOrders.empty()) return (p.lb <= 0);
    
        // MUDANÇA: Verifica 'lb' e 'ub' com base em UNIDADES
        int totalUnits = s.getTotalUnits(p);
        if (totalUnits < p.lb || totalUnits > p.ub) {
            return false;
        }
    
        // 2. Cria um estoque 'local' para esta simulação
        vector<map<int, int>> localStock(p.aisles.size());
        for (int j = 0; j < p.aisles.size(); j++) {
            for (const auto& item_quant : p.aisles[j]) {
                localStock[j][item_quant.ff] += item_quant.ss;
            }
        }
        
        // 3. Agrega *todos* os itens necessários
        map<int, int> totalNeeded;
        for(int orderIdx : s.mOrders) {
            for(const auto& item_quant : p.orders[orderIdx]) {
                totalNeeded[item_quant.ff] += item_quant.ss;
            }
        }
    
        set<int> visitedAisles;
        
        // 4. Tenta 'pegar' os itens
        for(auto const& [item, quant_needed] : totalNeeded) {
            int quantNeeded = quant_needed;
            for(int j = 0; j < p.aisles.size(); j++) {
                if (quantNeeded == 0) break;
                if (localStock[j].count(item) && localStock[j][item] > 0) {
                    int take = min(quantNeeded, localStock[j][item]);
                    localStock[j][item] -= take;
                    quantNeeded -= take;
                    visitedAisles.insert(j);
                }
            }
            
            // 5. Se, após checar todos os corredores, ainda faltar...
            if (quantNeeded > 0) {
                return false; // ...a solução é inviável (falta de estoque)
            }
        }
        
        // 6. Se chegou aqui, é viável. Popula a lista de corredores.
        s.mAisles.assign(visitedAisles.begin(), visitedAisles.end());
        return true;
    }
    
    void refinement(const Problem &p, Solution& temp) {
        bool improved = true;
        
        while (improved) {
            improved = false;
            double currentObj = temp.calculateScore(p);
            
            set<int> ordersIn(temp.mOrders.begin(), temp.mOrders.end());
            vector<int> ordersOut;
            for(int j = 0; j < p.orders.size(); j++) {
                if(ordersIn.find(j) == ordersIn.end()) {
                    ordersOut.push_back(j);
                }
            }
    
            // --- Movimento 1: "Add" (Adicionar 1 pedido) ---
            // 'recomputeSolution' vai checar o 'ub' de unidades
            for (int orderToAddIdx : ordersOut) {
                Solution neighbor = temp;
                neighbor.mOrders.push_back(orderToAddIdx);
                
                if(recomputeSolution(p, neighbor)) {
                    // MUDANÇA: Usa a nova função de pontuação
                    double neighborObj = neighbor.calculateScore(p);
                    if(neighborObj > (currentObj + 1e-9)) {
                        temp = neighbor;
                        improved = true;
                        break; 
                    }
                }
            }
            if(improved) continue; 
    
            // --- Movimento 2: "Remove" (Remover 1 pedido) ---
            // MUDANÇA: Removemos o 'if (temp.mOrders.size() > lb)'
            // 'recomputeSolution' vai checar o 'lb' de unidades
            for (size_t i = 0; i < temp.mOrders.size(); i++) {
                Solution neighbor;
                neighbor.mOrders = temp.mOrders;
                neighbor.mOrders.erase(neighbor.mOrders.begin() + i);
                
                if(recomputeSolution(p, neighbor)) {
                    double neighborObj = neighbor.calculateScore(p);
                    if(neighborObj > (currentObj + 1e-9)) {
                        temp = neighbor;
                        improved = true;
                        break;
                    }
                }
            }
            if(improved) continue;
    
            // --- Movimento 3: "Swap" (Trocar 1-1) ---
            vector<int> currentOrders = temp.mOrders; 
            for (size_t i = 0; i < currentOrders.size(); i++) {
                int orderToRemoveIdx = currentOrders[i];
                
                for (int orderToAddIdx : ordersOut) {
                    if(orderToRemoveIdx == orderToAddIdx) continue;
                    
                    Solution neighbor;
                    neighbor.mOrders = currentOrders;
                    neighbor.mOrders[i] = orderToAddIdx; 
    
                    if (recomputeSolution(p, neighbor)) {
                        double neighborObj = neighbor.calculateScore(p);
                        if (neighborObj > (currentObj + 1e-9)) { 
                            temp = neighbor;
                            improved = true; 
                            break;
                        }
                    }
                }
                if(improved) break; 
            }
        } 
    }
}
