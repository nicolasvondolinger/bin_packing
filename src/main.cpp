#include "include/common.hpp"

void construction(Solution& temp){
    
    const double alpha = 0.3;

    vector<int> candidates(o);
    iota(candidates.begin(), candidates.end(), 0);

    vector<map<int, int>> remainingStock(a);
    for (int j = 0; j < a; j++) {
        for (const auto& item_quant : aisles[j]) { 
            remainingStock[j][item_quant.ff] += item_quant.ss;
        }
    }

    while(!candidates.empty()){

        vector<pair<int, int>> costList;
        vector<int> invalidCandidates;

        int bestCost = -INF, worstCost = INF;

        set<int> currentAisles;
        for(auto const & idx : temp.mAisles) {
            currentAisles.insert(idx);
        }

        for (int c = 0; c < candidates.size(); c++) {
            int orderIndex = candidates[c];
            const auto& order = orders[orderIndex];

            if (temp.mOrders.size() + 1 > ub){ 
                invalidCandidates.pb(c);
                continue;
            }

            // 3.2. Verificar Viabilidade (Disponibilidade)
            // Precisamos verificar se o estoque *total restante* (em todos os corredores)
            // é suficiente para este pedido.
            bool feasible = true;
            map<int, int> itemsNeeded; // Agrega itens do pedido
            for (const auto& iten : order) {
                itemsNeeded[iten.ff] += iten.ss;
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
                invalidCandidates.pb(i);
                continue;
            }
            
            // 3.3. Calcular Custo Guloso
            // 'Custo(p) <- AvaliarCustoGuloso(Solucao U {p})'
            // Custo = - (número de *novos* corredores necessários)
            // (Queremos maximizar o custo, ou seja, minimizar novos corredores)
            set<int> newAislesNeeded;
            for (const auto& item_quant : order) {
                int item = item_quant.ff;
                // Procura o item nos corredores
                for (int j = 0; j < a; j++) {
                    if (remainingStock[j].count(item) && remainingStock[j][item] > 0) {
                        // Se o corredor 'j' tem o item e não está na solução...
                        if (currentAisles.find(j) == currentAisles.end()) {
                            newAislesNeeded.insert(j); // É um *novo* corredor
                        }
                    }
                }
            }
            
            int cost = - (int)newAislesNeeded.size();
            costList.push_back({cost, orderIndex});
            bestCost = max(bestCost, cost);
            worstCost = min(worstCost, cost);
        }

        // 3.4. Remover candidatos inválidos
        // (Itera de trás para frente para não invalidar os índices)
        sort(invalidCandidates.rbegin(), invalidCandidates.rend());
        for (int pos : invalidCandidates) {
            candidates.erase(candidates.begin() + pos);
        }

        if (candidates.empty() || costList.empty()) {
            break; // Nenhum candidato viável restante
        }

        // 4. Construir RCL
        // 'LimiteRCL <- MelhorCusto - alpha * (MelhorCusto - PiorCusto)'
        double RCL = bestCost - alpha * (bestCost - worstCost);
        
        vector<int> rcl;
        for (const auto& cost_order : costList) {
            if (cost_order.ff >= RCL) {
                rcl.push_back(cost_order.ss); // Adiciona indice do pedido
            }
        }

        // Nenhum candidato passou no filtro (pode acontecer se houver 1 só)
        if (rcl.empty()) break;
        
        // 5. Selecionar Aleatoriamente e Adicionar à Solução
        int rclIndex = rand() % rcl.size();
        int chosenOrderIndex = rcl[rclIndex];
        
        temp.mOrders.push_back(chosenOrderIndex);
        
        // 6. Atualizar Estado (Estoque e Corredores Visitados)
        // Esta é a parte de "coleta" (picking).
        // Estratégia: Tentar pegar itens de corredores *já visitados* primeiro.
        const auto& chosenOrder = orders[chosenOrderIndex];
        for (const auto& item_quant : chosenOrder) {
            int item = item_quant.ff;
            int quantNeeded = item_quant.ss;
            
            // 6.1. Tenta pegar de corredores *atuais*
            for (int aisleIdx : temp.mAisles) {
                if (quantNeeded == 0) break;
                if (remainingStock[aisleIdx].count(item)) {
                    int take = min(quantNeeded, remainingStock[aisleIdx][item]);
                    remainingStock[aisleIdx][item] -= take;
                    quantNeeded -= take;
                }
            }
            
            // 6.2. Se ainda precisar, pega de *novos* corredores
            if (quantNeeded > 0) {
                for (int j = 0; j < a; j++) {
                    if (quantNeeded == 0) break;
                    // Se 'j' NÃO é um corredor atual
                    if (currentAisles.find(j) == currentAisles.end()) {
                        if (remainingStock[j].count(item) && remainingStock[j][item] > 0) {
                            int take = min(quantNeeded, remainingStock[j][item]);
                            remainingStock[j][item] -= take;
                            quantNeeded -= take;
                            
                            // Adiciona este *novo* corredor à solução
                            temp.mAisles.push_back(j);
                            currentAisles.insert(j); // Marca como visitado para este loop
                        }
                    }
                }
            }
        } 

        // 7. Remover PedidoEscolhido dos Candidatos
        for (int c = 0; c < candidates.size(); c++) {
            if (candidates[c] == chosenOrderIndex) {
                candidates.erase(candidates.begin() + c);
                break;
            }
        }
    }

}

int main(){ _

    srand(time(NULL));

    cin >> o >> i >> a;

    orders.resize(o);
    aisles.resize(a);

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
    auto end_time = start_time + chrono::seconds(1200);

    while (chrono::high_resolution_clock::now() < end_time) {
        
        Solution temp; construction(temp);

        

        if (temp.mOrders.size() >= lb) {
            if (temp.calcObj() > sol.calcObj()) sol = temp;    
        }
    }

    sol.print();
    
    exit(0);
}