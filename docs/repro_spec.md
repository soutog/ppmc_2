# Especificação Reprodutível do ILS para o PPMC

## Objetivo

Este documento congela uma interpretação operacional do artigo
`METAHEURÍSTICA ILS APLICADA AO PROBLEMA DE p-MEDIANAS CAPACITADO`
para permitir uma implementação reprodutível no projeto atual.

O objetivo aqui nao e provar que esta era exatamente a implementacao
dos autores, e sim fixar uma versao consistente, executavel e auditavel
do metodo descrito no texto. Caso o codigo original dos autores seja
obtido no futuro, ele deve prevalecer sobre as decisoes arbitrais abaixo.

## Fonte

- Documento base:
  [galoa-proceedings-sbpo-2025-metaheuristica-ils-aplicada-ao-problema-de-p-medianas-capacitado.pdf](/home/gabriel/files/mestrado/codes/ppmc_2/docs/galoa-proceedings-sbpo-2025-metaheuristica-ils-aplicada-ao-problema-de-p-medianas-capacitado.pdf)

## Escopo da reproducao

Esta reproducao cobre a meta-heuristica do artigo:

- leitura de instancia;
- construcao inicial via GRASP;
- busca local VND com quatro movimentos;
- perturbacao baseada em trocas;
- ILS com aceitacao apenas por melhora;
- execucao em lote com 30 repeticoes por instancia.

Nao faz parte desta primeira fase:

- reproducao das formulacoes exatas via CPLEX;
- comparacao numerica com Stefanello et al. [2015];
- calibracao por IRACE;
- tentativa de reproduzir exatamente tempos de maquina do artigo.

## Convencoes de implementacao

- Internamente, os indices serao `0-based`.
- Nos relatorios e logs, o nome dos vertices pode continuar `1-based` se isso facilitar comparacao com o artigo.
- Distancias serao calculadas por distancia euclidiana em `double`.
- A matriz de distancias sera precomputada uma unica vez por instancia.
- Uma solucao sera considerada viavel se:
  - abrir exatamente `p` medianas distintas;
  - alocar todo cliente a exatamente uma mediana aberta;
  - respeitar a capacidade de cada mediana.

## Estruturas de dados

### Instancia

A instancia deve fornecer:

- `n`: numero de pontos;
- `p`: numero de medianas;
- para cada ponto `i`:
  - coordenadas `x_i`, `y_i`;
  - capacidade `Q_i`;
  - demanda `q_i`.

### Solucao

Uma solucao sera representada por:

- `vm`: vetor com as `p` medianas abertas;
- `va`: vetor de tamanho `n`, onde `va[j]` e o indice da mediana que atende o cliente `j`;
- `load`: carga total alocada em cada mediana aberta;
- `cost`: soma total das distancias cliente-mediana;
- `feasible`: indicador de viabilidade.

## Avaliacao da solucao

O valor da funcao objetivo sera:

- `cost = sum_j d(j, va[j])`

A viabilidade sera validada por:

- `sum_{j alocado a i} q_j <= Q_i` para toda mediana aberta `i`.

## Aleatoriedade e sementes

Para permitir reproducao:

- o gerador pseudoaleatorio sera `std::mt19937`;
- a semente base sera fornecida por linha de comando;
- em experimentos com 30 execucoes, a execucao `r` usara `seed_base + r`;
- toda escolha aleatoria do algoritmo deve usar exclusivamente esse gerador.

## Solucao inicial via GRASP

### Componente guloso

Para cada candidato `i`, calcular:

- `g(i) = sum_j d(i, j)`

Ordenar os candidatos em ordem crescente de `g(i)`.

Desempate:

- menor valor de `g(i)`;
- em empate numerico, menor indice do vertice.

### Lista restrita de candidatos

Usar:

- `LRC = { i | g(i) <= g_min + alpha * (g_max - g_min) }`

Parametro fixado:

- `alpha = 0.6`

### Escolha das medianas

Decisao de reproducao:

- selecionar `p` medianas distintas da `LRC`, sem reposicao, de forma uniforme.

Se `|LRC| < p`:

- a tentativa de construcao falha e o procedimento recomeca do zero.

### Alocacao dos clientes

Decisao de reproducao:

- embaralhar os clientes e processa-los em ordem aleatoria;
- para cada cliente `j`, escolher a mediana aberta mais proxima que ainda comporte sua demanda;
- em empate de distancia, escolher a mediana de menor indice.

Se algum cliente nao puder ser alocado:

- a tentativa de construcao falha e o procedimento recomeca do zero.

### Numero maximo de tentativas da construcao

Ambiguidade do artigo:

- o Algoritmo 1 recebe `NMax`, mas a Secao 4.2 nao informa valor calibrado para esse parametro;
- o pseudocodigo ainda sobrescreve `NMax <- 0`, o que e inconsistente.

Decisao de reproducao:

- definir `construction_max_tries = 1000`.

Racional:

- o parametro existe no pseudocodigo;
- e necessario limitar loops em construcao inviavel;
- `1000` e suficientemente alto para nao interferir em instancias usuais e suficientemente baixo para evitar travamento.

## Busca local VND

O VND sera implementado com a ordem fixa:

1. `M1` realocacao
2. `M2` substituicao intra-cluster
3. `M3` substituicao inter-cluster
4. `M4` troca

### Politica de busca

Ambiguidade do artigo:

- o texto afirma `Best Improvement`;
- o Algoritmo 2 mostra apenas "gera um vizinho".

Decisao de reproducao:

- usar `Best Improvement` completo em cada vizinhanca;
- ao encontrar melhora, aplicar o melhor vizinho daquela estrutura;
- apos melhora, reiniciar em `M1`;
- se nao houver melhora na estrutura corrente, avancar para a proxima.

### M1 - Realocacao

Definicao operacional:

- para cada cliente `j`;
- para cada mediana aberta `r2` diferente da mediana atual `r1 = va[j]`;
- testar mover `j` de `r1` para `r2`;
- movimento permitido apenas se a nova carga de `r2` respeitar capacidade.

### M2 - Substituicao intra-cluster

Definicao operacional:

- escolher uma mediana aberta `r1`;
- escolher um vertice `r2` nao-mediana pertencente ao cluster atualmente atendido por `r1`;
- substituir `r1` por `r2`;
- manter todos os clientes desse cluster alocados ao novo centro `r2`;
- o movimento e viavel apenas se a carga total do cluster couber na capacidade de `r2`.

### M3 - Substituicao inter-cluster

Ambiguidade do artigo:

- o texto diz apenas que uma mediana `r1` e substituida por um nao-mediana `r2` de outro cluster;
- o artigo nao define como as alocacoes sao reconstruidas apos essa troca.

Decisao de reproducao:

- escolher uma mediana aberta `r1`;
- escolher um vertice `r2` nao-mediana que pertence a outro cluster;
- abrir `r2` e fechar `r1`;
- reconstruir as alocacoes de todos os clientes por uma atribuicao gulosa viavel:
  - ordenar clientes por demanda decrescente;
  - para cada cliente, escolher a mediana aberta mais proxima com capacidade residual;
  - em empate, menor indice.

Se a reconstruicao falhar:

- o vizinho e descartado como inviavel.

### M4 - Troca

Definicao operacional:

- escolher dois clientes `j1` e `j2` pertencentes a clusters diferentes;
- trocar suas medianas de atendimento;
- o movimento e viavel apenas se ambas as capacidades forem respeitadas apos a troca.

## Perturbacao

O artigo declara tres niveis de perturbacao e diz explicitamente:

- nivel 1: `2` trocas;
- nivel 2: `3` trocas;
- nivel 3: `4` trocas.

Ambiguidade do artigo:

- o Algoritmo 3 aplica `k` iteracoes, o que contradiz a descricao em prosa.

Decisao de reproducao:

- prevalece a descricao em prosa;
- logo, `num_swaps(level) = level + 1`.

### Operacao da perturbacao

- a perturbacao usa apenas o movimento `M4`;
- cada troca e escolhida aleatoriamente entre vizinhos viaveis;
- a perturbacao nao exige melhora na funcao objetivo;
- se nao houver troca viavel apos `20 * n` tentativas de amostragem, a perturbacao encerra antecipadamente e retorna a melhor solucao perturbada ja construida.

## ILS

### Fluxo principal

1. construir `s0` via GRASP;
2. aplicar VND em `s0`, obtendo `s`;
3. definir `best = s`;
4. definir `iter_without_improvement = 0`;
5. definir `level = 1`;
6. enquanto `iter_without_improvement < NumIterMax`:
   - gerar `s'` por perturbacao de `s` no nivel `level`;
   - aplicar VND em `s'`, obtendo `s''`;
   - se `cost(s'') < cost(s)`:
     - `s = s''`;
     - se `cost(s) < cost(best)`, atualizar `best`;
     - `iter_without_improvement = 0`;
     - `level = 1`;
   - senao:
     - `iter_without_improvement += 1`;
     - `level = min(level + 1, 3)`.
7. retornar `best`.

### Criterio de parada

Ambiguidade do artigo:

- o texto diz "NumIterMax iteracoes sem melhora";
- o pseudocodigo incrementa `iter` em toda iteracao e nao o reseta.

Decisao de reproducao:

- prevalece a descricao em prosa;
- `NumIterMax` sera interpretado como numero maximo de iteracoes consecutivas sem melhora.

Parametro fixado:

- `NumIterMax = 20`

### Criterio de aceitacao

Decisao de reproducao:

- aceitar apenas melhora estrita;
- isto e, `s''` substitui `s` somente se `cost(s'') < cost(s)`.

Empates:

- solucoes com mesmo custo nao sao aceitas como nova solucao corrente.

## Tratamento das inconsistencias do artigo

### Formulação matematica

Na Formulação 2, o artigo escreve:

- `x_ij <= y_j`

Interpretacao corrigida:

- a restricao correta e `x_ij <= y_i`.

Essa correcao nao afeta diretamente a heuristica, mas fica registrada para evitar propagar erro de notacao.

### GRASP

Inconsistencias do Algoritmo 1:

- `NMax <- 0` deveria iniciar o contador de tentativas, nao o parametro;
- `enquanto k <= p` sugere selecao de `p + 1` medianas;
- a linha de retorno para a selecao de medianas nao esta bem alinhada com o fluxo de tentativas.

Interpretacao operacional adotada:

- usar contador de tentativas separado;
- selecionar exatamente `p` medianas;
- em caso de falha de alocacao, reiniciar toda a construcao.

### ILS

Inconsistencias do Algoritmo 4:

- nao explicita saturacao ou reinicio do nivel de perturbacao acima de `3`;
- nao reseta contador em caso de melhora, apesar do texto indicar esse comportamento.

Interpretacao operacional adotada:

- `level` cresce ate `3` e satura;
- contador de iteracoes sem melhora zera quando ha melhora.

## Protocolo experimental

Para reproduzir a Secao 4 do artigo na fase heuristica:

- executar `30` rodadas independentes por instancia;
- usar `alpha = 0.6`;
- usar `NumIterMax = 20`;
- registrar por instancia:
  - melhor valor de funcao objetivo;
  - valor medio de funcao objetivo;
  - tempo medio de execucao;
  - semente usada em cada rodada.

Tempo de execucao:

- medir com `std::chrono::steady_clock`;
- relatar em segundos com precisao decimal.

## Saidas esperadas do programa

O executavel principal deve suportar pelo menos dois modos:

- modo instancia unica:
  - recebe arquivo, semente e parametros;
  - imprime custo, viabilidade, medianas abertas, tempo e resumo da busca;
- modo experimento:
  - recebe uma pasta ou lista de instancias;
  - executa 30 rodadas por instancia;
  - gera tabela ou CSV consolidado.

## Itens ainda fora de escopo

Os itens abaixo podem ser adicionados depois sem alterar esta especificacao do ILS:

- comparador com CPLEX;
- exportacao de resultados para tabelas em estilo artigo;
- leitura de configuracao por arquivo;
- calibracao automatica de parametros.

## Resumo arbitral

Quando houver conflito entre texto e pseudocodigo do artigo, esta reproducao seguira a seguinte regra:

- prevalece a descricao textual quando ela for mais especifica do que o pseudocodigo;
- quando o artigo nao definir detalhe suficiente, sera usada a decisao mais conservadora, simples e auditavel;
- toda decisao arbitral deve ser registrada aqui antes de ser implementada.
