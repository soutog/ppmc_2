#!/usr/bin/env bash
# target-runner do irace para PPMC-2.
#
# Recebe a configuracao do irace e retorna o gap% (vs BKS do cenario) como
# score. alpha vai via CLI positional; gamma/max_volume/beta_r2 via env var.
# Em caso de erro, retorna penalidade 1e6 para nao quebrar o irace.

set -uo pipefail

if [[ $# -lt 4 ]]; then
    echo "1000000"
    exit 0
fi

CONFIG_ID="$1"
INSTANCE_ID="$2"
IRACE_SEED="$3"
SCENARIO="$4"
shift 4

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${PPMC_BIN:-${REPO_ROOT}/bin/main}"

IFS='|' read -r INSTANCE_REL BKS SCENARIO_SEED <<< "${SCENARIO}"
INSTANCE_PATH="${REPO_ROOT}/${INSTANCE_REL}"

if [[ ! -x "${BIN}" || ! -f "${INSTANCE_PATH}" ]]; then
    echo "1000000"
    exit 0
fi

# Defaults (devem bater com main.cpp).
ALPHA="0.6667"
GAMMA="18"
MAX_VOLUME="8"
BETA_R2="16"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --alpha)       ALPHA="$2";      shift 2 ;;
        --gamma)       GAMMA="$2";      shift 2 ;;
        --max_volume)  MAX_VOLUME="$2"; shift 2 ;;
        --beta_r2)     BETA_R2="$2";    shift 2 ;;
        *) shift ;;
    esac
done

# Tuning curto: NumIterMax=20 e time_limit=90s para cada experimento rodar
# em ~1-2min. O timeout externo corta em 150s como rede de seguranca.
NUM_ITER_MAX="${NUM_ITER_MAX:-20}"
TIME_LIMIT="${TIME_LIMIT:-90}"
CONSTRUCTION_MAX_TRIES="${CONSTRUCTION_MAX_TRIES:-1000}"
RUN_TIMEOUT="${RUN_TIMEOUT:-150}"

# Seed do run = combinacao do scenario seed com o seed do irace, para
# garantir replicabilidade dentro do mesmo (config, instance).
BASE_SEED=$((IRACE_SEED + SCENARIO_SEED))

TMP_OUTPUT="$(mktemp)"
trap 'rm -f "${TMP_OUTPUT}"' EXIT

if ! PPMC_GAMMA="${GAMMA}" \
     PPMC_MAX_VOLUME="${MAX_VOLUME}" \
     PPMC_BETA_R2="${BETA_R2}" \
     timeout "${RUN_TIMEOUT}s" \
        "${BIN}" \
        "${INSTANCE_PATH}" \
        "${BASE_SEED}" \
        "${ALPHA}" \
        "${CONSTRUCTION_MAX_TRIES}" \
        "${NUM_ITER_MAX}" \
        "${TIME_LIMIT}" > "${TMP_OUTPUT}" 2>&1; then
    echo "1000000"
    exit 0
fi

# Parse "Custo FINAL: <valor>" do stdout do binario.
BEST_COST="$(python3 - "${TMP_OUTPUT}" <<'PY'
import re, sys
path = sys.argv[1]
best = None
with open(path, 'r', encoding='utf-8', errors='ignore') as fh:
    for line in fh:
        m = re.match(r'\s*Custo FINAL:\s*([-+]?[0-9]*\.?[0-9]+)', line)
        if m:
            best = float(m.group(1))
            break
print(best if best is not None else "")
PY
)"

if [[ -z "${BEST_COST}" ]]; then
    echo "1000000"
    exit 0
fi

# Score = gap% vs BKS. Normaliza entre instancias.
SCORE="$(python3 - "${BEST_COST}" "${BKS}" <<'PY'
import sys
cost = float(sys.argv[1])
bks  = float(sys.argv[2])
gap  = 100.0 * (cost - bks) / bks
print(f"{gap:.10f}")
PY
)"

echo "${SCORE}"
