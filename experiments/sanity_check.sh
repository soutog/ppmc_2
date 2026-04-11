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
NUM_ITER_MAX=20
CONSTRUCTION_MAX_TRIES=1000
NUM_RUNS=5
SEED_BASE=42
RUN_TIMEOUT=600s

mkdir -p "$RESULT_DIR"
OUTFILE="$RESULT_DIR/sanity_$(date +%Y%m%d_%H%M%S).csv"
echo "instance,group,p,run,seed,grasp_cost,vnd_cost,ils_cost,partial_opt_cost,partial_opt_improved,ils_iterations,ils_improvements,time_partial_opt_s,time_total_s,status" > "$OUTFILE"

# instancia  grupo  p  referencia_ILS  referencia_CPLEX
INSTANCES=(
    "instances/lin318_005.txt lin318 5  180343.00 180281.21"
    "instances/lin318_015.txt lin318 15  89041.59  88901.56"
    "instances/ali535_005.txt ali535 5   10003.21   9956.77"
    "instances/u724_075.txt   u724 75   54912.78   54735.05"
    "instances/rl1304_010.txt rl1304 10   2158869.31   2146484.10"
)

if [[ ! -x "$BIN" ]]; then
    echo "Binario $BIN nao encontrado. Rode 'make' primeiro."
    exit 1
fi

printf "%-22s %-6s %4s %6s %12s %12s %10s %10s %9s %8s\n" \
    "instancia" "seed" "p" "run" "ILS" "FINAL" "gap_FINAL%" "gap_CPX%" "dPO" "tempo"
echo "-------------------------------------------------------------------------------------------------------"

for row in "${INSTANCES[@]}"; do
    read -r INST GROUP P_VAL REF_ILS REF_CPX <<< "$row"
    FNAME=$(basename "$INST")

    BEST_FINAL=""
    SUM_FINAL=0
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
        PO_COST=$(echo    "$OUTPUT" | grep "Custo pos-PartialOpt:" | awk '{print $NF}')
        PO_IMPROVED=$(echo "$OUTPUT" | grep "PartialOpt melhorou:" | awk '{print $NF}')
        ILS_ITER=$(echo   "$OUTPUT" | grep "Iteracoes ILS:" | sed -E 's/.*Iteracoes ILS: ([0-9]+).*/\1/')
        ILS_IMPR=$(echo   "$OUTPUT" | grep "melhorias:"     | sed -E 's/.*melhorias: ([0-9]+).*/\1/')
        T_PO=$(echo       "$OUTPUT" | grep "Tempo PartialOpt:" | awk '{print $NF}' | tr -d 's')
        T_TOT=$(echo      "$OUTPUT" | grep "Tempo total:"  | awk '{print $NF}' | tr -d 's')

        if [[ -z "$PO_COST" ]]; then
            PO_COST="$ILS_COST"
        fi
        if [[ -z "$PO_IMPROVED" ]]; then
            PO_IMPROVED="nao"
        fi
        if [[ -z "$T_PO" ]]; then
            T_PO="0"
        fi

        if echo "$OUTPUT" | grep -q "Validacao pos-ILS: OK"; then
            STATUS="OK"
        else
            STATUS="FAIL"
        fi

        GAP_FINAL=$(awk -v v="$PO_COST" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        GAP_CPX=$(awk -v v="$PO_COST" -v r="$REF_CPX" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        DELTA_PO=$(awk -v a="$ILS_COST" -v b="$PO_COST" 'BEGIN{printf "%.2f", a-b}')

        printf "%-22s %-6s %4s %6s %12.2f %12.2f %10s %10s %9s %8ss %s\n" \
            "$FNAME" "$SEED" "$P_VAL" "$((r+1))" "$ILS_COST" "$PO_COST" "$GAP_FINAL" "$GAP_CPX" "$DELTA_PO" "$T_TOT" "$STATUS"

        echo "$FNAME,$GROUP,$P_VAL,$((r+1)),$SEED,$GRASP_COST,$VND_COST,$ILS_COST,$PO_COST,$PO_IMPROVED,$ILS_ITER,$ILS_IMPR,$T_PO,$T_TOT,$STATUS" >> "$OUTFILE"

        # best e mean do algoritmo final (apos PartialOpt).
        if [[ -z "$BEST_FINAL" ]] || awk -v a="$PO_COST" -v b="$BEST_FINAL" 'BEGIN{exit !(a<b)}'; then
            BEST_FINAL="$PO_COST"
        fi
        SUM_FINAL=$(awk -v s="$SUM_FINAL" -v v="$PO_COST" 'BEGIN{printf "%.6f", s+v}')
        COUNT=$((COUNT+1))
    done

    if [[ $COUNT -gt 0 ]]; then
        MEAN_FINAL=$(awk -v s="$SUM_FINAL" -v c="$COUNT" 'BEGIN{printf "%.2f", s/c}')
        GAP_BEST=$(awk -v v="$BEST_FINAL" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        GAP_MEAN=$(awk -v v="$MEAN_FINAL" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        echo "  >> $FNAME  best_final=$BEST_FINAL (gap=${GAP_BEST}%)  mean_final=$MEAN_FINAL (gap=${GAP_MEAN}%)  ref_ILS=$REF_ILS"
    fi
    echo ""
done

echo "Resultados em: $OUTFILE"
