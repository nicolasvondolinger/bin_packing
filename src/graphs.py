import os
import matplotlib.pyplot as plt
import numpy as np
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

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
        with open(output_file_path, 'r') as file:
            lines = file.readlines()
            lines = [line.strip() for line in lines if line.strip()]
            if not lines: return [], []

            try:
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
            except:
                return [], []

        return list(set(selected_orders)), list(set(visited_aisles))

    def is_solution_feasible(self, selected_orders, visited_aisles):
        total_units_picked = 0
        for order in selected_orders:
            if order < 0 or order >= len(self.orders): return False
            total_units_picked += np.sum(list(self.orders[order].values()))

        if not (self.wave_size_lb <= total_units_picked <= self.wave_size_ub):
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

def calculate_score_for_file(dataset_path, result_path):
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

def process_and_plot(set_name):
    results_dir = os.path.join(ROOT_DIR, "results", set_name)
    datasets_dir = os.path.join(ROOT_DIR, "datasets", set_name)
    best_sol_dir = os.path.join(ROOT_DIR, "best_solutions", set_name)
    baseline_sol_dir = os.path.join(ROOT_DIR, "baseline/out", set_name)
    plots_dir = os.path.join(ROOT_DIR, "plots")

    os.makedirs(plots_dir, exist_ok=True)

    if not os.path.exists(results_dir):
        print(f"Set '{set_name}': Pasta de resultados não encontrada.")
        return

    result_files = sorted([f for f in os.listdir(results_dir) if f.endswith(".txt")])
    
    if not result_files:
        print(f"Set '{set_name}': Nenhum arquivo .txt em results.")
        return

    print(f"Gerando gráfico para Set '{set_name}' ({len(result_files)} instâncias encontradas)...")

    instance_names = []
    my_scores = []
    best_scores = []
    baseline_scores = []

    for filename in result_files:
        instance_name = os.path.splitext(filename)[0]
        instance_names.append(instance_name)

        res_path = os.path.join(results_dir, filename)
        data_path = os.path.join(datasets_dir, filename)
        best_path = os.path.join(best_sol_dir, filename)
        baseline_path = os.path.join(baseline_sol_dir, filename)

        my_s = calculate_score_for_file(data_path, res_path)
        best_s = calculate_score_for_file(data_path, best_path)
        baseline_s = calculate_score_for_file(data_path, baseline_path)

        my_scores.append(my_s)
        best_scores.append(best_s)
        baseline_scores.append(baseline_s)

    x = np.arange(len(instance_names))
    
    # Aumentei a largura para 0.25 para ficarem mais grossas
    # O offset agora é igual à largura, o que faz elas ficarem encostadas (padrão)
    # Se quiser um micro espaçamento, poderia usar width=0.24
    width = 0.25

    fig, ax = plt.subplots(figsize=(max(10, len(instance_names) * 0.5), 6))
    
    rects1 = ax.bar(x - width, my_scores, width, label='GRASP', color='tab:blue')
    rects2 = ax.bar(x, best_scores, width, label='Ótimo', color='tab:green')
    rects3 = ax.bar(x + width, baseline_scores, width, label='Baseline', color='tab:orange')

    all_values = my_scores + best_scores + baseline_scores
    if all_values:
        max_val = max(all_values)
        ax.set_ylim(0, max_val * 1.15)

    ax.set_ylabel('Score (Unidades / Corredores)')
    ax.set_xticks(x)
    ax.set_xticklabels(x)
    
    # Legenda fixada na esquerda (superior)
    ax.legend(loc='upper left')
    
    ax.grid(axis='y', linestyle='--', alpha=0.7)

    ax.bar_label(rects1, padding=3, fmt='%.2f', rotation=90, fontsize='small')
    ax.bar_label(rects2, padding=3, fmt='%.2f', rotation=90, fontsize='small')
    ax.bar_label(rects3, padding=3, fmt='%.2f', rotation=90, fontsize='small')

    fig.tight_layout()
    
    output_plot_path = os.path.join(plots_dir, f"{set_name}.png")
    plt.savefig(output_plot_path)
    plt.close(fig)
    print(f"  -> Gráfico salvo em: {output_plot_path}")

if __name__ == "__main__":
    sets = ['a', 'b', 'x']
    print("--- Atualizando Gráficos ---")
    for s in sets:
        process_and_plot(s)
    print("--- Concluído ---")