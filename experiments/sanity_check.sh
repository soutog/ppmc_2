#!/bin/bash
# Sanity check: 3 instancias pequenas (p <= 15), 5 seeds cada.
# Nessas instancias o CPLEX e o ILS do paper batem o otimo exato.
# Se aqui nao bater, e bug de implementacao, nao variancia.
#
# Uso:
#   make && ./experiments/sanity_check.sh
#
# Referencias (Tabela 2 do paper):
#   lin318_5  : ILS = 180343.00   CPLEX = 180281.21
#   lin318_15 : ILS =  89041.59   CPLEX =  88901.56
#   ali535_5  : ILS =  10003.21   CPLEX =   9956.77

set -u

BIN="./bin/main"
RESULT_DIR="./experiments/results"

ALPHA=0.6
NUM_ITER_MAX=200
CONSTRUCTION_MAX_TRIES=1000
NUM_RUNS=5
SEED_BASE=10
RUN_TIMEOUT=120s

mkdir -p "$RESULT_DIR"
OUTFILE="$RESULT_DIR/sanity_$(date +%Y%m%d_%H%M%S).csv"
echo "instance,group,p,run,seed,grasp_cost,vnd_cost,ils_cost,ils_iterations,ils_improvements,time_total_s,status" > "$OUTFILE"

# instancia  grupo  p  referencia_ILS  referencia_CPLEX
INSTANCES=(
    "instances/lin318_005.txt lin318 5  180343.00 180281.21"
    "instances/lin318_015.txt lin318 15  89041.59  88901.56"
    "instances/ali535_005.txt ali535 5   10003.21   9956.77"
)

if [[ ! -x "$BIN" ]]; then
    echo "Binario $BIN nao encontrado. Rode 'make' primeiro."
    exit 1
fi

printf "%-22s %-6s %4s %6s %12s %12s %10s %10s %8s\n" \
    "instancia" "seed" "p" "run" "ILS" "paper_ILS" "gap_ILS%" "gap_CPX%" "tempo"
echo "-------------------------------------------------------------------------------------------------------"

for row in "${INSTANCES[@]}"; do
    read -r INST GROUP P_VAL REF_ILS REF_CPX <<< "$row"
    FNAME=$(basename "$INST")

    BEST_ILS=""
    SUM_ILS=0
    COUNT=0

    for ((r=0; r<NUM_RUNS; r++)); do
        SEED=$((SEED_BASE + r))

        OUTPUT=$(timeout "$RUN_TIMEOUT" "$BIN" "$INST" "$SEED" "$ALPHA" "$CONSTRUCTION_MAX_TRIES" "$NUM_ITER_MAX" 2>&1)
        RC=$?
        if [[ $RC -eq 124 ]]; then
            printf "%-22s %-6s %4s %6s  TIMEOUT (%s)\n" "$FNAME" "$SEED" "$P_VAL" "$((r+1))" "$RUN_TIMEOUT"
            continue
        fi
        if [[ $RC -ne 0 ]]; then
            printf "%-22s %-6s %4s %6s  ERRO (exit=%d)\n" "$FNAME" "$SEED" "$P_VAL" "$((r+1))" "$RC"
            continue
        fi

        GRASP_COST=$(echo "$OUTPUT" | grep "Custo GRASP:" | awk '{print $NF}')
        VND_COST=$(echo   "$OUTPUT" | grep "Custo VND:"   | awk '{print $NF}')
        ILS_COST=$(echo   "$OUTPUT" | grep "Custo ILS:"   | awk '{print $NF}')
        ILS_ITER=$(echo   "$OUTPUT" | grep "Iteracoes ILS:" | sed -E 's/.*Iteracoes ILS: ([0-9]+).*/\1/')
        ILS_IMPR=$(echo   "$OUTPUT" | grep "melhorias:"     | sed -E 's/.*melhorias: ([0-9]+).*/\1/')
        T_TOT=$(echo      "$OUTPUT" | grep "Tempo total:"  | awk '{print $NF}' | tr -d 's')

        if echo "$OUTPUT" | grep -q "Validacao pos-ILS: OK"; then
            STATUS="OK"
        else
            STATUS="FAIL"
        fi

        GAP_ILS=$(awk -v v="$ILS_COST" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        GAP_CPX=$(awk -v v="$ILS_COST" -v r="$REF_CPX" 'BEGIN{printf "%.3f", (v-r)/r*100}')

        printf "%-22s %-6s %4s %6s %12.2f %12.2f %10s %10s %8ss %s\n" \
            "$FNAME" "$SEED" "$P_VAL" "$((r+1))" "$ILS_COST" "$REF_ILS" "$GAP_ILS" "$GAP_CPX" "$T_TOT" "$STATUS"

        echo "$FNAME,$GROUP,$P_VAL,$((r+1)),$SEED,$GRASP_COST,$VND_COST,$ILS_COST,$ILS_ITER,$ILS_IMPR,$T_TOT,$STATUS" >> "$OUTFILE"

        # best e mean
        if [[ -z "$BEST_ILS" ]] || awk -v a="$ILS_COST" -v b="$BEST_ILS" 'BEGIN{exit !(a<b)}'; then
            BEST_ILS="$ILS_COST"
        fi
        SUM_ILS=$(awk -v s="$SUM_ILS" -v v="$ILS_COST" 'BEGIN{printf "%.6f", s+v}')
        COUNT=$((COUNT+1))
    done

    if [[ $COUNT -gt 0 ]]; then
        MEAN_ILS=$(awk -v s="$SUM_ILS" -v c="$COUNT" 'BEGIN{printf "%.2f", s/c}')
        GAP_BEST=$(awk -v v="$BEST_ILS" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        GAP_MEAN=$(awk -v v="$MEAN_ILS" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        echo "  >> $FNAME  best=$BEST_ILS (gap=${GAP_BEST}%)  mean=$MEAN_ILS (gap=${GAP_MEAN}%)  ref_ILS=$REF_ILS"
    fi
    echo ""
done

echo "Resultados em: $OUTFILE"
