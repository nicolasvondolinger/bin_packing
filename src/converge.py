import os
import matplotlib.pyplot as plt
import sys

# Define diretórios
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
LOGS_DIR = os.path.join(ROOT_DIR, "convergence_logs")
PLOTS_DIR = os.path.join(ROOT_DIR, "plots", "convergence")

def plot_instance(set_name, instance_name):
    log_file = os.path.join(LOGS_DIR, set_name, f"{instance_name}.log")
    
    if not os.path.exists(log_file):
        print(f"Log não encontrado: {log_file}")
        return

    times = []
    scores = []

    # Leitura do arquivo de log
    try:
        with open(log_file, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) >= 2:
                    times.append(float(parts[0]))
                    scores.append(float(parts[1]))
    except Exception as e:
        print(f"Erro ao ler log {instance_name}: {e}")
        return

    if not times:
        print(f"Log vazio para {instance_name}")
        return

    # Plotagem
    plt.figure(figsize=(10, 6))
    
    # Plota a linha e os pontos de atualização
    plt.plot(times, scores, marker='o', linestyle='-', color='b', label='Melhor Solução')
    
    plt.title(f"Convergência GRASP - Set {set_name} - {instance_name}")
    plt.xlabel("Tempo (segundos)")
    plt.ylabel("Score (Unidades/Corredor)")
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend()
    
    # Garante que começa do 0 no eixo X se quiser ver desde o início
    # plt.xlim(left=0) 

    # Salva o gráfico
    output_folder = os.path.join(PLOTS_DIR, set_name)
    os.makedirs(output_folder, exist_ok=True)
    output_path = os.path.join(output_folder, f"{instance_name}.png")
    
    plt.savefig(output_path)
    plt.close()
    print(f"Gráfico gerado: {output_path}")

if __name__ == "__main__":
    sets = ['a', 'b', 'x']
    
    print("--- Gerando Gráficos de Convergência ---")
    for s in sets:
        set_path = os.path.join(LOGS_DIR, s)
        if os.path.exists(set_path):
            files = sorted([f for f in os.listdir(set_path) if f.endswith(".log")])
            for filename in files:
                inst_name = os.path.splitext(filename)[0]
                plot_instance(s, inst_name)
    print("--- Concluído ---")