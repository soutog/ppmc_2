#!/bin/bash
# Experimento em lote sobre o sample_instance.
# Formato de log identico ao sanity_check.sh, mas expandido para trazer
# metricas adicionais do CS (destroy/repair, promocoes p/ ILS) e estatisticas
# agregadas por instancia (best, mean, std, min/max de tempo).
#
# Uso:
#   make && ./experiments/run_batch.sh

set -u

BIN="./bin/main"
INSTANCE_DIR="./instances/sample_instance"
RESULT_DIR="./experiments/results"

ALPHA=0.6
NUM_ITER_MAX=20
CONSTRUCTION_MAX_TRIES=1000
NUM_RUNS=5
SEED_BASE=42
RUN_TIMEOUT=1200s

mkdir -p "$RESULT_DIR"
OUTFILE="$RESULT_DIR/batch_$(date +%Y%m%d_%H%M%S).csv"
echo "instance,group,p,run,seed,grasp_cost,vnd_cost,final_cost,ils_iterations,ils_improvements,cs_observations,cs_active_clusters,cs_new_clusters,cs_center_updates,cs_po_triggers,cs_po_improvements,cs_po_total_gain,cs_destroy_repair_calls,cs_destroy_repair_improvements,cs_ils_promotions,time_ils_s,time_total_s,status" > "$OUTFILE"

# Referencias extraidas de experiments/compare_tables.py (Tabelas 2 e 3 do paper
# + CPLEX quando disponivel). Campos: grupo p ref_ILS_best ref_ILS_mean ref_CPX
# ("-" quando nao ha valor de CPLEX na Tabela 2).
declare -A REF_PAPER_BEST REF_PAPER_MEAN REF_CPX
REF_PAPER_BEST["ali535_25"]=3702.10;    REF_PAPER_MEAN["ali535_25"]=3826.45;    REF_CPX["ali535_25"]=3695.15
REF_PAPER_BEST["ali535_150"]=1036.85;   REF_PAPER_MEAN["ali535_150"]=1078.35;   REF_CPX["ali535_150"]=1032.28
REF_PAPER_BEST["fnl4461_20"]=1300603.87; REF_PAPER_MEAN["fnl4461_20"]=1314504.75; REF_CPX["fnl4461_20"]="-"
REF_PAPER_BEST["fnl4461_250"]=336789.92; REF_PAPER_MEAN["fnl4461_250"]=340197.14; REF_CPX["fnl4461_250"]="-"
REF_PAPER_BEST["fnl4461_1000"]=146508.37; REF_PAPER_MEAN["fnl4461_1000"]=150584.92; REF_CPX["fnl4461_1000"]="-"
REF_PAPER_BEST["lin318_15"]=89041.59;   REF_PAPER_MEAN["lin318_15"]=89205.59;   REF_CPX["lin318_15"]=88901.56
REF_PAPER_BEST["lin318_100"]=23041.87;  REF_PAPER_MEAN["lin318_100"]=23208.57;  REF_CPX["lin318_100"]=22942.69
REF_PAPER_BEST["p3038_600"]=122918.75;  REF_PAPER_MEAN["p3038_600"]=125098.61;  REF_CPX["p3038_600"]="-"
REF_PAPER_BEST["p3038_1000"]=86153.89;  REF_PAPER_MEAN["p3038_1000"]=86934.94;  REF_CPX["p3038_1000"]="-"
REF_PAPER_BEST["pr2392_75"]=1099723.42; REF_PAPER_MEAN["pr2392_75"]=1134301.54; REF_CPX["pr2392_75"]="-"
REF_PAPER_BEST["pr2392_500"]=317689.13; REF_PAPER_MEAN["pr2392_500"]=319698.34; REF_CPX["pr2392_500"]="-"
REF_PAPER_BEST["rl1304_50"]=803974.59;  REF_PAPER_MEAN["rl1304_50"]=809531.15;  REF_CPX["rl1304_50"]=802283.41
REF_PAPER_BEST["rl1304_300"]=193214.26; REF_PAPER_MEAN["rl1304_300"]=193713.08; REF_CPX["rl1304_300"]=191224.85
REF_PAPER_BEST["u724_30"]=95201.56;     REF_PAPER_MEAN["u724_30"]=95821.23;     REF_CPX["u724_30"]=95034.01
REF_PAPER_BEST["u724_200"]=28719.14;    REF_PAPER_MEAN["u724_200"]=28897.76;    REF_CPX["u724_200"]=28079.97

if [[ ! -x "$BIN" ]]; then
    echo "Binario $BIN nao encontrado. Rode 'make' primeiro."
    exit 1
fi

INSTANCES=$(find "$INSTANCE_DIR" -type f \( -name '*.txt' -o -name '*.dat' \) | sort)
TOTAL=$(echo "$INSTANCES" | wc -l)

echo "Diretorio: $INSTANCE_DIR"
echo "Instancias: $TOTAL | Runs: $NUM_RUNS | Seeds: ${SEED_BASE}..$(( SEED_BASE + NUM_RUNS - 1 )) | alpha=$ALPHA | NumIterMax=$NUM_ITER_MAX | timeout/run=$RUN_TIMEOUT"
echo ""

printf "%-22s %-6s %6s %6s %14s %10s %10s %9s %8s %6s %9s\n" \
    "instancia" "seed" "p" "run" "FINAL" "gap_ILS%" "gap_CPX%" "CS_PO" "CS_DR" "prom" "tempo"
echo "--------------------------------------------------------------------------------------------------------------------"

for INST in $INSTANCES; do
    FNAME=$(basename "$INST")
    GROUP=$(echo "$FNAME" | sed -E 's/^([a-z]+[0-9]+)_.*/\1/')
    P_VAL=$(echo "$FNAME" | sed -E 's/^[a-z]+[0-9]+_0*([0-9]+)\..*/\1/')
    KEY="${GROUP}_${P_VAL}"

    REF_ILS="${REF_PAPER_BEST[$KEY]:-}"
    REF_MEAN="${REF_PAPER_MEAN[$KEY]:-}"
    REF_CPLEX="${REF_CPX[$KEY]:--}"

    BEST_FINAL=""
    SUM_FINAL=0
    SUM_SQ=0
    SUM_TIME=0
    MIN_TIME=""
    MAX_TIME=""
    COUNT=0

    for ((r=0; r<NUM_RUNS; r++)); do
        SEED=$((SEED_BASE + r))

        OUTPUT=$(timeout "$RUN_TIMEOUT" "$BIN" "$INST" "$SEED" "$ALPHA" "$CONSTRUCTION_MAX_TRIES" "$NUM_ITER_MAX" 2>&1)
        RC=$?
        if [[ $RC -eq 124 ]]; then
            printf "%-22s %-6s %6s %6s  TIMEOUT (%s)\n" "$FNAME" "$SEED" "$P_VAL" "$((r+1))" "$RUN_TIMEOUT"
            continue
        fi
        if [[ $RC -ne 0 ]]; then
            printf "%-22s %-6s %6s %6s  ERRO (exit=%d)\n" "$FNAME" "$SEED" "$P_VAL" "$((r+1))" "$RC"
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
        CS_DR_CALLS=$(echo "$OUTPUT" | grep "CS destroy/repair:" | sed -E 's|.*CS destroy/repair: ([0-9]+).*|\1|')
        CS_DR_IMPR=$(echo "$OUTPUT" | grep "CS destroy/repair:" | sed -E 's/.*com melhoria: ([0-9]+).*/\1/')
        CS_PROM=$(echo "$OUTPUT" | grep "CS destroy/repair:" | sed -E 's/.*promocoes ILS: ([0-9]+).*/\1/')
        T_ILS=$(echo "$OUTPUT" | grep "Tempo ILS:" | awk '{print $NF}' | tr -d 's')
        T_TOT=$(echo "$OUTPUT" | grep "Tempo total:" | awk '{print $NF}' | tr -d 's')

        if [[ -z "$FINAL_COST" ]]; then
            printf "%-22s %-6s %6s %6s  ERRO (sem custo final)\n" "$FNAME" "$SEED" "$P_VAL" "$((r+1))"
            continue
        fi

        : "${CS_OBS:=0}"; : "${CS_ACTIVE:=0}"; : "${CS_NEW:=0}"; : "${CS_UPD:=0}"
        : "${CS_PO_TRIG:=0}"; : "${CS_PO_IMPR:=0}"; : "${CS_PO_GAIN:=0}"
        : "${CS_DR_CALLS:=0}"; : "${CS_DR_IMPR:=0}"; : "${CS_PROM:=0}"
        : "${T_ILS:=0}"; : "${T_TOT:=0}"

        if echo "$OUTPUT" | grep -q "Validacao final: OK"; then
            STATUS="OK"
        else
            STATUS="FAIL"
        fi

        if [[ -n "$REF_ILS" ]]; then
            GAP_FINAL=$(awk -v v="$FINAL_COST" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        else
            GAP_FINAL="-"
        fi
        if [[ "$REF_CPLEX" != "-" ]]; then
            GAP_CPX=$(awk -v v="$FINAL_COST" -v r="$REF_CPLEX" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        else
            GAP_CPX="-"
        fi

        printf "%-22s %-6s %6s %6s %14.2f %10s %10s %9s %8s %6s %8ss %s\n" \
            "$FNAME" "$SEED" "$P_VAL" "$((r+1))" "$FINAL_COST" "$GAP_FINAL" "$GAP_CPX" \
            "$CS_PO_TRIG/$CS_PO_IMPR" "$CS_DR_CALLS/$CS_DR_IMPR" "$CS_PROM" "$T_TOT" "$STATUS"

        echo "$FNAME,$GROUP,$P_VAL,$((r+1)),$SEED,$GRASP_COST,$VND_COST,$FINAL_COST,$ILS_ITER,$ILS_IMPR,$CS_OBS,$CS_ACTIVE,$CS_NEW,$CS_UPD,$CS_PO_TRIG,$CS_PO_IMPR,$CS_PO_GAIN,$CS_DR_CALLS,$CS_DR_IMPR,$CS_PROM,$T_ILS,$T_TOT,$STATUS" >> "$OUTFILE"

        if [[ -z "$BEST_FINAL" ]] || awk -v a="$FINAL_COST" -v b="$BEST_FINAL" 'BEGIN{exit !(a<b)}'; then
            BEST_FINAL="$FINAL_COST"
        fi
        SUM_FINAL=$(awk -v s="$SUM_FINAL" -v v="$FINAL_COST" 'BEGIN{printf "%.6f", s+v}')
        SUM_SQ=$(awk -v s="$SUM_SQ" -v v="$FINAL_COST" 'BEGIN{printf "%.6f", s+v*v}')
        SUM_TIME=$(awk -v s="$SUM_TIME" -v v="$T_TOT" 'BEGIN{printf "%.6f", s+v}')
        if [[ -z "$MIN_TIME" ]] || awk -v a="$T_TOT" -v b="$MIN_TIME" 'BEGIN{exit !(a<b)}'; then
            MIN_TIME="$T_TOT"
        fi
        if [[ -z "$MAX_TIME" ]] || awk -v a="$T_TOT" -v b="$MAX_TIME" 'BEGIN{exit !(a>b)}'; then
            MAX_TIME="$T_TOT"
        fi
        COUNT=$((COUNT+1))
    done

    if [[ $COUNT -gt 0 ]]; then
        MEAN_FINAL=$(awk -v s="$SUM_FINAL" -v c="$COUNT" 'BEGIN{printf "%.2f", s/c}')
        # Desvio padrao amostral: sqrt((sum_sq - n*mean^2) / (n-1)); fallback 0 com c=1.
        STD_FINAL=$(awk -v ss="$SUM_SQ" -v s="$SUM_FINAL" -v c="$COUNT" \
            'BEGIN{if(c<2){print "0.00"; exit} m=s/c; v=(ss - c*m*m)/(c-1); if(v<0)v=0; printf "%.2f", sqrt(v)}')
        MEAN_TIME=$(awk -v s="$SUM_TIME" -v c="$COUNT" 'BEGIN{printf "%.2f", s/c}')

        if [[ -n "$REF_ILS" ]]; then
            GAP_BEST=$(awk -v v="$BEST_FINAL" -v r="$REF_ILS" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        else
            GAP_BEST="-"
        fi
        if [[ -n "$REF_MEAN" ]]; then
            GAP_MEAN=$(awk -v v="$MEAN_FINAL" -v r="$REF_MEAN" 'BEGIN{printf "%.3f", (v-r)/r*100}')
        else
            GAP_MEAN="-"
        fi

        echo "  >> $FNAME  best=$BEST_FINAL (gap_best=${GAP_BEST}% vs paper.best)  mean=$MEAN_FINAL ±$STD_FINAL (gap_mean=${GAP_MEAN}% vs paper.mean)  tempo[min/avg/max]=${MIN_TIME}/${MEAN_TIME}/${MAX_TIME}s"
    fi
    echo ""
done

echo "Resultados em: $OUTFILE"
