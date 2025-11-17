#include "include/common.hpp"

int getTotalUnits(const Solution& s) {
    int totalUnits = 0;
    for (int orderIdx : s.mOrders) {
        for (const auto& item_quant : orders[orderIdx]) {
            totalUnits += item_quant.ss;
        }
    }
    return totalUnits;
}

double calculateScore(const Solution& s) {
    int totalUnits = getTotalUnits(s);
    
    if (s.mAisles.empty()) return 0.0;
    
    return (double)totalUnits / s.mAisles.size();
}

void construction(Solution& temp){
    
    const double alpha = 0.3;

    vector<int> candidates(o);
    iota(candidates.begin(), candidates.end(), 0);

    vector<map<int, int>> remainingStock(a);
    for (int j = 0; j < a; j++) {
        for (const auto& item : aisles[j]) { 
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
            const auto& order = orders[orderIndex];

            // Checa se esse pedido + atuais está abaixo de UB
            // Ao mesmo tempo, prepara estrutura que vai chegar a disponibilidade de estoque

            int unitsInCandidate = 0;
            bool feasible = true;
            map<int, int> itemsNeeded;

            for (const auto& item : order) {
                unitsInCandidate += item.ss;
                itemsNeeded[item.ff] += item.ss;
            }

            if (currentTotalUnits + unitsInCandidate > ub){ 
                invalidCandidates.pb(c);
                continue;
            }
            
            for (const auto& needed : itemsNeeded) {
                int item = needed.ff, quant = needed.ss;
                int total_available = 0;
                for (int j = 0; j < a; j++) {
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
                for (int j = 0; j < a; j++) {
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
        int rclIndex = rand() % rcl.size();
        int chosenOrderIndex = rcl[rclIndex];
        temp.mOrders.push_back(chosenOrderIndex);
        
        // Atualiza Estado (Estoque e Corredores)
        const auto& chosenOrder = orders[chosenOrderIndex];
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
                for (int j = 0; j < a; j++) {
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

bool recomputeSolution(Solution& s) {
    s.mAisles.clear();
    
    // Solução vazia é viável por definição (se lb <= 0)
    if (s.mOrders.empty()) return (lb <= 0);

    // MUDANÇA: Verifica 'lb' e 'ub' com base em UNIDADES
    int totalUnits = getTotalUnits(s);
    if (totalUnits < lb || totalUnits > ub) {
        return false;
    }

    // 2. Cria um estoque 'local' para esta simulação
    vector<map<int, int>> localStock(a);
    for (int j = 0; j < a; j++) {
        for (const auto& item_quant : aisles[j]) {
            localStock[j][item_quant.ff] += item_quant.ss;
        }
    }
    
    // 3. Agrega *todos* os itens necessários
    map<int, int> totalNeeded;
    for(int orderIdx : s.mOrders) {
        for(const auto& item_quant : orders[orderIdx]) {
            totalNeeded[item_quant.ff] += item_quant.ss;
        }
    }

    set<int> visitedAisles;
    
    // 4. Tenta 'pegar' os itens
    for(auto const& [item, quant_needed] : totalNeeded) {
        int quantNeeded = quant_needed;
        for(int j = 0; j < a; j++) {
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

void refine(Solution& temp){
    
    bool improved = true;
    
    while (improved) {
        improved = false;
        double currentObj = calculateScore(temp);
        
        set<int> ordersIn(temp.mOrders.begin(), temp.mOrders.end());
        vector<int> ordersOut;
        for(int j = 0; j < o; j++) {
            if(ordersIn.find(j) == ordersIn.end()) {
                ordersOut.push_back(j);
            }
        }

        // --- Movimento 1: "Add" (Adicionar 1 pedido) ---
        // 'recomputeSolution' vai checar o 'ub' de unidades
        for (int orderToAddIdx : ordersOut) {
            Solution neighbor = temp;
            neighbor.mOrders.push_back(orderToAddIdx);
            
            if(recomputeSolution(neighbor)) {
                // MUDANÇA: Usa a nova função de pontuação
                double neighborObj = calculateScore(neighbor);
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
            
            if(recomputeSolution(neighbor)) {
                double neighborObj = calculateScore(neighbor);
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

                if (recomputeSolution(neighbor)) {
                    double neighborObj = calculateScore(neighbor);
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

int main(){ _

    srand(time(NULL));
    cin >> o >> i >> a;

    orders.resize(o); aisles.resize(a);

    for(int j = 0; j < o; j++){
        int k; cin >> k;
        for(int l = 0; l < k; l++){
            int iten, quant; cin >> iten >> quant;
            orders[j].pb({iten, quant});
        }
    }

    for(int j = 0; j < a; j++){
        int l; cin >> l;
        for(int t = 0; t < l; t++){
            int iten, quant; cin >> iten >> quant;
            aisles[j].pb({iten, quant});
        }
    }

    cin >> lb >> ub;

    Solution sol; 

    auto start_time = chrono::high_resolution_clock::now();
    auto end_time = start_time + chrono::seconds(10);

    double bestScore = 0.0;

    while (chrono::high_resolution_clock::now() < end_time) {
        
        Solution temp; 
        construction(temp); refine(temp);
        
        double tempScore = calculateScore(temp);
        if (tempScore > bestScore) {
            bestScore = tempScore;
            sol = temp;    
        }
    }

    sol.print();
    
    exit(0);
}