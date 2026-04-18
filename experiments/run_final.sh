#!/bin/bash
# ==========================================================================
# Experimento final: todas as 35 instancias x 10 seeds.
# Roda em rodadas: seed 42 para TODAS, depois seed 43, etc.
# Salva CSV detalhado (1 linha por run) + sumario com gaps vs paper (SBPO 2025).
#
# Uso:
#   make && ./experiments/run_final.sh
#
# Para retomar de onde parou (ex: seed 44):
#   SEED_START=44 ./experiments/run_final.sh
# ==========================================================================

set -u

BIN="./bin/main"
INSTANCE_DIR="./instances"
RESULT_DIR="./experiments/results"
ALPHA=0.6667
CONSTRUCTION_MAX_TRIES=1000
# NumIterMax e time_limit sao adaptativos dentro do binario.

NUM_SEEDS=10
SEED_BASE=${SEED_START:-42}
RUN_TIMEOUT=7200s

mkdir -p "$RESULT_DIR"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
CSV_FILE="$RESULT_DIR/final_${TIMESTAMP}.csv"
SUMMARY_FILE="$RESULT_DIR/final_${TIMESTAMP}_summary.txt"
LOG_FILE="$RESULT_DIR/final_${TIMESTAMP}.log"

# ---- CSV header ----
echo "instance,group,n,p,seed,grasp_cost,vnd_cost,final_cost,ils_iterations,ils_improvements,ils_stop_reason,cs_po_triggers,cs_po_improvements,cs_po_total_gain,cs_ils_promotions,time_grasp_s,time_vnd_s,time_ils_s,time_total_s,status" > "$CSV_FILE"

# ---- Referencia do paper (Tabelas 2, 3 e 4 — SBPO 2025 Freitas/Souza/Sa) ----
# Formato: KEY="best mean time_ils"
# As 30 runs do paper usaram i5-7200U 2.50GHz, 20GB RAM, NumIterMax=20, alpha=0.6
declare -A PAPER
PAPER["lin318_5"]="180313.09 181153.09 385.96"
PAPER["lin318_15"]="89041.59 89205.59 508.87"
PAPER["lin318_40"]="48091.32 48129.93 840.39"
PAPER["lin318_70"]="32291.75 32405.49 537.69"
PAPER["lin318_100"]="23041.87 23208.57 754.01"
PAPER["ali535_5"]="10033.04 10356.15 2588.65"
PAPER["ali535_25"]="3702.10 3826.45 2596.13"
PAPER["ali535_50"]="2503.34 2539.27 2887.95"
PAPER["ali535_100"]="1462.58 1592.58 2147.16"
PAPER["ali535_150"]="1036.85 1078.35 2546.97"
PAPER["u724_10"]="152019.24 152977.32 5183.34"
PAPER["u724_30"]="95201.56 95821.23 3785.08"
PAPER["u724_75"]="54912.78 55152.13 3243.15"
PAPER["u724_125"]="39127.15 39414.61 1674.26"
PAPER["u724_200"]="28719.14 28897.76 2039.60"
PAPER["rl1304_10"]="2158869.31 2225719.85 5434.84"
PAPER["rl1304_50"]="803974.59 809531.15 5785.08"
PAPER["rl1304_100"]="498516.64 499324.28 3243.15"
PAPER["rl1304_200"]="277098.41 278316.37 1674.26"
PAPER["rl1304_300"]="193214.26 193713.08 5394.30"
PAPER["pr2392_20"]="2414317.72 2336529.16 3852.16"
PAPER["pr2392_75"]="1099723.42 1134301.54 3979.38"
PAPER["pr2392_150"]="713906.35 714291.51 4512.67"
PAPER["pr2392_300"]="459906.71 463987.13 3629.41"
PAPER["pr2392_500"]="317689.13 319698.34 3763.56"
PAPER["fnl4461_20"]="1300603.87 1314504.75 8563.18"
PAPER["fnl4461_100"]="551321.58 560981.32 9046.80"
PAPER["fnl4461_250"]="336789.92 340197.14 9219.85"
PAPER["fnl4461_500"]="222506.71 232556.71 9804.23"
PAPER["fnl4461_1000"]="146508.37 150584.92 9102.87"
PAPER["p3038_600"]="122918.75 125098.61 4603.21"
PAPER["p3038_700"]="111025.64 111098.21 4309.72"
PAPER["p3038_800"]="100428.31 101104.56 5197.64"
PAPER["p3038_900"]="92587.42 93287.42 5427.38"
PAPER["p3038_1000"]="86153.89 86924.91 5675.22"

# Valores CPLEX (Tabela 2, "-" se nao disponivel)
declare -A CPLEX
CPLEX["lin318_5"]="180251.21";     CPLEX["lin318_15"]="88901.56"
CPLEX["lin318_40"]="47988.38";     CPLEX["lin318_70"]="32196.64"
CPLEX["lin318_100"]="22942.69"
CPLEX["ali535_5"]="9956.77";       CPLEX["ali535_25"]="3695.15"
CPLEX["ali535_50"]="3461.41";      CPLEX["ali535_100"]="1438.42"
CPLEX["ali535_150"]="1032.25"
CPLEX["u724_10"]="151752.96";      CPLEX["u724_30"]="95034.01"
CPLEX["u724_75"]="54735.06";       CPLEX["u724_125"]="38976.76"
CPLEX["u724_200"]="28079.97"
CPLEX["rl1304_10"]="2146484.10";   CPLEX["rl1304_50"]="802283.41"
CPLEX["rl1304_100"]="495925.93";   CPLEX["rl1304_200"]="276977.60"
CPLEX["rl1304_300"]="191224.85"
CPLEX["pr2392_20"]="2236720.55";   CPLEX["pr2392_75"]="1099723.42"

if [[ ! -x "$BIN" ]]; then
    echo "Binario $BIN nao encontrado. Rode 'make' primeiro."
    exit 1
fi

INSTANCES=$(find "$INSTANCE_DIR" -maxdepth 1 -type f \( -name '*.txt' -o -name '*.dat' \) | sort)
TOTAL=$(echo "$INSTANCES" | wc -l)
LAST_SEED=$((SEED_BASE + NUM_SEEDS - 1))

echo "============================================================"
echo "  EXPERIMENTO FINAL"
echo "  Instancias: $TOTAL | Seeds: ${SEED_BASE}..${LAST_SEED} | alpha=$ALPHA"
echo "  NumIterMax: 60 | timeout/run=$RUN_TIMEOUT"
echo "  CSV: $CSV_FILE"
echo "  Log: $LOG_FILE"
echo "============================================================"
echo ""

# Estrutura: para cada seed (rodada), rodar TODAS as instancias
for ((s=0; s<NUM_SEEDS; s++)); do
    SEED=$((SEED_BASE + s))
    echo "======== RODADA $((s+1))/$NUM_SEEDS  (seed=$SEED) ========"

    for INST in $INSTANCES; do
        FNAME=$(basename "$INST")
        GROUP=$(echo "$FNAME" | sed -E 's/^([a-z]+[0-9]+)_.*/\1/')
        P_VAL=$(echo "$FNAME" | sed -E 's/^[a-z]+[0-9]+_0*([0-9]+)\..*/\1/')

        echo -n "  $FNAME seed=$SEED ... "
        START_TS=$(date +%s)

        OUTPUT=$(timeout "$RUN_TIMEOUT" "$BIN" "$INST" "$SEED" "$ALPHA" "$CONSTRUCTION_MAX_TRIES" 2>&1)
        RC=$?

        ELAPSED=$(($(date +%s) - START_TS))

        if [[ $RC -eq 124 ]]; then
            echo "TIMEOUT (${ELAPSED}s)"
            echo "=== $FNAME seed=$SEED TIMEOUT ===" >> "$LOG_FILE"
            echo "$FNAME,$GROUP,,$P_VAL,$SEED,,,,,,,,,,,,,,,$ELAPSED,TIMEOUT" >> "$CSV_FILE"
            continue
        fi
        if [[ $RC -ne 0 ]]; then
            echo "ERRO exit=$RC (${ELAPSED}s)"
            echo "=== $FNAME seed=$SEED ERRO exit=$RC ===" >> "$LOG_FILE"
            echo "$OUTPUT" >> "$LOG_FILE"
            echo "$FNAME,$GROUP,,$P_VAL,$SEED,,,,,,,,,,,,,,,$ELAPSED,ERROR" >> "$CSV_FILE"
            continue
        fi

        # Parse output
        GRASP_COST=$(echo "$OUTPUT" | grep "Custo GRASP:" | awk '{for(i=1;i<=NF;i++) if($i ~ /^[0-9]+\./) {print $i; exit}}')
        VND_COST=$(echo "$OUTPUT" | grep "Custo VND:" | awk '{for(i=1;i<=NF;i++) if($i ~ /^[0-9]+\./) {print $i; exit}}')
        FINAL_COST=$(echo "$OUTPUT" | grep "Custo FINAL:" | awk '{print $NF}')
        ILS_ITER=$(echo "$OUTPUT" | grep "Iteracoes ILS:" | sed -E 's/.*Iteracoes ILS: ([0-9]+).*/\1/')
        ILS_IMPR=$(echo "$OUTPUT" | grep "melhorias:" | sed -E 's/.*melhorias: ([0-9]+).*/\1/')
        ILS_STOP=$(echo "$OUTPUT" | grep "parada:" | sed -E 's/.*parada: ([a-z_]+).*/\1/')
        CS_PO_TRIG=$(echo "$OUTPUT" | grep "CS gatilhos PO:" | sed -E 's/.*CS gatilhos PO: ([0-9]+).*/\1/')
        CS_PO_IMPR=$(echo "$OUTPUT" | grep "CS gatilhos PO:" | sed -E 's/.*melhorias PO: ([0-9]+).*/\1/')
        CS_PO_GAIN=$(echo "$OUTPUT" | grep "CS gatilhos PO:" | sed -E 's/.*ganho total PO: ([0-9.]+).*/\1/')
        CS_PROM=$(echo "$OUTPUT" | grep "CS destroy/repair:" | sed -E 's/.*promocoes ILS: ([0-9]+).*/\1/')
        T_GRASP=$(echo "$OUTPUT" | grep "tempo:" | head -1 | sed -E 's/.*tempo: ([0-9.]+)s.*/\1/')
        T_VND=$(echo "$OUTPUT" | grep "Tempo VND inicial:" | awk '{print $NF}' | tr -d 's')
        T_ILS=$(echo "$OUTPUT" | grep "Tempo ILS:" | awk '{print $NF}' | tr -d 's')
        T_TOT=$(echo "$OUTPUT" | grep "Tempo total:" | awk '{print $NF}' | tr -d 's')
        N_NODES=$(echo "$OUTPUT" | grep -E "^n=" | head -1 | sed -E 's/^n=([0-9]+).*/\1/')

        : "${FINAL_COST:=}"; : "${GRASP_COST:=}"; : "${VND_COST:=}"
        : "${ILS_ITER:=0}"; : "${ILS_IMPR:=0}"; : "${ILS_STOP:=}"
        : "${CS_PO_TRIG:=0}"; : "${CS_PO_IMPR:=0}"; : "${CS_PO_GAIN:=0}"; : "${CS_PROM:=0}"
        : "${T_GRASP:=0}"; : "${T_VND:=0}"; : "${T_ILS:=0}"; : "${T_TOT:=0}"
        : "${N_NODES:=}"

        if [[ -z "$FINAL_COST" ]]; then
            echo "ERRO (sem custo final, ${ELAPSED}s)"
            echo "$FNAME,$GROUP,$N_NODES,$P_VAL,$SEED,,,,,,,,,,,,,,,$ELAPSED,PARSE_ERROR" >> "$CSV_FILE"
            continue
        fi

        STATUS="OK"
        if ! echo "$OUTPUT" | grep -q "Validacao final: OK"; then
            STATUS="FAIL"
        fi

        echo "$FNAME,$GROUP,$N_NODES,$P_VAL,$SEED,$GRASP_COST,$VND_COST,$FINAL_COST,$ILS_ITER,$ILS_IMPR,$ILS_STOP,$CS_PO_TRIG,$CS_PO_IMPR,$CS_PO_GAIN,$CS_PROM,$T_GRASP,$T_VND,$T_ILS,$T_TOT,$STATUS" >> "$CSV_FILE"

        # Log completo por run
        echo "=== $FNAME seed=$SEED ===" >> "$LOG_FILE"
        echo "$OUTPUT" >> "$LOG_FILE"
        echo "" >> "$LOG_FILE"

        LC_NUMERIC=C printf "custo=%.2f  tempo=%ss  ILS=%s/%s  PO=%s/%s  %s  %s\n" \
            "$FINAL_COST" "$T_TOT" "$ILS_ITER" "$ILS_IMPR" "$CS_PO_TRIG" "$CS_PO_IMPR" "$ILS_STOP" "$STATUS"
    done
    echo ""
done

# ========================================================================
# Sumario: calcula best, mean, std, tempo medio, gaps vs paper
# ========================================================================
echo "Gerando sumario..."
python3 - "$CSV_FILE" <<'PYEOF' | tee "$SUMMARY_FILE"
import csv, sys, math
from collections import defaultdict

PAPER = {
    "lin318_5":    (180313.09, 181153.09, 385.96),
    "lin318_15":   (89041.59,  89205.59,  508.87),
    "lin318_40":   (48091.32,  48129.93,  840.39),
    "lin318_70":   (32291.75,  32405.49,  537.69),
    "lin318_100":  (23041.87,  23208.57,  754.01),
    "ali535_5":    (10033.04,  10356.15,  2588.65),
    "ali535_25":   (3702.10,   3826.45,   2596.13),
    "ali535_50":   (2503.34,   2539.27,   2887.95),
    "ali535_100":  (1462.58,   1592.58,   2147.16),
    "ali535_150":  (1036.85,   1078.35,   2546.97),
    "u724_10":     (152019.24, 152977.32, 5183.34),
    "u724_30":     (95201.56,  95821.23,  3785.08),
    "u724_75":     (54912.78,  55152.13,  3243.15),
    "u724_125":    (39127.15,  39414.61,  1674.26),
    "u724_200":    (28719.14,  28897.76,  2039.60),
    "rl1304_10":   (2158869.31,2225719.85,5434.84),
    "rl1304_50":   (803974.59, 809531.15, 5785.08),
    "rl1304_100":  (498516.64, 499324.28, 3243.15),
    "rl1304_200":  (277098.41, 278316.37, 1674.26),
    "rl1304_300":  (193214.26, 193713.08, 5394.30),
    "pr2392_20":   (2414317.72,2336529.16,3852.16),
    "pr2392_75":   (1099723.42,1134301.54,3979.38),
    "pr2392_150":  (713906.35, 714291.51, 4512.67),
    "pr2392_300":  (459906.71, 463987.13, 3629.41),
    "pr2392_500":  (317689.13, 319698.34, 3763.56),
    "fnl4461_20":  (1300603.87,1314504.75,8563.18),
    "fnl4461_100": (551321.58, 560981.32, 9046.80),
    "fnl4461_250": (336789.92, 340197.14, 9219.85),
    "fnl4461_500": (222506.71, 232556.71, 9804.23),
    "fnl4461_1000":(146508.37, 150584.92, 9102.87),
    "p3038_600":   (122918.75, 125098.61, 4603.21),
    "p3038_700":   (111025.64, 111098.21, 4309.72),
    "p3038_800":   (100428.31, 101104.56, 5197.64),
    "p3038_900":   (92587.42,  93287.42,  5427.38),
    "p3038_1000":  (86153.89,  86924.91,  5675.22),
}

csv_path = sys.argv[1]
data = defaultdict(list)  # key -> list of (cost, time)

with open(csv_path) as f:
    reader = csv.DictReader(f)
    for row in reader:
        if row["status"] not in ("OK",):
            continue
        try:
            cost = float(row["final_cost"])
            time = float(row["time_total_s"])
        except (ValueError, KeyError):
            continue
        group = row["group"]
        p = row["p"]
        key = f"{group}_{p}"
        data[key].append((cost, time))

print("=" * 130)
print(f"{'Instancia':<18} {'p':>5} {'n_runs':>6} {'Best':>14} {'Mean':>14} {'Std':>10} {'T_avg':>8}"
      f" {'gap_best%':>10} {'gap_mean%':>10} {'gap_time%':>10}")
print("=" * 130)

gap_bests = []
gap_means = []
gap_times = []

# Sort keys by group then p
def sort_key(k):
    parts = k.rsplit("_", 1)
    return (parts[0], int(parts[1]))

for key in sorted(data.keys(), key=sort_key):
    costs = [c for c, t in data[key]]
    times = [t for c, t in data[key]]
    n_runs = len(costs)
    best = min(costs)
    mean = sum(costs) / n_runs
    std = math.sqrt(sum((c - mean)**2 for c in costs) / max(1, n_runs - 1)) if n_runs > 1 else 0
    t_avg = sum(times) / n_runs

    parts = key.rsplit("_", 1)
    group = parts[0]
    p = parts[1]

    ref = PAPER.get(key)
    if ref:
        ref_best, ref_mean, ref_time = ref
        gb = 100.0 * (best - ref_best) / ref_best
        gm = 100.0 * (mean - ref_mean) / ref_mean
        gt = 100.0 * (t_avg - ref_time) / ref_time
        gap_bests.append(gb)
        gap_means.append(gm)
        gap_times.append(gt)
        s_gb = f"{gb:+.3f}"
        s_gm = f"{gm:+.3f}"
        s_gt = f"{gt:+.1f}"
    else:
        s_gb = s_gm = s_gt = "-"

    print(f"{group:<13} {p:>5}   {n_runs:>4}   {best:>14.2f} {mean:>14.2f} {std:>10.2f} {t_avg:>7.1f}s"
          f"   {s_gb:>10} {s_gm:>10} {s_gt:>10}")

print("=" * 130)
if gap_bests:
    avg_gb = sum(gap_bests) / len(gap_bests)
    avg_gm = sum(gap_means) / len(gap_means)
    avg_gt = sum(gap_times) / len(gap_times)
    print(f"{'MEDIA GERAL':<18} {'':>5} {'':>6} {'':>14} {'':>14} {'':>10} {'':>8}"
          f"   {avg_gb:>+10.3f} {avg_gm:>+10.3f} {avg_gt:>+10.1f}")

n_timeout = 0
n_error = 0
with open(csv_path) as f:
    reader = csv.DictReader(f)
    for row in reader:
        if row["status"] == "TIMEOUT":
            n_timeout += 1
        elif row["status"] in ("ERROR", "PARSE_ERROR", "FAIL"):
            n_error += 1

total_runs = sum(len(v) for v in data.values()) + n_timeout + n_error
print(f"\nTotal runs: {total_runs}  OK: {sum(len(v) for v in data.values())}  TIMEOUT: {n_timeout}  ERRO: {n_error}")
print(f"\nCSV detalhado: {csv_path}")
PYEOF

echo ""
echo "Sumario salvo em: $SUMMARY_FILE"
echo "CSV detalhado:    $CSV_FILE"
echo "Log completo:     $LOG_FILE"
