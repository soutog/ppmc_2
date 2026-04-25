#!/usr/bin/env python3
"""
Gera artefatos de publicacao a partir do CSV de resultados do Hybrid-CS-AILS:
  - CSV consolidado (resumo.csv) com best/mean/std/cv/tempo + gaps
  - Tabela T2 (Best vs literatura, gap em convencao padrao positivo=pior)
  - Tabela T3 (Mean +/- Std, CV%, tempo medio vs Freitas Mean)
  - Tabela T_family (agregada por familia de instancia)
  - Boxplots por grupo (um PDF por familia) — custo absoluto
  - Boxplot normalizado: gap% vs BKS para Hybrid/Freitas em 1 figura
  - Performance Profile (Dolan & More 2002) de Best e Mean
  - Scatter de ganho (Hybrid_mean - Freitas_mean) em gap% vs p/n
  - Wilcoxon signed-rank pareado, uma cauda, sobre gap% (Hybrid < Freitas)
  - Auditoria de gaps suspeitos (> 1.5% em absoluto vs BKS)

Uso:
    python make_publication_tables.py <arquivo_csv> [--out DIR]

Gera em <out>:
    tables/T2_best.{txt,tex}
    tables/T3_mean.{txt,tex}
    tables/wilcoxon.txt
    tables/audit.txt
    figures/boxplot_<grupo>.pdf

Convencoes:
    gap(Z) = (Z - BKS) / BKS * 100.0
    positivo = pior que BKS; negativo = melhor que BKS
    Best = min(final_cost) em N runs
    Mean = media(final_cost)
    Std  = desvio amostral (ddof=1)
    CV   = Std / Mean * 100 (%)
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import statistics
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy import stats as scipy_stats


# =============================================================================
# 1. DADOS DE LITERATURA
#
# Todos os valores sao "Best" salvo anotacao. BKS prefere:
#   - Otimo CPLEX quando Stefanello 2015 reporta "opt" na Tabela 7
#   - Min de 10 runs do IRMA (Stefanello 2015) caso contrario
#   - Freitas 2025 quando nao ha baseline de Stefanello
#
# VASCONCELOS_* deixado para o usuario preencher com os numeros reais do
# artigo de 2023; por enquanto fica vazio (linhas aparecem com "-").
# =============================================================================

# Stefanello 2015, Tabela 7 — conjunto de 35 instancias (Set 5)
# BKS coincidem com CPLEX opt onde ambos reportam (lin318, ali535 pequenos etc.)
BKS: dict[tuple[str, int], float] = {
    ("lin318", 5):      180281.20,
    ("lin318", 15):      88901.56,
    ("lin318", 40):      47988.38,
    ("lin318", 70):      32198.64,
    ("lin318", 100):     22942.69,
    ("ali535", 5):        9956.77,
    ("ali535", 25):       3695.15,
    ("ali535", 50):       2461.41,
    ("ali535", 100):      1438.42,
    ("ali535", 150):      1032.28,
    ("u724", 10):       181782.96,
    ("u724", 30):        95034.01,
    ("u724", 75):        54735.05,
    ("u724", 125):       38976.76,
    ("u724", 200):       28079.97,
    ("rl1304", 10):    2146484.10,
    ("rl1304", 50):     802283.41,
    ("rl1304", 100):    498090.74,
    ("rl1304", 200):    276977.60,
    ("rl1304", 300):    191224.85,
    ("pr2392", 20):    2235376.73,
    ("pr2392", 75):    1092294.02,
    ("pr2392", 150):    711119.29,
    ("pr2392", 300):    458145.29,
    ("pr2392", 500):    316042.97,
    ("fnl4461", 20):   1283536.73,
    ("fnl4461", 100):   548909.01,
    ("fnl4461", 250):   335888.87,
    ("fnl4461", 500):   224684.24,
    ("fnl4461", 1000):  145862.38,
    # p3038: Stefanello Set 4 (Tabela 6); usamos IRMA Min como baseline.
    ("p3038", 600):     122711.17,
    ("p3038", 700):     109677.30,
    ("p3038", 800):     100064.94,
    ("p3038", 900):      92310.09,
    ("p3038", 1000):     85854.05,
}

# Freitas SBPO 2025 — Tabela 2 (Best de 30 runs)
FREITAS_BEST: dict[tuple[str, int], float] = {
    ("lin318", 5):      180343.00,
    ("lin318", 15):      89041.59,
    ("lin318", 40):      48091.32,
    ("lin318", 70):      32293.75,
    ("lin318", 100):     23041.87,
    ("ali535", 5):       10003.21,
    ("ali535", 25):       3702.10,
    ("ali535", 50):       2503.34,
    ("ali535", 100):      1462.58,
    ("ali535", 150):      1036.85,
    ("u724", 10):       182036.23,
    ("u724", 30):        95201.56,
    ("u724", 75):        54912.78,
    ("u724", 125):       39127.45,
    ("u724", 200):       28719.14,
    ("rl1304", 10):    2158869.31,
    ("rl1304", 50):     803974.59,
    ("rl1304", 100):    498516.64,
    ("rl1304", 200):    277698.41,
    ("rl1304", 300):    193214.26,
    ("pr2392", 20):    2244317.72,
    ("pr2392", 75):    1099723.42,
    ("pr2392", 150):    713906.38,
    ("pr2392", 300):    459906.71,
    ("pr2392", 500):    317689.13,
    ("fnl4461", 20):   1300603.87,
    ("fnl4461", 100):   551321.58,
    ("fnl4461", 250):   336789.92,
    ("fnl4461", 500):   228994.53,
    ("fnl4461", 1000):  146508.37,
    ("p3038", 600):     122918.75,
    ("p3038", 700):     110923.64,
    ("p3038", 800):     100428.31,
    ("p3038", 900):      92587.42,
    ("p3038", 1000):     86153.89,
}

# Freitas SBPO 2025 — Tabela 3 (Mean de 30 runs)
FREITAS_MEAN: dict[tuple[str, int], float] = {
    ("lin318", 5):      181154.00,
    ("lin318", 15):      89205.59,
    ("lin318", 40):      48129.93,
    ("lin318", 70):      32405.49,
    ("lin318", 100):     23208.57,
    ("ali535", 5):       10356.43,
    ("ali535", 25):       3826.45,
    ("ali535", 50):       2539.37,
    ("ali535", 100):      1502.58,
    ("ali535", 150):      1078.35,
    ("u724", 10):       182971.52,
    ("u724", 30):        95821.23,
    ("u724", 75):        55152.13,
    ("u724", 125):       39614.61,
    ("u724", 200):       28897.76,
    ("rl1304", 10):    2225719.83,
    ("rl1304", 50):     809531.15,
    ("rl1304", 100):    499324.28,
    ("rl1304", 200):    278316.37,
    ("rl1304", 300):    193713.08,
    ("pr2392", 20):    2336529.16,
    ("pr2392", 75):    1134301.54,
    ("pr2392", 150):    714281.51,
    ("pr2392", 300):    463987.13,
    ("pr2392", 500):    319698.34,
    ("fnl4461", 20):   1314504.75,
    ("fnl4461", 100):   560981.32,
    ("fnl4461", 250):   340197.14,
    ("fnl4461", 500):   232506.71,
    ("fnl4461", 1000):  150584.92,
    ("p3038", 600):     125098.61,
    ("p3038", 700):     111098.21,
    ("p3038", 800):     101104.56,
    ("p3038", 900):      93287.42,
    ("p3038", 1000):     86934.94,
}

# Stefanello 2015 — Min de 10 runs (Phase 3, Tabela 7 para Set 5; Tabela 6
# para p3038). Onde nao ha valor distinto da BKS, reusamos BKS.
STEFANELLO_BEST: dict[tuple[str, int], float] = {key: val for key, val in BKS.items()}

# Vasconcellos 2023 — GVNS. Valores transcritos da tabela consolidada.
VASCONCELOS_BEST: dict[tuple[str, int], float] = {
    ("lin318", 5):     180281.21, ("lin318", 15):     89026.28,
    ("lin318", 40):     47982.75, ("lin318", 70):     32201.75,
    ("lin318", 100):    22971.72,
    ("ali535", 5):       9933.43, ("ali535", 25):      3688.51,
    ("ali535", 50):      2458.70, ("ali535", 100):     1433.60,
    ("ali535", 150):     1025.37,
    ("u724", 10):      181823.17, ("u724", 30):       95222.02,
    ("u724", 75):       54913.01, ("u724", 125):      39196.73,
    ("u724", 200):      28340.10,
    ("rl1304", 10):   2221470.50, ("rl1304", 50):    803150.33,
    ("rl1304", 100):   497796.79, ("rl1304", 200):   276310.25,
    ("rl1304", 300):   191544.95,
    ("pr2392", 20):   2332221.94, ("pr2392", 75):   1132362.51,
    ("pr2392", 150):   733273.63, ("pr2392", 300):   475276.82,
    ("pr2392", 500):   321174.72,
    ("fnl4461", 20):  1311798.15, ("fnl4461", 100):  560030.68,
    ("fnl4461", 250):  339654.89, ("fnl4461", 500):  232173.69,
    ("fnl4461", 1000): 150366.44,
    ("p3038", 600):    122752.19, ("p3038", 700):    109381.22,
    ("p3038", 800):     99054.98, ("p3038", 900):     90969.96,
    ("p3038", 1000):    83693.38,
}

VASCONCELOS_MEAN: dict[tuple[str, int], float] = {
    ("lin318", 5):     180284.20, ("lin318", 15):     88905.20,
    ("lin318", 40):     47995.69, ("lin318", 70):     32200.35,
    ("lin318", 100):    23168.12,
    ("ali535", 5):       9956.60, ("ali535", 25):      3695.22,
    ("ali535", 50):      2510.21, ("ali535", 100):     1451.92,
    ("ali535", 150):     1032.46,
    ("u724", 10):      181586.01, ("u724", 30):       95043.90,
    ("u724", 75):       54737.77, ("u724", 125):      38976.76,
    ("u724", 200):      28592.14,
    ("rl1304", 10):   2158039.60, ("rl1304", 50):    799718.88,
    ("rl1304", 100):   495945.74, ("rl1304", 200):   277477.09,
    ("rl1304", 300):   192995.41,
    ("pr2392", 20):   2235448.34, ("pr2392", 75):   1092362.64,
    ("pr2392", 150):   711157.29, ("pr2392", 300):   458170.55,
    ("pr2392", 500):   316056.12,
    ("fnl4461", 20):  1296295.22, ("fnl4461", 100):  548917.23,
    ("fnl4461", 250):  335911.49, ("fnl4461", 500):  228165.55,
    ("fnl4461", 1000): 147703.03,
    # p3038 mean nao reportado em varias instancias na tabela consolidada
}

# Stefanello 2015 (IRMA) — best e mean conforme tabela consolidada.
STEFANELLO_MEAN: dict[tuple[str, int], float] = {
    ("lin318", 5):     180281.21, ("lin318", 15):     88901.56,
    ("lin318", 40):     48003.88, ("lin318", 70):     32290.39,
    ("lin318", 100):    22942.69,
    ("ali535", 5):      10210.29, ("ali535", 25):      3701.88,
    ("ali535", 50):      2478.04, ("ali535", 100):     1448.00,
    ("ali535", 150):     1037.70,
    ("u724", 10):      182611.19, ("u724", 30):       95159.96,
    ("u724", 75):       54735.50, ("u724", 125):      38976.76,
    ("u724", 200):      28082.72,
    ("rl1304", 10):   2166552.03, ("rl1304", 50):    806425.28,
    ("rl1304", 100):   498411.69, ("rl1304", 200):   276989.47,
    ("rl1304", 300):   191400.09,
    ("pr2392", 20):   2250292.41, ("pr2392", 75):   1098559.96,
    ("pr2392", 150):   711656.01, ("pr2392", 300):   458298.21,
    ("pr2392", 500):   316230.72,
    ("fnl4461", 20):  1292621.57, ("fnl4461", 100):  550758.21,
    ("fnl4461", 250):  336006.96, ("fnl4461", 500):  224684.37,
    ("fnl4461", 1000): 145870.78,
    ("p3038", 600):    122724.49, ("p3038", 700):    109023.64,
    ("p3038", 800):    100084.41, ("p3038", 900):     92317.78,
    ("p3038", 1000):    85854.05,
}

# Tempos medios (segundos). ILS = Freitas 2025; IRMA = Stefanello 2015;
# GVNS = Vasconcellos 2023.
T_ILS: dict[tuple[str, int], float] = {
    ("lin318", 5): 345.96, ("lin318", 15): 598.87, ("lin318", 40): 840.59,
    ("lin318", 70): 537.69, ("lin318", 100): 754.01,
    ("ali535", 5): 2348.63, ("ali535", 25): 2586.15, ("ali535", 50): 2387.95,
    ("ali535", 100): 2147.16, ("ali535", 150): 2546.97,
    ("u724", 10): 1023.17, ("u724", 30): 1528.38, ("u724", 75): 1349.53,
    ("u724", 125): 1674.26, ("u724", 200): 2019.60,
    ("rl1304", 10): 5413.34, ("rl1304", 50): 3785.60, ("rl1304", 100): 3243.15,
    ("rl1304", 200): 4209.47, ("rl1304", 300): 5394.30,
    ("pr2392", 20): 3852.16, ("pr2392", 75): 3979.23, ("pr2392", 150): 4512.67,
    ("pr2392", 300): 3629.41, ("pr2392", 500): 4763.56,
    ("fnl4461", 20): 8523.18, ("fnl4461", 100): 9046.80, ("fnl4461", 250): 9219.85,
    ("fnl4461", 500): 9804.23, ("fnl4461", 1000): 9102.87,
    ("p3038", 600): 4603.21, ("p3038", 700): 4309.72, ("p3038", 800): 5197.64,
    ("p3038", 900): 5427.38, ("p3038", 1000): 5673.22,
}

T_IRMA: dict[tuple[str, int], float] = {
    ("lin318", 5): 13.77, ("lin318", 15): 39.64, ("lin318", 40): 480.64,
    ("lin318", 70): 191.75, ("lin318", 100): 548.95,
    ("ali535", 5): 68.33, ("ali535", 25): 818.16, ("ali535", 50): 1092.75,
    ("ali535", 100): 959.36, ("ali535", 150): 1145.43,
    ("u724", 10): 89.74, ("u724", 30): 452.45, ("u724", 75): 822.06,
    ("u724", 125): 967.89, ("u724", 200): 1062.64,
    ("rl1304", 10): 273.32, ("rl1304", 50): 1805.42, ("rl1304", 100): 2458.70,
    ("rl1304", 200): 1847.24, ("rl1304", 300): 1431.96,
    ("pr2392", 20): 830.24, ("pr2392", 75): 1242.53, ("pr2392", 150): 3038.00,
    ("pr2392", 300): 3584.41, ("pr2392", 500): 3614.83,
    ("fnl4461", 20): 810.90, ("fnl4461", 100): 5838.20, ("fnl4461", 250): 6909.87,
    ("fnl4461", 500): 5886.30, ("fnl4461", 1000): 5065.50,
    ("p3038", 600): 2752.51, ("p3038", 700): 2295.84, ("p3038", 800): 2889.74,
    ("p3038", 900): 1617.62, ("p3038", 1000): 1920.93,
}

T_GVNS: dict[tuple[str, int], float] = {
    ("lin318", 5): 358.0, ("lin318", 15): 574.0, ("lin318", 40): 853.0,
    ("lin318", 70): 527.0, ("lin318", 100): 752.0,
    ("ali535", 5): 2342.0, ("ali535", 25): 2612.0, ("ali535", 50): 2320.0,
    ("ali535", 100): 2113.0, ("ali535", 150): 2516.0,
    ("u724", 10): 1017.0, ("u724", 30): 1525.0, ("u724", 75): 1324.0,
    ("u724", 125): 1675.0, ("u724", 200): 2114.0,
    ("rl1304", 10): 5385.0, ("rl1304", 50): 3715.0, ("rl1304", 100): 3198.0,
    ("rl1304", 200): 4179.0, ("rl1304", 300): 5411.0,
    ("pr2392", 20): 3796.0, ("pr2392", 75): 3981.0, ("pr2392", 150): 4438.0,
    ("pr2392", 300): 3635.0, ("pr2392", 500): 4717.0,
    ("fnl4461", 20): 8522.0, ("fnl4461", 100): 9115.0, ("fnl4461", 250): 9411.0,
    ("fnl4461", 500): 9781.0, ("fnl4461", 1000): 9251.0,
    ("p3038", 600): 4549.0, ("p3038", 700): 4316.0, ("p3038", 800): 5283.0,
    ("p3038", 900): 5384.0, ("p3038", 1000): 5688.0,
}

# CPLEX (Stefanello 2015 Tabela 7). Nem sempre otimo provado — muitas vezes
# com time limit. Disponivel apenas para as familias pequenas.
CPLEX_BEST: dict[tuple[str, int], float] = {
    ("lin318", 5):    180281.21, ("lin318", 15):    88901.56,
    ("lin318", 40):    47988.38, ("lin318", 70):    32198.64,
    ("lin318", 100):   22942.69,
    ("ali535", 5):      9956.77, ("ali535", 25):     3695.15,
    ("ali535", 50):     2461.41, ("ali535", 100):    1438.42,
    ("ali535", 150):    1032.28,
    ("u724", 10):     181782.96, ("u724", 30):      95034.01,
    ("u724", 75):      54735.05, ("u724", 125):     38976.76,
    ("u724", 200):     28079.97,
    ("rl1304", 10):  2146484.10, ("rl1304", 50):   802283.41,
    ("rl1304", 100):  495925.93, ("rl1304", 200):  276977.60,
    ("rl1304", 300):  191224.85,
}


GROUPS_ORDER = ["lin318", "ali535", "u724", "rl1304", "pr2392", "fnl4461", "p3038"]

# BKS verdadeira = min conhecido entre CPLEX, IRMA, GVNS (bests da literatura).
# Note: Hybrid pode sobrescrever se for melhor no runtime (ex: u724_10).
def _compute_true_bks() -> dict[tuple[str, int], float]:
    bks: dict[tuple[str, int], float] = {}
    for key in set(BKS.keys()) | set(VASCONCELOS_BEST.keys()):
        candidates = []
        if key in BKS:              candidates.append(BKS[key])
        if key in CPLEX_BEST:       candidates.append(CPLEX_BEST[key])
        if key in VASCONCELOS_BEST: candidates.append(VASCONCELOS_BEST[key])
        if key in FREITAS_BEST:     candidates.append(FREITAS_BEST[key])
        if candidates:
            bks[key] = min(candidates)
    return bks

# Sobrescreve a BKS antiga com o min global.
BKS = _compute_true_bks()


# =============================================================================
# 2. IO + ESTATISTICAS
# =============================================================================

def load_csv(filepath: str) -> dict[tuple[str, int], list[dict]]:
    results: dict[tuple[str, int], list[dict]] = defaultdict(list)
    with open(filepath) as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row["group"], int(row["p"]))
            results[key].append(
                {
                    "run": int(row["run"]) if "run" in row and row["run"] else 0,
                    "seed": int(row["seed"]),
                    "final_cost": float(row["final_cost"]),
                    "time_total_s": float(row["time_total_s"]),
                }
            )
    return results


def gap_pct(value: float, ref: float | None) -> float | None:
    if ref is None or ref == 0.0 or value is None:
        return None
    return (value - ref) / ref * 100.0


def compute_stats(runs: list[dict]) -> dict[str, float]:
    costs = [r["final_cost"] for r in runs]
    times = [r["time_total_s"] for r in runs]
    mean = statistics.fmean(costs)
    std = statistics.stdev(costs) if len(costs) > 1 else 0.0
    return {
        "n": len(costs),
        "best": min(costs),
        "mean": mean,
        "std": std,
        "cv_pct": (std / mean * 100.0) if mean > 0 else 0.0,
        "time_mean": statistics.fmean(times),
    }


def sorted_keys(keys: list[tuple[str, int]]) -> list[tuple[str, int]]:
    return sorted(
        keys,
        key=lambda k: (
            GROUPS_ORDER.index(k[0]) if k[0] in GROUPS_ORDER else 99,
            k[1],
        ),
    )


# =============================================================================
# 3. TABELA T2 — BEST vs literatura
# =============================================================================

def build_table_t2(results) -> tuple[str, str]:
    """Retorna (txt, tex)."""
    header_cols = [
        "Instancia", "BKS",
        "Hybrid", "gap_H%",
        "Freitas", "gap_F%",
        "Stefanello", "gap_S%",
        "Vasconcelos", "gap_V%",
    ]
    rows_txt: list[list[str]] = []
    rows_tex: list[list[str]] = []

    for key in sorted_keys(list(results.keys())):
        group, p = key
        stats = compute_stats(results[key])
        bks = BKS.get(key)
        hybrid = stats["best"]
        freitas = FREITAS_BEST.get(key)
        stefanello = STEFANELLO_BEST.get(key)
        vasc = VASCONCELOS_BEST.get(key)

        gap_h = gap_pct(hybrid, bks)
        gap_f = gap_pct(freitas, bks)
        gap_s = gap_pct(stefanello, bks)
        gap_v = gap_pct(vasc, bks)

        # Marca em negrito no tex o menor Best entre Hybrid/Freitas/Stefanello/Vasconcelos.
        candidates = {
            "H": hybrid, "F": freitas, "S": stefanello, "V": vasc,
        }
        present = {k: v for k, v in candidates.items() if v is not None}
        winner = min(present, key=present.get) if present else None

        def fmt(val, winner_tag=None):
            if val is None:
                return "-"
            s = f"{val:.2f}"
            return s

        def fmt_tex(val, tag):
            if val is None:
                return "-"
            s = f"{val:.2f}"
            if tag == winner:
                s = r"\textbf{" + s + "}"
            return s

        def fmt_gap(g):
            if g is None:
                return "-"
            return f"{g:+.3f}"

        rows_txt.append([
            f"{group}_{p}",
            fmt(bks),
            fmt(hybrid),    fmt_gap(gap_h),
            fmt(freitas),   fmt_gap(gap_f),
            fmt(stefanello),fmt_gap(gap_s),
            fmt(vasc),      fmt_gap(gap_v),
        ])
        rows_tex.append([
            f"{group}\\_{p}",
            fmt(bks),
            fmt_tex(hybrid, "H"),     fmt_gap(gap_h),
            fmt_tex(freitas, "F"),    fmt_gap(gap_f),
            fmt_tex(stefanello, "S"), fmt_gap(gap_s),
            fmt_tex(vasc, "V"),       fmt_gap(gap_v),
        ])

    txt = render_txt_table("T2 - Best values vs literatura "
                           "(gap = (Z-BKS)/BKS*100, positivo=pior)",
                           header_cols, rows_txt)
    tex = render_tex_table(
        caption="Melhores valores da funcao objetivo (10 runs). "
                "Gap relativo a BKS (Stefanello 2015). "
                "Negrito: menor Best por instancia.",
        label="tab:best",
        columns=header_cols,
        rows=rows_tex,
        align="lrrrrrrrrr",
    )
    return txt, tex


# =============================================================================
# 4. TABELA T3 — MEAN +/- STD, CV%, tempo vs Freitas Mean
# =============================================================================

def build_table_t3(results) -> tuple[str, str]:
    header_cols = [
        "Instancia", "n",
        "Mean", "Std", "CV%",
        "Freitas_M", "gap_M%",
        "t_avg(s)",
    ]
    rows_txt: list[list[str]] = []
    rows_tex: list[list[str]] = []

    for key in sorted_keys(list(results.keys())):
        group, p = key
        stats = compute_stats(results[key])
        freitas_mean = FREITAS_MEAN.get(key)
        gap_m = gap_pct(stats["mean"], freitas_mean)

        def fmt_mean():
            return f"{stats['mean']:.2f}"

        def fmt_std():
            return f"{stats['std']:.2f}"

        def fmt_cv():
            return f"{stats['cv_pct']:.3f}"

        def fmt_freitas():
            return f"{freitas_mean:.2f}" if freitas_mean is not None else "-"

        def fmt_gap():
            return f"{gap_m:+.3f}" if gap_m is not None else "-"

        row_common = [
            f"{group}_{p}",
            str(stats["n"]),
            fmt_mean(), fmt_std(), fmt_cv(),
            fmt_freitas(), fmt_gap(),
            f"{stats['time_mean']:.1f}",
        ]
        row_tex = list(row_common)
        row_tex[0] = f"{group}\\_{p}"
        rows_txt.append(row_common)
        rows_tex.append(row_tex)

    txt = render_txt_table(
        "T3 - Media, desvio padrao, coeficiente de variacao e tempo "
        "(gap_M = (Mean - FreitasMean)/FreitasMean*100, positivo=pior)",
        header_cols, rows_txt,
    )
    tex = render_tex_table(
        caption="Valores medios (10 runs), desvio-padrao amostral, coeficiente "
                "de variacao (CV\\%) e tempo medio. Gap relativo a media "
                "reportada por Freitas et al.~2025.",
        label="tab:mean",
        columns=header_cols,
        rows=rows_tex,
        align="lrrrrrrr",
    )
    return txt, tex


# =============================================================================
# 5. WILCOXON SIGNED-RANK PAREADO
# =============================================================================

def run_wilcoxon(results) -> str:
    lines = [
        "Teste de Wilcoxon signed-rank pareado (duas caudas)",
        "H0: mediana de (Hybrid_mean - Freitas_mean) = 0",
        "=" * 64,
    ]
    pairs_all = []
    pairs_without_p3038 = []  # p3038 e caso limite — separamos
    for key in sorted_keys(list(results.keys())):
        group, _p = key
        stats = compute_stats(results[key])
        freitas = FREITAS_MEAN.get(key)
        if freitas is None:
            continue
        diff = stats["mean"] - freitas
        pairs_all.append((stats["mean"], freitas, diff, key))
        if group != "p3038":
            pairs_without_p3038.append((stats["mean"], freitas, diff, key))

    def report(label, sample):
        if len(sample) < 6:
            lines.append(f"\n[{label}] amostra {len(sample)} pequena demais para Wilcoxon.")
            return
        hybrid = np.array([s[0] for s in sample])
        freitas = np.array([s[1] for s in sample])
        diff = np.array([s[2] for s in sample])
        # zero_method='wilcox' descarta zeros (padrao); duas caudas.
        try:
            stat, pval = scipy_stats.wilcoxon(
                hybrid, freitas, zero_method="wilcox", alternative="two-sided"
            )
        except ValueError as exc:
            lines.append(f"\n[{label}] falha no Wilcoxon: {exc}")
            return
        # Effect size r = Z / sqrt(N). scipy retorna W, calculamos Z aproximado via normal.
        n = len(diff)
        mean_w = n * (n + 1) / 4.0
        std_w = math.sqrt(n * (n + 1) * (2 * n + 1) / 24.0)
        z = (stat - mean_w) / std_w if std_w > 0 else float("nan")
        r = abs(z) / math.sqrt(n) if n > 0 else float("nan")

        n_better = int(np.sum(diff < 0))
        n_worse = int(np.sum(diff > 0))
        n_tie   = int(np.sum(diff == 0))
        lines.append(f"\n[{label}]")
        lines.append(f"  N={n}  melhor={n_better}  pior={n_worse}  empate={n_tie}")
        lines.append(f"  W={stat:.2f}  Z~={z:.3f}  p={pval:.5f}  |r|={r:.3f}")
        if pval < 0.001:
            verdict = "p < 0.001 -> diferenca altamente significativa."
        elif pval < 0.01:
            verdict = "p < 0.01  -> diferenca significativa."
        elif pval < 0.05:
            verdict = "p < 0.05  -> diferenca significativa."
        else:
            verdict = "p >= 0.05 -> sem evidencia de diferenca."
        lines.append(f"  {verdict}")

    report("Todas as instancias", pairs_all)
    report("Excluindo p3038 (familia com pior desempenho)", pairs_without_p3038)
    return "\n".join(lines) + "\n"


# =============================================================================
# 6. AUDITORIA DE GAPS SUSPEITOS
# =============================================================================

def run_audit(results) -> str:
    lines = [
        "Auditoria de instancias com gap absoluto > 1.5% vs BKS",
        "=" * 64,
        "Formato: gap_best_vs_BKS%  gap_mean_vs_BKS%  Best  Mean  BKS",
        "",
    ]
    flagged = []
    for key in sorted_keys(list(results.keys())):
        group, p = key
        stats = compute_stats(results[key])
        bks = BKS.get(key)
        if bks is None:
            continue
        gap_b = gap_pct(stats["best"], bks)
        gap_m = gap_pct(stats["mean"], bks)
        if gap_b is None:
            continue
        if abs(gap_b) > 1.5 or (gap_m is not None and abs(gap_m) > 2.0):
            flagged.append((key, gap_b, gap_m, stats["best"], stats["mean"], bks))

    if not flagged:
        lines.append("(sem instancias com gap > 1.5% — OK)")
    else:
        for (g, p), gb, gm, best, mean, bks in flagged:
            lines.append(
                f"  {g}_{p:<5}  gap_best={gb:+7.3f}%  "
                f"gap_mean={gm:+7.3f}%  best={best:.2f}  "
                f"mean={mean:.2f}  BKS={bks:.2f}"
            )

    # Check direto no u724_10 (C7 do estudo ORION).
    key = ("u724", 10)
    if key in results:
        stats = compute_stats(results[key])
        bks = BKS.get(key)
        lines.append("")
        lines.append("Verificacao do u724_10 (ponto flagged como +19.5% no summary):")
        lines.append(f"  best={stats['best']:.4f}  mean={stats['mean']:.4f}")
        lines.append(f"  BKS={bks:.4f}  Freitas_best={FREITAS_BEST.get(key)}")
        lines.append(f"  gap_best_vs_BKS = {gap_pct(stats['best'], bks):+.4f}%")
        lines.append(f"  gap_mean_vs_BKS = {gap_pct(stats['mean'], bks):+.4f}%")
        lines.append(f"  gap_best_vs_Freitas = "
                     f"{gap_pct(stats['best'], FREITAS_BEST.get(key)):+.4f}%")
        if stats["best"] < bks:
            lines.append("  => Hybrid encontrou solucao estritamente MELHOR que a BKS "
                         "reportada por Stefanello. Revisar se houve atualizacao de BKS "
                         "ou se o run passou no validate().")
        if abs(gap_pct(stats["best"], bks) or 0.0) < 0.1:
            lines.append("  => NAO ha gap real de ~20%; o valor do summary estava incorreto.")

    return "\n".join(lines) + "\n"


# =============================================================================
# 7. BOXPLOTS POR GRUPO
# =============================================================================

def make_boxplots(results, fig_dir: Path) -> list[Path]:
    generated = []
    by_group: dict[str, list[tuple[int, list[float]]]] = defaultdict(list)
    for key in sorted_keys(list(results.keys())):
        group, p = key
        costs = [r["final_cost"] for r in results[key]]
        by_group[group].append((p, costs))

    for group, entries in by_group.items():
        entries.sort(key=lambda x: x[0])
        p_values = [e[0] for e in entries]
        data = [e[1] for e in entries]

        fig, ax = plt.subplots(figsize=(1.1 + 0.9 * len(p_values), 4.2))
        bp = ax.boxplot(
            data, labels=[str(p) for p in p_values],
            showmeans=True, meanline=True,
            patch_artist=True,
        )
        for patch in bp["boxes"]:
            patch.set_facecolor("#d6e4f0")
            patch.set_edgecolor("#2c5784")
        for median in bp["medians"]:
            median.set_color("#a83232")
            median.set_linewidth(1.5)

        # Linha horizontal: Freitas Mean, por caixa (cinza tracejada).
        for idx, p in enumerate(p_values, start=1):
            fm = FREITAS_MEAN.get((group, p))
            if fm is None:
                continue
            ax.hlines(
                fm, idx - 0.35, idx + 0.35,
                colors="#888888", linestyles="--", linewidth=1.2,
                label="Freitas Mean" if idx == 1 else None,
            )
            bks = BKS.get((group, p))
            if bks is not None:
                ax.hlines(
                    bks, idx - 0.35, idx + 0.35,
                    colors="#2c8c3a", linestyles=":", linewidth=1.2,
                    label="BKS (Stefanello)" if idx == 1 else None,
                )

        ax.set_title(f"Grupo {group} — distribuicao do custo em {len(data[0])} runs")
        ax.set_xlabel("p (numero de medianas)")
        ax.set_ylabel("Custo (funcao objetivo)")
        ax.grid(True, axis="y", alpha=0.3, linestyle="--")
        # Remove duplicatas na legenda
        handles, labels = ax.get_legend_handles_labels()
        seen = set()
        unique = [(h, l) for h, l in zip(handles, labels) if not (l in seen or seen.add(l))]
        if unique:
            ax.legend([h for h, _ in unique], [l for _, l in unique],
                      loc="upper right", fontsize=8)

        fig.tight_layout()
        out = fig_dir / f"boxplot_{group}.pdf"
        fig.savefig(out)
        out_png = fig_dir / f"boxplot_{group}.png"
        fig.savefig(out_png, dpi=150)
        plt.close(fig)
        generated.append(out)

    return generated


# =============================================================================
# 8. RENDERIZADORES
# =============================================================================

def render_txt_table(title: str, cols: list[str], rows: list[list[str]]) -> str:
    widths = [len(c) for c in cols]
    for r in rows:
        for i, cell in enumerate(r):
            widths[i] = max(widths[i], len(cell))
    line = "=" * (sum(widths) + 2 * (len(widths) - 1))
    header = "  ".join(c.rjust(widths[i]) for i, c in enumerate(cols))
    out = [title, line, header, line]
    for r in rows:
        out.append("  ".join(cell.rjust(widths[i]) for i, cell in enumerate(r)))
    out.append(line)
    return "\n".join(out) + "\n"


def render_tex_table(caption: str, label: str, columns: list[str],
                     rows: list[list[str]], align: str) -> str:
    safe_cols = [c.replace("%", "\\%").replace("_", r"\_") for c in columns]
    body_lines = [
        r"\begin{table}[!htb]",
        r"\centering",
        r"\caption{" + caption + "}",
        r"\label{" + label + "}",
        r"\small",
        r"\begin{tabular}{" + align + "}",
        r"\hline",
        " & ".join(safe_cols) + r" \\",
        r"\hline",
    ]
    for r in rows:
        body_lines.append(" & ".join(cell for cell in r) + r" \\")
    body_lines.append(r"\hline")
    body_lines.append(r"\end{tabular}")
    body_lines.append(r"\end{table}")
    return "\n".join(body_lines) + "\n"


# =============================================================================
# 9. CSV CONSOLIDADO (resumo.csv)
# =============================================================================

def build_consolidated_csv(results, out_path: Path) -> None:
    """Resumo.csv com uma linha por (grupo, p): best, mean, std, cv, tempo
    e gaps contra BKS e Freitas."""
    fieldnames = [
        "instance", "group", "n_runs", "p",
        "hybrid_best", "hybrid_mean", "hybrid_std", "hybrid_cv_pct",
        "time_mean_s",
        "bks", "freitas_best", "freitas_mean",
        "gap_best_vs_bks_pct", "gap_mean_vs_bks_pct",
        "gap_best_vs_freitas_pct", "gap_mean_vs_freitas_pct",
    ]
    with open(out_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for key in sorted_keys(list(results.keys())):
            group, p = key
            s = compute_stats(results[key])
            bks = BKS.get(key)
            fb = FREITAS_BEST.get(key)
            fm = FREITAS_MEAN.get(key)
            w.writerow({
                "instance": f"{group}_{p}",
                "group": group,
                "n_runs": s["n"],
                "p": p,
                "hybrid_best": f"{s['best']:.4f}",
                "hybrid_mean": f"{s['mean']:.4f}",
                "hybrid_std": f"{s['std']:.4f}",
                "hybrid_cv_pct": f"{s['cv_pct']:.4f}",
                "time_mean_s": f"{s['time_mean']:.2f}",
                "bks": f"{bks:.4f}" if bks is not None else "",
                "freitas_best": f"{fb:.4f}" if fb is not None else "",
                "freitas_mean": f"{fm:.4f}" if fm is not None else "",
                "gap_best_vs_bks_pct":
                    f"{gap_pct(s['best'], bks):+.4f}" if bks else "",
                "gap_mean_vs_bks_pct":
                    f"{gap_pct(s['mean'], bks):+.4f}" if bks else "",
                "gap_best_vs_freitas_pct":
                    f"{gap_pct(s['best'], fb):+.4f}" if fb else "",
                "gap_mean_vs_freitas_pct":
                    f"{gap_pct(s['mean'], fm):+.4f}" if fm else "",
            })


# =============================================================================
# 10. TABELA AGREGADA POR FAMILIA
# =============================================================================

def build_table_family(results) -> tuple[str, str]:
    """Uma linha por familia: mediana (robusto) e media dos gaps, CV medio,
    wins contra Freitas, tempo total."""
    header = [
        "Familia", "#inst",
        "gap_best_vs_BKS (mean)", "gap_mean_vs_BKS (mean)",
        "CV% (mean)", "wins/losses vs Freitas_mean",
        "t_total (h)",
    ]
    rows_txt: list[list[str]] = []
    rows_tex: list[list[str]] = []

    for group in GROUPS_ORDER:
        keys = [k for k in results.keys() if k[0] == group]
        if not keys:
            continue
        gaps_best = []
        gaps_mean_bks = []
        cvs = []
        wins = 0
        losses = 0
        t_total = 0.0
        for key in keys:
            s = compute_stats(results[key])
            bks = BKS.get(key)
            fm = FREITAS_MEAN.get(key)
            if bks:
                gaps_best.append(gap_pct(s["best"], bks))
                gaps_mean_bks.append(gap_pct(s["mean"], bks))
            cvs.append(s["cv_pct"])
            if fm is not None:
                if s["mean"] < fm:
                    wins += 1
                elif s["mean"] > fm:
                    losses += 1
            t_total += s["time_mean"] * s["n"]

        def avg(x):
            return sum(x) / len(x) if x else 0.0

        row = [
            group, str(len(keys)),
            f"{avg(gaps_best):+.3f}", f"{avg(gaps_mean_bks):+.3f}",
            f"{avg(cvs):.3f}", f"{wins}/{losses}",
            f"{t_total/3600.0:.2f}",
        ]
        rows_txt.append(row)
        rows_tex.append(list(row))

    txt = render_txt_table(
        "Resumo por familia (medias dos gaps, CV%, wins/losses vs Freitas)",
        header, rows_txt,
    )
    tex = render_tex_table(
        caption="Resumo agregado por familia. Gap medio em relacao a "
                "BKS (Stefanello 2015). Wins/losses refere-se a Mean "
                "comparado a Freitas et al.~2025.",
        label="tab:family",
        columns=header,
        rows=rows_tex,
        align="lrrrrrr",
    )
    return txt, tex


# =============================================================================
# 11. BOXPLOT DE GAP% NORMALIZADO (1 figura, varios metodos)
# =============================================================================

def make_gap_boxplot(results, fig_dir: Path) -> Path:
    """Boxplot de gap% vs BKS para Hybrid (melhor e media) e Freitas
    (melhor e media), agregando todas as instancias em uma figura."""
    hybrid_best_gaps, hybrid_mean_gaps = [], []
    freitas_best_gaps, freitas_mean_gaps = [], []

    for key in sorted_keys(list(results.keys())):
        s = compute_stats(results[key])
        bks = BKS.get(key)
        if bks is None:
            continue
        hybrid_best_gaps.append(gap_pct(s["best"], bks))
        hybrid_mean_gaps.append(gap_pct(s["mean"], bks))
        fb = FREITAS_BEST.get(key)
        fm = FREITAS_MEAN.get(key)
        if fb:
            freitas_best_gaps.append(gap_pct(fb, bks))
        if fm:
            freitas_mean_gaps.append(gap_pct(fm, bks))

    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    data = [hybrid_best_gaps, freitas_best_gaps,
            hybrid_mean_gaps, freitas_mean_gaps]
    labels = ["Hybrid\nBest", "Freitas\nBest", "Hybrid\nMean", "Freitas\nMean"]
    colors = ["#2c5784", "#a83232", "#5b9bd5", "#e06666"]

    bp = ax.boxplot(
        data, tick_labels=labels,
        showmeans=True, meanline=True, patch_artist=True,
        widths=0.55,
    )
    for patch, c in zip(bp["boxes"], colors):
        patch.set_facecolor(c)
        patch.set_alpha(0.55)
        patch.set_edgecolor("black")
    for median in bp["medians"]:
        median.set_color("black")
        median.set_linewidth(1.6)

    ax.axhline(0.0, color="#2c8c3a", linestyle="--", linewidth=1.1,
               label="BKS (Stefanello 2015)")
    ax.set_ylabel("Gap (%) em relacao a BKS")
    ax.set_title(
        f"Distribuicao do gap vs BKS em {len(hybrid_best_gaps)} instancias"
    )
    ax.grid(True, axis="y", alpha=0.3, linestyle="--")
    ax.legend(loc="upper left", fontsize=9)

    fig.tight_layout()
    out = fig_dir / "boxplot_gap_pct.pdf"
    fig.savefig(out)
    fig.savefig(fig_dir / "boxplot_gap_pct.png", dpi=150)
    plt.close(fig)
    return out


# =============================================================================
# 12. PERFORMANCE PROFILE (Dolan & More 2002)
# =============================================================================

def make_performance_profile(results, fig_dir: Path,
                             metric: str = "mean") -> Path:
    """Performance profile: rho_s(tau) = fracao de instancias onde o metodo s
    esta dentro de fator tau do melhor. metric in {'best','mean'}."""
    series = {}  # method -> list of values (per instance, alinhado)
    for key in sorted_keys(list(results.keys())):
        s = compute_stats(results[key])
        if metric == "best":
            hybrid_val = s["best"]
            freitas_val = FREITAS_BEST.get(key)
        else:
            hybrid_val = s["mean"]
            freitas_val = FREITAS_MEAN.get(key)
        if freitas_val is None:
            continue
        series.setdefault("Hybrid-CS-AILS", []).append(hybrid_val)
        series.setdefault("Freitas 2025", []).append(freitas_val)

    if not series or len(next(iter(series.values()))) == 0:
        return None

    n_inst = len(series["Hybrid-CS-AILS"])
    # Para cada instancia i, ratio r_{s,i} = val_{s,i} / min_s val_{s,i}
    ratios = {s: [] for s in series}
    for i in range(n_inst):
        vals = {s: series[s][i] for s in series}
        best = min(vals.values())
        for s, v in vals.items():
            ratios[s].append(v / best if best > 0 else 1.0)

    # rho_s(tau) = |{i : r_{s,i} <= tau}| / n_inst
    tau_grid = np.linspace(1.0, 1.10, 300)  # ate 10% pior que o melhor
    fig, ax = plt.subplots(figsize=(7.0, 4.4))
    styles = {
        "Hybrid-CS-AILS": {"color": "#2c5784", "linestyle": "-", "linewidth": 2.0},
        "Freitas 2025":   {"color": "#a83232", "linestyle": "--", "linewidth": 2.0},
    }
    for s, rs in ratios.items():
        rs_arr = np.array(rs)
        rho = [float(np.mean(rs_arr <= t)) for t in tau_grid]
        ax.step(tau_grid, rho, where="post", label=s, **styles.get(s, {}))

    ax.set_xlabel(r"$\tau$ (fator relativo ao melhor da instancia)")
    ax.set_ylabel(r"$\rho_s(\tau)$ = fracao de instancias")
    ax.set_title(f"Performance Profile ({metric})")
    ax.set_xlim(1.0, 1.10)
    ax.set_ylim(0.0, 1.03)
    ax.grid(True, alpha=0.3, linestyle="--")
    ax.legend(loc="lower right", fontsize=10)

    fig.tight_layout()
    out = fig_dir / f"performance_profile_{metric}.pdf"
    fig.savefig(out)
    fig.savefig(fig_dir / f"performance_profile_{metric}.png", dpi=150)
    plt.close(fig)
    return out


# =============================================================================
# 13. SCATTER: GANHO vs p/n
# =============================================================================

def make_gain_vs_pn(results, fig_dir: Path) -> Path | None:
    """Scatter de (Hybrid_mean - Freitas_mean) em gap% contra p/n.
    Mostra onde Hybrid vence / perde."""
    xs, ys, labels = [], [], []
    for key in sorted_keys(list(results.keys())):
        group, p = key
        n_map = {"lin318": 318, "ali535": 535, "u724": 724, "rl1304": 1304,
                 "pr2392": 2392, "fnl4461": 4461, "p3038": 3038}
        n = n_map.get(group)
        s = compute_stats(results[key])
        fm = FREITAS_MEAN.get(key)
        if fm is None or n is None:
            continue
        gap_h = gap_pct(s["mean"], fm)
        xs.append(p / n)
        ys.append(gap_h)
        labels.append(f"{group}_{p}")

    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    colors_by_group = {
        "lin318":  "#1f77b4", "ali535":  "#ff7f0e", "u724":    "#2ca02c",
        "rl1304":  "#d62728", "pr2392":  "#9467bd", "fnl4461": "#8c564b",
        "p3038":   "#e377c2",
    }
    for x, y, lb in zip(xs, ys, labels):
        group = lb.split("_")[0]
        ax.scatter(x, y, color=colors_by_group.get(group, "black"),
                   alpha=0.75, s=55, edgecolors="black", linewidths=0.6)

    ax.axhline(0.0, color="black", linestyle="-", linewidth=0.8)
    ax.fill_between([0, 1], 0, ax.get_ylim()[0] if ax.get_ylim()[0] < 0 else -5,
                    color="#2c8c3a", alpha=0.08, label="Hybrid melhor")
    ax.set_xlabel("p / n (densidade de medianas)")
    ax.set_ylabel("Gap (%) de Hybrid Mean vs Freitas Mean")
    ax.set_title("Ganho de Hybrid-CS-AILS sobre Freitas 2025 por densidade p/n")
    ax.grid(True, alpha=0.3, linestyle="--")

    # Legenda por familia
    from matplotlib.patches import Patch
    handles = [Patch(color=c, label=g) for g, c in colors_by_group.items()
               if g in {lb.split("_")[0] for lb in labels}]
    ax.legend(handles=handles, loc="best", fontsize=8, ncol=2)

    fig.tight_layout()
    out = fig_dir / "gain_vs_pn.pdf"
    fig.savefig(out)
    fig.savefig(fig_dir / "gain_vs_pn.png", dpi=150)
    plt.close(fig)
    return out


# =============================================================================
# 14. WILCOXON SOBRE GAP% (uma cauda)
# =============================================================================

def run_wilcoxon_gap(results) -> str:
    """Wilcoxon signed-rank pareado, uma cauda (H1: Hybrid < Freitas),
    aplicado sobre gap% vs BKS (em vez de custo absoluto). Mais apropriado
    para amostras com escalas heterogeneas."""
    lines = [
        "Teste de Wilcoxon signed-rank PAREADO em gap% vs BKS",
        "H0: mediana(gap_H - gap_F) = 0",
        "H1: gap_H < gap_F  (Hybrid tem gap menor que Freitas)  -> uma cauda",
        "=" * 64,
    ]

    def collect(metric_hybrid, metric_freitas_dict):
        out = []
        for key in sorted_keys(list(results.keys())):
            s = compute_stats(results[key])
            bks = BKS.get(key)
            fref = metric_freitas_dict.get(key)
            if bks is None or fref is None:
                continue
            hybrid_val = s[metric_hybrid]
            gap_h = gap_pct(hybrid_val, bks)
            gap_f = gap_pct(fref, bks)
            out.append((gap_h, gap_f, key))
        return out

    def report(label, sample):
        if len(sample) < 6:
            lines.append(f"\n[{label}] amostra {len(sample)} pequena demais.")
            return
        gh = np.array([s[0] for s in sample])
        gf = np.array([s[1] for s in sample])
        diff = gh - gf
        try:
            # one-sided: alternative='less' testa H1: gh < gf
            stat, pval = scipy_stats.wilcoxon(
                gh, gf, zero_method="wilcox", alternative="less"
            )
        except ValueError as exc:
            lines.append(f"\n[{label}] falha: {exc}")
            return
        n = len(diff)
        mean_w = n * (n + 1) / 4.0
        std_w = math.sqrt(n * (n + 1) * (2 * n + 1) / 24.0)
        z = (stat - mean_w) / std_w if std_w > 0 else float("nan")
        r = abs(z) / math.sqrt(n) if n > 0 else float("nan")
        n_better = int(np.sum(diff < 0))
        n_worse = int(np.sum(diff > 0))
        n_tie = int(np.sum(diff == 0))
        lines.append(f"\n[{label}]  N={n}")
        lines.append(f"  Hybrid com gap MENOR: {n_better}   "
                     f"MAIOR: {n_worse}   empate: {n_tie}")
        lines.append(f"  mediana(gap_H - gap_F) = {np.median(diff):+.4f}%")
        lines.append(f"  W={stat:.2f}  Z~={z:.3f}  p(one-sided)={pval:.5f}  "
                     f"|r|={r:.3f}")
        if pval < 0.001:
            verdict = "p < 0.001 -> Hybrid estritamente melhor (altamente sig.)"
        elif pval < 0.01:
            verdict = "p < 0.01  -> Hybrid estritamente melhor (sig.)"
        elif pval < 0.05:
            verdict = "p < 0.05  -> Hybrid estritamente melhor (sig.)"
        else:
            verdict = f"p = {pval:.3f}   -> sem evidencia (teste nao rejeita H0)"
        lines.append(f"  {verdict}")

    report("Best (gap% vs BKS)  — todas as instancias",
           collect("best", FREITAS_BEST))
    report("Mean (gap% vs BKS)  — todas as instancias",
           collect("mean", FREITAS_MEAN))
    # Excluindo p3038
    def without_p3038(sample):
        return [s for s in sample if s[2][0] != "p3038"]
    report("Best (gap% vs BKS)  — excluindo p3038",
           without_p3038(collect("best", FREITAS_BEST)))
    report("Mean (gap% vs BKS)  — excluindo p3038",
           without_p3038(collect("mean", FREITAS_MEAN)))

    return "\n".join(lines) + "\n"


# =============================================================================
# 15. TABELA DE SPEEDUP POR FAMILIA
# =============================================================================

def _geo_mean(vals: list[float]) -> float:
    if not vals:
        return float("nan")
    return float(np.exp(np.mean(np.log(vals))))


def build_speedup_table(results) -> tuple[str, str]:
    """Speedup medio geometrico de Hybrid sobre cada baseline, por familia.
    Uso da media geometrica em vez de aritmetica para razoes."""
    header = [
        "Familia", "#inst",
        "T_Hybrid (s)", "T_ILS (s)", "T_IRMA (s)", "T_GVNS (s)",
        "speedup_ILS", "speedup_IRMA", "speedup_GVNS",
    ]
    rows_txt: list[list[str]] = []
    rows_tex: list[list[str]] = []

    for group in GROUPS_ORDER:
        keys = [k for k in results.keys() if k[0] == group]
        if not keys:
            continue
        t_hybrid_list, t_ils_list, t_irma_list, t_gvns_list = [], [], [], []
        sp_ils, sp_irma, sp_gvns = [], [], []
        for key in keys:
            s = compute_stats(results[key])
            th = s["time_mean"]
            t_hybrid_list.append(th)
            t_ils = T_ILS.get(key)
            t_irma = T_IRMA.get(key)
            t_gvns = T_GVNS.get(key)
            if t_ils:
                t_ils_list.append(t_ils)
                sp_ils.append(t_ils / th if th > 0 else float("nan"))
            if t_irma:
                t_irma_list.append(t_irma)
                sp_irma.append(t_irma / th if th > 0 else float("nan"))
            if t_gvns:
                t_gvns_list.append(t_gvns)
                sp_gvns.append(t_gvns / th if th > 0 else float("nan"))

        row = [
            group, str(len(keys)),
            f"{statistics.fmean(t_hybrid_list):.0f}",
            f"{statistics.fmean(t_ils_list):.0f}" if t_ils_list else "-",
            f"{statistics.fmean(t_irma_list):.0f}" if t_irma_list else "-",
            f"{statistics.fmean(t_gvns_list):.0f}" if t_gvns_list else "-",
            f"{_geo_mean(sp_ils):.1f}x" if sp_ils else "-",
            f"{_geo_mean(sp_irma):.1f}x" if sp_irma else "-",
            f"{_geo_mean(sp_gvns):.1f}x" if sp_gvns else "-",
        ]
        rows_txt.append(row)
        rows_tex.append(list(row))

    # Linha "TOTAL" com media geometrica global
    all_sp_ils, all_sp_irma, all_sp_gvns = [], [], []
    t_h_total, t_ils_total, t_irma_total, t_gvns_total = [], [], [], []
    for key in sorted_keys(list(results.keys())):
        s = compute_stats(results[key])
        th = s["time_mean"]
        t_h_total.append(th)
        if key in T_ILS:
            t_ils_total.append(T_ILS[key])
            all_sp_ils.append(T_ILS[key] / th)
        if key in T_IRMA:
            t_irma_total.append(T_IRMA[key])
            all_sp_irma.append(T_IRMA[key] / th)
        if key in T_GVNS:
            t_gvns_total.append(T_GVNS[key])
            all_sp_gvns.append(T_GVNS[key] / th)

    total_row = [
        "GLOBAL", str(len(t_h_total)),
        f"{statistics.fmean(t_h_total):.0f}",
        f"{statistics.fmean(t_ils_total):.0f}",
        f"{statistics.fmean(t_irma_total):.0f}",
        f"{statistics.fmean(t_gvns_total):.0f}",
        f"{_geo_mean(all_sp_ils):.1f}x",
        f"{_geo_mean(all_sp_irma):.1f}x",
        f"{_geo_mean(all_sp_gvns):.1f}x",
    ]
    rows_txt.append(total_row)
    rows_tex.append(list(total_row))

    txt = render_txt_table(
        "Tempo medio e speedup (media geometrica) de Hybrid sobre cada baseline",
        header, rows_txt,
    )
    tex = render_tex_table(
        caption="Tempo medio por familia e speedup (media geometrica) "
                "do Hybrid-CS-AILS em relacao a ILS 2025, IRMA 2015 e GVNS 2023.",
        label="tab:speedup",
        columns=header,
        rows=rows_tex,
        align="lrrrrrrrr",
    )
    return txt, tex


# =============================================================================
# 16. MATRIZ DE WINS/TIES/LOSSES (4x4)
# =============================================================================

def build_wins_matrix(results) -> str:
    methods = ["Hybrid", "ILS", "IRMA", "GVNS"]

    def best_of(key, method):
        if method == "Hybrid":
            s = compute_stats(results[key])
            return s["best"]
        if method == "ILS":
            return FREITAS_BEST.get(key)
        if method == "IRMA":
            return STEFANELLO_BEST.get(key)
        if method == "GVNS":
            return VASCONCELOS_BEST.get(key)
        return None

    # Matriz win[i][j] = # instancias onde method i bate method j (strict)
    n = len(methods)
    wins = [[0] * n for _ in range(n)]
    ties = [[0] * n for _ in range(n)]
    total = [[0] * n for _ in range(n)]
    for key in sorted_keys(list(results.keys())):
        for i, mi in enumerate(methods):
            for j, mj in enumerate(methods):
                if i == j:
                    continue
                vi, vj = best_of(key, mi), best_of(key, mj)
                if vi is None or vj is None:
                    continue
                total[i][j] += 1
                if vi < vj:
                    wins[i][j] += 1
                elif vi == vj:
                    ties[i][j] += 1

    lines = [
        "Matriz de comparacao pareada (best-of-run) — linha bate coluna",
        "Formato: wins / ties / total (linha venceu estritamente <= total)",
        "=" * 72,
    ]
    header = "        " + "  ".join(m.rjust(12) for m in methods)
    lines.append(header)
    lines.append("-" * 72)
    for i, mi in enumerate(methods):
        cells = []
        for j, mj in enumerate(methods):
            if i == j:
                cells.append("         -  ")
            else:
                cells.append(f"{wins[i][j]:>3}/{ties[i][j]:>2}/{total[i][j]:<3}".rjust(12))
        lines.append(f"{mi:<8}" + "  ".join(cells))
    lines.append("=" * 72)

    # Taxa de vitoria agregada (quanto vence dos outros 3 juntos)
    lines.append("\nTaxa de vitoria agregada (wins / n instancias):")
    for i, mi in enumerate(methods):
        total_wins = sum(wins[i][j] for j in range(n) if j != i)
        total_comparisons = sum(total[i][j] for j in range(n) if j != i)
        if total_comparisons:
            pct = 100.0 * total_wins / total_comparisons
            lines.append(f"  {mi:<8}  {total_wins:>3} / {total_comparisons:>3}   ({pct:5.1f}%)")

    return "\n".join(lines) + "\n"


# =============================================================================
# 17. SCATTER TIME x QUALITY (log-log Pareto)
# =============================================================================

def make_time_quality_scatter(results, fig_dir: Path) -> Path:
    """Scatter log-log de tempo medio vs gap% medio vs BKS, um ponto por
    (metodo, instancia). Visualiza o Pareto front qualidade x tempo."""
    # Coleta (tempo, gap%) por metodo
    data = {
        "Hybrid-CS-AILS": {"t": [], "g": []},
        "ILS 2025":       {"t": [], "g": []},
        "IRMA 2015":      {"t": [], "g": []},
        "GVNS 2023":      {"t": [], "g": []},
    }

    for key in sorted_keys(list(results.keys())):
        bks = BKS.get(key)
        if bks is None:
            continue
        s = compute_stats(results[key])
        # Hybrid: usar mean para representar a "qualidade tipica"
        data["Hybrid-CS-AILS"]["t"].append(s["time_mean"])
        data["Hybrid-CS-AILS"]["g"].append(gap_pct(s["mean"], bks))

        if key in FREITAS_MEAN and key in T_ILS:
            data["ILS 2025"]["t"].append(T_ILS[key])
            data["ILS 2025"]["g"].append(gap_pct(FREITAS_MEAN[key], bks))
        if key in STEFANELLO_MEAN and key in T_IRMA:
            data["IRMA 2015"]["t"].append(T_IRMA[key])
            data["IRMA 2015"]["g"].append(gap_pct(STEFANELLO_MEAN[key], bks))
        if key in VASCONCELOS_MEAN and key in T_GVNS:
            data["GVNS 2023"]["t"].append(T_GVNS[key])
            data["GVNS 2023"]["g"].append(gap_pct(VASCONCELOS_MEAN[key], bks))

    fig, ax = plt.subplots(figsize=(7.5, 5.0))
    styles = {
        "Hybrid-CS-AILS": {"c": "#2c5784", "marker": "o", "s": 65},
        "ILS 2025":       {"c": "#a83232", "marker": "s", "s": 55},
        "IRMA 2015":      {"c": "#2c8c3a", "marker": "^", "s": 55},
        "GVNS 2023":      {"c": "#d48820", "marker": "D", "s": 55},
    }

    for method, pts in data.items():
        if not pts["t"]:
            continue
        ax.scatter(
            pts["t"], pts["g"],
            label=method,
            color=styles[method]["c"],
            marker=styles[method]["marker"],
            s=styles[method]["s"],
            alpha=0.75,
            edgecolors="black", linewidths=0.5,
        )

    ax.set_xscale("log")
    ax.set_xlabel("Tempo medio (segundos, escala log)")
    ax.set_ylabel("Gap (%) em relacao a BKS (mean)")
    ax.set_title("Trade-off qualidade x tempo — cada ponto = 1 instancia")
    ax.grid(True, alpha=0.3, linestyle="--", which="both")
    ax.legend(loc="upper right", fontsize=9)
    ax.axhline(0.0, color="gray", linewidth=0.8, linestyle=":")

    fig.tight_layout()
    out = fig_dir / "time_quality_scatter.pdf"
    fig.savefig(out)
    fig.savefig(fig_dir / "time_quality_scatter.png", dpi=150)
    plt.close(fig)
    return out


# =============================================================================
# 18. PERFORMANCE PROFILE COM 4 METODOS
# =============================================================================

def make_performance_profile_4way(results, fig_dir: Path,
                                   metric: str = "mean") -> Path:
    """Performance profile com Hybrid, ILS, IRMA, GVNS."""
    method_best = {
        "Hybrid-CS-AILS": lambda key, s: s["best"],
        "ILS 2025":       lambda key, s: FREITAS_BEST.get(key),
        "IRMA 2015":      lambda key, s: STEFANELLO_BEST.get(key),
        "GVNS 2023":      lambda key, s: VASCONCELOS_BEST.get(key),
    }
    method_mean = {
        "Hybrid-CS-AILS": lambda key, s: s["mean"],
        "ILS 2025":       lambda key, s: FREITAS_MEAN.get(key),
        "IRMA 2015":      lambda key, s: STEFANELLO_MEAN.get(key),
        "GVNS 2023":      lambda key, s: VASCONCELOS_MEAN.get(key),
    }
    fns = method_best if metric == "best" else method_mean

    # Coleta apenas instancias onde TODOS os 4 metodos tem valor.
    valid_keys = []
    all_series = {m: [] for m in fns}
    for key in sorted_keys(list(results.keys())):
        s = compute_stats(results[key])
        vals = {m: fn(key, s) for m, fn in fns.items()}
        if any(v is None for v in vals.values()):
            continue
        valid_keys.append(key)
        for m, v in vals.items():
            all_series[m].append(v)

    if not valid_keys:
        return None

    n_inst = len(valid_keys)
    ratios = {m: [] for m in fns}
    for i in range(n_inst):
        vals = {m: all_series[m][i] for m in fns}
        best = min(vals.values())
        for m, v in vals.items():
            ratios[m].append(v / best if best > 0 else 1.0)

    tau_grid = np.linspace(1.0, 1.15, 400)
    fig, ax = plt.subplots(figsize=(7.5, 4.8))
    styles = {
        "Hybrid-CS-AILS": {"color": "#2c5784", "linestyle": "-",  "linewidth": 2.2},
        "ILS 2025":       {"color": "#a83232", "linestyle": "--", "linewidth": 1.8},
        "IRMA 2015":      {"color": "#2c8c3a", "linestyle": "-.", "linewidth": 1.8},
        "GVNS 2023":      {"color": "#d48820", "linestyle": ":",  "linewidth": 2.0},
    }
    for m, rs in ratios.items():
        rs_arr = np.array(rs)
        rho = [float(np.mean(rs_arr <= t)) for t in tau_grid]
        ax.step(tau_grid, rho, where="post", label=m, **styles[m])

    title_suffix = "Best" if metric == "best" else "Mean"
    ax.set_xlabel(r"$\tau$ (fator relativo ao melhor da instancia)")
    ax.set_ylabel(r"$\rho_s(\tau)$ = fracao de instancias")
    ax.set_title(f"Performance Profile ({title_suffix}) — {n_inst} instancias")
    ax.set_xlim(1.0, 1.15)
    ax.set_ylim(0.0, 1.03)
    ax.grid(True, alpha=0.3, linestyle="--")
    ax.legend(loc="lower right", fontsize=9)

    fig.tight_layout()
    out = fig_dir / f"performance_profile_4way_{metric}.pdf"
    fig.savefig(out)
    fig.savefig(fig_dir / f"performance_profile_4way_{metric}.png", dpi=150)
    plt.close(fig)
    return out


# =============================================================================
# 19. MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", help="CSV de resultados (formato final_*.csv)")
    parser.add_argument("--out", default=None,
                        help="Diretorio de saida (default: <csv>.artifacts/)")
    args = parser.parse_args()

    csv_path = Path(args.csv).resolve()
    if not csv_path.exists():
        print(f"Erro: arquivo nao encontrado: {csv_path}", file=sys.stderr)
        sys.exit(1)

    out_dir = Path(args.out) if args.out else csv_path.with_suffix("").parent / (
        csv_path.stem + ".artifacts"
    )
    tables_dir = out_dir / "tables"
    figures_dir = out_dir / "figures"
    tables_dir.mkdir(parents=True, exist_ok=True)
    figures_dir.mkdir(parents=True, exist_ok=True)

    results = load_csv(str(csv_path))
    n_keys = len(results)
    n_runs = sum(len(v) for v in results.values())
    print(f"Carregado: {n_keys} instancias, {n_runs} runs  ({csv_path.name})")

    # CSV consolidado (a base de tudo, pedido explicito do usuario)
    resumo_csv = out_dir / "resumo.csv"
    build_consolidated_csv(results, resumo_csv)

    t2_txt, t2_tex = build_table_t2(results)
    (tables_dir / "T2_best.txt").write_text(t2_txt)
    (tables_dir / "T2_best.tex").write_text(t2_tex)

    t3_txt, t3_tex = build_table_t3(results)
    (tables_dir / "T3_mean.txt").write_text(t3_txt)
    (tables_dir / "T3_mean.tex").write_text(t3_tex)

    tf_txt, tf_tex = build_table_family(results)
    (tables_dir / "T_family.txt").write_text(tf_txt)
    (tables_dir / "T_family.tex").write_text(tf_tex)

    wilcoxon_txt = run_wilcoxon(results)
    (tables_dir / "wilcoxon.txt").write_text(wilcoxon_txt)

    wilcoxon_gap_txt = run_wilcoxon_gap(results)
    (tables_dir / "wilcoxon_gap.txt").write_text(wilcoxon_gap_txt)

    audit_txt = run_audit(results)
    (tables_dir / "audit.txt").write_text(audit_txt)

    box_files = make_boxplots(results, figures_dir)
    gap_box = make_gap_boxplot(results, figures_dir)
    pp_best = make_performance_profile(results, figures_dir, metric="best")
    pp_mean = make_performance_profile(results, figures_dir, metric="mean")
    gain_fig = make_gain_vs_pn(results, figures_dir)

    # Novos artefatos baseados nos dados completos da literatura
    speedup_txt, speedup_tex = build_speedup_table(results)
    (tables_dir / "T_speedup.txt").write_text(speedup_txt)
    (tables_dir / "T_speedup.tex").write_text(speedup_tex)

    wins_txt = build_wins_matrix(results)
    (tables_dir / "wins_matrix.txt").write_text(wins_txt)

    pp4_mean = make_performance_profile_4way(results, figures_dir, metric="mean")
    pp4_best = make_performance_profile_4way(results, figures_dir, metric="best")
    tq_fig = make_time_quality_scatter(results, figures_dir)

    print("\nArtefatos gerados:")
    print(f"  {resumo_csv}")
    print(f"  {tables_dir/'T2_best.txt'}")
    print(f"  {tables_dir/'T2_best.tex'}")
    print(f"  {tables_dir/'T3_mean.txt'}")
    print(f"  {tables_dir/'T3_mean.tex'}")
    print(f"  {tables_dir/'T_family.txt'}")
    print(f"  {tables_dir/'T_family.tex'}")
    print(f"  {tables_dir/'wilcoxon.txt'}")
    print(f"  {tables_dir/'wilcoxon_gap.txt'}")
    print(f"  {tables_dir/'audit.txt'}")
    for path in box_files:
        print(f"  {path}")
    if gap_box: print(f"  {gap_box}")
    if pp_best: print(f"  {pp_best}")
    if pp_mean: print(f"  {pp_mean}")
    if gain_fig: print(f"  {gain_fig}")
    print(f"  {tables_dir/'T_speedup.txt'}")
    print(f"  {tables_dir/'wins_matrix.txt'}")
    if pp4_mean: print(f"  {pp4_mean}")
    if pp4_best: print(f"  {pp4_best}")
    if tq_fig: print(f"  {tq_fig}")

    print("\nTabela de speedup:")
    print(speedup_txt)
    print("\nMatriz de wins:")
    print(wins_txt)
    print("\nWilcoxon em gap% (teste recomendado):")
    print(wilcoxon_gap_txt)
    print("\nResumo por familia:")
    print(tf_txt)


if __name__ == "__main__":
    main()
