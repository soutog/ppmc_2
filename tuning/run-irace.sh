#!/usr/bin/env bash
# Dispara o irace no diretorio tuning. Rebuilda o binario antes se
# necessario (precisa dos hooks PPMC_GAMMA/PPMC_MAX_VOLUME/PPMC_BETA_R2
# adicionados em main.cpp).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [[ ! -x "${REPO_ROOT}/bin/main" ]]; then
    echo "Binario nao encontrado. Rodando make..."
    (cd "${REPO_ROOT}" && make)
fi

cd "${SCRIPT_DIR}"

if command -v irace >/dev/null 2>&1; then
    exec irace -s scenario.txt "$@"
fi

if command -v Rscript >/dev/null 2>&1; then
    IRACE_BIN="$(Rscript -e "cat(system.file('bin', 'irace', package = 'irace'))" 2>/dev/null || true)"
    if [[ -n "${IRACE_BIN}" && -x "${IRACE_BIN}" ]]; then
        exec "${IRACE_BIN}" -s scenario.txt "$@"
    fi
    echo "Rscript encontrado, mas o pacote 'irace' nao parece instalado."
    echo "  Rscript -e \"install.packages('irace', repos='https://cloud.r-project.org')\""
    exit 1
fi

echo "Nem 'irace' nem 'Rscript' encontrados. Instale o R e depois o pacote irace."
exit 1
