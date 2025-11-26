import os
import subprocess
import sys
import numpy as np

# --- Configurações ---
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC_DIR = os.path.abspath(os.path.dirname(__file__))
EXECUTABLE_PATH = os.path.join(SRC_DIR, "bin", "solver")

# --- CLASSE DO CHECKER (Mantida igual, omitida aqui para economizar espaço) ---
class WaveOrderPicking:
    def __init__(self):
        self.orders = None; self.aisles = None; self.wave_size_lb = None; self.wave_size_ub = None
    def read_input(self, input_file_path):
        with open(input_file_path, 'r') as file:
            lines = file.readlines()
            first_line = lines[0].strip().split()
            o, i, a = int(first_line[0]), int(first_line[1]), int(first_line[2])
            self.orders = []
            line_index = 1
            for j in range(o):
                order_line = lines[line_index].strip().split(); line_index += 1; d = int(order_line[0])
                order_map = {int(order_line[2 * k + 1]): int(order_line[2 * k + 2]) for k in range(d)}
                self.orders.append(order_map)
            self.aisles = []
            for j in range(a):
                aisle_line = lines[line_index].strip().split(); line_index += 1; d = int(aisle_line[0])
                aisle_map = {int(aisle_line[2 * k + 1]): int(aisle_line[2 * k + 2]) for k in range(d)}
                self.aisles.append(aisle_map)
            bounds = lines[line_index].strip().split()
            self.wave_size_lb = int(bounds[0]); self.wave_size_ub = int(bounds[1])
    def read_output(self, output_file_path):
        with open(output_file_path, 'r') as file:
            lines = file.readlines()
            lines = [line.strip() for line in lines if line.strip()]
            if not lines: return [], []
            try:
                num_orders = int(lines[0]); current_line = 1; selected_orders = []
                for _ in range(num_orders): selected_orders.append(int(lines[current_line])); current_line += 1
                if current_line < len(lines):
                    num_aisles = int(lines[current_line]); current_line += 1; visited_aisles = []
                    for _ in range(num_aisles): visited_aisles.append(int(lines[current_line])); current_line += 1
                else: visited_aisles = []
            except: return [], []
        return list(set(selected_orders)), list(set(visited_aisles))
    def is_solution_feasible(self, selected_orders, visited_aisles):
        total_units_picked = 0
        for order_idx in selected_orders:
            if order_idx < 0 or order_idx >= len(self.orders): return False
            total_units_picked += np.sum(list(self.orders[order_idx].values()))
        if not (self.wave_size_lb <= total_units_picked <= self.wave_size_ub):
            if total_units_picked == 0 and self.wave_size_lb <= 0: pass
            else: return False
        required_items = {}
        for order_idx in selected_orders:
            for item, quant in self.orders[order_idx].items(): required_items[item] = required_items.get(item, 0) + quant
        for item, total_required in required_items.items():
            total_available = 0
            for aisle_idx in visited_aisles:
                 if aisle_idx < 0 or aisle_idx >= len(self.aisles): return False
                 total_available += self.aisles[aisle_idx].get(item, 0)
            if total_required > total_available: return False
        return True
    def compute_objective_function(self, selected_orders, visited_aisles):
        total_units_picked = 0
        for order_idx in selected_orders: total_units_picked += np.sum(list(self.orders[order_idx].values()))
        num_visited_aisles = len(visited_aisles)
        if num_visited_aisles == 0: return 0.0
        return total_units_picked / num_visited_aisles

# --- FUNÇÕES DE EXECUÇÃO ---

def compile_code():
    print("Iniciando compilação...")
    result = subprocess.run(["make"], capture_output=True, text=True, cwd=SRC_DIR)
    if result.returncode != 0:
        print("--- ERRO NA COMPILAÇÃO ---"); print(result.stderr); return False
    print("Compilação bem-sucedida.")
    return True

def run_set(set_name):
    input_folder = os.path.join(ROOT_DIR, "datasets", set_name)
    output_folder = os.path.join(ROOT_DIR, "results", set_name)
    
    # Nova pasta para os logs de convergência
    log_folder = os.path.join(ROOT_DIR, "convergence_logs", set_name)
    os.makedirs(log_folder, exist_ok=True)
    os.makedirs(output_folder, exist_ok=True)

    instances = sorted([f for f in os.listdir(input_folder) if f.endswith(".txt")])
    if not instances: print(f"Nenhuma instância encontrada em {input_folder}"); return

    print(f"\n--- Rodando Set: {set_name} ({len(instances)} instâncias) ---")

    for filename in instances:
        instance_name = os.path.splitext(filename)[0]
        input_file = os.path.join(input_folder, filename)
        output_file = os.path.join(output_folder, f"{instance_name}.txt")
        
        # Caminho do arquivo de log para esta instância
        log_file = os.path.join(log_folder, f"{instance_name}.log")

        print(f"  Rodando: {filename}")
        wop = WaveOrderPicking()
        try: wop.read_input(input_file)
        except Exception as e: print(f"  ERRO FATAL: {e}"); continue

        try:
            with open(input_file, 'r') as f_in, open(output_file, 'w') as f_out:
                # Passamos o '3' (heurística) E o caminho do log
                cmd = [EXECUTABLE_PATH, '3', log_file]
                
                result = subprocess.run(cmd, stdin=f_in, stdout=f_out, stderr=subprocess.PIPE, text=True, cwd=ROOT_DIR)
                
                if result.returncode != 0:
                    print(f"    ERRO (Código: {result.returncode})"); print(result.stderr)
                else:
                    try:
                        my_orders, my_aisles = wop.read_output(output_file)
                        if wop.is_solution_feasible(my_orders, my_aisles):
                            score = wop.compute_objective_function(my_orders, my_aisles)
                            print(f"    Sucesso. Score: {score:.4f}")
                        else: print(f"    SOLUÇÃO INVÁLIDA.")
                    except: pass
        except Exception as e: print(f"  Erro inesperado: {e}")

if __name__ == "__main__":
    if not compile_code(): sys.exit(1)
    sets_to_run = ['a', 'b', 'x']
    for set_name in sets_to_run: run_set(set_name)
    print("\nExecução concluída! Logs de convergência gerados em 'convergence_logs/'.")