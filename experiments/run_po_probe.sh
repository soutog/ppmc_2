#!/bin/bash
# Mini-batch para coletar a distribuicao de t_last_inc do PartialOptimizer.
# Roda apenas as 3 instancias problematicas em 2 seeds e captura o log
# completo de cada [PARTIAL_OPT] para analise posterior.
#
# Uso:
#   make && ./experiments/run_po_probe.sh
#
# Pos-processamento sugerido:
#   grep '^\[PARTIAL_OPT\] ref_cluster' experiments/results/po_probe_*.log \
#     | awk -F'[| ]+' '{for(i=1;i<=NF;i++){if($i~/^t_last_inc=/){gsub("s$","",$i); split($i,a,"="); print a[2]}}}' \
#     | sort -n | awk '{arr[NR]=$1} END {n=NR; print "n="n, "p50="arr[int(n*0.5)], "p90="arr[int(n*0.9)], "p99="arr[int(n*0.99)], "max="arr[n]}'

set -u

BIN="./bin/main"
RESULT_DIR="./experiments/results"
ALPHA=0.6
NUM_ITER_MAX=60
CONSTRUCTION_MAX_TRIES=1000
RUN_TIMEOUT=2400s

INSTANCES=(
    "./instances/sample_instance/ali535_150.txt"
    "./instances/sample_instance/pr2392_075.txt"
    "./instances/sample_instance/u724_200.txt"
)
SEEDS=(42 43)

mkdir -p "$RESULT_DIR"
LOGFILE="$RESULT_DIR/po_probe_$(date +%Y%m%d_%H%M%S).log"

if [[ ! -x "$BIN" ]]; then
    echo "Binario $BIN nao encontrado. Rode 'make' primeiro."
    exit 1
fi

echo "Probe PO: ${#INSTANCES[@]} instancias x ${#SEEDS[@]} seeds | timeout/run=$RUN_TIMEOUT"
echo "Log completo: $LOGFILE"
echo ""

for INST in "${INSTANCES[@]}"; do
    FNAME=$(basename "$INST")
    for SEED in "${SEEDS[@]}"; do
        echo "=== $FNAME seed=$SEED ===" | tee -a "$LOGFILE"
        START=$(date +%s)
        stdbuf -oL -eL timeout "$RUN_TIMEOUT" "$BIN" "$INST" "$SEED" \
            "$ALPHA" "$CONSTRUCTION_MAX_TRIES" "$NUM_ITER_MAX" 2>&1 | tee -a "$LOGFILE"
        RC=${PIPESTATUS[0]}
        ELAPSED=$(( $(date +%s) - START ))
        if [[ $RC -eq 124 ]]; then
            echo "[STATUS] TIMEOUT apos ${ELAPSED}s" | tee -a "$LOGFILE"
        elif [[ $RC -ne 0 ]]; then
            echo "[STATUS] ERRO exit=$RC apos ${ELAPSED}s" | tee -a "$LOGFILE"
        else
            echo "[STATUS] OK em ${ELAPSED}s" | tee -a "$LOGFILE"
        fi
        echo "" | tee -a "$LOGFILE"
    done
done

echo "---- Distribuicao de t_last_inc (segundos ate a ultima incumbent) ----"
grep '^\[PARTIAL_OPT\] ref_cluster' "$LOGFILE" \
    | awk -F'[|]' '{for(i=1;i<=NF;i++){if($i~/t_last_inc=/){gsub(/[^0-9.]/,"",$i); print $i}}}' \
    | sort -n \
    | awk 'NF>0 {arr[NR]=$1}
           END {
             n=NR;
             if(n==0){print "(sem amostras)"; exit}
             function q(p){return arr[int(p*n+0.5)>0?int(p*n+0.5):1]}
             print "n="n, "min="arr[1], "p50="q(0.5), "p75="q(0.75), "p90="q(0.9), "p95="q(0.95), "p99="q(0.99), "max="arr[n]
           }'

echo ""
echo "Log salvo em: $LOGFILE"
