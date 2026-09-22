# Manual — como rodar

## 0. Pré-requisitos

Windows não tem gcc nativo. Usar WSL (já tem Ubuntu instalado):

```
wsl gcc --version
```

Se não tiver `numpy` no WSL (pro `prova_real.py`):

```
wsl pip3 install numpy
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

Compilar as outras versões também com `-O2` (openmp precisa de `-fopenmp`, pthreads de `-lpthread`):

```
wsl gcc -std=c11 -Wall -Wextra -O2 -o cod_fork cod_fork.c matriz.c
wsl gcc -std=c11 -Wall -Wextra -O2 -o cod_pthreads cod_pthreads.c matriz.c -lpthread
wsl gcc -std=c11 -Wall -Wextra -O2 -fopenmp -o cod_openmp cod_openmp.c matriz.c
```

Os arquivos de código-fonte usam o prefixo `cod_` (`cod_sequencial.c`, `cod_fork.c`, `cod_pthreads.c`, `cod_openmp.c`) pra ficarem visivelmente agrupados no `ls`, separados de `matriz.c`/`matriz.h` (módulo de I/O compartilhado) e dos scripts Python. Os binários compilados mantêm o mesmo prefixo. Gera binários ELF, só rodam dentro do WSL (não dá duplo-clique no Windows).

### Bônus pra apresentação: com/sem otimização

Pra mostrar que o ganho de desempenho não vem só do paralelismo, mas também do compilador, compilar uma segunda leva **sem otimização** (`-O0`, binário com sufixo `_o0`) e comparar contra a leva `-O2` acima:

```
wsl gcc -std=c11 -Wall -Wextra -O0 -o cod_sequencial_o0 cod_sequencial.c matriz.c
wsl gcc -std=c11 -Wall -Wextra -O0 -o cod_fork_o0 cod_fork.c matriz.c
wsl gcc -std=c11 -Wall -Wextra -O0 -o cod_pthreads_o0 cod_pthreads.c matriz.c -lpthread
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
- grava o resultado em `resultado_<metodo>.csv` (ex.: `resultado_sequencial.csv`)

## 4. Verificar corretude

```
wsl python3 prova_real.py
```

Verifica os 4 métodos de uma vez (só reporta os que já têm `resultado_*.csv` gerado). Pra verificar um método específico:

```
wsl python3 prova_real.py Sequencial
wsl python3 prova_real.py Fork
wsl python3 prova_real.py Pthreads
wsl python3 prova_real.py OpenMP
```

Saída esperada por método: `Resultado correto`, `Resultado DIVERGENTE`, `Dimensão incorreta` ou `Arquivo não encontrado`.

## 5. Testes de desempenho

Preencher `matriz_a.csv`/`matriz_b.csv` com matrizes N×N geradas (N = 200, 500, 1000 — ver ADR), rodar cada uma das 4 versões (compiladas com `-O2`, seção 2), anotar o tempo impresso no terminal. Comparação é manual (sem script), então anotar os tempos numa tabela à parte pra apresentação. Opcionalmente, repetir com os binários `_o0` (seção "Bônus") pra comparar também o efeito da otimização do compilador.
