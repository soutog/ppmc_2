#!/usr/bin/env python3
"""
Gera as 4 figuras do paper de forma isolada, para edicao facil dos labels.

Uso:
    python figures_standalone.py <arquivo_csv> [--out DIR]

Labels, titulos, cores e dimensoes ficam concentrados em LABELS/STYLE no topo,
para trocar o texto sem mexer na logica.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

# Reaproveita dados de literatura e utilidades do script principal.
from make_publication_tables import (
    BKS, FREITAS_BEST, FREITAS_MEAN,
    load_csv, compute_stats, gap_pct, sorted_keys,
)


# =============================================================================
# LABELS — edite aqui para mudar o texto exibido nas figuras
# =============================================================================

LABELS = {
    # --- Figura 1: boxplot de gap% ---
    "box_title":      "Distribuicao do gap vs BKS em {n} instancias",
    "box_ylabel":     "Gap (%) em relacao a BKS",
    "box_bks_legend": "BKS (Stefanello 2015)",
    "box_x_tick_1":   "Hybrid\nBest",
    "box_x_tick_2":   "Freitas\nBest",
    "box_x_tick_3":   "Hybrid\nMean",
    "box_x_tick_4":   "Freitas\nMean",

    # --- Figura 2 e 3: performance profile ---
    "pp_title_best":  "Performance Profile (Best)",
    "pp_title_mean":  "Performance Profile (Mean)",
    "pp_xlabel":      r"$\tau$ (fator relativo ao melhor da instancia)",
    "pp_ylabel":      r"$\rho_s(\tau)$ = fracao de instancias",
    "pp_method_a":    "Hybrid-CS-AILS",
    "pp_method_b":    "Freitas 2025",

    # --- Figura 4: scatter ganho vs p/n ---
    "sc_title":       "Ganho de Hybrid-CS-AILS sobre Freitas 2025 por densidade p/n",
    "sc_xlabel":      "p / n (densidade de medianas)",
    "sc_ylabel":      "Gap (%) de Hybrid Mean vs Freitas Mean",
}

STYLE = {
    # cores dos 4 boxes (ordem: HybridBest, FreitasBest, HybridMean, FreitasMean)
    "box_colors": ["#2c5784", "#a83232", "#5b9bd5", "#e06666"],
    "box_bks_color": "#2c8c3a",

    # performance profile
    "pp_color_a": "#2c5784",   # Hybrid
    "pp_color_b": "#a83232",   # Freitas
    "pp_style_a": "-",
    "pp_style_b": "--",

    # scatter: uma cor por familia
    "family_colors": {
        "lin318":  "#1f77b4", "ali535":  "#ff7f0e", "u724":    "#2ca02c",
        "rl1304":  "#d62728", "pr2392":  "#9467bd", "fnl4461": "#8c564b",
        "p3038":   "#e377c2",
    },

    # tamanhos em polegadas (largura, altura)
    "box_figsize":    (7.2, 4.6),
    "pp_figsize":     (7.0, 4.4),
    "sc_figsize":     (7.2, 4.6),

    # grade e transparencia
    "grid_alpha":     0.3,
}

# Tamanho amostral esperado de n para cada familia (para eixo p/n do scatter).
GROUP_N = {
    "lin318": 318, "ali535": 535, "u724": 724, "rl1304": 1304,
    "pr2392": 2392, "fnl4461": 4461, "p3038": 3038,
}


# =============================================================================
# FIGURA 1 — Boxplot de gap% (4 caixas, 1 figura)
# =============================================================================

def make_gap_boxplot(results, fig_dir: Path) -> Path:
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

    fig, ax = plt.subplots(figsize=STYLE["box_figsize"])
    data = [hybrid_best_gaps, freitas_best_gaps,
            hybrid_mean_gaps, freitas_mean_gaps]
    labels = [LABELS["box_x_tick_1"], LABELS["box_x_tick_2"],
              LABELS["box_x_tick_3"], LABELS["box_x_tick_4"]]

    bp = ax.boxplot(
        data, tick_labels=labels,
        showmeans=True, meanline=True, patch_artist=True,
        widths=0.55,
    )
    for patch, c in zip(bp["boxes"], STYLE["box_colors"]):
        patch.set_facecolor(c)
        patch.set_alpha(0.55)
        patch.set_edgecolor("black")
    for median in bp["medians"]:
        median.set_color("black")
        median.set_linewidth(1.6)

    ax.axhline(0.0, color=STYLE["box_bks_color"], linestyle="--",
               linewidth=1.1, label=LABELS["box_bks_legend"])
    ax.set_ylabel(LABELS["box_ylabel"])
    ax.set_title(LABELS["box_title"].format(n=len(hybrid_best_gaps)))
    ax.grid(True, axis="y", alpha=STYLE["grid_alpha"], linestyle="--")
    ax.legend(loc="upper left", fontsize=9)

    fig.tight_layout()
    out = fig_dir / "boxplot_gap_pct.pdf"
    fig.savefig(out)
    fig.savefig(fig_dir / "boxplot_gap_pct.png", dpi=150)
    plt.close(fig)
    return out


# =============================================================================
# FIGURA 2 e 3 — Performance Profile
# =============================================================================

def make_performance_profile(results, fig_dir: Path, metric: str) -> Path:
    """metric in {'best','mean'}"""
    assert metric in {"best", "mean"}
    series: dict[str, list[float]] = {}

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
        series.setdefault(LABELS["pp_method_a"], []).append(hybrid_val)
        series.setdefault(LABELS["pp_method_b"], []).append(freitas_val)

    if not series:
        return None

    n_inst = len(next(iter(series.values())))
    ratios = {s: [] for s in series}
    for i in range(n_inst):
        vals = {s: series[s][i] for s in series}
        best = min(vals.values())
        for s, v in vals.items():
            ratios[s].append(v / best if best > 0 else 1.0)

    tau_grid = np.linspace(1.0, 1.10, 300)
    fig, ax = plt.subplots(figsize=STYLE["pp_figsize"])
    styles = {
        LABELS["pp_method_a"]: {"color": STYLE["pp_color_a"],
                                 "linestyle": STYLE["pp_style_a"],
                                 "linewidth": 2.0},
        LABELS["pp_method_b"]: {"color": STYLE["pp_color_b"],
                                 "linestyle": STYLE["pp_style_b"],
                                 "linewidth": 2.0},
    }
    for s, rs in ratios.items():
        rs_arr = np.array(rs)
        rho = [float(np.mean(rs_arr <= t)) for t in tau_grid]
        ax.step(tau_grid, rho, where="post", label=s, **styles.get(s, {}))

    title = LABELS["pp_title_best"] if metric == "best" else LABELS["pp_title_mean"]
    ax.set_xlabel(LABELS["pp_xlabel"])
    ax.set_ylabel(LABELS["pp_ylabel"])
    ax.set_title(title)
    ax.set_xlim(1.0, 1.10)
    ax.set_ylim(0.0, 1.03)
    ax.grid(True, alpha=STYLE["grid_alpha"], linestyle="--")
    ax.legend(loc="lower right", fontsize=10)

    fig.tight_layout()
    out = fig_dir / f"performance_profile_{metric}.pdf"
    fig.savefig(out)
    fig.savefig(fig_dir / f"performance_profile_{metric}.png", dpi=150)
    plt.close(fig)
    return out


# =============================================================================
# FIGURA 4 — Scatter de ganho vs p/n
# =============================================================================

def make_gain_vs_pn(results, fig_dir: Path) -> Path | None:
    xs, ys, labels = [], [], []
    for key in sorted_keys(list(results.keys())):
        group, p = key
        n = GROUP_N.get(group)
        s = compute_stats(results[key])
        fm = FREITAS_MEAN.get(key)
        if fm is None or n is None:
            continue
        gap_h = gap_pct(s["mean"], fm)
        xs.append(p / n)
        ys.append(gap_h)
        labels.append(f"{group}_{p}")

    if not xs:
        return None

    fig, ax = plt.subplots(figsize=STYLE["sc_figsize"])
    for x, y, lb in zip(xs, ys, labels):
        group = lb.split("_")[0]
        ax.scatter(x, y, color=STYLE["family_colors"].get(group, "black"),
                   alpha=0.75, s=55, edgecolors="black", linewidths=0.6)

    ax.axhline(0.0, color="black", linestyle="-", linewidth=0.8)
    # faixa verde abaixo de 0 = regiao onde Hybrid vence
    y_min = min(ys + [0.0]) - 0.5
    ax.fill_between([0, 1], 0, y_min, color="#2c8c3a", alpha=0.08)
    ax.set_xlabel(LABELS["sc_xlabel"])
    ax.set_ylabel(LABELS["sc_ylabel"])
    ax.set_title(LABELS["sc_title"])
    ax.grid(True, alpha=STYLE["grid_alpha"], linestyle="--")

    handles = [Patch(color=c, label=g)
               for g, c in STYLE["family_colors"].items()
               if g in {lb.split("_")[0] for lb in labels}]
    ax.legend(handles=handles, loc="best", fontsize=8, ncol=2)

    fig.tight_layout()
    out = fig_dir / "gain_vs_pn.pdf"
    fig.savefig(out)
    fig.savefig(fig_dir / "gain_vs_pn.png", dpi=150)
    plt.close(fig)
    return out


# =============================================================================
# MAIN
# =============================================================================

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="CSV de resultados (formato final_*.csv)")
    ap.add_argument("--out", default=None,
                    help="Diretorio de saida (default: <csv>.figures/)")
    args = ap.parse_args()

    csv_path = Path(args.csv).resolve()
    out_dir = Path(args.out) if args.out else (
        csv_path.with_suffix("").parent / (csv_path.stem + ".figures")
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    results = load_csv(str(csv_path))
    f1 = make_gap_boxplot(results, out_dir)
    f2 = make_performance_profile(results, out_dir, metric="best")
    f3 = make_performance_profile(results, out_dir, metric="mean")
    f4 = make_gain_vs_pn(results, out_dir)

    print("Figuras geradas:")
    for p in [f1, f2, f3, f4]:
        if p:
            print(f"  {p}")


if __name__ == "__main__":
    main()
