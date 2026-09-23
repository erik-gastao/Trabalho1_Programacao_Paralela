# Trabalho 1 — Paralelismo com Memória Compartilhada

## Objetivo do trabalho

Aplicar uma forma de paralelismo com memória compartilhada (Fork, Pthreads ou OpenMP) sobre um problema de escolha do aluno.

## Problema escolhido

**Multiplicação de matrizes (matriz × matriz)**, no contexto da matéria de Álgebra Linear Aplicada.

Motivo da escolha dentro do conjunto de operações consideradas (soma, transposição, multiplicação matriz-vetor, multiplicação matriz-matriz): é a operação com maior custo computacional. Para matrizes N×N, soma e transposição custam O(N²), multiplicação matriz-vetor custa O(N²), e multiplicação matriz-matriz custa O(N³). Essa diferença faz o ganho de desempenho do paralelismo aparecer de forma mais visível nos testes.

## Entrada de dados

As matrizes de entrada serão lidas a partir de arquivos CSV, no formato do modelo fornecido: um arquivo por matriz, valores separados por vírgula, sem linha de cabeçalho.

- O tamanho da matriz é dinâmico: não é fixado no código, e sim determinado durante a própria leitura do CSV (contando linhas e colunas do arquivo).
- Serão trabalhadas apenas **matrizes quadradas** (N×N), ou seja, número de linhas igual ao número de colunas. Essa escolha garante automaticamente a compatibilidade dimensional exigida pela multiplicação de matrizes (colunas de A = linhas de B), sem necessidade de tratar matrizes retangulares.
- Ainda assim, o programa deve validar se as dimensões de A e B lidas do CSV são compatíveis entre si antes de iniciar o cálculo, reportando erro caso não sejam.

## Ferramentas de paralelismo

O mesmo problema (multiplicação de matrizes) será implementado em **três versões paralelas diferentes**, para fins de comparação:

1. Fork (processos, com memória compartilhada explícita)
2. Pthreads (threads)
3. OpenMP (diretivas de compilador)

Também será mantida uma **versão sequencial** da multiplicação, em um arquivo separado e único, servindo de baseline de comparação de tempo de execução contra as três versões paralelas. Como o algoritmo sequencial não muda entre as comparações, não há necessidade de duplicá-lo dentro dos três arquivos paralelos.

Cada método terá seu **próprio arquivo de código-fonte** (sequencial, Fork, Pthreads, OpenMP — quatro arquivos no total). Cada arquivo deve, ao final da execução, imprimir a métrica de tempo de execução daquela versão (medindo apenas a etapa da multiplicação, não a leitura do CSV, para manter a comparação justa entre os quatro), e também salvar a matriz resultado em um arquivo CSV próprio (`resultado_sequencial.csv`, `resultado_fork.csv`, `resultado_pthreads.csv`, `resultado_openmp.csv`). A comparação de desempenho entre os quatro será feita manualmente na apresentação, sem necessidade de um script automatizado de comparação de tempos.

### Estratégia de divisão do trabalho

Divisão **por linha, em blocos contíguos**: com P threads/processos e N linhas na matriz resultado, cada worker calcula um bloco de linhas seguidas (ex.: N=100, P=4 → linhas 0–24, 25–49, 50–74, 75–99). Evita conflito de escrita (cada worker escreve em linhas distintas) e mantém boa localidade de cache, sem a complexidade extra de um particionamento em blocos 2D. Na versão Fork, a matriz resultado fica em memória compartilhada (mmap), alocada antes dos `fork()`s.

## Verificação de corretude (comparador paralelo)

Além das métricas de tempo, um comparador em C, também paralelo, confere se as quatro versões chegaram ao mesmo resultado:

- Toma `resultado_sequencial.csv` como referência e compara `resultado_fork.csv`, `resultado_pthreads.csv` e `resultado_openmp.csv` contra ela, célula a célula (tolerância 1e-9 relativa + absoluta).
- As linhas são divididas em blocos contíguos entre os workers, igual à multiplicação; cada worker conta as divergências do seu bloco e as contagens são juntadas no final.
- Implementado nas três ferramentas: `comp_fork.c` (contagens voltam por mmap), `comp_pthreads.c` (junção manual após join), `comp_openmp.c` (cláusula `reduction`). Parte comum em `comparador.h`/`comparador.c`.
- Reporta, para cada método: OK, divergente (quantas células e a primeira), dimensão errada ou arquivo não encontrado. Mede o próprio tempo de comparação.

Os testes de desempenho são feitos com N = 300, 500, 1000, 1500 e 2000; o comparador deve dar "consistente" em cada N antes de os tempos serem considerados válidos.

## Comparação a ser feita

- Tempo de execução: sequencial vs. Fork vs. Pthreads vs. OpenMP
- Comportamento com diferentes tamanhos de matriz (N pequeno, médio, grande)
- Facilidade/complexidade de implementação de cada abordagem (a ser discutido na ADR)
- Bônus: efeito do nível de otimização do compilador (`-O0` vs `-O2`) sobre cada versão, pra mostrar que o ganho de desempenho não vem só do paralelismo (ver `manual-rodar.md`)

## Entregáveis

1. Todos os arquivos de código-fonte (as quatro versões: sequencial, Fork, Pthreads, OpenMP, e o comparador paralelo nas três ferramentas)
2. Slides (se houver apresentação)
3. ADR (Architecture Decision Record) documentando as decisões de implementação, incluindo:
   - Escolha do problema (multiplicação de matrizes) e justificativa
   - Escolha das ferramentas de paralelismo e comparação entre elas
   - Estratégia de divisão do trabalho entre threads/processos
   - Resultados de desempenho obtidos e discussão sobre os mesmos

## Pontos decididos

- [x] Estratégia de divisão do trabalho: por linha, em blocos contíguos (ver seção "Ferramentas de paralelismo")
- [x] Tamanhos (N) de matriz para os testes de desempenho: 300, 500, 1000, 1500, 2000
- [x] Quantidade de workers nos testes: 1 (sequencial), 2, 4, 8, 16 processos/threads (pedido no terminal ao executar)
- [x] Verificação de corretude: comparador paralelo em C (ver seção "Verificação de corretude")
- [x] Grupo: 1 integrante (Erik)
