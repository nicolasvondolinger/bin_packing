import os
import numpy as np
import sys

# --- CONFIGURAÇÃO DE DIRETÓRIOS ---
# Ajuste conforme a estrutura da sua pasta se necessário
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

# --- CLASSE E LÓGICA DE CÁLCULO (Reutilizada do seu código) ---
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

            self.orders = []
            line_index = 1
            for j in range(o):
                order_line = lines[line_index].strip().split()
                line_index += 1
                d = int(order_line[0])
                order_map = {int(order_line[2 * k + 1]): int(order_line[2 * k + 2]) for k in range(d)}
                self.orders.append(order_map)

            self.aisles = []
            for j in range(a):
                aisle_line = lines[line_index].strip().split()
                line_index += 1
                d = int(aisle_line[0])
                aisle_map = {int(aisle_line[2 * k + 1]): int(aisle_line[2 * k + 2]) for k in range(d)}
                self.aisles.append(aisle_map)

            bounds = lines[line_index].strip().split()
            self.wave_size_lb = int(bounds[0])
            self.wave_size_ub = int(bounds[1])

    def read_output(self, output_file_path):
        try:
            with open(output_file_path, 'r') as file:
                lines = file.readlines()
                lines = [line.strip() for line in lines if line.strip()]
                if not lines: return [], []

                num_orders = int(lines[0])
                current_line = 1
                selected_orders = []
                for _ in range(num_orders):
                    selected_orders.append(int(lines[current_line]))
                    current_line += 1
                
                if current_line < len(lines):
                    num_aisles = int(lines[current_line])
                    current_line += 1
                    visited_aisles = []
                    for _ in range(num_aisles):
                        visited_aisles.append(int(lines[current_line]))
                        current_line += 1
                else:
                    visited_aisles = []
                return list(set(selected_orders)), list(set(visited_aisles))
        except:
            return [], []

    def is_solution_feasible(self, selected_orders, visited_aisles):
        total_units_picked = 0
        for order in selected_orders:
            if order < 0 or order >= len(self.orders): return False
            total_units_picked += np.sum(list(self.orders[order].values()))

        if not (self.wave_size_lb <= total_units_picked <= self.wave_size_ub):
            # Tratamento para caso trivial de 0 unidades se permitido
            if total_units_picked == 0 and self.wave_size_lb <= 0: pass
            else: return False

        required_items = {}
        for order in selected_orders:
            for item, quant in self.orders[order].items():
                required_items[item] = required_items.get(item, 0) + quant

        for item, total_required in required_items.items():
            total_available = 0
            for aisle in visited_aisles:
                if aisle >= 0 and aisle < len(self.aisles):
                    total_available += self.aisles[aisle].get(item, 0)
            if total_required > total_available: return False
        return True

    def compute_objective_function(self, selected_orders, visited_aisles):
        total_units_picked = 0
        for order in selected_orders:
            if order >= 0 and order < len(self.orders):
                total_units_picked += np.sum(list(self.orders[order].values()))
        num_visited_aisles = len(visited_aisles)
        if num_visited_aisles == 0: return 0.0
        return total_units_picked / num_visited_aisles

def calculate_score(dataset_path, result_path):
    """Retorna score ou 0.0 se falhar/inválido"""
    if not os.path.exists(dataset_path) or not os.path.exists(result_path):
        return 0.0
    
    wop = WaveOrderPicking()
    try:
        wop.read_input(dataset_path)
        orders, aisles = wop.read_output(result_path)
        if wop.is_solution_feasible(orders, aisles):
            return wop.compute_objective_function(orders, aisles)
    except:
        pass
    return 0.0

# --- GERADOR DE TABELA LATEX ---

def generate_latex_tables():
    sets = ['a', 'b', 'x']
    
    for set_name in sets:
        results_dir = os.path.join(ROOT_DIR, "results", set_name)
        datasets_dir = os.path.join(ROOT_DIR, "datasets", set_name)
        best_sol_dir = os.path.join(ROOT_DIR, "best_solutions", set_name)
        baseline_sol_dir = os.path.join(ROOT_DIR, "baseline/out", set_name)

        if not os.path.exists(results_dir):
            continue

        result_files = sorted([f for f in os.listdir(results_dir) if f.endswith(".txt")])
        
        # Cabeçalho da Tabela LaTeX
        print("\n" + "%" * 50)
        print(f"% TABELA LATEX PARA O SET: {set_name}")
        print("%" * 50)
        print("\\begin{table}[ht]")
        print("\\centering")
        print(f"\\caption{{Comparativo de Resultados - Instâncias do Conjunto {set_name}}}")
        print("\\label{tab:results_" + set_name + "}")
        print("\\begin{tabular}{c c c c c}")
        print("\\hline")
        # Cabeçalho das colunas conforme solicitado
        print("Instância & Ótimo & Controle & GRASP & Gap (\\%) \\\\")
        print(" & (Itens/Corr.) & (CPLEX) & (Tempo) & (GRASP vs Controle) \\\\")
        print("\\hline")

        # Processamento das linhas
        for idx, filename in enumerate(result_files):
            # Caminhos
            res_path = os.path.join(results_dir, filename)
            data_path = os.path.join(datasets_dir, filename)
            best_path = os.path.join(best_sol_dir, filename)
            baseline_path = os.path.join(baseline_sol_dir, filename)

            # Cálculo dos Scores
            score_grasp = calculate_score(data_path, res_path)
            score_best = calculate_score(data_path, best_path)
            score_control = calculate_score(data_path, baseline_path)

            # Cálculo do Gap (GRASP vs Controle)
            # Fórmula: ((GRASP - Control) / Control) * 100
            # Se for negativo, GRASP foi pior. Se positivo, GRASP foi melhor.
            if score_control > 0:
                gap = ((score_grasp - score_control) / score_control) * 100
                gap_str = f"{gap:.2f}"
            elif score_control == 0 and score_grasp > 0:
                gap_str = "\\infty" # Melhoria infinita sobre 0
            else:
                gap_str = "0.00"

            # Formatação da linha
            # Destaque em negrito se o GRASP for >= ao Controle
            if score_grasp >= score_control and score_control > 0:
                gap_display = f"\\textbf{{{gap_str}}}"
            else:
                gap_display = gap_str

            print(f"{idx} & {score_best:.2f} & {score_control:.2f} & {score_grasp:.2f} & {gap_display} \\\\")

        print("\\hline")
        print("\\end{tabular}")
        print("\\end{table}")
        print("\n")

if __name__ == "__main__":
    generate_latex_tables()