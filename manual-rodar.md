# Manual — como rodar

## 0. Pré-requisitos

Windows não tem gcc nativo. Usar WSL (já tem Ubuntu instalado):

```
wsl gcc --version
```

## 1. Preencher as matrizes de entrada

### Manual

Editar `matriz_a.csv` e `matriz_b.csv` na raiz do projeto: uma linha por linha da matriz, valores separados por vírgula, sem cabeçalho. Ex. matriz 3×3:

```
1,6,4
2,5,1
4,8,2
```

Matrizes precisam ser quadradas (N×N) e A.colunas == B.linhas (no caso quadrado, mesmo N nas duas).

### Gerar aleatório (recomendado pros testes de desempenho)

`gerar_matriz.py` gera as duas matrizes (`matriz_a.csv` e `matriz_b.csv`) numa única execução, N×N, valores inteiros aleatórios de 0 a 99:

```
wsl python3 gerar_matriz.py
```

Pergunta só `Tamanho das matrizes:` (digitar N). As duas saem com o mesmo tamanho automaticamente, mas com conteúdo distinto entre si. Ver `manual-criar-matriz.md` pra detalhes.

## 2. Compilar

Da raiz do projeto (PowerShell ou Bash), via WSL.

Pros testes de desempenho "oficiais" (comparação justa entre as 4 versões), compilar todas com **o mesmo nível de otimização**, `-O2`:

```
wsl gcc -std=c11 -Wall -Wextra -O2 -o cod_sequencial cod_sequencial.c matriz.c
```

Compilar as outras versões também com `-O2` (openmp precisa de `-fopenmp`, pthreads de `-pthread`):

```
wsl gcc -std=c11 -Wall -Wextra -O2 -o cod_fork cod_fork.c matriz.c
wsl gcc -std=c11 -Wall -Wextra -O2 -o cod_pthreads cod_pthreads.c matriz.c -pthread
wsl gcc -std=c11 -Wall -Wextra -O2 -fopenmp -o cod_openmp cod_openmp.c matriz.c
```

Comparador paralelo (verificação de corretude, seção 4), uma versão por ferramenta. Todos precisam de `comparador.c` e `matriz.c`:

```
wsl gcc -std=c11 -Wall -Wextra -O2 -o comp_fork comp_fork.c comparador.c matriz.c
wsl gcc -std=c11 -Wall -Wextra -O2 -pthread -o comp_pthreads comp_pthreads.c comparador.c matriz.c
wsl gcc -std=c11 -Wall -Wextra -O2 -fopenmp -o comp_openmp comp_openmp.c comparador.c matriz.c
```

Os arquivos de código-fonte usam o prefixo `cod_` (`cod_sequencial.c`, `cod_fork.c`, `cod_pthreads.c`, `cod_openmp.c`) pra ficarem visivelmente agrupados no `ls`, separados de `matriz.c`/`matriz.h` (módulo de I/O compartilhado) e dos scripts Python. O comparador usa o prefixo `comp_` (`comp_fork.c`, `comp_pthreads.c`, `comp_openmp.c`), com a parte comum em `comparador.c`/`comparador.h`. Os binários compilados mantêm o mesmo prefixo. Gera binários ELF, só rodam dentro do WSL (não dá duplo-clique no Windows).

### Bônus pra apresentação: com/sem otimização

Pra mostrar que o ganho de desempenho não vem só do paralelismo, mas também do compilador, compilar uma segunda leva **sem otimização** (`-O0`, binário com sufixo `_o0`) e comparar contra a leva `-O2` acima:

```
wsl gcc -std=c11 -Wall -Wextra -O0 -o cod_sequencial_o0 cod_sequencial.c matriz.c
wsl gcc -std=c11 -Wall -Wextra -O0 -o cod_fork_o0 cod_fork.c matriz.c
wsl gcc -std=c11 -Wall -Wextra -O0 -o cod_pthreads_o0 cod_pthreads.c matriz.c -pthread
wsl gcc -std=c11 -Wall -Wextra -O0 -fopenmp -o cod_openmp_o0 cod_openmp.c matriz.c
```

Rodar cada versão nas duas variantes (`-O0` e `-O2`) com o mesmo N e anotar os 8 tempos (4 métodos × 2 níveis) numa tabela. Serve pra discutir na apresentação: otimização do compilador vs paralelismo, o que pesa mais.

## 3. Executar

```
wsl bash -c "cd /mnt/c/Trabalho1_Programacao_Paralela && ./cod_sequencial"
```

Troca `./cod_sequencial` pelo binário desejado (`./cod_fork`, `./cod_pthreads`, `./cod_openmp`).

Cada execução:
- lê `matriz_a.csv` e `matriz_b.csv`
- imprime no terminal o tempo de execução só da multiplicação
- avisa quando termina o cálculo e quando começa/termina a escrita do resultado
- grava o resultado em `resultado_<metodo>.csv` (ex.: `resultado_sequencial.csv`), escrito em paralelo pelo próprio método (fork escreve com fork, pthreads com pthreads, openmp com openmp; sequencial escreve sequencial)

Todos os binários paralelos (`cod_*` e `comp_*`) perguntam ao iniciar `Numero de threads (Enter = 16):` (ou `Numero de processos` no fork). Digitar o número (testes: 2, 4, 8, 16) e Enter; Enter vazio usa um por núcleo. A pergunta acontece antes da leitura dos CSVs, fora do tempo medido. Também dá pra passar direto como argumento, sem pergunta (ex.: `./cod_openmp 4`), útil pra rodar em sequência num script.

## 4. Verificar corretude (comparador paralelo)

Depois de rodar as 4 versões com o mesmo N, rodar um dos comparadores (qualquer um; dá pra rodar os três e comparar o tempo deles também):

```
wsl bash -c "cd /mnt/c/Trabalho1_Programacao_Paralela && ./comp_openmp"
```

Troca por `./comp_fork` ou `./comp_pthreads`. Toma `resultado_sequencial.csv` como referência e compara os outros três contra ele, célula a célula. Saída esperada:

```
Comparador OpenMP (16 threads): 3 resultados 1000x1000 vs sequencial -> tempo de comparacao: 0.007246 s
  fork      OK (igual ao sequencial)
  pthreads  OK (igual ao sequencial)
  openmp    OK (igual ao sequencial)
Veredito: os 4 resultados sao consistentes
```

Se algum método divergir, aparece `DIVERGENTE` (quantas células e a primeira posição), `DIMENSAO ERRADA` ou `NAO VERIFICADO` (arquivo não existe), e o programa sai com código 1. Só considerar válidos os tempos de um N se o veredito for "consistentes".

## 5. Testes de desempenho

Preencher `matriz_a.csv`/`matriz_b.csv` com matrizes N×N geradas (N = 300, 500, 1000, 1500, 2000 — ver ADR), rodar cada uma das 4 versões (compiladas com `-O2`, seção 2), conferir com o comparador (seção 4), anotar o tempo impresso no terminal. Comparação é manual (sem script), então anotar os tempos numa tabela à parte pra apresentação. Opcionalmente, repetir com os binários `_o0` (seção "Bônus") pra comparar também o efeito da otimização do compilador.
