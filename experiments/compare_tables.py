#!/usr/bin/env python3
"""Gera tabelas comparativas com os resultados do artigo (Tabelas 2 e 3)."""

import csv
import sys
from collections import defaultdict

# Tabela 2 do artigo: melhores valores da funcao objetivo
# formato: (grupo, p) -> valor ILS do artigo
PAPER_BEST = {
    ("lin318", 5): 180343.00,
    ("lin318", 15): 89041.59,
    ("lin318", 40): 48091.32,
    ("lin318", 70): 32293.75,
    ("lin318", 100): 23041.87,
    ("ali535", 5): 10003.21,
    ("ali535", 25): 3702.10,
    ("ali535", 50): 2503.34,
    ("ali535", 100): 1462.58,
    ("ali535", 150): 1036.85,
    ("u724", 10): 182036.23,
    ("u724", 30): 95201.56,
    ("u724", 75): 54912.78,
    ("u724", 125): 39127.45,
    ("u724", 200): 28719.14,
    ("rl1304", 10): 2158869.31,
    ("rl1304", 50): 803974.59,
    ("rl1304", 100): 498516.64,
    ("rl1304", 200): 277698.41,
    ("rl1304", 300): 193214.26,
    ("pr2392", 20): 2244317.72,
    ("pr2392", 75): 1099723.42,
    ("pr2392", 150): 713906.38,
    ("pr2392", 300): 459906.71,
    ("pr2392", 500): 317689.13,
    ("fnl4461", 20): 1300603.87,
    ("fnl4461", 100): 551321.58,
    ("fnl4461", 250): 336789.92,
    ("fnl4461", 500): 228994.53,
    ("fnl4461", 1000): 146508.37,
    ("p3038", 600): 122918.75,
    ("p3038", 700): 110923.64,
    ("p3038", 800): 100428.31,
    ("p3038", 900): 92587.42,
    ("p3038", 1000): 86153.89,
}

# Tabela 3 do artigo: valores medios da funcao objetivo
PAPER_MEAN = {
    ("lin318", 5): 181154.00,
    ("lin318", 15): 89205.59,
    ("lin318", 40): 48129.93,
    ("lin318", 70): 32405.49,
    ("lin318", 100): 23208.57,
    ("ali535", 5): 10356.43,
    ("ali535", 25): 3826.45,
    ("ali535", 50): 2539.37,
    ("ali535", 100): 1502.58,
    ("ali535", 150): 1078.35,
    ("u724", 10): 182971.52,
    ("u724", 30): 95821.23,
    ("u724", 75): 55152.13,
    ("u724", 125): 39614.61,
    ("u724", 200): 28897.76,
    ("rl1304", 10): 2225719.83,
    ("rl1304", 50): 809531.15,
    ("rl1304", 100): 499324.28,
    ("rl1304", 200): 278316.37,
    ("rl1304", 300): 193713.08,
    ("pr2392", 20): 2336529.16,
    ("pr2392", 75): 1134301.54,
    ("pr2392", 150): 714281.51,
    ("pr2392", 300): 463987.13,
    ("pr2392", 500): 319698.34,
    ("fnl4461", 20): 1314504.75,
    ("fnl4461", 100): 560981.32,
    ("fnl4461", 250): 340197.14,
    ("fnl4461", 500): 232506.71,
    ("fnl4461", 1000): 150584.92,
    ("p3038", 600): 125098.61,
    ("p3038", 700): 111098.21,
    ("p3038", 800): 101104.56,
    ("p3038", 900): 93287.42,
    ("p3038", 1000): 86934.94,
}

# Referencia CPLEX (Tabela 2)
CPLEX_BEST = {
    ("lin318", 5): 180281.21,
    ("lin318", 15): 88901.56,
    ("lin318", 40): 47988.38,
    ("lin318", 70): 32198.64,
    ("lin318", 100): 22942.69,
    ("ali535", 5): 9956.77,
    ("ali535", 25): 3695.15,
    ("ali535", 50): 2461.41,
    ("ali535", 100): 1438.42,
    ("ali535", 150): 1032.28,
    ("u724", 10): 181782.96,
    ("u724", 30): 95034.01,
    ("u724", 75): 54735.05,
    ("u724", 125): 38976.76,
    ("u724", 200): 28079.97,
    ("rl1304", 10): 2146484.10,
    ("rl1304", 50): 802283.41,
    ("rl1304", 100): 495925.93,
    ("rl1304", 200): 276977.60,
    ("rl1304", 300): 191224.85,
}


def load_csv(filepath):
    """Carrega CSV de resultados e agrupa por (grupo, p)."""
    results = defaultdict(list)
    with open(filepath) as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row["group"], int(row["p"]))
            results[key].append({
                "run": int(row["run"]),
                "seed": int(row["seed"]),
                "grasp_cost": float(row["grasp_cost"]),
                "vnd_cost": float(row["vnd_cost"]),
                "ils_cost": float(row["ils_cost"]),
                "ils_iterations": int(row["ils_iterations"]),
                "ils_improvements": int(row["ils_improvements"]),
                "time_total_s": float(row["time_total_s"]),
            })
    return results


def gap_pct(value, reference):
    """Calcula gap percentual."""
    if reference == 0:
        return float("inf")
    return (value - reference) / reference * 100.0


def print_table2(results):
    """Tabela 2: melhores valores (comparacao com artigo e CPLEX)."""
    print("=" * 100)
    print("TABELA 2 — Melhores valores da funcao objetivo")
    print("=" * 100)
    header = f"{'Instancia':<16} {'Nosso':>12} {'Artigo':>12} {'CPLEX':>12} {'Gap Art(%)':>10} {'Gap CPLEX(%)':>12}"
    print(header)
    print("-" * 100)

    groups_order = ["lin318", "ali535", "u724", "rl1304", "pr2392", "fnl4461", "p3038"]
    sorted_keys = sorted(results.keys(), key=lambda k: (groups_order.index(k[0]) if k[0] in groups_order else 99, k[1]))

    for key in sorted_keys:
        group, p = key
        runs = results[key]
        our_best = min(r["ils_cost"] for r in runs)

        paper = PAPER_BEST.get(key)
        cplex = CPLEX_BEST.get(key)

        paper_str = f"{paper:>12.2f}" if paper else f"{'—':>12}"
        cplex_str = f"{cplex:>12.2f}" if cplex else f"{'—':>12}"
        gap_art = f"{gap_pct(our_best, paper):>10.2f}" if paper else f"{'—':>10}"
        gap_cpx = f"{gap_pct(our_best, cplex):>12.2f}" if cplex else f"{'—':>12}"

        print(f"{group}_{p:<10} {our_best:>12.2f} {paper_str} {cplex_str} {gap_art} {gap_cpx}")

    print()


def print_table3(results):
    """Tabela 3: valores medios (comparacao com artigo)."""
    print("=" * 100)
    print("TABELA 3 — Valores medios da funcao objetivo")
    print("=" * 100)
    header = f"{'Instancia':<16} {'Nosso':>12} {'Artigo':>12} {'Gap(%)':>10} {'Tempo med(s)':>12} {'Runs':>6}"
    print(header)
    print("-" * 100)

    groups_order = ["lin318", "ali535", "u724", "rl1304", "pr2392", "fnl4461", "p3038"]
    sorted_keys = sorted(results.keys(), key=lambda k: (groups_order.index(k[0]) if k[0] in groups_order else 99, k[1]))

    for key in sorted_keys:
        group, p = key
        runs = results[key]
        n_runs = len(runs)
        our_mean = sum(r["ils_cost"] for r in runs) / n_runs
        mean_time = sum(r["time_total_s"] for r in runs) / n_runs

        paper = PAPER_MEAN.get(key)
        paper_str = f"{paper:>12.2f}" if paper else f"{'—':>12}"
        gap = f"{gap_pct(our_mean, paper):>10.2f}" if paper else f"{'—':>10}"

        print(f"{group}_{p:<10} {our_mean:>12.2f} {paper_str} {gap} {mean_time:>12.2f} {n_runs:>6}")

    print()


def print_summary(results):
    """Resumo geral."""
    print("=" * 100)
    print("RESUMO")
    print("=" * 100)

    total_instances = len(results)
    total_runs = sum(len(v) for v in results.values())

    # Gap medio vs artigo (best)
    gaps_best = []
    for key, runs in results.items():
        paper = PAPER_BEST.get(key)
        if paper:
            our_best = min(r["ils_cost"] for r in runs)
            gaps_best.append(gap_pct(our_best, paper))

    # Gap medio vs artigo (mean)
    gaps_mean = []
    for key, runs in results.items():
        paper = PAPER_MEAN.get(key)
        if paper:
            our_mean = sum(r["ils_cost"] for r in runs) / len(runs)
            gaps_mean.append(gap_pct(our_mean, paper))

    print(f"Instancias testadas: {total_instances}")
    print(f"Total de runs: {total_runs}")
    if gaps_best:
        print(f"Gap medio (melhor vs artigo melhor): {sum(gaps_best)/len(gaps_best):.2f}%")
    if gaps_mean:
        print(f"Gap medio (medio vs artigo medio):   {sum(gaps_mean)/len(gaps_mean):.2f}%")
    print()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Uso: {sys.argv[0]} <arquivo_csv>")
        sys.exit(1)

    results = load_csv(sys.argv[1])
    print_table2(results)
    print_table3(results)
    print_summary(results)
