#!/bin/bash
# Sanity check da arquitetura atual:
#   GRASP -> VND -> ILS + Clustering Search
# onde o Partial Optimizer e disparado dentro do CS.
#
# As instancias pequenas continuam servindo como teste de consistencia:
# se houver degradacao grande nelas, o problema tende a ser de implementacao,
# e nao apenas variancia estocastica.
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
echo "instance,group,p,run,seed,grasp_cost,vnd_cost,final_cost,ils_iterations,ils_improvements,cs_observations,cs_active_clusters,cs_new_clusters,cs_center_updates,cs_po_triggers,cs_po_improvements,cs_po_total_gain,time_ils_s,time_total_s,status" > "$OUTFILE"

# instancia  grupo  p  referencia_ILS_paper  referencia_CPLEX
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

printf "%-22s %-6s %4s %6s %12s %10s %10s %9s %8s\n" \
    "instancia" "seed" "p" "run" "FINAL" "gap_ILS%" "gap_CPX%" "CS_PO" "tempo"
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
        FINAL_COST=$(echo "$OUTPUT" | grep "Custo FINAL:" | awk '{print $NF}')
        ILS_ITER=$(echo   "$OUTPUT" | grep "Iteracoes ILS:" | sed -E 's/.*Iteracoes ILS: ([0-9]+).*/\1/')
        ILS_IMPR=$(echo   "$OUTPUT" | grep "melhorias:"     | sed -E 's/.*melhorias: ([0-9]+).*/\1/')
        CS_OBS=$(echo "$OUTPUT" | grep "CS observacoes:" | sed -E 's/.*CS observacoes: ([0-9]+).*/\1/')
        CS_ACTIVE=$(echo "$OUTPUT" | grep "CS observacoes:" | sed -E 's/.*clusters ativos: ([0-9]+).*/\1/')
        CS_NEW=$(echo "$OUTPUT" | grep "CS observacoes:" | sed -E 's/.*novos clusters: ([0-9]+).*/\1/')
        CS_UPD=$(echo "$OUTPUT" | grep "CS observacoes:" | sed -E 's/.*updates de centro: ([0-9]+).*/\1/')
        CS_PO_TRIG=$(echo "$OUTPUT" | grep "CS gatilhos PO:" | sed -E 's/.*CS gatilhos PO: ([0-9]+).*/\1/')
        CS_PO_IMPR=$(echo "$OUTPUT" | grep "CS gatilhos PO:" | sed -E 's/.*melhorias PO: ([0-9]+).*/\1/')
        CS_PO_GAIN=$(echo "$OUTPUT" | grep "CS gatilhos PO:" | sed -E 's/.*ganho total PO: ([0-9.]+).*/\1/')
        T_ILS=$(echo "$OUTPUT" | grep "Tempo ILS:" | awk '{print $NF}' | tr -d 's')
        T_TOT=$(echo      "$OUTPUT" | grep "Tempo total:"  | awk '{print $NF}' | tr -d 's')

        if [[ -z "$FINAL_COST" ]]; then
            printf "%-22s %-6s %4s %6s  ERRO (sem custo final)\n" "$FNAME" "$SEED" "$P_VAL" "$((r+1))"
            continue
        fi
        if [[ -z "$CS_OBS" ]]; then CS_OBS="0"; fi
        if [[ -z "$CS_ACTIVE" ]]; then CS_ACTIVE="0"; fi
        if [[ -z "$CS_NEW" ]]; then CS_NEW="0"; fi
        if [[ -z "$CS_UPD" ]]; then CS_UPD="0"; fi
        if [[ -z "$CS_PO_TRIG" ]]; then CS_PO_TRIG="0"; fi
        if [[ -z "$CS_PO_IMPR" ]]; then CS_PO_IMPR="0"; fi
        if [[ -z "$CS_PO_GAIN" ]]; then CS_PO_GAIN="0"; fi
        if [[ -z "$T_ILS" ]]; then T_ILS="0"; fi

        if echo "$OUTPUT" | grep -q "Validacao final: OK"; then
            STATUS="OK"
        else
            STATUS="FAIL"
        fi

        GAP_FINAL=$(awk -v v="$FINAL_COST" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        GAP_CPX=$(awk -v v="$FINAL_COST" -v r="$REF_CPX" 'BEGIN{printf "%.3f", (v-r)/r*100}')

        printf "%-22s %-6s %4s %6s %12.2f %10s %10s %9s %8ss %s\n" \
            "$FNAME" "$SEED" "$P_VAL" "$((r+1))" "$FINAL_COST" "$GAP_FINAL" "$GAP_CPX" "$CS_PO_TRIG/$CS_PO_IMPR" "$T_TOT" "$STATUS"

        echo "$FNAME,$GROUP,$P_VAL,$((r+1)),$SEED,$GRASP_COST,$VND_COST,$FINAL_COST,$ILS_ITER,$ILS_IMPR,$CS_OBS,$CS_ACTIVE,$CS_NEW,$CS_UPD,$CS_PO_TRIG,$CS_PO_IMPR,$CS_PO_GAIN,$T_ILS,$T_TOT,$STATUS" >> "$OUTFILE"

        if [[ -z "$BEST_FINAL" ]] || awk -v a="$FINAL_COST" -v b="$BEST_FINAL" 'BEGIN{exit !(a<b)}'; then
            BEST_FINAL="$FINAL_COST"
        fi
        SUM_FINAL=$(awk -v s="$SUM_FINAL" -v v="$FINAL_COST" 'BEGIN{printf "%.6f", s+v}')
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
