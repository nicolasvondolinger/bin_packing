import os
import subprocess
import sys
import platform
import re
import matplotlib.pyplot as plt
import numpy as np  # <--- Adicionado

# --- Configurações ---

# Define o diretório raiz do projeto (um nível acima de 'src/')
# __file__ é o caminho deste script (ex: /home/projeto/src/run_challenge.py)
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

# Define o diretório 'src'
SRC_DIR = os.path.abspath(os.path.dirname(__file__))

# Caminho para o executável que o 'make' cria
EXECUTABLE_PATH = os.path.join(SRC_DIR, "bin", "solver")

# ---
# --- CLASSE DO CHECKER INTEGRADA ---
# ---
class WaveOrderPicking:
    def __init__(self):
        self.orders = None
        self.aisles = None
        self.wave_size_lb = None
        self.wave_size_ub = None

    def read_input(self, input_file_path):
        with open(input_file_path, 'r') as file:
            lines = file.readlines()
            first_line = lines[0].strip().split()
            o, i, a = int(first_line[0]), int(first_line[1]), int(first_line[2])

            # Read orders
            self.orders = []
            line_index = 1
            for j in range(o):
                order_line = lines[line_index].strip().split()
                line_index += 1
                d = int(order_line[0])
                order_map = {int(order_line[2 * k + 1]): int(order_line[2 * k + 2]) for k in range(d)}
                self.orders.append(order_map)

            # Read aisles
            self.aisles = []
            for j in range(a):
                aisle_line = lines[line_index].strip().split()
                line_index += 1
                d = int(aisle_line[0])
                aisle_map = {int(aisle_line[2 * k + 1]): int(aisle_line[2 * k + 2]) for k in range(d)}
                self.aisles.append(aisle_map)

            # Read wave size bounds
            bounds = lines[line_index].strip().split()
            self.wave_size_lb = int(bounds[0])
            self.wave_size_ub = int(bounds[1])

    def read_output(self, output_file_path):
        with open(output_file_path, 'r') as file:
            lines = file.readlines()
            # Remove linhas vazias e espaços
            lines = [line.strip() for line in lines if line.strip()]
            
            if not lines: # Arquivo vazio
                return [], []

            num_aisles = int(lines[0])
            visited_aisles = [int(lines[i + 1]) for i in range(num_aisles)]
            
            num_orders = int(lines[num_aisles + 1])
            selected_orders = [int(lines[num_aisles + 2 + i]) for i in range(num_orders)]

        # Seu C++ usa o formato: A-list, O-list
        # O checker.py original usava: O-list, A-list
        # Ajustei o read_output para o formato do seu C++
        
        selected_orders = list(set(selected_orders))
        visited_aisles = list(set(visited_aisles))
        return selected_orders, visited_aisles

    def is_solution_feasible(self, selected_orders, visited_aisles):
        total_units_picked = 0
        for order_idx in selected_orders:
            if order_idx < 0 or order_idx >= len(self.orders):
                print(f"  Aviso: Índice de pedido inválido {order_idx}")
                return False # Índice fora do range
            total_units_picked += np.sum(list(self.orders[order_idx].values()))

        # Check if total units picked are within bounds
        if not (self.wave_size_lb <= total_units_picked <= self.wave_size_ub):
            # Soluções vazias (0 unidades) são válidas se lb <= 0
            if total_units_picked == 0 and self.wave_size_lb <= 0:
                pass # É viável
            else:
                print(f"  Aviso: Falha no limite de unidades (LB={self.wave_size_lb}, UB={self.wave_size_ub}, Total={total_units_picked})")
                return False

        # Compute all items that are required by the selected orders
        required_items = {}
        for order_idx in selected_orders:
            for item, quant in self.orders[order_idx].items():
                required_items[item] = required_items.get(item, 0) + quant

        # Check if all required items are available in the visited aisles
        for item, total_required in required_items.items():
            total_available = 0
            for aisle_idx in visited_aisles:
                 if aisle_idx < 0 or aisle_idx >= len(self.aisles):
                    print(f"  Aviso: Índice de corredor inválido {aisle_idx}")
                    return False # Índice fora do range
                 total_available += self.aisles[aisle_idx].get(item, 0)

            if total_required > total_available:
                print(f"  Aviso: Falha de estoque (Item {item}: Precisa {total_required}, Disponível {total_available})")
                return False

        return True

    def compute_objective_function(self, selected_orders, visited_aisles):
        total_units_picked = 0

        # Calculate total units picked
        for order_idx in selected_orders:
            total_units_picked += np.sum(list(self.orders[order_idx].values()))

        # Calculate the number of visited aisles
        num_visited_aisles = len(visited_aisles)

        if num_visited_aisles == 0:
            return 0.0 # Evita divisão por zero

        # Objective function: total units picked / number of visited aisles
        return total_units_picked / num_visited_aisles

# ---
# --- FIM DA CLASSE DO CHECKER ---
# ---

def compile_code():
    """Roda 'make' dentro da pasta 'src'."""
    print("Iniciando compilação com 'make' dentro de src/...")
    
    result = subprocess.run(
        ["make"],
        capture_output=True,
        text=True,
        cwd=SRC_DIR
    )

    if result.returncode != 0:
        print("--- ERRO NA COMPILAÇÃO ---")
        print(result.stderr)
        print(result.stdout)
        print("----------------------------")
        return False

    print("Compilação bem-sucedida.")
    return True


def run_set(set_name):
    """Roda todas as instâncias de um set (a, b, ou x)."""
    
    input_folder = os.path.join(ROOT_DIR, "datasets", set_name)
    output_folder = os.path.join(ROOT_DIR, "results", set_name)
    best_solution_folder = os.path.join(ROOT_DIR, "best_solutions", set_name) # Corrigido 'best_solution' para 'best_solutions'

    os.makedirs(output_folder, exist_ok=True)

    instance_names = []
    my_scores = []
    best_scores = []

    instances = sorted([f for f in os.listdir(input_folder) if f.endswith(".txt")])

    if not instances:
        print(f"Nenhuma instância .txt encontrada em {input_folder}")
        return [], [], []

    print(f"\n--- Rodando Set: {set_name} ({len(instances)} instâncias) ---")

    for filename in instances:
        instance_name = os.path.splitext(filename)[0]
        input_file = os.path.join(input_folder, filename)
        output_file = os.path.join(output_folder, f"{instance_name}.txt") # Corrigido nome de saída
        best_file = os.path.join(best_solution_folder, f"{instance_name}.txt") # Corrigido nome de saída

        print(f"  Rodando: {filename}")
        instance_names.append(instance_name)

        # --- LÓGICA DE VALIDAÇÃO ATUALIZADA ---
        wop = WaveOrderPicking()
        
        try:
            # 1. Carrega os dados da instância no checker
            wop.read_input(input_file)
        except Exception as e:
            print(f"  ERRO FATAL: Falha ao ler o ARQUIVO DE ENTRADA {filename}: {e}")
            my_scores.append(0.0)
            best_scores.append(0.0)
            continue # Pula para a próxima instância

        # 2. Roda o solver C++
        try:
            with open(input_file, 'r') as f_in, open(output_file, 'w') as f_out:
                cmd = [EXECUTABLE_PATH]
                
                result = subprocess.run(
                    cmd,
                    stdin=f_in,
                    stdout=f_out,
                    stderr=subprocess.PIPE,
                    text=True,
                    cwd=ROOT_DIR
                )
                
                # 3. Valida a saída do Solver C++
                if result.returncode != 0:
                    print(f"    ERRO NO EXECUTÁVEL! (Código: {result.returncode})")
                    print(result.stderr)
                    my_scores.append(0.0)
                else:
                    # Sucesso, agora valida a solução
                    try:
                        my_orders, my_aisles = wop.read_output(output_file)
                        if wop.is_solution_feasible(my_orders, my_aisles):
                            score = wop.compute_objective_function(my_orders, my_aisles)
                            print(f"    Sucesso. Score: {score:.4f} (Solução Válida)")
                            my_scores.append(score)
                        else:
                            print(f"    SOLUÇÃO INVÁLIDA (gerada pelo solver). Score: 0.0")
                            my_scores.append(0.0)
                    except Exception as e:
                        print(f"    Erro ao parsear a SAÍDA DO SOLVER: {e}")
                        my_scores.append(0.0)

        except Exception as e:
            print(f"  Erro inesperado ao rodar {filename}: {e}")
            my_scores.append(0.0)

        # 4. Valida o arquivo da Melhor Solução (Gabarito)
        if os.path.exists(best_file):
            try:
                best_orders, best_aisles = wop.read_output(best_file)
                if wop.is_solution_feasible(best_orders, best_aisles):
                    best_score = wop.compute_objective_function(best_orders, best_aisles)
                    best_scores.append(best_score)
                else:
                    print(f"  Aviso: Melhor solução {best_file} é INVÁLIDA.")
                    best_scores.append(0.0)
            except Exception as e:
                print(f"  Aviso: Erro ao parsear {best_file}: {e}")
                best_scores.append(0.0)
        else:
            print(f"  Aviso: Não foi encontrado o arquivo da melhor solução: {best_file}")
            best_scores.append(0.0)

    return instance_names, my_scores, best_scores


def plot_results(set_name, instance_names, my_scores, best_scores):
    """Gera um gráfico de barras comparativo para o set."""
    
    if not instance_names:
        print(f"Sem dados para plotar para o set {set_name}.")
        return

    output_folder = os.path.join(ROOT_DIR, "results", set_name)
    plot_file = os.path.join(output_folder, f"comparativo_{set_name}.png")

    x = np.arange(len(instance_names))
    width = 0.35 

    fig, ax = plt.subplots(figsize=(max(10, len(instance_names) * 0.5), 6))
    
    rects1 = ax.bar(x - width/2, my_scores, width, label='Sua Solução (Solver)', color='tab:blue')
    rects2 = ax.bar(x + width/2, best_scores, width, label='Melhor Solução (Gabarito)', color='tab:orange')

    ax.set_ylabel('Score (Unidades / Corredores)') # <-- Métrica atualizada
    ax.set_title(f'Comparativo de Scores - Set {set_name}')
    ax.set_xticks(x)
    ax.set_xticklabels(instance_names, rotation=45, ha="right")
    ax.legend()
    ax.grid(axis='y', linestyle='--', alpha=0.7)

    ax.bar_label(rects1, padding=3, fmt='%.2f', rotation=90, fontsize='small')
    ax.bar_label(rects2, padding=3, fmt='%.2f', rotation=90, fontsize='small')

    fig.tight_layout()
    plt.savefig(plot_file)
    plt.close(fig)
    
    print(f"Gráfico salvo em: {plot_file}")


# --- Execução Principal ---

if __name__ == "__main__":
    if not compile_code():
        print("Saindo due ao erro de compilação.")
        sys.exit(1)

    sets_to_run = ['a', 'b', 'x']
    
    all_results = {}

    for set_name in sets_to_run:
        names, my_scores, bests = run_set(set_name)
        all_results[set_name] = {
            "names": names,
            "my_scores": my_scores,
            "best_scores": bests
        }

    print("\n--- Gerando Gráficos ---")
    for set_name in sets_to_run:
        results = all_results[set_name]
        plot_results(set_name, results["names"], results["my_scores"], results["best_scores"])

    print("\nProcesso concluído!")