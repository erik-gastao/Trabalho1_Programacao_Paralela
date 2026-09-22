# Manual — como criar as matrizes de entrada

Duas formas de preencher `matriz_a.csv` e `matriz_b.csv`: manualmente ou com os scripts geradores.

## Opção 1 — Script gerador (recomendado)

`gerar_matriz.py` gera **as duas matrizes numa única execução** (`matriz_a.csv` e `matriz_b.csv`), N×N, com valores inteiros aleatórios de 0 a 99. A única coisa perguntada é o tamanho — as duas saem sempre do mesmo tamanho, mas com conteúdo distinto entre si.

### Rodar

```
wsl python3 gerar_matriz.py
```

Pergunta:

```
Tamanho das matrizes:
```

Digitar um número inteiro (ex.: `200`) e apertar Enter.

### Exemplo

```
$ wsl python3 gerar_matriz.py
Tamanho das matrizes: 200
matriz_a.csv e matriz_b.csv geradas: 200x200, valores aleatorios de 0 a 99
```

Como as matrizes são sempre quadradas (N×N) neste trabalho e as duas saem com o mesmo N na mesma execução, a compatibilidade pra multiplicação (colunas de A = linhas de B) já sai garantida — não precisa se preocupar em digitar tamanhos diferentes.

## Opção 2 — Editar manualmente

Abrir `matriz_a.csv`/`matriz_b.csv` num editor de texto e digitar os valores: uma linha do arquivo = uma linha da matriz, valores separados por vírgula, sem cabeçalho. Ex. matriz 3×3:

```
1,6,4
2,5,1
4,8,2
```

Útil pra matrizes pequenas de teste (2×2, 3×3, 4×4) usadas na verificação de corretude com `prova_real.py`. Pros testes de desempenho (N grande: 200, 500, 1000), usar a Opção 1.

## Depois de gerar

Ver `manual-rodar.md` pra compilar e executar as versões (sequencial/fork/pthreads/openmp) sobre as matrizes geradas.
