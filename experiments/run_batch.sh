#!/bin/bash
# Experimento em lote: roda o ILS multiplas vezes por instancia
# Uso: ./experiments/run_batch.sh
#
# Configuracao fixa para reproducao:
#   5 runs por instancia, seeds 42..46, alpha=0.6, NumIterMax=20

BIN="./bin/main"
INSTANCE_DIR="./instances/sample_instance"
RESULT_DIR="./experiments/results"

NUM_RUNS=5
SEED_BASE=10
ALPHA=0.6
NUM_ITER_MAX=20
CONSTRUCTION_MAX_TRIES=1000

mkdir -p "$RESULT_DIR"

OUTFILE="$RESULT_DIR/batch_$(date +%Y%m%d_%H%M%S).csv"
echo "instance,group,p,run,seed,grasp_cost,vnd_cost,ils_cost,ils_iterations,ils_improvements,time_total_s" > "$OUTFILE"

INSTANCES=$(find "$INSTANCE_DIR" -type f \( -name '*.txt' -o -name '*.dat' \) | sort)

TOTAL=$(echo "$INSTANCES" | wc -l)
echo "Instancias: $TOTAL | Runs: $NUM_RUNS | Seeds: ${SEED_BASE}..$(( SEED_BASE + NUM_RUNS - 1 )) | alpha=$ALPHA | NumIterMax=$NUM_ITER_MAX"
echo ""

for INST in $INSTANCES; do
    FNAME=$(basename "$INST")
    GROUP=$(echo "$FNAME" | sed -E 's/^([a-z]+[0-9]+)_.*/\1/')
    P_VAL=$(echo "$FNAME" | sed -E 's/^[a-z]+[0-9]+_0*([0-9]+)\..*/\1/')

    echo "=== $FNAME (grupo=$GROUP, p=$P_VAL) ==="

    for ((r=0; r<NUM_RUNS; r++)); do
        SEED=$((SEED_BASE + r))
        echo -n "  run $((r+1))/$NUM_RUNS (seed=$SEED)... "

        OUTPUT=$("$BIN" "$INST" "$SEED" "$ALPHA" "$CONSTRUCTION_MAX_TRIES" "$NUM_ITER_MAX" 2>&1) || {
            echo "ERRO (exit code $?)"
            continue
        }

        GRASP_COST=$(echo "$OUTPUT" | grep "Custo GRASP:" | awk '{print $NF}')
        VND_COST=$(echo "$OUTPUT" | grep "Custo VND:" | awk '{print $NF}')
        ILS_COST=$(echo "$OUTPUT" | grep "Custo ILS:" | awk '{print $NF}')
        ILS_ITER=$(echo "$OUTPUT" | grep "Iteracoes ILS:" | sed -E 's/.*Iteracoes ILS: ([0-9]+).*/\1/')
        ILS_IMPR=$(echo "$OUTPUT" | grep "melhorias:" | sed -E 's/.*melhorias: ([0-9]+).*/\1/')
        TOTAL_TIME=$(echo "$OUTPUT" | grep "Tempo total:" | awk '{print $NF}' | tr -d 's')

        if echo "$OUTPUT" | grep -q "Validacao pos-ILS: OK"; then
            STATUS="OK"
        else
            STATUS="FAIL"
        fi

        echo "$FNAME,$GROUP,$P_VAL,$((r+1)),$SEED,$GRASP_COST,$VND_COST,$ILS_COST,$ILS_ITER,$ILS_IMPR,$TOTAL_TIME" >> "$OUTFILE"
        echo "ILS=$ILS_COST  tempo=${TOTAL_TIME}s  [$STATUS]"
    done
done

echo ""
echo "Resultados salvos em: $OUTFILE"
echo "Total de instancias: $TOTAL"
echo "Runs por instancia: $NUM_RUNS"
