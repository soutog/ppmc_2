# PPMC — Problema de p-Medianas Capacitado

Reprodução do artigo **"Metaheurística ILS aplicada ao Problema de p-Medianas Capacitado"**
(Freitas, Souza & Sá — SBPO 2025).

## Objetivo

Reproduzir o pipeline completo do artigo (GRASP → VND → Perturbação → ILS) e bater ou
igualar os resultados reportados nas Tabelas 2 e 3 do paper original.

## Problema

Dado um conjunto de `n` instalações candidatas, selecionar `p` medianas e alocar cada
cliente à mediana mais próxima, minimizando a soma das distâncias, respeitando a capacidade
de cada mediana.

- **Função objetivo**: `cost = sum_j d(j, va[j])`
- **Restrição de capacidade**: `sum_{j alocado a i} q_j <= Q_i` para toda mediana `i`

## Estado Atual do Código

| Módulo | Arquivo(s) | Status |
|--------|-----------|--------|
| Leitura de instância | `instance.h/.cpp` | Implementado |
| Matriz de distâncias | `distance_matrix.h/.cpp` | Implementado |
| Representação de solução | `solution.h/.cpp` | Implementado |
| Avaliador / Validador | `evaluator.h/.cpp` | Implementado |
| Construção GRASP | `grasp_constructor.h/.cpp` | Implementado |
| **Vizinhanças (M1-M4)** | — | **Pendente** |
| **Busca local VND** | — | **Pendente** |
| **Perturbação** | — | **Pendente** |
| **ILS** | — | **Pendente** |
| Experimento em lote (30 runs) | — | **Pendente** |

## Plano de Implementação

### Fase 1 — Busca Local (vizinhanças leves)

1. **M1 — Realocação**: mover cliente `j` de sua mediana para outra mediana aberta (se capacidade permitir)
2. **M4 — Troca**: trocar medianas de dois clientes de clusters diferentes (se ambas capacidades permitirem)
3. **VND parcial (M1, M4)**: validar melhoria sobre GRASP puro

### Fase 2 — Busca Local (vizinhanças pesadas)

4. **M2 — Substituição intra-cluster**: substituir mediana `r1` por não-mediana `r2` do mesmo cluster
5. **M3 — Substituição inter-cluster**: substituir mediana `r1` por não-mediana `r2` de outro cluster, com reconstrução gulosa das alocações
6. **VND completo (M1 → M2 → M3 → M4)**: Best Improvement, reinicia em M1 ao melhorar

### Fase 3 — Meta-heurística

7. **Perturbação**: `level + 1` trocas aleatórias via M4 (níveis 1–3 → 2–4 swaps)
8. **ILS**: loop com `NumIterMax = 20` iterações sem melhora, nível de perturbação crescente

### Fase 4 — Validação

9. Experimento em lote: 30 runs por instância, `alpha = 0.6`, sementes `seed_base + r`
10. Comparação com Tabelas 2 e 3 do paper

### Fase 5 — Exploração (pós-reprodução, congelada)

- Analisar vizinhanças de Vasconcelos [2023] (GVNS)
- Incorporar ideias de Stefanello et al. [2015] (matheuristics / partes exatas)
- Investigar Clustering Search
- Calibração de parâmetros (IRACE)

## Parâmetros do Algoritmo

| Parâmetro | Valor | Origem |
|-----------|-------|--------|
| `alpha` | 0.6 | IRACE (Tabela 1 do paper) |
| `NumIterMax` | 20 | IRACE (Tabela 1 do paper) |
| `construction_max_tries` | 1000 | Decisão de reprodução |
| Perturbação swaps | `level + 1` | Descrição textual do paper |

## Instâncias

35 instâncias de 7 famílias (lin318, ali535, u724, rl1304, pr2392, fnl4461, p3038),
com `n` de 318 a 4461 e `p` de 5 a 1000. Formato: `n p` na primeira linha, seguido de
`x y capacidade demanda` por nó.

## Compilação e Execução

```bash
make            # compila em ./bin/main
make debug      # compila com -O0 -g
make clean      # limpa binários e temporários

./bin/main <instância> [seed] [alpha] [max_tries]
```

## Referências

- Freitas, Souza & Sá (2025). *Metaheurística ILS aplicada ao PPMC*. SBPO 2025.
- Stefanello et al. (2015). Matheuristics para CPMP.
- Vasconcelos (2023). GVNS para CPMP.
- Spec de reprodução: [`docs/repro_spec.md`](docs/repro_spec.md)
