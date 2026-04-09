# sample_instance

Amostra de 15 instâncias escolhida para representar bem o conjunto em
`instances/`.

Critério usado:
- Cobrir todas as 7 famílias do conjunto principal.
- Cobrir diferentes tamanhos de instância (`n = 318` até `4461`).
- Cobrir diferentes razões `p/n`, desde casos muito restritos até casos com
  muitas medianas abertas.
- Dar um pouco mais de peso à família `fnl4461`, por ser a maior e a mais
  diferente em escala.

Instâncias selecionadas:

| Arquivo | n | p | p/n | Papel na amostra |
| --- | ---: | ---: | ---: | --- |
| `ali535_025.txt` | 535 | 25 | 0.0467 | pequeno porte, `p/n` baixo |
| `ali535_150.txt` | 535 | 150 | 0.2804 | pequeno porte, `p/n` alto |
| `fnl4461_0020.txt` | 4461 | 20 | 0.0045 | maior escala, `p/n` muito baixo |
| `fnl4461_0250.txt` | 4461 | 250 | 0.0560 | maior escala, faixa intermediária |
| `fnl4461_1000.txt` | 4461 | 1000 | 0.2242 | maior escala, `p/n` alto |
| `lin318_015.txt` | 318 | 15 | 0.0472 | menor escala, `p/n` baixo |
| `lin318_100.txt` | 318 | 100 | 0.3145 | menor escala, `p/n` muito alto |
| `p3038_600.dat` | 3038 | 600 | 0.1975 | família concentrada em `p/n` alto |
| `p3038_1000.dat` | 3038 | 1000 | 0.3292 | família concentrada em `p/n` muito alto |
| `pr2392_075.txt` | 2392 | 75 | 0.0314 | grande porte, `p/n` baixo |
| `pr2392_500.txt` | 2392 | 500 | 0.2090 | grande porte, `p/n` alto |
| `rl1304_050.txt` | 1304 | 50 | 0.0383 | porte médio-grande, `p/n` baixo |
| `rl1304_300.txt` | 1304 | 300 | 0.2301 | porte médio-grande, `p/n` alto |
| `u724_030.txt` | 724 | 30 | 0.0414 | porte médio, `p/n` baixo |
| `u724_200.txt` | 724 | 200 | 0.2762 | porte médio, `p/n` alto |

Observação:
- Esta pasta foi pensada como amostra compacta e diversificada, não como
  subconjunto "fácil" ou "difícil".
- Se o objetivo for tuning, eu recomendaria usar esta pasta como ponto de
  partida e ainda reservar um pequeno hold-out fora dela.
